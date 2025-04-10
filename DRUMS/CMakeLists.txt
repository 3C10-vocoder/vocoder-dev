cmake_minimum_required(VERSION 3.13)

# Initialize the SDK
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(dma_audio C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable
add_executable(dma_audio
    main.c
)

# Enable USB serial output, disable UART output
pico_enable_stdio_usb(dma_audio 1)
pico_enable_stdio_uart(dma_audio 0)

# Link to libraries
target_link_libraries(dma_audio 
    pico_stdlib 
    hardware_dma 
    hardware_pwm
    hardware_pio
    hardware_gpio
    hardware_spi  # Added SPI library for MCP4922 DAC
)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dma_audio)