# Beacon synch

## How to use example

### Configure the project

```
make menuconfig
```

* Set serial port under Serial Flasher Options.

# set SSID and password under main->synch_beacon.c. HARDCODED for now.
 
### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

Miquel Puig
