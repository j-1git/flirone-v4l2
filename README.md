### Flir One for Linux v4l2

This is a cleaned up version of code posted here:
http://www.eevblog.com/forum/thermal-imaging/question-about-flir-one-for-android/

All credit goes to tomas123, cynfab etc from that forum who did the awesome research and work to make this support.

#### What it does

This program acess the FLIR ONE and FLIR ONE Pro thermal cameras.
It produces two video streams: one of the visible camera and other for the thermal image.
To see the image you will need ther kernel module https://github.com/afgranero/v4l2loopback (forked from  https://github.com/umlaeute/v4l2loopback).
With this module you will can use it for instance:
* with VLC on =/dev/video2= for visible image, and =/dev/video2= for the thermal one;
* with Python using OpenCV, as described in this example: https://opencv-python-tutroals.readthedocs.io/en/latest/py_tutorials/py_gui/py_video_display/py_video_display.html just changing the device index.


#### Dependencies

Some dependencies needed to compile it (may be more):

* `libc6-dev`;
* `libusb-1.0-0-dev`;
* `libjpeg-dev`.
