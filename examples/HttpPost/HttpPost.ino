// Minimal A7670E example: attach, then POST JSON with three headers.
// Serial = debug console, SerialAT = UART to the modem.

#define TINY_GSM_RX_BUFFER 1024
// #define TINY_GSM_DEBUG Serial

#include <TinyGsmClientA7670E.h>
#include <TinyGsmA7670EHttp.h>

// UART wired to the modem. Change for your board, e.g. on STM32:
//   HardwareSerial SerialAT(PB11, PB10);  // RX, TX
#ifndef SerialAT
#define SerialAT Serial1
#endif

#define APN     "airteliot.com"
#define HOST    "example.com"
#define PORT    80

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200);

  Serial.println("Waiting for modem...");
  if (!modem.testAT(30000UL) || !modem.init()) {
    Serial.println("Modem did not answer");
    return;
  }
  Serial.println("IMEI: " + modem.getIMEI());

  // LTE first, 2G if LTE fails (includes the 2 s NETOPEN settle delay).
  if (!modem.connectNetwork(APN, A7670E_PREFER_LTE, 90000UL)) {
    Serial.println("No network / data attach failed");
    Serial.println(a7670ePowerVerdictText(modem.diagnosePower()));
    return;
  }
  Serial.println("Network: " + modem.getNetworkType());
  modem.setDNS("8.8.8.8", "1.1.1.1");
  Serial.println("IP: " + modem.getLocalIP());

  const char* headers[] = {
      "Content-Type: application/json",
      "apikey: YOUR_KEY",
      "Authorization: Bearer YOUR_KEY",
  };
  String body = "{\"level\":42}";
  String resp;
  int status = A7670EHttp::post(client, HOST, PORT, "/api/test", headers, 3,
                                body, &resp);
  Serial.println("HTTP status: " + String(status));
  Serial.println("Body: " + resp);

  modem.gprsDisconnect();
}

void loop() {}
