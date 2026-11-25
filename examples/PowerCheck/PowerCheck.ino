// Is it the battery or the modem?
//
// 1. Load test: switches the radio on (2G, the heaviest load) and watches
//    the modem's supply. A reset or a big voltage drop = power problem.
// 2. Connects with LTE first, 2G as fallback.
// 3. Every 30 s checks the modem; if it restarted or went silent, prints
//    whether the supply or the modem is to blame, then reconnects.

#define TINY_GSM_RX_BUFFER 1024
// #define TINY_GSM_DEBUG Serial

#include <TinyGsmClientA7670E.h>

// UART wired to the modem. Change for your board, e.g. on STM32:
//   HardwareSerial SerialAT(PB11, PB10);  // RX, TX
#ifndef SerialAT
#define SerialAT Serial1
#endif

#define APN "airteliot.com"

// Optional: MCU reads the battery through a divider (here 2:1 on A0).
// Lets the library decide even when the modem does not answer at all.
// Remove setSupplyReader() below if you have no divider.
#define BATT_PIN A0
uint16_t readBatteryMv() {
  return (uint32_t)analogRead(BATT_PIN) * 3300UL * 2UL / 1023UL;
}

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);

void printTest(const A7670EPowerTest& t) {
  Serial.println("---- power stress test ----");
  Serial.print("idle  mV: "); Serial.println(t.idleMv);
  Serial.print("min   mV: "); Serial.println(t.minMv);
  Serial.print("drop  mV: "); Serial.println(t.dropMv);
  Serial.print("samples : "); Serial.println(t.samples);
  Serial.print("restart : "); Serial.println(t.restarted ? "YES" : "no");
  Serial.print("silent  : "); Serial.println(t.stoppedAnswering ? "YES" : "no");
  Serial.print("under-V : "); Serial.println(t.underVoltage ? "YES" : "no");
  Serial.print("verdict : "); Serial.println(a7670ePowerVerdictText(t.verdict));
}

bool connect() {
  if (!modem.init()) return false;
  // LTE first, then 2G. Other choices: A7670E_PREFER_2G, A7670E_LTE_ONLY,
  // A7670E_2G_ONLY, A7670E_AUTO. Or call modem.useLTE() / modem.use2G().
  if (!modem.connectNetwork(APN, A7670E_PREFER_LTE, 60000UL)) return false;
  Serial.print("Connected on: ");
  Serial.println(modem.getNetworkType());  // "LTE" or "GSM"
  return true;
}

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);
  modem.setSupplyReader(readBatteryMv);

  Serial.println("Waiting for modem...");
  if (!modem.testAT(30000UL) || !modem.init()) {
    Serial.println("Modem did not answer");
    Serial.println(a7670ePowerVerdictText(modem.diagnosePower()));
    return;
  }

  printTest(modem.powerStressTest(30000UL, true));

  if (!connect()) Serial.println("Connect failed");
}

void loop() {
  delay(30000);

  A7670EPowerVerdict v = modem.diagnosePower();
  A7670EPowerStats   s = modem.getPowerStats();
  Serial.print("VBAT ");
  Serial.print(s.lastMv);
  Serial.print(" mV (min ");
  Serial.print(s.minMv);
  Serial.print(") restarts ");
  Serial.print(s.reboots);
  Serial.print(" -> ");
  Serial.println(a7670ePowerVerdictText(v));

  if (v == A7670E_PWR_BROWNOUT || v == A7670E_PWR_RUN_STRESS_TEST ||
      !modem.isGprsConnected()) {
    Serial.println("Reconnecting...");
    connect();
  }
}
