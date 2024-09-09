/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef OPENOCD_FLASH_NOR_STMSPI_H
#define OPENOCD_FLASH_NOR_STMSPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "spi.h"


struct spi_registers {
    uint32_t cr1;
    uint32_t cr2;
    uint32_t sr;
    uint32_t dr;
    uint32_t crcpr;
    uint32_t rxcrcr;
    uint32_t txcrcr;
    uint32_t i2scfgr;
    uint32_t i2spr;
};

struct dma_stream_registers {
    uint32_t cr;   // Configuration
    uint32_t ndtr; // Number of Data
    uint32_t par;  // Peripheral address
    uint32_t m0ar; // Memory0 address
    uint32_t m1ar; // Memory1 address
    uint32_t fcr;  // FIFO control
};


#define DMA_STREAM_NUMBER 8
struct dma_registers {
    uint32_t lisr; // Interrupt status
    uint32_t hisr; // Interrupt status
    uint32_t lifcr;// Interrupt flag clear
    uint32_t hifcr;// Interrupt flag clear
    struct dma_stream_registers s[DMA_STREAM_NUMBER];
};


struct rcc_registers {
    uint32_t cr;
    uint32_t pllcfgr;
    uint32_t cfgr;
    uint32_t cir;
    uint32_t ahb_rstr[3];
    uint32_t reserved_1;
    uint32_t apb_rstr[2];
    uint32_t reserved_2[2];
    uint32_t ahb_enr[3];
    uint32_t reserved_3;
    uint32_t apb_enr[2];
    uint32_t reserved_4[2];
    uint32_t ahb_lpenr[3];
    uint32_t reserved_5;
    uint32_t apb_lpenr[2];
    uint32_t reserved_6[2];
    uint32_t bdcr;
    uint32_t csr;
    uint32_t reserved_7[2];
    uint32_t sscgr;
    uint32_t plli2scfgr;
    uint32_t pllsaicfgr;
    uint32_t dckcfgr[2];
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct dma_params {
    uint32_t io_base;
    int number;
    int tx_channel;
    int tx_stream;
    int rx_channel;
    int rx_stream;
};

struct spi_cs_params {
    uint32_t io_base;
    size_t pin_number;
    bool active_high;
};

struct spi_params {
    uint32_t io_base;
    int number;
    struct spi_cs_params cs;
};


struct rcc_params {
    uint32_t io_base;
};

struct remote_cache_address {
    size_t   size;
    uint32_t tx_data;
    uint32_t rx_data;
};


struct stmspi_flash_bank {
    bool probed;
    char devname[32];
    uint16_t manufacturer;
	uint16_t device_id;

    struct flash_device dev;
    struct spi_params spi;
    struct dma_params dma;
    struct rcc_params rcc;
    struct remote_cache_address cache;

    int ( *xfer)(struct flash_bank *bank, const void *cmd, size_t cmd_size,
                                          const void *tx,  size_t tx_size,
                                          void *rx,  size_t rx_size);

    int (*configure)(struct flash_bank*);
    int (*write)(struct flash_bank*, const uint8_t*, uint32_t, uint32_t);
    int (*read)(struct flash_bank*, uint8_t*, uint32_t, uint32_t);
    int (*erase)(struct flash_bank*, uint32_t, uint32_t);
    int (*erase_all)(struct flash_bank*);
    int (*protect)(struct flash_bank*, int, unsigned int, unsigned int);
    int (*erase_check)(struct flash_bank*);
    int (*verify)(struct flash_bank*, const uint8_t*, uint32_t, uint32_t);
    int (*protect_check)(struct flash_bank*);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define RCC_ADDR(base, member) ( (base) + offsetof(struct rcc_registers, member) )
#define SPI_ADDR(base, member) ( (base) + offsetof(struct spi_registers, member) )
#define DMA_ADDR(base, member) ( (base) + offsetof(struct dma_registers, member) )


#define DUMP_RCC_REG(target, val, base, member) \
    target_read_u32(target, RCC_ADDR(base, member), &val); \
    LOG_INFO("[0x%08lx] %s:\t0x%08x",\
             RCC_ADDR(base, member),\
             #member,\
             val)

#define DUMP_SPI_REG(target, val, base, member) \
    target_read_u16(target, SPI_ADDR(base, member), &val); \
    LOG_INFO("[0x%08lx] %s:\t0x%08x",\
             SPI_ADDR(base, member),\
             #member,\
             val)

#define DUMP_DMA_REG(target, val, base, member) \
    target_read_u32(target, DMA_ADDR(base, member), &val); \
    LOG_INFO("[0x%08lx] %s:\t0x%08x",\
             DMA_ADDR(base, member),\
             #member,\
             val)


#define DUMP_RCC_REGS(target, val, rcc) \
    LOG_INFO(" ~~~ RCC: base ~~~");                        \
    DUMP_RCC_REG(target, val, rcc.io_base, cr);            \
    DUMP_RCC_REG(target, val, rcc.io_base, pllcfgr);       \
    DUMP_RCC_REG(target, val, rcc.io_base, cfgr);          \
    DUMP_RCC_REG(target, val, rcc.io_base, cir);           \
    LOG_INFO(" ~~~ RCC: AHB Reset ~~~");                   \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_rstr[0] );  \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_rstr[1] );  \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_rstr[2] );  \
    LOG_INFO(" ~~~ RCC: APB Reset ~~~");                   \
    DUMP_RCC_REG(target, val, rcc.io_base, apb_rstr[0] );  \
    DUMP_RCC_REG(target, val, rcc.io_base, apb_rstr[1] );  \
    LOG_INFO(" ~~~ RCC: AHB enable ~~~");                  \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_enr[0] );   \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_enr[1] );   \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_enr[2] );   \
    LOG_INFO(" ~~~ RCC: APB Enable ~~~");                  \
    DUMP_RCC_REG(target, val, rcc.io_base, apb_enr[0] );   \
    DUMP_RCC_REG(target, val, rcc.io_base, apb_enr[1] );   \
    LOG_INFO(" ~~~ RCC: AHB low power enable ~~~");        \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_lpenr[0] ); \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_lpenr[1] ); \
    DUMP_RCC_REG(target, val, rcc.io_base, ahb_lpenr[2] ); \
    LOG_INFO(" ~~~ RCC: APB low power enable ~~~");        \
    DUMP_RCC_REG(target, val, rcc.io_base, apb_lpenr[0] ); \
    DUMP_RCC_REG(target, val, rcc.io_base, apb_lpenr[1] ); \
    LOG_INFO(" ~~~ RCC: tail ~~~");                        \
    DUMP_RCC_REG(target, val, rcc.io_base, bdcr);          \
    DUMP_RCC_REG(target, val, rcc.io_base, csr);           \
    DUMP_RCC_REG(target, val, rcc.io_base, sscgr);         \
    DUMP_RCC_REG(target, val, rcc.io_base, plli2scfgr);    \
    DUMP_RCC_REG(target, val, rcc.io_base, pllsaicfgr);    \
    DUMP_RCC_REG(target, val, rcc.io_base, dckcfgr[0]);    \
    DUMP_RCC_REG(target, val, rcc.io_base, dckcfgr[1])


#define DUMP_SPI_BASE_REGS(target, val, spi) \
    LOG_INFO(" ~~~ SPI%d ~~~", spi.number);         \
    DUMP_SPI_REG(target, val, spi.io_base, cr1);    \
    DUMP_SPI_REG(target, val, spi.io_base, cr2);    \
    DUMP_SPI_REG(target, val, spi.io_base, sr);     \
    DUMP_SPI_REG(target, val, spi.io_base, crcpr);  \
    DUMP_SPI_REG(target, val, spi.io_base, rxcrcr); \
    DUMP_SPI_REG(target, val, spi.io_base, txcrcr)


#define DUMP_DMA_BASE_REGS(target, val, dma) \
    LOG_INFO(" ~~~ DMA%d ~~~", dma.number);        \
    DUMP_DMA_REG(target, val, dma.io_base, lisr);  \
    DUMP_DMA_REG(target, val, dma.io_base, hisr);  \
    DUMP_DMA_REG(target, val, dma.io_base, lifcr); \
    DUMP_DMA_REG(target, val, dma.io_base, hifcr)


#define DUMP_DMA_STREAM_REGS(target, val, dma, idx) \
    LOG_INFO("  --- DMA%d.s%d --- ", dma.number, idx);   \
    DUMP_DMA_REG(target, val, dma.io_base, s[idx].cr);   \
    DUMP_DMA_REG(target, val, dma.io_base, s[idx].ndtr); \
    DUMP_DMA_REG(target, val, dma.io_base, s[idx].par);  \
    DUMP_DMA_REG(target, val, dma.io_base, s[idx].m0ar); \
    DUMP_DMA_REG(target, val, dma.io_base, s[idx].m1ar); \
    DUMP_DMA_REG(target, val, dma.io_base, s[idx].fcr)

#endif /* OPENOCD_FLASH_NOR_STMSPI_H */
