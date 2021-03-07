#ifndef M68KSUBR_H
#define M68KSUBR_H

#include "m68k.h"
#include "m68kcpu.h"
#include "m68kops.h"

#define RETURN_STACK_MARKER     0xaffeaffe

extern int m68k_execute_subroutine(long sp, long subroutine);
#endif // M68KSUBR_H

