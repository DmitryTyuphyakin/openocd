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



FLASH_BANK_COMMAND_HANDLER(stmspi_flash_bank_command)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

COMMAND_HANDLER(stmspi_handle_mass_erase_command)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

COMMAND_HANDLER(stmspi_handle_set)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

COMMAND_HANDLER(stmspi_handle_cmd)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}



static int stmspi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int stmspi_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int stmspi_blank_check(struct flash_bank *bank)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int stmspi_read(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count)
{
    LOG_DEBUG("%s: offset=0x%08" PRIx32 " count=0x%08" PRIx32, __func__, offset, count);
    return ERROR_OK;
}

static int stmspi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    LOG_DEBUG("%s: offset=0x%08" PRIx32 " count=0x%08" PRIx32, __func__, offset, count);
	return ERROR_OK;
}

static int stmspi_verify(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    LOG_DEBUG("%s: offset=0x%08" PRIx32 " count=0x%08" PRIx32, __func__, offset, count);
    return ERROR_OK;
}

static int stmspi_probe(struct flash_bank *bank)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int stmspi_auto_probe(struct flash_bank *bank)
{
    LOG_DEBUG("%s", __func__);
    return stmspi_probe(bank);
}

static int stmspi_protect_check(struct flash_bank *bank)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int stmspi_get_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}



static const struct command_registration stmspi_exec_command_handlers[] = {
    {
        .name    = "mass_erase",
        .handler = stmspi_handle_mass_erase_command,
        .mode    = COMMAND_EXEC,
        .usage   = "bank_id",
        .help    = "Mass erase entire flash device.",
    },
    {
        .name    = "set",
        .handler = stmspi_handle_set,
        .mode    = COMMAND_EXEC,
        .usage   = "bank_id name chip_size page_size read_cmd qread_cmd pprg_cmd "
            "[ mass_erase_cmd ] [ sector_size sector_erase_cmd ]",
        .help    = "Set params of single flash chip",
    },
    {
        .name    = "cmd",
        .handler = stmspi_handle_cmd,
        .mode    = COMMAND_EXEC,
        .usage   = "bank_id num_resp cmd_byte ...",
        .help    = "Send low-level command cmd_byte and following bytes or read num_resp.",
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
    .erase_check        = stmspi_blank_check,
    .protect_check      = stmspi_protect_check,
    .info               = stmspi_get_info,
    .free_driver_priv   = default_flash_free_driver_priv,
};
