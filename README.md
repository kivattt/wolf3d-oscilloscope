Don't bother trying to run this yourself! Beware of hardcoded values...

https://github.com/user-attachments/assets/bd838dee-f011-4207-867a-1fc238e67f30

Only works on X11, it screenshots constantly and outputs an amplitude-modulated sine wave with an initial trigger as sound suitable for an oscilloscope.

I run Wolfenstein 3D with dosbox on Linux Mint Cinnamon, where the window opens in the middle of my 1920x1080 screen at 480x240 resolution for the game area.

## How does it work?
I split the screen into vertical slices and find the height of the wall at every vertical slice.\
Since the floors and ceilings of Wolfenstein 3D are a flat color, I simply count from the top and bottom of the game screen until there is no more floor/ceiling color.

I then choose the smallest of these two values to use for the final output.

One "frame" consists of a high amplitude (negative) trigger signal, followed by a high-frequency sine wave which is amplitude-modulated based on the vertical slices mentioned before.

## Bugs
- Ceiling lamps are incorrectly thought to be the start of a wall, making them appear as large objects
- Same problem with enemies, and also the gun in the middle of the screen (though this is mitigated by choosing the smallest of the top and bottom "scans")
