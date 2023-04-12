/**
 * Copyright (c) 2021 Brian Starkey <stark3y@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "RP2040.h"
#include "pico/time.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/watchdog.h"
#include "hardware/gpio.h"
#include "hardware/resets.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "boot/uf2.h"

#include "pico/stdlib.h"
//
#include "ff.h" /* Obtains integer types */
//
#include "diskio.h" /* Declarations of disk functions */
//
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"

// Defined in linker file
extern void* __APPLICATION_HEADER_OFFSET;
extern void* __APPLICATION_OFFSET;

// SD_APPLICATION_NAME can be defined externally
#ifndef SD_APPLICATION_NAME
#define SD_APPLICATION_NAME "0:/application.uf2"
#endif

// The bootloader can be entered in three ways:
//  - Application uf2 file found on SD card
//  - Watchdog scratch[5] == BOOTLOADER_ENTRY_MAGIC && scratch[6] == ~BOOTLOADER_ENTRY_MAGIC
//  - No valid image header
#define BOOTLOADER_ENTRY_MAGIC 0xb105f00d

#define IMAGE_HEADER_OFFSET ((uint32_t)&__APPLICATION_HEADER_OFFSET)

#define APP_ADDR_MIN (XIP_BASE + IMAGE_HEADER_OFFSET)
#define FLASH_ADDR_MAX (XIP_BASE + PICO_FLASH_SIZE_BYTES)

static void disable_interrupts(void)
{
	SysTick->CTRL &= ~1;

	NVIC->ICER[0] = 0xFFFFFFFF;
	NVIC->ICPR[0] = 0xFFFFFFFF;
}

static void reset_peripherals(void)
{
	reset_block(~(
			RESETS_RESET_IO_QSPI_BITS |
			RESETS_RESET_PADS_QSPI_BITS |
			RESETS_RESET_SYSCFG_BITS |
			RESETS_RESET_PLL_SYS_BITS
	));
}

static void jump_to_vtor(uint32_t vtor)
{
	// Derived from the Leaf Labs Cortex-M3 bootloader.
	// Copyright (c) 2010 LeafLabs LLC.
	// Modified 2021 Brian Starkey <stark3y@gmail.com>
	// Originally under The MIT License
	uint32_t reset_vector = *(volatile uint32_t *)(vtor + 0x04);

	SCB->VTOR = (volatile uint32_t)(vtor);

	asm volatile("msr msp, %0"::"g"
			(*(volatile uint32_t *)vtor));
	asm volatile("bx %0"::"r" (reset_vector));
}

// ptr must be 4-byte aligned and len must be a multiple of 4
static uint32_t calc_crc32(void *ptr, uint32_t len)
{
	uint32_t dummy_dest, crc;

	int channel = dma_claim_unused_channel(true);
	dma_channel_config c = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);
	channel_config_set_sniff_enable(&c, true);

	// Seed the CRC calculation
	dma_hw->sniff_data = 0xffffffff;

	// Mode 1, then bit-reverse the result gives the same result as
	// golang's IEEE802.3 implementation
	dma_sniffer_enable(channel, 0x1, true);
	dma_hw->sniff_ctrl |= DMA_SNIFF_CTRL_OUT_REV_BITS;

	dma_channel_configure(channel, &c, &dummy_dest, ptr, len / 4, true);

	dma_channel_wait_for_finish_blocking(channel);

	// Read the result before resetting
	crc = dma_hw->sniff_data ^ 0xffffffff;

	dma_sniffer_disable();
	dma_channel_unclaim(channel);

	return crc;
}

struct image_header {
	uint32_t vtor;
	uint32_t size;
	uint32_t crc;
	uint8_t pad[FLASH_PAGE_SIZE - (3 * 4)];
};
static_assert(sizeof(struct image_header) == FLASH_PAGE_SIZE, "image_header must be FLASH_PAGE_SIZE bytes");

static bool image_header_ok(struct image_header *hdr)
{
	uint32_t *vtor = (uint32_t *)hdr->vtor;

	uint32_t calc = calc_crc32((void *)hdr->vtor, hdr->size);

	// CRC has to match
	if (calc != hdr->crc) {
		return false;
	}

	// Stack pointer needs to be in RAM
	if (vtor[0] < SRAM_BASE) {
		return false;
	}

	// Reset vector should be in the image, and thumb (bit 0 set)
	if ((vtor[1] < hdr->vtor) || (vtor[1] > hdr->vtor + hdr->size) || !(vtor[1] & 1)) {
		return false;
	}

	// Looks OK.
	return true;
}

static void do_reboot(bool to_bootloader)
{
	hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
	if (to_bootloader) {
		watchdog_hw->scratch[5] = BOOTLOADER_ENTRY_MAGIC;
		watchdog_hw->scratch[6] = ~BOOTLOADER_ENTRY_MAGIC;
	} else {
		watchdog_hw->scratch[5] = 0;
		watchdog_hw->scratch[6] = 0;
	}
	watchdog_reboot(0, 0, 0);
	while (1) {
		tight_loop_contents();
		asm("");
	}
}

bool check_for_sd_application(sd_card_t* pSd)
{
	bool fileExists = false;

	if (pSd->mounted && !sd_test_com(pSd))
	{
		f_unmount(pSd->pcName);
		pSd->mounted = false;
	}

	FRESULT fr = FR_OK;

	if (!pSd->mounted)
	{
		// Wait a moment to ensure SD is powered, and then try to mount it
		sleep_ms(100);
		fr = f_mount(&pSd->fatfs, pSd->pcName, 1);
	}

	if (FR_OK == fr)
	{
		// Now mounted
		pSd->mounted = true;
		// Check for existence of application file
		FILINFO fileInfo;
		fr = f_stat(SD_APPLICATION_NAME, &fileInfo);
		fileExists = (FR_OK == fr && fileInfo.fsize > 0);
	}
	else
	{
		// Failed
		pSd->m_Status |= STA_NOINIT;
		pSd->mounted = false;
	}

	return fileExists;
}

static bool should_stay_in_bootloader()
{
	bool wd_says_so = (watchdog_hw->scratch[5] == BOOTLOADER_ENTRY_MAGIC) &&
		(watchdog_hw->scratch[6] == ~BOOTLOADER_ENTRY_MAGIC);

	return wd_says_so || check_for_sd_application(sd_get_by_num(0));
}

void load_from_sd(sd_card_t* pSd)
{
	// Keep trying to load forever
	while (true)
	{
		if (check_for_sd_application(pSd))
		{
			gpio_put(PICO_DEFAULT_LED_PIN, 0);
			FIL file;
			FRESULT fr = f_open(&file, SD_APPLICATION_NAME, FA_READ | FA_OPEN_EXISTING);
			if (FR_OK == fr)
			{
				// Erase the header area to invalidate the application
				flash_range_erase(IMAGE_HEADER_OFFSET, FLASH_SECTOR_SIZE);

				uint8_t header[FLASH_SECTOR_SIZE];
				memset(header, 0xFF, sizeof(header));

				UINT br = 0;
				struct uf2_block uf2_page;
				fr = f_read(&file, &uf2_page, sizeof(uf2_page), &br);
				while (FR_OK == fr && br == sizeof(uf2_page))
				{
					if (uf2_page.magic_start0 == UF2_MAGIC_START0
						&& uf2_page.magic_start1 == UF2_MAGIC_START1
						&& uf2_page.magic_end == UF2_MAGIC_END
						&& (uf2_page.flags & UF2_FLAG_FAMILY_ID_PRESENT) != 0
						&& uf2_page.file_size == RP2040_FAMILY_ID)
					{
						// Valid RP2040 block

						if (uf2_page.target_addr >= APP_ADDR_MIN)
						{
							if (uf2_page.target_addr < (APP_ADDR_MIN + FLASH_SECTOR_SIZE))
							{
								// Header should be written last
								uint32_t offset = uf2_page.target_addr - APP_ADDR_MIN;
								memcpy(&header[offset], uf2_page.data, uf2_page.payload_size);
							}
							else if (uf2_page.target_addr < FLASH_ADDR_MAX)
							{
								// Valid application address range
								uint32_t flashOffset = uf2_page.target_addr - XIP_BASE;
								if ((flashOffset % FLASH_SECTOR_SIZE) == 0)
								{
									// Beginning of flash sector
									flash_range_erase(flashOffset, FLASH_SECTOR_SIZE);
								}
								// Assumption: uf2_page.payload_size is always 256
								flash_range_program(
									flashOffset,
									uf2_page.data,
									uf2_page.payload_size);
							}
						}
					}
					br = 0;
					fr = f_read(&file, &uf2_page, sizeof(uf2_page), &br);
				}

				// TODO: only write if above was successful
				// Write header last
				flash_range_program(IMAGE_HEADER_OFFSET, header, sizeof(header));

				f_close(&file);

				// Delete the file as confirmation that it was loaded
				f_unlink(SD_APPLICATION_NAME);

				// Complete
				return;
			}
			else
			{
				sleep_ms(2000);
			}
		}
		else
		{
			sleep_ms(2000);
		}
		gpio_put(PICO_DEFAULT_LED_PIN, 1);
	}
}

int main(void)
{
	sd_init_driver();
	sd_get_by_num(0)->mounted = false;

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	gpio_put(PICO_DEFAULT_LED_PIN, 1);

	// Enable for debug, disable for release
	// stdio_usb_init();
	// for (uint32_t i = 10; i > 0; --i)
	// {
	// 	printf("%lu\n", i);
	// 	sleep_ms(1000);
	// }
	// printf("0... start\n");

	sleep_ms(100);

	struct image_header *hdr = (struct image_header *)(XIP_BASE + IMAGE_HEADER_OFFSET);

	if (!should_stay_in_bootloader() && image_header_ok(hdr)) {
		uint32_t vtor = *((uint32_t *)(XIP_BASE + IMAGE_HEADER_OFFSET));
		disable_interrupts();
		reset_peripherals();
		jump_to_vtor(vtor);
	}

	// This loops until file is found and loaded
	load_from_sd(sd_get_by_num(0));

	do_reboot(false);

	return 0;
}
