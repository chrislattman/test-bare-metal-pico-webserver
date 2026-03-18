cmake_minimum_required(VERSION 3.27)

include(pico_sdk_import.cmake)

project(test-bare-metal-pico-webserver)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(WIFI_SSID $ENV{WIFI_SSID})
set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})

include(FreeRTOS_Kernel_import.cmake)

add_executable(server server.c)
target_compile_definitions(server PRIVATE 
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    NO_SYS=0
    LWIP_SOCKET=1
)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(server
    pico_cyw43_arch_lwip_sys_freertos
    pico_stdlib
    FreeRTOS-Kernel-Heap4
    )

pico_add_extra_outputs(server)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()
