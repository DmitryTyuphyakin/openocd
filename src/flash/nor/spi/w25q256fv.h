#pragma once

#define W25Q256FV_SIZE (32 * 1024 * 1024)

#define W25Q256FV_MANUFACTURER_ID  0xef
#define W25Q256FV_DEVICE_ID        0x1940


#define W25Q64FL_SECTOR_NUMBER  2048
#define W25Q64FL_SECTOR_SIZE    (4*1024)
#define W25Q64FL_PAGE_SIZE      256



/*
 * REGISTERS
 */

// SR1 register
#define W25Q256FV_SR1_BUSY_FLAG  (1 << 0) //< Erase/Write in progress
#define W25Q256FV_SR1_WEL_FLAG   (1 << 1) //< Write Enable Latch
#define W25Q256FV_SR1_BP0_FLAG   (1 << 2) //< Block protect 0
#define W25Q256FV_SR1_BP1_FLAG   (1 << 3) //< Block protect 1
#define W25Q256FV_SR1_BP2_FLAG   (1 << 4) //< Block protect 2
#define W25Q256FV_SR1_TB_FLAG    (1 << 5) //< Top/Buttom Protect
#define W25Q256FV_SR1_SEC_FLAG   (1 << 6) //< Sector Protect
#define W25Q256FV_SR1_SRP0_FLAG  (1 << 7) //< Status Register Protect 0

// SR2 register
#define W25Q256FV_SR2_SRP1_FLAG (1 << 0) //< Status Register Protect 1
#define W25Q256FV_SR2_QE_FLAG   (1 << 1) //< Quad Enable
#define W25Q256FV_SR2_R_FLAG    (1 << 2) //< Reserved
#define W25Q256FV_SR2_LB1_FLAG  (1 << 3) //< Security Register Lock Bits 1
#define W25Q256FV_SR2_LB2_FLAG  (1 << 4) //< Security Register Lock Bits 2
#define W25Q256FV_SR2_LB3_FLAG  (1 << 5) //< Security Register Lock Bits 3
#define W25Q256FV_SR2_CMP_FLAG  (1 << 6) //< Complement Protect
#define W25Q256FV_SR2_SUS_FLAG  (1 << 7) //< Suspend status


/*
 * OPERATION CODES
 */

// read device id
#define W25Q256FV_RDID_OPCODE 0x9f

// register access
#define W25Q256FV_RDSR1_OPCODE  0x05
#define W25Q256FV_RDSR2_OPCODE  0x35
#define W25Q256FW_4BAME_OPCODE  0xb7

#define W25Q256FV_WRDI_OPCODE   0x04
#define W25Q256FV_WREN_OPCODE   0x06

// read flash array
#define W25Q256FV_4READ_OPCODE      0x13
#define W25Q256FV_4FAST_READ_OPCODE 0x0c

// program flash array
#define W25Q256FV_PP_OPCODE     0x02

// erase flash array
#define W25Q256FV_P4E_OPCODE   0x20
#define W25Q256FV_BE_OPCODE    0x60

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <flash/nor/core.h>


int w25q256fv_configure(struct flash_bank *bank);

int w25q256fv_write(struct flash_bank *bank,
                    const uint8_t *buffer,
                    uint32_t offset,
                    uint32_t count);

int w25q256fv_read(struct flash_bank *bank,
                   uint8_t *out,
                   uint32_t offset,
                   uint32_t size);

int w25q256fv_erase_all(struct flash_bank *bank);

int w25q256fv_erase(struct flash_bank *bank,
                    unsigned int first,
                    unsigned int last);
