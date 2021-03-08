#include "m68ksubr.h"
#include "debug.h"

extern int m68ki_initial_cycles;

/* Execute a subroutine until it returns */
int m68k_execute_subroutine(long sp, long subroutine)
{
    /*
     * save the current emulator context
     */
    m68ki_cpu_core saved_core = m68ki_cpu;

    dbg("saved active CPU core\r\n");

    /*
     * set address to our subroutine
     * and the stackpointer to our stack
     */

    m68k_set_reg(M68K_REG_PC, subroutine);
    m68k_set_reg(M68K_REG_SR, 0x0300);
    m68k_set_reg(M68K_REG_SP, sp);

    /* Make sure we're not stopped */
    if (! CPU_STOPPED)
    {
        /* Set our pool of clock cycles available */
        SET_CYCLES(10000);
        m68ki_initial_cycles = 10000;

        /* ASG: update cycles */
        USE_CYCLES(CPU_INT_CYCLES);
        CPU_INT_CYCLES = 0;

        /* Return point if we had an address error */
        m68ki_set_address_error_trap(); /* auto-disable (see m68kcpu.h) */


        /* Main loop.  Keep going until we run out of clock cycles */
        do
        {
            /* Set tracing accodring to T1. (T0 is done inside instruction) */
            m68ki_trace_t1(); /* auto-disable (see m68kcpu.h) */

            /* Set the address space for reads */
            m68ki_use_data_space(); /* auto-disable (see m68kcpu.h) */

            /* Call external hook to peek at CPU */
            m68ki_instr_hook(); /* auto-disable (see m68kcpu.h) */

            // dbg("0x%08x: %s\r\n", REG_PC, m68ki_disassemble_quick(REG_PC, M68K_CPU_TYPE_68020));

            /* Record previous program counter */
            REG_PPC = REG_PC;

            /* Read an instruction and call its handler */
            REG_IR = m68ki_read_imm_16();

            /*
             * check if we have an rts instruction and our stack marker on the stack
             * to see if we need to return
             */
            if ((REG_IR & 0xffff) == 0x4e75 && * (unsigned long *) REG_SP == RETURN_STACK_MARKER)
            {
                long reg_d0 = m68k_get_reg(NULL, M68K_REG_D0);

                dbg("arrived at RTS, return value is %lx\r\n", reg_d0);
                m68ki_cpu = saved_core;

                return reg_d0;
            }
            else
            {
                m68ki_instruction_jump_table[REG_IR]();
                USE_CYCLES(CYC_INSTRUCTION[REG_IR]);
            }

            /* Trace m68k_exception, if necessary */
            m68ki_exception_if_trace(); /* auto-disable (see m68kcpu.h) */
        } while (GET_CYCLES() > 0);

        /* set previous PC to current PC for the next entry into the loop */
        REG_PPC = REG_PC;

        /* ASG: update cycles */
        USE_CYCLES(CPU_INT_CYCLES);
        CPU_INT_CYCLES = 0;

        /* return how many clocks we used */
        return m68ki_initial_cycles - GET_CYCLES();
    }

    dbg("stoped???\r\n");
    /* We get here if the CPU is stopped or halted */
    SET_CYCLES(0);
    CPU_INT_CYCLES = 0;

    return 0;
}
