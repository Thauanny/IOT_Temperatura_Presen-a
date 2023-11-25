#include "DHT.h"
#include <FS.h>
#include <Arduino.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

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
const char* mqtt_keyAdafruitIO = "aio_sAzW76HtuoLbdgIbtJ3i7AciBblX";

#define ONBOARD_LED 2
#define DHTPIN 4
#define DHTTYPE DHT11

#define ENTRY_PIN 22
#define EXIT_PIN 23
#define MIN_PEOPLE_TO_ADJUST 3

DHT dht(DHTPIN, DHTTYPE);

int people = 0;
int targetTemperature = 25;

void setup() {
  Serial.begin(9600);
  
  Serial.println(F("Begin!"));
  Serial.print(F("People: "));
  Serial.println(people);

  //Arquivo
  //formatar arquivo ao iniciar
  formatFile();
  //se precisar ler descomentar esse trecho
  //Serial.println("abrir arquivo");
  //openFS();
  //String alunos = readFile("/alunos.txt");

  // Fim Arquivo

  // Configurando Wifi
  WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
  WiFi.begin(wifi_ssid, wifi_password);
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);


  // Configurando Pinos
  pinMode(ENTRY_PIN, INPUT);
  pinMode(EXIT_PIN, INPUT);
  pinMode(ONBOARD_LED,OUTPUT);

  dht.begin();
}

void loop() {
  delay(500);

  if (!mqtt_client.connected()) { // Se MQTT não estiver conectado
    connectMQTT();
  }

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  if (digitalRead(ENTRY_PIN) == HIGH && mqtt_client.connected()) {
    people++;
    Serial.print(F("People: "));
    Serial.println(people);
    mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.people-in-room", String(people).c_str(), true);
    //substituir pelo codigo do RFID
    writeFile("ID: " + String(3123123) + "Nome: " + "Thauanny" + "Data: " + "20/06/2321" + "Estado: " + "Entrou", "/alunos.txt");

    Serial.println(F("Someone entered the room"));
    if (people % MIN_PEOPLE_TO_ADJUST == 0) {
      adjustTemperatureMessage(true, temperature, humidity); // Increase temperature
    }
  }

  if (digitalRead(EXIT_PIN) == HIGH && mqtt_client.connected() && people > 0) {
    people--;
    Serial.print(F("People: "));
    Serial.println(people);
    mqtt_client.publish("Gabsop/feeds/tempwarden-pro-max-v2.people-in-room", String(people).c_str(), true);
    //substituir pelo codigo do RFID
    writeFile("ID: " + String(3123123) + "Nome: " + "Thauanny" + "Data: " + "20/06/2321" + "Estado: " + "Saiu" , "/alunos.txt");
    Serial.println(F("Someone left the room"));
    if (people % MIN_PEOPLE_TO_ADJUST == 0) {
      adjustTemperatureMessage(false, temperature, humidity); // Decrease temperature
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
