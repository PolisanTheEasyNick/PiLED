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
- [ ] Get current color support in protocol
- [ ] OpenRGB SDK support (connect to server and set colors too)  



LED PROTOCOL v2
Simply contains `HEADER` + `HMAC-SHA-256` + `PAYLOAD`  
Currently max buffer size: 8+8+1+32+1+1+1+1 = 53 bytes.

## Version History
| Version    | Description                                    |
| :--------: | ---------------------------------------------- |
| v1, 0x1    | Initial release, support of changing color     |
| v2, 0x2    | Added plain changing from current color to new |



## HEADER Structure
| Name      | Size                    | Description               |
| :-----:   | :---------------------: | ------------------------- |
| timestamp | 8 bytes, 64 bit, signed | Timestamp of current time |
| nonce     | 8 bytes, 64 bit, signed | Random data               |
| version   | 1 byte, 8 bit, signed   | Used version of protocol  |


## HMAC-SHA-256 (32 bytes)
Must be generated with SHA-256 algorithm with using `shared_secret` which writed in both client and server and `data` must be used `header+payload`:
`HMAC(SHA-256, shared_secret, header + payload)`

## PAYLOAD Structure
| Name        | Size                    | Description                                               |
| :-----:     | :---------------------: | --------------------------------------------------------- |
| RED color   | 1 byte, 8 bits, signed  | RED color value                                           |
| GREEN color | 1 byte, 8 bits, signed  | GREEN color value                                         |
| BLUE color  | 1 byte, 8 bits, signed  | BLUE color value                                          |
| Duration    | 1 byte, 8 bits, signed  | Duration in seconds of changing color from current to new |

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
