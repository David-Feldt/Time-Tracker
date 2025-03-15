# Time Tracker

I'm building a hardware time tracker and I hope to integrate it with an app

v5.0 release

Getting Started
Plug in your T-Embed. Make sure it is recognized by the OS (I recommend linux)
Clone the ESP-IDF repository V5.0 branch git clone --depth=1 --recursive -b v5.0 https://github.com/espressif/esp-idf.git
Run the install.sh in the ESP-IDF repo root install.sh all
Export the ESP-IDF config into the shell . ./export.sh
Check the IDF version idf.py --version (should be v5.0)
Clone this repo
Change to the example/esp-idf-v5.0 dir
idf.py flash && idf.py monitor