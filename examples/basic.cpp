#include <SPIFFS.h>
#include "StructFS.h" 

struct Config {
    uint32_t baud;
    uint8_t  mode;
    char     name[16];
};

void setDefaults(Config& c) {
    c.baud = 115200;
    c.mode = 1;
    strncpy(c.name, "default", sizeof(c.name));
}

StructFS::Storage<Config> cfg(SPIFFS, "/config.bin", 1, 0x31474643UL, &setDefaults);

void setup() {
    SPIFFS.begin(true);
    cfg.loadOrDefault();

    // работа с cfg.data()
    cfg.data().mode = 2;
    cfg.save();
}
