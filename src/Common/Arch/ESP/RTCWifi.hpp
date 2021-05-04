#pragma once

#include <stdint.h>
#include <WString.h>

#include <ESP8266WiFi.h>

#include "Common/Arch/ESP/RTC.hpp"
#include "Common/Arch/ESP/Utils.hpp"

namespace ExtdESP {

constexpr const uint32_t magic = 0xDEADBEEFu;

template <typename Storage>
class RTCWifi final
{
    private:
        struct alignas(uint32_t) rtc_data_base_t : Storage
        {
            // wifi info
            int32_t channel;

            // bssid is only 6 bytes long however, slots in RTC memory is just 32 bit wide slots
            uint32_t bssid[2];

            // magic number
            uint32_t wifi_stored;
        };
        static_assert(sizeof(rtc_data_base_t) <= 512, "There is only 512 bytes free in RTC memory.");
        static_assert(std::is_trivial<Storage>::value, "Storage tempalte argument must be trivial type aka plain old structure.");

        RTC<rtc_data_base_t> rtc_data;

    public:

        void connect(const String &ssid, const String &password)
        {
            // don't mess with sdk flash
            WiFi.persistent(false);

            const bool wifi_restored_and_connected = try_connect_wifi_with_rtc_settings(ssid, password);
            if (wifi_restored_and_connected)
            {
                return;
            }

            Serial.println(F("Normal boot..."));

            // connect to wifi
            const bool wifi_connected = connect_wifi(ssid, password);
            if (!wifi_connected)
            {
                // ok.. maybe is wifi down so we just going to sleep for some time
                Serial.println(F("Unable to access wifi network. Going to sleep for some time."));
                ExtdESP::Utils::sleep_me(3800e6);
                return;
            }

            // get stored wifi info
            rtc_data_base_t &data = rtc_data.get();

            void *const bssid_stored = reinterpret_cast<void *>(&data.bssid);
            const uint8_t *const bssid_actual = WiFi.BSSID();

            data.wifi_stored = 0;
            if (bssid_actual != NULL)
            {
                Serial.println(F("Storing wifi info."));
                ::memcpy(bssid_stored, bssid_actual, 6);
                data.channel = WiFi.channel();
                data.wifi_stored = ExtdESP::magic;
            }

            rtc_data.commit();
        }

        RTC<rtc_data_base_t> &get_rtc_data()
        {
            if (!is_restored())
            {
                rtc_data.restore();
            }

            return rtc_data;
        }

        bool is_restored() const
        {
            const rtc_data_base_t &data = rtc_data.get();
            return (data.wifi_stored == ExtdESP::magic);
        }

    private:

        [[nodiscard]] bool connect_wifi(const String &ssid, const String &password, int32_t channel = -1, const uint8_t* bssid = nullptr)
        {
            // We can pass default argument values to begin() because there is check for unvalid
            // channel and bssid values
            WiFi.begin(ssid, password, channel, bssid);

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

        bool try_connect_wifi_with_rtc_settings(const String &ssid, const String &password)
        {
            const struct rst_info *const actual_rst_info = ESP.getResetInfoPtr();
            if (actual_rst_info->reason != REASON_DEEP_SLEEP_AWAKE)
            {
                return false;
            }

            Serial.println(F("Woke from deep sleep. Restoring WIFI settings from RTC memory."));

            auto &rtc_data = get_rtc_data();
            rtc_data_base_t &data = rtc_data.get();

            if (data.wifi_stored != ExtdESP::magic)
            {
                Serial.println(F("RTC memory doesn't contain valid magic words."));
                return false;
            }

            // connect to wifi with stored informations
            uint8_t *bssid = reinterpret_cast<uint8_t *>(&(data.bssid[0]));       
            const bool connected = connect_wifi(ssid, password, data.channel, bssid);
            return connected;
        }
};

} // end of namespace