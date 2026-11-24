/*
Name:    Chitra Sandilya
Roll Number: 2501CS15

I, Chitra Sandilya, declare that this source file is my own work.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_WORDS 65536L      /* memory size in 32-bit words */
#define MAX_STEPS 10000000L   /* give up after this many instructions */

/* opcodes */
#define OP_LDC    0
#define OP_ADC    1
#define OP_LDL    2
#define OP_STL    3
#define OP_LDNL   4
#define OP_STNL   5
#define OP_ADD    6
#define OP_SUB    7
#define OP_SHL    8
#define OP_SHR    9
#define OP_ADJ    10
#define OP_A2SP   11
#define OP_SP2A   12
#define OP_CALL   13
#define OP_RETURN 14
#define OP_BRZ    15
#define OP_BRLZ   16
#define OP_BR     17
#define OP_HALT   18
#define OP_MUL    19
#define OP_DIV    20
#define NUM_OPS   21

/* Mnemonic table, indexed by opcode. Used by the trace and by -isa. */
typedef struct {
    const char *name;
    int has_operand;
} OpInfo;

static const OpInfo ops[NUM_OPS] = {
    {"ldc", 1},  {"adc", 1},  {"ldl", 1},  {"stl", 1},   {"ldnl", 1},
    {"stnl", 1}, {"add", 0},  {"sub", 0},  {"shl", 0},   {"shr", 0},
    {"adj", 1},  {"a2sp", 0}, {"sp2a", 0}, {"call", 1},  {"return", 0},
    {"brz", 1},  {"brlz", 1}, {"br", 1},   {"HALT", 0},  {"mul", 0},
    {"div", 0}
};

/* machine state */
static long mem[MEM_WORDS];
static long reg_a, reg_b, reg_pc, reg_sp;
static long prog_size;   /* number of words loaded from the object file */
static long steps;       /* instructions executed so far */
static long fault_pc;    /* address of the instruction being executed */



static long to_signed32(unsigned long u) /* converts the lower 32 bits of an unsigned long into a signed 32-bit value using two's complement arithmetic */
{
    u &= 0xFFFFFFFFUL;
    if (u >= 0x80000000UL)
        return -(long)(0xFFFFFFFFUL - u) - 1L;
    return (long)u;
}

static long wrap32(long v) /* helper function that wraps a value so it stays within signed 32-bit integer limits, simulating 32-bit hardware register overflow */
{
    return to_signed32((unsigned long)v);
}

static long signed24(unsigned long word) /* extracts the upper 24 bits from an instruction word and sign-extends them into a signed long value */
{
    long v = (long)((word & 0xFFFFFFFFUL) >> 8);

    if (v >= 0x800000L)
        v -= 0x1000000L;
    return v;
}

static unsigned long as_hex(long v) /* converts a signed long value into an unsigned 32-bit hex representation */
{
    return (unsigned long)v & 0xFFFFFFFFUL;
}

static void runtime_error(const char *msg) /* prints formatted runtime errors with the current program counter value to standard error */
{
    fprintf(stderr, "emu: runtime error at PC=%08lX: %s\n",
            as_hex(fault_pc), msg);
}

static int bad_address(long addr) /* validates whether a memory address is within acceptable boundaries and logs a runtime error if out of bounds */
{
    if (addr < 0 || addr >= MEM_WORDS) {
        runtime_error("memory access outside the machine");
        return 1;
    }
    return 0;
}



static int load_object(const char *name) /* opens a binary object file, reads 4-byte instruction words in little-endian order into machine memory, and returns 1 on success or 0 on failure */
{
    FILE *f;
    int b0, b1, b2, b3;
    unsigned long w;

    f = fopen(name, "rb");
    if (f == NULL) {
        perror(name); /* uses standard library function perror to print system error message for failed file open */
        return 0;
    }
    prog_size = 0;
    while ((b0 = getc(f)) != EOF) { /* loops through file byte by byte until reaching EOF */
        b1 = getc(f);
        b2 = getc(f);
        b3 = getc(f);
        if (b3 == EOF) {
            fprintf(stderr, "%s: file size is not a multiple of 4 bytes\n",
                    name);
            fclose(f);
            return 0;
        }
        if (prog_size >= MEM_WORDS) {
            fprintf(stderr, "%s: program is too big for memory\n", name);
            fclose(f);
            return 0;
        }
        w = (unsigned long)b0 | ((unsigned long)b1 << 8) |
            ((unsigned long)b2 << 16) | ((unsigned long)b3 << 24);
        mem[prog_size] = to_signed32(w);
        prog_size++;
    }
    fclose(f);
    return 1;
}

static void dump_memory(long from, long to) /* prints a range of memory contents formatted as four 32-bit hex words per line starting with their base address */
{
    long i;

    for (i = from; i < to; i++) {
        if ((i - from) % 4 == 0) {
            if (i != from)
                putchar('\n');
            printf("%08lX", (unsigned long)i);
        }
        printf(" %08lX", as_hex(mem[i]));
    }
    putchar('\n');
}

static void print_registers(void) /* prints the current values of registers A, B, PC, and SP in both signed decimal and hexadecimal formats */
{
    printf("A  = %ld (0x%08lX)\n", reg_a, as_hex(reg_a));
    printf("B  = %ld (0x%08lX)\n", reg_b, as_hex(reg_b));
    printf("PC = 0x%08lX\n", as_hex(reg_pc));
    printf("SP = 0x%08lX\n", as_hex(reg_sp));
}

static void print_isa(void) /* displays a table listing all supported opcodes, their mnemonic names, and whether an operand is required */
{
    int i;

    printf("Opcode  Mnemonic  Operand\n");
    for (i = 0; i < NUM_OPS; i++)
        printf("%6d  %-8s  %s\n", i, ops[i].name,
               ops[i].has_operand ? "yes" : "no");
}

static void trace_line(int opcode, long operand) /* prints a single execution trace line showing register states and the instruction being run */
{
    printf("PC=%08lX SP=%08lX A=%08lX B=%08lX  ", as_hex(reg_pc),
           as_hex(reg_sp), as_hex(reg_a), as_hex(reg_b));
    if (opcode >= 0 && opcode < NUM_OPS) {
        printf("%s", ops[opcode].name);
        if (ops[opcode].has_operand)
            printf(" %ld", operand);
    } else {
        printf("??? (opcode %d)", opcode);
    }
    putchar('\n');
}



static int run(int trace) /* runs the main fetch-decode-execute instruction cycle of the virtual processor until a HALT instruction or error is hit */
{
    unsigned long word;
    long operand, addr;
    int opcode;

    for (;;) { /* endless execution loop fetching and processing instructions */
        fault_pc = reg_pc;

        if (reg_pc < 0 || reg_pc >= prog_size) {
            runtime_error("PC is outside the program (ran off the end?)");
            return 1;
        }
        if (steps >= MAX_STEPS) {
            runtime_error("instruction limit reached (infinite loop?)");
            return 1;
        }

        word = (unsigned long)mem[reg_pc] & 0xFFFFFFFFUL;
        opcode = (int)(word & 0xFF);
        operand = signed24(word);

        if (trace)
            trace_line(opcode, operand);

        reg_pc++;
        steps++;

        switch (opcode) {
        case OP_LDC:
            reg_b = reg_a;
            reg_a = operand;
            break;
        case OP_ADC:
            reg_a = wrap32(reg_a + operand);
            break;
        case OP_LDL:
            addr = reg_sp + operand;
            if (bad_address(addr))
                return 1;
            reg_b = reg_a;
            reg_a = mem[addr];
            break;
        case OP_STL:
            addr = reg_sp + operand;
            if (bad_address(addr))
                return 1;
            mem[addr] = reg_a;
            reg_a = reg_b;
            break;
        case OP_LDNL:
            addr = reg_a + operand;
            if (bad_address(addr))
                return 1;
            reg_a = mem[addr];
            break;
        case OP_STNL:
            addr = reg_a + operand;
            if (bad_address(addr))
                return 1;
            mem[addr] = reg_b;
            break;
        case OP_ADD:
            reg_a = wrap32(reg_b + reg_a);
            break;
        case OP_SUB:
            reg_a = wrap32(reg_b - reg_a);
            break;
        case OP_SHL:
            if (reg_a < 0 || reg_a > 31) {
                runtime_error("shift count must be between 0 and 31");
                return 1;
            }
            reg_a = to_signed32((unsigned long)reg_b << reg_a);
            break;
        case OP_SHR:
            if (reg_a < 0 || reg_a > 31) {
                runtime_error("shift count must be between 0 and 31");
                return 1;
            }
            reg_a = reg_b >> reg_a;   /* arithmetic shift on gcc */
            break;
        case OP_ADJ:
            reg_sp = wrap32(reg_sp + operand);
            break;
        case OP_A2SP:
            reg_sp = reg_a;
            reg_a = reg_b;
            break;
        case OP_SP2A:
            reg_b = reg_a;
            reg_a = reg_sp;
            break;
        case OP_CALL:
            reg_b = reg_a;
            reg_a = reg_pc;
            reg_pc = reg_pc + operand;
            break;
        case OP_RETURN:
            reg_pc = reg_a;
            reg_a = reg_b;
            break;
        case OP_BRZ:
            if (reg_a == 0)
                reg_pc = reg_pc + operand;
            break;
        case OP_BRLZ:
            if (reg_a < 0)
                reg_pc = reg_pc + operand;
            break;
        case OP_BR:
            reg_pc = reg_pc + operand;
            break;
        case OP_HALT:
            printf("HALT after %ld instructions\n", steps);
            return 0;
        case OP_MUL:
            /* multiply as unsigned so an overflow just wraps around */
            reg_a = to_signed32((unsigned long)reg_b * (unsigned long)reg_a);
            break;
        case OP_DIV:
            if (reg_a == 0) {
                runtime_error("division by zero");
                return 1;
            }
            reg_a = wrap32(reg_b / reg_a);
            break;
        default:
            runtime_error("unknown opcode");
            return 1;
        }
    }
}



static void usage(const char *prog) /* prints command line argument usage options to standard error stream */
{
    fprintf(stderr, "usage: %s [-trace] [-before] [-after] [-isa] file.o\n",
            prog);
}

int main(int argc, char *argv[]) /* entry point of program */
{
    const char *file = NULL;
    int trace = 0, show_before = 0, show_after = 0;
    int i, status;

    for (i = 1; i < argc; i++) { /* parses command-line arguments to set configuration flags */
        if (strcmp(argv[i], "-trace") == 0) { /* strcmp compares two strings and returns 0 if they match exactly */
            trace = 1;
        } else if (strcmp(argv[i], "-before") == 0) {
            show_before = 1;
        } else if (strcmp(argv[i], "-after") == 0) {
            show_after = 1;
        } else if (strcmp(argv[i], "-isa") == 0) {
            print_isa();
            return 0;
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
            return 2;
        } else {
            file = argv[i];
        }
    }
    if (file == NULL) {
        usage(argv[0]);
        return 2;
    }
    if (!load_object(file))
        return 2;

    reg_a = 0;
    reg_b = 0;
    reg_pc = 0;
    reg_sp = MEM_WORDS;      /* stack starts at the top, grows downwards */

    printf("Loaded %ld words from %s\n", prog_size, file);
    if (show_before) {
        printf("Memory before execution:\n");
        dump_memory(0, prog_size);
    }

    status = run(trace);

    if (show_after) {
        printf("Memory after execution:\n");
        dump_memory(0, prog_size);
    }
    print_registers();
    return status;
}