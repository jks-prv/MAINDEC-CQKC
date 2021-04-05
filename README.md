[updated 6 April 2021]

MAINDEC-CQKC
============

Instruction exerciser for DEC PDP-11/04 /05 /20 /34 /40 /45
----------------------------------------------------------------------

A complete, tested, editable binary listing of CQKC revision D that matches the available DEC documentation.

It is sometimes difficult to find a specific revision of a MAINDEC diagnostic binary file that matches
an online scan of the available documentation (containing the program listing).

This is true of CQKC. Binary files are available for revisions E0, G1 and G2 but the only
documentation is for revision D. The changes between revisions means diagnostic error messages,
which reference the program counter (PC) value, don't match the listing in the documentation.
This makes it very difficult to pinpoint exactly what test is failing.

The file `CQKC_D_34_40_45.txt` was carefully transcribed from the revision D documentation PDF file.
Now we have a diagnostic binary and listing pair that match.

A C-language program `txt2abs` converts the `.txt` file into an "absolute format" (`.abs`) file
suitable for use by various loaders and emulation programs. Nested conditional compilation
(i.e. the usual `#ifdef xxx ... #endif` syntax) is supported. For example a bug fix from
CQKC revision E0 is added by specifying `#define CQKC_E0_FIX`.

Although CQKC was originally intended for the /40 and /45 it has been found to work on an /05 emulator.
It has also been extended to work on /04 hardware using `#define 11/04`
and on /20 and /34 emulators using `#define 11/20` and `#define 11/34` respectively.

The CQKC revision D documentation is available on [Bitsavers](http://bitsavers.informatik.uni-stuttgart.de/pdf/dec/pdp11/xxdp/diag_listings/1140_45/028_MAINDEC-11-DCQKC-D_D_1140_1145_INSTRUCTION_EXERCISER_Sep74.pdf)
and also included here.
