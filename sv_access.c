#include "sv_access.h"
#include "osbind.h"
#include "debug.h"
#include "hashtable.h"
#include "m68ksubr.h"

bool is_super(void)
{
    return ((unsigned short) m68k_get_reg(NULL, M68K_REG_SR) & 0x2000);
}

void berr(void)
{
    unsigned pc, sr, adr, write, instruction, fc;

    pc = m68k_get_reg(NULL, M68K_REG_PC);

    m68ki_stack_frame_buserr(pc, sr, adr, write, instruction, fc);
    m68k_set_reg(M68K_REG_PC, emu_vector_table[3]);
}

static unsigned *addr;

unsigned read_sup(void)
{
    unsigned ret = *addr;
    return ret;
}

unsigned int read_super(unsigned address, int length)
{
    unsigned val;

    addr = (unsigned *) address;
    val = Supexec((long (*)(void)) read_sup);

    dbg("*%p = 0x%x\r\n", address, val);
    return val;
}

static unsigned val;

unsigned write_sup(void)
{
    switch ((int) addr)
    {
        default:
            *addr = val;
            break;
    }
}

struct vector
{
    void (*handler)(void);
    bool native;
};

static unsigned long emu_vector_table[255] = { 0 };

# define HT_SIZE    20
static struct hashtable *ht = NULL;


/*
 * execute the given address as emulated exception handler
 */
#ifdef __mcoldfire__
struct format_status_struct
{
    unsigned format     : 4;
    unsigned fsup       : 2;
    unsigned vec        : 8;
    unsigned fslo       : 2;
    unsigned sr         : 16;
};

static void exception_handler(struct format_status_struct format_status, void (*pc)(void)) __attribute__ ((interrupt));
static void exception_handler(struct format_status_struct format_status, void (*pc)(void))
{
    // void (*emul_function)() = ht_get()
    void (*value)(void);
#define STACKSIZE   128
    long stack[STACKSIZE];

    stack[STACKSIZE - 1] = RETURN_STACK_MARKER;

    dbg("\r\n");
    ht_get(ht, (long[1]) { 8 }, sizeof(long), &value,  sizeof(value));
    m68k_execute_subroutine(stack, value);
}
#else
#ifdef __mc68000__
struct exception_stackframe
{
    unsigned pc         : 32;
    unsigned access     : 32;
    unsigned sr         : 32;
};

static void exception_handler(struct exception_stackframe ex, void (*pc)(void)) __attribute__ ((interrupt));
static void exception_handler(struct exception_stackframe ex, void (*pc)(void))
{
    void (*value)(void);
#define STACKSIZE   128
    long stack[STACKSIZE];

    stack[STACKSIZE - 1] = RETURN_STACK_MARKER;

    dbg("pc=%p, access=%p sr=%x\r\n",
        ex.pc, ex.access, ex.sr);
    ht_get(ht, (long[1]) { 8 }, sizeof(long), &value,  sizeof(value));
    m68k_execute_subroutine((long) stack, (long) value);
}
#endif
#endif

void write_super(unsigned address, unsigned value)
{
    /*
     * if the address to write is one of the vector table, insert a hook that allows us
     * to later call the exception handler for it in the emulator.
     * For this, we manage a hash table of vector table addresses to emulator handlers
     */

    if (ht == NULL)
        ht = ht_new(HT_SIZE);

    unsigned pc, sr, adr, write, instruction, fc;

    switch((long) addr)
    {
        case 0xfffa42:
        case 0xfffffa42:
            pc = m68k_get_reg(NULL, M68K_REG_PC);
            sr = m68k_get_reg(NULL, M68K_REG_SR);
            adr = (unsigned) addr;
            write = 1;
            instruction = 1;        /* no idea, yet */
            fc = 0;                 /* sloppy */
            m68ki_stack_frame_buserr(pc, sr, adr, write, instruction, fc);
            m68k_set_reg(M68K_REG_PC, emu_vector_table[3]);

            return;

        case 8:
            emu_vector_table[address / 4 + 1] = value;
            return;
            break;
    }

    addr = (unsigned *) address;
    val = (unsigned int) &exception_handler;
    dbg("*%p = 0x%x\r\n", addr, val);
    Supexec(write_sup);
    ht_put(ht, &address, sizeof(&address), &value, sizeof(&value));
}
