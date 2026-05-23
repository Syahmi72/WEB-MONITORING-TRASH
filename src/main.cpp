#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// === KONFIGURASI WIFI ===
#define WIFI_SSID "Sami"
#define WIFI_PASSWORD "Kualisami29"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR ===
#define PIN_INDUKTIF 4    // D4: Sensor Logam
#define PIN_KAPASITIF 19  // D19: Sensor Plastik
#define PIN_IR 22         // D22: Sensor Organik (Kembali Ditambahkan)

// === VARIABEL GLOBAL ===
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int totalLogam = 0;     bool logamTerlihat = false;
int totalPlastik = 0;    bool plastikTerlihat = false;
int totalOrganik = 0;    bool organikTerlihat = false;

int kapasitifStable = 0;    // Penstabil sensor plastik
String statusTerakhir = ""; // Gembok data agar tidak spam ke Firebase

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n\n=================================");
  Serial.println("   MONITORING TEMPAT SAMPAH V2");
  Serial.println("=================================");

  // Setup pin dengan INPUT_PULLUP agar sinyal terkunci dan tidak mengambang
  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLUP); // Diubah ke PULLUP karena tipenya Normally Closed (NC)
  pinMode(PIN_IR, INPUT_PULLUP);
  
  pinMode(2, OUTPUT); 
  digitalWrite(2, LOW);

  Serial.println("✓ Semua pin sensor (Logam, Plastik, Organik) siap.");

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
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    // Setup Firebase
    Serial.print("🔄 Menghubungkan ke Firebase...");
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Serial.println(" ✓ Firebase Terkoneksi!");

  } else {
    Serial.println(" ✗ WiFi GAGAL! Berjalan dalam mode offline.");
  }
  Serial.println("=================================\n");
}

void loop() {
  // 1. Baca nilai digital dari masing-masing pin
  int bacaSensorInduktif = digitalRead(PIN_INDUKTIF);
  int bacaSensorKapasitif = digitalRead(PIN_KAPASITIF);
  int bacaSensorIR = digitalRead(PIN_IR);

  // 2. Terjemahkan Logika Deteksi
  bool adaLogam = (bacaSensorInduktif == LOW);
  bool adaOrganik = (bacaSensorIR == LOW);
  
  // PERBAIKAN UTAMA: Karena fisik sensor kapasitif terbalik (lampu mati saat didekatkan benda),
  // maka kondisi "ada plastik" justru dibaca saat bernilai LOW.
  bool rawPlastik = (bacaSensorKapasitif == LOW); 

  // 3. Sistem Debounce Penstabil Sensor Kapasitif
  if (rawPlastik) {
    if (kapasitifStable < 3) kapasitifStable++;
  } else {
    if (kapasitifStable > 0) kapasitifStable--;
  }
  bool adaPlastik = (kapasitifStable >= 3);

  // Nyalakan LED internal jika salah satu sensor mendeteksi objek
  digitalWrite(2, (adaLogam || adaPlastik || adaOrganik) ? HIGH : LOW);

  bool adaSampahBaru = false; 

  // 4. Perhitungan Total Sampah Logam
  if (adaLogam && !logamTerlihat) {
    totalLogam++;
    logamTerlihat = true;
    adaSampahBaru = true;
  } else if (!adaLogam) {
    logamTerlihat = false;
  }

  // 5. Perhitungan Total Sampah Plastik
  if (adaPlastik && !plastikTerlihat) {
    totalPlastik++;
    plastikTerlihat = true;
    adaSampahBaru = true;
  } else if (!adaPlastik) {
    plastikTerlihat = false;
  }

  // 6. Perhitungan Total Sampah Organik
  if (adaOrganik && !organikTerlihat) {
    totalOrganik++;
    organikTerlihat = true;
    adaSampahBaru = true;
  } else if (!adaOrganik) {
    organikTerlihat = false;
  }

  // 7. Penentuan Teks Status untuk Web Dashboard
  String status;
  if (adaLogam) status = "Logam Terdeteksi!";
  else if (adaPlastik) status = "Plastik Terdeteksi!";
  else if (adaOrganik) status = "Organik Terdeteksi!";
  else status = "Menunggu Sampah...";

  // 8. SISTEM FILTER ANTI-SPAM (Penyembuh Bug Hang/Delay)
  // Data hanya diproses dan dikirim jika status berubah atau hitungan bertambah
  if (status != statusTerakhir || adaSampahBaru) {
    
    // Tampilkan di Serial Monitor laptop
    Serial.print("Induktif: "); Serial.print(bacaSensorInduktif);
    Serial.print(" | Kapasitif: "); Serial.print(bacaSensorKapasitif);
    Serial.print(" | IR: "); Serial.print(bacaSensorIR);
    Serial.print(" | Status Terbaru: "); Serial.print(status);

    if (adaSampahBaru) {
      Serial.print(" -> [UPDATE COUNTER]");
    }
    Serial.println();

    // Kirim Data Real-time ke Firebase (Akan langsung merespon ke Web)
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      
      // Update status jika teks berubah
      if (status != statusTerakhir) {
        Firebase.setString(fbdo, "/sistem/status_aktif", status);
        statusTerakhir = status; // Kunci status sekarang
      }
      
      // Update nilai counter jika ada sampah masuk
      if (adaSampahBaru) {
        Firebase.setInt(fbdo, "/sensor_induktif/total", totalLogam);
        Firebase.setInt(fbdo, "/sensor_plastik/total", totalPlastik);
        Firebase.setInt(fbdo, "/sensor_organik/total", totalOrganik);
      }
    }
  }

  delay(150); // Jeda aman loop agar ESP32 tetap responsif
}