#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// --- KONFIGURASI WIFI ---
#define WIFI_SSID "ITK-LAB2.X"
#define WIFI_PASSWORD "K@mpusM3rdeka!"

// --- KONFIGURASI FIREBASE ---
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

#define PIN_INDUKTIF 21

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_INDUKTIF, INPUT_PULLUP);

  // Hubungkan WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");

  // Inisialisasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  int bacaSensor = digitalRead(PIN_INDUKTIF);
  
  // Karena NPN, LOW = Logam Terdeteksi
  String statusLogam = (bacaSensor == LOW) ? "Logam Terdeteksi" : "Tidak Ada Logam";

  // Kirim ke Firebase di folder /sensor_induktif
  if (Firebase.ready()) {
    if (Firebase.setString(fbdo, "/sensor_induktif/status", statusLogam)) {
      Serial.print("Update Firebase: ");
      Serial.println(statusLogam);
    } else {
      Serial.print("Gagal kirim: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(1000); // Kirim data setiap 1 detik
}