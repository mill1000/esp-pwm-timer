# ESP PWM
A PWM timer/scheduler for ESP32 devices. Designed to control aquarium lighting according to a daily schedule. Supports up to 8 channels of independent control, 4 independent PWM frequencies and schedule granularity down to the minute.

## Demo
![ESP PWM Web Interface Demo](docs/web_interface.gif)

## Features
ESP PWM supports up to 8 channel of independent PWM control on arbitrary GPIOs. PWM frequency of each channel can be selected from 1 of 4 independent sources. Channels can be named to personal preference.

### Automatic Time Sync
ESP PWM maintains time via NTP. To ensure correct local time, a proper timezone must be configured under System Settings.

#### Timezones
ESP PWM recognizes POSIX style timezones. e.g. Mountain Time (America/Denver) is represented as `MST7MDT,M3.2.0,M11.1.0`.

### Sweep Generation
The web interface can generate sweeps to slowly ramp up/down the light intensity over time. Right click the channel header to access the context menu and add a sweep.

### Backup & Restore
All system settings can be backed up to and restored from your local computer via the web interface. This backup & restore options can be found under System Settings.

## Hardware
While driving small LEDs directly is possible, controlling larger LED strips will require an driver. A MOSFET in a low-side switch configuration may be sufficient, but likely a constant-current LED driver such as the [PicoBuck](https://www.sparkfun.com/products/13705) or [Femtobuck](https://www.sparkfun.com/products/13716) will be required. ESP PWM can control any device that supports 3.3 V signaling.

## Color Palette
https://coolors.co/e4e3e3-88a0a8-546a76-000000