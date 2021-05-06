#pragma once

#include <stdint.h>
#include <type_traits>

#include <ESP.h>

namespace CoolESP {

template <typename Storage>
class RTC
{
    static_assert(sizeof(Storage) <= 512, "There is only 512 bytes free in RTC memory.");

    public:

        // It's your responsibility to commit data to memory...
        Storage &get() 
        {
            return rtc_data;
        }

        const Storage &get() const
        {
            return rtc_data;
        }

        void commit()
        {
            uint32_t *data = reinterpret_cast<uint32_t *>(&rtc_data);
            ESP.rtcUserMemoryWrite(0, data, sizeof(Storage));
        }

        void restore()
        {
            uint32_t *data = reinterpret_cast<uint32_t *>(&rtc_data);
            ESP.rtcUserMemoryRead(0, data, sizeof(Storage));
        }

    private:
        Storage rtc_data;
};

} // end of namespace