# Export 2 to Garmin Connect

This is a fork of export2garmin repo from RobertWojtowicz: https://github.com/RobertWojtowicz/export2garmin

Repo forked to add support for Inkplate2 board (ESP32 with e-ink display) only .ino file changed (at least for now)

Description from original repo:
### 2.2. Miscale module | ESP32 VERSION
- After weighing, Mi Body Composition Scale 2 is active for 15 minutes on bluetooth transmission;
- ESP32 module operates in a deep sleep and wakes up every 7 minutes, scans BLE devices for 10 seconds to acquire data from scale, process can be started immediately via reset button;
- ESP32 module sends acquired data via MQTT protocol to MQTT broker installed on server;
- Body weight and impedance data on server are appropriately processed by scripts;
- Processed data are sent to Garmin Connect;
- Raw and calculated data from scale is backed up on server in miscale_backup.csv file.


## If you like this project, you can buy original author a coffee
<a href="https://www.buymeacoffee.com/RobertWojtowicz" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>