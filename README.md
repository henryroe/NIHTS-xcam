*xenics_pluto* 
==============

This repository was forked from a stripped down version of [Henry Roe's Xenics XEVA near-infrared camera control software](https://github.com/henryroe/xenics_pluto) originally built for Pluto occultation observations.

The goal of this fork is to try to replace xenics xeneth or x-control software.

Installation
============

see the original project at [github](https://github.com/henryroe/xenics_pluto)

Installation Notes for Windows
============

* use cygwin on windows as a shell.
* install packages: swig, libusb, g++, make, python, python-numpy, python-devel, libusb
* download libusb windows binaries from [sourceforge](https://sourceforge.net/projects/libusb-win32/files/libusb-win32-releases/1.2.6.0/)
* add the appropriate libusb filter/wrapper for the xenics camera via libusb-win32-devel-filter-1.2.6.0.exe



original Author
======
Henry Roe (hroe@hroe.me) 

License
=======
*xenics* is licensed under the MIT License, see ``LICENSE.txt``.
