#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h> 

// === KONFIGURASI WIFI ===
#define WIFI_SSID "ITK-LAB2.X"
#define WIFI_PASSWORD "K@mpusM3rdeka!"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR ===
#define PIN_INDUKTIF 19   // Sensor Logam
#define PIN_KAPASITIF 15  // Sensor Plastik
#define PIN_IR 22         // Sensor Organik

// === PIN SERVO ===
#define PIN_SERVO_LOGAM 25    // Pindah dari 13 ke 25
#define PIN_SERVO_PLASTIK 26  // Pindah dari 12 ke 26
#define PIN_SERVO_ORGANIK 14  // Tetap di 14 (Aman)

// =======================================================
// ⚙️ SETTING TRIGGER SENSOR (LOGIKA DIBALIK)
// =======================================================
#define TRIGGER_INDUKTIF LOW    
#define TRIGGER_KAPASITIF HIGH  // UBAH JADI HIGH KARENA SENSOR TIPE NPN-NC (NORMALLY CLOSED)
#define TRIGGER_IR LOW          

// === VARIABEL GLOBAL ===
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

Servo servoLogam;
Servo servoPlastik;
Servo servoOrganik;

int totalLogam = 0;     bool logamTerlihat = false;
int totalPlastik = 0;   bool plastikTerlihat = false;
int totalOrganik = 0;   bool organikTerlihat = false;

String statusTerakhir = ""; 
unsigned long waktuDebugTerakhir = 0; 

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n\n=================================");
  Serial.println("   MONITORING TEMPAT SAMPAH V8");
  Serial.println("    (LOGIKA NPN-NC FIXED)");
  Serial.println("=================================");

  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLUP); 
  pinMode(PIN_IR, INPUT_PULLUP);
  
  pinMode(2, OUTPUT); 
  digitalWrite(2, LOW);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoLogam.attach(PIN_SERVO_LOGAM);
  servoPlastik.attach(PIN_SERVO_PLASTIK);
  servoOrganik.attach(PIN_SERVO_ORGANIK);

  // Posisi Awal 
  servoLogam.write(0);    // Tipe 180 -> Tutup rapat di 0
  servoPlastik.write(90); // TIPE 360 -> WAJIB 90 AGAR NGEREM DIAM
  servoOrganik.write(0);  // Tipe 180 -> Tutup rapat di 0

  Serial.println("✓ Semua pin Sensor & 3 Servo siap.");

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
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    Serial.print("🔄 Menghubungkan ke Firebase...");
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    fbdo.setBSSLBufferSize(1024, 1024);
    fbdo.setResponseSize(1024);

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Serial.println(" ✓ Firebase Terkoneksi!");
  } else {
    Serial.println(" ✗ WiFi GAGAL! Berjalan offline.");
  }
  
  delay(1500); 
  Serial.println("=================================\n");
}

void loop() {
  int bacaSensorInduktif = digitalRead(PIN_INDUKTIF);
  int bacaSensorKapasitif = digitalRead(PIN_KAPASITIF);
  int bacaSensorIR = digitalRead(PIN_IR);

  // RADAR DEBUG (Muncul di Serial Monitor setiap 1 detik)
  if (millis() - waktuDebugTerakhir > 1000) {
    Serial.printf("🔍 RADAR -> Induktif: %d | Kapasitif: %d | IR: %d\n", bacaSensorInduktif, bacaSensorKapasitif, bacaSensorIR);
    waktuDebugTerakhir = millis();
  }

  bool adaLogam = (bacaSensorInduktif == TRIGGER_INDUKTIF);
  bool adaPlastik = (bacaSensorKapasitif == TRIGGER_KAPASITIF);
  bool adaOrganik = (bacaSensorIR == TRIGGER_IR);

  digitalWrite(2, (adaLogam || adaPlastik || adaOrganik) ? HIGH : LOW);
  bool adaSampahBaru = false; 

  if (adaLogam && !logamTerlihat) { totalLogam++; logamTerlihat = true; adaSampahBaru = true; } 
  else if (!adaLogam) { logamTerlihat = false; }

  if (adaPlastik && !plastikTerlihat) { totalPlastik++; plastikTerlihat = true; adaSampahBaru = true; } 
  else if (!adaPlastik) { plastikTerlihat = false; }

  if (adaOrganik && !organikTerlihat) { totalOrganik++; organikTerlihat = true; adaSampahBaru = true; } 
  else if (!adaOrganik) { organikTerlihat = false; }

  String status;
  if (adaLogam) status = "Logam Terdeteksi!";
  else if (adaPlastik) status = "Plastik Terdeteksi!";
  else if (adaOrganik) status = "Organik Terdeteksi!";
  else status = "Menunggu Sampah...";

  if (status != statusTerakhir || adaSampahBaru) {
    Serial.println("🚀 [UPDATE] Status: " + status);
    
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      if (status != statusTerakhir) {
        Firebase.setStringAsync(fbdo, "/sistem/status_aktif", status);
        statusTerakhir = status; 
      }
      if (adaSampahBaru) {
        Firebase.setIntAsync(fbdo, "/sensor_induktif/total", totalLogam);
        Firebase.setIntAsync(fbdo, "/sensor_plastik/total", totalPlastik);
        Firebase.setIntAsync(fbdo, "/sensor_organik/total", totalOrganik);
      }
    }
  }

  // =======================================================
  // ⚙️ LOGIKA BUKA-TUTUP SERVO BERDASARKAN TIPE HARDWARE
  // =======================================================
  
  if (adaLogam) {
    Serial.println("⚙️ Membuka Tutup LOGAM (Tipe 180)...");
    servoLogam.write(180);  // Buka maksimal 
    delay(3000);           
    servoLogam.write(0);    // Tutup rapat
    Serial.println("⚙️ Menutup Tutup LOGAM.");

    while (digitalRead(PIN_INDUKTIF) == TRIGGER_INDUKTIF) { delay(100); }
  } 
  
  else if (adaPlastik) {
    Serial.println("⚙️ Membuka Tutup PLASTIK (Tipe 360)...");
    servoPlastik.write(180); // Muter ke kanan
    delay(800);              
    servoPlastik.write(90);  // NGEREM
    
    delay(3000);             // Tahan 3 detik
    
    Serial.println("⚙️ Menutup Tutup PLASTIK.");
    servoPlastik.write(0);   // Muter balik
    delay(800);              
    servoPlastik.write(90);  // NGEREM
    
    while (digitalRead(PIN_KAPASITIF) == TRIGGER_KAPASITIF) { delay(100); }
  } 
  
  else if (adaOrganik) {
    Serial.println("⚙️ Membuka Tutup ORGANIK (Tipe 180)...");
    servoOrganik.write(180); // Buka maksimal
    delay(3000);             
    servoOrganik.write(0);   // Tutup rapat
    Serial.println("⚙️ Menutup Tutup ORGANIK.");

    while (digitalRead(PIN_IR) == TRIGGER_IR) { delay(100); }
  }

  delay(50);
}