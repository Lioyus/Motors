#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Configuration WiFi & Serveur
const char* ssid = "ton ssid";
const char* password = "ton mot de passe";
const bool useProductionServer = true;
const char* localServerUrl = "http://10.48.94.45:3000";
const char* productionServerUrl = "https://motors-7luf.onrender.com";
const char* serverUrl = useProductionServer ? productionServerUrl : localServerUrl;

// Pins L298N
const int ENA = 14; 
const int IN1 = 27;
const int IN2 = 26;

// Configuration PWM pour ESP32
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

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

void setup() {
  Serial.begin(115200);
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  
  // Setup PWM
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(ENA, ledChannel);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK !");
  Serial.println(serverUrl);
}

void notifierFin() {
  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;
  if (!beginHttpClient(http, client, secureClient, String(serverUrl) + "/fini")) {
    Serial.println("Impossible d'ouvrir la connexion vers /fini");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST("{}"); // Signale la fin au serveur
  if (httpResponseCode <= 0) {
    Serial.printf("Erreur POST /fini: %s\n", http.errorToString(httpResponseCode).c_str());
  } else {
    Serial.printf("POST /fini -> %d\n", httpResponseCode);
  }
  http.end();
}

void runMotor(int ms_par_tour, int nbr, int speed) {
  long tempsTotal = (long)ms_par_tour * nbr;
  Serial.printf("Rotation pour %d tours (%ld ms)\n", nbr, tempsTotal);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  ledcWrite(ledChannel, speed);
  
  delay(tempsTotal); 
  
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ledChannel, 0);

  notifierFin(); // Libère l'interface web
}

void loop() {
  ensureWifi();

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;
    WiFiClientSecure secureClient;
    if (!beginHttpClient(http, client, secureClient, String(serverUrl) + "/commande.json")) {
      Serial.println("Impossible d'ouvrir la connexion vers /commande.json");
      delay(1000);
      return;
    }
    
    int httpCode = http.GET();
    if (httpCode == 200) {
      StaticJsonDocument<200> doc;
      String payload = http.getString();
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.printf("JSON invalide: %s\n", error.c_str());
        Serial.println("Reponse brute recue:");
        Serial.println(payload);
        http.end();
        delay(1000);
        return;
      }

      const char* action = doc["action"];
      if (action && strcmp(action, "run") == 0) {
        int ms = doc["ms_par_tour"];
        int n = doc["nbr"];
        runMotor(ms, n, 255);
      }
    } else if (httpCode > 0) {
      Serial.printf("GET /commande.json -> %d\n", httpCode);
    } else {
      Serial.printf("Connexion echouee: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  delay(1000); // Vérifie le serveur toutes les secondes
}
