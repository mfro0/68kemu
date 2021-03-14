/*
  m68kinl.h
  Written in 2010-2012 by Vincent Riviere <vincent.riviere@freesbee.fr>

  This file is part of:
  68Kemu - A CPU emulator for Atari TOS computers
  http://vincent.riviere.free.fr/soft/68kemu/

  To the extent possible under law, the author(s) have dedicated all copyright
  and related and neighboring rights to this software to the public domain
  worldwide. This software is distributed without any warranty.

  You should have received a copy of the CC0 Public Domain Dedication along
  with this software.
  If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef __INC_M68KINL_H__
#define __INC_M68KINL_H__

#include "debug.h"

#define LOWMEM  0x800

INLINE unsigned int m68k_read_memory_8(unsigned int address)
{
    if ((unsigned) address > LOWMEM)
        return * (unsigned char *) address;
    else
        dbg("attempt to read low memory\r\n");

    return 0;
}

INLINE unsigned int m68k_read_memory_16(unsigned int address)
{
    if ((unsigned) address > LOWMEM)
        return * (unsigned short *) address;
    else
        dbg("attempt to read low memory\r\n");

    return 0;
}

INLINE unsigned int m68k_read_memory_32(unsigned int address)
{
    if ((unsigned) address > LOWMEM)
        return * (unsigned long *) address;
    else
        dbg("attempt to read low memory\r\n");

    return 0;
}

INLINE unsigned int m68k_read_disassembler_8(unsigned int address)
{
    if ((unsigned) address > LOWMEM)
        return * (unsigned char *) address;
    else
        dbg("attempt to read low memory to disassembler\r\n");

    return * (unsigned char *) address;
}

INLINE unsigned int m68k_read_disassembler_16 (unsigned int address)
{
    if ((unsigned) address > LOWMEM)
        return * (unsigned short *) address;
    else
        dbg("attempt to read low memory to disassembler\r\n");

    return * (unsigned short *) address;
}

INLINE unsigned int m68k_read_disassembler_32 (unsigned int address)
{
    if ((unsigned) address > LOWMEM)
        return * (unsigned long *) address;
    else
        dbg("attempt to read low memory to disassembler\r\n");

    return * (unsigned long *) address;
}

INLINE void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if ((unsigned) address > LOWMEM)
    {
        * (unsigned char*) address = (char) value;
        return;
    }
    else
        dbg("attempt to write to low memory\r\n");
}

INLINE void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if ((unsigned) address > LOWMEM)
    {
        * (unsigned short *) address = (short) value;
        return;
    }
    else
        dbg("attempt to write to low memory\r\n");
}

INLINE void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    if ((unsigned) address > LOWMEM)
    {
        * (unsigned long *) address = (long) value;

        return;
    }
    else
        dbg("attempt to write to low memory\r\n");
}

#endif /* __INC_M68KINL_H__ */
