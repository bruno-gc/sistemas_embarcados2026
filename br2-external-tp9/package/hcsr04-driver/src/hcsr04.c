// SPDX-License-Identifier: GPL-2.0
/*
 * hcsr04.c - Driver Linux para o sensor ultrassonico HC-SR04
 *
 * TP Aula 9 - Sistemas Embarcados - Grupo 01
 *
 * Cada leitura de /dev/hcsr04 dispara uma medicao e devolve a distancia
 * em milimetros, em texto. Exemplo:
 *
 *     $ cat /dev/hcsr04
 *     374
 *
 * Fluxo de uma medicao:
 *   1. read() gera um pulso de 10 us no TRIG (GPIO23);
 *   2. o sensor emite o ultrassom e coloca o ECHO (GPIO24) em nivel alto;
 *   3. a IRQ da borda de subida do ECHO marca o instante t0;
 *   4. a IRQ da borda de descida marca t1 e acorda o read();
 *   5. distancia_mm = (t1 - t0) em us * 343 / 2000.
 *
 * Os GPIOs vem do Device Tree (trig-gpios e echo-gpios), nunca de numeros
 * fixos no codigo. O driver se associa ao hardware pelo compatible
 * "unisanta,hcsr04".
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define HCSR04_TRIG_PULSE_US	10	/* largura do pulso de trigger (datasheet) */
#define HCSR04_TIMEOUT_MS	100	/* o eco mais longo dura ~38 ms */
#define HCSR04_INTERVAL_MS	60	/* intervalo minimo entre medicoes (datasheet) */

struct hcsr04 {
	struct device *dev;
	struct gpio_desc *trig;
	struct gpio_desc *echo;
	int irq;
	struct miscdevice misc;
	struct mutex lock;		/* garante uma medicao por vez */
	struct completion done;		/* sinalizada na borda de descida do ECHO */
	ktime_t t_rise;			/* instante da borda de subida do ECHO */
	ktime_t t_fall;			/* instante da borda de descida do ECHO */
	ktime_t last;			/* fim da ultima medicao */
	bool armed;			/* true somente durante uma medicao */
	bool got_rise;			/* a borda de subida ja foi vista */
};

/*
 * Rotina de interrupcao: roda nas duas bordas do ECHO.
 * So registra o tempo; nenhum calculo nem espera acontece aqui.
 */
static irqreturn_t hcsr04_echo_irq(int irq, void *data)
{
	struct hcsr04 *hc = data;
	ktime_t now = ktime_get();

	/* ignora bordas fora de uma medicao (ruido, sensor desconectado) */
	if (!READ_ONCE(hc->armed))
		return IRQ_HANDLED;

	if (gpiod_get_value(hc->echo)) {
		hc->t_rise = now;
		hc->got_rise = true;
	} else if (hc->got_rise) {
		hc->t_fall = now;
		WRITE_ONCE(hc->armed, false);
		complete(&hc->done);
	}

	return IRQ_HANDLED;
}

/*
 * Faz uma medicao completa. Deve ser chamada com hc->lock travado.
 * Retorna 0 e a distancia em *mm, ou um erro negativo.
 */
static int hcsr04_measure(struct hcsr04 *hc, u32 *mm)
{
	s64 since_last_ms, pulse_us;
	long left;

	/* respeita o intervalo minimo entre medicoes, para o eco anterior morrer */
	since_last_ms = ktime_ms_delta(ktime_get(), hc->last);
	if (since_last_ms >= 0 && since_last_ms < HCSR04_INTERVAL_MS)
		msleep(HCSR04_INTERVAL_MS - since_last_ms);

	reinit_completion(&hc->done);
	hc->got_rise = false;
	WRITE_ONCE(hc->armed, true);

	/* pulso de trigger: 10 us em nivel alto */
	gpiod_set_value(hc->trig, 1);
	udelay(HCSR04_TRIG_PULSE_US);
	gpiod_set_value(hc->trig, 0);

	/* dorme ate a IRQ da borda de descida, sem busy-waiting */
	left = wait_for_completion_interruptible_timeout(&hc->done,
				msecs_to_jiffies(HCSR04_TIMEOUT_MS));

	WRITE_ONCE(hc->armed, false);
	hc->last = ktime_get();

	if (left == 0)
		return -ETIMEDOUT;	/* sem eco: sensor desligado ou mal ligado */
	if (left < 0)
		return left;		/* interrompido por sinal (-ERESTARTSYS) */

	pulse_us = ktime_us_delta(hc->t_fall, hc->t_rise);
	if (pulse_us <= 0)
		return -EIO;

	/* distancia = tempo * velocidade do som / 2  (343 m/s) */
	*mm = (u32)div_s64(pulse_us * 343, 2000);
	return 0;
}

static ssize_t hcsr04_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct hcsr04 *hc = container_of(file->private_data,
					 struct hcsr04, misc);
	char text[16];
	int len, ret;
	u32 mm;

	/*
	 * Uma medicao por abertura do arquivo. A segunda leitura devolve
	 * fim de arquivo, para "cat /dev/hcsr04" mostrar um valor e terminar.
	 */
	if (*ppos > 0)
		return 0;

	if (mutex_lock_interruptible(&hc->lock))
		return -ERESTARTSYS;
	ret = hcsr04_measure(hc, &mm);
	mutex_unlock(&hc->lock);

	if (ret)
		return ret;

	len = scnprintf(text, sizeof(text), "%u\n", mm);
	return simple_read_from_buffer(buf, count, ppos, text, len);
}

static const struct file_operations hcsr04_fops = {
	.owner	= THIS_MODULE,
	.read	= hcsr04_read,
	.llseek	= noop_llseek,
};

static int hcsr04_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hcsr04 *hc;
	int ret;

	hc = devm_kzalloc(dev, sizeof(*hc), GFP_KERNEL);
	if (!hc)
		return -ENOMEM;

	hc->dev = dev;
	mutex_init(&hc->lock);
	init_completion(&hc->done);

	/* GPIOs descritos no Device Tree: trig-gpios e echo-gpios */
	hc->trig = devm_gpiod_get(dev, "trig", GPIOD_OUT_LOW);
	if (IS_ERR(hc->trig))
		return dev_err_probe(dev, PTR_ERR(hc->trig),
				     "trig-gpios nao encontrado no Device Tree\n");

	hc->echo = devm_gpiod_get(dev, "echo", GPIOD_IN);
	if (IS_ERR(hc->echo))
		return dev_err_probe(dev, PTR_ERR(hc->echo),
				     "echo-gpios nao encontrado no Device Tree\n");

	/* a IRQ le o ECHO direto; um GPIO que pode dormir nao serve */
	if (gpiod_cansleep(hc->echo))
		return dev_err_probe(dev, -EINVAL,
				     "o GPIO do ECHO nao pode ser lido em IRQ\n");

	hc->irq = gpiod_to_irq(hc->echo);
	if (hc->irq < 0)
		return dev_err_probe(dev, hc->irq,
				     "o GPIO do ECHO nao tem IRQ\n");

	ret = devm_request_irq(dev, hc->irq, hcsr04_echo_irq,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			       dev_name(dev), hc);
	if (ret)
		return dev_err_probe(dev, ret, "falha ao registrar a IRQ\n");

	hc->misc.minor	= MISC_DYNAMIC_MINOR;
	hc->misc.name	= "hcsr04";
	hc->misc.fops	= &hcsr04_fops;
	hc->misc.mode	= 0444;		/* /dev/hcsr04 somente leitura */
	hc->misc.parent	= dev;

	ret = misc_register(&hc->misc);
	if (ret)
		return dev_err_probe(dev, ret,
				     "falha ao registrar /dev/hcsr04\n");

	platform_set_drvdata(pdev, hc);
	dev_info(dev, "HC-SR04 pronto em /dev/hcsr04 (IRQ %d)\n", hc->irq);
	return 0;
}

/* GPIOs e IRQ sao liberados automaticamente (recursos devm_*) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void hcsr04_remove(struct platform_device *pdev)
#else
static int hcsr04_remove(struct platform_device *pdev)
#endif
{
	struct hcsr04 *hc = platform_get_drvdata(pdev);

	misc_deregister(&hc->misc);
	dev_info(&pdev->dev, "HC-SR04 removido\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static const struct of_device_id hcsr04_of_match[] = {
	{ .compatible = "unisanta,hcsr04" },
	{ }
};
MODULE_DEVICE_TABLE(of, hcsr04_of_match);

static struct platform_driver hcsr04_driver = {
	.probe	= hcsr04_probe,
	.remove	= hcsr04_remove,
	.driver	= {
		.name		= "hcsr04",
		.of_match_table	= hcsr04_of_match,
	},
};
module_platform_driver(hcsr04_driver);

MODULE_AUTHOR("Grupo 01 - TP9 Sistemas Embarcados");
MODULE_DESCRIPTION("Driver para o sensor ultrassonico HC-SR04 (/dev/hcsr04)");
MODULE_LICENSE("GPL");
