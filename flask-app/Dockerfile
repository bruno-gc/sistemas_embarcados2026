FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

# o Flask escuta na 5000; o docker run publica como porta 80 da Raspberry
EXPOSE 5000

CMD ["python", "app.py"]
