# Derived from the Pico SDK, which carries the following
# LICENSE.txt:
# Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
#    disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_VERBOSE_MAKEFILE ON)

# Have pico_sdk_import.cmake use the local SDK if it was pulled down; otherwise, use path at
# PICO_SDK_PATH environment variable as specified within pico_sdk_import.cmake
if (EXISTS "${CMAKE_SOURCE_DIR}/ext/pico-sdk/CMakeLists.txt")
    set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/ext/pico-sdk/" CACHE INTERNAL PICO_SDK_PATH)
    message("Using local pico SDK at ('${PICO_SDK_PATH}')")
endif ()

if (NOT DEFINED PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH not defined; either execute:\ngit submodule update --recursive --init\nor set PICO_SDK_PATH in environment")
endif ()

if (EXISTS "${CMAKE_SOURCE_DIR}/ext/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI/CMakeLists.txt")
    set(SD_SPI_PROJECT_PATH "${CMAKE_SOURCE_DIR}/ext/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI" CACHE INTERNAL SD_SPI_PROJECT_PATH)
    message("Using local FatFs_SPI at ('${SD_SPI_PROJECT_PATH}')")
endif ()

if (NOT DEFINED SD_SPI_PROJECT_PATH AND NOT DEFINED ENV{SD_SPI_PROJECT_PATH})
    message(FATAL_ERROR "SD_SPI_PROJECT_PATH not defined; either execute:\ngit submodule update --recursive --init\nor set SD_SPI_PROJECT_PATH in environment")
endif ()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(test_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Import SD card project
add_subdirectory("${SD_SPI_PROJECT_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/FatFs_SPI")

add_subdirectory(src)
