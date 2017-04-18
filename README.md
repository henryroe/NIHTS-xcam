*NIHTS-xcam* 
==============

This package contains the basic control software for the Xenics XEVA near-infrared slit-viewing camera for NIHTS (Near-Infrared High-Throughput Spectrograph) on Lowell Observatory's Discovery Channel Telescope.

Installation
============

To compile the shared object file requires that Xcode and its command line tools be installed.

TK: update python install instructions, including:
    swig
    astropy

Install [homebrew](http://brew.sh/).

Install libusb and libusb-compat using homebrew:   

    brew install libusb
    brew install libusb-compat

To download & install this package:

    cd ~/   # assume we are doing in user's home dir
    git clone https://github.com/henryroe/NIHTS-xcam.git
    cd NIHTS-xcam/xenics
    make
    cd ..
    python setup.py develop

You will likely also want to install an image viewer.  I use [ztv](https://github.com/henryroe/ztv), which can be installed from [pypi](https://pypi.python.org/pypi/ztv) with:

    pip install ztv
    
Usage
=====

TK: update

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
*NIHTS-xcam* is licensed under the MIT License, see ``LICENSE.txt``. Basically, feel free to use any or all of this code in any way. But, no warranties, guarantees, etc etc..