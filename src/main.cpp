#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// === KONFIGURASI WIFI ===
#define WIFI_SSID "Teman Kenangan"
#define WIFI_PASSWORD "PaduanPas!05"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR ===
#define PIN_INDUKTIF 4    
#define PIN_KAPASITIF 19  
#define PIN_IR 22         

// === VARIABEL GLOBAL ===
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int totalLogam = 0;   bool logamTerlihat = false;
int totalPlastik = 0; bool plastikTerlihat = false;
int totalOrganik = 0; bool organikTerlihat = false;
String statusTerakhir = ""; 

// ================================================
// 📍 KOORDINAT LOKASI ALAT (HARDCODE PRESISI)
// Silakan buka Google Maps, klik kanan titik lokasi alat,
// lalu copy angka koordinatnya ke sini.
// ================================================
float lokasi_latitude = -1.149112;   // Contoh: Titik di Kampus ITK Balikpapan
float lokasi_longitude = 116.861234; 

void setup() {
  Serial.begin(115200);
  
  // Konfigurasi Pin sesuai hardware Sami
  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLDOWN); // Mengembalikan ke PULLDOWN sesuai koreksi hardware
  pinMode(PIN_IR, INPUT_PULLUP);
  pinMode(2, OUTPUT); 

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔄 Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" ✓ Terhubung!");

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // === MENGIRIM KOORDINAT LOKASI PASTI KE DASHBOARD ===
  if (Firebase.ready()) {
    Firebase.setFloat(fbdo, "/sistem/lokasi/lat", lokasi_latitude);
    Firebase.setFloat(fbdo, "/sistem/lokasi/lng", lokasi_longitude);
    Serial.println(" ✓ Koordinat GPS Hardcode terkirim ke Dashboard!");
  }
}

void loop() {
  // Membaca semua sensor
  bool adaLogam = (digitalRead(PIN_INDUKTIF) == LOW);
  bool adaPlastik = (digitalRead(PIN_KAPASITIF) == LOW); // Logika: Lampu Mati = Plastik Terdeteksi
  bool adaOrganik = (digitalRead(PIN_IR) == LOW);

  digitalWrite(2, (adaLogam || adaPlastik || adaOrganik) ? HIGH : LOW);

  // Penghitungan Logam
  if (adaLogam && !logamTerlihat) {
    totalLogam++; logamTerlihat = true;
    Firebase.setInt(fbdo, "/sensor_induktif/total", totalLogam);
  } else if (!adaLogam) { logamTerlihat = false; }

  // Penghitungan Plastik
  if (adaPlastik && !plastikTerlihat) {
    totalPlastik++; plastikTerlihat = true;
    Firebase.setInt(fbdo, "/sensor_plastik/total", totalPlastik);
  } else if (!adaPlastik) { plastikTerlihat = false; }

  // Penghitungan Organik (Daun)
  if (adaOrganik && !organikTerlihat) {
    totalOrganik++; organikTerlihat = true;
    Firebase.setInt(fbdo, "/sensor_organik/total", totalOrganik);
  } else if (!adaOrganik) { organikTerlihat = false; }

  // Update Status Teks Dashboard
  String st = "Menunggu Sampah...";
  if (adaLogam) st = "Memilah Logam";
  else if (adaPlastik) st = "Memilah Plastik";
  else if (adaOrganik) st = "Memilah Organik";

  if (st != statusTerakhir) {
    Firebase.setString(fbdo, "/sistem/status_aktif", st);
    statusTerakhir = st;
    Serial.print("Status Baru: ");
    Serial.println(st);
  }
  delay(50); // Jeda responsif
}