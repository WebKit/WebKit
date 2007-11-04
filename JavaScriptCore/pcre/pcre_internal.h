/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/


/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
           Copyright (c) 1997-2006 University of Cambridge
           Copyright (c) 2004, 2005 Apple Computer, Inc.

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/* This header contains definitions that are shared between the different
modules, but which are not relevant to the exported API. This includes some
functions whose names all begin with "_pcre_". */

#ifndef PCRE_INTERNAL_H
#define PCRE_INTERNAL_H

#include "Assertions.h"

#if COMPILER(MSVC)
#pragma warning(disable: 4232)
#pragma warning(disable: 4244)
#endif

#define _pcre_OP_lengths kjs_pcre_OP_lengths
#define _pcre_default_tables kjs_pcre_default_tables
#define _pcre_ord2utf8 kjs_pcre_ord2utf8
#define _pcre_printint kjs_pcre_printint
#define _pcre_try_flipped kjs_pcre_try_flipped
#define _pcre_ucp_findchar kjs_pcre_ucp_findchar
#define _pcre_utf8_table1 kjs_pcre_utf8_table1
#define _pcre_utf8_table1_size  kjs_pcre_utf8_table1_size
#define _pcre_utf8_table2 kjs_pcre_utf8_table2
#define _pcre_utf8_table3 kjs_pcre_utf8_table3
#define _pcre_utf8_table4 kjs_pcre_utf8_table4
#define _pcre_utt kjs_pcre_utt
#define _pcre_utt_size kjs_pcre_utt_size
#define _pcre_valid_utf8 kjs_pcre_valid_utf8
#define _pcre_xclass kjs_pcre_xclass

/* Define DEBUG to get debugging output on stdout. */

#if 0
#define DEBUG
#endif

/* Use a macro for debugging printing, 'cause that eliminates the use of #ifdef
inline, and there are *still* stupid compilers about that don't like indented
pre-processor statements, or at least there were when I first wrote this. After
all, it had only been about 10 years then... */

#ifdef DEBUG
#define DPRINTF(p) printf p
#else
#define DPRINTF(p) /*nothing*/
#endif


/* Get the definitions provided by running "configure" */

#include "pcre-config.h"

/* Standard C headers plus the external interface definition. The only time
setjmp and stdarg are used is when NO_RECURSE is set. */

#include <ctype.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include the public PCRE header and the definitions of UCP character property
values. */

#include "pcre.h"

typedef unsigned short pcre_uint16;
typedef unsigned pcre_uint32;
typedef unsigned char uschar;

typedef JSRegExp pcre;

typedef JSRegExpChar pcre_char;
typedef JSRegExpChar pcre_uchar;
typedef const JSRegExpChar* USPTR;

/* Temporary fastMalloc/fastFree until we port to C++. */
#ifdef __cplusplus
extern "C" {
#endif
extern void* (*pcre_malloc)(size_t);
extern void (*pcre_free)(void*);
#ifdef __cplusplus
} /* extern "C" */
#endif

/* PCRE keeps offsets in its compiled code as 2-byte quantities (always stored
in big-endian order) by default. These are used, for example, to link from the
start of a subpattern to its alternatives and its end. The use of 2 bytes per
offset limits the size of the compiled regex to around 64K, which is big enough
for almost everybody. However, I received a request for an even bigger limit.
For this reason, and also to make the code easier to maintain, the storing and
loading of offsets from the byte string is now handled by the macros that are
defined here.

The macros are controlled by the value of LINK_SIZE. This defaults to 2 in
the config.h file, but can be overridden by using -D on the command line. This
is automated on Unix systems via the "configure" command. */

#if LINK_SIZE == 2

#define PUT(a,n,d)   \
  (a[n] = (d) >> 8), \
  (a[(n)+1] = (d) & 255)

#define GET(a,n) \
  (((a)[n] << 8) | (a)[(n)+1])

#define MAX_PATTERN_SIZE (1 << 16)


#elif LINK_SIZE == 3

#define PUT(a,n,d)       \
  (a[n] = (d) >> 16),    \
  (a[(n)+1] = (d) >> 8), \
  (a[(n)+2] = (d) & 255)

#define GET(a,n) \
  (((a)[n] << 16) | ((a)[(n)+1] << 8) | (a)[(n)+2])

#define MAX_PATTERN_SIZE (1 << 24)


#elif LINK_SIZE == 4

#define PUT(a,n,d)        \
  (a[n] = (d) >> 24),     \
  (a[(n)+1] = (d) >> 16), \
  (a[(n)+2] = (d) >> 8),  \
  (a[(n)+3] = (d) & 255)

#define GET(a,n) \
  (((a)[n] << 24) | ((a)[(n)+1] << 16) | ((a)[(n)+2] << 8) | (a)[(n)+3])

#define MAX_PATTERN_SIZE (1 << 30)   /* Keep it positive */


#else
#error LINK_SIZE must be either 2, 3, or 4
#endif


/* Convenience macro defined in terms of the others */

#define PUTINC(a,n,d)   PUT(a,n,d), a += LINK_SIZE


/* PCRE uses some other 2-byte quantities that do not change when the size of
offsets changes. There are used for repeat counts and for other things such as
capturing parenthesis numbers in back references. */

#define PUT2(a,n,d)   \
  a[n] = (d) >> 8; \
  a[(n)+1] = (d) & 255

#define GET2(a,n) \
  (((a)[n] << 8) | (a)[(n)+1])

#define PUT2INC(a,n,d)  PUT2(a,n,d), a += 2


/* When UTF-8 encoding is being used, a character is no longer just a single
byte. The macros for character handling generate simple sequences when used in
byte-mode, and more complicated ones for UTF-8 characters. */

/* Get the next UTF-8 character, not advancing the pointer, incrementing length
if there are extra bytes. This is called when we know we are in UTF-8 mode. */

#define GETUTF8CHARLEN(c, eptr, len) \
  c = *eptr; \
  if ((c & 0xc0) == 0xc0) \
    { \
    int gcii; \
    int gcaa = _pcre_utf8_table4[c & 0x3f];  /* Number of additional bytes */ \
    int gcss = 6*gcaa; \
    c = (c & _pcre_utf8_table3[gcaa]) << gcss; \
    for (gcii = 1; gcii <= gcaa; gcii++) \
      { \
      gcss -= 6; \
      c |= (eptr[gcii] & 0x3f) << gcss; \
      } \
    len += gcaa; \
    }

/* Get the next UTF-8 character, advancing the pointer. This is called when we
know we are in UTF-8 mode. */

#define GETUTF8CHARINC(c, eptr) \
c = *eptr++; \
if ((c & 0xc0) == 0xc0) \
{ \
  int gcaa = _pcre_utf8_table4[c & 0x3f];  /* Number of additional bytes */ \
    int gcss = 6*gcaa; \
      c = (c & _pcre_utf8_table3[gcaa]) << gcss; \
        while (gcaa-- > 0) \
        { \
          gcss -= 6; \
            c |= (*eptr++ & 0x3f) << gcss; \
        } \
}

#define LEAD_OFFSET (0xd800 - (0x10000 >> 10))
#define SURROGATE_OFFSET (0x10000 - (0xd800 << 10) - 0xdc00)

#define IS_LEADING_SURROGATE(c) (((c) & ~0x3ff) == 0xd800)
#define IS_TRAILING_SURROGATE(c) (((c) & ~0x3ff) == 0xdc00)

#define DECODE_SURROGATE_PAIR(l, t) (((l) << 10) + (t) + SURROGATE_OFFSET)
#define LEADING_SURROGATE(c) (LEAD_OFFSET + ((c) >> 10))
#define TRAILING_SURROGATE(c) (0xdc00 + ((c) & 0x3FF))

#define GETCHAR(c, eptr) \
  c = eptr[0]; \
  if (IS_LEADING_SURROGATE(c)) \
    c = DECODE_SURROGATE_PAIR(c, eptr[1])

#define GETCHARTEST(c, eptr) GETCHAR(c, eptr)

#define GETCHARINC(c, eptr) \
  c = *eptr++; \
  if (IS_LEADING_SURROGATE(c)) \
    c = DECODE_SURROGATE_PAIR(c, *eptr++)

#define GETCHARINCTEST(c, eptr) GETCHARINC(c, eptr)

#define GETCHARLEN(c, eptr, len) \
  c = eptr[0]; \
  if (IS_LEADING_SURROGATE(c)) \
    { \
    c = DECODE_SURROGATE_PAIR(c, eptr[1]); \
    ++len; \
    }

#define GETCHARLENEND(c, eptr, end, len) \
  c = eptr[0]; \
  if (IS_LEADING_SURROGATE(c)) \
    { \
    c = DECODE_SURROGATE_PAIR(c, eptr + 1 < end ? eptr[1] : 0); \
    ++len; \
    }

#define ISMIDCHAR(c) IS_TRAILING_SURROGATE(c)

/* If the pointer is not at the start of a character, move it back until
it is. Called only in UTF-8 mode. */

#define BACKCHAR(eptr) while(ISMIDCHAR(*eptr)) eptr--;


/* In case there is no definition of offsetof() provided - though any proper
Standard C system should have one. */

#ifndef offsetof
#define offsetof(p_type,field) ((size_t)&(((p_type *)0)->field))
#endif


/* Private options flags start at the most significant end of the four bytes,
but skip the top bit so we can use ints for convenience without getting tangled
with negative values. The public options defined in pcre.h start at the least
significant end. Make sure they don't overlap! */

#define PCRE_FIRSTSET      0x40000000  /* first_byte is set */
#define PCRE_REQCHSET      0x20000000  /* req_byte is set */
#define PCRE_STARTLINE     0x10000000  /* start after \n for multiline */
#define PCRE_ANCHORED      0x02000000  /* can't use partial with this regex */
#define PCRE_CASELESS      JS_REGEXP_CASELESS
#define PCRE_MULTILINE     JS_REGEXP_MULTILINE

/* Negative values for the firstchar and reqchar variables */

#define REQ_UNSET (-2)
#define REQ_NONE  (-1)

/* The maximum remaining length of subject we are prepared to search for a
req_byte match. */

#define REQ_BYTE_MAX 1000

/* Flags added to firstbyte or reqbyte; a "non-literal" item is either a
variable-length repeat, or a anything other than literal characters. */

#define REQ_CASELESS 0x0100    /* indicates caselessness */
#define REQ_VARY     0x0200    /* reqbyte followed non-literal item */

/* Miscellaneous definitions */

typedef int BOOL;

#define FALSE   0
#define TRUE    1

/* Flag bits and data types for the extended class (OP_XCLASS) for classes that
contain UTF-8 characters with values greater than 255. */

#define XCL_NOT    0x01    /* Flag: this is a negative class */
#define XCL_MAP    0x02    /* Flag: a 32-byte map is present */

#define XCL_END       0    /* Marks end of individual items */
#define XCL_SINGLE    1    /* Single item (one multibyte char) follows */
#define XCL_RANGE     2    /* A range (two multibyte chars) follows */

/* These are escaped items that aren't just an encoding of a particular data
value such as \n. They must have non-zero values, as check_escape() returns
their negation. Also, they must appear in the same order as in the opcode
definitions below, up to ESC_z. There's a dummy for OP_ANY because it
corresponds to "." rather than an escape sequence. The final one must be
ESC_REF as subsequent values are used for \1, \2, \3, etc. There is are two
tests in the code for an escape greater than ESC_b and less than ESC_Z to
detect the types that may be repeated. These are the types that consume
characters. If any new escapes are put in between that don't consume a
character, that code will have to change. */

enum { ESC_B = 1, ESC_b, ESC_D, ESC_d, ESC_S, ESC_s, ESC_W, ESC_w, ESC_REF };

/* Opcode table: OP_BRA must be last, as all values >= it are used for brackets
that extract substrings. Starting from 1 (i.e. after OP_END), the values up to
OP_EOD must correspond in order to the list of escapes immediately above.
Note that whenever this list is updated, the two macro definitions that follow
must also be updated to match. */

enum {
  OP_END,                   /* End of pattern */

  /* Values corresponding to backslashed metacharacters */

  OP_NOT_WORD_BOUNDARY,     /* \B */
  OP_WORD_BOUNDARY,         /* \b */
  OP_NOT_DIGIT,             /* \D */
  OP_DIGIT,                 /* \d */
  OP_NOT_WHITESPACE,        /* \S */
  OP_WHITESPACE,            /* \s */
  OP_NOT_WORDCHAR,          /* \W */
  OP_WORDCHAR,              /* \w */

  OP_ANY,                   /* . -- Match any character */

  OP_CIRC,                  /* ^ */
  OP_DOLL,                  /* $ */
  OP_CHAR,                  /* Match one character, casefully */
  OP_CHARNC,                /* Match one character, caselessly */
  OP_ASCII_CHAR,            /* Match one ASCII (0-127) character. */
  OP_ASCII_LETTER_NC,       /* Match one ASCII letter, caselessly. */
  OP_NOT,                   /* Match anything but the following char */

  OP_STAR,                  /* The maximizing and minimizing versions of */
  OP_MINSTAR,               /* all these opcodes must come in pairs, with */
  OP_PLUS,                  /* the minimizing one second. */
  OP_MINPLUS,               /* This first set applies to single characters */
  OP_QUERY,
  OP_MINQUERY,
  OP_UPTO,                  /* From 0 to n matches */
  OP_MINUPTO,
  OP_EXACT,                 /* Exactly n matches */

  OP_NOTSTAR,               /* This set applies to "not" single characters */
  OP_NOTMINSTAR,
  OP_NOTPLUS,
  OP_NOTMINPLUS,
  OP_NOTQUERY,
  OP_NOTMINQUERY,
  OP_NOTUPTO,
  OP_NOTMINUPTO,
  OP_NOTEXACT,

  OP_TYPESTAR,              /* This set applies to character types such as \d */
  OP_TYPEMINSTAR,
  OP_TYPEPLUS,
  OP_TYPEMINPLUS,
  OP_TYPEQUERY,
  OP_TYPEMINQUERY,
  OP_TYPEUPTO,
  OP_TYPEMINUPTO,
  OP_TYPEEXACT,

  OP_CRSTAR,                /* These are for character classes and back refs */
  OP_CRMINSTAR,
  OP_CRPLUS,
  OP_CRMINPLUS,
  OP_CRQUERY,
  OP_CRMINQUERY,
  OP_CRRANGE,               /* These are different to the three sets above. */
  OP_CRMINRANGE,

  OP_CLASS,                 /* Match a character class, chars < 256 only */
  OP_NCLASS,                /* Same, but the bitmap was created from a negative
                               class - the difference is relevant when a UTF-8
                               character > 255 is encountered. */

  OP_XCLASS,                /* Extended class for handling UTF-8 chars within the
                               class. This does both positive and negative. */

  OP_REF,                   /* Match a back reference */

  OP_ALT,                   /* Start of alternation */
  OP_KET,                   /* End of group that doesn't have an unbounded repeat */
  OP_KETRMAX,               /* These two must remain together and in this */
  OP_KETRMIN,               /* order. They are for groups the repeat for ever. */

  /* The assertions must come before ONCE and COND */

  OP_ASSERT,                /* Positive lookahead */
  OP_ASSERT_NOT,            /* Negative lookahead */

  /* ONCE and COND must come after the assertions, with ONCE first, as there's
  a test for >= ONCE for a subpattern that isn't an assertion. */

  OP_ONCE,                  /* Once matched, don't back up into the subpattern */

  OP_BRAZERO,               /* These two must remain together and in this */
  OP_BRAMINZERO,            /* order. */

  OP_BRANUMBER,             /* Used for extracting brackets whose number is greater
                               than can fit into an opcode. */

  OP_BRA                    /* This and greater values are used for brackets that
                               extract substrings up to EXTRACT_BASIC_MAX. After
                               that, use is made of OP_BRANUMBER. */
};

/* WARNING WARNING WARNING: There is an implicit assumption in pcre.c and
study.c that all opcodes are less than 128 in value. This makes handling UTF-8
character sequences easier. */

/* The highest extraction number before we have to start using additional
bytes. (Originally PCRE didn't have support for extraction counts highter than
this number.) The value is limited by the number of opcodes left after OP_BRA,
i.e. 255 - OP_BRA. We actually set it a bit lower to leave room for additional
opcodes. */

#define EXTRACT_BASIC_MAX  100


/* This macro defines the length of fixed length operations in the compiled
regex. The lengths are used when searching for specific things, and also in the
debugging printing of a compiled regex. We use a macro so that it can be
defined close to the definitions of the opcodes themselves.

As things have been extended, some of these are no longer fixed lenths, but are
minima instead. For example, the length of a single-character repeat may vary
in UTF-8 mode. The code that uses this table must know about such things. */

#define OP_LENGTHS \
  1,                             /* End                                    */ \
  1, 1, 1, 1, 1, 1, 1, 1,        /* \B, \b, \D, \d, \S, \s, \W, \w         */ \
  1,                             /* Any                                    */ \
  1, 1,                          /* ^, $                                   */ \
  2, 2,                          /* Char, Charnc - minimum lengths         */ \
  2, 2,                          /* ASCII char or non-cased                */ \
  2,                             /* not                                    */ \
  /* Positive single-char repeats                            ** These are  */ \
  2, 2, 2, 2, 2, 2,              /* *, *?, +, +?, ?, ??      ** minima in  */ \
  4, 4, 4,                       /* upto, minupto, exact     ** UTF-8 mode */ \
  /* Negative single-char repeats - only for chars < 256                   */ \
  2, 2, 2, 2, 2, 2,              /* NOT *, *?, +, +?, ?, ??                */ \
  4, 4, 4,                       /* NOT upto, minupto, exact               */ \
  /* Positive type repeats                                                 */ \
  2, 2, 2, 2, 2, 2,              /* Type *, *?, +, +?, ?, ??               */ \
  4, 4, 4,                       /* Type upto, minupto, exact              */ \
  /* Character class & ref repeats                                         */ \
  1, 1, 1, 1, 1, 1,              /* *, *?, +, +?, ?, ??                    */ \
  5, 5,                          /* CRRANGE, CRMINRANGE                    */ \
 33,                             /* CLASS                                  */ \
 33,                             /* NCLASS                                 */ \
  0,                             /* XCLASS - variable length               */ \
  3,                             /* REF                                    */ \
  1+LINK_SIZE,                   /* Alt                                    */ \
  1+LINK_SIZE,                   /* Ket                                    */ \
  1+LINK_SIZE,                   /* KetRmax                                */ \
  1+LINK_SIZE,                   /* KetRmin                                */ \
  1+LINK_SIZE,                   /* Assert                                 */ \
  1+LINK_SIZE,                   /* Assert not                             */ \
  1+LINK_SIZE,                   /* Once                                   */ \
  1, 1,                          /* BRAZERO, BRAMINZERO                    */ \
  3,                             /* BRANUMBER                              */ \
  1+LINK_SIZE                    /* BRA                                    */ \


/* Error code numbers. They are given names so that they can more easily be
tracked. */

enum { ERR0,  ERR1,  ERR2,  ERR3,  ERR4,  ERR5,  ERR6,  ERR7,  ERR8,  ERR9,
       ERR10, ERR11, ERR12, ERR13, ERR14, ERR15, ERR16, ERR17, ERR18, ERR19,
       ERR20, ERR21, ERR22, ERR23, ERR24, ERR25, ERR26, ERR27, ERR28, ERR29,
       ERR30, ERR31, ERR32, ERR33, ERR34, ERR35, ERR36, ERR37, ERR38, ERR39,
       ERR40, ERR41, ERR42, ERR43, ERR44, ERR45, ERR46, ERR47 };

/* The real format of the start of the pcre block; the index of names and the
code vector run on as long as necessary after the end. We store an explicit
offset to the name table so that if a regex is compiled on one host, saved, and
then run on another where the size of pointers is different, all might still
be well. For the case of compiled-on-4 and run-on-8, we include an extra
pointer that is always NULL. For future-proofing, a few dummy fields were
originally included - even though you can never get this planning right - but
there is only one left now.

NOTE NOTE NOTE:
Because people can now save and re-use compiled patterns, any additions to this
structure should be made at the end, and something earlier (e.g. a new
flag in the options or one of the dummy fields) should indicate that the new
fields are present. Currently PCRE always sets the dummy fields to zero.
NOTE NOTE NOTE:
*/

typedef struct real_pcre {
  pcre_uint32 size;               /* Total that was malloced */
  pcre_uint32 options;

  pcre_uint16 top_bracket;
  pcre_uint16 top_backref;
  pcre_uint16 first_byte;
  pcre_uint16 req_byte;
} real_pcre;

/* Structure for passing "static" information around between the functions
doing the compiling, so that they are thread-safe. */

typedef struct compile_data {
  const uschar *lcc;            /* Points to lower casing table */
  const uschar *fcc;            /* Points to case-flipping table */
  const uschar *cbits;          /* Points to character type table */
  const uschar *ctypes;         /* Points to table of type maps */
  const uschar *start_code;     /* The start of the compiled code */
  const pcre_uchar *start_pattern;   /* The start of the pattern */
  int  top_backref;             /* Maximum back reference */
  unsigned int backref_map;     /* Bitmap of low back refs */
  int  req_varyopt;             /* "After variable item" flag for reqbyte */
} compile_data;

/* When compiling in a mode that doesn't use recursive calls to match(),
a structure is used to remember local variables on the heap. It is defined in
pcre.c, close to the match() function, so that it is easy to keep it in step
with any changes of local variable. However, the pointer to the current frame
must be saved in some "static" place over a longjmp(). We declare the
structure here so that we can put a pointer in the match_data structure.
NOTE: This isn't used for a "normal" compilation of pcre. */

struct heapframe;

/* Structure for passing "static" information around between the functions
doing traditional NFA matching, so that they are thread-safe. */

typedef struct match_data {
  unsigned long int match_call_count;      /* As it says */
  int   *offset_vector;         /* Offset vector */
  int    offset_end;            /* One past the end */
  int    offset_max;            /* The maximum usable for return data */
  const uschar *lcc;            /* Points to lower casing table */
  const uschar *ctypes;         /* Points to table of type maps */
  BOOL   offset_overflow;       /* Set if too many extractions */
  USPTR  start_subject;         /* Start of the subject string */
  USPTR  end_subject;           /* End of the subject string */
  USPTR  start_match;           /* Start of this match attempt */
  USPTR  end_match_ptr;         /* Subject position at end match */
  int    end_offset_top;        /* Highwater mark at end of match */
  BOOL   multiline;
  BOOL   caseless;
} match_data;

/* Bit definitions for entries in the pcre_ctypes table. */

#define ctype_space   0x01
#define ctype_digit   0x04
#define ctype_xdigit  0x08
#define ctype_word    0x10   /* alphameric or '_' */

/* Offsets for the bitmap tables in pcre_cbits. Each table contains a set
of bits for a class map. Some classes are built by combining these tables. */

#define cbit_space     0      /* \s */
#define cbit_digit    32      /* \d */
#define cbit_word     64      /* \w */
#define cbit_length   96      /* Length of the cbits table */

/* Offsets of the various tables from the base tables pointer, and
total length. */

#define lcc_offset      0
#define fcc_offset    256
#define cbits_offset  512
#define ctypes_offset (cbits_offset + cbit_length)
#define tables_length (ctypes_offset + 128)

/* Layout of the UCP type table that translates property names into types and
codes. */

typedef struct {
  const char *name;
  pcre_uint16 type;
  pcre_uint16 value;
} ucp_type_table;


/* Internal shared data tables. These are tables that are used by more than one
of the exported public functions. They have to be "external" in the C sense,
but are not part of the PCRE public API. The data for these tables is in the
pcre_tables.c module. */

extern const int    _pcre_utf8_table1[];
extern const int    _pcre_utf8_table2[];
extern const int    _pcre_utf8_table3[];
extern const uschar _pcre_utf8_table4[];

extern const int    _pcre_utf8_table1_size;

extern const uschar _pcre_default_tables[];

extern const uschar _pcre_OP_lengths[];


/* Internal shared functions. These are functions that are used by more than
one of the exported public functions. They have to be "external" in the C
sense, but are not part of the PCRE public API. */

extern int         _pcre_ord2utf8(int, uschar *);
extern int         _pcre_ucp_othercase(const int);
extern BOOL        _pcre_xclass(int, const uschar *);

#define IS_NEWLINE(nl) ((nl) == 0xA || (nl) == 0xD || (nl) == 0x2028 || (nl) == 0x2029)

#endif

/* End of pcre_internal.h */
