# PiLED
![C Build](https://github.com/PolisanTheEasyNick/PiLED/actions/workflows/c.yml/badge.svg)  

<p align="left">
  <img src="https://github.com/user-attachments/assets/31671b08-1246-451f-ad12-4d499cce511a" width=30% height=30%>
</p>

This project used for installing on Raspberry Pi.  
LED Strip is connected to Raspberry Pi GPIO pins.  
This program opens TCP server on port 3384 and waits for plain TCP packets, created as described below.  
If builded with `libwebsockets` library and with DWITH_WS=ON flag, PiLED opens 3385 port as WebSocket server and waits for "RED,GREEN,BLUE,DURATION" data.  
If builded with `microhttpd` library and with DWITH_HTML=ON flag, PiLED opens 3386 port as HTML server and waits for `?R={red}&G={green}&B={blue}&DURATION={dur}` GET request.  
Note that HTML and WS servers skips all security checks, so make sure to not expose these ports to WAN and create additional layer of security by network side if used.  

![scheme](https://github.com/user-attachments/assets/8dfd6e76-bd6d-4d10-821b-5ac0036d3364)


> [!WARNING]  
> Before assembling or using this electronic schematic, **ensure that all voltages and power requirements are correctly calculated** and appropriate for your components and setup.  
> Incorrect voltage or power supply can result in permanent damage to the components or pose safety risks.

> [!CAUTION]
> I provide this schematic as-is without any warranties or guarantees.
> By using this design, you agree that I am not liable for any damages, injuries, or losses that may occur as a result of following these instructions.  
> **Use at your own risk.**  


## Features:
* Basic color change
* Smooth color change from current color to desired with given timing
* Fade and Pulse animations
* [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) SDK support (refer to [OpenRGB](#openrgb) section)
* WebSocket support for simpler controlling.

LED PROTOCOL v4
Simply contains `HEADER` + `HMAC-SHA-256` + `PAYLOAD`  
Currently max buffer size: 8+8+1+1+32+1+1+1+1+1 = 55 bytes.

## Version History
| Version | Description                                                  |
| :-----: | ------------------------------------------------------------ |
| v1, 0x1 | Initial release, support of changing color                   |
| v2, 0x2 | Added plain changing from current color to new               |
| v3, 0x3 | Added support of getting current color                       |
| v4, 0x4 | Added support of animations (would be added more by new OPs) |



## HEADER Structure
| Offset |   Name   | Size              | Version | Description              |
| :----: | :------: | :---------------: | :-----: | ------------------------ |
|  0x0   |timestamp | 8 bytes, unsigned |    1    |Timestamp of current time |
|  0x8   |nonce     | 8 bytes, unsigned |    1    |Random data               |
|  0x10  |version   | 1 byte, unsigned  |    1    |Used version of protocol  |
|  0x11  |OP        | 1 byte, unsigned  |    3    |Operational code          |


## HMAC-SHA-256 (32 bytes)
Must be generated with SHA-256 algorithm with using `shared_secret` which writed in both client and server and `data` must be used `header+payload`:
`HMAC(SHA-256, shared_secret, header + payload)`

## Operational Codes
| Value | Name                                            | Description                                     |
| :---: | :---------------------------------------------: | ----------------------------------------------- |
| 0     | [LED_SET_COLOR](#led_set_color)                 | Set RGB color, described in PAYLOAD             |
| 1     | [LED_GET_CURRENT_COLOR](#led_get_current_color) | Request led-server current LEDs color           |
| 2     | [ANIM_SET_FADE](#anim_set_fade)                 | Start FADE animation                            |
| 3     | [ANIM_SET_PULSE](#anim_set_pulse)               | Start PULSE animation                           |
| 4     | [SYS_TOGGLE_SUSPEND](#sys_toggle_suspend)       | Toggle suspend mode                             |
| 5     | [SYS_COLOR_CHANGED](#sys_color_changed)         | Sent from server to all clients about new color |


## PAYLOAD Structure
| Offset | Name        | Size                      | Version | Description                                               |
| :----: | :---------: | :-----------------------: | :-----: |---------------------------------------------------------- |
|  0x32  | RED color   | 1 byte, 8 bits, unsigned  |    1    | RED color value                                           |
|  0x33  | GREEN color | 1 byte, 8 bits, unsigned  |    1    | GREEN color value                                         |
|  0x34  | BLUE color  | 1 byte, 8 bits, unsigned  |    1    | BLUE color value                                          |
|  0x35  | Duration    | 1 byte, 8 bits, unsigned  |    2    | Duration in seconds of changing color from current to new |
|  0x36  | Speed       | 1 byte, 8 bits, unsigned  |    4    | Speed of animation, from 0 to 255, conv. units            |

## Packet-Specific Documentation

## LED_SET_COLOR
Request size: 55 bytes (`HEADER` + `HMAC` + `PAYLOAD`)  
Response size: 0 bytes (no response)  
Sets up RGB color, defined in `PAYLOAD` to GPIO pins.

## LED_GET_CURRENT_COLOR
Request size: 50 bytes (`HEADER` + `HMAC` without `PAYLOAD`)  
Response size: 0 bytes (no response).  
Triggers `SYS_COLOR_CHANGED` call, which will send current color.  

## ANIM_SET_FADE
Request size: 55 bytes (`HEADER` + `HMAC` + `PAYLOAD`)  
Response size: 0 bytes (no response)  
Starts FADE animation. To `PAYLOAD` must be added `Speed` field as described at [PAYLOAD Structure](#payload-structure)

## ANIM_SET_PULSE
Request size: 55 bytes (`HEADER` + `HMAC` + `PAYLOAD`)  
Response size: 0 bytes (no response)  
Start PULSE animation. In `PAYLOAD` must be provided color fields and `Duration`.

## SYS_TOGGLE_SUSPEND
Request size: 55 bytes (`HEADER` + `HMAC` + `PAYLOAD`)  
Response size: 0 bytes (no response).  
Toggles Suspend mode.  
While suspended, it will turn off all lights and ignore all commands but `SYS_TOGGLE_SUSPEND`.
On suspend off will set given color from payload.

## SYS_COLOR_CHANGED
Request size: 0 bytes (no response).  
Response size: 55 bytes. (`HEADER` + `HMAC` + `PAYLOAD`)  
Sends info about new color to all clients.  

## Client Side Workflow
1. Generate Timestamp and Nonce  
    * Timestamp: Use Unix time (seconds since January 1, 1970). (64-bit)  
    * Nonce: Generate a random number to ensure each request is unique. (64-bit)
2. Prepare the Payload
    * The payload contains the actual data to be sent to the server, such as the color value.
3. Generate the HMAC
4. Pack package and send to server

## Server Side Workflow
1. Receive and Parse the Request
   * Receive the header, HMAC, and payload.
2. Verify Timestamp and Nonce
   * Ensure the timestamp is recent and the nonce hasn’t been used before.
3. Recompute the HMAC
   * Data to HMAC: Concatenate the header and payload.
   * HMAC Calculation: Recompute the HMAC using the shared secret.
   * Compare the recomputed HMAC with the received HMAC. If they match, proceed; if not, reject the request.
4. Process the Request
   * Process the color value from the payload.

## Requirements
* RPi with running `pigpiod`
* `libssl-dev`
* `libconfig`
* `libwebsockets` (optional, for WS server support for trusted networks)  
* `libmicrohttpd-dev` (optional, for HTML server support for trusted networks).
  
## Building and Running
* `git clone --recursive https://github.com/PolisanTheEasyNick/PiLED`
* `cd PiLED`
* `mkdir build`
* `cd build`
* `cmake -DWITH_HTML=ON -DWITH_WS=ON ..` (flags are optional.)
* `make`
* `./piled`  

You can also install piled to your system using:  
`sudo make install`  
which will copy executable to `${CMAKE_INSTALL_BINDIR}`, which often refers to `/usr/local/bin/piled` along with config file copy at `/etc/piled/piled.conf` and systemd service file at `/etc/systemd/system/piled.service`.  

## WebSockets  
For WebSockets support you need to install `libwebsockets` library.  
Unfortunately, on Raspbian it needs manual building:  
* `git clone https://libwebsockets.org/repo/libwebsockets`
* `cd libwebsockets`
* `mkdir build`
* `cd build`
* `cmake ..`
* `make`
* `sudo make install`
After this steps you can remake piled with `-DWITH_WS=ON` flag and it will be builed with WebSockets support.  

## HTML
For HTML server support, you need to install `libmicrohttpd-dev` package and rebuild with `-DWITH_HTML=ON` CMake flag.  

## Configuring
You can configure PiLED by editing config file /etc/piled/piled.conf or by copying him into ~/.config/piled.conf and editing at home dir.  
Note that systemd service is not running as any user so it may not find your home directory by $HOME.  
If you want OpenRGB device changing too, do not forget to define `OPENRGB_SERVER` at config file and run `openrgb_configurator` as described at [OpenRGB](#openrgb) section.  

## OpenRGB
PiLED supports connecting to OpenRGB server for setting current color to PC's controllers.  
For configuring OpenRGB you need to specify OpenRGB server IP and port.  
Then you need to run `openrgb_configurator` executable with root perms, which is built along with `piled`.  
Root is needed because configurator will create file with your picked PC's controllers at `/etc/piled/openrgb_config` (and later will be read by systemd service, for example).  
After OpenRGB is set, any requests for changing color to PiLED would be automatically retranslated to OpenRGB server and your PC will be in-sync with PiLED.  
