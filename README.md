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