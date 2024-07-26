#pragma once

#define S25FL256_SIZE (32 * 1024 * 1024)
#define S25FL_ID_SIZE 8

#define S25FL_MANUFACTURER_ID           0x01
#define S25FL_DEVICE_ID                 0x1902
#define S25FL_SECTOR_ARCH_UNIFORM       0


#define S25FL_PARAMETER_SECTOR_NUMBER   32
#define S25FL_PARAMETER_SECTOR_SIZE     (4 * 1024)

#define S25FL_SECTOR_NUMBER             510
#define S25FL_SECTOR_SIZE               (64 * 1024)
#define S25FL_HYBRID_PAGE_SIZE          256


#define S25FL_UNIFORM_SECTOR_NUMBER     128
#define S25FL_UNIFORM_SECTOR_SIZE       (256 * 1024)
#define S25FL_UNIFORM_PAGE_SIZE         512



#define S25FL_MAX_INSTRUCTION_SIZE      6



/*
 * REGISTERS
 */

// SR1 register
#define S25FL_SR1_WIP_FLAG   (1 << 0)
#define S25FL_SR1_WEL_FLAG   (1 << 1)
#define S25FL_SR1_BP0_FLAG   (1 << 2)
#define S25FL_SR1_BP1_FLAG   (1 << 3)
#define S25FL_SR1_BP2_FLAG   (1 << 4)
#define S25FL_SR1_E_ERR_FLAG (1 << 5)
#define S25FL_SR1_P_ERR_FLAG (1 << 6)
#define S25FL_SR1_SRWD_FLAG  (1 << 7)



// SR2 register
#define S25FL_SR2_PS_FLAG (1 << 0)
#define S25FL_SR2_ES_FLAG (1 << 1)

// CR1 register
#define S25FL_CR1_FREEZE_FLAG (1 << 0)
#define S25FL_CR1_QUAD_FLAG   (1 << 1)
#define S25FL_CR1_TBPARM_FLAG (1 << 2)
#define S25FL_CR1_BPNV_FLAG   (1 << 3)
#define S25FL_CR1_RFU_FLAG    (1 << 4)
#define S25FL_CR1_TBPROT_FLAG (1 << 5)
#define S25FL_CR1_LC0_FLAG    (1 << 6)
#define S25FL_CR1_LC1_FLAG    (1 << 7)

/*
 * OPERATION CODES
 */

// read device id
#define S25FL_REMS_OPCODE 0x90
#define S25FL_RDID_OPCODE 0x9f
#define S25FL_RES_OPCODE  0xab

// register access
#define S25FL_RDSR1_OPCODE  0x05
#define S25FL_RDSR2_OPCODE  0x07
#define S25FL_RDCR_OPCODE   0x35
#define S25FL_WRR_OPCODE    0x01
#define S25FL_WRDI_OPCODE   0x04
#define S25FL_WREN_OPCODE   0x06
#define S25FL_CLSR_OPCODE   0x30
#define S25FL_ABRD_OPCODE   0x14
#define S25FL_ABWR_OPCODE   0x15
#define S25FL_BRRD_OPCODE   0x16
#define S25FL_BRWR_OPCODE   0x17
#define S25FL_BRAC_OPCODE   0xb9
#define S25FL_DLPRD_OPCODE  0x41
#define S25FL_PNVDLR_OPCODE 0x43
#define S25FL_WVDLR_OPCODE  0x4a


// read flash array
#define S25FL_READ_OPCODE       0x03
#define S25FL_4READ_OPCODE      0x13
#define S25FL_FAST_READ_OPCODE  0x0b
#define S25FL_4FAST_READ_OPCODE 0x0c
#define S25FL_DDRFR_OPCODE      0x0d
#define S25FL_4DDRFR_OPCODE     0x0e
#define S25FL_DOR_OPCODE        0x3b
#define S25FL_4DOR_OPCODE       0x3c
#define S25FL_QOR_OPCODE        0x6b
#define S25FL_4QOR_OPCODE       0x6c
#define S25FL_DIOR_OPCODE       0xbb
#define S25FL_4DIOR_OPCODE      0xbc
#define S25FL_DDRDIOR_OPCODE    0xbd
#define S25FL_4DDRDIOR_OPCODE   0xbe
#define S25FL_QIOR_OPCODE       0xeb
#define S25FL_4QIOR_OPCODE      0xec
#define S25FL_DDRQIOR_OPCODE    0xed
#define S25FL_4DDRQIOR_OPCODE   0xee


// program flash array
#define S25FL_PP_OPCODE     0x02
#define S25FL_4PP_OPCODE    0x12
#define S25FL_QPP_OPCODE    0x32
#define S25FL_QPP_AI_OPCODE 0x38
#define S25FL_4QPP_OPCODE   0x34
#define S25FL_PGSP_OPCODE   0x85
#define S25FL_PGRS_OPCODE   0x8a

// erase flash array
#define S25FL_P4E_OPCODE   0x20
#define S25FL_4P4E_OPCODE  0x21
#define S25FL_BE_OPCODE    0x60
#define S25FL_BE_AI_OPCODE 0xc7
#define S25FL_SE_OPCODE    0xd8
#define S25FL_4SE_OPCODE   0xdc
#define S25FL_ERSP_OPCODE  0x75
#define S25FL_ERRS_OPCODE  0x7a



// otp array
#define S25FL_OTPP_OPCODE 0x42
#define S25FL_OTPR_OPCODE 0x4b

// advanced sector protection
#define S25FL_DYBRD_OPCODE  0xe0
#define S25FL_DYBWR_OPCODE  0xe1
#define S25FL_PPBRD_OPCODE  0xe2
#define S25FL_PPBP_OPCODE   0xe3
#define S25FL_PPBE_OPCODE   0xe4
#define S25FL_ASPRD_OPCODE  0x2b
#define S25FL_ASPP_OPCODE   0x2f
#define S25FL_PLBRD_OPCODE  0xa7
#define S25FL_PLBWR_OPCODE  0xa6
#define S25FL_PASSRD_OPCODE 0xe7
#define S25FL_PASSP_OPCODE  0xe8
#define S25FL_PASSU_OPCODE  0xe9

// reset
#define S25FL_RESET_OPCODE 0xf0
#define S25FL_MBR_OPCODE   0xff

// reserved
#define S25FL_MPM_OPCODE 0xa3


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <flash/nor/core.h>


int s25fl_configure(struct flash_bank *bank);

int s25fl_write(struct flash_bank *bank,
                const uint8_t *buffer,
                uint32_t offset,
                uint32_t count);

int s25fl_read(struct flash_bank *bank,
               uint8_t *out,
               uint32_t offset,
               uint32_t size);

int s25fl_erase_all(struct flash_bank *bank);

int s25fl_erase(struct flash_bank *bank,
                unsigned int first,
                unsigned int last);
