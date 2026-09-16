# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# The packaged Pico SDK omits git submodules. Populate the exact BTstack
# revision referenced by Pico SDK 2.3.1 before pico_sdk_init() discovers it.
include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Declare(
    btstack
    GIT_REPOSITORY https://github.com/bluekitchen/btstack.git
    GIT_TAG eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec
)
FetchContent_GetProperties(btstack)
if(NOT btstack_POPULATED)
    FetchContent_Populate(btstack)
endif()
set(PICO_BTSTACK_PATH ${btstack_SOURCE_DIR} CACHE PATH "BTstack source path" FORCE)

project(PicoMonsterEyes C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(PicoMonsterEyes
    main.cpp
        
    # Drivers
    drivers/ssd1351_display.cpp
    drivers/max98357a_i2s_output.cpp
    drivers/spi_bus.cpp
        
    # Src
    src/app.cpp
    src/bluetooth_audio_source.cpp
    src/display_manager.cpp
    src/eye_renderer.cpp
    # Assets
    assets/graphics/default_eye.cpp
)

pico_set_program_name(PicoMonsterEyes "PicoMonsterEyes")
pico_set_program_version(PicoMonsterEyes "0.1")

pico_generate_pio_header(PicoMonsterEyes ${CMAKE_CURRENT_LIST_DIR}/drivers/audio_i2s.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(PicoMonsterEyes 0)
pico_enable_stdio_usb(PicoMonsterEyes 1)

# Add the standard library to the build
target_link_libraries(PicoMonsterEyes
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_cyw43
    pico_btstack_classic
    pico_btstack_sbc_decoder
    pico_multicore
)

target_sources(PicoMonsterEyes PRIVATE
    ${PICO_BTSTACK_PATH}/example/a2dp_sink_demo.c
)

# Add the standard include files to the build
target_include_directories(PicoMonsterEyes PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/drivers
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/boards
    ${CMAKE_CURRENT_LIST_DIR}/assets/graphics
)

# Add any user requested libraries
target_link_libraries(PicoMonsterEyes 
        hardware_spi
        hardware_i2c
    hardware_dma
    hardware_pio
    hardware_irq
    hardware_clocks
        )

pico_add_extra_outputs(PicoMonsterEyes)
# (touched to force rebuild after asset accessor refactor)
# End of file

