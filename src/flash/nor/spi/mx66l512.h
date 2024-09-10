#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <flash/nor/core.h>


int mx66l512_configure(struct flash_bank *bank);

int mx66l512_write(struct flash_bank *bank,
                   const uint8_t *buffer,
                   uint32_t offset,
                   uint32_t count);

int mx66l512_read(struct flash_bank *bank,
                  uint8_t *out,
                  uint32_t offset,
                  uint32_t size);

int mx66l512_erase_all(struct flash_bank *bank);

int mx66l512_erase(struct flash_bank *bank,
                   unsigned int first,
                   unsigned int last);
