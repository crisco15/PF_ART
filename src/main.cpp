#include "DHT.h"
#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include <UTFTGLUE.h>
#include <HTTPClient.h>
#define DHTPIN 4
#define DHTTYPE DHT11

BH1750 lightMeter;
UTFTGLUE myGLCD(0,13,12,33,32,15);
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "nelvis";
const char* password = "092115nac";
const char* serverUrl = "http://192.168.80.31:8000/sensor";

void setup() {
    Serial.begin(115200);
    Wire.begin();
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

    dht.begin();
    lightMeter.begin();
    randomSeed(analogRead(0));
    // Setup the LCD
    myGLCD.InitLCD();
    myGLCD.setFont(SmallFont);
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
    int buf[478];
    int x, x2;
    int y, y2;
    int r;
  
  // Clear the screen and draw the frame
    myGLCD.clrScr();
  
    myGLCD.setColor(255, 0, 0);
    myGLCD.fillRect(0, 0, 479, 13);
    myGLCD.setColor(64, 64, 64);
    myGLCD.fillRect(0, 306, 479, 319);
    myGLCD.setColor(255, 255, 255);
    myGLCD.setBackColor(255, 0, 0);
    myGLCD.print("* Universal Color TFT Display Library *", CENTER, 1);
    myGLCD.setBackColor(64, 64, 64);
    myGLCD.setColor(255,255,0);

    myGLCD.print("<http://www.RinkyDinkElectronics.com/>", CENTER, 307);

    myGLCD.setColor(0, 0, 255);
    myGLCD.drawRect(0, 14, 479, 305);
    delay(10000);
}