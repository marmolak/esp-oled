#pragma once

#include <sys/pgmspace.h>

// This file is ignored by .gitignore.

namespace Config { namespace Wifi {

    const char *const mdns_hostname PROGMEM = "espdisplay";

    const String ssid      = "";
    const String password  = "";

    // Don't forget to change this password in platformio.ini. 
    const char *const ota_password PROGMEM = "123";

}} // end of namespace
