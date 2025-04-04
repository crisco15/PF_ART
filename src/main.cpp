#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <HTTPClient.h>
#define DHTPIN 23
#define DHTTYPE DHT11

BH1750 lightMeter;
MCUFRIEND_kbv tft;
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "NELVIS";
const char* password = "092115nac";
const char* serverUrl = "http://192.168.1.18:8000/sensor";

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

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

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        float lux = lightMeter.readLightLevel();

        if (isnan(h) || isnan(t)) {
            Serial.println(F("Failed to read from DHT sensor!"));
            return;
        }  

        Serial.printf("Humedad: %.2f%%  Temperatura: %.2f°C  Luminosidad: %.2f lux\n", h, t, lux);
    
        tft.fillRect(10, 30, 220, 80, 0x0000);  // Fondo negro sobre los valores previos

        // Escribir los valores en pantalla
        tft.fillScreen(TFT_BLACK);  // Borra la pantalla
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
    } else {
        Serial.println("WiFi desconectado. Intentando reconectar...");
        WiFi.disconnect();
        WiFi.reconnect();
    }
    delay(10000);
}