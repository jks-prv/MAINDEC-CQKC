//
// Converts a text file describing PDP-11 binary data into
// a binary file in absolute format (.abs) suitable for use with the absolute loader
//
// Usage: txt2abs [--list] [--def xxx] infile.txt outfile.abs
//
// Syntax of infile.txt:
// = address                    set address origin
// nnnnnn [nnnnnn] [nnnnnn]     one to three 16-bit octal word values (instructions, operands and data)
// b nnn                        single byte value, pc incremented by 1
// = nnnnnn                     set pc origin, subsequently incremented by 2/1 for word/byte values
// : nnnnnn                     consistency check: pc of the previous word must match this value
// :: nnnnnn                    consistency check: pc of the current word must match this value
//
// Also supports:
//      A subset of the usual conditional compilation: (can be nested)
//      #define xxx, #ifdef xxx, #if 1, #if 0, #else, #endif
//      Can also use "--def xxx" argument to program in addition to "#define xxx"
//
//      #warning xxx, #error xxx
//
// Absolute format block is emitted as each "=" or ":" directive is encountered.
// A "halt" block is emitted at the end of the file.
//
// Details of the absolute format:
// www.pcjs.org/apps/pdp11/tapes/absloader
//
// jks@jks.com
// 2019-2021
//

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

typedef unsigned char   u1_t;
typedef unsigned int    u4_t;

int lnum, errs;
bool list = false;
bool rm = false;
char *fn_in, *fn_out;

void error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("line %d ERROR: ", lnum);
	vprintf(fmt, ap);
	printf("\n");
	errs++;
	va_end(ap);
	
	if (!rm) {
	    char *s;
	    asprintf(&s, "rm %s", fn_out);
	    system(s);
	    free(s);
	    rm = true;
	}
}

void note(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("line %d NOTE: ", lnum);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

#define write_le(sp, w) *sp++ = (w) & 0xff; *sp++ = ((w) >> 8) & 0xff;
#define writeHL_le(sp, w) sp ## L = (w) & 0xff; sp ## H = ((w) >> 8) & 0xff;

#define ABS_SIG 1
#define HDR_LEN 6
#define CKSUM_LEN 1
#define NBLK (64 * 1024)

struct blk_t {
    u1_t sigL, sigH;
    u1_t lenL, lenH;
    u1_t addrL, addrH;
    u1_t data[NBLK];
};
blk_t blk;

bool have_blk = false;
u1_t *sp = blk.data;
u4_t org, pc;
FILE *fwp;
enum abs_t { ABS_BLK, ABS_HALT };

void write_abs(abs_t type)
{
    if (type == ABS_BLK) {
        if (!have_blk) return;
    } else
    if (type == ABS_HALT) {
        org = 1;    // halt if addr of block is odd
    }
    
    writeHL_le(blk.sig, ABS_SIG);
    int len = (int) (sp - blk.data);
    int tlen = len + HDR_LEN + CKSUM_LEN;
    writeHL_le(blk.len, tlen - CKSUM_LEN)
    writeHL_le(blk.addr, org)

    u4_t cksum = ABS_SIG + blk.lenL + blk.lenH + blk.addrL + blk.addrH;
    for (int i = 0; i < len; i++) cksum += blk.data[i];
    cksum = (0x100 - (cksum & 0xff)) & 0xff;
    *sp = cksum;

    if (fwrite(&blk, 1, tlen, fwp) != tlen) printf("write error\n");
    if (list)
        printf("wrote %s org %06o len %06o cksum %04o(0x%02x)\n\n",
            (type == ABS_BLK)? "BLK" : "HALT", org, len, cksum, cksum);

    have_blk = false;
    sp = blk.data;
    org = pc;
}

int n_ifdefs;
#define N_IFDEFS 32
char *ifdefs[N_IFDEFS];

#define NBUF 256
char buf[NBUF];

int main(int argc, char *argv[])
{
    #define ARG(s) (strcmp(argv[ai], "--" s) == 0)
    #define ARGP argv[++ai]
    
    int i, n;
    int ai = 0;
    bool help = false;

    for (int ai = 1; argv[ai]; ai++) {
        if (ARG("h") || ARG("help")) help = true; else
        if (ARG("in")) fn_in = ARGP; else
        if (ARG("out")) fn_out = ARGP; else
        if (ARG("list")) list = true; else
        
        if (ARG("def")) {
            ifdefs[n_ifdefs] = ARGP;
            printf("--def %s\n", ifdefs[n_ifdefs]);
            n_ifdefs++;
        } else

        { printf("unknown option: %s\n", argv[ai]); return -1; }
    }

    if (argc < 3 || help) {
        printf("usage: %s [--list] [--def xxx] --in infile.txt --out outfile.abs\n", argv[0]);
        return -1;
    }

    FILE *frp;
    if ((frp = fopen(fn_in, "r")) == NULL) { printf("fopen R %s\n", fn_in); return -1; }
    if ((fwp = fopen(fn_out, "w")) == NULL) { printf("fopen W %s\n", fn_out); return -1; }

    u4_t norg, chk, w0, w1, w2, b;
    u4_t lvl = 1;
    u4_t inside_if = 0, ignore_input = 0;
    
    while (fgets(buf, NBUF, frp)) {
        char *bp = buf;
        lnum++;
        buf[strlen(buf)-1] = '\0';  // remove \n
        if (list) printf("line #%04d: %06o | %s\n", lnum, pc, bp);
        while (*bp != '\0' && isspace(*bp)) bp++;   // remove leading whitespace
        if (*bp == '\0' || strncmp(bp, "//", 2) == 0)
            continue;
        
        char def_s[64];
        if (strcmp(bp, "#else") == 0) {
            if (!(inside_if & lvl))
                error("#else not inside #if lvl=0x%x inside_if=0x%x ignore_input=0x%x", lvl, inside_if, ignore_input);
            else
                ignore_input ^= lvl;
            //note("#else  lvl=0x%x inside_if=0x%x ignore_input=0x%x", lvl, inside_if, ignore_input);
            continue;
        }

        if (strcmp(bp, "#endif") == 0) {
            inside_if &= ~lvl;
            ignore_input &= ~lvl;
            lvl >>= 1;
            if (lvl == 0) error("#endif without corresponding #if");
            //note("#endif lvl=0x%x inside_if=0x%x ignore_input=0x%x", lvl, inside_if, ignore_input);
            continue;
        }

        if (strcmp(bp, "#if 1") == 0) { lvl <<= 1; inside_if |= lvl; ignore_input &= ~lvl; continue; }
        if (strcmp(bp, "#if 0") == 0) { lvl <<= 1; inside_if |= lvl; ignore_input |= lvl; continue; }

        if (sscanf(bp, "#ifdef %s", def_s) == 1) {
            for (i = 0; i < n_ifdefs; i++) {
                if (strcmp(def_s, ifdefs[i]) == 0) {
                    break;
                }
            }
            lvl <<= 1;
            inside_if |= lvl;
            if (i == n_ifdefs) ignore_input |= lvl; else ignore_input &= ~lvl;
            //note("#ifdef lvl=0x%x inside_if=0x%x ignore_input=0x%x %s", lvl, inside_if, ignore_input, def_s);
            continue;
        }
        
        if (ignore_input) continue;     // i.e. ignore if ignoring on any level

        if (sscanf(bp, "#define %s", def_s) == 1) {
            printf("#define %s\n", def_s);
            ifdefs[n_ifdefs] = strdup(def_s);
            n_ifdefs++;
            continue;
        }

        if (strncmp(bp, "#error", 6) == 0) { error("\"%s\"", bp); continue; }
        if (strncmp(bp, "#warning", 8) == 0) { note("\"%s\"", bp); continue; }

        if (sscanf(bp, "= %o", &norg) == 1) {
            write_abs(ABS_BLK);
            if (norg > 0177777) error("range norg=%06o", norg);
            pc = org = norg;
        } else

        if (sscanf(bp, ":: %o", &chk) == 1) {
            if (chk > 0177777) error("'::' range chk=%06o", chk);
            if (pc != chk) error("consistency check, expecting pc=%06o but \":: %06o\" specified", pc, chk);
            write_abs(ABS_BLK);
        } else

        if (sscanf(bp, ": %o", &chk) == 1) {
            if (chk > 0177777) error("':' range chk=%06o", chk);
            int pcm2 = pc - 2;
            if (pcm2 != chk) error("consistency check, expecting (pc-2)=%06o but \": %06o\" specified", pcm2, chk);
            write_abs(ABS_BLK);
        } else

        if (sscanf(bp, "b %o", &b) == 1) {
            if (b > 0377) error("range b=%04o", b);
            *sp++ = b; pc += 1;
            have_blk = true;
        } else {

            n = sscanf(bp, "%o %o %o", &w0, &w1, &w2);
            if (n != 1 && n != 2 && n != 3) {
                error("syntax error \"%s\"", buf);
            } else {
                if (w0 > 0177777) error("range w0=%06o", w0);
                if (n >= 2 && w1 > 0177777) error("range w1=%06o", w1);
                if (n >= 3 && w2 > 0177777) error("range w2=%06o", w2);
                if (pc & 1) error("odd pc=%06o", pc);
                write_le(sp, w0); pc += 2;
                if (n >= 2) { write_le(sp, w1); pc += 2; }
                if (n >= 3) { write_le(sp, w2); pc += 2; }
                have_blk = true;
            }
        }
    }
    
    write_abs(ABS_BLK);
    write_abs(ABS_HALT);
    fclose(fwp);

    if (list || errs) printf("%d error%s\n", errs, (errs != 1)? "s":"");
    return 0;
}
