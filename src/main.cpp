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
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // --- TAMBAHKAN BARIS INI UNTUK MENCEGAH TIMEOUT ---
  fbdo.setBSSLBufferSize(1024, 1024); // Memperbesar ukuran memori untuk keamanan SSL
  Firebase.setReadTimeout(fbdo, 1000 * 60); // Memberi waktu tunggu lebih lama (60 detik)
  Firebase.setwriteSizeLimit(fbdo, "tiny"); // Mengecilkan paket data agar cepat terkirim
  Firebase.reconnectWiFi(true);
}

void loop() {
  int bacaSensor = digitalRead(PIN_INDUKTIF);
  Serial.print("RAW INDUKTIF = ");
  Serial.println(bacaSensor);

  // Karena NPN, LOW = Logam Terdeteksi
  // Teks harus disesuaikan agar web bisa mengenalinya
  String statusLive = (bacaSensor == LOW) ? "Logam Terdeteksi!" : "Menunggu Sampah...";

  // Kirim ke Firebase di folder /sistem/status_aktif (Kamar yang benar!)
  if (Firebase.ready()) {
    if (Firebase.setString(fbdo, "/sistem/status_aktif", statusLive)) {
      Serial.print("Update Firebase: ");
      Serial.println(statusLive);
    } else {
      Serial.print("Gagal kirim: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(300); // Kirim data setiap 1 detik
}