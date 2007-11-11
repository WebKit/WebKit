/* This is JavaScriptCore's variant of the PCRE library. While this library
started out as a copy of PCRE, many of the features of PCRE have been
removed. This library now supports only the regular expression features
required by the JavaScript language specification, and has only the functions
needed by JavaScriptCore and the rest of WebKit.

                 Originally written by Philip Hazel
           Copyright (c) 1997-2006 University of Cambridge
    Copyright (C) 2002, 2004, 2006, 2007 Apple Inc. All rights reserved.

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

#ifndef DFTABLES

#include "Assertions.h"

#if COMPILER(MSVC)
#pragma warning(disable: 4232)
#pragma warning(disable: 4244)
#endif

/* The value of LINK_SIZE determines the number of bytes used to store links as
offsets within the compiled regex. The default is 2, which allows for compiled
patterns up to 64K long. This covers the vast majority of cases. However, PCRE
can also be compiled to use 3 or 4 bytes instead. This allows for longer
patterns in extreme cases. On systems that support it, "configure" can be used
to override this default. */

#define LINK_SIZE   2

/* The value of MATCH_LIMIT determines the default number of times the internal
match() function can be called during a single execution of pcre_exec(). There
is a runtime interface for setting a different limit. The limit exists in order
to catch runaway regular expressions that take for ever to determine that they
do not match. The default is set very large so that it does not accidentally
catch legitimate cases. On systems that support it, "configure" can be used to
override this default default. */

#define MATCH_LIMIT 10000000

/* The above limit applies to all calls of match(), whether or not they
increase the recursion depth. In some environments it is desirable to limit the
depth of recursive calls of match() more strictly, in order to restrict the
maximum amount of stack (or heap, if NO_RECURSE is defined) that is used. The
value of MATCH_LIMIT_RECURSION applies only to recursive calls of match(). To
have any useful effect, it must be less than the value of MATCH_LIMIT. There is
a runtime method for setting a different limit. On systems that support it,
"configure" can be used to override this default default. */

#define MATCH_LIMIT_RECURSION MATCH_LIMIT

#define _pcre_default_tables kjs_pcre_default_tables
#define _pcre_ord2utf8 kjs_pcre_ord2utf8
#define _pcre_utf8_table1 kjs_pcre_utf8_table1
#define _pcre_utf8_table1_size  kjs_pcre_utf8_table1_size
#define _pcre_utf8_table2 kjs_pcre_utf8_table2
#define _pcre_utf8_table3 kjs_pcre_utf8_table3
#define _pcre_utf8_table4 kjs_pcre_utf8_table4
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

typedef UChar pcre_char;
typedef UChar pcre_uchar;
typedef const UChar* USPTR;

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

#define BACKCHAR(eptr) while(ISMIDCHAR(*eptr)) eptr--;

#define PCRE_FIRSTSET      0x40000000  /* first_byte is set */
#define PCRE_REQCHSET      0x20000000  /* req_byte is set */
#define PCRE_STARTLINE     0x10000000  /* start after \n for multiline */
#define PCRE_ANCHORED      0x02000000  /* can't use partial with this regex */
#define PCRE_CASELESS      0x00000001
#define PCRE_MULTILINE     0x00000002

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

#define FOR_EACH_OPCODE(macro) \
    macro(END) \
    \
    macro(NOT_WORD_BOUNDARY) \
    macro(WORD_BOUNDARY) \
    macro(NOT_DIGIT) \
    macro(DIGIT) \
    macro(NOT_WHITESPACE) \
    macro(WHITESPACE) \
    macro(NOT_WORDCHAR) \
    macro(WORDCHAR) \
    \
    macro(ANY) \
    \
    macro(CIRC) \
    macro(DOLL) \
    macro(CHAR) \
    macro(CHARNC) \
    macro(ASCII_CHAR) \
    macro(ASCII_LETTER_NC) \
    macro(NOT) \
    \
    macro(STAR) \
    macro(MINSTAR) \
    macro(PLUS) \
    macro(MINPLUS) \
    macro(QUERY) \
    macro(MINQUERY) \
    macro(UPTO) \
    macro(MINUPTO) \
    macro(EXACT) \
    \
    macro(NOTSTAR) \
    macro(NOTMINSTAR) \
    macro(NOTPLUS) \
    macro(NOTMINPLUS) \
    macro(NOTQUERY) \
    macro(NOTMINQUERY) \
    macro(NOTUPTO) \
    macro(NOTMINUPTO) \
    macro(NOTEXACT) \
    \
    macro(TYPESTAR) \
    macro(TYPEMINSTAR) \
    macro(TYPEPLUS) \
    macro(TYPEMINPLUS) \
    macro(TYPEQUERY) \
    macro(TYPEMINQUERY) \
    macro(TYPEUPTO) \
    macro(TYPEMINUPTO) \
    macro(TYPEEXACT) \
    \
    macro(CRSTAR) \
    macro(CRMINSTAR) \
    macro(CRPLUS) \
    macro(CRMINPLUS) \
    macro(CRQUERY) \
    macro(CRMINQUERY) \
    macro(CRRANGE) \
    macro(CRMINRANGE) \
    \
    macro(CLASS) \
    macro(NCLASS) \
    macro(XCLASS) \
    \
    macro(REF) \
    \
    macro(ALT) \
    macro(KET) \
    macro(KETRMAX) \
    macro(KETRMIN) \
    \
    macro(ASSERT) \
    macro(ASSERT_NOT) \
    \
    macro(ONCE) \
    \
    macro(BRAZERO) \
    macro(BRAMINZERO) \
    macro(BRANUMBER) \
    macro(BRA)

#define OPCODE_ENUM_VALUE(opcode) OP_##opcode,
enum { FOR_EACH_OPCODE(OPCODE_ENUM_VALUE) };

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


/* Internal shared functions. These are functions that are used by more than
one of the exported public functions. They have to be "external" in the C
sense, but are not part of the PCRE public API. */

extern int         _pcre_ord2utf8(int, uschar *);
extern int         _pcre_ucp_othercase(const int);
extern BOOL        _pcre_xclass(int, const uschar *);

#define IS_NEWLINE(nl) ((nl) == 0xA || (nl) == 0xD || (nl) == 0x2028 || (nl) == 0x2029)

#endif

/* Bit definitions for entries in the pcre_ctypes table. */

#define ctype_space   0x01
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

#endif

/* End of pcre_internal.h */
