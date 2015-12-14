# gst-qrinfo
Element that superimposes QR coded metadata on the frame.

## Usage
```
gst-launch-1.0 videotestsrc ! qr ! xvimagesink
```
### Parameters
- `scale` -- QR code scale factor
- `x` -- X position offset
- `y` -- Y position offset

## Compiling
All you need is run:
```
./autogen.sh
make
```
### Installing
- either `make install`
- or copy it to `~/.local/share/gstreamer-1.0/plugins`
### Dependecies
- gstreamer >=1.0
- libqrencode >= 3.0

# Credits
Thanks to great GStreamer project and [gst-template](http://cgit.freedesktop.org/gstreamer/gst-template/).
