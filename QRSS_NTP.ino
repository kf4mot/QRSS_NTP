/* QRSS DFCW Transmitter with State Machine
----------------------------------------
- Transmit a Morse-style message using DFCW on a Si5351.
- Uses a state machine for timing: IDLE → TX_CHAR → TX_SYMBOL → TX_PAUSE.
- Synchronizes to UTC via NTP, triggers TX every 10 minutes.
- ESP32 + Si5351 CLK1 output
- SSD1306 OLED display (SDA:6, SCL:7)
- Wi-Fi required for NTP
- dit = 6 s (dot), dah = 18 s (dash)
- sym_pause = 6 s, char_pause = 18 s
- TX repeats every 10 min UTC
- Serial logs state transitions and TX events.
- OLED displays "ON AIR" or "WAIT".

Band	QRSS Frequency (±100 Hz)      USB Dial(Hz)	Audio Frequency (Hz)
600m	476,100 kHz	                      474,200	        1.900
160m	1.837,900 kHz	                  1.836,600	        1.300
80m	3.569,900 kHz  popular	          3.568,600	        1.300
60m	5.288,550 kHz	                    5.287,200	        1.350
40m	7.039,900 kHz popular	            7.038,600	        1.300
30m	10.140,000 kHz la mas popular    10.138,700	        1.300
22m	13.555,400 kHz	                 13.553,900	        1.300
20m	14.096,900 kHz popular	         14.095,600	        1.300
17m	18.105,900 kHz	                 18.104,600	        1.300
15m	21.095,900 kHz	                 21.094,600	        1.300
12m	24.925,900 kHz	                 24.924,600	        1,300
10m	28.125,700 kHz	                 28,124,600	        1.100
6m	50.294,300 kHz	                 50.293,000	        1.100
*/

#include <Wire.h>
#include <si5351.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <WiFi.h>

#include "Fixed8x16.h"
#include "Credentials.h"

time_t lastEpoch = 0;
unsigned long lastMillis = 0;
bool timeValid = false;
time_t lastTxEpoch = 0;

// -------------------- OLED --------------------
#define OLED_SDA 6
#define OLED_SCL 7
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- SI5351 --------------------
Si5351 si5351;
int32_t manual_offset_hz = -145;  // freq. calibration
uint64_t base = (10140000ULL + manual_offset_hz) * 100ULL;
//uint64_t base = (10131000ULL + manual_offset_hz) * 100ULL; //JS8 freq. bench testing
uint64_t shift = 10ULL * 100ULL;
uint64_t MARK  = base + shift;
uint64_t SPACE = base;

// -------------------- QRSS timing --------------------
const int dit = 6000;         // QRSS6 ms
const int dah = dit * 3;
const int sym_pause = dit;
const int char_pause = dit * 3;

// -------------------- Message--------------------
extern const char* message[]; // Located in Credentials.h

// -------------------- State Machine --------------------
enum TxState {IDLE, TX_CHAR, TX_SYMBOL, TX_PAUSE};
TxState txState = IDLE;

int currentChar = 0;
int currentSymbol = 0;
unsigned long stateMillis = 0;

//-----------------------Declarations-----------------
//void connectWiFi();
//bool syncTimeUTC();
//time_t currentEpoch();
//bool isTransmitWindow(time_t now);

//void setFreq(char s);
//void stopFreq();

// -------------------- Helpers --------------------
void setFreq(char s) {
  if (s == '.') si5351.set_freq(MARK, SI5351_CLK1);
  else if (s == '-') si5351.set_freq(SPACE, SI5351_CLK1);
  si5351.output_enable(SI5351_CLK1, 1);
}

void stopFreq() {
  si5351.output_enable(SI5351_CLK1, 0);
}

//------------------Wifi+NTP-------------------------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("WiFi connecting");
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 20000) {
      Serial.println("\nWiFi failed");
      return;
    }
    Serial.print(".");
    delay(250);
  }
  Serial.println("\nWiFi connected");
}

bool syncTimeUTC() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeinfo, 500)) {
      Serial.println("UTC time synced");
      return true;
    }
  }
  Serial.println("NTP sync failed");
  return false;
}

//--------------------WiFi Status Update--------------------

int lastWiFiStatus = -1; // global variable to remember last Wi-Fi status

void updateWiFiStatus() {
    int currentStatus = WiFi.status();
    if (currentStatus != lastWiFiStatus) {
        lastWiFiStatus = currentStatus;

        display.fillRect(0, 48, 128, 16, BLACK); // Clear line 4

        display.setCursor(0, 60); // Set cursor to line 4

        if (currentStatus == WL_CONNECTED) {
            display.println("Wi-Fi: Connected");
        } else {
            display.println("Wi-Fi: Offline");
        }

        display.display();
    }
}

//--------------------Time helpers----------------------
time_t currentEpoch() {
  if (!timeValid) return 0;
  return lastEpoch + (millis() - lastMillis) / 1000;
}

bool isTransmitWindow(time_t now) {
  if (!timeValid) return false;
  int secondsIntoHour = now % 3600;
  return (secondsIntoHour % 600) == 0;  // every 10 minutes
}

// -------------------- Setup --------------------
void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);
  Serial.begin(115200);
  delay(1000);
  Serial.println("INIT");

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setFont(&Fixed8x16);
  display.setTextColor(WHITE);
  display.setCursor(0, 12); //Line 1
  display.println("DFCW NTP KF4MOT");
  display.display();

  connectWiFi();
  if (WiFi.status() == WL_CONNECTED && syncTimeUTC()) {
    time(&lastEpoch);
    lastMillis = millis();
    timeValid = true;
  }

  if (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0)) {
    Serial.println("ERROR SI5351!");
    while (1);
  }

  si5351.set_freq(SPACE, SI5351_CLK1);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA); //2, 4, 6, or 8MA
  si5351.output_enable(SI5351_CLK1, 0);

  display.setCursor(0, 28); //Line 2
  display.println("INIT OK");
  display.display();

uint64_t freq_hz = base / 100ULL;   // centi-Hz → Hz
float freq_mhz = freq_hz / 1e6;     // Hz → MHz

display.setCursor(0, 44); //Line 3
display.print(freq_mhz, 3);
display.println(" MHz");
}

// --------------------Loop --------------------
void loop() {
  time_t now = currentEpoch();

  updateWiFiStatus();

  if (txState == IDLE && timeValid && isTransmitWindow(now) && now != lastTxEpoch) {
    lastTxEpoch = now;
    currentChar = 0;
    currentSymbol = 0;
    txState = TX_CHAR;

    Serial.printf("[TX] UTC epoch %lu\n", now);
    if (WiFi.status() == WL_CONNECTED) {
  Serial.println("Wi-Fi: Connected");
} else {
  Serial.printf("Wi-Fi: Offline (status=%d)\n", WiFi.status());
}

    display.fillRect(0, 16, 128, 16, BLACK); // Clear line 2
    display.setCursor(0,28);
    display.println("        ON AIR");
    display.display();
  }

  switch (txState) {
    case IDLE:
      break;

    case TX_CHAR:
      if (message[currentChar] == NULL) {
        txState = IDLE;
        display.fillRect(0, 16, 128, 16, BLACK); // Clear line 2
        display.setCursor(0,28);
        display.println("WAIT");
        display.display();
        Serial.println("TX cycle complete");
      } else {
        currentSymbol = 0;
        txState = TX_SYMBOL;
        stateMillis = millis();
      }
      break;

    case TX_SYMBOL: {
      char s = message[currentChar][currentSymbol];
      unsigned long duration = (s == '.' ? dit : dah);

      if (millis() - stateMillis >= duration) {
        stopFreq();
        txState = TX_PAUSE;
        stateMillis = millis();
      } else {
        setFreq(s);
      }
      break;
    }

    case TX_PAUSE: {
      unsigned long pauseDuration =
        (message[currentChar][currentSymbol + 1] == 0) ? char_pause : sym_pause;

      if (millis() - stateMillis >= pauseDuration) {
        currentSymbol++;
        if (message[currentChar][currentSymbol] == 0) {
          currentChar++;
          txState = TX_CHAR;
        } else {
          txState = TX_SYMBOL;
        }
        stateMillis = millis();
      }
      break;
    }
  }
}

