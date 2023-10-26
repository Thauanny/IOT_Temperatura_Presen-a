#include "DHT.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

const char* wifi_ssid = "Gabriel’s iPhone";
const char* wifi_password = "umasenha";
int wifi_timeout = 100000;

const char* mqtt_broker = "io.adafruit.com";
const int mqtt_port = 1883;
int mqtt_timeout = 10000;

const char* mqtt_usernameAdafruitIO = "Gabsop";
const char* mqtt_keyAdafruitIO = "aio_hgRK23qm6Mf5zNvXLsyGJ4CpMm62";

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

  // Configurando Wifi
  WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
  WiFi.begin(wifi_ssid, wifi_password);
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);


  // Configurando Pinos
  pinMode(ENTRY_PIN, INPUT);
  pinMode(EXIT_PIN, INPUT);

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
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexão com WiFi falhou!");
  } else {
    Serial.print("Conectado com o IP: ");
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
