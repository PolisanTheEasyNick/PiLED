# led-server

This project used for installing on Raspberry Pi.  
LED Strip is connected to Raspberry Pi GPIO pins.  
This program opens TCP server on port 3384 and waits for plain TCP packets, created as described here:

## TODO:
- [x] Basic color change
- [x] Protocol parser with all needed checks
- [x] Setting up pigpio and setting color to pins
- [x] Use config file
- [ ] Some basic animations support  
- [x] Get current color support in protocol
- [ ] OpenRGB SDK support (connect to server and set colors too)  



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
| Name      | Size              | Version | Description              |
| :-------: | :---------------: | :-----: | ------------------------ |
| timestamp | 8 bytes, unsigned |    1    |Timestamp of current time |
| nonce     | 8 bytes, unsigned |    1    |Random data               |
| version   | 1 byte, unsigned  |    1    |Used version of protocol  |
| OP        | 1 byte, unsigned  |    3    |Operational code          |


## HMAC-SHA-256 (32 bytes)
Must be generated with SHA-256 algorithm with using `shared_secret` which writed in both client and server and `data` must be used `header+payload`:
`HMAC(SHA-256, shared_secret, header + payload)`

## Operational Codes
| Value | Name                                            | Description                           |
| :---- | :---------------------------------------------: | ------------------------------------- |
| 0     | [LED_SET_COLOR](#led_set_color)                 | Set RGB color, described in PAYLOAD   |
| 1     | [LED_GET_CURRENT_COLOR](#led_get_current_color) | Request led-server current LEDs color |
| 2     | [ANIM_SET_FADE](#anim_set_fade) | Request led-server current LEDs color |


## PAYLOAD Structure
| Name        | Size                      | Version | Description                                               |
| :---------: | :-----------------------: | :-----: |---------------------------------------------------------- |
| RED color   | 1 byte, 8 bits, unsigned  |    1    | RED color value                                           |
| GREEN color | 1 byte, 8 bits, unsigned  |    1    | GREEN color value                                         |
| BLUE color  | 1 byte, 8 bits, unsigned  |    1    | BLUE color value                                          |
| Duration    | 1 byte, 8 bits, unsigned  |    2    | Duration in seconds of changing color from current to new |
| Speed       | 1 byte, 8 bits, unsigned  |    4    | Speed of animation, from 0 to 255, conv. units            |

## Packet-Specific Documentation

## LED_SET_COLOR
Request size: 55 bytes (`HEADER` + `HMAC` + `PAYLOAD`)  
Response size: 0 bytes (no response)  
Sets up RGB color, defined in `PAYLOAD` to GPIO pins.

## LED_GET_CURRENT_COLOR
Request size: 50 bytes (`HEADER` + `HMAC` without `PAYLOAD`)  
Response size: 11 bytes  
The response contains `timestamp` when response package created (8 bytes) and RGB values (3 bytes summary respectively).  

## ANIM_SET_FADE
Request size: 55 bytes (`HEADER` + `HMAC` + `PAYLOAD`)  
Response size: 0 bytes (no response)  
Starts FADE animation. To `PAYLOAD` must be added `Speed` field as described at [PAYLOAD Structure](#payload-structure)


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
