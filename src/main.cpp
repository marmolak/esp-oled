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

#include "Common/Arch/ESP/Utils.hpp"
#include "Common/Arch/ESP/RTCWifi.hpp"

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

struct alignas(uint32_t) my_rtc_data_t : ExtdESP::rtc_data_base_t
{
    // last logo state
    logo_state last_state;
};

ExtdESP::RTCWifi<my_rtc_data_t> rtc_wifi; 

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
    auto &rtc_data = rtc_wifi.get_rtc_data();

    my_rtc_data_t &data = rtc_data.get();
    data.last_state = new_state;
    rtc_data.commit();
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


} // end namespace

void setup()
{
    Serial.begin(115200);
    //gdbstub_init();

    rtc_wifi.connect();

    if (rtc_wifi.is_restored())
    {
        auto &rtc_data = rtc_wifi.get_rtc_data();
        const my_rtc_data_t &data = rtc_data.get();
        state = data.last_state;
    }
//    setup_OTA();
//    MDNS.begin(Config::Wifi::mdns_hostname);
//    MDNS.update();
//    ArduinoOTA.handle();

    setup_MQTT();

    u8g2.begin();
    show_logo();

    if (client.connected())
    {
        client.publish("home/1st/workplace1/consolemaster/logo", state_name);
        client.disconnect();
    }

    ExtdESP::Utils::sleep_me(20e6);
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
