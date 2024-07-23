// wifi manager, used by the webserver and wifi serial IP
#include "WifiManager.h"

#if OPERATIONAL_MODE == WIFI

#include "../tasks/OnTask.h"

#if STA_AUTO_RECONNECT == true
  void reconnectStationWrapper() { wifiManager.reconnectStation(); }
#endif

bool WifiManager::init() {

  if (!active) {

    #ifdef NV_WIFI_SETTINGS_BASE
      if (WifiSettingsSize < sizeof(WifiSettings)) { nv.initError = true; DL("ERR: WifiManager::init(), WifiSettingsSize error"); }

      if (!nv.hasValidKey() || nv.isNull(NV_WIFI_SETTINGS_BASE, sizeof(WifiSettings))) {
        ML("MSG: WiFi, writing defaults to NV");
        nv.writeBytes(NV_WIFI_SETTINGS_BASE, &settings, sizeof(WifiSettings));
      }

      nv.readBytes(NV_WIFI_SETTINGS_BASE, &settings, sizeof(WifiSettings));
    #endif

    IPAddress ap_ip = IPAddress(settings.ap.ip);
    IPAddress ap_gw = IPAddress(settings.ap.gw);
    IPAddress ap_sn = IPAddress(settings.ap.sn);

    M("MSG: WiFi, Master Pwd  = "); ML(settings.masterPassword);

    M("MSG: WiFi, AP Enable   = "); ML(settings.accessPointEnabled);
    M("MSG: WiFi, AP Fallback = "); ML(settings.stationApFallback);

    if (settings.accessPointEnabled || settings.stationApFallback) {
      M("MSG: WiFi, AP SSID     = "); ML(settings.ap.ssid);
      M("MSG: WiFi, AP PWD      = "); ML(settings.ap.pwd);
      M("MSG: WiFi, AP CH       = "); ML(settings.ap.channel);
      M("MSG: WiFi, AP IP       = "); ML(ap_ip.toString());
      M("MSG: WiFi, AP GATEWAY  = "); ML(ap_gw.toString());
      M("MSG: WiFi, AP SN       = "); ML(ap_sn.toString());
    }

    sta = &settings.station[stationNumber - 1];

    IPAddress sta_ip = IPAddress(sta->ip);
    IPAddress sta_gw = IPAddress(sta->gw);
    IPAddress sta_sn = IPAddress(sta->sn);
    M("MSG: WiFi, Sta Enable  = "); ML(settings.stationEnabled);

    if (settings.stationEnabled) {
      M("MSG: WiFi, Station#    = "); ML(stationNumber);
      M("MSG: WiFi, Sta DHCP En = "); ML(sta->dhcpEnabled);
      M("MSG: WiFi, Sta SSID    = "); ML(sta->ssid);
      M("MSG: WiFi, Sta PWD     = "); ML(sta->pwd);
      M("MSG: WiFi, Sta IP      = "); ML(sta_ip.toString());
      M("MSG: WiFi, Sta GATEWAY = "); ML(sta_gw.toString());
      M("MSG: WiFi, Sta SN      = "); ML(sta_sn.toString());
      IPAddress target = IPAddress(sta->target);
      M("MSG: WiFi, Sta TARGET  = "); ML(target.toString());
    }

  TryAgain:
    WiFi.setHostname("OnstepX");
    if (settings.accessPointEnabled && !settings.stationEnabled) {
      ML("MSG: WiFi, starting Soft AP");
      WiFi.softAP(settings.ap.ssid, settings.ap.pwd, settings.ap.channel);
      #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
      #endif
      WiFi.mode(WIFI_AP);
    } else
    if (!settings.accessPointEnabled && settings.stationEnabled) {
      ML("MSG: WiFi, starting Station");
      WiFi.begin(sta->ssid, sta->pwd);
      #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
      #endif
      WiFi.mode(WIFI_STA);
    } else
    if (settings.accessPointEnabled && settings.stationEnabled) {
      ML("MSG: WiFi, starting Soft AP");
      WiFi.softAP(settings.ap.ssid, settings.ap.pwd, settings.ap.channel);
      ML("MSG: WiFi, starting Station");
      WiFi.begin(sta->ssid, sta->pwd);
      #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
      #endif
      WiFi.mode(WIFI_AP_STA);
    }

    delay(100);
    
    if (settings.stationEnabled && !sta->dhcpEnabled) WiFi.config(sta_ip, sta_gw, sta_sn);
    if (settings.accessPointEnabled) WiFi.softAPConfig(ap_ip, ap_gw, ap_sn);

    // wait for connection
    if (settings.stationEnabled) { for (int i = 0; i < 8; i++) if (WiFi.status() != WL_CONNECTED) delay(1000); else break; }

    if (settings.stationEnabled && WiFi.status() != WL_CONNECTED) {

      // if connection fails fall back to access-point mode
      if (settings.stationApFallback && !settings.accessPointEnabled) {
        ML("MSG: WiFi starting station failed");
        WiFi.disconnect();
        delay(3000);
        ML("MSG: WiFi, switching to SoftAP mode");
        settings.stationEnabled = false;
        settings.accessPointEnabled = true;
        goto TryAgain;
      }

      if (!settings.accessPointEnabled) {
        ML("MSG: WiFi, initialization failed");
      } else {
        active = true;
        ML("MSG: WiFi, AP initialized station failed");
      }
    } else {    
      M("MSG: WiFi, Sta DHCP IP  = "); ML(WiFi.localIP());
      M("MSG: WiFi, Sta RSSI  = "); ML(WiFi.RSSI());
      active = true;
      ML("MSG: WiFi, initialized");

      #if MDNS_SERVER == ON && !defined(ESP8266)
        if (MDNS.begin(MDNS_NAME)) { ML("MSG: WiFi, mDNS started"); } else { ML("WRN: WiFi, mDNS start failed!"); }
      #endif

      #if STA_AUTO_RECONNECT == true
        if (settings.stationEnabled) {
          M("MSG: WiFi, start connection check task (rate 8s priority 7)... ");
          if (tasks.add(8000, 0, true, 7, reconnectStationWrapper, "WifiChk")) { ML("success"); } else { ML("FAILED!"); }
        }
      #endif
    }
  }

  return active;
}

#if STA_AUTO_RECONNECT == true
  void WifiManager::reconnectStation() {
    if (WiFi.status() != WL_CONNECTED) {
      ML("MSG: WiFi, attempting reconnect");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
#endif

void WifiManager::setStation(int number) {
  if (number >= 1 && number <= WifiStationCount) stationNumber = number;
  sta = &settings.station[stationNumber - 1];
}

void WifiManager::disconnect() {
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  active = false;
  ML("MSG: WiFi, disconnected");      
}

void WifiManager::writeSettings() {
  #ifdef NV_WIFI_SETTINGS_BASE
    ML("MSG: WifiManager, writing settings to NV");
    nv.writeBytes(NV_WIFI_SETTINGS_BASE, &settings, sizeof(WifiSettings));
  #endif
}

WifiManager wifiManager;

#endif
