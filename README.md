# TinyGsmA7670E

[TinyGSM](https://github.com/vshymanskyy/TinyGSM) modem driver for the **SIMCom A7670E** (LTE Cat-1 with GSM fallback), plus a small raw-HTTP helper.

## Why

TinyGSM has no A7670E driver, and the SIM7600 driver almost works but not quite:

- After `+NETOPEN: 0` the A7670E's IP stack is not ready yet, so an immediate `AT+CIPOPEN` returns `ERROR`. This driver waits 2 s after NETOPEN.
- `connect()` retries `AT+CIPOPEN` up to 3 times, closing the link between attempts.
- `Network is already opened` from NETOPEN counts as success.
- Setup commands that some firmware rejects (`CTZR`, `CTZU`, `CIPCCFG`) don't abort init.

The modem's built-in HTTP stack (`AT+HTTPPARA="USERDATA"`) can't send more than one header cleanly. `TinyGsmA7670EHttp.h` writes the HTTP/1.1 request over a TCP socket itself, the same bytes `curl` sends, so any number of headers works.

## Install

Arduino IDE: **Library Manager → search "TinyGsmA7670E"**. It also installs TinyGSM (0.12.x).

PlatformIO / manual: download this repo as ZIP and use **Sketch → Include Library → Add .ZIP Library**.

## Usage

Use `TinyGsmClientA7670E.h` instead of `#define TINY_GSM_MODEM_xxx` + `<TinyGsmClient.h>`:

```cpp
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClientA7670E.h>
#include <TinyGsmA7670EHttp.h>

#define SerialAT Serial1

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);

void setup() {
  SerialAT.begin(115200);
  modem.init();
  modem.waitForNetwork(90000UL);
  modem.gprsConnect("your.apn");

  const char* headers[] = { "Content-Type: application/json",
                            "Authorization: Bearer xxx" };
  String resp;
  int status = A7670EHttp::post(client, "example.com", 80, "/api",
                                headers, 2, "{\"a\":1}", &resp);
}
```

`A7670EHttp::request / get / post` return the HTTP status code, or a negative error:

| Code | Meaning |
|-----:|---------|
| -1 | TCP connect failed |
| -2 | modem accepted fewer bytes than sent |
| -3 | no status line before timeout |
| -4 | reply was not `HTTP/1.x NNN` |

Full example: [examples/HttpPost](examples/HttpPost/HttpPost.ino).

### LTE / 2G

```cpp
modem.connectNetwork(APN);                     // LTE, then 2G if LTE fails
modem.connectNetwork(APN, A7670E_PREFER_2G);   // 2G, then LTE
modem.connectNetwork(APN, A7670E_LTE_ONLY);    // also A7670E_2G_ONLY, A7670E_AUTO
modem.connectNetwork(APN, A7670E_PREFER_LTE, 60000UL, "user", "pass");  // timeout per radio

modem.useLTE();  modem.use2G();  modem.useAuto();   // just set the mode
modem.getNetworkType();     // "LTE", "GSM", "NO SERVICE"
modem.getConnectedMode();   // A7670E_MODE_LTE / A7670E_MODE_2G after connectNetwork()
```

### Battery power: is it the supply or the modem?

On battery, the A7670E draws up to 2 A in short 2G transmit bursts (about 1 A on LTE). If the battery, wires or regulator can't deliver that, the voltage dips, the modem resets or shuts down, and it looks like the modem "stopped working". The library can tell the two cases apart.

**1. Load test** (run once, before connecting):

```cpp
A7670EPowerTest t = modem.powerStressTest(30000UL, true);  // 30 s, forced to 2G
Serial.println(a7670ePowerVerdictText(t.verdict));
// t.idleMv, t.minMv, t.dropMv, t.restarted, t.stoppedAnswering, t.underVoltage
```

It turns the radio off and on (registration means transmit bursts) and reads VBAT (`AT+CBC`) about every 50 ms. If the modem resets or goes silent, the supply is the problem. If it passes, power is not the cause of your failures.

**2. Quick check** (call when a send or connect fails, no radio activity):

```cpp
A7670EPowerVerdict v = modem.diagnosePower();
```

| Verdict | Meaning |
|---|---|
| `A7670E_PWR_OK` | supply and modem fine |
| `A7670E_PWR_WEAK` | works, but VBAT < 3.5 V, under-voltage warnings, or > 300 mV sag |
| `A7670E_PWR_BROWNOUT` | modem reset or shut down because of the supply |
| `A7670E_PWR_MODEM_ISSUE` | supply fine but modem hung: not a power problem |
| `A7670E_PWR_NO_RESPONSE` | modem silent and no supply reading to decide (see below) |
| `A7670E_PWR_RUN_STRESS_TEST` | modem restarted but voltage looked fine: run `powerStressTest()` |

**3. Optional: let the MCU read the battery.** If the modem doesn't answer at all, it can't report its own voltage. Give the library an ADC reading so it can still decide:

```cpp
uint16_t readBatteryMv() { return analogRead(A0) * 3300UL * 2 / 1023; }  // 2:1 divider
modem.setSupplyReader(readBatteryMv);
```

**What it watches:**
- modem restarts: the `RDY` / `*ATREADY` boot messages, and echo coming back on (`checkRestarted()`)
- `UNDER-VOLTAGE WARNNING`, `UNDER-VOLTAGE POWER DOWN` and `OVER-VOLTAGE ...` messages from the modem
- VBAT after every attach and connect (`getPowerStats()`: last, min, max, restart and warning counts)

`AT+CBC` is an averaged reading, so very short dips don't show in `minMv`. A restart under load is the reliable sign.

**Typical fixes:** a 1000-2200 µF low-ESR capacitor right at the modem's VBAT pin, short thick power wires, a battery or regulator rated for 2 A peaks, or use LTE only (lower peaks than 2G).

See [examples/PowerCheck](examples/PowerCheck/PowerCheck.ino).

### Extra modem functions

- `getRegistrationStatus()` (LTE, `CEREG`) and `getGsmRegistrationStatus()` (2G, `CREG`)
- `getNetworkMode()` / `setNetworkMode(2 | 13 | 38)`: auto / GSM only / LTE only
- `setDNS("8.8.8.8", "1.1.1.1")`

### Tuning (define before the include)

| Macro | Default |
|-------|---------|
| `TINY_GSM_A7670E_NETOPEN_SETTLE_MS` | `2000` |
| `TINY_GSM_A7670E_CIPOPEN_TRIES` | `3` |
| `TINY_GSM_A7670E_CIPOPEN_RETRY_MS` | `2000` |
| `A7670E_HTTP_CHUNK` | `1024` bytes per `AT+CIPSEND` |
| `TINY_GSM_A7670E_POWER_MONITOR` | `1` (read VBAT after attach and connect) |
| `TINY_GSM_A7670E_VBAT_LOW_MV` | `3500` |
| `TINY_GSM_A7670E_VBAT_CRITICAL_MV` | `3400` |
| `TINY_GSM_A7670E_VBAT_MAX_DROP_MV` | `300` |
| `TINY_GSM_A7670E_STRESS_SAMPLE_MS` | `50` |
| `TINY_GSM_A7670E_NO_TYPEDEFS` | define to skip the `TinyGsm` / `TinyGsmClient` typedefs |

## Limits

- Plain `http://` only. SSL/HTTPS is not implemented.
- Tested with TinyGSM 0.12.x.

## License

LGPL-3.0, same as TinyGSM (the driver is based on TinyGSM's SIM7600 driver).
