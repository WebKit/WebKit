/* eval.h   header file for eval.c
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */

#ifndef YASM_EVAL_H
#define YASM_EVAL_H

/*
 * -------------------------
 * Error reporting functions
 * -------------------------
 */

/*
 * An error reporting function should look like this.
 */
typedef void (*efunc) (void *private_data, int severity, const char *fmt, ...);

/*
 * These are the error severity codes which get passed as the first
 * argument to an efunc.
 */

#define ERR_DEBUG       0x00000008      /* put out debugging message */
#define ERR_WARNING     0x00000000      /* warn only: no further action */
#define ERR_NONFATAL    0x00000001      /* terminate assembly after phase */
#define ERR_FATAL       0x00000002      /* instantly fatal: exit with error */
#define ERR_PANIC       0x00000003      /* internal error: panic instantly
                                        * and dump core for reference */
#define ERR_MASK        0x0000000F      /* mask off the above codes */
#define ERR_NOFILE      0x00000010      /* don't give source file name/line */
#define ERR_USAGE       0x00000020      /* print a usage message */
#define ERR_PASS1       0x00000040      /* only print this error on pass one */

/*
 * These codes define specific types of suppressible warning.
 */

#define ERR_WARN_MASK   0x0000FF00      /* the mask for this feature */
#define ERR_WARN_SHR    8               /* how far to shift right */

#define ERR_WARN_MNP    0x00000100      /* macro-num-parameters warning */
#define ERR_WARN_MSR    0x00000200      /* macro self-reference */
#define ERR_WARN_OL     0x00000300      /* orphan label (no colon, and
                                        * alone on line) */
#define ERR_WARN_NOV    0x00000400      /* numeric overflow */
#define ERR_WARN_GNUELF 0x00000500      /* using GNU ELF extensions */
#define ERR_WARN_MAX    5               /* the highest numbered one */

/*
 * The expression evaluator must be passed a scanner function; a
 * standard scanner is provided as part of nasmlib.c. The
 * preprocessor will use a different one. Scanners, and the
 * token-value structures they return, look like this.
 *
 * The return value from the scanner is always a copy of the
 * `t_type' field in the structure.
 */
struct tokenval {
    int t_type;
    yasm_intnum *t_integer, *t_inttwo;
    char *t_charptr;
};
typedef int (*scanner) (void *private_data, struct tokenval *tv);

/*
 * Token types returned by the scanner, in addition to ordinary
 * ASCII character values, and zero for end-of-string.
 */
enum {                                 /* token types, other than chars */
    TOKEN_INVALID = -1,                /* a placeholder value */
    TOKEN_EOS = 0,                     /* end of string */
    TOKEN_EQ = '=', TOKEN_GT = '>', TOKEN_LT = '<',   /* aliases */
    TOKEN_ID = 256, TOKEN_NUM, TOKEN_REG, TOKEN_INSN,  /* major token types */
    TOKEN_ERRNUM,                      /* numeric constant with error in */
    TOKEN_HERE, TOKEN_BASE,            /* $ and $$ */
    TOKEN_SPECIAL,                     /* BYTE, WORD, DWORD, FAR, NEAR, etc */
    TOKEN_PREFIX,                      /* A32, O16, LOCK, REPNZ, TIMES, etc */
    TOKEN_SHL, TOKEN_SHR,              /* << and >> */
    TOKEN_SDIV, TOKEN_SMOD,            /* // and %% */
    TOKEN_GE, TOKEN_LE, TOKEN_NE,      /* >=, <= and <> (!= is same as <>) */
    TOKEN_DBL_AND, TOKEN_DBL_OR, TOKEN_DBL_XOR,   /* &&, || and ^^ */
    TOKEN_SEG, TOKEN_WRT,              /* SEG and WRT */
    TOKEN_FLOAT                        /* floating-point constant */
};

/*
 * The actual expression evaluator function looks like this. When
 * called, it expects the first token of its expression to already
 * be in `*tv'; if it is not, set tv->t_type to TOKEN_INVALID and
 * it will start by calling the scanner.
 *
 * `critical' is non-zero if the expression may not contain forward
 * references. The evaluator will report its own error if this
 * occurs; if `critical' is 1, the error will be "symbol not
 * defined before use", whereas if `critical' is 2, the error will
 * be "symbol undefined".
 *
 * If `critical' has bit 8 set (in addition to its main value: 0x101
 * and 0x102 correspond to 1 and 2) then an extended expression
 * syntax is recognised, in which relational operators such as =, <
 * and >= are accepted, as well as low-precedence logical operators
 * &&, ^^ and ||.
 */
#define CRITICAL 0x100
typedef yasm_expr *(*evalfunc) (scanner sc, void *scprivate, struct tokenval *tv,
                                int critical, efunc error, yasm_symtab *symtab);

/*
 * The evaluator itself.
 */
yasm_expr *evaluate (scanner sc, void *scprivate, struct tokenval *tv,
                     void *eprivate, int critical, efunc report_error,
                     yasm_symtab *symtab);

#endif
