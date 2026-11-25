/**
 * @file   TinyGsmClientA7670E.h
 * @brief  TinyGSM driver for the SIMCom A7670E (LTE Cat-1, GSM fallback).
 *
 * Based on TinyGsmClientSIM7600.h from TinyGSM 0.12 (LGPL-3.0), with the
 * A7670E-specific fixes:
 *   - After "+NETOPEN: 0" the IP stack is not ready yet, and an immediate
 *     AT+CIPOPEN returns ERROR. gprsConnect() waits
 *     TINY_GSM_A7670E_NETOPEN_SETTLE_MS (2 s) after NETOPEN.
 *   - connect() tries AT+CIPOPEN up to TINY_GSM_A7670E_CIPOPEN_TRIES (3)
 *     times, closing the link and waiting between attempts.
 *   - "Network is already opened" from NETOPEN counts as success.
 *   - Non-essential setup commands (CTZR, CTZU, CIPCCFG) do not abort init.
 *
 * Usage (instead of TINY_GSM_MODEM_xxx + <TinyGsmClient.h>):
 *
 *   #define TINY_GSM_RX_BUFFER 1024
 *   #include <TinyGsmClientA7670E.h>
 *   TinyGsm       modem(SerialAT);
 *   TinyGsmClient client(modem);
 */

#ifndef TINYGSM_CLIENT_A7670E_H_
#define TINYGSM_CLIENT_A7670E_H_

// #define TINY_GSM_DEBUG Serial

// Pull TinyGSM into the include path (Arduino library detection) first.
#include <TinyGsmCommon.h>

#ifndef TINY_GSM_A7670E_NETOPEN_SETTLE_MS
#define TINY_GSM_A7670E_NETOPEN_SETTLE_MS 2000UL
#endif
#ifndef TINY_GSM_A7670E_CIPOPEN_TRIES
#define TINY_GSM_A7670E_CIPOPEN_TRIES 3
#endif
#ifndef TINY_GSM_A7670E_CIPOPEN_RETRY_MS
#define TINY_GSM_A7670E_CIPOPEN_RETRY_MS 2000UL
#endif

// Power diagnostics. The A7670E runs on 3.4-4.2 V and shuts itself down
// below about 3.4 V; 2G transmit bursts draw up to 2 A.
#ifndef TINY_GSM_A7670E_POWER_MONITOR
#define TINY_GSM_A7670E_POWER_MONITOR 1  // sample VBAT after attach/connect
#endif
#ifndef TINY_GSM_A7670E_VBAT_LOW_MV
#define TINY_GSM_A7670E_VBAT_LOW_MV 3500
#endif
#ifndef TINY_GSM_A7670E_VBAT_CRITICAL_MV
#define TINY_GSM_A7670E_VBAT_CRITICAL_MV 3400
#endif
#ifndef TINY_GSM_A7670E_VBAT_MAX_DROP_MV
#define TINY_GSM_A7670E_VBAT_MAX_DROP_MV 300  // idle -> under-load sag
#endif
#ifndef TINY_GSM_A7670E_STRESS_SAMPLE_MS
#define TINY_GSM_A7670E_STRESS_SAMPLE_MS 50
#endif

// 1 = strict LTE from the start (same as calling setStrictLTE(true)).
#ifndef TINY_GSM_A7670E_STRICT_LTE
#define TINY_GSM_A7670E_STRICT_LTE 0
#endif

#define TINY_GSM_MUX_COUNT 10
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
#ifdef AT_NL
#undef AT_NL
#endif
#define AT_NL "\r\n"

#ifdef MODEM_MANUFACTURER
#undef MODEM_MANUFACTURER
#endif
#define MODEM_MANUFACTURER "SIMCom"

#ifdef MODEM_MODEL
#undef MODEM_MODEL
#endif
#define MODEM_MODEL "A7670E"

#include "TinyGsmModem.tpp"
#include "TinyGsmTCP.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmTime.tpp"
#include "TinyGsmNTP.tpp"
#include "TinyGsmBattery.tpp"

enum A7670ERegStatus {
  A7670E_REG_NO_RESULT    = -1,
  A7670E_REG_UNREGISTERED = 0,
  A7670E_REG_OK_HOME      = 1,
  A7670E_REG_SEARCHING    = 2,
  A7670E_REG_DENIED       = 3,
  A7670E_REG_UNKNOWN      = 4,
  A7670E_REG_OK_ROAMING   = 5,
};

// AT+CNMP values.
enum A7670ENetworkMode : uint8_t {
  A7670E_MODE_AUTO = 2,
  A7670E_MODE_2G   = 13,  // GSM only
  A7670E_MODE_LTE  = 38,  // LTE only
};

// Order in which connectNetwork() tries the radios.
enum A7670ENetworkPref : uint8_t {
  A7670E_PREFER_LTE = 0,  // LTE, then 2G if LTE fails
  A7670E_PREFER_2G,       // 2G, then LTE
  A7670E_LTE_ONLY,
  A7670E_2G_ONLY,
  A7670E_AUTO,            // let the modem choose
};

// Answer of diagnosePower() / powerStressTest().
enum A7670EPowerVerdict : uint8_t {
  A7670E_PWR_OK = 0,           // supply fine, modem fine
  A7670E_PWR_WEAK,             // modem works, but supply is low or sags a lot
  A7670E_PWR_BROWNOUT,         // modem reset/shut down because of the supply
  A7670E_PWR_MODEM_ISSUE,      // supply fine, modem hung: not a power problem
  A7670E_PWR_NO_RESPONSE,      // modem silent, no supply reading to decide
  A7670E_PWR_RUN_STRESS_TEST,  // modem restarted, voltage looked fine: run
                               // powerStressTest() to find out
};

inline const char* a7670ePowerVerdictText(A7670EPowerVerdict v) {
  switch (v) {
    case A7670E_PWR_OK: return "OK: supply and modem fine";
    case A7670E_PWR_WEAK:
      return "WEAK SUPPLY: modem works but voltage is low or sags under load";
    case A7670E_PWR_BROWNOUT:
      return "POWER PROBLEM: modem reset/shut down from low supply";
    case A7670E_PWR_MODEM_ISSUE:
      return "MODEM PROBLEM: supply fine but modem not answering";
    case A7670E_PWR_NO_RESPONSE:
      return "MODEM SILENT: set a supply reader to tell power from modem";
    case A7670E_PWR_RUN_STRESS_TEST:
      return "MODEM RESTARTED: voltage looked fine, run powerStressTest()";
  }
  return "?";
}

// Running power statistics (millivolts / counts).
struct A7670EPowerStats {
  int16_t  lastMv;               // last AT+CBC reading
  int16_t  minMv;                // lowest reading since reset
  int16_t  maxMv;                // highest reading since reset
  uint16_t samples;              // number of readings
  uint16_t reboots;              // modem restarts seen (RDY / echo back on)
  uint16_t underVoltWarnings;    // "UNDER-VOLTAGE WARNNING" URCs
  uint16_t underVoltPowerDowns;  // "UNDER-VOLTAGE POWER DOWN" URCs
  uint16_t overVoltage;          // "OVER-VOLTAGE ..." URCs
  uint16_t mcuSupplyMv;          // last reading from setSupplyReader()
};

// Result of powerStressTest().
struct A7670EPowerTest {
  int16_t            idleMv;    // average before the radio starts
  int16_t            minMv;     // lowest reading during the test
  int16_t            dropMv;    // idleMv - minMv
  uint16_t           samples;
  bool               restarted;         // modem rebooted during the test
  bool               stoppedAnswering;  // modem went silent
  bool               underVoltage;      // modem reported under-voltage
  A7670EPowerVerdict verdict;
};

// MCU-side supply reading in mV (e.g. ADC on a battery divider), or 0.
typedef uint16_t (*A7670ESupplyReader)();

class TinyGsmA7670E : public TinyGsmModem<TinyGsmA7670E>,
                      public TinyGsmGPRS<TinyGsmA7670E>,
                      public TinyGsmTCP<TinyGsmA7670E, TINY_GSM_MUX_COUNT>,
                      public TinyGsmSMS<TinyGsmA7670E>,
                      public TinyGsmTime<TinyGsmA7670E>,
                      public TinyGsmNTP<TinyGsmA7670E>,
                      public TinyGsmBattery<TinyGsmA7670E> {
  friend class TinyGsmModem<TinyGsmA7670E>;
  friend class TinyGsmGPRS<TinyGsmA7670E>;
  friend class TinyGsmTCP<TinyGsmA7670E, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSMS<TinyGsmA7670E>;
  friend class TinyGsmTime<TinyGsmA7670E>;
  friend class TinyGsmNTP<TinyGsmA7670E>;
  friend class TinyGsmBattery<TinyGsmA7670E>;

  /*
   * Inner Client
   */
 public:
  class GsmClientA7670E : public GsmClient {
    friend class TinyGsmA7670E;

   public:
    GsmClientA7670E() {}

    explicit GsmClientA7670E(TinyGsmA7670E& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    bool init(TinyGsmA7670E* modem, uint8_t mux = 0) {
      this->at       = modem;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      this->mux              = mux % TINY_GSM_MUX_COUNT;
      at->sockets[this->mux] = this;
      return true;
    }

   public:
    virtual int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      dumpModemBuffer(maxWaitMs);
      at->modemClose(mux);
      sock_connected = false;
    }
    void stop() override {
      stop(15000L);
    }

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Constructor
   */
 public:
  explicit TinyGsmA7670E(Stream& stream) : stream(stream) {
    memset(sockets, 0, sizeof(sockets));
    resetPowerStats();
    supplyReader_  = nullptr;
    lastBootMs_    = 0;
    connectedMode_ = 0;
    strictLTE_     = TINY_GSM_A7670E_STRICT_LTE;
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = nullptr) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientA7670E"));

    if (!testAT()) { return false; }

    sendAT(GF("E0"));  // Echo off
    if (waitResponse() != 1) { return false; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // verbose error codes
#else
    sendAT(GF("+CMEE=0"));
#endif
    waitResponse();

    DBG(GF("### Modem:"), getModemName());

    // Time zone URCs off, automatic time zone update on. Not fatal if the
    // firmware rejects them.
    sendAT(GF("+CTZR=0"));
    waitResponse(10000L);
    sendAT(GF("+CTZU=1"));
    waitResponse(10000L);

    // Strict LTE: put the radio back in LTE-only on every init (also after
    // a modem restart).
    if (strictLTE_) {
      sendAT(GF("+CNMP="), (uint8_t)A7670E_MODE_LTE);
      if (waitResponse() != 1) { DBG(GF("### Could not set LTE only")); }
    }

    SimStatus ret = getSimStatus();
    if (ret != SIM_READY && pin != nullptr && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    }
    return (ret == SIM_READY || ret == SIM_LOCKED);
  }

  bool factoryDefaultImpl() {
    return false;
  }

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = nullptr) {
    if (!testAT()) { return false; }
    sendAT(GF("+CRESET"));
    if (waitResponse(10000L) != 1) { return false; }
    delay(5000L);
    // The A7670E boots in roughly 10-20 s; poll instead of a fixed wait.
    if (!testAT(30000L)) { return false; }
    return init(pin);
  }

  bool powerOffImpl() {
    sendAT(GF("+CPOF"));
    return waitResponse() == 1;
  }

  bool radioOffImpl() {
    if (!setPhoneFunctionality(4)) { return false; }
    delay(3000);
    return true;
  }

  bool sleepEnableImpl(bool enable = true) {
    sendAT(GF("+CSCLK="), enable);
    return waitResponse() == 1;
  }

  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false) {
    sendAT(GF("+CFUN="), fun, reset ? ",1" : "");
    return waitResponse(10000L) == 1;
  }

  /*
   * Generic network functions
   */
 public:
  // LTE registration (the A7670E's main radio).
  A7670ERegStatus getRegistrationStatus() {
    return (A7670ERegStatus)getRegistrationStatusXREG("CEREG");
  }
  // 2G fallback registration.
  A7670ERegStatus getGsmRegistrationStatus() {
    return (A7670ERegStatus)getRegistrationStatusXREG("CREG");
  }

  int16_t getNetworkMode() {
    sendAT(GF("+CNMP?"));
    if (waitResponse(GF(AT_NL "+CNMP:")) != 1) { return false; }
    int16_t mode = streamGetIntBefore('\n');
    waitResponse();
    return mode;
  }

  // 2 = automatic, 13 = GSM only, 38 = LTE only
  // In strict LTE mode anything other than A7670E_MODE_LTE is refused.
  bool setNetworkMode(uint8_t mode) {
    if (strictLTE_ && mode != A7670E_MODE_LTE) {
      DBG(GF("### Strict LTE: mode refused:"), mode);
      return false;
    }
    sendAT(GF("+CNMP="), mode);
    return waitResponse() == 1;
  }

  /**
   * Strict LTE: the modem only ever uses LTE. Sets LTE-only now and on every
   * init(), refuses 2G/auto modes, counts the network as connected only on
   * LTE registration (CEREG), and makes connectNetwork() LTE-only.
   */
  bool setStrictLTE(bool enable = true) {
    strictLTE_ = enable;
    if (!enable) { return true; }
    return setNetworkMode(A7670E_MODE_LTE);
  }
  bool isStrictLTE() const {
    return strictLTE_;
  }

  // True if the modem is registered and serving on LTE right now.
  bool isOnLTE() {
    A7670ERegStatus s = getRegistrationStatus();
    return (s == A7670E_REG_OK_HOME || s == A7670E_REG_OK_ROAMING) &&
           getNetworkType() == "LTE";
  }

  // Set the DNS servers used by the socket stack (APN DNS is not always usable).
  bool setDNS(const char* primary, const char* secondary = nullptr) {
    if (secondary && strlen(secondary) > 0) {
      sendAT(GF("+CDNSCFG=\""), primary, GF("\",\""), secondary, '"');
    } else {
      sendAT(GF("+CDNSCFG=\""), primary, '"');
    }
    return waitResponse() == 1;
  }

 protected:
  bool isNetworkConnectedImpl() {
    A7670ERegStatus s = getRegistrationStatus();
    if (s == A7670E_REG_OK_HOME || s == A7670E_REG_OK_ROAMING) { return true; }
    if (strictLTE_) { return false; }  // 2G registration doesn't count
    s = getGsmRegistrationStatus();
    return (s == A7670E_REG_OK_HOME || s == A7670E_REG_OK_ROAMING);
  }

  String getLocalIPImpl() {
    sendAT(GF("+IPADDR"));
    String res;
    if (waitResponse(10000L, res) != 1) { return ""; }
    res.replace(AT_NL "OK" AT_NL, "");
    res.replace("+IPADDR:", "");
    res.trim();
    return res;
  }

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = nullptr,
                       const char* pwd = nullptr) {
    gprsDisconnect();  // start from a closed socket service

    if (user && strlen(user) > 0) {
      sendAT(GF("+CGAUTH=1,0,\""), pwd, GF("\",\""), user, '"');
      waitResponse();
    }

    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, '"');
    waitResponse();

    sendAT(GF("+CIPMODE=0"));  // command (non-transparent) mode
    waitResponse();

    sendAT(GF("+CIPSENDMODE=0"));  // don't wait for peer ACK on send
    waitResponse();

    // Retries, delay, ack, errMode, HeaderType=1 (+RECEIVE,<mux>,<len>),
    // async, min RTO. Not fatal if a firmware version rejects it.
    sendAT(GF("+CIPCCFG=10,0,0,0,1,0,75000"));
    waitResponse();

    sendAT(GF("+CIPTIMEOUT="), 75000, ',', 15000, ',', 15000);
    waitResponse();

    // NETOPEN replies OK at once, then "+NETOPEN: 0" when the stack is up.
    // If the stack is already up it replies
    // "+IP ERROR: Network is already opened" followed by ERROR.
    sendAT(GF("+NETOPEN"));
    int8_t res = waitResponse(75000L, GF(AT_NL "+NETOPEN: 0"),
                              GF("already opened"), GFP(GSM_ERROR));
    if (res == 2) {
      waitResponse(1000L);  // swallow the trailing ERROR
    } else if (res != 1) {
      DBG(GF("### NETOPEN failed:"), res);
      return false;
    }

    // "+NETOPEN: 0" arrives before the IP stack can open sockets; an
    // immediate CIPOPEN returns ERROR. Give it time to settle.
    delay(TINY_GSM_A7670E_NETOPEN_SETTLE_MS);
#if TINY_GSM_A7670E_POWER_MONITOR
    sampleVoltage();  // right after the attach bursts
#endif
    return true;
  }

  bool gprsDisconnectImpl() {
    // Closes all sockets and stops the socket service.
    sendAT(GF("+NETCLOSE"));
    int8_t res = waitResponse(60000L, GF(AT_NL "+NETCLOSE: 0"), GFP(GSM_ERROR),
                              GF("not opened"));
    if (res == 3) { waitResponse(1000L); }  // trailing ERROR
    // "Network not opened" (or ERROR when nothing was open) is fine too.
    return res == 1 || res == 2 || res == 3;
  }

  bool isGprsConnectedImpl() {
    sendAT(GF("+NETOPEN?"));
    if (waitResponse(GF(AT_NL "+NETOPEN: 1")) != 1) { return false; }
    waitResponse();

    sendAT(GF("+IPADDR"));
    return waitResponse() == 1;
  }

  String getProviderImpl() {
    sendAT(GF("+CSPN?"));
    if (waitResponse(GF("+CSPN:")) != 1) { return ""; }
    streamSkipUntil('"');
    String res = stream.readStringUntil('"');
    waitResponse();
    return res;
  }

  /*
   * SIM card functions
   */
 protected:
  String getSimCCIDImpl() {
    sendAT(GF("+CICCID"));
    if (waitResponse(GF(AT_NL "+ICCID:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  /*
   * Battery functions
   */
 protected:
  // "+CBC: 3.875V" -> millivolts
  int16_t getBattVoltageImpl() {
    sendAT(GF("+CBC"));
    if (waitResponse(GF(AT_NL "+CBC:")) != 1) { return 0; }
    float voltage = streamGetFloatBefore('\n');
    waitResponse();
    return static_cast<int16_t>(voltage * 1000);
  }

  int8_t getBattPercentImpl() TINY_GSM_ATTR_NOT_AVAILABLE;

  int8_t getBattChargeStateImpl() TINY_GSM_ATTR_NOT_AVAILABLE;

  bool getBattStatsImpl(int8_t& chargeState, int8_t& percent,
                        int16_t& milliVolts) {
    chargeState = 0;
    percent     = 0;
    milliVolts  = getBattVoltage();
    return true;
  }

  /*
   * Client related functions
   */
 protected:
  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 15) {
    if (ssl) {
      DBG("### SSL is not supported by this driver");
      return false;
    }

    // Receive data manually (AT+CIPRXGET=2) on all links.
    sendAT(GF("+CIPRXGET=1"));
    waitResponse();

    const uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

    for (uint8_t attempt = 1; attempt <= TINY_GSM_A7670E_CIPOPEN_TRIES;
         attempt++) {
      sendAT(GF("+CIPOPEN="), mux, GF(",\"TCP\",\""), host, GF("\","), port);
      // OK, then "+CIPOPEN: <mux>,<err>" (err 0 = connected). A bare
      // ERROR means the stack refused the command (e.g. not ready yet).
      int8_t res = waitResponse(timeout_ms, GF(AT_NL "+CIPOPEN:"),
                                GFP(GSM_ERROR));
      if (res == 1) {
        int8_t openedMux = streamGetIntBefore(',');
        int8_t err       = streamGetIntBefore('\n');
        if (openedMux == mux && err == 0) {
          DBG(GF("### CIPOPEN ok on attempt"), attempt);
#if TINY_GSM_A7670E_POWER_MONITOR
          sampleVoltage();
#endif
          return true;
        }
        DBG(GF("### CIPOPEN err"), err, GF("attempt"), attempt);
      } else {
        DBG(GF("### CIPOPEN ERROR/timeout, attempt"), attempt);
      }

      // Free the link before trying again.
      modemClose(mux);
      if (attempt < TINY_GSM_A7670E_CIPOPEN_TRIES) {
        delay(TINY_GSM_A7670E_CIPOPEN_RETRY_MS);
      }
    }
    return false;
  }

  void modemClose(uint8_t mux) {
    sendAT(GF("+CIPCLOSE="), mux);
    // OK then "+CIPCLOSE: <mux>,<err>", or ERROR if the link was not open.
    if (waitResponse(5000L) == 1) {
      if (waitResponse(3000L, GF(AT_NL "+CIPCLOSE:")) == 1) {
        streamSkipUntil('\n');
      }
    }
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    sendAT(GF("+CIPSEND="), mux, ',', (uint16_t)len);
    if (waitResponse(GF(">")) != 1) { return 0; }
    stream.write(reinterpret_cast<const uint8_t*>(buff), len);
    stream.flush();
    if (waitResponse(GF(AT_NL "+CIPSEND:")) != 1) { return 0; }
    streamSkipUntil(',');  // mux
    streamSkipUntil(',');  // requested length
    return streamGetIntBefore('\n');
  }

  size_t modemRead(size_t size, uint8_t mux) {
    if (!sockets[mux]) return 0;
#ifdef TINY_GSM_USE_HEX
    sendAT(GF("+CIPRXGET=3,"), mux, ',', (uint16_t)size);
#else
    sendAT(GF("+CIPRXGET=2,"), mux, ',', (uint16_t)size);
#endif
    if (waitResponse(GF("+CIPRXGET:")) != 1) { return 0; }
    streamSkipUntil(',');  // mode
    streamSkipUntil(',');  // mux
    int16_t len_requested = streamGetIntBefore(',');
    int16_t len_confirmed = streamGetIntBefore('\n');  // still unread
    for (int i = 0; i < len_requested; i++) {
      uint32_t startMillis = millis();
#ifdef TINY_GSM_USE_HEX
      while (stream.available() < 2 &&
             (millis() - startMillis < sockets[mux]->_timeout)) {
        TINY_GSM_YIELD();
      }
      char buf[4] = {0};
      buf[0]      = stream.read();
      buf[1]      = stream.read();
      char c      = strtol(buf, nullptr, 16);
#else
      while (!stream.available() &&
             (millis() - startMillis < sockets[mux]->_timeout)) {
        TINY_GSM_YIELD();
      }
      char c = stream.read();
#endif
      sockets[mux]->rx.put(c);
    }
    sockets[mux]->sock_available = len_confirmed;
    waitResponse();
    return len_requested;
  }

  size_t modemGetAvailable(uint8_t mux) {
    if (!sockets[mux]) return 0;
    sendAT(GF("+CIPRXGET=4,"), mux);
    size_t result = 0;
    if (waitResponse(GF("+CIPRXGET:")) == 1) {
      streamSkipUntil(',');  // mode 4
      streamSkipUntil(',');  // mux
      result = streamGetIntBefore('\n');
      waitResponse();
    }
    if (!result) { sockets[mux]->sock_connected = modemGetConnected(mux); }
    return result;
  }

  bool modemGetConnected(uint8_t mux) {
    // +CIPCLOSE: <link0_state>,<link1_state>,...,<link9_state>
    sendAT(GF("+CIPCLOSE?"));
    if (waitResponse(GF("+CIPCLOSE:")) != 1) {
      waitResponse();
      return false;
    }
    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      bool muxState = stream.parseInt();
      if (sockets[muxNo]) { sockets[muxNo]->sock_connected = muxState; }
    }
    waitResponse();
    if (!sockets[mux]) return false;
    return sockets[mux]->sock_connected;
  }

  /*
   * Utilities
   */
 public:
  bool handleURCs(String& data) {
    if (data.endsWith(GF(AT_NL "+CIPRXGET:"))) {
      int8_t mode = streamGetIntBefore(',');
      if (mode == 1) {
        int8_t mux = streamGetIntBefore('\n');
        if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
          sockets[mux]->got_data = true;
        }
        data = "";
        return true;
      }
      data += mode;
      return false;
    } else if (data.endsWith(GF(AT_NL "+RECEIVE:"))) {
      int8_t  mux = streamGetIntBefore(',');
      int16_t len = streamGetIntBefore('\n');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
        sockets[mux]->got_data = true;
        if (len >= 0 && len <= 1024) { sockets[mux]->sock_available = len; }
      }
      data = "";
      return true;
    } else if (data.endsWith(GF("+IPCLOSE:"))) {
      int8_t mux = streamGetIntBefore(',');
      streamSkipUntil('\n');  // reason
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
        sockets[mux]->sock_connected = false;
      }
      data = "";
      DBG("### Closed: ", mux);
      return true;
    } else if (data.endsWith(GF("+CIPEVENT:"))) {
      // "+CIPEVENT: NETWORK CLOSED UNEXPECTEDLY"
      streamSkipUntil('\n');
      DBG("### Network closed unexpectedly");
      for (int i = 0; i < TINY_GSM_MUX_COUNT; i++) {
        if (sockets[i]) { sockets[i]->sock_connected = false; }
      }
      data = "";
      return true;
    } else if (data.endsWith(GF(AT_NL "RDY")) ||
               data.endsWith(GF("*ATREADY:"))) {
      // Boot messages: the modem has just (re)started.
      if (data.endsWith(GF(":"))) { streamSkipUntil('\n'); }
      noteBoot();
      data = "";
      return true;
    } else if (data.endsWith(GF("-VOLTAGE WARN")) ||
               data.endsWith(GF("-VOLTAGE POWER DOWN"))) {
      // "UNDER-VOLTAGE WARNNING" / "UNDER-VOLTAGE POWER DOWN" /
      // "OVER-VOLTAGE WARNNING" / "OVER-VOLTAGE POWER DOWN"
      bool under = data.indexOf("UNDER") >= 0;
      bool down  = data.endsWith(GF("DOWN"));
      if (!down) { streamSkipUntil('\n'); }  // rest of "WARNNING"
      if (!under) {
        stats_.overVoltage++;
      } else if (down) {
        stats_.underVoltPowerDowns++;
      } else {
        stats_.underVoltWarnings++;
      }
      DBG(under ? "### UNDER-VOLTAGE" : "### OVER-VOLTAGE",
          down ? "POWER DOWN" : "WARNING");
      data = "";
      return true;
    }
    return false;
  }

  /*
   * Network mode (LTE / 2G)
   */
 public:
  bool useLTE() {
    return setNetworkMode(A7670E_MODE_LTE);
  }
  bool use2G() {
    return setNetworkMode(A7670E_MODE_2G);
  }
  bool useAuto() {
    return setNetworkMode(A7670E_MODE_AUTO);
  }

  // Radio actually in use: "LTE", "GSM", "NO SERVICE"... ("" = no answer).
  String getNetworkType() {
    sendAT(GF("+CPSI?"));
    if (waitResponse(GF("+CPSI:")) != 1) { return ""; }
    String type = stream.readStringUntil(',');
    waitResponse();
    type.trim();
    return type;
  }

  /**
   * Register and open data in one call, falling back to the other radio.
   *   modem.connectNetwork("airteliot.com");                    // LTE, then 2G
   *   modem.connectNetwork("airteliot.com", A7670E_2G_ONLY);
   * @return true when data is up. getConnectedMode() says which radio.
   */
  bool connectNetwork(const char*       apn,
                      A7670ENetworkPref pref             = A7670E_PREFER_LTE,
                      uint32_t          timeoutPerModeMs = 60000UL,
                      const char* user = nullptr, const char* pwd = nullptr) {
    if (strictLTE_) { pref = A7670E_LTE_ONLY; }  // never fall back to 2G
    uint8_t modes[2];
    uint8_t count = 0;
    switch (pref) {
      case A7670E_PREFER_LTE:
        modes[count++] = A7670E_MODE_LTE;
        modes[count++] = A7670E_MODE_2G;
        break;
      case A7670E_PREFER_2G:
        modes[count++] = A7670E_MODE_2G;
        modes[count++] = A7670E_MODE_LTE;
        break;
      case A7670E_LTE_ONLY: modes[count++] = A7670E_MODE_LTE; break;
      case A7670E_2G_ONLY: modes[count++] = A7670E_MODE_2G; break;
      default: modes[count++] = A7670E_MODE_AUTO; break;
    }

    connectedMode_ = 0;
    for (uint8_t i = 0; i < count; i++) {
      DBG(GF("### Trying network mode"), modes[i]);
      if (!setNetworkMode(modes[i])) { continue; }
      if (waitForNetwork(timeoutPerModeMs) && gprsConnect(apn, user, pwd)) {
        connectedMode_ = modes[i];
        return true;
      }
      DBG(GF("### No data on mode"), modes[i]);
    }
    return false;
  }

  // A7670E_MODE_LTE / _2G / _AUTO after a successful connectNetwork(), else 0.
  uint8_t getConnectedMode() const {
    return connectedMode_;
  }

  /*
   * Power diagnostics
   *
   * On battery the supply often sags during transmit bursts (up to 2 A on
   * 2G). The modem then resets or shuts down and "stops working". These
   * functions tell a power problem apart from a modem problem.
   */
 public:
  // Optional: MCU-side supply reading in mV (ADC on the battery). Used to
  // decide when the modem itself does not answer.
  void setSupplyReader(A7670ESupplyReader fn) {
    supplyReader_ = fn;
  }

  // Read the modem's own supply (AT+CBC) in mV and add it to the stats.
  // 0 = no answer.
  int16_t sampleVoltage() {
    int16_t mv = getBattVoltage();
    recordVoltage(mv);
    return mv;
  }

  A7670EPowerStats getPowerStats() const {
    return stats_;
  }

  void resetPowerStats() {
    stats_          = A7670EPowerStats();
    windowMinMv_    = 0;
    restartSnap_    = 0;
    diagUnderSnap_  = 0;
  }

  // True if the modem restarted since the last call. Seen from the boot
  // messages (RDY, *ATREADY) and from echo being on again: init() turns it
  // off, a restart turns it back on. After a restart, call init() and
  // connect again.
  bool checkRestarted() {
    waitResponse(100L);  // handle queued URCs (RDY, UNDER-VOLTAGE ...)
    sendAT(GF(""));
    String data;
    if (waitResponse(1000L, data) == 1 && data.indexOf("AT") >= 0) {
      sendAT(GF("E0"));
      waitResponse();
      if (stats_.reboots == restartSnap_) { noteBoot(); }  // URC was missed
    }
    bool restarted = stats_.reboots != restartSnap_;
    restartSnap_   = stats_.reboots;
    return restarted;
  }

  /**
   * Quick check, no radio activity. Call it when a send/connect fails to
   * learn whether the supply is to blame.
   */
  A7670EPowerVerdict diagnosePower() {
    if (!testAT(3000L)) {
      stats_.mcuSupplyMv = supplyReader_ ? supplyReader_() : 0;
      if (stats_.underVoltPowerDowns > 0 && consumeUnderVoltage()) {
        return A7670E_PWR_BROWNOUT;
      }
      if (!supplyReader_) { return A7670E_PWR_NO_RESPONSE; }
      return stats_.mcuSupplyMv < TINY_GSM_A7670E_VBAT_CRITICAL_MV
                 ? A7670E_PWR_BROWNOUT
                 : A7670E_PWR_MODEM_ISSUE;
    }

    bool    restarted = checkRestarted();
    int16_t mv        = sampleVoltage();
    bool    underV    = consumeUnderVoltage();
    bool    low = (mv > 0 && mv < TINY_GSM_A7670E_VBAT_LOW_MV) ||
               (windowMinMv_ > 0 && windowMinMv_ < TINY_GSM_A7670E_VBAT_LOW_MV);
    windowMinMv_ = mv;  // next window starts now

    if (restarted) {
      return (underV || low) ? A7670E_PWR_BROWNOUT : A7670E_PWR_RUN_STRESS_TEST;
    }
    if (underV || low) { return A7670E_PWR_WEAK; }
    return A7670E_PWR_OK;
  }

  /**
   * Load test: turns the radio off and on (optionally forced to 2G, the
   * heaviest load) and reads VBAT every few ms while the modem registers.
   * A restart or a large sag here means the supply is the problem; a clean
   * pass means it is not. Run it before connecting: it leaves the modem
   * registered but without data, and restores the network mode.
   */
  A7670EPowerTest powerStressTest(uint32_t durationMs = 30000UL,
                                  bool     force2G    = true) {
    A7670EPowerTest t = A7670EPowerTest();
    checkRestarted();  // start from a clean restart flag
    consumeUnderVoltage();

    int32_t sum = 0;
    uint8_t n   = 0;
    for (uint8_t i = 0; i < 5; i++) {
      int16_t mv = sampleVoltage();
      if (mv > 0) {
        sum += mv;
        n++;
      }
      delay(100);
    }
    t.idleMv = n ? static_cast<int16_t>(sum / n) : 0;
    t.minMv  = t.idleMv;

    int16_t oldMode = getNetworkMode();
    if (force2G && !strictLTE_) { setNetworkMode(A7670E_MODE_2G); }
    setPhoneFunctionality(0);
    delay(1000);
    setPhoneFunctionality(1);  // registration -> transmit bursts

    uint8_t        misses = 0;
    const uint32_t start  = millis();
    while (millis() - start < durationMs) {
      sendAT(GF("+CBC"));
      if (waitResponse(1000L, GF(AT_NL "+CBC:")) == 1) {
        int16_t mv =
            static_cast<int16_t>(streamGetFloatBefore('\n') * 1000);
        waitResponse();
        recordVoltage(mv);
        t.samples++;
        if (mv > 0 && (t.minMv == 0 || mv < t.minMv)) { t.minMv = mv; }
        misses = 0;
      } else if (++misses >= 5) {
        t.stoppedAnswering = true;
        break;
      }
      delay(TINY_GSM_A7670E_STRESS_SAMPLE_MS);
    }

    if (t.stoppedAnswering) {
      testAT(20000L);  // give a rebooting modem time to come back
    }
    t.restarted    = checkRestarted();
    t.underVoltage = consumeUnderVoltage();
    t.dropMv = (t.idleMv > 0 && t.minMv > 0) ? t.idleMv - t.minMv : 0;

    if (oldMode > 0 && !t.stoppedAnswering) {
      setNetworkMode(static_cast<uint8_t>(oldMode));
    }

    if (t.stoppedAnswering && !t.restarted && supplyReader_) {
      stats_.mcuSupplyMv = supplyReader_();
      t.verdict = stats_.mcuSupplyMv < TINY_GSM_A7670E_VBAT_CRITICAL_MV
                      ? A7670E_PWR_BROWNOUT
                      : A7670E_PWR_MODEM_ISSUE;
    } else if (t.restarted || t.stoppedAnswering) {
      t.verdict = A7670E_PWR_BROWNOUT;  // died under radio load
    } else if (t.underVoltage ||
               (t.minMv > 0 && t.minMv < TINY_GSM_A7670E_VBAT_LOW_MV) ||
               t.dropMv > TINY_GSM_A7670E_VBAT_MAX_DROP_MV) {
      t.verdict = A7670E_PWR_WEAK;
    } else {
      t.verdict = A7670E_PWR_OK;
    }
    return t;
  }

 protected:
  void recordVoltage(int16_t mv) {
    if (mv <= 0) { return; }
    stats_.lastMv = mv;
    stats_.samples++;
    if (stats_.minMv == 0 || mv < stats_.minMv) { stats_.minMv = mv; }
    if (mv > stats_.maxMv) { stats_.maxMv = mv; }
    if (windowMinMv_ == 0 || mv < windowMinMv_) { windowMinMv_ = mv; }
  }

  void noteBoot() {
    // RDY and *ATREADY both arrive on one boot; count it once.
    if (stats_.reboots == restartSnap_ || millis() - lastBootMs_ > 5000UL) {
      stats_.reboots++;
    }
    lastBootMs_ = millis();
    connectedMode_ = 0;
    for (int i = 0; i < TINY_GSM_MUX_COUNT; i++) {
      if (sockets[i]) { sockets[i]->sock_connected = false; }
    }
    DBG("### Modem restarted");
  }

  // True if new under-voltage URCs arrived since the last call.
  bool consumeUnderVoltage() {
    uint16_t total = stats_.underVoltWarnings + stats_.underVoltPowerDowns;
    bool     fresh = total != diagUnderSnap_;
    diagUnderSnap_ = total;
    return fresh;
  }

 public:
  Stream& stream;

 protected:
  GsmClientA7670E*   sockets[TINY_GSM_MUX_COUNT];
  A7670EPowerStats   stats_;
  A7670ESupplyReader supplyReader_;
  int16_t            windowMinMv_;
  uint16_t           restartSnap_;
  uint16_t           diagUnderSnap_;
  uint32_t           lastBootMs_;
  uint8_t            connectedMode_;
  bool               strictLTE_;
};

// Same names TinyGsmClient.h would give, so existing code keeps working.
#ifndef TINY_GSM_A7670E_NO_TYPEDEFS
typedef TinyGsmA7670E                  TinyGsm;
typedef TinyGsmA7670E::GsmClientA7670E TinyGsmClient;
#endif

#endif  // TINYGSM_CLIENT_A7670E_H_
