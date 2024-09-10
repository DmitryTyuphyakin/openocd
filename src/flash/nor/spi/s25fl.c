#include "s25fl.h"

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



static int __poll_busy(struct flash_bank *bank, size_t wait_s)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    int retval = ERROR_OK;
    uint8_t cmd[1] = {0};
    uint8_t status;
    time_t stop = time(NULL) + wait_s;
    bool alive = true;

    while (alive) {
        cmd[0] = S25FL_RDSR1_OPCODE;
        retval = info->xfer(bank, cmd, 1, NULL, 0, &status, 1);
        if (retval != ERROR_OK) return retval;

        alive = status & S25FL_SR1_WIP_FLAG;

        if (time(NULL) >= stop) {
            retval = ERROR_FAIL;
            alive = false;
        }
    }

    return retval;
}


static bool __check_status(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint8_t cmd = S25FL_RDSR1_OPCODE;
    uint8_t status = 0;

    int retval = info->xfer(bank, &cmd, 1, NULL, 0, &status, 1);
    bool error = status & (S25FL_SR1_P_ERR_FLAG | S25FL_SR1_E_ERR_FLAG);

    return  (retval == ERROR_OK) && (!error);
}


static int __write_enable(struct flash_bank *bank, bool enable)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    uint8_t cmd = (enable) ? S25FL_WREN_OPCODE
                           : S25FL_WRDI_OPCODE;
    int retval= info->xfer(bank, &cmd, 1, NULL, 0, NULL, 0);
    if (retval != ERROR_OK) return retval;

    return __poll_busy(bank, 5);
}


static int __erase_sector(struct flash_bank *bank, size_t idx)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct flash_sector *sectors = bank->sectors;
    int retval = ERROR_OK;
    uint8_t opcode = (sectors[idx].size > S25FL_PARAMETER_SECTOR_SIZE)
        ? S25FL_4SE_OPCODE
        : S25FL_4P4E_OPCODE;
    uint8_t cmd[] = {
        opcode,
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
    retval = __poll_busy(bank, 5);
    if (retval != ERROR_OK) {
        LOG_ERROR("%s: ** waiting error: %u",__func__, retval);
        return retval;
    }

    LOG_DEBUG("%s: * Check status", __func__);
    if (!__check_status(bank)) {
        retval = ERROR_FAIL;
        LOG_ERROR("%s: ** status is not clear",__func__);
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
        S25FL_4FAST_READ_OPCODE,
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
        S25FL_4PP_OPCODE,
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
    retval = __poll_busy(bank, 10);
    if (retval != ERROR_OK) {
        LOG_ERROR("%s: ** device is busy", __func__);
        return retval;
    }

    LOG_DEBUG("%s: * check status", __func__);
    if (!__check_status(bank)) {
        LOG_ERROR("%s: ** status is not clear",__func__);
        return ERROR_FAIL;
    }

    return retval;
}



//----------------


int s25fl_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
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


int s25fl_erase_all(struct flash_bank *bank)
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
    cmd = S25FL_BE_OPCODE;
    retval = info->xfer(bank, &cmd, 1, NULL, 0, NULL, 0);
    if (retval) {
        LOG_ERROR("%s: bulk erase error: 0x%08x", __func__, retval);
        return retval;
    }

    // Wait
    LOG_DEBUG("%s: * waiting", __func__);
    retval = __poll_busy(bank, 180);
    if (retval) {
        LOG_ERROR("%s: waiting error: 0x%08x", __func__, retval);
        return retval;
    }

    // Check status
    LOG_DEBUG("%s: * check status", __func__);
    if (!__check_status(bank)) {
        retval = ERROR_FAIL;
        LOG_ERROR("%s: incorrect status", __func__);
        return retval;
    }

    LOG_DEBUG("%s: * update sector erased status", __func__);
    for (size_t i=0; i<bank->num_sectors; i++) {
        bank->sectors[i].is_erased = true;
    }

    return retval;
}


int s25fl_read(struct flash_bank *bank, uint8_t *out,
                      uint32_t offset, uint32_t size)
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

int s25fl_write(struct flash_bank *bank, const uint8_t *buffer,
                       uint32_t offset, uint32_t count)
{
    struct flash_sector *sector = NULL;
    uint32_t chunk_size = 0;
    uint32_t sector_idx=0;
    uint32_t address = 0;
    size_t page =0;
    size_t page_number = 0;
    size_t page_size = 0;
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
            page_size = S25FL_HYBRID_PAGE_SIZE; // TODO: checkup!
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


int s25fl_configure(struct flash_bank *bank)
{
    struct stmspi_flash_bank *info = bank->driver_priv;
    struct jedec_memory_info jedec_info = {0};
    size_t offset = 0;
    size_t idx = 0;
    int retval = ERROR_OK;
    uint8_t cmd[8] = {0};

    // Check flash exists
    cmd[0] = S25FL_RDID_OPCODE;
    retval = info->xfer(bank, cmd, 1, NULL, 0, &jedec_info, sizeof(jedec_info));
    if (retval != ERROR_OK) return retval;

    if ( (jedec_info.manufacturer != S25FL_MANUFACTURER_ID) ||\
         (jedec_info.device != S25FL_DEVICE_ID)) {
        return ERROR_FAIL;
    }


    // Bind configuration
    bank->size = S25FL256_SIZE;
    info->manufacturer = jedec_info.manufacturer;
    info->device_id    = jedec_info.device;

    if (jedec_info.sector_arch == S25FL_SECTOR_ARCH_UNIFORM) {
        bank->num_sectors = S25FL_UNIFORM_SECTOR_NUMBER;
        bank->sectors = calloc(bank->num_sectors, sizeof(struct flash_sector));
        if (bank->sectors == NULL) return ERROR_FAIL;

        while (idx<bank->num_sectors) {
            bank->sectors[idx].offset       = offset;
            bank->sectors[idx].size         = S25FL_UNIFORM_SECTOR_SIZE;
            bank->sectors[idx].is_erased    = false;
    		bank->sectors[idx].is_protected = 0;

            offset += S25FL_UNIFORM_SECTOR_SIZE;
            idx++;
        }
    } else {
        bank->num_sectors = S25FL_PARAMETER_SECTOR_NUMBER + S25FL_SECTOR_NUMBER;
        bank->sectors = calloc(bank->num_sectors, sizeof(struct flash_sector));
        if (bank->sectors == NULL) return ERROR_FAIL;

        while (idx<S25FL_PARAMETER_SECTOR_NUMBER) {
            bank->sectors[idx].offset = offset;
            bank->sectors[idx].size = S25FL_PARAMETER_SECTOR_SIZE;
            bank->sectors[idx].is_erased = false;
    		bank->sectors[idx].is_protected = 0;

            offset += S25FL_PARAMETER_SECTOR_SIZE;
            idx++;
        }

        while (idx<bank->num_sectors) {
            bank->sectors[idx].offset = offset;
            bank->sectors[idx].size = S25FL_SECTOR_SIZE;
            bank->sectors[idx].is_erased = false;
    		bank->sectors[idx].is_protected = 0;

            offset += S25FL_SECTOR_SIZE;
            idx++;
        }
    }

    // Clear status register
    cmd[0] = S25FL_CLSR_OPCODE;
    retval = info->xfer(bank, cmd, 1, NULL, 0, NULL, 0);
    if (retval) return retval;

    retval = __poll_busy(bank, 2);

    return retval;
}


