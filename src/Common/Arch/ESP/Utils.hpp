#pragma once

#include <stdint.h>

namespace ExtdESP { namespace Utils {

[[noreturn]] void sleep_me(const uint64_t time_us);

}} // end of namespaces