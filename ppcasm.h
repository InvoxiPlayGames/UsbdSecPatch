/*
    ppcasm.h by InvoxiPlayGames, 2021
    WTFPL License
*/

#include <stdint.h>

// Poke values
#define POKE_8(addr, val)   do { *(uint8_t*)(addr) = (uint8_t)(val); } while (0)
#define POKE_16(addr, val)  do { *(uint16_t*)(addr) = (uint16_t)(val); } while (0)
#define POKE_32(addr, val)  do { *(uint32_t*)(addr) = (uint32_t)(val); } while (0)

// PowerPC instructions!
#define ADDI(dest, src, val)  (0x38000000 + ((uint8_t)dest << 21) + ((uint8_t)src << 16) + ((uint16_t)val))
#define ADDIS(dest, src, val) (0x3C000000 + ((uint8_t)dest << 21) + ((uint8_t)src << 16) + ((uint16_t)val))
#define ORI(dest, src, val)   (0x60000000 + ((uint8_t)dest << 21) + ((uint8_t)src << 16) + ((uint16_t)val))
#define ORIS(dest, src, val)  (0x64000000 + ((uint8_t)dest << 21) + ((uint8_t)src << 16) + ((uint16_t)val))
#define B(dest, src)          (0x48000000 + (((uint32_t)(dest) - (uint32_t)(src)) & 0x3ffffff))

// PowerPC shorthand instructions!
#define LI(dest, val)  ADDI(dest, 0, val)
#define LIS(dest, val) ADDIS(dest, 0, val)
#define BL(dest, src)  B(dest, src) + 1
#define NOP            ORI(0, 0, 0)

// PowerPC instructions 2: This time it's hardcoded
#define BLR   0x4e800020
#define BCTR  0x4e800420
#define BLRL  BLR+1
#define BCTRL BCTR+1

// Poke branches
#define POKE_B(addr, dest)  POKE_32(addr, B(dest, addr))
#define POKE_BL(addr, dest) POKE_32(addr, BL(dest, addr))