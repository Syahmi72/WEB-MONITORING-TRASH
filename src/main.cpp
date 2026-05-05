#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>

// === KONFIGURASI WIFI ===
#define WIFI_SSID "ITK-LAB2.X"
#define WIFI_PASSWORD "K@mpusM3rdeka!"

// === KONFIGURASI FIREBASE ===
#define FIREBASE_HOST "monitoring-tempat-sampah-c7e1a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "kqGzNtFg7WN8ZAx5N5uqLaga6WBaSklVmjDa5Dfe"

// === PIN SENSOR ===
#define PIN_INDUKTIF 4    
#define PIN_KAPASITIF 19  
#define PIN_IR 22         

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int totalLogam = 0;   bool logamTerlihat = false;
int totalPlastik = 0; bool plastikTerlihat = false;
int totalOrganik = 0; bool organikTerlihat = false;
String statusTerakhir = ""; 

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_INDUKTIF, INPUT_PULLUP);
  pinMode(PIN_KAPASITIF, INPUT_PULLDOWN); 
  pinMode(PIN_IR, INPUT_PULLUP);
  pinMode(2, OUTPUT); 

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Ambil Lokasi Real-time via IP
  HTTPClient http;
  http.begin("http://ip-api.com/csv/?fields=lat,lon");
  if(http.GET() > 0) {
    String res = http.getString();
    int koma = res.indexOf(',');
    if(koma > 0) {
      float lat = res.substring(0, koma).toFloat();
      float lng = res.substring(koma + 1).toFloat();
      Firebase.setFloat(fbdo, "/sistem/lokasi/lat", lat);
      Firebase.setFloat(fbdo, "/sistem/lokasi/lng", lng);
    }
  }
  http.end();
}

void loop() {
  bool adaLogam = (digitalRead(PIN_INDUKTIF) == LOW);
  bool adaPlastik = (digitalRead(PIN_KAPASITIF) == LOW); 
  bool adaOrganik = (digitalRead(PIN_IR) == LOW);

  digitalWrite(2, (adaLogam || adaPlastik || adaOrganik) ? HIGH : LOW);

  if (adaLogam && !logamTerlihat) {
    totalLogam++; logamTerlihat = true;
    Firebase.setInt(fbdo, "/sensor_induktif/total", totalLogam);
  } else if (!adaLogam) { logamTerlihat = false; }

  if (adaPlastik && !plastikTerlihat) {
    totalPlastik++; plastikTerlihat = true;
    Firebase.setInt(fbdo, "/sensor_plastik/total", totalPlastik);
  } else if (!adaPlastik) { plastikTerlihat = false; }

  if (adaOrganik && !organikTerlihat) {
    totalOrganik++; organikTerlihat = true;
    Firebase.setInt(fbdo, "/sensor_organik/total", totalOrganik);
  } else if (!adaOrganik) { organikTerlihat = false; }

  String st = "Menunggu Sampah...";
  if (adaLogam) st = "Memilah Logam";
  else if (adaPlastik) st = "Memilah Plastik";
  else if (adaOrganik) st = "Memilah Organik";

  if (st != statusTerakhir) {
    Firebase.setString(fbdo, "/sistem/status_aktif", st);
    statusTerakhir = st;
  }
  delay(50);
}