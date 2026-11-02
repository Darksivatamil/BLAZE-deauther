/* BLAZE - Team DARK
   Tiny key=value config on LittleFS (/blaze.cfg) */

#include "BlazeCfg.h"

#include <LittleFS.h>

namespace BlazeCfg {
    const char* PATH = "/blaze.cfg";

    void setup() {
        // LittleFS.begin() already ran in the .ino setup()
    }

    String get(const String& key, const String& def) {
        File f = LittleFS.open(PATH, "r");
        if (!f) return def;

        String out     = def;
        String prefix  = key + "=";

        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.startsWith(prefix)) {
                out = line.substring(prefix.length());
                break;
            }
        }
        f.close();
        return out;
    }

    long getInt(const String& key, long def) {
        String v = get(key, String(def));
        return v.toInt();
    }

    float getFloat(const String& key, float def) {
        String v = get(key, String(def));
        return v.toFloat();
    }

    bool set(const String& key, const String& value) {
        String out      = "";
        String prefix   = key + "=";
        bool   replaced = false;

        File f = LittleFS.open(PATH, "r");
        if (f) {
            while (f.available()) {
                String line = f.readStringUntil('\n');
                line.trim();
                if (line.length() == 0) continue;
                if (line.startsWith(prefix)) {
                    out += prefix + value + "\n";
                    replaced = true;
                } else {
                    out += line + "\n";
                }
            }
            f.close();
        }
        if (!replaced) out += prefix + value + "\n";

        f = LittleFS.open(PATH, "w");
        if (!f) return false;
        f.print(out);
        f.close();
        return true;
    }
}
