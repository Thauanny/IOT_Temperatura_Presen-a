#include "DHT.h"

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

  pinMode(ENTRY_PIN, INPUT);
  pinMode(EXIT_PIN, INPUT);

  dht.begin();
}

void loop() {
  delay(2000);

  Serial.println(digitalRead(ENTRY_PIN));

  if (digitalRead(ENTRY_PIN) == LOW) {
    people++;
    Serial.print(F("People: "));
    Serial.println(people);
    if (people % MIN_PEOPLE_TO_ADJUST == 0) {
      adjustTemperatureMessage(true); // Increase temperature
    }
  }
  if (digitalRead(EXIT_PIN) == LOW) {
    if (people > 0) {
      people--;
      Serial.print(F("People: "));
      Serial.println(people);
      if (people % MIN_PEOPLE_TO_ADJUST == 0) {
        adjustTemperatureMessage(false); // Decrease temperature
      }
    }
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C Heat Index: "));
  Serial.print(hic);
  Serial.println(F("°C "));
}

void adjustTemperatureMessage(bool increase) {
  if (increase) {
    Serial.println("Increase temperature on the air conditioner.");
    targetTemperature += 1;
  } else {
    Serial.println("Decrease temperature on the air conditioner.");
    targetTemperature -= 1;
  }
  Serial.print("Target Temperature: ");
  Serial.println(targetTemperature);
}
