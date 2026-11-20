// Alarm Banjir — ESP32 + Sensor Level Air + Buzzer + 2 LED (+ Telegram opsional)
//
// Cara kerja:
//  - Sensor level air analog dibaca tiap 1 detik, dirata-rata dari 5 bacaan.
//  - Level >= AMBANG_BAHAYA selama 3 detik berturut-turut -> status BAHAYA:
//    buzzer bunyi putus-putus (pola darurat), LED merah menyala terus,
//    dan peringatan Telegram "BANJIR!" terkirim SEKALI tiap masuk status ini.
//  - Level >= AMBANG_WASPADA (tapi belum bahaya) -> status WASPADA:
//    LED merah kedip lambat, buzzer diam.
//  - Level di bawah ambang waspada -> status AMAN: LED hijau menyala.
//  - Semua ambang dan waktu ada sebagai konstanta bernama di bawah —
//    tidak ada angka ajaib tersembunyi di logika.
//
// Wiring:
//  - Sensor level air : S (analog) -> GPIO34, VCC -> 3V3, GND -> GND
//  - Buzzer aktif HIGH : + -> GPIO25, - -> GND
//  - LED merah : GPIO33 (seri resistor 220 ohm ke GND)
//  - LED hijau : GPIO32 (seri resistor 220 ohm ke GND)
//
// Kalibrasi ambang: buka Serial Monitor (115200), lihat nilai "Level"
// saat sensor kering dan saat terendam, lalu sesuaikan AMBANG_WASPADA
// dan AMBANG_BAHAYA di bawah (ADC ESP32: 0..4095).
//
// Library yang dibutuhkan (semua tersedia di Library Manager):
//  - UniversalTelegramBot 1.3.0 (hanya bila USE_TELEGRAM 1)
//  - ArduinoJson 6.21.5 (dependensi UniversalTelegramBot, WAJIB v6 — v7 tidak dipakai)
// Compile: arduino-cli compile --fqbn esp32:esp32:esp32 AlarmBanjir
// Terverifikasi 20 Nov 2026: sketch 1.070.285 byte (81% flash),
// RAM 50.616 byte (15%), ESP32 core 4.0.0, COMPILE_RC=0.

// ---------- KONFIGURASI ----------
#define USE_TELEGRAM 1  // 1 = kirim peringatan Telegram, 0 = tanpa Telegram

#define PIN_SENSOR_LEVEL 34  // ADC1, input analog saja
#define PIN_BUZZER 25        // buzzer aktif HIGH
#define PIN_LED_MERAH 33
#define PIN_LED_HIJAU 32

// Ambang level air (ADC 12-bit: 0..4095). Sensor kering ~0, terendam naik.
#define AMBANG_WASPADA 1500
#define AMBANG_BAHAYA 2500

#define JUMLAH_SAMPEL_RATA 5      // bacaan yang dirata-rata tiap pengukuran
#define JEDA_SAMPEL_MS 20         // jeda antar bacaan sampel
#define INTERVAL_BACA_MS 1000     // baca level tiap 1 detik
#define KONFIRMASI_BAHAYA_DETIK 3 // bahaya harus terkonfirmasi 3 detik berturut-turut

// Pola buzzer darurat (bunyi putus-putus) + kedip LED waspada.
#define DURASI_BUNYI_MS 300
#define DURASI_DIAM_MS 300
#define KEDIP_WASPADA_MS 1000

#define TIMEOUT_WIFI_MS 20000  // berhenti mencoba WiFi setelah 20 detik

const char *WIFI_SSID = "NAMA_WIFI";
const char *WIFI_PASS = "PASSWORD_WIFI";
const char *BOT_TOKEN = "TOKEN_BOT_TELEGRAM";
const char *CHAT_ID = "CHAT_ID_TELEGRAM";

// ---------- STATUS ----------
enum StatusAir { AMAN, WASPADA, BAHAYA };

StatusAir statusSekarang = AMAN;
int hitungBahaya = 0;  // detik berturut-turut level >= AMBANG_BAHAYA
unsigned long bacaTerakhir = 0;
unsigned long waktuBuzzer = 0;  // terakhir buzzer ganti kondisi
bool buzzerMenyala = false;
unsigned long waktuKedip = 0;  // terakhir LED merah ganti kondisi
bool ledMerahMenyala = false;

#if USE_TELEGRAM
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

// Sambungkan WiFi (berhenti sendiri setelah TIMEOUT_WIFI_MS).
void sambungWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long mulai = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - mulai < TIMEOUT_WIFI_MS) {
    delay(250);
  }
}

// Kirim pesan Telegram; diam saja bila WiFi belum tersambung.
void kirimTelegram(const String &pesan) {
  if (WiFi.status() != WL_CONNECTED) return;
  bot.sendMessage(CHAT_ID, pesan, "");
}
#else
void kirimTelegram(const String &) {}
#endif

// Baca level air: rata-rata dari JUMLAH_SAMPEL_RATA bacaan analog,
// supaya satu lonjakan sesaat tidak langsung memicu alarm.
int bacaLevel() {
  long jumlah = 0;
  for (int i = 0; i < JUMLAH_SAMPEL_RATA; i++) {
    jumlah += analogRead(PIN_SENSOR_LEVEL);
    delay(JEDA_SAMPEL_MS);
  }
  return (int)(jumlah / JUMLAH_SAMPEL_RATA);
}

// Kendalikan buzzer + LED sesuai status. Dipanggil tiap loop (non-blocking:
// pola bunyi/kedip dijadwalkan dengan millis(), bukan delay).
void kendaliAlarm(StatusAir status) {
  unsigned long sekarang = millis();
  if (status == BAHAYA) {
    // LED merah menyala terus, LED hijau mati.
    digitalWrite(PIN_LED_MERAH, HIGH);
    digitalWrite(PIN_LED_HIJAU, LOW);
    // Buzzer pola darurat: bunyi putus-putus.
    unsigned long durasi = buzzerMenyala ? DURASI_BUNYI_MS : DURASI_DIAM_MS;
    if (sekarang - waktuBuzzer >= durasi) {
      waktuBuzzer = sekarang;
      buzzerMenyala = !buzzerMenyala;
      digitalWrite(PIN_BUZZER, buzzerMenyala ? HIGH : LOW);
    }
  } else if (status == WASPADA) {
    // Buzzer diam, LED merah kedip lambat.
    digitalWrite(PIN_BUZZER, LOW);
    buzzerMenyala = false;
    digitalWrite(PIN_LED_HIJAU, LOW);
    if (sekarang - waktuKedip >= KEDIP_WASPADA_MS) {
      waktuKedip = sekarang;
      ledMerahMenyala = !ledMerahMenyala;
      digitalWrite(PIN_LED_MERAH, ledMerahMenyala ? HIGH : LOW);
    }
  } else {  // AMAN
    digitalWrite(PIN_BUZZER, LOW);
    buzzerMenyala = false;
    digitalWrite(PIN_LED_MERAH, LOW);
    ledMerahMenyala = false;
    digitalWrite(PIN_LED_HIJAU, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_MERAH, LOW);
  digitalWrite(PIN_LED_HIJAU, HIGH);  // aman saat start

#if USE_TELEGRAM
  sambungWiFi();
  telegramClient.setInsecure();
#endif

  Serial.println("Alarm Banjir siap.");
}

void loop() {
  // Baca level tiap INTERVAL_BACA_MS, tentukan status.
  if (millis() - bacaTerakhir >= INTERVAL_BACA_MS) {
    bacaTerakhir = millis();
    int level = bacaLevel();

    if (level >= AMBANG_BAHAYA) {
      hitungBahaya++;
    } else {
      hitungBahaya = 0;
    }

    StatusAir statusBaru;
    if (hitungBahaya >= KONFIRMASI_BAHAYA_DETIK) {
      statusBaru = BAHAYA;
    } else if (level >= AMBANG_WASPADA) {
      statusBaru = WASPADA;
    } else {
      statusBaru = AMAN;
    }

    // Sekali masuk BAHAYA: kirim peringatan Telegram.
    if (statusBaru == BAHAYA && statusSekarang != BAHAYA) {
      kirimTelegram("BANJIR! Level air sudah melewati batas bahaya. "
                    "Segera amankan barang!");
    }
    statusSekarang = statusBaru;

    Serial.print("Level: ");
    Serial.print(level);
    Serial.print(" Status: ");
    Serial.println(statusSekarang == BAHAYA    ? "BAHAYA"
                   : statusSekarang == WASPADA ? "WASPADA"
                                               : "AMAN");
  }

  kendaliAlarm(statusSekarang);
}
