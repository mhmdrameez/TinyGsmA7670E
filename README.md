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
| `TINY_GSM_A7670E_NO_TYPEDEFS` | define to skip the `TinyGsm` / `TinyGsmClient` typedefs |

## Limits

- Plain `http://` only. SSL/HTTPS is not implemented.
- Tested with TinyGSM 0.12.x.

## License

LGPL-3.0, same as TinyGSM (the driver is based on TinyGSM's SIM7600 driver).
