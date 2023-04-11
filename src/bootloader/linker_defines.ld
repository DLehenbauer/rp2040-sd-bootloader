/* DO NOT USE ANY SHORT HAND BESIDES k IN THIS FILE (gen_imghdr.py parses this file for values) */

/* This is flash size on a pico board - change if different */
__FLASH_SIZE = 2048k;

/* All lengths must be a multiple of 4k */
__BOOTLOADER_LENGTH = 13 * 4k;
__APPLICATION_HEADER_LENGTH = 1 * 4k;
__APPLICATION_LENGTH = 2048k - __APPLICATION_HEADER_LENGTH - __BOOTLOADER_LENGTH;

/* It's best not to modify these */
__BOOTLOADER_OFFSET = 0;
__APPLICATION_HEADER_OFFSET = __BOOTLOADER_LENGTH;
__APPLICATION_OFFSET = __APPLICATION_HEADER_OFFSET + __APPLICATION_HEADER_LENGTH;
