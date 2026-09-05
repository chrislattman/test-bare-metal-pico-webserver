# Raspberry Pi Pico 2 W web server with FreeRTOS + lwIP

This is essentially a bare metal version of https://github.com/chrislattman/webserver/blob/master/server.c

While this repo implements its own HTTP server, a popular embedded C web server is Mongoose.

Instructions:

- Run `git submodule update --init --recursive && git config submodule.pico-sdk.ignore all`
- Run `sed -i "s/-DPICOTOOL_NO_LIBUSB=1/-DPICOTOOL_NO_LIBUSB=0/g" pico-sdk/tools/Findpicotool.cmake`
- Run `sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib`
- You need to set the `WIFI_SSID` and `WIFI_PASSWORD` environment variables for this example to work

To generate Makefiles:

```
PICO_SDK_PATH=$(git rev-parse --show-toplevel)/pico-sdk \
FREERTOS_KERNEL_PATH=$(git rev-parse --show-toplevel)/FreeRTOS-Kernel \
cmake -DPICO_BOARD=pico2_w \
    -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-gcc \
    -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-g++ \
    -DPICOTOOL_FORCE_FETCH_FROM_GIT=ON \
    -S . -B build
```

To build application and run on board: 

- Unplug USB cable from board
- Hold down BOOTSEL button while plugging in USB cable
- Run `cmake --build build --target run`
