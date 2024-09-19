Matthew Cloutier

This is a project I have been working on by myself outisde of academics.
With this project I am building a server that can run poker games and keep track of earnings by each player across games.

This was made on a raspberry and currently runs but is in an uncompleted state.

# Mongoose Wizard Project

This project is generated by Mongoose Wizard, https://mongoose.ws/wizard/

## Build and flash

Step 1. Click on "sync" button to syncronize (download) project files to your
        local directory. First time, a popup window will ask you for the
        destination folder. Next time you click, sync happens without asking.
        Alternatively, you can click on .zip button, download and unzip.

Step 2. Setup your build environment - follow instructions at
        https://mongoose.ws/documentation/tutorials/tools/ 
        Note for Windows: do not skip instructions even if you think you could

Step 3. If the target IDE is Cube/Keil/MCUXpresso/Arduino, open destination
        folder in the respective IDE, build and flash.
        If the target IDE is GCC+make, open terminal/command prompt,
        go to the destination directory, and type `make`


## Copying functionality to an existing firmware

In order to move the functionality to some existing firmware code, copy only
the following ("purple") files:

- mongoose.{c,h}       - This is Mongoose Library, a full TCP/IP + TLS stack
- mongoose_custom.h    - (only if present). This is Mongoose settings file
- mongoose_fs.c        - Embedded filesystem. Contains Web UI files and TLS certs
- mongoose_impl.c      - Generated network functionality
- mongoose_glue.{c,h}  - A "glue", or "binding" functions for your code

Note, the only file you should edit to integrate with your code, is
`mongoose_glue.c`. That file "glues" your firmware with the networking
functionality generated by the Mongoose Wizard. It contains functions that
return device data. Generated functions use "fake" mock data; you need to
edit those functions and return "real" device data.

When you copy the files, add the following snippet somewhere in your code:

```c
#include "mongoose_glue.h"

...
run_mongoose(); // This function blocks forever. Call it at the end of main(),
                // or in a separate RTOS task. Give that task 8k stack space.
```
