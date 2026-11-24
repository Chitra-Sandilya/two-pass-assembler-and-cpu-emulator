/*
Name:    Chitra Sandilya
Roll Number: 2501CS15

I, Chitra Sandilya , declare that this source file is my own work.
*/
#include <ctype.h>  /* provides functions for testing and converting characters */
#include <errno.h>  /* defines macros for reporting and retrieving error conditions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#define MAX_LINE    256       /*maximum character length for reading a single input line */
#define MAX_LABELS  2048    /* maximum number of labels that the program can store in its symbol table */
#define MAX_NAME    31        /* maximum length for label name */
#define MAX_TOKEN   64       /* sets size for individual tokens */
#undef MAX_PATH
#define MAX_PATH    512       /* maximum allowed string for file paths */
#define OPERAND_MIN (-8388608L)   
#define OPERAND_MAX 8388607L 
          /* an enumeration that represents the type of operands an instruction may have */
typedef enum {
OP_NONE,         /* represents instructions that take no operand */
OP_VALUE,       /* represents instructions that take a numeric value */
OP_BRANCH,    /* represents jump/branch instructions */
OP_DATA,          /* data directive that stores numeric data in the PC */
OP_SET            /* SET directive, which creates a constant symbol in the symbol table */
} OperandKind;
typedef struct {
const char *mnemonic;
int opcode;             
OperandKind kind;
} Instruction; /* structure that represents a single instruction format */
/* an array containing all instructions supported by the assembler, its scope is restricted to the source file and the table is read only */
static const Instruction instr_table[] = {
{"data",   -1, OP_DATA},
{"ldc",    0, OP_VALUE},
{"adc",    1, OP_VALUE},
{"ldl",    2, OP_VALUE},
{"stl",    3, OP_VALUE},
{"ldnl",    4, OP_VALUE},
{"stnl",    5, OP_VALUE},
{"add",    6, OP_NONE},
{"sub",    7, OP_NONE},
{"shl",    8, OP_NONE},
{"shr",    9, OP_NONE},
{"adj",    10, OP_VALUE},
{"a2sp",   11, OP_NONE},
{"sp2a",   12, OP_NONE},
{"call",   13, OP_BRANCH},
{"return", 14, OP_NONE},
{"brz",    15, OP_BRANCH},
{"brlz",   16, OP_BRANCH},
{"br",     17, OP_BRANCH},
{"HALT",   18, OP_NONE},
{"mul",    19, OP_NONE},
{"div",    20, OP_NONE},
{"SET",    -1, OP_SET}
};
#define INSTR_COUNT (sizeof(instr_table) / sizeof(instr_table[0]))
typedef struct {
char name[MAX_NAME + 1];  /* stores the label name e.g. loop */
long value; /* stores its memory address or if the label was created via set it stores the constant value */
int  is_set;    /* Boolean flag, set to 1 if created via SET else it remains 0 */
int  used;   /* Boolean flag, used during pass 2 to see if label was referred anywhere in the program, if it remains zero the unused label warning is issued */
int  line;    /* stores the line number where label was defined */
} Label;
static Label labels[MAX_LABELS];  /* an array for holding labels */
static int   label_count = 0;
static int        cur_pass = 1;   
static long       pc = 0;         
static int        errors = 0;
static int        warnings = 0;
static const char *src_name = "";
static FILE       *obj_file = NULL;
static FILE       *lst_file = NULL;
                 
static void report(int line, const char *kind, const char *msg, const char *arg) 
{
fprintf(stderr, "%s:%d: %s: %s", src_name, line, kind, msg); /* private helper function used to print formatted error and warning messages to standard error */
if (arg != NULL && arg[0] != '\0')
fprintf(stderr, " '%s'", arg);
fputc('\n', stderr);
}
/* we use fprintf and fputc instead of printf and puts because they can only write to stdout while fprintf and fputc allow us to specify where to send the output (stderr in this case) */
static void syntax_error(int line, const char *msg, const char *arg)
{
if (cur_pass == 1) {
report(line, "error", msg, arg);
errors++;
}
}
/* a helper function that takes a pointer to a string and returns a pointer to the first non whitespace character */
static char *skip_space(char *p)
{
while (*p != '\0' && isspace((unsigned char)*p))
p++; /* isspace returns true when character is whitespace. we use unsigned char here because passing a char with negative value to isspace can produce undefined behaviour */
return p;
}
                         /* function to remove comments and white spaces, because if they are directly passed to the assembler it would process them as instructions, causing syntax errors */
static void strip_comment(char *line)
{
char *semi = strchr(line, ';'); /* strchr is a c function that returns pointer to the first character matching with the argument given or else returns NULL. */
size_t n;
if (semi != NULL)
*semi = '\0';       /* If semicolon was found, truncate line there */
n = strlen(line);
while (n > 0 && isspace((unsigned char)line[n - 1]))
line[--n] = '\0'; /* strip trailing spaces */
}
static int same_name_nocase(const char *a, const char *b) /* a helper function that compares two strings a and b for equality and this comparison is case insensitive. The function returns 1 if strings match otherwise it returns 0 */
{
while (*a != '\0' && *b != '\0') { /* loops through both strings */
if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) /* converts each character to lower case using tolower(). If the lower case versions do not match it returns 0 */
return 0;
a++; 
b++;
}
return *a == *b; /* true only if both strings are identical in length and content */
}
static const Instruction *lookup_instruction(const char *name) /* a function that searches the table for a particular mnemonic code and returns a pointer to that instruction or NULL */
{
size_t i;
for (i = 0; i < INSTR_COUNT; i++) /* a loop that iterates through all the contents of the instruction table */
if (same_name_nocase(name, instr_table[i].mnemonic)) /* calls our helper function from above to compare given mnemonic name to mnemonics given in the table */
return &instr_table[i];
return NULL;   /* if the loop finishes without finding any match that function returns null meaning no such mnemonic is recognised by our assembly language */
}
static int valid_label_name(const char *s) /* function to check correct label name. returns 1 if valid, 0 if invalid. */
{
size_t i;
if (!isalpha((unsigned char)s[0]) || strlen(s) > MAX_NAME) /* checks if the first character is an alphabet and checks if the length of the label exceeds maximum allowed length, if either condition is true it returns 0 for invalid label */
return 0;
for (i = 1; s[i] != '\0'; i++)
if (!isalnum((unsigned char)s[i])) /* checks if characters in string from index one to end are alphabetic or numerical, if not then it returns 0 */
return 0;
return 1;
}
static int parse_number(const char *tok, long *out)
{
    char *end;
    long long v;
    errno = 0;
    v = strtoll(tok, &end, 0);
    if (end == tok || *end != '\0' || errno == ERANGE)
        return 0;
    *out = (long)v;
    return 1;
}
static int fits_data(long v) /* a function that checks whether a given data value v fits within valid 32-bit limits. It compares v against the minimum signed 32-bit integer (-2147483647L - 1L) and ensures it does not exceed the maximum unsigned 32-bit value represented in hex as 0xFFFFFFFFUL */
{
return v >= (-2147483647L - 1L) &&
(v < 0 || (unsigned long)v <= 0xFFFFFFFFUL);
}
static Label *find_label(const char *name) /* a function that searches the symbol table array for a label matching the given name using strcmp, which compares two strings character by character and returns 0 if they are identical */
{
int i;
for (i = 0; i < label_count; i++)
if (strcmp(labels[i].name, name) == 0)
return &labels[i];
return NULL;
}
static const Label *find_label_at(long address) /* a function that searches the symbol table for any non-set label whose stored value matches the given memory address */
{
int i;
for (i = 0; i < label_count; i++)
if (!labels[i].is_set && labels[i].value == address)
return &labels[i];
return NULL;
}
static void define_label(const char *name, long value, int is_set, int line) /* a function that adds a new label to the symbol table array. It first calls find_label to verify that a label with this name does not already exist so strcmp returns non-zero for uniqueness, checks if label_count exceeds MAX_LABELS before storing the name, value, set flag, initial usage status as 0, and line number */
{
Label *l;
if (find_label(name) != NULL) {
syntax_error(line, "duplicate label", name);
return;
}
if (label_count >= MAX_LABELS) {
syntax_error(line, "too many labels", name);
return;
}
l = &labels[label_count++];
strcpy(l->name, name);
l->value = value;
l->is_set = is_set;
l->used = 0;
l->line = line;
}
static void emit_word(unsigned long w) /* a function that writes a 32-bit machine word to the binary object file one byte at a time in little-endian order. It uses bitwise AND with 0xFF to extract the lowest 8 bits, and right shifts by 8, 16, and 24 bits combined with 0xFF to isolate and write the remaining bytes sequentially */
{
fputc((int)(w & 0xFF), obj_file);
fputc((int)((w >> 8) & 0xFF), obj_file);
fputc((int)((w >> 16) & 0xFF), obj_file);
fputc((int)((w >> 24) & 0xFF), obj_file);
}
static void list_set(const char *name, const char *opnd) /* a function that prints a formatted SET directive entry into the listing file */
{
fprintf(lst_file, "    %-10s SET %s\n", name, opnd);
}
static void list_instruction(const Instruction *ins, unsigned long word, 
const char *mnemonic, const char *opnd,
int label_operand, long value, int lineno, const char *err_msg)
{
const Label *target;
const char *shown = opnd;
if (ins != NULL && ins->kind == OP_BRANCH && !label_operand) {
target = find_label_at(pc + 1 + value);
if (target != NULL)
shown = target->name;
}
/* We use fprintf instead of printf because printf only prints to standard output (the screen), whereas fprintf lets us write directly into our lst_file (the listing text file).
   We use formatting codes like %04lX and %08lX where the numbers 4 and 8 specify minimum field widths, padding numbers with leading zeros so columns line up neatly. 
   The 'l' means long integer, and 'X' prints the value in uppercase hexadecimal format.
   We use %-5s and %-10s where the minus sign (-) means left-aligned, and 5 or 10 specifies the exact character width of the text column, ensuring mnemonics and operands align nicely in columns. */
fprintf(lst_file, "%04lX  %08lX  ", (unsigned long)pc, word);
if (ins != NULL && ins->kind != OP_NONE)
fprintf(lst_file, "%-5s %-10s", ins->mnemonic, shown);
else if (mnemonic[0] != '\0')
fprintf(lst_file, "%-16s", mnemonic);
else
fprintf(lst_file, "%-16s", "");
/* This checks if an error message exists for this line, and if so, writes 'ERR: Line [number]: [message]' into the listing file.
   Otherwise, it checks if the instruction is a branch and the jump value is negative, which means it jumps backward in code. Since backward jumps can create infinite loops if conditions never change, it logs a warning message into the listing file and increments our warnings counter. */
if (err_msg != NULL && err_msg[0] != '\0') {
fprintf(lst_file, " ERR: Line %d: %s", lineno, err_msg);
} else if (ins != NULL && ins->kind == OP_BRANCH && value < 0) {
fprintf(lst_file, " WARN: Line %d: possible infinite loop (backward jump)", lineno);
warnings++;
}
fputc('\n', lst_file);
}
static void process_line(char *line, int lineno)
{
char label_name[MAX_TOKEN], mnem[MAX_TOKEN], opnd[MAX_TOKEN];
char err_msg[128] = "";
const Instruction *ins = NULL;
Label *t;
char *p, *q;
size_t len;
long value = 0;
unsigned long word = 0;
int have_label = 0, bad = 0, label_operand = 0, is_set = 0;
label_name[0] = mnem[0] = opnd[0] = '\0';
strip_comment(line); /* removes inline comments starting with semicolon and strips trailing spaces */
p = skip_space(line); /* advances pointer past any leading whitespace characters */

q = p;
while (*q != '\0' && !isspace((unsigned char)*q) && *q != ':')
q++;
if (*q == ':') { /* checks if a colon is found, indicating a label definition at the start of the line */
len = (size_t)(q - p);
if (len >= MAX_TOKEN) len = MAX_TOKEN - 1;
memcpy(label_name, p, len); /* copies the characters before the colon into label_name */
label_name[len] = '\0';
if (!valid_label_name(label_name)) { /* checks if the extracted label name uses valid alphabetic and alphanumeric characters */
sprintf(err_msg, "invalid label '%s'", label_name);
syntax_error(lineno, "bogus label name", label_name);
bad = 1;
}
have_label = 1; /* marks that a valid label prefix has been processed on this line */
p = skip_space(q + 1); /* moves pointer past the colon to continue parsing the instruction */
}

if (*p == '\0') { /* handles lines containing only a label with no instruction or data */
if (have_label && cur_pass == 1)
define_label(label_name, pc, 0, lineno); /* records the current program counter value for this label in pass 1 */
return;
}

q = p;
while (*q != '\0' && !isspace((unsigned char)*q))
q++;
len = (size_t)(q - p);
if (len >= MAX_TOKEN) len = MAX_TOKEN - 1;
memcpy(mnem, p, len); /* extracts the instruction mnemonic string (alphabetic text like ldc, add, data) */
mnem[len] = '\0';
ins = lookup_instruction(mnem); /* searches instruction table case-insensitively to match the mnemonic string to its defined opcode and operand type */
if (ins == NULL) { /* handles unknown text mnemonics that are not part of the assembly language instruction set */
sprintf(err_msg, "unknown mnemonic '%s'", mnem);
syntax_error(lineno, "unknown mnemonic", mnem);
bad = 1;
} else if (ins->kind == OP_SET) {
is_set = 1; /* flags the directive as a SET constant declaration */
}

p = skip_space(q);
strcpy(opnd, p); /* copies the remaining string tokens as the operand text */
if (!bad) {
if (ins->kind == OP_NONE && opnd[0] != '\0') {
sprintf(err_msg, "unexpected operand");
syntax_error(lineno, "unexpected operand", opnd);
bad = 1;
} else if (ins->kind != OP_NONE && opnd[0] == '\0') {
sprintf(err_msg, "missing operand");
syntax_error(lineno, "missing operand", mnem);
bad = 1;
} else if (is_set && !have_label) {
sprintf(err_msg, "SET needs a label on the same line");
syntax_error(lineno, "SET needs a label on the same line", "");
bad = 1;
}
}

if (!bad && ins->kind != OP_NONE) {
if (parse_number(opnd, &value)) { /* tries to parse the operand as a numeric constant */
label_operand = 0;
} else {
label_operand = 1; /* treats non-numeric operands as symbol references or labels */
if (is_set) {
sprintf(err_msg, "SET needs a numeric operand");
syntax_error(lineno, "SET needs a numeric operand", opnd);
bad = 1;
} else if (cur_pass == 2) {
t = find_label(opnd); /* looks up the label in the symbol table during pass 2 */
if (t == NULL) {
sprintf(err_msg, "undefined label '%s'", opnd);
report(lineno, "error", "no such label", opnd);
errors++;
bad = 1;
} else {
t->used = 1; /* marks the label as referenced so no unused warning is triggered */
value = t->value;
if (ins->kind == OP_BRANCH)
value -= (pc + 1); /* calculates relative offset for branch jump instructions */
}
}
}
if (!bad) {
int ok;
if (ins->kind == OP_DATA || is_set)
ok = fits_data(value);
else
ok = (value >= OPERAND_MIN && value <= OPERAND_MAX);
if (!ok) {
sprintf(err_msg, "operand out of range '%s'", opnd);
if (label_operand) {
if (cur_pass == 2) {
report(lineno, "error", "operand out of range", opnd);
errors++;
}
} else {
syntax_error(lineno, "operand out of range", opnd);
}
bad = 1;
}
}
}

if (have_label && cur_pass == 1)
define_label(label_name, is_set ? value : pc, is_set, lineno); /* adds the label and its address or constant value to the symbol table during pass 1 */

if (cur_pass == 2) {
if (have_label && is_set) {
list_set(label_name, opnd); /* writes SET constant lines to the listing file */
} else {
if (ins != NULL) {
if (ins->kind == OP_DATA)
word = (unsigned long)value & 0xFFFFFFFFUL; /* formats raw 32-bit data words for data directives */
else
word = (((unsigned long)value & 0xFFFFFFUL) << 8) |
(unsigned long)(ins->opcode >= 0 ? ins->opcode : 0); /* packs 24-bit operand value left-shifted by 8 bits combined with the 8-bit instruction opcode using bitwise OR to create the final 32-bit machine word */
}
if (!bad)
emit_word(word); /* writes the assembled 32-bit word byte by byte into the binary object file (.o) */
list_instruction(ins, word, mnem, opnd, label_operand, value, lineno, err_msg); /* writes the formatted assembly code, hex address, machine code word, and warnings into the text listing file (.lst) */
}
}
if (!is_set)
pc++; /* increments the program counter address for every assembled instruction or data line */
}
static void run_pass(FILE *src, int pass)
{
char line[MAX_LINE];
int lineno = 0;
int c;
cur_pass = pass; /* sets the current assembler pass variable (1 or 2) */
pc = 0; /* resets the program counter back to zero at the beginning of each pass */
rewind(src); /* resets the file pointer back to the very beginning of the source file so we can read it from the start */
while (fgets(line, (int)sizeof line, src) != NULL) { /* loops through the file line by line using fgets until the end of the file is reached */
lineno++; /* increments the line number counter for each line read */
if (strchr(line, '\n') == NULL && !feof(src)) { /* checks if the line was too long to fit in our buffer by checking for a missing newline before EOF */
syntax_error(lineno, "line too long", "");
while ((c = getc(src)) != '\n' && c != EOF) { /* discards the remainder of the oversized line until a newline or end of file is reached */
}
continue;
}
process_line(line, lineno); /* calls process_line to parse and assemble the current line of code */
}
}
static int make_name(char *dst, const char *src, const char *ext)
{
const char *dot, *slash;
size_t base;
slash = strrchr(src, '/'); /* searches for forward slashes to find directory separators in the file path */
if (slash == NULL)
slash = strrchr(src, '\\'); /* if no forward slash is found, searches for backward slashes for Windows paths */
dot = strrchr(src, '.'); /* searches for the last dot character to find the file extension position */
base = (dot != NULL && (slash == NULL || dot > slash))
? (size_t)(dot - src) : strlen(src); /* determines the length of the base filename excluding its extension */
if (base + strlen(ext) + 1 > MAX_PATH)
return 0; /* returns 0 if the combined filename path exceeds the maximum allowed path length */
memcpy(dst, src, base); /* copies the base filename part into the destination buffer */
strcpy(dst + base, ext); /* appends the new file extension string (like .o or .lst) to the base name */
return 1; /* returns 1 to indicate success */
}
int main(int argc, char *argv[])
{
FILE *src;
char obj_name[MAX_PATH], lst_name[MAX_PATH];
int i;
if (argc != 2) { /* checks if the user provided exactly one command line argument (the source assembly file name) */
fprintf(stderr, "usage: %s file.asm\n", argv[0]);
return 2; /* returns exit code 2 if arguments are incorrect */
}
src_name = argv[1]; /* stores the input source filename from command line arguments */
if (!make_name(obj_name, src_name, ".o") || /* calls make_name to construct the binary object filename ending in .o */
!make_name(lst_name, src_name, ".lst")) { /* calls make_name to construct the text listing filename ending in .lst */
fprintf(stderr, "file name too long\n");
return 2;
}
src = fopen(src_name, "r"); /* opens the assembly source file in read mode */
if (src == NULL) {
perror(src_name); /* prints system error message if the file cannot be opened */
return 2;
}
run_pass(src, 1); /* executes pass 1 to scan labels, build the symbol table, and check syntax */
obj_file = fopen(obj_name, "wb"); /* opens the binary object file in write-binary mode */
lst_file = fopen(lst_name, "w"); /* opens the listing text file in write mode */
if (obj_file == NULL || lst_file == NULL) {
fprintf(stderr, "cannot create output files\n");
return 2;
}
run_pass(src, 2); /* executes pass 2 to generate machine code, write object bytes, and create the listing file */
for (i = 0; i < label_count; i++) {
if (!labels[i].used) { /* loops through all defined labels to check if any label was never referenced in the program */
fprintf(lst_file, "WARN: Label '%s' defined but never used\n", labels[i].name);
report(labels[i].line, "warning", "unused label", labels[i].name);
warnings++; /* increments warning count for unused labels */
}
}
fclose(src); /* closes the source file handle */
fclose(obj_file); /* closes the binary object file handle */
fclose(lst_file); /* closes the text listing file handle */
if (errors > 0) {
remove(obj_name); /* deletes the incomplete object file if assembly errors occurred, keeping only the listing file */
fprintf(stderr, "%s: %d error(s), %d warning(s) - listing generated\n",
src_name, errors, warnings);
return 1; /* returns exit code 1 to indicate assembly failure */
}
printf("%s: assembled OK, %ld word(s), %d warning(s)\n",
src_name, pc, warnings); /* prints success message with total assembled words and warnings */
printf("  object  : %s\n  listing : %s\n", obj_name, lst_name); /* prints paths of generated object and listing files */
return 0; /* returns exit code 0 for successful assembly */
}