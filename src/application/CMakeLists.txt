
add_executable(blink_noboot2 blink.c)

pico_add_extra_outputs(blink_noboot2)

set_target_properties(blink_noboot2 PROPERTIES COMPILE_FLAGS "-Wall")

# Either or, not both (combined function builds standalone application too)
#bootloader_build_standalone_application(blink_noboot2)
bootloader_build_combined(blink_noboot2)

target_link_libraries(blink_noboot2 pico_stdlib_headers pico_stdlib hardware_watchdog hardware_structs)
