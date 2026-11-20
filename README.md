# Alarm Banjir — ESP32 + Sensor Level Air + Buzzer

Alat kecil yang memberi peringatan sebelum air banjir masuk rumah.
Sensor level air membaca tinggi air; begitu melewati batas bahaya,
buzzer berbunyi putus-putus, LED merah menyala, dan peringatan Telegram
"BANJIR!" terkirim ke HP-mu — jadi kamu punya waktu menyelamatkan
barang sebelum banjir datang.

Video penjelasan (60 detik):
https://youtube.com/shorts/XUpVm6eTSLs

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Sensor level air (analog) | Membaca tinggi air (ADC 0..4095) |
| Buzzer aktif HIGH | Alarm bunyi putus-putus saat bahaya |
| LED merah | Menyala saat bahaya / kedip lambat saat waspada |
| LED hijau | Menyala saat aman |
| Resistor 220 Ω ×2 | Seri dengan LED merah dan LED hijau |

## Wiring

| Dari | Ke |
|---|---|
| Sensor S (analog) | ESP32 GPIO34 |
| Sensor VCC | ESP32 3V3 |
| Sensor GND | GND |
| Buzzer + | ESP32 GPIO25 (aktif HIGH) |
| Buzzer - | GND |
| LED merah | ESP32 GPIO33 (seri 220 Ω ke GND) |
| LED hijau | ESP32 GPIO32 (seri 220 Ω ke GND) |
| Semua GND | Digabung |

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

1. ESP32 membaca level air tiap 1 detik, dirata-rata dari 5 bacaan
   supaya satu lonjakan sesaat tidak langsung memicu alarm.
2. Level ≥ AMBANG_WASPADA (1500) → status WASPADA: LED merah kedip
   lambat, buzzer diam.
3. Level ≥ AMBANG_BAHAYA (2500) selama 3 detik berturut-turut →
   status BAHAYA: buzzer bunyi putus-putus (pola darurat), LED merah
   menyala terus, dan peringatan Telegram "BANJIR!" terkirim sekali
   tiap masuk status ini.
4. Level di bawah ambang waspada → status AMAN: LED hijau menyala.
5. Status dan nilai level dicetak ke Serial Monitor (115200) tiap detik.

## Konfigurasi (di awal sketch)

- `USE_TELEGRAM` (1) — kirim peringatan Telegram. Set 0 untuk tanpa
  Telegram (dan tanpa WiFi).
- `AMBANG_WASPADA` (1500) / `AMBANG_BAHAYA` (2500) — ambang ADC 12-bit.
  Kalibrasi: buka Serial Monitor, lihat nilai "Level" saat sensor kering
  dan saat terendam, lalu sesuaikan kedua ambang ini.
- `KONFIRMASI_BAHAYA_DETIK` (3) — bahaya harus terkonfirmasi 3 detik
  berturut-turut sebelum alarm berbunyi.
- `DURASI_BUNYI_MS` / `DURASI_DIAM_MS` (300 / 300) — pola bunyi
  putus-putus buzzer saat bahaya.
- `KEDIP_WASPADA_MS` (1000) — interval kedip LED merah saat waspada.
- `WIFI_SSID` / `WIFI_PASS` — WiFi rumahmu.
- `BOT_TOKEN` / `CHAT_ID` — token bot dan chat ID Telegram.

## Library yang dibutuhkan

| Library | Versi | Keterangan |
|---|---|---|
| UniversalTelegramBot | 1.3.0 | Notifikasi Telegram (hanya bila `USE_TELEGRAM` 1) |
| ArduinoJson | 6.21.5 | Dependensi UniversalTelegramBot (WAJIB v6 — v7 tidak dipakai di sini) |

Semua library di atas juga tersedia sebagai ZIP di folder [`libraries/`](libraries/)
— install lewat Arduino IDE: Sketch → Include Library → Add .ZIP Library.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 AlarmBanjir
```

Terverifikasi 20 Nov 2026: sketch 1.070.285 byte (81% flash),
RAM 50.616 byte (15%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
