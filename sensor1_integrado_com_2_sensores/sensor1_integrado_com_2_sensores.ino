#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ThingSpeak.h>
#include <WiFi.h>
#include "sd_read_write.h"
#include "HardwareSerial.h"

// =====================
// Dados da rede Wi-Fi
// =====================
const char ssid[] = "caprinovi/nesatec";
const char password[] = "C@prinovi";

// =====================
// Configurações ThingSpeak
// =====================
WiFiClient client;
const long CHANNEL = 3042773;
const char *WRITE_API = "TINUHYUAB8DTO9Z5";

// =====================
// Display OLED
// =====================
static SSD1306Wire display(0x3C, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// =====================
// SPI (SD Card)
// =====================
SPIClass sd_spi(HSPI);

// =====================
// UART do sensor RS485
// =====================
#define RX_SENSOR 33
#define TX_SENSOR 34
#define RE 48   // pino ligado ao DE e RE do módulo RS485
#define bussControl 45
#define sensorControl 47

HardwareSerial mod(1);  // UART1 para o sensor

// =====================
// Variáveis globais
// =====================
long prevMillisThingSpeak = 0;
int intervalThingSpeak = 300000;
volatile uint32_t contadorLeituras = 0;

// Protótipos
void sdInit(void);
void gravaDados(float temp_1, float umid_1, float cond_1, float temp_2, float umid_2, float cond_2);
void readSensor_1(float *temperatura, float *umidade, float *condutividade);
void readSensor_2(float *temperatura, float *umidade, float *condutividade);
uint16_t ModRTU_CRC(uint8_t buf[], int len);

//=====================
// Energia externa
// =====================
void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}

// =====================
// SETUP
// =====================
void setup() {
  VextON();
  delay(200);

  Serial.begin(115200);
  Serial.println("\nIniciando sensor 1");

  // Inicializa UART do sensor
  mod.begin(9600, SERIAL_8N1, RX_SENSOR, TX_SENSOR);
  pinMode(RE, OUTPUT);
  digitalWrite(RE, LOW); // modo recepção

  pinMode(bussControl, OUTPUT);
  pinMode(sensorControl, OUTPUT);

  digitalWrite(bussControl, LOW);
  digitalWrite(sensorControl, LOW);

  // Inicializa SD
  sdInit();

   File file = SD.open("/data.txt");
  if (!file) {
    Serial.println("Criando arquivo no SD...");
    writeFile(
    SD,
    "/data.csv",
    "Leitura,Data,Hora,Temperatura_S1(°C),Umidade_S1(%),Condutividade_S1(uS/cm),Temperatura_S2(°C),Umidade_S2(%),Condutividade_S2(uS/cm),Millis\r\n"); 
}else {
    Serial.println("Arquivo já existe.");
  }
  file.close();

  // Conecta Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(1000);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());
 
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;

  while(!getLocalTime(&timeinfo))
  {
    Serial.println("Aguardando sincronização NTP...");
    delay(1000);
  }

Serial.println("Relógio sincronizado!");


  // Inicializa ThingSpeak
  ThingSpeak.begin(client);

  // Inicializa display
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 24, "Inicializando...");
  display.display();

  display.clear();
  display.drawString(64, 24, "sistema pronto!");
  display.display();
  delay(1000);
}

// =====================
// LOOP PRINCIPAL
// =====================
void loop() {
  float temperatura_1 = 0, umidade_1 = 0, condutividade_1 = 0;
  float temperatura_2 = 0, umidade_2 = 0, condutividade_2 = 0;

    contadorLeituras++;

   readSensor_1(&temperatura_1, &umidade_1, &condutividade_1);
   delay(200);
   
   for (int tentativa = 1; tentativa <= 3; tentativa++) {

    readSensor_2(&temperatura_2, &umidade_2, &condutividade_2);

    // Sai se a leitura foi válida
    if (!(temperatura_2 == 0 &&
          umidade_2 == 0 &&
          condutividade_2 == -1))
    {
        if (tentativa > 1) {
            Serial.printf("Sensor 2 respondeu na tentativa %d\n", tentativa);
        }
        break;
    }

    Serial.printf("Tentativa %d falhou!\n", tentativa);

    delay(300);
}

    // Envia dados ao ThingSpeak
    if (millis() - prevMillisThingSpeak > intervalThingSpeak) {
      
      if (WiFi.status() == WL_CONNECTED) {
      ThingSpeak.setField(1, temperatura_1);
      ThingSpeak.setField(2, umidade_1);
      ThingSpeak.setField(3, condutividade_1);

      ThingSpeak.setField(4, temperatura_2);
      ThingSpeak.setField(5, umidade_2);
      ThingSpeak.setField(6, condutividade_2);

      int x = ThingSpeak.writeFields(CHANNEL, WRITE_API);

      if (x == 200)
      Serial.println("Update no ThingSpeak OK");
      else 
      Serial.println("Erro HTTP: " + String(x));

    } else {
      Serial.println("WiFi desconectado!");

      prevMillisThingSpeak = millis();
    }
    }
    /// Serial monitor
    Serial.println("===== SENSOR 1 =====");
    Serial.printf("T: %.1f C\n", temperatura_1);
    Serial.printf("U: %.1f %%\n", umidade_1);
    Serial.printf("C: %.0f uS/cm\n", condutividade_1);

    Serial.println();

    Serial.println("===== SENSOR 2 =====");
    Serial.printf("T: %.1f C\n", temperatura_2);
    Serial.printf("U: %.1f %%\n", umidade_2);
    Serial.printf("C: %.0f uS/cm\n", condutividade_2);

    // Display
    // Display
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // Sensor 1
    display.drawString(0, 0,  "S1");
    display.drawString(0, 12, "T:" + String(temperatura_1,1) + "C");
    display.drawString(0, 24, "U:" + String(umidade_1,1) + "%");
    display.drawString(0, 36, "C:" + String(condutividade_1,0));

    // Sensor 2
    display.drawString(64, 0,  "S2");
    display.drawString(64, 12, "T:" + String(temperatura_2,1) + "C");
    display.drawString(64, 24, "U:" + String(umidade_2,1) + "%");
    display.drawString(64, 36, "C:" + String(condutividade_2,0));

    display.display();

    // Grava no SD
    gravaDados(temperatura_1, umidade_1, condutividade_1, temperatura_2, umidade_2, condutividade_2);

    delay(500);
  }

  

// =====================
// Lê sensor RS485
// =====================
void readSensor_1(float *temperatura, float *umidade, float *condutividade) {

 const uint8_t CMD[] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xF8};
 // const uint8_t CMD[] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xCB};
  uint8_t buffer[11];
  uint8_t i = 0;

  //  LIGA SENSOR 
  digitalWrite(bussControl, HIGH);
  delay(1000);

  digitalWrite(sensorControl, HIGH);
  delay(2000);   // tempo de estabilização real

  //  Limpa buffer
  while (mod.available()) mod.read();

  // Envia comando
  digitalWrite(RE, HIGH);
  delay(5);

  mod.write(CMD, sizeof(CMD));
  mod.flush();

  digitalWrite(RE, LOW);

  // 🔹 Aguarda resposta
  uint32_t timeout = millis();
  while (i < 11 && (millis() - timeout) < 1000) {
    if (mod.available()) {
      buffer[i++] = mod.read();
    }
  }

  
  if (i >= 9) {
    uint16_t rawHum  = (buffer[3] << 8) | buffer[4];
    uint16_t rawTemp = (buffer[5] << 8) | buffer[6];
    uint16_t rawCond = (buffer[7] << 8) | buffer[8];

    *umidade = rawHum / 10.0;
    *temperatura = rawTemp / 10.0;
    *condutividade = rawCond;
  } 
  else {
    Serial.println("Falha na leitura do sensor 1");
    *temperatura = *umidade = *condutividade = 0;
  }

  //  DESLIGA SENSOR
  digitalWrite(sensorControl, LOW);
  delay(200);

  digitalWrite(bussControl, LOW);
}

void readSensor_2(float *temperatura, float *umidade, float *condutividade) {

  const uint8_t CMD[] = {0x01, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xCB};
  uint8_t buffer[11];
  uint8_t i = 0;

  //  LIGA SENSOR 
  digitalWrite(bussControl, HIGH);
  delay(1000);

  digitalWrite(sensorControl, HIGH);
  delay(2000);   // tempo de estabilização real

  //  Limpa buffer
  while (mod.available()) mod.read();

  // Envia comando
  digitalWrite(RE, HIGH);
  delay(5);

  mod.write(CMD, sizeof(CMD));
  mod.flush();

  digitalWrite(RE, LOW);

  // 🔹 Aguarda resposta
  uint32_t timeout = millis();
  while (i < 11 && (millis() - timeout) < 1000) {
    if (mod.available()) {
      buffer[i++] = mod.read();
    }
  }

  
  if (i == 9) {

    uint16_t crcCalculado = ModRTU_CRC(buffer, 7);

    uint16_t crcRecebido =
        buffer[7] |
        (buffer[8] << 8);

    if (crcCalculado == crcRecebido &&
        buffer[0] == 0x01 &&
        buffer[1] == 0x03)
    {

        uint16_t rawHum  = (buffer[3] << 8) | buffer[4];
        uint16_t rawTemp = (buffer[5] << 8) | buffer[6];

        *umidade = rawHum / 10.0;
        *temperatura = rawTemp / 10.0;

    }
    else {

        Serial.println("CRC inválido Sensor 2");

        *temperatura = 0;
        *umidade = 0;

    }

}

    // =====================
// LEITURA CONDUTIVIDADE
// =====================

const uint8_t CMD_EC[] = {
    0x01, 0x03, 0x00, 0x15,
    0x00, 0x01, 0x95, 0xCE
};

uint16_t rawEC = 0;
bool leituraValida = false;

// Faz até 3 tentativas
for (int tentativa = 1; tentativa <= 3; tentativa++) {

    if (tentativa > 1) {
        Serial.printf("Nova tentativa EC (%d)\n", tentativa);
        delay(300);
    }

    while (mod.available()) mod.read();

    digitalWrite(RE, HIGH);
    delay(5);

    mod.write(CMD_EC, sizeof(CMD_EC));
    mod.flush();

    digitalWrite(RE, LOW);

    i = 0;
    timeout = millis();

    while (i < 7 && (millis() - timeout) < 1000) {

        if (mod.available()) {

            buffer[i++] = mod.read();

        }

    }

   if(i == 7)
{

    uint16_t crcCalculado = ModRTU_CRC(buffer,5);

    uint16_t crcRecebido =
        buffer[5] |
        (buffer[6] << 8);

    if(crcCalculado == crcRecebido &&
       buffer[0] == 0x01 &&
       buffer[1] == 0x03)
    {

        rawEC = (buffer[3] << 8) | buffer[4];

        Serial.print("EC recebida: ");
        Serial.println(rawEC);

        if(rawEC != 0)
        {
            leituraValida = true;
            break;
        }

        Serial.println("EC = 0");

    }
    else
    {

        Serial.println("CRC inválido na EC");

    }

}

if (leituraValida) {

    *condutividade = rawEC;

}
else {

    // mantém 0 caso realmente tenha vindo 0
    *condutividade = rawEC;

}

  //  DESLIGA SENSOR
  digitalWrite(sensorControl, LOW);
  delay(200);

  digitalWrite(bussControl, LOW);
}
}

// =====================
// Inicialização do SD
// =====================
void sdInit(void) {
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sd_spi)) {
    Serial.println("Falha ao montar SD");
    return;
  }
  Serial.println("Cartão SD pronto.");
}

// =====================
// Grava dados no SD
// =====================
void gravaDados(float temp_1,
                float umid_1,
                float cond_1,
                float temp_2,
                float umid_2,
                float cond_2)
{

    struct tm timeinfo;

    String data = "SemData";
    String hora = "SemHora";

    if(getLocalTime(&timeinfo))
    {
        char dataBuffer[11];
        char horaBuffer[9];

        strftime(dataBuffer,sizeof(dataBuffer),"%d/%m/%Y",&timeinfo);
        strftime(horaBuffer,sizeof(horaBuffer),"%H:%M:%S",&timeinfo);

        data = String(dataBuffer);
        hora = String(horaBuffer);
    }

    String linha =
        String(contadorLeituras) + "," +
        data + "," +
        hora + "," +
        String(temp_1,1) + "," +
        String(umid_1,1) + "," +
        String(cond_1,0) + "," +
        String(temp_2,1) + "," +
        String(umid_2,1) + "," +
        String(cond_2,0) + "," +
        String(millis()) +
        "\r\n";

    appendFile(SD,"/data.csv",linha.c_str());
}

uint16_t ModRTU_CRC(uint8_t buf[], int len)
{
    uint16_t crc = 0xFFFF;

    for (int pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buf[pos];

        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}
