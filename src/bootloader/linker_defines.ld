/* This file should only include variable definitions used by other linker directive files.
 * gen_imghdr.py also parses this file for values.
 */

/* NOTE: All lengths must be multiple of the size of a page (4k) */

/* This is flash size on a pico board - change if different */
__FLASH_SIZE = 2M;

/* Adjust desired max size of bootloader code here */
__BOOTLOADER_LENGTH = 13 * 4k;

/* It's best not to modify any of the below */
__APPLICATION_HEADER_LENGTH = 1 * 4k;
__APPLICATION_LENGTH = 2048k - __APPLICATION_HEADER_LENGTH - __BOOTLOADER_LENGTH;
__BOOTLOADER_OFFSET = 0;
__APPLICATION_HEADER_OFFSET = __BOOTLOADER_OFFSET + __BOOTLOADER_LENGTH;
__APPLICATION_OFFSET = __APPLICATION_HEADER_OFFSET + __APPLICATION_HEADER_LENGTH;

__FLASH_START_ADDR = 0x10000000;
