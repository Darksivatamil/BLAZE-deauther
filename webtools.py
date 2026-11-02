#!/usr/bin/env python3
"""BLAZE web converter.

Gzips every source file in data/web/ and regenerates webfiles.h
(PROGMEM byte arrays + copyWebFiles()) in the exact structure the
deauther expects.

Usage:  python3 webtools.py
"""

import gzip
import os

ROOT = os.path.dirname(os.path.abspath(__file__))
WEB = os.path.join(ROOT, "data", "web")
OUT = os.path.join(ROOT, "webfiles.h")

# order matters: same order as the original webConverter.py output
FILES = [
    "index.html",
    "scan.html",
    "info.html",
    "ssids.html",
    "attack.html",
    "settings.html",
    "login.html",
    "tracker.html",
    "twin.html",
    "style.css",
    "js/ssids.js",
    "js/site.js",
    "js/attack.js",
    "js/scan.js",
    "js/settings.js",
    "js/blaze.js",
    "js/tracker.js",
    "js/twin.js",
    "lang/en.lang",
    "LICENSE",
]

COMMENT = ("// comment that out if you want to save program memory "
           "and know how to upload the web files to the SPIFFS manually")


def array_name(rel):
    # js/site.js -> sitejs, index.html -> indexhtml, LICENSE -> LICENSE
    return rel.split("/")[-1].replace(".", "")


def main():
    arrays = []
    entries = []

    for rel in FILES:
        src = os.path.join(WEB, rel)
        if not os.path.isfile(src):
            raise SystemExit("missing source: " + src)

        with open(src, "rb") as f:
            data = f.read()

        gz = gzip.compress(data, compresslevel=9, mtime=0)
        with open(src + ".gz", "wb") as f:
            f.write(gz)

        name = array_name(rel)
        path = "/web/" + rel + ".gz"
        hexdata = ", ".join("0x%02x" % b for b in gz)

        arrays.append("const char %s[] PROGMEM = {%s};" % (name, hexdata))
        entries.append(
            'if(!LittleFS.exists("%s") || force) '
            'progmemToSpiffs(%s, sizeof(%s), "%s");' % (path, name, name, path)
        )

        print("%-24s %6d bytes -> %s" % (name, len(gz), path))

    body = []
    body.append("#ifndef webfiles_h")
    body.append("#define webfiles_h")
    body.append("")
    body.append(COMMENT)
    body.append("#define USE_PROGMEM_WEB_FILES ")
    body.append("")
    body.append("#ifdef USE_PROGMEM_WEB_FILES")
    body.extend(arrays)
    body.append("#endif")
    body.append("")
    body.append("void copyWebFiles(bool force){")
    body.append("#ifdef USE_PROGMEM_WEB_FILES")
    body.append("if(settings::getWebSettings().use_spiffs){")
    body.extend("  " + e for e in entries)
    body.append("}")
    body.append("#endif")
    body.append("}")
    body.append("")
    body.append("#endif")
    body.append("")

    with open(OUT, "w") as f:
        f.write("\n".join(body))

    print("wrote %s (%d arrays)" % (OUT, len(arrays)))


if __name__ == "__main__":
    main()
