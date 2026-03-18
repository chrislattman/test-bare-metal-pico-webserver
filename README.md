# Raspberry Pi Pico 2 W web server with FreeRTOS + lwIP

This is essentially a bare metal version of https://github.com/chrislattman/webserver/blob/master/server.c

While this repo implements its own HTTP server, a popular embedded C web server is Mongoose.

- For Rust, `picoserve` is the most popular `no_std` Rust HTTP server

Instructions:

- Clone https://github.com/raspberrypi/pico-sdk
- In `~/.bashrc` add `export PICO_SDK_PATH=/path/to/pico-sdk`
- In the `pico-sdk` folder run `git submodule update --init`
- Run `sudo apt install build-essential cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib`
- Clone https://github.com/raspberrypi/picotool
- Run `sudo apt install pkg-config libusb-1.0-0-dev`
- In `~/.bashrc` add `export PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig/"`
- In the `picotool` folder run 
    ```
    cmake -S . -B build
    cmake --build build
    sudo cmake --install build
    sudo cp udev/60-picotool.rules /etc/udev/rules.d/
    sudo udevadm control --reload
    ```
- Clone https://github.com/raspberrypi/FreeRTOS-Kernel
- In `~/.bashrc` add `export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel`
- You need to set the `WIFI_SSID` and `WIFI_PASSWORD` environment variables for this example to work

To generate Makefiles: `cmake -DPICO_BOARD=pico2_w -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-g++ -S . -B build`

To build application: `cmake --build build`

To run on board: 

- Unplug USB cable from board
- Hold down BOOTSEL button while plugging in USB cable
- Run `cp build/server.uf2 /media/$USER/RP2350` (flashes the board with the .uf2 file)
    - Alternatively, run `picotool load -u -v -x build/server.elf`
