imagetolaser
============

I use the imagecarve program and mapped the Z axis to the S value for the laser.

This is a fork from https://code.google.com/p/imagecarve/

Here is the readme file for imagecarve:
This program is for early adoptors running on linux only at the moment.

There's nothing hugely linux specific (hey, it may even compile in a
cygwin environment), but I just haven't tested it.

To compile:
  make

To use it: (assuming you have netpbm installed)

  cat image.png | pngtopnm | ppmtopgm |
    ./imagecarve --toolsize=3.2 --stepover=1 --scale=0.25 --clear=12 > output.g

Reads as "Cutting with a 3.2mm tool, moving 1mm across for each row, each pixel
is 0.25mm wide, and the clearance height is 12mm"

All units are assumed to be in millimeters.
The cutter is assumed to be a ball-nose cutter.
No feed value is specified in the output G-code. It's assumed
that you'll edit it to make it readable by your machine..

For no good reason, it is assumed that 0 (black) in the image is the maximum cut depth.
It's assumed that Z=0 is the top surface of the work material.
