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

target_link_directories(bootloader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
pico_set_linker_script(bootloader ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.ld)
pico_add_link_depend(bootloader ${CMAKE_CURRENT_SOURCE_DIR}/linker_defines.ld)

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
set(BOOTLOADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")

# Provide a helper to build a standalone target

function(bootloader_build_standalone_application NAME)
	set(APP_HDR_NAME ${NAME}_hdr_standalone)
	set(APP_HDR_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_HDR_NAME}.bin)
	set(APP_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.bin)
	set(APP_WHEADER_NAME ${NAME}_w_header)
	set(APP_WHEADER_ELF_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.elf)
	set(APP_WHEADER_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.bin)
	set(APP_WHEADER_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.uf2)

	target_link_directories(${NAME} PRIVATE ${BOOTLOADER_DIR})
	pico_set_linker_script(${NAME} ${BOOTLOADER_DIR}/application.ld)
	pico_add_link_depend(${NAME} ${BOOTLOADER_DIR}/linker_defines.ld)
	pico_add_bin_output(${NAME})

	add_custom_target(${APP_HDR_NAME} ALL
		COMMENT "Generating application checksum and header"
		COMMAND ${BOOTLOADER_DIR}/gen_imghdr.py --linker-defines-file ${BOOTLOADER_DIR}/linker_defines.ld ${APP_BIN_PATH} ${APP_HDR_PATH}
	 	DEPENDS ${NAME} ${APP_BIN_PATH})

	add_custom_target(${APP_WHEADER_NAME} ALL
		COMMENT "Generating application with updated header"
		COMMAND ${CMAKE_OBJCOPY} --update-section .app_hdr=${APP_HDR_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.elf ${APP_WHEADER_ELF_PATH}
		COMMAND ${CMAKE_OBJCOPY} -O binary ${APP_WHEADER_ELF_PATH} ${APP_WHEADER_BIN_PATH}
		COMMAND ${CMAKE_BINARY_DIR}/elf2uf2/elf2uf2 ${APP_WHEADER_ELF_PATH} ${APP_WHEADER_UF2_PATH}
		DEPENDS ${APP_HDR_NAME} ${APP_HDR_PATH} ${CMAKE_BINARY_DIR}/elf2uf2/elf2uf2)

endfunction()

# Provide a helper to build a combined target

function(bootloader_build_combined APP_NAME)
	set(BTL_NAME bootloader)
	set(BTL_UF2_PATH ${BOOTLOADER_BINARY_DIR}/${BTL_NAME}.uf2)
	set(APP_WHEADER_NAME ${APP_NAME}_w_header)
	set(APP_WHEADER_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${APP_WHEADER_NAME}.uf2)

	bootloader_build_standalone_application(${APP_NAME})

	# Simply concatenate the two uf2 files which *should* both be self-contained
	# Data length of each UF2 block must be exactly 256 for bootloader to function properly
	set(COMBINED_NAME ${BTL_NAME}_${APP_NAME}_combined)
	set(COMBINED_UF2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${COMBINED_NAME}.uf2)
	add_custom_target(${COMBINED_NAME} ALL
		COMMENT "Building full UF2 image"
		COMMAND ${Python3_EXECUTABLE}
				${BOOTLOADER_DIR}/uf2tool.py
				--data-len-min 256
				--data-len-max 256
				-o ${COMBINED_UF2_PATH} ${BTL_UF2_PATH} ${APP_WHEADER_UF2_PATH}
	 	DEPENDS ${BTL_NAME} ${APP_WHEADER_NAME})

endfunction()


# Embed version info into the uf2 file
pico_set_program_name(bootloader "Bootloader")
pico_set_program_version(bootloader "1.0")
# Prints how much memory used at end of build output
# (RAM usage only shows global, preallocated mem and nothing else)
set_property(TARGET bootloader APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")
