# Hardware — HC-SR04 na Raspberry Pi 4

Este documento descreve o hardware do TP9, as ligações entre o sensor e a Raspberry Pi 4 e o divisor de tensão que protege o GPIO24.

![Montagem do HC-SR04 com o divisor de tensão](../evidence/hardware/01-montagem.jpg)

## Componentes

| Componente | Qtd. | Função |
| --- | --- | --- |
| Raspberry Pi 4 Model B + fonte USB-C 5 V / 3 A | 1 | plataforma do sistema |
| Sensor ultrassônico HC-SR04 | 1 | mede a distância pelo tempo de voo do ultrassom |
| Resistor de 1 kΩ | 3 | divisor de tensão do ECHO |
| Protoboard e jumpers | — | montagem do circuito |
| microSD de 8 GB ou mais | 1 | imagem gerada pelo Buildroot |

## Funcionamento do HC-SR04

| Característica | Valor |
| --- | --- |
| Tensão de alimentação | 5 V |
| Pinos | VCC, TRIG, ECHO, GND |
| Faixa de medição | cerca de 2 cm a 400 cm |
| Pulso de disparo (TRIG) | 10 µs em nível alto |
| Saída (ECHO) | pulso em nível alto, em 5 V, com duração igual ao tempo de ida e volta do som |

O sensor recebe um pulso de 10 µs no TRIG, emite um burst de ultrassom e mantém o ECHO em nível alto até o eco voltar. Quanto mais longe o objeto, mais longo é o pulso no ECHO:

```
distância = tempo × velocidade_do_som / 2
distância_mm ≈ Δt_µs × 343 / 2000
```

## Ligações

| HC-SR04 | Função | Raspberry Pi 4 | Pino físico |
| --- | --- | --- | --- |
| VCC | Alimentação | 5 V | 2 |
| TRIG | Saída de trigger | GPIO23 (BCM) | 16 |
| ECHO | Entrada de echo | GPIO24 (BCM), pelo divisor | 18 |
| GND | Terra | GND | 6 |

No conector de 40 pinos, o pino 1 fica na ponta mais distante das portas USB e Ethernet. Os pinos ímpares ficam na fileira de dentro da placa, e os pares, na fileira da borda. Os quatro pinos usados (2, 6, 16 e 18) estão todos na fileira da borda.

## Divisor de tensão do ECHO

O pino ECHO do HC-SR04 trabalha em **5 V**, e as entradas GPIO da Raspberry Pi 4 operam em **3,3 V** e não toleram 5 V. Ligar o ECHO direto no GPIO24 pode danificar o pino. Por isso o ECHO passa por um divisor resistivo:

```
HC-SR04 ECHO
     │
   [1 kΩ]          R1
     │
     ├────────── GPIO24 (pino 18)
     │
   [1 kΩ]  ┐
     │     ├ R2 = 2 kΩ (dois resistores de 1 kΩ em série)
   [1 kΩ]  ┘
     │
    GND (pino 6)
```

A tensão que chega ao GPIO24 quando o ECHO está em nível alto é:

```
VGPIO = 5 V × R2 / (R1 + R2) = 5 V × 2k / (1k + 2k) ≈ 3,33 V
```

Esse valor fica dentro da faixa aceita pela entrada da Raspberry, e ainda é reconhecido como nível alto. Em nível baixo, o ECHO fica em 0 V, e o GPIO24 também.

Os 2 kΩ foram formados com **dois resistores de 1 kΩ em série**, porque o resistor de 1 kΩ é o valor mais comum e assim todo o divisor usa um único valor.

### Por que o TRIG não precisa de divisor

No TRIG, o sinal vai da Raspberry para o sensor. A Raspberry gera 3,3 V, e o HC-SR04 reconhece essa tensão como nível alto. Não há risco para nenhum dos lados.

## Por que GPIO23 e GPIO24

- São os pinos padronizados pelo enunciado do TP9.
- São GPIOs de uso geral, sem função alternativa (UART, I²C, SPI ou PWM) ativa por padrão na Raspberry Pi 4, então não disputam o pino com outro periférico.
- Os pinos físicos 16 e 18 ficam na mesma fileira do 5 V (pino 2) e do GND (pino 6), o que simplifica a fiação.
- O GPIO24 aceita interrupção nas duas bordas, que o driver usa para medir o pulso do ECHO.

Os pinos não estão fixos no código do driver: eles são descritos no Device Tree overlay (`br2-external-tp9/board/rpi4/hcsr04-overlay.dts`), nas propriedades `trig-gpios` e `echo-gpios`. Detalhes em [driver.md](driver.md).

## Conferência da montagem

1. Montar com a Raspberry desligada da tomada.
2. Conferir que o ECHO só chega ao pino 18 passando pelo resistor R1.
3. Conferir que nenhum fio liga o 5 V (pino 2) aos pinos 16 ou 18.
4. Com a Raspberry ligada e o sensor medindo (`cat /dev/hcsr04`), medir com multímetro entre o ponto central do divisor e o GND: a tensão deve ficar perto de 3,3 V, e nunca perto de 5 V.
