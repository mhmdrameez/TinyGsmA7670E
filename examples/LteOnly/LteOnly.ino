// Strict LTE: the modem only ever uses LTE, never 2G.
// If LTE is not available it stays offline instead of falling back.

#define TINY_GSM_RX_BUFFER 1024
// #define TINY_GSM_DEBUG Serial
// Or turn strict LTE on for the whole sketch at compile time:
// #define TINY_GSM_A7670E_STRICT_LTE 1

#include <TinyGsmClientA7670E.h>

// UART wired to the modem. Change for your board, e.g. on STM32:
//   HardwareSerial SerialAT(PB11, PB10);  // RX, TX
#ifndef SerialAT
#define SerialAT Serial1
#endif

#define APN "airteliot.com"

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);

bool connectLTE() {
  if (!modem.init()) return false;  // init() re-applies LTE only
  // In strict mode this is always LTE only, whatever preference you pass.
  if (!modem.connectNetwork(APN, A7670E_LTE_ONLY, 120000UL)) return false;
  Serial.println(modem.isOnLTE() ? "On LTE" : "Not on LTE!");
  return true;
}

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);

  Serial.println("Waiting for modem...");
  if (!modem.testAT(30000UL)) {
    Serial.println("Modem did not answer");
    return;
  }

  modem.setStrictLTE(true);  // LTE only, now and after every init()

  Serial.println(modem.use2G() ? "2G allowed?!" : "2G refused (strict LTE)");

  if (connectLTE()) {
    Serial.println("IP: " + modem.getLocalIP());
  } else {
    Serial.println("No LTE coverage / attach failed");
  }
}

void loop() {
  delay(30000);
  if (!modem.isOnLTE() || !modem.isGprsConnected()) {
    Serial.println("Lost LTE, reconnecting (LTE only)...");
    connectLTE();
  }
}
