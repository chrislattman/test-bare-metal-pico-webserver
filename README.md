# Raspberry Pi Pico 2 W web server with FreeRTOS + lwIP

This is essentially a bare metal version of https://github.com/chrislattman/webserver/blob/master/server.c

While this repo implements its own HTTP server, a popular embedded C web server is Mongoose.

Instructions:

- Run `git submodule update --init --recursive`
- Run `sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib`
- In `~/.bashrc` add
    ```
    export PICO_SDK_PATH=/path/to/test-bare-metal-pico-webserver/pico-sdk
    export FREERTOS_KERNEL_PATH=/path/to/test-bare-metal-pico-webserver/FreeRTOS-Kernel
    ```
- Follow the instructions at https://github.com/chrislattman/test-bare-metal-pico to install picotool if you haven't already
- You need to set the `WIFI_SSID` and `WIFI_PASSWORD` environment variables for this example to work

To generate Makefiles: `cmake -DPICO_BOARD=pico2_w -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-g++ -S . -B build`

To build application: `cmake --build build`

To run on board: 

- Unplug USB cable from board
- Hold down BOOTSEL button while plugging in USB cable
- Run `cp build/server.uf2 /media/$USER/RP2350` (flashes the board with the .uf2 file)
    - Alternatively, run `picotool load -u -v -x build/server.elf`
