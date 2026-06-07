#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h> 

// === KONFIGURASI WIFI ===
#define WIFI_SSID "ITK-LAB.X"
#define WIFI_PASSWORD "K@mpusM3rdeka!"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR SAMPAH ===
#define PIN_INDUKTIF 19   // Sensor Logam
#define PIN_KAPASITIF 15  // Sensor Plastik
#define PIN_IR 22         // Sensor Organik

// === PIN SENSOR ULTRASONIK ===
#define PIN_TRIG 5
#define PIN_ECHO 18

// 📏 KALIBRASI ULTRASONIK (UBAH ANGKA INI SESUAI LEBAR FISIK TONGMU)
// Jarak tembakan mentok ke dinding kanan saat 3 tong masih kosong
#define JARAK_KOSONG 60  
// Jarak tembakan saat tumpukan sampah menghalangi sensor
#define JARAK_PENUH 15   

// === PIN SERVO ===
#define PIN_SERVO_LOGAM 25    
#define PIN_SERVO_PLASTIK 26  
#define PIN_SERVO_ORGANIK 14  

// === SETTING TRIGGER SENSOR ===
#define TRIGGER_INDUKTIF HIGH    
#define TRIGGER_KAPASITIF HIGH  
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
unsigned long waktuUltrasonikTerakhir = 0;

int kapasitasPersen = 0;
bool tongPenuh = false;

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n\n=================================");
  Serial.println("   MONITORING TEMPAT SAMPAH V9");
  Serial.println(" (FULL SYSTEM + RADAR ULTRASONIK)");
  Serial.println("=================================");

  // Inisialisasi pin
  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLUP); 
  pinMode(PIN_IR, INPUT_PULLUP);
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

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
  servoLogam.write(0);    // Tipe 180
  servoPlastik.write(90); // TIPE 360 -> DIAM
  servoOrganik.write(90); // TIPE 360 -> DIAM (Sudah diupdate ke 360)

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
}

void loop() {
  // =======================================================
  // 1. PEMBACAAN RADAR ULTRASONIK (Setiap 1 Detik agar stabil)
  // =======================================================
  if (millis() - waktuUltrasonikTerakhir > 1000) {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, HIGH);
    
    long duration = pulseIn(PIN_ECHO, HIGH, 30000); // Batas timeout bacaan
    int jarak = duration * 0.034 / 2;

    if (jarak == 0 || jarak > JARAK_KOSONG) jarak = JARAK_KOSONG; 

    // Konversi jarak menyamping menjadi Persentase Penuh
    if (jarak <= JARAK_PENUH) {
      kapasitasPersen = 100;
      tongPenuh = true;
    } else {
      kapasitasPersen = map(jarak, JARAK_KOSONG, JARAK_PENUH, 0, 100);
      kapasitasPersen = constrain(kapasitasPersen, 0, 100);
      tongPenuh = false;
    }

    waktuUltrasonikTerakhir = millis();
  }

  // =======================================================
  // 2. PEMBACAAN SENSOR PINTU MASUK
  // =======================================================
  int bacaSensorInduktif = digitalRead(PIN_INDUKTIF);
  int bacaSensorKapasitif = digitalRead(PIN_KAPASITIF);
  int bacaSensorIR = digitalRead(PIN_IR);

  // RADAR DEBUG 
  if (millis() - waktuDebugTerakhir > 1000) {
    Serial.printf("🔍 RADAR -> Induktif:%d | Kapasitif:%d | IR:%d | Ultra:%d%% (Penuh:%d)\n", 
    bacaSensorInduktif, bacaSensorKapasitif, bacaSensorIR, kapasitasPersen, tongPenuh);
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

  // =======================================================
  // 3. UPDATE DATA KE FIREBASE
  // =======================================================
  String status;
  if (tongPenuh) status = "Tempat Sampah Penuh!";
  else if (adaLogam) status = "Logam Terdeteksi!";
  else if (adaPlastik) status = "Plastik Terdeteksi!";
  else if (adaOrganik) status = "Organik Terdeteksi!";
  else status = "Menunggu Sampah...";

  if (status != statusTerakhir || adaSampahBaru) {
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      if (status != statusTerakhir) {
        Serial.print("🚀 [UPDATE] Status: "); Serial.println(status);
        Firebase.setString(fbdo, "/sistem/status_aktif", status);
        Firebase.setInt(fbdo, "/sistem/kapasitas", kapasitasPersen); // Kirim persentase ke PWA
        statusTerakhir = status; 
      }
      if (adaSampahBaru) {
        Firebase.setInt(fbdo, "/sensor_induktif/total", totalLogam);
        Firebase.setInt(fbdo, "/sensor_plastik/total", totalPlastik);
        Firebase.setInt(fbdo, "/sensor_organik/total", totalOrganik);
      }
    }
  }

  // =======================================================
  // 4. LOGIKA PENGGERAK MOTOR SERVO (MANDIRI & ANTI-MACET)
  // =======================================================
  unsigned long timeoutTunggu; 

  // JIKA TONG PENUH, SEMUA SERVO DIKUNCI (Tidak bisa buka)
  if (!tongPenuh) {

    // --- JALUR SERVO SAMPAH LOGAM (Tipe 180) ---
    if (adaLogam) {
      Serial.println("⚙️ Membuka Tutup LOGAM...");
      servoLogam.write(180);  
      delay(3000);           
      servoLogam.write(0);    
      Serial.println("⚙️ Menutup Tutup LOGAM.");

      if (WiFi.status() == WL_CONNECTED && Firebase.ready()) Firebase.setString(fbdo, "/sistem/status_aktif", "Sampah Logam Masuk");
      delay(3000); 

      timeoutTunggu = millis();
      while (digitalRead(PIN_INDUKTIF) == TRIGGER_INDUKTIF && millis() - timeoutTunggu < 5000) { delay(100); }
      statusTerakhir = "Sampah Logam Masuk"; 
    } 
    
    // --- JALUR SERVO SAMPAH PLASTIK (Tipe 360) ---
    if (adaPlastik) {
      Serial.println("⚙️ Membuka Tutup PLASTIK...");
      servoPlastik.write(180); 
      delay(6000); 
      
      servoPlastik.write(90); 
      delay(3000);              
      
      Serial.println("⚙️ Menutup Tutup PLASTIK.");
      servoPlastik.write(0);  
      delay(5500); 
      
      servoPlastik.write(90); 

      if (WiFi.status() == WL_CONNECTED && Firebase.ready()) Firebase.setString(fbdo, "/sistem/status_aktif", "Sampah Plastik Masuk");
      delay(3000);

      timeoutTunggu = millis();
      while (digitalRead(PIN_KAPASITIF) == TRIGGER_KAPASITIF && millis() - timeoutTunggu < 5000) { delay(100); }
      statusTerakhir = "Sampah Plastik Masuk";
    } 
    
    // --- JALUR SERVO SAMPAH ORGANIK (Tipe 360) ---
    if (adaOrganik) {
      Serial.println("⚙️ Membuka Tutup ORGANIK...");
      servoOrganik.write(180); 
      delay(6000);             
      
      servoOrganik.write(90);  
      delay(3000);             
      
      Serial.println("⚙️ Menutup Tutup ORGANIK.");
      servoOrganik.write(0);   
      delay(5500);             
      
      servoOrganik.write(90);  

      if (WiFi.status() == WL_CONNECTED && Firebase.ready()) Firebase.setString(fbdo, "/sistem/status_aktif", "Sampah Organik Masuk");
      delay(3000);

      timeoutTunggu = millis();
      while (digitalRead(PIN_IR) == TRIGGER_IR && millis() - timeoutTunggu < 5000) { delay(100); }
      statusTerakhir = "Sampah Organik Masuk";
    }

  } // Akhir dari Blok Kunci Tong Penuh

  delay(50);
}