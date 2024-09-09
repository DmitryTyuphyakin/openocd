// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include <helper/binarybuffer.h>
#include <helper/bits.h>
#include <helper/time_support.h>
#include <target/algorithm.h>
#include <target/armv7m.h>
#include <target/image.h>

#include "stmspi.h"
#include "sfdp.h"


#include "spi/s25fl.h"


static int _spi_cs(struct flash_bank *bank, bool enable)
{
    struct target *target = bank->target;
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct spi_params *spi = &info->spi;
    bool on = (spi->cs.active_high) ? enable : (!enable);

    uint32_t reg = spi->cs.io_base + 0x14; // OUTPUT DATA
    uint32_t mask = (1 << spi->cs.pin_number);

    return (on) ? target_modify_u32(target, reg, mask, 0)
                : target_modify_u32(target, reg, 0, mask);
}

static int spi_xmit(struct flash_bank *bank, size_t size)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct target *target = bank->target;
    struct dma_params *dma = &info->dma;
    struct spi_params *spi = &info->spi;
    uint32_t set_mask = 0;
    uint32_t clr_mask = 0;
    uint32_t address = 0;
    uint32_t h_status = 0;
    uint32_t l_status = 0;
    int retval = 0;
    bool ready = false;
    bool error = false;

    // DMA: set data size
    address = DMA_ADDR(dma->io_base, s[dma->tx_stream].ndtr);
    retval = target_write_u32(target, address, size);
    if (retval != ERROR_OK) return retval;

    address = DMA_ADDR(dma->io_base, s[dma->rx_stream].ndtr);
    retval = target_write_u32(target, address, size);
    if (retval != ERROR_OK) return retval;

    // DMA: reset events
    address = DMA_ADDR(dma->io_base, hifcr);
    retval = target_write_u32(target, address, 0xffffffff);
    if (retval != ERROR_OK) return retval;

    address = DMA_ADDR(dma->io_base, lifcr);
    retval = target_write_u32(target, address, 0xffffffff);
    if (retval != ERROR_OK) return retval;

    // DMA: enable stream
    address = DMA_ADDR(dma->io_base, s[dma->rx_stream].cr);
    set_mask = (1 << 0); // Enable
    retval = target_modify_u32(target, address, set_mask, 0);
    if (retval != ERROR_OK) return retval;

    address = DMA_ADDR(dma->io_base, s[dma->tx_stream].cr);
    set_mask = (1 << 0); // Enable
    retval = target_modify_u32(target, address, set_mask, 0);
    if (retval != ERROR_OK) return retval;

    // CS: enable
    retval = _spi_cs(bank, true);
    if (retval != ERROR_OK) return retval;

    // SPI: enable DMA
    address = SPI_ADDR(spi->io_base, cr2);
    set_mask = (3 << 0); // Enable TX/RX DMA
    retval = target_modify_u32(target, address, set_mask, 0);
    if (retval != ERROR_OK) return retval;

    // Wait compleat
    while (!ready) {
        // Read DMA status
        address = DMA_ADDR(dma->io_base, hisr);
        retval = target_read_u32(target, address, &h_status);
        if (retval != ERROR_OK) return retval;

        address = DMA_ADDR(dma->io_base, lisr);
        retval = target_read_u32(target, address, &l_status);
        if (retval != ERROR_OK) return retval;

        // Check status
        error = (h_status & 0x034d034d) | (l_status & 0x034d034d); // DMEIFx | TEIFx | FEIFx
        ready = (h_status & 0x08200820) | (l_status & 0x08200820); // TCIFx

        if (error) return ERROR_FAIL;
    }

    // TX: disable DMA streams
    address = DMA_ADDR(dma->io_base, s[dma->tx_stream].cr);
    clr_mask = (1 << 0); // Disable
    retval = target_modify_u32(target, address, 0, clr_mask);
    if (retval != ERROR_OK) return retval;

    // RX: disable DMA streams
    address = DMA_ADDR(dma->io_base, s[dma->rx_stream].cr);
    clr_mask = (1 << 0); // Disable
    retval = target_modify_u32(target, address, 0, clr_mask);
    if (retval != ERROR_OK) return retval;

    // SPI: disable DMA
    address = SPI_ADDR(spi->io_base, cr2);
    clr_mask = (3 << 0); // Disable TX/RX DMA
    retval = target_modify_u32(target, address, 0, clr_mask);
    if (retval != ERROR_OK) return retval;

    // CS: disabe
    retval = _spi_cs(bank, false);
    if (retval != ERROR_OK) return retval;

    return retval;
}

static int xfer(struct flash_bank *bank, const void *cmd, size_t cmd_size,
                                         const void *tx,  size_t tx_size,
                                               void *rx,  size_t rx_size)
{
    struct target *target = bank->target;
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct remote_cache_address *addr = &info->cache;
    int retval = 0;
    uint32_t address = 0;

    // TX: fill buffer
    address = addr->tx_data;
    retval = target_write_buffer(target, address, cmd_size, cmd);
    if (retval) return retval;

    if (tx_size) {
        address += cmd_size;
        retval = target_write_buffer(target, address, tx_size, tx);
        if (retval != ERROR_OK) return retval;
    }

    // Process command
    retval = spi_xmit(bank, cmd_size + tx_size + rx_size);
    if (retval != ERROR_OK) return retval;

    // RX: get data
    address = addr->rx_data + cmd_size + tx_size;
    retval = target_read_buffer(target, address, rx_size, rx);
    if (retval != ERROR_OK) return retval;

    return retval;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


static int stmspi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->erase) ? info->erase(bank, first, last)
                         : ERROR_NOT_IMPLEMENTED;
}

static int stmspi_read(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->read) ? info->read(bank, buffer, offset, count)
                        : ERROR_NOT_IMPLEMENTED;
}

static int stmspi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->write) ? info->write(bank, buffer, offset, count)
                         : ERROR_NOT_IMPLEMENTED;
}


static int stmspi_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->protect) ? info->protect(bank, set, first, last)
                           : ERROR_NOT_IMPLEMENTED;
}

static int stmspi_erase_check(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->erase_check) ? info->erase_check(bank)
                               : ERROR_NOT_IMPLEMENTED;
}

static int stmspi_verify(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->verify) ? info->verify(bank, buffer, offset, count)
                          : ERROR_NOT_IMPLEMENTED;
}

static int stmspi_protect_check(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->protect_check) ? info->protect_check(bank)
                                 : ERROR_NOT_IMPLEMENTED;
}




// DMA registers
#define RCC_AHB1ENR_DMA1EN (1 << 21)
#define RCC_AHB1ENR_DMA2EN (1 << 22)

#define RCC_AHB1LPENR_DMA1EN (1 << 21)
#define RCC_AHB1LPENR_DMA2EN (1 << 22)

#define RCC_AHB1RSTR_DMA1EN (1 << 21)
#define RCC_AHB1RSTR_DMA2EN (1 << 22)

static int _dma_init(struct flash_bank *bank)
{
    struct target *target = bank->target;
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct remote_cache_address *cache = &info->cache;
    int retval = ERROR_OK;
    uint32_t address = 0;
    uint32_t mask = 0;

    /* -= Enable POWER =- */
    address = RCC_ADDR(info->rcc.io_base, apb_enr[0]);
    mask = (1 << 28); // PWR_EN
    retval = target_modify_u32(target, address, mask, 0);
    if (retval != ERROR_OK) return retval;


    /* -= Enable DMA =- */
    // RCC_AHB1ENR
    address = RCC_ADDR(info->rcc.io_base, ahb_enr[0]);
    mask = (info->dma.number == 1) ? RCC_AHB1ENR_DMA1EN
                                   : RCC_AHB1ENR_DMA2EN;
    retval = target_modify_u32(target, address, mask, 0);
    if (retval != ERROR_OK) return retval;

    // RCC_AHB1LPENR
    address = RCC_ADDR(info->rcc.io_base, ahb_lpenr[0]);
    mask = (info->dma.number == 1) ? RCC_AHB1LPENR_DMA1EN
                                   : RCC_AHB1LPENR_DMA2EN;
    retval = target_modify_u32(target, address, mask, 0);
    if (retval != ERROR_OK) return retval;


    /* -= Reset DMA =- */
    address = RCC_ADDR(info->rcc.io_base, ahb_rstr[0]);
    mask = (info->dma.number == 1) ? RCC_AHB1RSTR_DMA1EN
                                   : RCC_AHB1RSTR_DMA2EN;
    retval = target_modify_u32(target, address, mask, 0);
    if (retval != ERROR_OK) return retval;

    usleep(100);

    retval = target_modify_u32(target, address, 0, mask);
    if (retval != ERROR_OK) return retval;

    usleep(100);


    /* -= Configure TX DMA =-*/
    // Configure Register
    address = DMA_ADDR(info->dma.io_base, s[info->dma.tx_stream].cr);
    mask =  (info->dma.tx_channel << 25) | // Channel
            (1 << 16)                    | // Priority
            (1 << 10)                    | // Memory increment mode
            (1 << 06);                     // Dir: memory to peripheral
    retval = target_write_u32(bank->target, address, mask);
    if (retval != ERROR_OK) return retval;

    // Peripheral address
    address = DMA_ADDR(info->dma.io_base, s[info->dma.tx_stream].par);
    mask = SPI_ADDR(info->spi.io_base, dr);
    retval = target_write_u32(bank->target, address, mask);
    if (retval != ERROR_OK) return retval;

    // Memory address
    address = DMA_ADDR(info->dma.io_base, s[info->dma.tx_stream].m0ar);
    mask = cache->tx_data;
    retval = target_write_u32(bank->target, address, mask);
    if (retval != ERROR_OK) return retval;


    /* -= Configure RX DMA =-*/
    // Configure Register
    address = DMA_ADDR(info->dma.io_base, s[info->dma.rx_stream].cr);
    mask =  (info->dma.rx_channel << 25) | // Channel
            (1 << 16)                    | // Priority
            (1 << 10);                     // Memory increment mode
    retval = target_write_u32(bank->target, address, mask);
    if (retval != ERROR_OK) return retval;

    // Peripheral address
    address = DMA_ADDR(info->dma.io_base, s[info->dma.rx_stream].par);
    mask = SPI_ADDR(info->spi.io_base, dr);
    retval = target_write_u32(bank->target, address, mask);
    if (retval != ERROR_OK) return retval;

    // Memory address
    address = DMA_ADDR(info->dma.io_base, s[info->dma.rx_stream].m0ar);
    mask = cache->rx_data;
    retval = target_write_u32(bank->target, address, mask);
    if (retval != ERROR_OK) return retval;

    return retval;
}


static int stmspi_probe(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    int retval = ERROR_OK;

    if (info == NULL) return ERROR_FAIL;
    if (info->probed) return ERROR_OK;

    LOG_INFO("%s: use spi%d", __func__, info->spi.number);

    // init cache to DTCM SRAM address
    info->cache.size =    0x00010000;
    info->cache.tx_data = 0x20010000;
    info->cache.rx_data = 0x20020000;

    retval = _dma_init(bank);
    if (retval != ERROR_OK) return retval;

    if (info->configure) {
        retval = info->configure(bank);
        info->probed = (retval == ERROR_OK);
    } else {
        retval = ERROR_NOT_IMPLEMENTED;
    }

    return retval;
}


static int stmspi_auto_probe(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;

    if (info == NULL) return ERROR_FAIL;

    return (info->probed) ? ERROR_OK
                          : stmspi_probe(bank);
}


static int stmspi_get_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct stmspi_flash_bank *info = bank->driver_priv;

	if (!info->probed) {
		command_print_sameline(cmd, "\nstm32f7x external flash not probed yet\n");
		return ERROR_FLASH_BANK_NOT_PROBED;
	}

	command_print_sameline(cmd,
        "SPI%d memory: manufacturer(0x%02x), device_id(0x%04x)",
        info->spi.number,
        info->manufacturer,
        info->device_id
    );

    return ERROR_OK;
}

//~~~~~~~~ CUT HERE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
typedef enum {
    STMSPI_FLASH_S25FL,
    STMSPI_FLASH_NUMBER
} stmspi_flash_type;

FLASH_BANK_COMMAND_HANDLER(stmspi_flash_bank_command)
{
    struct stmspi_flash_bank *info = NULL;
    uint32_t cs_base = 0;
    uint32_t cs_number = 0;
    bool cs_active_high = false;
    int flash_type = -1;
    uint32_t spi_number = 0;
    uint32_t spi_io_base[] = {
        0xffffffff, // <unavailable>
        0x40013000, // SPI 1
        0x40003800, // SPI 2
        0x40003c00, // SPI 3
        0x40013400, // SPI 4
        0x40015000, // SPI 5
        0x40015400, // SPI 6
    };
    struct dma_params dma[] = {
        // IO_BASE,   NUM, TX_CH, TX_STR, RX_CH, RX_STR
        { 0xffffffff, -1,   -1,    -1,     -1,    -1 }, // <unavailable>
        { 0x40026400,  2,    3,     3,      3,     2 }, // SPI 1
        { 0x40026000,  1,    0,     4,      9,     1 }, // SPI 2
        { 0x40026000,  1,    0,     5,      0,     0 }, // SPI 3
        { 0x40026400,  2,    4,     1,      4,     0 }, // SPI 4
        { 0x40026400,  2,    7,     6,      7,     5 }, // SPI 5
        { 0x40026400,  2,    1,     5,      1,     6 }, // SPI 6
    };


    // Get base address from arguments
    if (CMD_ARGC < 11) {
        LOG_ERROR("invalid syntax");
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    // Parse SPI number
    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[6], spi_number);
    if (spi_number > 6) {
        LOG_ERROR("incorrect SPI number: %d (0-6)", spi_number);
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    // Parse CS
    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[7], cs_base);
    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[8], cs_number);
    COMMAND_PARSE_ON_OFF(     CMD_ARGV[9], cs_active_high);

    // Parse SPI Flash
    if (strncmp(CMD_ARGV[10], "s25fl", strlen(CMD_ARGV[10])) == 0) {
        flash_type = STMSPI_FLASH_S25FL;
    } else {
        LOG_ERROR("incorrect SPI flash: %s (s25fl)", CMD_ARGV[10]);
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    LOG_DEBUG("%s: SPI_%d, CS: base(0x%08x), number(%u), active_high(%b)",
             __func__, spi_number, cs_base, cs_number, cs_active_high);

    // Prepare driver private data
    info = calloc(1, sizeof(struct stmspi_flash_bank));
    if (!info) {
        LOG_ERROR("not enough memory");
        return ERROR_FAIL;
    }

    // SPI
    info->spi.number         = spi_number;
    info->spi.io_base        = spi_io_base[spi_number];
    info->spi.cs.io_base     = cs_base;
    info->spi.cs.pin_number  = cs_number;
    info->spi.cs.active_high = cs_active_high;

    // DMA
    memcpy(&info->dma, &dma[spi_number], sizeof(info->dma));

    // RCC
    info->rcc.io_base = 0x40023800;

    // Callback
    info->xfer = xfer;

    // Flash
    switch (flash_type) {
        case STMSPI_FLASH_S25FL:
            info->configure = s25fl_configure;
            info->write     = s25fl_write;
            info->read      = s25fl_read;
            info->erase     = s25fl_erase;
            info->erase_all = s25fl_erase_all;
            break;
        default:
            assert(false);
    }


    bank->driver_priv = info;

    return ERROR_OK;
}

//~~~~~~~~ CUT HERE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void _dump_dma_base_registers(struct flash_bank *bank, bool dma1)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint32_t val = 0;
    DUMP_DMA_BASE_REGS(bank->target, val, info->dma);
}

static void _dump_dma_stream_registers(struct flash_bank *bank, bool dma1, int stream)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint32_t val = 0;

    switch (stream) {
        case 0: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 0); break;
        case 1: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 1); break;
        case 2: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 2); break;
        case 3: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 3); break;
        case 4: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 4); break;
        case 5: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 5); break;
        case 6: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 6); break;
        case 7: DUMP_DMA_STREAM_REGS(bank->target, val, info->dma, 7); break;
        default: break;
    }

}

static void _dump_rcc_stream_registers(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint32_t val = 0;
    DUMP_RCC_REGS(bank->target, val, info->rcc);
}

static void _dump_spi_registers(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint16_t val = 0;
    // DUMP_SPI_BASE_REGS(bank->target, val, info->spi);


    struct target *target = bank->target;
    uint32_t address = 0;
    const char *cr1_br[] = {
        " | pCLK/2",
        " | pCLK/4",
        " | pCLK/8",
        " | pCLK/16",
        " | pCLK/32",
        " | pCLK/64",
        " | pCLK/128",
        " | pCLK/256",
    };

    address = SPI_ADDR(info->spi.io_base, cr1);
    target_read_u16(target, address, &val);
    LOG_ERROR("[0x%08x] cr1:    0x%08x %s%s%s%s%s%s%s%s%s%s%s%s%s%s",
        address, val,
        (val & 0x8000) ? " | BIDIMODE" : "",
        (val & 0x4000) ? " | BIDIOE"   : "",
        (val & 0x2000) ? " | CRCEN"    : "",
        (val & 0x1000) ? " | CRCNEXT"  : "",
        (val & 0x0800) ? " | CRCCL"    : "",
        (val & 0x0400) ? " | RXONLY"   : "",
        (val & 0x0200) ? " | SSM"      : "",
        (val & 0x0100) ? " | SSI"      : "",
        (val & 0x0080) ? " | LSBFIRST" : "",
        (val & 0x0040) ? " | SPE"      : "",
        cr1_br[ (val >> 3) & 0x7 ],
        (val & 0x0004) ? " | MSTR"     : "",
        (val & 0x0002) ? " | CPOL"     : "",
        (val & 0x0001) ? " | CPHA"     : ""
    );

    address = SPI_ADDR(info->spi.io_base, cr2);
    target_read_u16(target, address, &val);
    int cr2_ds = ( (val >> 8) & 0xf ) + 1;
    LOG_ERROR("[0x%08x] cr2:    0x%08x %s%s%s | DS(%dbit)%s%s%s%s%s%s%s%s",
        address, val,
        (val & 0x8000) ? " | LDMA_TX" : "",
        (val & 0x4000) ? " | LDMA_RX" : "",
        (val & 0x2000) ? " | FRXTH"   : "",
        cr2_ds,
        (val & 0x0080) ? " | TXEIE"   : "",
        (val & 0x0040) ? " | RXNEIE"  : "",
        (val & 0x0020) ? " | ERRIE"   : "",
        (val & 0x0010) ? " | FRF"     : "",
        (val & 0x0008) ? " | NSSP"    : "",
        (val & 0x0004) ? " | SSOE"    : "",
        (val & 0x0002) ? " | TXDMAEN" : "",
        (val & 0x0001) ? " | RXDMAEN" : ""
    );

    address = SPI_ADDR(info->spi.io_base, sr);
    target_read_u16(target, address, &val);
    LOG_ERROR("[0x%08x] sr:     0x%08x  | FTLVL(%d) | FRLVL(%d) %s%s%s%s%s%s%s%s%s",
        address, val,
        ((val >> 11) & 0x3),
        ((val >>  9) & 0x3),
        (val & 0x0100) ? " | FRE"    : "",
        (val & 0x0080) ? " | BSY"    : "",
        (val & 0x0040) ? " | OVR"    : "",
        (val & 0x0020) ? " | MODF"   : "",
        (val & 0x0010) ? " | CRC_ERR": "",
        (val & 0x0008) ? " | UDR"    : "",
        (val & 0x0004) ? " | CHSIDE" : "",
        (val & 0x0002) ? " | TXE"    : "",
        (val & 0x0001) ? " | RXNE"   : ""
    );
}

COMMAND_HANDLER(stmspi_handle_reg_command)
{
	struct flash_bank *bank = NULL;
    int retval;
    uint16_t val=0;

    // lambda compare
    bool str_compare(const char *arg, const char *etalon) {
        return strncmp(arg, etalon, strlen(arg)) == 0;
    }

	if ( (CMD_ARGC < 2) || (CMD_ARGC > 3) ) {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

	retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK) return retval;

    retval = stmspi_auto_probe(bank);
    if (retval != ERROR_OK) return retval;

    if (str_compare(CMD_ARGV[1], "dma")) {
        if (CMD_ARGC >= 3) {
            COMMAND_PARSE_NUMBER(u16, CMD_ARGV[2], val);
            _dump_dma_stream_registers(bank, false, val);
        } else {
            _dump_dma_base_registers(bank, false);
        }
    } else if (str_compare(CMD_ARGV[1], "spi")) {
        _dump_spi_registers(bank);
    } else if (str_compare(CMD_ARGV[1], "rcc")) {
        _dump_rcc_stream_registers(bank);
    } else {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    return ERROR_OK;
}

//~~~~~~~~ CUT HERE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COMMAND_HANDLER(stmspi_handle_mem_command)
{
	struct flash_bank *bank = NULL;
    struct stmspi_flash_bank *info = NULL;
    int retval;

    // lambda compare
    bool str_compare(const char *arg, const char *etalon) {
        return strncmp(arg, etalon, strlen(arg)) == 0;
    }

	if (CMD_ARGC != 2) {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

	retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK) return retval;

    retval = stmspi_auto_probe(bank);
    if (retval != ERROR_OK) return retval;

    info = bank->driver_priv;
    if (str_compare(CMD_ARGV[1], "id")) {
        LOG_INFO("ID: manufacturer(0x%02x) device(0x%04x)",
                  info->manufacturer, info->device_id);
    } else {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    return ERROR_OK;
}

//~~~~~~~~ CUT HERE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COMMAND_HANDLER(stmspi_handle_mass_erase_command)
{
    struct flash_bank *bank = NULL;
    int retval = ERROR_OK;

    LOG_DEBUG("%s", __func__);

	if (CMD_ARGC != 1) return ERROR_COMMAND_SYNTAX_ERROR;

	retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
	if (retval != ERROR_OK) return retval;

    retval = stmspi_auto_probe(bank);
    if (retval != ERROR_OK) return retval;

    struct stmspi_flash_bank *info = bank->driver_priv;
    return (info->erase_all) ? info->erase_all(bank)
                             : ERROR_NOT_IMPLEMENTED;
}

//~~~~~~~~ CUT HERE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static const struct command_registration stmspi_exec_command_handlers[] = {
    {
        .name    = "reg",
        .handler = stmspi_handle_reg_command,
        .mode    = COMMAND_EXEC,
        .usage   = "bank_id {rcc/spi/dma/dma [0..7]}",
        .help    = "Dump registers: {rcc/spi/dma/dma [0..7]}",
    },
    {
        .name    = "mem",
        .handler = stmspi_handle_mem_command,
        .mode    = COMMAND_EXEC,
        .usage   = "bank_id {id}",
        .help    = "SPI-memory: id (dump memory id)",
    },
    {
        .name    = "mass_erase",
        .handler = stmspi_handle_mass_erase_command,
        .mode    = COMMAND_EXEC,
        .usage   = "bank_id",
        .help    = "Mass erase entire flash device.",
    },
    COMMAND_REGISTRATION_DONE
};

static const struct command_registration stmspi_command_handlers[] = {
    {
        .name  = "stmspi",
        .mode  = COMMAND_ANY,
        .help  = "stmspi flash command group",
        .usage = "",
        .chain = stmspi_exec_command_handlers,
    },
    COMMAND_REGISTRATION_DONE
};


const struct flash_driver stmspi_flash = {
    .name 			    = "stmspi",
    .commands           = stmspi_command_handlers,
    .flash_bank_command = stmspi_flash_bank_command,
    .erase              = stmspi_erase,
    .protect            = stmspi_protect,
    .write              = stmspi_write,
    .read               = stmspi_read,
    .verify             = stmspi_verify,
    .probe              = stmspi_probe,
    .auto_probe         = stmspi_auto_probe,
    .erase_check        = stmspi_erase_check,
    .protect_check      = stmspi_protect_check,
    .info               = stmspi_get_info,
    .free_driver_priv   = default_flash_free_driver_priv,
};
