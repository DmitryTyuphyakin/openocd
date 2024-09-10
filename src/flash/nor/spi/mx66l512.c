#include "mx66l512.h"

#include <assert.h>

#include <helper/log.h>
#include <flash/nor/stmspi.h>

#include "utils.h"


struct jedec_memory_info {
    uint8_t manufacturer;
    uint16_t device;
    uint8_t id_cfi_length;
    uint8_t sector_arch;
    uint8_t family;
} __attribute__((packed));



#define MX66L512_MAX_INSTRUCTION_SIZE  6

#define MX66L512_ERASE_SECTOR_4K 0x1000
#define MX66L512_ERASE_BLOCK_32K 0x8000
#define MX66L512_ERASE_BLOCK_64K 0x10000

#define MX66L512_MANUFACTURER_ID 0xc2
#define MX66L512_DEVICE_ID       0x1a20

#define MX66L512_FLASH_SIZE      0x4000000 // 512 MBits => 64MBytes
#define MX66L512_BLOCK_SIZE      0x10000   // 1024 sectors of 64KBytes
#define MX66L512_SECTOR_SIZE     0x1000    // 16384 subsectors of 4KBytes
#define MX66L512_PAGE_SIZE       0x100     // 262144 pages pf 256 bytes

#define MX66L512_SECTOR_NUMBER  (MX66L512_FLASH_SIZE / MX66L512_SECTOR_SIZE)


/* -= REGISTERS =- */
/* Status Register */
#define MX66L512_SR_WIP_FLAG  (1 << 0)
#define MX66L512_SR_WEL_FLAG  (1 << 1)
#define MX66L512_SR_BP0_FLAG  (1 << 2)
#define MX66L512_SR_BP1_FLAG  (1 << 3)
#define MX66L512_SR_BP2_FLAG  (1 << 4)
#define MX66L512_SR_BP3_FLAG  (1 << 5)
#define MX66L512_SR_QE_FLAG   (1 << 6) // Quad IO mode enabled if =1
#define MX66L512_SR_SRWR_FLAG (1 << 7) // Status register write enable/disable
/* Configuration Register */
#define MX66L512_CR_ODS0_FLAG  (1 << 0) // Output driver strength
#define MX66L512_CR_ODS1_FLAG  (1 << 1) // Output driver strength
#define MX66L512_CR_ODS2_FLAG  (1 << 2) // Output driver strength
#define MX66L512_CR_TB_FLAG    (1 << 3) // Top/Bottom bit used to configure the block protect area
/* #define MX66L512_CR_PBE_FLAG  (1 << 4) // Preamble Bit Enable*/
#define MX66L512_CR_4BYTE_FLAG (1 << 5) // 3-bytes or 4-bytes addressing
#define MX66L512_CR_DC0_FLAG   (1 << 6) // Number of dummy clock cycles
#define MX66L512_CR_DC1_FLAG   (1 << 7) // Number of dummy clock cycles


/* -=OPERATION CODES=- */
// read device id
#define MX66L512_RDID_OPCODE   0x9F
#define MX66L512_QPIID_OPCODE  0xAF
#define MX66L512_RDSFDP_OPCODE 0x5A
// Reset Operations
#define MX66L512_RSTEN_OPCODE  0x66
#define MX66L512_RST_OPCODE    0x99
// Read Operations
#define MX66L512_READ_OPCODE        0x03
#define MX66L512_READ4B_OPCODE      0x13
#define MX66L512_FAST_READ_OPCODE   0x0B
#define MX66L512_FAST_READ4B_OPCODE 0x0C
#define MX66L512_DREAD_OPCODE       0x3B
#define MX66L512_DREAD4B_OPCODE     0x3C
#define MX66L512_2READ_OPCODE       0xBB
#define MX66L512_2READ4B_OPCODE     0xBC
#define MX66L512_QREAD_OPCODE       0x6B
#define MX66L512_QREAD4B_OPCODE     0x6C
#define MX66L512_4READ_OPCODE       0xEB
#define MX66L512_4READ4B_OPCODE     0xEC
// Write Operations
#define MX66L512_WREN_OPCODE 0x06
#define MX66L512_WRDI_OPCODE 0x04
// Register Operations
#define MX66L512_RDSR_OPCODE  0x05
#define MX66L512_RDCR_OPCODE  0x15
#define MX66L512_WRSR_OPCODE  0x01
#define MX66L512_RDLR_OPCODE  0x2D
#define MX66L512_WRLR_OPCODE  0x2C
#define MX66L512_RDEAR_OPCODE 0xC8
#define MX66L512_WREAR_OPCODE 0xC5
// Program Operations
#define MX66L512_PP_OPCODE    0x02
#define MX66L512_PP4B_OPCODE  0x12
#define MX66L512_4PP_OPCODE   0x38
#define MX66L512_4PP4B_OPCODE 0x3E
// Erase Operations
#define MX66L512_SE_OPCODE              0x20
#define MX66L512_SE4B_OPCODE            0x21
#define MX66L512_BE_32K_OPCODE          0x52
#define MX66L512_BE32K4B_OPCODE         0x5C
#define MX66L512_BE_OPCODE              0xD8
#define MX66L512_BE4B_OPCODE            0xDC
#define MX66L512_CE_OPCODE              0xC7
#define MX66L512_RESUME_PGM_ERS_OPCODE  0x30
#define MX66L512_SUSPEND_PGM_ERS_OPCODE 0xB0
// 4-byte Address Mode Operations
#define MX66L512_EN4B_OPCODE 0xB7
#define MX66L512_EX4B_OPCODE 0xE9
// Quad Operations
#define MX66L512_EQIO_OPCODE   0x35
#define MX66L512_RSTQIO_OPCODE 0xF5





static int __poll(struct flash_bank *bank, uint8_t flag, bool state, size_t wait_s)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    int retval = ERROR_OK;
    uint8_t cmd = MX66L512_RDSR_OPCODE;
    uint8_t status;
    time_t stop = time(NULL) + wait_s;
    bool alive = true;
    bool flag_state;

    while (alive) {
        retval = info->xfer(bank, &cmd, 1, NULL, 0, &status, 1);
        if (retval != ERROR_OK) return retval;

        flag_state = (status & flag);
        alive = (flag_state != state);

        if (time(NULL) >= stop) {
            retval = ERROR_FAIL;
            alive = false;
        }
    }

    return retval;
}


static int __write_enable(struct flash_bank *bank, bool enable)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint8_t cmd = (enable) ? MX66L512_WREN_OPCODE
                           : MX66L512_WRDI_OPCODE;
    int retval= info->xfer(bank, &cmd, 1, NULL, 0, NULL, 0);
    if (retval != ERROR_OK) return retval;

    return __poll(bank, MX66L512_SR_WEL_FLAG, enable, 5);
}


static int __erase_sector(struct flash_bank *bank, size_t idx)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct flash_sector *sectors = bank->sectors;
    int retval = ERROR_OK;
    uint8_t cmd[] = {
        MX66L512_SE4B_OPCODE,
        (sectors[idx].offset >> 24) & 0xff,
        (sectors[idx].offset >> 16) & 0xff,
        (sectors[idx].offset >>  8) & 0xff,
        (sectors[idx].offset >>  0) & 0xff,
    };

    LOG_DEBUG("%s: * write enable", __func__);
    retval = __write_enable(bank, true);
    if (retval != ERROR_OK) {
        LOG_ERROR("%s: ** write enable error", __func__);        
        return retval;
    }

    LOG_DEBUG("%s: * Erase sector", __func__);
    retval = info->xfer(bank, cmd, sizeof(cmd), NULL, 0, NULL, 0);
    if (retval != ERROR_OK) {
        LOG_ERROR("%s: ** erase sector error: %u",__func__, retval);
        return retval;
    }

    LOG_DEBUG("%s: * Waiting", __func__);
    retval = __poll(bank, MX66L512_SR_WIP_FLAG, false, 5);
    if (retval != ERROR_OK) {
        LOG_ERROR("%s: ** waiting error: %u",__func__, retval);
        return retval;
    }

    LOG_DEBUG("%s: * set sector %ld erased flag", __func__, idx);
    sectors[idx].is_erased = true;
    
    return retval;
}


static int __read(struct flash_bank *bank, uint8_t *out,
                  uint32_t offset, uint32_t size)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint8_t cmd[] = {
        MX66L512_FAST_READ4B_OPCODE,
        (offset >> 24) & 0xff,
        (offset >> 16) & 0xff,
        (offset >>  8) & 0xff,
        (offset >>  0) & 0xff,
        0 //dummy for flash ready
    };

    return info->xfer(bank, cmd, sizeof(cmd), NULL, 0, out, size);
}


static int __write_page(struct flash_bank *bank, uint32_t address,
                        const uint8_t *in, uint32_t size)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint8_t cmd[5] = {
        MX66L512_PP4B_OPCODE,
        (address >> 24) & 0xff,
        (address >> 16) & 0xff,
        (address >>  8) & 0xff,
        (address >>  0) & 0xff
    };
    int retval = ERROR_OK;

    LOG_DEBUG("%s: * write enable", __func__);
    retval = __write_enable(bank, true);
    if (retval) {
        LOG_ERROR("%s: ** write enable error: 0x%08x", __func__, retval);
        return retval;
    }

    LOG_DEBUG("%s: * write page", __func__);
    retval = info->xfer(bank, cmd, sizeof(cmd), in, size, NULL, 0);
    if (retval) {
        LOG_ERROR("%s: ** write page error: 0x%08x", __func__, retval);
        return retval;
    }

    LOG_DEBUG("%s: * waiting", __func__);
    retval = __poll(bank, MX66L512_SR_WIP_FLAG, false, 10);
    if (retval != ERROR_OK) {
        LOG_ERROR("%s: ** device is busy", __func__);
        return retval;
    }

    return retval;
}






int mx66l512_configure(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct jedec_memory_info jedec_info = {0};
    size_t offset = 0;
    size_t idx = 0;
    uint8_t cmd;
    uint8_t config;
    int retval;

    // Read ID
    cmd = MX66L512_RDID_OPCODE;
    retval = info->xfer(bank, &cmd, 1, NULL, 0, &jedec_info, sizeof(jedec_info));
    if (retval != ERROR_OK) return retval;
    if ( (jedec_info.manufacturer != MX66L512_MANUFACTURER_ID) ||\
         (jedec_info.device != MX66L512_DEVICE_ID)) {
        return ERROR_FAIL;
    }

    // Bind configuration
    bank->size = MX66L512_FLASH_SIZE;
    info->manufacturer = jedec_info.manufacturer;
    info->device_id    = jedec_info.device;
    bank->num_sectors = MX66L512_SECTOR_NUMBER;
    bank->sectors = calloc(bank->num_sectors, sizeof(struct flash_sector));
    if (bank->sectors == NULL) return ERROR_FAIL;

    while (idx<bank->num_sectors) {
        bank->sectors[idx].offset       = offset;
        bank->sectors[idx].size         = MX66L512_SECTOR_SIZE;
        bank->sectors[idx].is_erased    = false;
        bank->sectors[idx].is_protected = 0;

        offset += MX66L512_SECTOR_SIZE;
        idx++;
    }

    // Goto 4byte address mode
    cmd = MX66L512_EN4B_OPCODE;
    retval = info->xfer(bank, &cmd, 1, NULL, 0, NULL, 0);
    if (retval != ERROR_OK) return retval;

    // Check configuration
    cmd = MX66L512_RDCR_OPCODE;
    retval = info->xfer(bank, &cmd, 1, NULL, 0, &config, sizeof(config));
    if (retval != ERROR_OK)                 return retval;
    if (!(config & MX66L512_CR_4BYTE_FLAG)) return ERROR_FAIL;

    return retval;
}



int mx66l512_write(struct flash_bank *bank,
                   const uint8_t *buffer,
                   uint32_t offset,
                   uint32_t count)
{
    struct flash_sector *sector = NULL;
    uint32_t chunk_size = 0;
    uint32_t sector_idx=0;
    uint32_t address = 0;
    size_t page =0;
    size_t page_number = 0;
    size_t page_size = MX66L512_PAGE_SIZE;
    int retval = ERROR_OK;
    bool new_sector = true;

    while (count) {
        sector = &bank->sectors[sector_idx];

        while (offset >= sector->size) {
            offset -= sector->size;
            sector_idx++;
            new_sector = true;
            continue;
        }

        if (offset) {
            // TODO: write sector tail
            offset = 0;
            continue;
        }

        if (new_sector) {
            if (!sector->is_erased) {
                LOG_INFO("Sector(%d), erased: %d", sector_idx, sector->is_erased);
                retval = __erase_sector(bank, sector_idx);
                if (retval) return retval;
            }

            sector->is_erased = false;

            page = 0;
            page_number = sector->size / page_size;
            new_sector  = false;
        }

        address = sector->offset + page * page_size;
        chunk_size = (count > page_size) ? page_size
                                         : count;

        LOG_INFO("%s: offset=0x%08x count=0x%04x", __func__, address, chunk_size);

        // SKip empty page
        if (!filled(buffer, chunk_size, 0xff)) {
            retval = __write_page(bank, address, buffer, chunk_size);
            if (retval) return retval;
        }

        buffer += chunk_size;
        count  -= chunk_size;

        page++;
        if (page >= page_number) {
            sector_idx++;
            new_sector = true;
        }
    }

    return retval;
}



int mx66l512_read(struct flash_bank *bank,
                  uint8_t *out,
                  uint32_t offset,
                  uint32_t size)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    int retval = ERROR_OK;
    uint32_t max_chunk_size = info->cache.size - 0x10; // Reserve for protocol
    uint32_t chunk_size = 0;

    while (size) {
        chunk_size = (size > max_chunk_size) ? max_chunk_size
                                             : size;

        LOG_INFO("%s: offset=0x%08x count=0x%08x", __func__, offset, chunk_size);

        retval = __read(bank, out, offset, chunk_size);
        if (retval != ERROR_OK) return retval;

        size   -= chunk_size;
        offset += chunk_size;
        out    += chunk_size;
    }

    return retval;
}



int mx66l512_erase_all(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    int retval = 0;
    uint8_t cmd = 0;

    LOG_INFO("%s: erase flash bank %d", __func__, bank->bank_number);


    LOG_DEBUG("%s: * write enable: on", __func__);
    retval = __write_enable(bank, true);
    if (retval) {
        LOG_ERROR("%s: write enable error: 0x%08x", __func__, retval);
        return retval;
    }

    // Bulk errase
    LOG_DEBUG("%s: * send bulk erase", __func__);
    cmd = MX66L512_CE_OPCODE;
    retval = info->xfer(bank, &cmd, 1, NULL, 0, NULL, 0);
    if (retval) {
        LOG_ERROR("%s: bulk erase error: 0x%08x", __func__, retval);
        return retval;
    }

    // Wait
    LOG_DEBUG("%s: * waiting", __func__);
    retval = __poll(bank, MX66L512_SR_WIP_FLAG, false, 180);
    if (retval) {
        LOG_ERROR("%s: waiting error: 0x%08x", __func__, retval);
        return retval;
    }

    LOG_DEBUG("%s: * update sector erased status", __func__);
    for (size_t i=0; i<bank->num_sectors; i++) {
        bank->sectors[i].is_erased = true;
    }

    return retval;
}



int mx66l512_erase(struct flash_bank *bank,
                   unsigned int first,
                   unsigned int last)
{
    int retval = ERROR_OK;

    if (first > bank->num_sectors) return ERROR_OK;
    if (last  > bank->num_sectors) last = bank->num_sectors;

    for (size_t i=first; i<last; i++) {
        LOG_INFO("%s: sector=%ld", __func__, i);
        retval = __erase_sector(bank, i);
        if (retval) return retval;
    }

    return retval;
}
