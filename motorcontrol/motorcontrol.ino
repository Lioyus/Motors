#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Configuration WiFi & Serveur
const char* ssid = "Evolve Wifi Orange 2.4GHz";
const char* password = "Evolve@123";
const bool useProductionServer = true;
const char* localServerUrl = "http://10.48.94.45:3000";
const char* productionServerUrl = "https://motors-7luf.onrender.com";
const char* serverUrl = useProductionServer ? productionServerUrl : localServerUrl;

const int motorCount = 3;
const int enaPins[motorCount] = {14, 25, 33};
const int in1Pins[motorCount] = {27, 18, 23};
const int in2Pins[motorCount] = {26, 19, 22};
const int ledChannels[motorCount] = {0, 1, 2};

const int freq = 5000;
const int resolution = 8;

struct MotorCommand {
  int id;
  const char* action;
  int ms_par_tour;
  int nbr;
};

bool beginHttpClient(HTTPClient& http, WiFiClient& client, WiFiClientSecure& secureClient, const String& url) {
  http.useHTTP10(true);

  if (url.startsWith("https://")) {
    secureClient.setInsecure();
    return http.begin(secureClient, url);
  }

  return http.begin(client, url);
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Reconnexion WiFi...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnecte");
  } else {
    Serial.println("\nEchec reconnexion WiFi");
  }
}

void stopMotor(int index) {
  digitalWrite(in1Pins[index], LOW);
  digitalWrite(in2Pins[index], LOW);
  ledcWrite(ledChannels[index], 0);
}

void notifierFin(int motorId) {
  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;
  if (!beginHttpClient(http, client, secureClient, String(serverUrl) + "/fini")) {
    Serial.println("Impossible d'ouvrir la connexion vers /fini");
    return;
  }

  StaticJsonDocument<64> body;
  body["motorId"] = motorId;
  String payload;
  serializeJson(body, payload);

  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(payload);
  if (httpResponseCode <= 0) {
    Serial.printf("Erreur POST /fini moteur %d: %s\n", motorId, http.errorToString(httpResponseCode).c_str());
  } else {
    Serial.printf("POST /fini moteur %d -> %d\n", motorId, httpResponseCode);
  }
  http.end();
}

void runMotor(int index, int ms_par_tour, int nbr, int speed) {
  long tempsTotal = (long)ms_par_tour * nbr;
  Serial.printf("Moteur %d pour %d tours (%ld ms)\n", index + 1, nbr, tempsTotal);

  digitalWrite(in1Pins[index], HIGH);
  digitalWrite(in2Pins[index], LOW);
  ledcWrite(ledChannels[index], speed);

  delay(tempsTotal);

  stopMotor(index);
  notifierFin(index + 1);
}

void setupMotors() {
  for (int index = 0; index < motorCount; index++) {
    pinMode(in1Pins[index], OUTPUT);
    pinMode(in2Pins[index], OUTPUT);
    ledcSetup(ledChannels[index], freq, resolution);
    ledcAttachPin(enaPins[index], ledChannels[index]);
    stopMotor(index);
  }
}

void fetchCommands(MotorCommand* commands, size_t commandCount) {
  for (size_t index = 0; index < commandCount; index++) {
    commands[index].id = index + 1;
    commands[index].action = "idle";
    commands[index].ms_par_tour = 2600;
    commands[index].nbr = 1;
  }

  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;
  if (!beginHttpClient(http, client, secureClient, String(serverUrl) + "/commande.json")) {
    Serial.println("Impossible d'ouvrir la connexion vers /commande.json");
    return;
  }

  int httpCode = http.GET();
  if (httpCode == 200) {
    DynamicJsonDocument doc(1024);
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.printf("JSON invalide: %s\n", error.c_str());
      Serial.println("Reponse brute recue:");
      Serial.println(payload);
      http.end();
      return;
    }

    JsonArray motors = doc["motors"].as<JsonArray>();
    if (motors.isNull()) {
      Serial.println("Champ motors introuvable dans la reponse");
      http.end();
      return;
    }

    for (JsonObject motor : motors) {
      int motorId = motor["id"] | 0;
      if (motorId < 1 || motorId > motorCount) {
        continue;
      }

      int index = motorId - 1;
      commands[index].id = motorId;
      commands[index].action = motor["action"] | "idle";
      commands[index].ms_par_tour = motor["ms_par_tour"] | 2600;
      commands[index].nbr = motor["nbr"] | 1;
    }
  } else if (httpCode > 0) {
    Serial.printf("GET /commande.json -> %d\n", httpCode);
  } else {
    Serial.printf("Connexion echouee: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  setupMotors();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK !");
  Serial.println(serverUrl);
}

void loop() {
  ensureWifi();

  if (WiFi.status() == WL_CONNECTED) {
    MotorCommand commands[motorCount];
    fetchCommands(commands, motorCount);

    for (int index = 0; index < motorCount; index++) {
      if (strcmp(commands[index].action, "run") == 0) {
        runMotor(index, commands[index].ms_par_tour, commands[index].nbr, 255);
      }
    }
  }

  delay(1000);
}
