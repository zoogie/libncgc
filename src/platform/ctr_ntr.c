/* libncgc
 * Copyright (C) 2017 angelsl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../include/ncgc/platform/ctr_ntr.h"

#include <stdbool.h>
#include <stdint.h>

#include "../../include/ncgc/ntrcard.h"

#define REG_CARDCONF            (*(volatile uint16_t *)0x1000000C)
#define REG_CARDCONF2           (*(volatile uint8_t *)0x10000010)

#define REG_CTRCARD_SECCNT      (*(volatile uint32_t *)0x10004008)
#define REG_MCNT                (*(volatile uint16_t *)0x10164000)
#define REG_MDATA               (*(volatile uint16_t *)0x10164002)
#define REG_ROMCNT              (*(volatile uint32_t *)0x10164004)
#define REG_CMDP                ((volatile uint8_t *)0x10164008)
#define REG_CMD                 (*(volatile uint64_t *)0x10164008)
#define REG_SEEDX_L             (*(volatile uint32_t *)0x10164010)
#define REG_SEEDY_L             (*(volatile uint32_t *)0x10164014)
#define REG_SEEDX_H             (*(volatile uint16_t *)0x10164018)
#define REG_SEEDY_H             (*(volatile uint16_t *)0x1016401A)
#define REG_FIFO                (*(volatile uint32_t *)0x1016401C)

#define ROMCNT_ACTIVATE         (1u << 31)              // begin operation (CS low when set)
#define ROMCNT_BUSY             (ROMCNT_ACTIVATE)       // operation in progress i.e. CS still low
#define ROMCNT_WR               (1u << 30)              // card write enable
#define ROMCNT_NRESET           (1u << 29)              // RESET high when set
#define ROMCNT_SEC_LARGE        (1u << 28)              // Use "other" secure area mode, which tranfers blocks of 0x1000 bytes at a time
#define ROMCNT_CLK_SLOW         (1u << 27)              // Transfer clock rate (0 = 6.7MHz, 1 = 4.2MHz)
#define ROMCNT_BLK_SIZE(n)      (((n) & 0x7u) << 24)    // Transfer block size, (0 = None, 1..6 = (0x100 << n) bytes, 7 = 4 bytes)
#define ROMCNT_BLK_SIZE_MASK    (ROMCNT_BLK_SIZE(7))
#define ROMCNT_DATA_READY       (1u << 23)              // REG_FIFO is ready to be read
#define ROMCNT_SEC_CMD          (1u << 22)              // The command transfer will be hardware encrypted (KEY2)
#define ROMCNT_DELAY2(n)        (((n) & 0x3Fu) << 16)   // Transfer delay length part 2
#define ROMCNT_DELAY2_MASK      (ROMCNT_DELAY2(0x3F))
#define ROMCNT_SEC_SEED         (1u << 15)              // Apply encryption (KEY2) seed to hardware registers
#define ROMCNT_SEC_EN           (1u << 14)              // Security enable
#define ROMCNT_SEC_DAT          (1u << 13)              // The data transfer will be hardware encrypted (KEY2)
#define ROMCNT_DELAY1(n)        ((n) & 0x1FFFu)         // Transfer delay length part 1
#define ROMCNT_DELAY1_MASK      (ROMCNT_DELAY1(0x1FFF))
#define ROMCNT_CMD_SETTINGS     (ROMCNT_DELAY1_MASK | ROMCNT_DELAY2_MASK | ROMCNT_SEC_LARGE | \
                                    ROMCNT_SEC_CMD | ROMCNT_SEC_DAT | ROMCNT_CLK_SLOW | ROMCNT_SEC_EN)

#define MCNT_CR1_ENABLE         0x8000u
#define MCNT_CR1_IRQ            0x4000u

void ncgc_nplatform_ctr_wait_for_card(void) {
    while (REG_CARDCONF2 & 0x1);
}

bool ncgc_nplatform_ctr_card_inserted(void) {
    return !(REG_CARDCONF2 & 0x1);
}

static void io_delay(uint32_t delay) {
    /* i tried replacing this with the equivalent C code
       while (--delay) { __asm__ (""); }
       but gcc insisted on unrolling the loop so
       ¯\_(ツ)_/¯ */

    __asm__ volatile (
        "1:\n\t"
        "subs %[delay], #1\n\t"
        "bne 1b"
        : [delay] "=r" (delay)
        : "0" (delay)
    );
}

static void seed_key2(ncgc_ncard_t *const card, uint64_t x, uint64_t y) {
    (void)card;

    REG_ROMCNT = 0;
    REG_SEEDX_L = (uint32_t) (x & 0xFFFFFFFF);
    REG_SEEDY_L = (uint32_t) (y & 0xFFFFFFFF);
    REG_SEEDX_H = (uint16_t) ((x >> 32) & 0x7F);
    REG_SEEDY_H = (uint16_t) ((y >> 32) & 0x7F);
    REG_ROMCNT = ROMCNT_NRESET | ROMCNT_SEC_SEED | ROMCNT_SEC_EN | ROMCNT_SEC_DAT;
}

static int32_t reset(ncgc_ncard_t *const card) {
    if (REG_CARDCONF2 & 0x1) {
        return -1;
    }

    REG_CARDCONF2 = 0x0C;
    REG_CARDCONF &= ~3;
    if (REG_CARDCONF2 == 0xC) {
        while (REG_CARDCONF2 != 0);
    }
    if (REG_CARDCONF2 != 0) {
        return -2;
    }
    REG_CARDCONF2 = 0x4;
    while (REG_CARDCONF2 != 0x4);
    REG_CARDCONF2 = 0x8;
    while (REG_CARDCONF2 != 0x8);

    REG_CTRCARD_SECCNT &= 0xFFFFFFFB;
    io_delay(0x40000);

    REG_ROMCNT = ROMCNT_NRESET;
    REG_CARDCONF &= ~3;
    REG_CARDCONF &= ~0x100;
    REG_MCNT = MCNT_CR1_ENABLE;
    io_delay(0x40000);

    REG_ROMCNT = 0;
    REG_MCNT &= 0xFF;
    io_delay(0x40000);

    REG_MCNT |= (MCNT_CR1_ENABLE | MCNT_CR1_IRQ);
    REG_ROMCNT = ROMCNT_NRESET | ROMCNT_SEC_SEED;
    while (REG_ROMCNT & ROMCNT_BUSY);

    card->encryption_state = NCGC_NRAW;
    return 0;
}

static int32_t send_command(ncgc_ncard_t *const card, const uint64_t cmd, const uint32_t read_size,
        void *const dest, const uint32_t dest_size, const ncgc_nflags_t flags) {
    (void)card;

    uint32_t blksizeflag;
    switch (read_size) {
        default: return -1;
        case 0: blksizeflag = 0; break;
        case 4: blksizeflag = 7; break;
        case 0x200: blksizeflag = 1; break;
        case 0x400: blksizeflag = 2; break;
        case 0x800: blksizeflag = 3; break;
        case 0x1000: blksizeflag = 4; break;
        case 0x2000: blksizeflag = 5; break;
        case 0x4000: blksizeflag = 6; break;
    }
    uint32_t bitflags = ROMCNT_ACTIVATE | ROMCNT_NRESET | ROMCNT_BLK_SIZE(blksizeflag) |
        ((ncgc_nflags_key2_command(flags) || ncgc_nflags_key2_data(flags)) ? ROMCNT_SEC_EN : 0) |
        (flags.flags & ROMCNT_CMD_SETTINGS);

    REG_CMD = cmd;
    REG_ROMCNT = bitflags;

    uint32_t *cur = dest;
    uint32_t ctr = 0;
    do {
        if (REG_ROMCNT & ROMCNT_DATA_READY) {
            uint32_t data = REG_FIFO;
            if (dest && ctr < dest_size) {
                *(cur++) = data;
            } else {
                (void)data;
            }
            ctr += 4;
        }
	} while (REG_ROMCNT & ROMCNT_BUSY);
    return ctr;
}

void ncgc_nplatform_ctr_init(ncgc_ncard_t *card) {
    card->platform = (ncgc_nplatform_t) {
        .data = { .int_data = 0 },
        .reset = reset,
        .send_command = send_command,
        .io_delay = io_delay,
        .seed_key2 = seed_key2,
        .hw_key2 = true
    };
}
