/*
  68kemu.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include "musashi/m68k.h"
#include "musashi/m68kcpu.h"

#include "userdefs.h"
#include <gem.h>

#include "debug.h"

static void* old_ssp_real;

/*
 * bring both the real *and* emulated processor from user mode to supervisor mode
 */
static void* BothSuperFromUser(void *new_ssp_emu)
{
    unsigned short sr;
    void* old_ssp_emu;

    // Switch the real CPU to supervisor mode
    old_ssp_real = (void *) Super(SUP_SET);

    // Switch the emulated CPU to supervisor mode
    if (new_ssp_emu == NULL)
        new_ssp_emu = (void *) m68k_get_reg(NULL, M68K_REG_SP);

    sr = (unsigned short) m68k_get_reg(NULL, M68K_REG_SR);
    sr |= 0x2000;
    m68k_set_reg(M68K_REG_SR, sr);

    old_ssp_emu = (void *) m68k_get_reg(NULL, M68K_REG_SP);
    m68k_set_reg(M68K_REG_SP, (int) new_ssp_emu);
    m68k_set_reg(M68K_REG_D0, (int) old_ssp_emu);

    return old_ssp_emu;
}

/*
 * bring both the real *and* emulated processor from supervisor mode to user mode
 */
static void BothSuperToUser(void *old_ssp_emu)
{
    unsigned short sr;

    // Switch the real CPU to user mode
    SuperToUser(old_ssp_real);

    m68k_set_reg(M68K_REG_SP, (int) old_ssp_emu);

    sr = (unsigned short) m68k_get_reg(NULL, M68K_REG_SR);
    sr &= ~0x2000;
    m68k_set_reg(M68K_REG_SR, sr);

    m68k_set_reg(M68K_REG_D0, (int) 0);
}

void m68ki_hook_trap1()
{
    unsigned short *sp = (unsigned short *) m68k_get_reg(NULL, M68K_REG_SP);
    unsigned short num = *sp;

    dbg("GEMDOS(0x%02x)\n", num);

    if (num == 0x20)
    {
        void *param = * (void **)(sp + 1);
        dbg("Super(0x%08lx)\n", (long) param);

        if (param != (void *) 1)
        {
            long current_super = Super(SUP_INQUIRE);
            if (current_super)
            {
                BothSuperToUser(param);
                return;
            }
            else /* current user */
            {
                BothSuperFromUser(param);
                return;
            }
        }
    }

    // Standard block
    {
        register long reg_d0 __asm__("d0");

        __asm__ volatile
        (
            "move.l	sp,a3\n\t"
            "move.l	%[sp],sp\n\t"
            "trap	#1\n\t"
            "move.l	a3,sp"
        : "=r"(reg_d0)                                      /* outputs */
        : [sp] "g"(sp)
        : "d1", "d2", "a0", "a1", "a2", "a3", "memory"      /* clobbered regs */
        );

        m68k_set_reg(M68K_REG_D0, (int) reg_d0);
        dbg("return value=%d\r\n", reg_d0);
    }
}

void m68ki_hook_trap2()
{
    void *sp = (void *) m68k_get_reg(NULL, M68K_REG_SP);
    unsigned long ad0 = (unsigned long) m68k_get_reg(NULL, M68K_REG_D0);
    unsigned long ad1 = (unsigned long) m68k_get_reg(NULL, M68K_REG_D1);

    /*
     * need to intercept objc_draw(), objc_change() and menu_bar() calls
     * since these might bring USERDEFS with them
     */

    if ((short) ad0 == 200)     /* is this an AES call? */
    {
        /*
         * yes, retrieve parameter arrays
         */
        AESPB *pb = (AESPB *) ad1;

#define FN_MENU_BAR     30
#define FN_OBJC_ADD     40
#define FN_OBJC_DRAW    42
#define FN_OBJC_CHANGE  47

        if (pb->control[0] == FN_OBJC_ADD)
            dbg("\r\nobjc_add() call detected\r\n\r\n");

        /*
         * objc_draw(), objc_change() or menu_bar() call?
         */
        if (pb->control[0] == FN_OBJC_DRAW || pb->control[0] == FN_OBJC_CHANGE
                || pb->control[0] == FN_MENU_BAR)
        {
            OBJECT *tree;
            short obj;
            short depth;

            tree = (OBJECT *) pb->addrin[0];        /* object tree */

            switch (pb->control[0])
            {
                case FN_OBJC_DRAW:
                    dbg("detected objc_draw() call\r\n");
                    obj = pb->intin[0];             /* start object index */
                    depth = pb->intin[1];           /* drawing depth */
                    break;

                case FN_OBJC_CHANGE:
                    dbg("detected objc_change() call\r\n");
                    obj = pb->intin[0];
                    depth = 0;
                    break;

                case FN_MENU_BAR:
                    dbg("detected menu_bar() call\r\n");
                    //if (pb->intin[0] != 1)          /* draw the bar */
                    //    return;
                    obj = 0;
                    depth = 20;
                    break;
            }

            fix_userdefs(tree, obj, depth);
        }
    }

    //printf("GEM\n");
    __asm__ volatile
    (
        "move.l	sp,a3                   \n\t"
        "move.l	%[ad0],d0               \n\t"
        "move.l	%[ad1],d1               \n\t"
        "move.l	%[sp],sp                \n\t"
        "trap	#2                      \n\t"
        "move.l	a3,sp                   \n\t"
    : "=g" (ad0)                                                      /* outputs */
    : [ad0] "g"(ad0), [ad1] "g"(ad1), [sp] "g"(sp)
    : "d0", "d1", "d2", "a0", "a1", "a2", "a3", "memory"    /* clobbered regs */
    );

    m68k_set_reg(M68K_REG_D0, (int) ad0);
    dbg("return value=%d\r\n", ad0);
}

static void setexc_hook(void)
{

}

void m68ki_hook_trap13()
{
    unsigned short* sp = (unsigned short *)m68k_get_reg(NULL, M68K_REG_SP);
    unsigned short num = *sp;
    register long reg_d0 __asm__("d0");
    unsigned short low, hi;

    printf("BIOS(0x%02x)\n", num);
    if (num == 5)                       /* this is a Setexc() call */
    {
        if ((low = sp[2]) == 0xffff && (hi = sp[3]) == 0xffff)
        {
            /* just inquire - pass */
            dbg("Setexc() - just inquire: pass\r\n");
        }
        else
        {
            long new_vector = hi << 16 | low;
            dbg("Setexc(): attempt to reroute vector %d to %p\r\n",
                sp[0], new_vector);
        }
    }

    __asm__ volatile
    (
        "move.l	sp,a3\n\t"
        "move.l	%1,sp\n\t"
        "trap	#13\n\t"
        "move.l	a3,sp"
    : "=r"(reg_d0)			/* outputs */
    : "g"(sp)
    : "d1", "d2", "a0", "a1", "a2", "a3", "memory"    /* clobbered regs */
    );

    m68k_set_reg(M68K_REG_D0, (int)reg_d0);
}

typedef void VOIDFUNC(void);
/*
VOIDFUNC* nextCallback = NULL;

// This callback is called from the real CPU
void generic_callback(void)
{
    if (nextCallback == NULL)
        abort();

    // TODO
}
*/

typedef unsigned long SUPEXEC_CALLBACK_TYPE(void);
typedef unsigned long SUPEXEC_TYPE(SUPEXEC_CALLBACK_TYPE* f);
/*
static unsigned long RunEmulatedFunction(SUPEXEC_TYPE* f, SUPEXEC_CALLBACK_TYPE* param)
{
    return f(param);
}
*/
//unsigned long SupexecImpl(SUPEXEC_CALLBACK_TYPE* f);
void SupexecImpl(void);

/*
// This function is run on the emulated CPU
unsigned long SupexecImpl(SUPEXEC_CALLBACK_TYPE* f)
{
    long from_super = SuperInquire();
    void* old_ssp;
    unsigned long ret;

    if (!from_super)
        old_ssp = SuperFromUser();

    ret = f();

    if (!from_super)
        SuperToUser(old_ssp);

    return ret;
}
*/
void m68ki_hook_trap14()
{
    unsigned char* sp = (unsigned char *)m68k_get_reg(NULL, M68K_REG_SP);
    unsigned short num = *(unsigned short*)sp;
    register long reg_d0 __asm__("d0");

    //printf("XBIOS(0x%02x)\n", num);

    if (num == 0x26)
    {
        //SUPEXEC_CALLBACK_TYPE* pCallback = *(SUPEXEC_CALLBACK_TYPE**)(sp + 2);
        //unsigned long ret;
        unsigned long* sp;
        void* pc;

        //printf("Supexec(0x%08lx)\n", (unsigned long)pCallback);
        //ret = RunEmulatedFunction(SupexecImpl, pCallback);
        //m68k_set_reg(M68K_REG_D0, (int)ret);
        pc = (void *) m68k_get_reg(NULL, M68K_REG_PC);
        //printf("*pc = 0x%04x\n", *(((unsigned short*)pc)-1));
        //return;
        sp = (unsigned long *) m68k_get_reg(NULL, M68K_REG_SP);
        *--sp = (unsigned long)pc;
        m68k_set_reg(M68K_REG_SP, (int) sp);
        m68k_set_reg(M68K_REG_PC, (int) SupexecImpl);
        return;
    }

    __asm__ volatile
    (
        "move.l	sp,a3\n\t"
        "move.l	%1,sp\n\t"
        "trap	#14\n\t"
        "move.l	a3,sp"
    : "=r"(reg_d0) /* outputs */
    : "g"(sp)
    : "d1", "d2", "a0", "a1", "a2", "a3", "memory" /* clobbered regs */
    );

    //nextCallback = NULL;

    m68k_set_reg(M68K_REG_D0, (int)reg_d0);
}

void m68ki_hook_linea()
{
    unsigned short* pc = (unsigned short *)m68k_get_reg(NULL, M68K_REG_PC);
    unsigned short* sp = (unsigned short *)m68k_get_reg(NULL, M68K_REG_SP);
    register long reg_d0 __asm__("d0");
    register long reg_a0 __asm__("a0");
    register long reg_a1 __asm__("a1");
    register long reg_a2 __asm__("a2");
    unsigned short opcode = pc[-1];
    unsigned short num = opcode & 0x000f;

    dbg("Line A %u 0x%04x\n", num, opcode);

    __asm__ volatile
    (
        "move.l	 sp,a3\n\t"
        "move.l	 %4,sp\n\t"
        "moveq	 #0,d0\n\t"
        "move.w	 %5,d0\n\t"
        "lsl.l   #2,d0\n\t"
        "jmp     0f(pc,d0.l)\n\t"
        "0:\n\t"
        ".dc.w   0xa920\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa921\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa922\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa923\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa924\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa925\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa926\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa927\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa928\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa929\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa92a\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa92b\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa92c\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa92d\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa92e\n\t"
        "bra.s   1f\n\t"
        ".dc.w   0xa92f\n\t"
        "1:\n\t"
        "move.l	a3,sp"
    : "=r"(reg_d0), "=r"(reg_a0), "=r"(reg_a1), "=r"(reg_a2) /* outputs */
    : "g"(sp), "g"(num)
    : "d1", "d2", "a3", "memory"    /* clobbered regs */
    );

    m68k_set_reg(M68K_REG_D0, (int)reg_d0);
    m68k_set_reg(M68K_REG_A0, (int)reg_a0);
    m68k_set_reg(M68K_REG_A1, (int)reg_a1);
    m68k_set_reg(M68K_REG_A2, (int)reg_a2);
}

static int int_ack_callback_vector = M68K_INT_ACK_AUTOVECTOR;

// Exception vector callback
int int_ack_callback(int int_level)
{
    return int_ack_callback_vector;
}

void instr_hook(void)
{
    // dbg("0x%08X: %s\r\n", REG_PC, m68ki_disassemble_quick(REG_PC, M68K_CPU_TYPE_68020));
}

unsigned char systack[64 * 1024];

void buildCommandTail(char tail[128], char* argv[], int argc)
{
    int i;

    tail[1] = '\0';
    for (i = 0; i < argc; ++i)
    {
        if (i > 0)
            strcat(tail + 1, " ");

        strcat(tail + 1, argv[i]);
    }

    tail[0] = (char)strlen(tail + 1);
}

int main(int argc, char* argv[])
{
    BASEPAGE* bp;
    unsigned long* pStack;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <program.tos> [arguments...]\n", argv[0]);

        fputs(
          "\033E" // Clear screen
          "68Kemu 20120317 ALPHA - A CPU emulator for Atari TOS computers\n"
          "Homepage: http://vincent.riviere.free.fr/soft/68kemu/\n"
          "Written in 2010-2012 by Vincent Riviere <vincent.riviere@freesbee.fr>\n"
          "\n"
          "Based on the Musashi M680x0 emulator Copyright 1998-2001 Karl Stenerud.\n"
          "Usage and redistribution are free for any non-commercial purpose.\n"
          "Please contact the authors for commercial usage and redistribution.\n"
          "\n"
          "Press any key.\n"
          , stderr);

        Cnecin();

        return 1;
    }

    char tail[128];
    buildCommandTail(tail, argv + 2, argc - 2);
    bp = (BASEPAGE*) Pexec(PE_LOAD, argv[1], tail, NULL);
    if ((long) bp < 0)
    {
        fprintf(stderr, "error: cannot load %s (%p).\n", argv[1], bp);
        return 1;
    }

    m68k_set_cpu_type(M68K_CPU_TYPE_68020);
    m68k_pulse_reset(); // Patched
    //m68k_set_int_ack_callback(int_ack_callback);

    pStack = (unsigned long*)bp->p_hitpa;
    *--pStack = (unsigned long) bp;
    *--pStack = (unsigned long) 0;

    m68k_set_reg(M68K_REG_SR, 0x0300);
    m68k_set_reg(M68K_REG_SP, (int) pStack);
    m68k_set_reg(M68K_REG_PC, (int) bp->p_tbase);

    for (;;)
    {
        m68k_execute(10000);
    }

    return 0;
}

void fc_handler(long fc)
{
    if (fc == 6)
    {
        // dbg("0x%08x: %s\r\n", REG_PPC, m68ki_disassemble_quick(REG_PPC, M68K_CPU_TYPE_68020));
    }
}
