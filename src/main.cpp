#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// === KONFIGURASI WIFI ===
#define WIFI_SSID "ITK-LAB2.X"
#define WIFI_PASSWORD "K@mpusM3rdeka!"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR ===
#define PIN_INDUKTIF 4    // GPIO 4 untuk sensor induktif
#define PIN_KAPASITIF 19  // GPIO 19 untuk sensor kapasitif
#define PIN_IR 22

// === VARIABEL GLOBAL ===
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int totalLogam = 0;
bool logamTerlihat = false;

int totalPlastik = 0;
bool plastikTerlihat = false;

String statusTerakhir = ""; // Untuk mencegah spamming Firebase

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n\n=================================");
  Serial.println("   MONITORING TEMPAT SAMPAH");
  Serial.println("=================================");

  // KEDUA PIN WAJIB PULLUP!
  // Karena sensor NPN NC (Kapasitif) butuh tarikan ke atas saat memutus arus (lampu mati)
  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLUP); 
  pinMode(PIN_IR, INPUT);
  
  pinMode(2, OUTPUT); // LED built-in ESP32
  digitalWrite(2, LOW);

  Serial.println("✓ Pin sensor sudah diset (Mode PULLUP)");

  // Setup WiFi
  Serial.print("🔄 Menghubungkan WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" ✓ WiFi TERHUBUNG!");
    
    // Setup Firebase
    Serial.print("🔄 Setup Firebase...");
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    // Optimasi koneksi Firebase
    fbdo.setBSSLBufferSize(1024, 1024);
    Firebase.setReadTimeout(fbdo, 1000 * 60);
    Firebase.setwriteSizeLimit(fbdo, "tiny");
    
    Serial.println(" ✓ Firebase OK!");
  } else {
    Serial.println(" ✗ WiFi GAGAL! Mode offline...");
  }
  Serial.println("=================================\n");
}

void loop() {
  // Baca kedua sensor
  int bacaSensorInduktif = digitalRead(PIN_INDUKTIF);
  int bacaSensorKapasitif = digitalRead(PIN_KAPASITIF);
  int irStatus = digitalRead(PIN_IR);

  // LOGIKA FINAL (DISILANG SESUAI SIFAT FISIK SENSOR)
  // Induktif (NO): Deteksi = LOW, Standby = HIGH
  // Kapasitif (NC): Deteksi = HIGH (Lampu mati), Standby = LOW (Lampu nyala)
  bool adaLogam = (bacaSensorInduktif == LOW);
  bool adaPlastik = (bacaSensorKapasitif == HIGH);

  // Nyalakan LED biru di ESP32 jika salah satu mendeteksi
  digitalWrite(2, (adaLogam || adaPlastik) ? HIGH : LOW);

  // === MENGHITUNG LOGAM ===
  if (adaLogam && !logamTerlihat) {
    totalLogam++;
    logamTerlihat = true;
    Serial.print(">> LOGAM MASUK! Total: ");
    Serial.println(totalLogam);
    
    if (Firebase.ready()) {
      Firebase.setInt(fbdo, "/sensor_induktif/total", totalLogam);
    }
  } else if (!adaLogam) {
    logamTerlihat = false;
  }

  // === MENGHITUNG PLASTIK ===
  if (adaPlastik && !plastikTerlihat) {
    totalPlastik++;
    plastikTerlihat = true;
    Serial.print(">> PLASTIK MASUK! Total: ");
    Serial.println(totalPlastik);
    
    if (Firebase.ready()) {
      Firebase.setInt(fbdo, "/sensor_plastik/total", totalPlastik);
    }
  } else if (!adaPlastik) {
    plastikTerlihat = false;
  }

  if (irStatus == LOW) {
    Firebase.setString(fbdo, "/sistem/status_kapasitas", "Penuh");
  } else {
    Firebase.setString(fbdo, "/sistem/status_kapasitas", "Aman");
  }
  
  delay(500); // Jeda agar Firebase tidak ter-spam


  // === UPDATE STATUS WEB ===
  String statusLive = "Menunggu Sampah...";
  if (adaLogam) {
    statusLive = "Logam Terdeteksi!";
  } else if (adaPlastik) {
    statusLive = "Plastik Terdeteksi!";
  }

  // Hanya kirim ke Firebase jika tulisan statusnya berubah (Mencegah Spam)
  if (statusLive != statusTerakhir) {
    if (Firebase.ready()) {
      Firebase.setString(fbdo, "/sistem/status_aktif", statusLive);
    }
    statusTerakhir = statusLive; // Simpan status terbaru
  }

  delay(100); 
}