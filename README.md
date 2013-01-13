
xf86-networktablet
==================

X.org/XF86 input driver: virtual tablet (controlled by network)

*Caution: This project is a proof-of-concept yet. Code quality is not
mature, there are no security features etc.*


What does it do?
================

xf86-networktablet is an input driver for Xorg that creates a virtual tablet
(absolute pointing device). The tablet can be controlled via UDP so that every network application can
simulate movements and button clicks.

The virtual tablet has 3 axes (x, y, pressure) and 1 button.


Installation
============

Tested with Ubuntu 11.10 and 12.10 (64-bit)

* Install dependencies (X.org development files)
* Adapt Makefile to your paths
* Adapt networktablet.c to your needs. At the moment, there is a hardcoded tablet resolution of 1280x800.
* make
* sudo make install
* Add the input device to xorg.conf. Minimal configuration:
``` 
    Section "ServerLayout"
        Identifier     "DefaultLayout"
        InputDevice    "NetworkTablet0"
    EndSection
    
    Section "InputDevice"
        Identifier     "NetworkTablet0"
        Driver         "networktablet"
    EndSection
``` 
* Restart X server. The X.org logs should show the networktablet device. `xinput list`
  should show the network tablet device.


Usage
=====

Once the driver is running, it *listens on 0.0.0.0:40117 for UDP packets*.
Any application in the network can send UDP packets to this port and control
the virtual tablet.

The structure of the packets is defined in `protocol.h`.

This input driver is used by the XorgTablet android application which enables
you to use any Android device (especially these with stylus pen) as
an X.org graphics tablet.
