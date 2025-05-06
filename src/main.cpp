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

// Objetos y configuración
BH1750 lightMeter;
MCUFRIEND_kbv tft;
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Configuración WiFi y MQTT
const char* ssid = "Crisco";
const char* password = "150504yo";
const char* serverUrl = "http://presart.ddns.net/sensor";
const char* mqtt_server = "98.82.10.79";

// Variables de alerta
bool alertaActiva = false;
unsigned long alertaInicio = 0;
const unsigned long duracionAlerta = 10000; // 10 segundos
bool pantallaAlertaMostrada = false;
String mensajeAlerta = "";

// Variables de envío de datos
unsigned long previousMillis = 0;
const long interval = 10000; // Cada 10 segundos

// Funciones
void dibujarIconoAlerta() {
    int baseWidth = 120;
    int height = 100;
    int centerX = tft.width() / 2;
    int centerY = tft.height() / 2 - 25;

    int x0 = centerX - baseWidth / 2;
    int y0 = centerY + height / 2;
    int x1 = centerX + baseWidth / 2;
    int y1 = centerY + height / 2;
    int x2 = centerX;
    int y2 = centerY - height / 2;

    // Triángulo negro (borde)
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_BLACK);

    // Triángulo amarillo (interior)
    int borde = 5;
    tft.fillTriangle(
        x0 + borde, y0 - borde,
        x1 - borde, y1 - borde,
        x2, y2 + borde,
        TFT_YELLOW
    );

    // Signo de exclamación
    tft.fillRect(centerX - 5, centerY - 20, 10, 40, TFT_BLACK);
    tft.fillCircle(centerX, centerY + 30, 6, TFT_BLACK);
}

void mostrarAlerta() {
    tft.fillScreen(TFT_RED);
    dibujarIconoAlerta();

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    String titulo = "ALERTA!";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(titulo, 0, 0, &x1, &y1, &w, &h);
    int textoX = (tft.width() - w) / 2;
    tft.setCursor(textoX, 10);
    tft.println(titulo);

    tft.setTextSize(2);
    tft.setCursor(10, tft.height() - 60);
    tft.println("Precaucion:");
    tft.setCursor(10, tft.height() - 30);
    tft.println(mensajeAlerta);
}

void mostrarDatosSensores() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 20);

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float lux = lightMeter.readLightLevel();

    if (isnan(h) || isnan(t)) {
        Serial.println(F("Error leyendo el sensor DHT"));
        return;
    }

    // Calibrar y sobrescribir t con el valor calibrado
    t = 0.0349 * t * t - 0.851 * t + 26.272;

    // Corregir la lectura de lux
    lux = -0.000179 * lux * lux + 1.481561 * lux - 83.217964;

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

    Serial.printf("Humedad: %.2f%%  TempCalib: %.2f°C  LuzCalib: %.2f lux\n", h, t, lux);

    // Enviar datos al servidor
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{\"temperatura\":" + String(t, 2) + ",\"humedad\":" + String(h, 2) + ",\"lux\":" + String(lux, 2) + "}";
    int httpResponseCode = http.POST(jsonData);
    http.setTimeout(5000);

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 200) {
        Serial.println("Datos enviados correctamente.");
        Serial.println("Respuesta servidor: " + http.getString());
    } else {
        Serial.println("Error enviando datos.");
    }

    http.end();
}

void callback(char* topic, byte* payload, unsigned int length) {
    mensajeAlerta = "";
    for (int i = 0; i < length; i++) {
        mensajeAlerta += (char)payload[i];
    }
    Serial.println("Mensaje MQTT recibido: " + mensajeAlerta);

    alertaActiva = true;
    alertaInicio = millis();
    pantallaAlertaMostrada = false; // forzar que se dibuje la pantalla roja
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Intentando conexión MQTT...");
        if (client.connect("ESP32Client")) {
            Serial.println("Conectado");
            client.subscribe("arte/alertas");
        } else {
            Serial.print("Fallo, rc=");
            Serial.print(client.state());
            Serial.println(" intentando de nuevo en 5 segundos");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
        delay(1000);
        Serial.print(".");
        intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConectado a WiFi");
    } else {
        Serial.println("\nNo se pudo conectar a WiFi, reiniciando...");
        ESP.restart();
    }

    tft.reset();
    uint16_t ID = tft.readID();
    tft.begin(ID);
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(-1);

    dht.begin();
    Wire.begin(19, 18);
    lightMeter.begin();

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);

    //Serial.println("Sensor BH1750 iniciado");
    delay(2000);
}

void loop() {
    client.loop();

    unsigned long tiempoActual = millis();

    // Revisar conexión WiFi periódicamente
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi desconectado. Intentando reconectar...");
        WiFi.disconnect();
        WiFi.reconnect();
        delay(1000);
    }

    if (alertaActiva) {
        if (!pantallaAlertaMostrada) {
            mostrarAlerta();
            pantallaAlertaMostrada = true;
        }

        if (tiempoActual - alertaInicio > duracionAlerta) {
            alertaActiva = false;
            pantallaAlertaMostrada = false; // Para que en la siguiente alerta se redibuje
            previousMillis = tiempoActual;  // Reiniciar temporizador para que no se acumulen lecturas
            mostrarDatosSensores();         // Volver a mostrar los datos inmediatamente
        }
    } else {
        if (tiempoActual - previousMillis >= interval) {
            previousMillis = tiempoActual;
            mostrarDatosSensores();
        }
    }

    if (!client.connected()) {
        reconnect();
    }
}