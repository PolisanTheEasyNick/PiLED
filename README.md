# led-server

This project used for installing on Raspberry Pi.  
To GPIO pins connected LED strip.  
This program opens TCP server on port 3384 and waits for packets, created in this protocol:

LED PROTOCOL v1.0
Simply contains HEADER + HMAC-SHA-256 + PAYLOAD  
Note that config lines are hardcoded into `utils.h`

## HEADER Structure
```
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|                                        HEADER                                     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+   
|timestamp: 64-bit signed integer; 8 bytes                                          |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+   
|nonce:     64-bit signed integer, random; 8 byte value                             |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+   
|version:   8-bit signed int, 1 byte version of protocol. ex. 0001 for version 1.0  |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+ 
```  

## HMAC-SHA-256 (32 bytes)
Must be generated with SHA-256 algorithm with using shared_secret which writed in both client and server and data must be used header+payload:
HMAC(SHA-256, shared_secret, header + payload)

## PAYLOAD Structure
```
+--+--+--+--+--+--+--+--+--+-+
|          PAYLOAD           |
+--+--+--+--+--+--+--+--+--+-+
|RED color:   8 bits, 1 byte |
+--+--+--+--+--+--+--+--+--+-+
|GREEN color: 8 bits, 1 byte |
+--+--+--+--+--+--+--+--+--+-+
|BLUE color:  8 bits, 1 byte |
+--+--+--+--+--+--+--+--+--+-+
```
Currently max buffer size: 8+8+1+32+1+1+1 = 52 bytes.
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
