DrvLoader
=========

A command line tool to load and unload a device driver. Supported driver types
are:
- Legacy (Windows NT driver model) device drivers
- Windows Driver Model (WDM) device drivers
- File system minifilter drivers

Other types of device drivers such as Windows Driver Frameworks (WDF) based ones
are not supported.

To compile , clone full source code from Github with a below command and build
it on Visual Studio.

    $ git clone --recursive https://github.com/tandasat/DrvLoader.git


Supported Platform(s)
-----------------
- Windows XP SP3 and later (x86/x64)


License
-----------------
This software is released under the MIT License, see LICENSE.
