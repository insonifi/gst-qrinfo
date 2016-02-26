# gst-qrinfo
Element that superimposes QR coded metadata on the frame.

## Usage
```
gst-launch-1.0 videotestsrc ! qr ! xvimagesink
```
### Parameters
- `scale` &mdash; QR code scale factor
- `x` &mdash; X position offset
- `y` &mdash; Y position offset
- `string` &mdash; Custom string to be appended to metadata

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
