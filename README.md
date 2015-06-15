*xenics_pluto* 
==============

This package contains a stripped down version of Henry Roe's Xenics XEVA near-infrared camera control software for Pluto occultation observations.

Installation
============

To compile the shared object file requires that Xcode and its command line tools be installed.

This version of xenics software expects a copy of Enthought's Canopy python installation to be installed and available as the default python (or at least be the default python in the terminal session from which you want to run xenics.)

Using Canopy Package Manager install:
- astropy
- swig

Install [homebrew](http://brew.sh/).

Install libusb and libusb-compat using homebrew:   

    brew install libusb
    brew install libusb-compat

To download & install this package:

    cd ~/   # assume we are doing in user's home dir
    git clone https://github.com/henryroe/xenics_pluto.git
    cd xenics_pluto/xenics
    make
    cd ..
    python setup.py develop
    

Usage
=====

In python (preferably ipython to give you niceties like tab completion), bring the camera up and run a sequence

    from xenics import XenicsCamera
    x = XenicsCamera()
    x.set_gain(False)
    x.go(0.2, 1, 100, save_every_Nth_to_currentfits=10)
    # etc....  take more sequences, whatever
    # then, to shutdown:
    x.close_camera()
    

Author
======
Henry Roe (hroe@hroe.me) 

License
=======
*xenics_pluto* is licensed under the MIT License, see ``LICENSE.txt``. Basically, feel free to use any or all of this code in any way. But, no warranties, guarantees, etc etc..