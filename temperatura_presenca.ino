#include "DHT.h"
#include <FS.h>
#include <Arduino.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>

struct Student {
  String name;
  String UID;
  bool state;
};

struct Student students[2] = {
  {"Gabriel", "99 135 252 148", 0},
  {"Lucas", "115 46 136 17", 0}
};

int num=0;
String str;
String s;
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

const char* wifi_ssid = "Gabriel’s iPhone";
const char* wifi_password = "umasenha";
int wifi_timeout = 100000;

const char* mqtt_broker = "io.adafruit.com";
const int mqtt_port = 1883;
int mqtt_timeout = 10000;

const char* mqtt_usernameAdafruitIO = "Gabsop";
const char* mqtt_keyAdafruitIO = "aio_gnHE33fZI3Lgq86hm0Svn7hM8VkJ";

#define ONBOARD_LED 2
#define DHTPIN 4
#define DHTTYPE DHT11
#define MIN_PEOPLE_TO_ADJUST 3

// RFID Pins
#define SS_PIN  5  // ESP32 pin GPIO5 
#define RST_PIN 27 // ESP32 pin GPIO27

// Telegram
#define BOTtoken = "6811241521:AAERWvA5qVXYoD-Yx3OqwQ3Txa5hHOV9s0Y";

MFRC522 rfid(SS_PIN, RST_PIN);
DHT dht(DHTPIN, DHTTYPE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

int people = 0;
int targetTemperature = 25;

void setup() {
  Serial.begin(9600);
  
  Serial.println(F("Begin!"));
  Serial.print(F("People: "));
  Serial.println(people);

  // Configurando Wifi
  WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
  WiFi.begin(wifi_ssid, wifi_password);
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  
  // RFID
  SPI.begin(); // init SPI bus
  rfid.PCD_Init(); // init MFRC522 

  // Biblioteca de data
  timeClient.begin();
  timeClient.setTimeOffset(-10800);
  timeClient.update();

  // Arquivo
  // Formatar arquivo ao iniciar
  // formatFile();
  openFS();
  updateVariables("/alunos.txt");

  // Configurando Pinos
  pinMode(ONBOARD_LED,OUTPUT);
  dht.begin();
}

void loop() {
  delay(500);
  timeClient.update();

  if (!mqtt_client.connected()) { // Se MQTT não estiver conectado
    connectMQTT();
  }

  readCard();
}

void updateVariables(String path) {
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r");//r+ leitura e escrita
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    String date = timeClient.getFormattedDate();
    int indexOfT = date.indexOf("T");
    date.remove(indexOfT, date.length());
    while (rFile.position() < rFile.size()) {
      s = rFile.readStringUntil('\n');
      for (int i = 0; i < 2; ++i) {
        if(s.indexOf(students[i].name) > 0 && s.indexOf(date) > 0 && s.indexOf("Entrou") > 0) {
          students[i].state = 1;
          people++;
        }
        if(s.indexOf(students[i].name) > 0 && s.indexOf(date) > 0 && s.indexOf("Saiu") > 0) {
          students[i].state = 0;
          people--;
        }
      }
      s.trim();
      Serial.println(s);
    }
    rFile.close(); 
  }
}

void readCard() {
if (rfid.PICC_IsNewCardPresent()) { // new tag is available
    if (rfid.PICC_ReadCardSerial()) { // NUID has been readed

      String UID;
      Serial.print("UID:");
      for (int i = 0; i < rfid.uid.size; i++) {
        UID.concat(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        UID.concat(rfid.uid.uidByte[i]);
        UID.trim();
      }                 
   
      String date = timeClient.getFormattedDate();
      String time = timeClient.getFormattedTime();
      int indexOfT = date.indexOf("T");
      date.remove(indexOfT, date.length());

      float humidity = dht.readHumidity();
      float temperature = dht.readTemperature();

      if (isnan(humidity) || isnan(temperature)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
      }

      String studentName;
      String state = "Entrou";
      for (int i = 0; i < 2; ++i) {
        if (students[i].UID == UID) {
          studentName = students[i].name;
          if(students[i].state == 0) {
            state = "Entrou";
            people++;
            Serial.print(F("People: "));
            Serial.println(people);
            students[i].state = 1;
          }
          else {
            state = "Saiu";
            people--;
            Serial.print(F("People: "));
            Serial.println(people);
            students[i].state = 0;
          }
        }
      }

      if(studentName == "") {
        studentName = "Estudante não cadastrado";
      }
      
      writeFile("ID: " + String(UID) + " Nome: " + studentName + " Data: " + date + " Hora: " + time + " Estado: " + state , "/alunos.txt"); 
      if(mqtt_client.connected()) {
        mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.people-in-room", String(people).c_str(), true);
        if (people % MIN_PEOPLE_TO_ADJUST == 0) {
          adjustTemperatureMessage(false, temperature, humidity); // Decrease temperature
        } else {
          adjustTemperatureMessage(true, temperature, humidity); // Increase temperature
        }
      }     

      rfid.PICC_HaltA(); // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
  }
}

void adjustTemperatureMessage(bool increase, float temperature, float humidity) {
  if (!increase) {
    Serial.println("Increase temperature on the air conditioner.");
    targetTemperature += 1;
  } else {
    Serial.println("Decrease temperature on the air conditioner.");
    targetTemperature -= 1;
  }

  if (mqtt_client.connected()){
    mqtt_client.loop();
    
    Serial.print("Target Temperature: ");
    Serial.println(targetTemperature);
    
    mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.target-temperature", String(targetTemperature).c_str(), true);
    mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.should-increase-or-decrease", String(increase).c_str(), true);
    mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.temperature", String(temperature).c_str(), true);
    mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.humidity", String(humidity).c_str(), true);
    delay(1000);
  }
}

void connectWiFi() {
  Serial.print("Conectando à rede WiFi");

  unsigned long tempoInicial = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - tempoInicial < wifi_timeout)) {
    digitalWrite(ONBOARD_LED,HIGH);
    Serial.print(".");
    delay(500);
    digitalWrite(ONBOARD_LED,LOW);
    delay(500);
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexão com WiFi falhou!");
    digitalWrite(ONBOARD_LED,LOW);
  } else {
    Serial.print("Conectado com o IP: ");
    digitalWrite(ONBOARD_LED,HIGH);
    Serial.println(WiFi.localIP());
  }
}

void connectMQTT() {
  unsigned long tempoInicial = millis();
  while (!mqtt_client.connected() && (millis() - tempoInicial < mqtt_timeout)) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }
    Serial.print("Conectando ao MQTT Broker..");

    if (mqtt_client.connect("ESP32Client", mqtt_usernameAdafruitIO, mqtt_keyAdafruitIO)) {
      Serial.println();
      Serial.print("Conectado ao broker MQTT!");
      mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.people-in-room", String(people).c_str(), true);
    } else {
      Serial.println();
      Serial.print("Conexão com o broker MQTT falhou!");
      delay(500);
    }
  }
  Serial.println();
}

void writeFile(String string, String path) { //escreve conteúdo em um arquivo
  File rFile = SPIFFS.open(path, "a");//a para anexar
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("tamanho");
    Serial.println(rFile.size());
    rFile.println(string);
    Serial.print("Gravou: ");
    Serial.println(string);
  }
  rFile.close();
}


String readFile(String path) {
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r");//r+ leitura e escrita
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("----------Lendo arquivo ");
    Serial.print(path);
    Serial.println("  ---------");
    while (rFile.position() < rFile.size())
    {
      s = rFile.readStringUntil('\n');
      s.trim();
      Serial.println(s);
    }
    rFile.close();
    return s;
  }
}

void formatFile() {
  Serial.println("Formantando SPIFFS");
  SPIFFS.format();
  Serial.println("Formatou SPIFFS");
}


void openFS(void) {
  if (!SPIFFS.begin()) {
    Serial.println("\nErro ao abrir o sistema de arquivos");
  }
  else {
    Serial.println("\nSistema de arquivos aberto com sucesso!");
  }
}
