# WiFi Broadcast FPV client for Windows platform.


fpv-wfb is an app for windows packaging multiple pieces together to decode an H264/H265 video feed broadcast by wfb-ng over the air.

- [devourer](https://github.com/openipc/devourer): userspace rtl8812au driver initially created by [buldo](https://github.com/buldo) and converted to C by [josephnef](https://github.com/josephnef) .
- [wfb-ng](https://github.com/svpcom/wfb-ng): library allowing the broadcast of the video feed over the air.
 
Supported rtl8812au wifi adapter only 

![img.png](img/img.png)

### Usage
- download [Zadig](https://github.com/pbatard/libwdi/releases/download/v1.5.0/zadig-2.8.exe)
- Repair libusb driver( maybe need enable ```[options]->[list all devices]``` to show your adapter)

![img.png](img/img1.png)

- select you 8812au adapter
- select your wfb key
- select your drone channel
- enjoy

### Delay test

![img.png](img/delay.jpg)

### Todo
- OSD
- Hardware acceleration decoding
- Record MP4 file
- Capture frame to JPG
- Streaming to RTMP/RTSP/SRT/WHIP server
- Receive multiple video streams using a single adapter
- Onvif/GB28181/SIP client

### How to build
- take a look for 
[GithubAction](https://github.com/TalusL/fpv-wfb/blob/main/.github/workflows/msbuild.yml)
