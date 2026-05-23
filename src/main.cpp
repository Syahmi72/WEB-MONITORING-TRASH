#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h> 

// === KONFIGURASI WIFI ===
#define WIFI_SSID "Sami"
#define WIFI_PASSWORD "Kualisami29"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR ===
#define PIN_INDUKTIF 33   
#define PIN_KAPASITIF 19  
#define PIN_IR 22         

// === PIN SERVO ===
#define PIN_SERVO_LOGAM 13    
#define PIN_SERVO_PLASTIK 12  
#define PIN_SERVO_ORGANIK 14  

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

int kapasitifStable = 0;    
String statusTerakhir = ""; 

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n\n=================================");
  Serial.println("   MONITORING TEMPAT SAMPAH V6");
  Serial.println("         (FINAL CONFIG)");
  Serial.println("=================================");

  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLUP); 
  pinMode(PIN_IR, INPUT_PULLUP);
  
  pinMode(2, OUTPUT); 
  digitalWrite(2, LOW);

  // Inisialisasi Servo
  servoLogam.attach(PIN_SERVO_LOGAM);
  servoPlastik.attach(PIN_SERVO_PLASTIK);
  servoOrganik.attach(PIN_SERVO_ORGANIK);

  // Posisi awal diam rapat
  servoLogam.write(0);   // Servo 180 derajat ke titik 0
  servoPlastik.write(0);  // Servo 180 derajat ke titik 0
  servoOrganik.write(90); // Servo 360 derajat ngerem/diam

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

  // Logika pembacaan sensor
  bool adaLogam = (bacaSensorInduktif == LOW);
  bool adaOrganik = (bacaSensorIR == LOW);
  bool rawPlastik = (bacaSensorKapasitif == LOW); // Kapasitif tipe NC

  if (rawPlastik) {
    if (kapasitifStable < 3) kapasitifStable++;
  } else {
    if (kapasitifStable > 0) kapasitifStable--;
  }
  bool adaPlastik = (kapasitifStable >= 3);

  digitalWrite(2, (adaLogam || adaPlastik || adaOrganik) ? HIGH : LOW);

  bool adaSampahBaru = false; 

  // Hitung Logam
  if (adaLogam && !logamTerlihat) {
    totalLogam++; logamTerlihat = true; adaSampahBaru = true;
  } else if (!adaLogam) { logamTerlihat = false; }

  // Hitung Plastik
  if (adaPlastik && !plastikTerlihat) {
    totalPlastik++; plastikTerlihat = true; adaSampahBaru = true;
  } else if (!adaPlastik) { plastikTerlihat = false; }

  // Hitung Organik
  if (adaOrganik && !organikTerlihat) {
    totalOrganik++; organikTerlihat = true; adaSampahBaru = true;
  } else if (!adaOrganik) { organikTerlihat = false; }

  // Tentukan Status Teks
  String status;
  if (adaLogam) status = "Logam Terdeteksi!";
  else if (adaPlastik) status = "Plastik Terdeteksi!";
  else if (adaOrganik) status = "Organik Terdeteksi!";
  else status = "Menunggu Sampah...";

  // Kirim data ke Serial & Firebase
  if (status != statusTerakhir || adaSampahBaru) {
    Serial.print("Induktif: "); Serial.print(bacaSensorInduktif);
    Serial.print(" | Kapasitif: "); Serial.print(bacaSensorKapasitif);
    Serial.print(" | IR: "); Serial.print(bacaSensorIR);
    Serial.print(" | Status: "); Serial.print(status);
    if (adaSampahBaru) Serial.print(" -> [COUNTER UP]");
    Serial.println();

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
  // ⚙️ LOGIKA BUKA-TUTUP SERVO MASING-MASING BILIK
  // =======================================================
  
  if (adaLogam) {
    Serial.println("⚙️ Membuka Tutup LOGAM (Servo 180)...");
    servoLogam.write(90);  
    delay(3000);           
    servoLogam.write(0);   
    Serial.println("⚙️ Menutup Tutup LOGAM.");
  } 
  
  else if (adaPlastik) {
    Serial.println("⚙️ Membuka Tutup PLASTIK (Servo 360)...");
    servoPlastik.write(180); // Putar untuk membuka
    delay(800);              // Lamanya proses membuka (silakan naikkan/turunkan angka ini)
    servoPlastik.write(90);  // NGEREM / DIAM dalam posisi terbuka
    
    delay(3000);             // Tahan selama 3 detik agar sampah plastik masuk
    
    Serial.println("⚙️ Menutup Tutup PLASTIK (Servo 360)...");
    servoPlastik.write(0);   // Putar balik untuk menutup
    delay(800);              // Waktunya harus sama dengan durasi membuka di atas
    servoPlastik.write(90);  // NGEREM / DIAM dalam posisi tertutup rapat
  } 
  
  else if (adaOrganik) {
    Serial.println("⚙️ Membuka Tutup ORGANIK (Servo 360)...");
    servoOrganik.write(180); 
    delay(500);              
    servoOrganik.write(90);  
    
    delay(3000);             
    
    Serial.println("⚙️ Menutup Tutup ORGANIK (Servo 360)...");
    servoOrganik.write(0);   
    delay(500);              
    servoOrganik.write(90);  
  }

  delay(150); 
}