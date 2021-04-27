#include <Arduino.h>

#include <GDBStub.h>

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>

#include <MQTT.h>

#include <Wire.h>
#include <U8g2lib.h>

#include <AsyncDelay.h>

#include "Config/Wifi.hpp"
#include "Config/MQTT.hpp"

#include "sega.hpp"
#include "3do.hpp"
#include "nintendo.hpp"
#include "xbox.hpp"
#include "pngegg.hpp"

// Workaround for newest c++14 version
void operator delete(void *ptr, size_t size)
{
    (void) size;
    free(ptr);
}

void operator delete[](void *ptr, size_t size)
{
    (void) size;
    free(ptr);
}

void operator delete[](void *ptr)
{
    free(ptr);
}

namespace
{

  constexpr uint32_t magic = 0xDEADBEEFu;

  // MQTT related stuff
  WiFiClient net;
  MQTTClient client;

  /* used in high power code
  AsyncDelay delay_5000s { 5000u, AsyncDelay::MILLIS };
  AsyncDelay delay_2000s { 2000u, AsyncDelay::MILLIS };
  */


  enum class logo_state : uint8_t
  {
    NOTHING     = 0,
    THREEDO     = 1,
    SEGA        = 2,
    NINTENDO    = 3,
    XBOX        = 4,
    PLAYSTATION = 5,
  };

  String state_name = "3DO";
  logo_state state = logo_state::THREEDO;

  struct alignas(uint32_t) my_rtc_data_t
  {
      // wifi info
      int32_t channel;

      // bssid is only 6 bytes long however, slots in RTC memory is just 32 bit wide slots
      uint32_t bssid[2];

      uint32_t wifi_stored;

      // last logo state
      logo_state last_state;

  } my_rtc_data;
  static_assert(sizeof(my_rtc_data) <= 512, "There is only 512 bytes free in RTC memory.");


const uint16_t kPanasonicAddress = 0x4004;   // Panasonic address (Pre data)
const uint32_t kPanasonicPower = 0x100BCBD;  // Panasonic Power button

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


void drawLogothreedo() {
  u8g2.firstPage();
 do {
   u8g2.drawXBMP(0, 0, threedo_width, threedo_height, threedo_bits); 
 } while ( u8g2.nextPage() );
}

void drawLogosega() {
  u8g2.firstPage();
 do {
   u8g2.drawXBMP(0, 0, sega_width, sega_height, sega_bits); 
 } while ( u8g2.nextPage() );
}

void drawLogonin() {
  u8g2.firstPage();
 do {
   u8g2.drawXBMP(0, 0, nintendo_width, nintendo_height, nintendo_bits); 
 } while ( u8g2.nextPage() );
}

void drawLogoxbox() {
  u8g2.firstPage();
 do {
   u8g2.drawXBMP(0, 0, xbox_width, xbox_height, xbox_bits); 
 } while ( u8g2.nextPage() );
}

void drawLogops() {
  u8g2.firstPage();
 do {
   u8g2.drawXBMP(0, 0, pngegg_width, pngegg_height, pngegg_bits); 
 } while ( u8g2.nextPage() );
}

void update_state_to(const logo_state new_state)
{
    state = new_state;

    const uint32_t offset = offsetof(my_rtc_data_t, last_state) / sizeof(uint32_t); 

    uint32_t tmp_state = static_cast<uint32_t>(new_state);
    ESP.rtcUserMemoryWrite(offset, &tmp_state, sizeof(tmp_state));
}


void show_logo()
{
    switch (state)
    {
        case logo_state::THREEDO:
        {
            drawLogothreedo();
            state_name = "3DO";
            update_state_to(logo_state::SEGA);
            return;
        }
        break;
  
        case logo_state::SEGA:
        {
            drawLogosega();
            state_name = "SEGA";
            update_state_to(logo_state::NINTENDO);
            return;
        }
        break;


        case logo_state::NINTENDO:
        {
            drawLogonin();
            state_name = "NINTENDO";
            update_state_to(logo_state::XBOX);
            return;
        }
        break;

        case logo_state::XBOX:
        {
            drawLogoxbox();
            state_name = "XBOX";
            update_state_to(logo_state::PLAYSTATION);
            return;
        }
        break;

        case logo_state::PLAYSTATION:
        {
            drawLogops();
            state_name = "PLAYSTATION";
            update_state_to(logo_state::THREEDO);
            return;
        }
        break;
    }

}


void setup_OTA()
{
    ArduinoOTA.onStart([]()
    {
      if (ArduinoOTA.getCommand() == U_FLASH)
      {
        return;
      }

      // Take care only about FS flash.
      LittleFS.end();
    });

    ArduinoOTA.onEnd([]()
    {
        LittleFS.begin();
    });
  
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
    //  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error)
    {
      /*Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");*/
    });


    ArduinoOTA.setHostname((const char *) Config::Wifi::mdns_hostname);
    ArduinoOTA.setPassword((const char *) Config::Wifi::ota_password);

    ArduinoOTA.begin();

    ESP.getResetReason();
}

void messageReceived(String &topic, String &payload)
{
  //Serial.println("incoming: " + topic + " - " + payload);
}

void setup_MQTT()
{
    client.begin(Config::MQTT::broker_host, net);
    client.onMessage(messageReceived);

    for (uint8_t p = 0; p < 20; ++p)
    {
        const bool connected = client.connect("consolemaster", "public", "public");
        if (connected)
        {
            return;
        }
        delay(1000);
        if (!WiFi.isConnected()) {
          // give up we don't have mqtt
          return;
        }
    }
}


[[nodiscard]] bool connect_wifi(int32_t channel = -1, const uint8_t* bssid = nullptr)
{
    // We can pass default argument values to begin() because there is check for unvalid
    // channel and bssid values
    WiFi.begin(FPSTR(Config::Wifi::ssid), FPSTR(Config::Wifi::password), channel, bssid);

    for (uint8_t i = 0; i < 20; ++i)
    {
        if (WiFi.isConnected())
        {
            return true;
        }
        delay(500);
    }

    return false;
}

} // end namespace


[[noreturn]] void sleep_me(const uint64_t time_us)
{
    Serial.flush();
    ESP.deepSleep(time_us, WAKE_RF_DEFAULT);
}

bool try_connect_wifi_with_rtc_settings()
{
    const struct rst_info *const actual_rst_info = ESP.getResetInfoPtr();
    if (actual_rst_info->reason != REASON_DEEP_SLEEP_AWAKE)
    {
        return false;
    }

    Serial.println(F("Woke from deep sleep. Restoring WIFI settings from RTC memory."));
    uint32_t *data = reinterpret_cast<uint32_t *>(&my_rtc_data);
    ESP.rtcUserMemoryRead(0, data, sizeof(my_rtc_data_t));

    if (my_rtc_data.wifi_stored != magic)
    {
        Serial.println(F("RTC memory doesn't contain valid magic words."));
        return false;
    }

    state = my_rtc_data.last_state;
    // connect to wifi with stored informations
    uint8_t *bssid = reinterpret_cast<uint8_t *>(&(my_rtc_data.bssid[0]));       
    const bool connected = connect_wifi(my_rtc_data.channel, bssid);
    return connected;
}

void connect_store_maybe_restore_wifi_settings()
{
    const bool wifi_restored_and_connected = try_connect_wifi_with_rtc_settings();
    if (wifi_restored_and_connected)
    {
        return;
    }

    Serial.println(F("Normal boot..."));

    // connect to wifi
    const bool wifi_connected = connect_wifi();
    if (!wifi_connected)
    {
        // ok.. maybe is wifi down so we just going to sleep for some time
        Serial.println(F("Unable to access wifi network. Going to sleep for some time."));
        sleep_me(3800e6);
        return;
    }

    // get stored wifi info
    void *const bssid_stored = reinterpret_cast<void *>(&my_rtc_data.bssid);
    const uint8_t *const bssid_actual = WiFi.BSSID();

    my_rtc_data.wifi_stored = 0;
    if (bssid_actual != NULL)
    {
        Serial.println(F("Storing wifi info."));
        memcpy(bssid_stored, bssid_actual, 6);
        my_rtc_data.channel = WiFi.channel();
        my_rtc_data.wifi_stored = magic;
    }

    uint32_t *data = reinterpret_cast<uint32_t *>(&my_rtc_data);
    ESP.rtcUserMemoryWrite(0, data, sizeof(my_rtc_data_t));
}


void setup()
{
    Serial.begin(115200);
    //gdbstub_init();

    // don't mess with sdk flash
    WiFi.persistent(false);
    connect_store_maybe_restore_wifi_settings();

    setup_OTA();
    MDNS.begin(Config::Wifi::mdns_hostname);
    MDNS.update();
    ArduinoOTA.handle();

    u8g2.begin();

    setup_MQTT();

    show_logo();

    if (client.connected())
    {
        client.publish("home/1st/workplace1/consolemaster/logo", state_name);
        client.disconnect();
    }

    sleep_me(20e6);
}

void loop()
{

  /* old code which can be used later however setup now works as a loop.
    ArduinoOTA.handle();
    if (delay_2000s.isExpired())
    {
        MDNS.update();
        delay_2000s.restart();
    }

    if (!delay_5000s.isExpired())
    {
        return;
    }

    show_logo();
    client.publish("home/1st/workplace1/consolemaster/logo", state_name);
    delay_5000s.restart();*/
}
