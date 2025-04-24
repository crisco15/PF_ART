#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#define DHTPIN 23
#define DHTTYPE DHT11

BH1750 lightMeter;
MCUFRIEND_kbv tft;
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "NELVIS";
const char* password = "092115nac";
const char* serverUrl = "http://presart.ddns.net/sensor";
const char* mqtt_server = "3.87.63.131";
bool alertaActiva = false;
unsigned long alertaInicio = 0;
const unsigned long duracionAlerta = 10000; // 10 segundos
String mensajeAlerta = "";

unsigned long previousMillis = 0;
const long interval = 10000; // Intervalo de 10 segundos para enviar datos

void callback(char* topic, byte* payload, unsigned int length) {
    mensajeAlerta = "";
    for (int i = 0; i < length; i++) {
        mensajeAlerta += (char)payload[i];
    }
    Serial.println("Mensaje MQTT recibido: " + mensajeAlerta);

    alertaActiva = true;
    alertaInicio = millis();
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {  // Limitar intentos
        delay(1000);
        Serial.print(".");
        intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConectado a WiFi");
    } else {
        Serial.println("\nNo se pudo conectar a WiFi, reiniciando...");
        ESP.restart();  // Reinicia el ESP32 si no se conecta
    }

    tft.reset();
    uint16_t ID = tft.readID();
    tft.begin(ID);
    tft.fillScreen(0x0000);  // Fondo negro
    tft.setRotation(-1);  // Ajusta la orientación según tu pantalla

    dht.begin();
    Wire.begin(19, 18);
    lightMeter.begin();

    tft.setTextColor(0xFFFF);  // Texto blanco
    tft.setTextSize(2);

    Serial.println("Sensor BH1750 iniciado");
    delay(2000);
}

void reconnect() {
    while (!client.connected()) {
      if (client.connect("ESP32Client")) {
        client.subscribe("arte/alertas");
      } else {
        delay(5000);
      }
    }
}

void dibujarIconoAlerta(int x, int y) {
    // Triángulo amarillo (advertencia)
    tft.fillTriangle(
        x, y + 60,        // Punto inferior izquierdo
        x + 60, y + 60,   // Punto inferior derecho
        x + 30, y         // Punto superior
        , TFT_YELLOW
    );

    // Borde negro del triángulo
    tft.drawTriangle(
        x, y + 60,
        x + 60, y + 60,
        x + 30, y,
        TFT_BLACK
    );

    // Signo de exclamación
    tft.fillRect(x + 28, y + 20, 4, 20, TFT_BLACK);  // línea vertical
    tft.fillCircle(x + 30, y + 45, 3, TFT_BLACK);    // punto inferior
}

void loop() {
    client.loop();

    if (millis() - alertaInicio < duracionAlerta) {
        tft.fillScreen(TFT_RED);
        dibujarIconoAlerta(10, 10);  // posición del triángulo en pantalla
    
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(3);
        tft.setCursor(80, 20);
        tft.println("¡ALERTA!");
    
        tft.setTextSize(2);
        tft.setCursor(10, 90);
        tft.println("Precaución:");
        tft.setCursor(10, 120);
        tft.println(mensajeAlerta);
        return;
    }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        if (WiFi.status() == WL_CONNECTED) {
            float h = dht.readHumidity();
            float t = dht.readTemperature();
            float lux = lightMeter.readLightLevel();

            if (isnan(h) || isnan(t)) {
                Serial.println(F("Failed to read from DHT sensor!"));
                return;
            }

            Serial.printf("Humedad: %.2f%%  Temperatura: %.2f°C  Luminosidad: %.2f lux\n", h, t, lux);
        
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(10, 20);
            tft.setTextSize(2);
            tft.setTextColor(TFT_WHITE);

            tft.println("Datos del sensor:");
            tft.setCursor(10, 50);
            tft.print("Temp: ");
            tft.print(t, 2);
            tft.print(" C");

            tft.setCursor(10, 80);
            tft.print("Humedad: ");
            tft.print(h, 2);
            tft.print(" %");

            tft.setCursor(10, 110);
            tft.print("Luz: ");
            tft.print(lux, 2);
            tft.print(" lx");

            HTTPClient http;
            http.begin(serverUrl);
            http.addHeader("Content-Type", "application/json");

            String jsonData = "{\"temperatura\":" + String(t, 2) + ",\"humedad\":" + String(h, 2) + ",\"lux\":" + String(lux,2) + "}";
            int httpResponseCode = http.POST(jsonData);
            http.setTimeout(5000);

            Serial.print("Código de respuesta: ");
            Serial.println(httpResponseCode);

            if (httpResponseCode == 200) {
                Serial.println("Datos enviados correctamente.");
                Serial.println("Respuesta del servidor: " + http.getString());
            } else {
                Serial.println("Error al enviar datos.");
            }

            http.end();

            if (!client.connected()) {
                reconnect();
            }
        } else {
            Serial.println("WiFi desconectado. Intentando reconectar...");
            WiFi.disconnect();
            WiFi.reconnect();
        }
    }
}