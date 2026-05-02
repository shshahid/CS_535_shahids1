# CS 535 (Computer Graphics)
***Spring 2026***  
***Author: Shazman Shahid***  
***Original Author: Dr. Michael J. Reale***  
***SUNY Polytechnic Institute*** 

## Software Dependencies
You will need to install the following manually:
- C++ compilers
- CMake
- Visual Code
- Vulkan SDK

The rest of the dependencies will be automatically fetched by CMake.

## Applications

### VulkanStart
This application should show a multi-color quad on the screen with a cyan background.

### Assign01
This application shows a multi-color quad with an animated background. There are two implementations:
1) A rainbow animation using sine waves for R, G, and B values (0.0 to 1.0). (default when running program)
2) A basic swap between two colors every 2 seconds, which are defined right before the main render loop. (commented out)

### Assign02
This application shows a variable polygon with an animated background. The polygon generated depends on the maxSub parameter, which when large enough just generates a circle.  
The createShape function takes in 2 parameters: maxSub (equal to number of triangles generated; default = 10) and flipWinding (CCW when false, CW when true; default = false). The function requires maxSub to be at least 3, values less than 3 (i.e. 1 and 2) do not work well due to the usage of cos and sin in generation.

### Assign03
This application shows either the sphere.obj or bunnyteatime.glb based on command line arguments. Different transformations, including Z rotation, Z offset, and useNormalAsColor value, are controlled by keyboard presses with a key for each increase (+) and decrease (-) as follows:  
rotAngleZ: J +, K -  
zOffset: U +, Y -  
useNormalAsColor: O +, I -

### Assign 04
This application shows the models with a ring of 4 lights with a moveable camera using the mouse and keyboard. The mouse moves the camera. Keyboard presses move the camera, change the light colors, and the amount of lights as follows:  
W: forward, S: backward, D: right, A: left  
1: light colors all white, 2: light colors all red, 3: light colors all green, 4: light colors all blue  
X: increment light count, Z: decrement light count