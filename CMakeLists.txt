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

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Import SD card project
add_subdirectory("${SD_SPI_PROJECT_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/FatFs_SPI")

# Build the bootloader as a standalone thing

add_executable(bootloader main.c sd_hw_config.c)

function(target_cl_options option)
    target_compile_options(bootloader PRIVATE ${option})
    target_link_options(bootloader PRIVATE ${option})
endfunction()

target_cl_options("-Os")
target_cl_options("-ffunction-sections")
target_cl_options("-fdata-sections")
target_link_options(bootloader PRIVATE "LINKER:--gc-sections")

pico_add_extra_outputs(bootloader)
pico_set_binary_type(bootloader copy_to_ram)

# The sd driver takes control of the LED unless NO_PICO_LED is defined
target_compile_definitions(bootloader PUBLIC
  NO_PICO_LED
)

set_target_properties(bootloader PROPERTIES COMPILE_FLAGS "-Wall")

pico_set_linker_script(bootloader ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.ld)

target_link_libraries(bootloader
                      pico_stdlib
					  #pico_stdio_usb
                      hardware_dma
                      hardware_flash
                      hardware_structs
                      hardware_resets
                      cmsis_core
					  boot_uf2_headers
					  FatFs_SPI)

set(BOOTLOADER_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

# Build a library to embed into applications

function(bootloader_define_library)
	set(NAME bootloader)
	set(ORIGINAL_BIN ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.bin)
	set(BIN_ASM ${CMAKE_CURRENT_BINARY_DIR}/${NAME}_bin.S)

	add_custom_target(${NAME}_bin DEPENDS ${ORIGINAL_BIN})
	add_custom_command(OUTPUT ${ORIGINAL_BIN} DEPENDS ${NAME} COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${NAME}> ${ORIGINAL_BIN})

	find_package (Python3 REQUIRED COMPONENTS Interpreter)
	add_custom_target(${NAME}_bin_asm DEPENDS ${BIN_ASM})
	add_custom_command(OUTPUT ${BIN_ASM} DEPENDS ${ORIGINAL_BIN}
		COMMAND ${Python3_EXECUTABLE} ${BOOTLOADER_DIR}/mkasm.py ${ORIGINAL_BIN} ${BIN_ASM}
	)

	add_library(${NAME}_library INTERFACE)
	add_dependencies(${NAME}_library ${NAME}_bin_asm)
	# not strictly (or indeed actually) a link library, but this avoids dependency cycle
	target_link_libraries(${NAME}_library INTERFACE ${BIN_ASM})
endfunction()

bootloader_define_library()

# Provide a helper to build a standalone target

function(bootloader_build_standalone NAME)
	set(APP_HDR_NAME ${NAME}_hdr_standalone)
	set(APP_HDR_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_HDR_NAME}.bin)
	set(APP_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.bin)
	set(APP_WHEADER_NAME ${NAME}_w_header)
	set(APP_WHEADER_ELF_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.elf)
	set(APP_WHEADER_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.bin)
	set(APP_WHEADER_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.uf2)

	pico_set_linker_script(${NAME} ${BOOTLOADER_DIR}/standalone.ld)
	pico_add_bin_output(${NAME})

	add_custom_target(${APP_HDR_NAME} DEPENDS ${NAME})
	add_custom_command(TARGET ${APP_HDR_NAME} DEPENDS ${APP_BIN_PATH}
		COMMAND ${BOOTLOADER_DIR}/gen_imghdr.py -a 0x1000E000 ${APP_BIN_PATH} ${APP_HDR_PATH}
	)

	add_custom_target(${APP_WHEADER_NAME} ALL DEPENDS ${APP_HDR_NAME})
	add_custom_command(TARGET ${APP_WHEADER_NAME} DEPENDS ${APP_HDR_PATH}
		COMMAND ${CMAKE_OBJCOPY} --update-section .app_hdr=${APP_HDR_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.elf ${APP_WHEADER_ELF_PATH}
	)
	add_custom_command(TARGET ${APP_WHEADER_NAME} DEPENDS ${APP_WHEADER_ELF_PATH}
		COMMAND ${CMAKE_OBJCOPY} -O binary ${APP_WHEADER_ELF_PATH} ${APP_WHEADER_BIN_PATH}
	)
	add_custom_command(TARGET ${APP_WHEADER_NAME} DEPENDS ${APP_WHEADER_ELF_PATH} elf2uf2/elf2uf2
		COMMAND elf2uf2/elf2uf2 ${APP_WHEADER_ELF_PATH} ${APP_WHEADER_UF2_PATH}
	)
endfunction()

# Provide a helper to build a combined target

function(bootloader_build_combined BTL_NAME APP_NAME)
	set(BTL_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${BTL_NAME}.uf2)
	set(APP_WHEADER_NAME ${APP_NAME}_w_header)
	set(APP_WHEADER_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.uf2)

	bootloader_build_standalone(${APP_NAME})

	# Simply concatenate the two uf2 files which *should* both be self-contained
	set(COMBINED_NAME ${BTL_NAME}_${APP_NAME}_combined)
	set(COMBINED_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${COMBINED_NAME}.uf2)
	add_custom_target(${COMBINED_NAME} ALL
		COMMENT "Building full UF2 image"
		COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/uf2tool.py
				-o ${COMBINED_UF2_PATH} ${BTL_UF2_PATH} ${APP_WHEADER_UF2_PATH}
	 	DEPENDS ${BTL_NAME} ${APP_WHEADER_NAME})

endfunction()


# Embed version info into the uf2 file
pico_set_program_name(bootloader "Bootloader")
pico_set_program_version(bootloader "1.0")
# Prints how much memory used at end of build output
# (RAM usage only shows global, preallocated mem and nothing else)
set_property(TARGET bootloader APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")





add_executable(blink_noboot2 blink.c)

pico_add_extra_outputs(blink_noboot2)

set_target_properties(blink_noboot2 PROPERTIES COMPILE_FLAGS "-Wall")

#bootloader_build_standalone(blink_noboot2)
bootloader_build_combined(bootloader blink_noboot2)

target_link_libraries(blink_noboot2 pico_stdlib_headers pico_stdlib hardware_watchdog hardware_structs)
