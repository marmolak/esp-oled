#include "Common/Arch/ESP/Utils.hpp"
#include <Esp.h>

namespace CoolESP { namespace Utils {

[[noreturn]] void sleep_me(const uint64_t time_us)
{
    Serial.flush();
    ESP.deepSleep(time_us, WAKE_RF_DEFAULT);
}

}} // end of namespaces
