#include "utils.h"

#include <string.h>


bool filled(const void *memory, size_t size, uint8_t val)
{
    const uint8_t *mm = (const uint8_t*)memory;

    bool first_ok  = (mm[0] == val);
    bool others_ok = (memcmp(mm, mm+1, size-1) == 0);

    return first_ok && others_ok;
}
