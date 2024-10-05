# Flir One for Linux v4l2

## What it is

This is an enhanced and cleaned version forked from  https://github.com/fnoop/flirone-v4l2.

That is a cleaned up version of code posted here: http://www.eevblog.com/forum/thermal-imaging/question-about-flir-one-for-android/.

All credit goes to tomas123, cynfab etc from that forum who did the awesome research and work to make this support.

## Changes from the original

This version has several differences from the original:

* a raw stream of the image thermal data with all his 16 bit (65535 levels) encoded as a RBG stream (with B channel as the MSB and the G channel as the LSB), for image processing purposes.

* a slightly better error treatment:
  * it does not gives you segmentation fault if the device is not ready, connected or turned on. Now the program waits for the device (or optionally exits with a clear error message).
  * it does not gives you segmentation fault if the `v4lloopback` is not installed, but a more clear error message;
  * if the cpalette file is inexistent or inaccessible you don't get a segmentation fault anymore.

* it has command line options for:
  * turn off the crosshairs, and temperature at the bottom of image;
  * turn off the waiting for the device, exiting with an error.

* the code was made more clear:
  * the code was put on a more consistent and clear formatting standard;
  * each action was separated in smaller functions;
  * some of of those actions were rewrote to have single responsibilities, so the name of the function is not misleading;
  * the hard coded sizes of the image were substituted by constants.

## What it does

This program access the FLIR ONE and FLIR ONE Pro thermal cameras.

It produces three video streams: 

* `/dev/video1`: the raw thermal image stream encoded as a RGB image with the 16 bit gray scale encoded in B (MSB) and G (LSB) bytes;
* `/dev/video2`: the visible camera;
* `/dev/video3`: the thermal image with the thermal values rescaled to 8 bit gray scaled and colored with a 256 colors palette

## Dependencies

Some dependencies needed to compile it (may be more):

* `libc6-dev`;
* `libusb-1.0-0-dev`;
* `libjpeg-dev`.

Dependency needed to use this program: 

* the kernel module https://github.com/afgranero/v4l2loopback (forked from  https://github.com/umlaeute/v4l2loopback).

## How to compile

Once you have the dependencies set up, you can compile by using the command:

```
make all
```

## How to run

### Install kernel module

```
sudo insmod ./v4l2loopback.ko devices=3
```

or

```
sudo modprobe ./v4l2loopback.ko devices=3
```

#### Run it

This is an example of how to run it:

```
sudo ./flirone ./palettes/Iron2.raw
```

The file `Iron2.raw` is a palette file that specifies the color palette to be used by the thermal image on `/dev/video3`. You can experiment with other palettes are available on `./palettes` folder.

#### Command line switches

This version has a few more options than the original.
The syntax is

```
flirone [--dontwaitdevice] [--nooverlays] palettefile.raw
```

`--dontwaitdevice` turns off the default behavior of waiting for the device to be connected turned on and ready, even if the device is disconnected or turned off during streaming. Instead it exits immediately with an error.

`--nooverlays` turns off the image overlays of the colored image on `/dev/video3`: the cross-hairs and the temperature bar at the bottom of the image (reduces the image 8 pixels in height used for that bar).

## How to see and use the video streams

With this module you will can use it for instance:

* with VLC connected to one of the capture devices: `/dev/video1`, `/dev/video2` and `/dev/video3` described above;
* with Python using OpenCV, as described in this example: https://opencv-python-tutroals.readthedocs.io/en/latest/py_tutorials/py_gui/py_video_display/py_video_display.html just changing the device index.

## Some working details

### Color palette files

The color palette files on folder `./palette` are always 768 bytes long.
Each group of 3 bytes are the RGB values for each of the ordered 256 levels of the grayscale thermal image to indicate temperature.

Those colors are not absolute, as the image levels are dynamically rescaled as explained below.

### Image rescaling

The raw thermal on `/dev/video1` has 16 bits (65536 gray levels) encoded on a RGB video stream with B as the MSB and G as the LSB.
This stream is useful for video processing the raw image. 

A typical scene don't span the whole range of temperatures that can be measured with the FLIR ONE PRO (-20°C to 400°C). For instance a typical room image with electrical equipment has about 1.5% this range (about 1000 levels wide), and a typical face image 0.6% to 0.7% this range (about 400 to 500 levels wide).

With such a narrow range a raw image even color coded the fine details of thermal variation can not be perceived by human eye.

For visualizing purposes the image stream on `/dev/video3` is dynamically rescaled at each frame, allowing to see slight temperature differences on image.

At each frame the lower level is the minimal temperature found on image, and the higher one the maximum temperature found. This range is rescaled to 8 but (256 levels) encoded to colors according the palette chosen.

This enables a good visualization of the thermal image, but turns it useless for image processing (at least without a temperature reference spot on the image) as the levels are relative to the scene and not absolute (that is why we provided the) raw thermal stream on `/dev/video1`.

## Image overlays

The image stream on `/dev/video3` have some features overlayed to the image:
* a center image indicator for indicating the spot where temperature indicated on the status bar is being measured
* a temperature status bar indicating:
  * the minimum temperature found on image;
  * the temperature at the center image indicator (taken as an average of four pixels around the center);
  * the maximum temperature found on image
* a pair of crosshairs on vertical `|` and other horizontal `<` to indicate the hottest point on the image.

Those overlays can be turned off by using the command line switch `--nooverlays` described above.

## FFC calibration

The FLIR ONE Pro has an automatic calibration.
When the scene has big variations of temperature a FFC calibration may occur.

When this happens the shutter closes and some frames labeled as FFC (for flat field correction) are sent. This makes the image to be interrupted for about 1 to 2 seconds.

This process is more accurately described as dark field correction than flat field correction.

Those frames are eliminated from the stream, but they can be in the future used to correct the different of sensitivity for each pixel of the sensor issue #20). 
