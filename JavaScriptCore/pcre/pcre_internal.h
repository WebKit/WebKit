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
#define fcc_offset    128
#define cbits_offset  256
#define ctypes_offset (cbits_offset + cbit_length)
#define tables_length (ctypes_offset + 128)

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

/* The below limit restricts the number of recursive match calls in order to
limit the maximum amount of stack (or heap, if NO_RECURSE is defined) that is used. The
value of MATCH_LIMIT_RECURSION applies only to recursive calls of match().
 
 This limit is tied to the size of MatchFrame.  Right now we allow PCRE to allocate up
 to MATCH_LIMIT_RECURSION - 16 * sizeof(MatchFrame) bytes of "stack" space before we give up.
 Currently that's 100000 - 16 * (23 * 4)  ~ 90MB
 */

#define MATCH_LIMIT_RECURSION 100000

#define _pcre_default_tables kjs_pcre_default_tables
#define _pcre_ord2utf8 kjs_pcre_ord2utf8
#define _pcre_utf8_table1 kjs_pcre_utf8_table1
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

static inline void putOpcodeValueAtOffset(uschar* opcodePtr, size_t offset, unsigned short value)
{
    opcodePtr[offset] = value >> 8;
    opcodePtr[offset + 1] = value & 255;
}

static inline short getOpcodeValueAtOffset(const uschar* opcodePtr, size_t offset)
{
    return ((opcodePtr[offset] << 8) | opcodePtr[offset + 1]);
}

#define MAX_PATTERN_SIZE (1 << 16)

#elif LINK_SIZE == 3

static inline void putOpcodeValueAtOffset(uschar* opcodePtr, size_t offset, unsigned value)
{
    ASSERT(!(value & 0xFF000000)); // This function only allows values < 2^24
    opcodePtr[offset] = value >> 16;
    opcodePtr[offset + 1] = value >> 8;
    opcodePtr[offset + 2] = value & 255;
}

static inline int getOpcodeValueAtOffset(const uschar* opcodePtr, size_t offset)
{
    return ((opcodePtr[offset] << 16) | (opcodePtr[offset + 1] << 8) | opcodePtr[offset + 2]);
}

#define MAX_PATTERN_SIZE (1 << 24)

#elif LINK_SIZE == 4

static inline void putOpcodeValueAtOffset(uschar* opcodePtr, size_t offset, unsigned value)
{
    opcodePtr[offset] = value >> 24;
    opcodePtr[offset + 1] = value >> 16;
    opcodePtr[offset + 2] = value >> 8;
    opcodePtr[offset + 3] = value & 255;
}

static inline int getOpcodeValueAtOffset(const uschar* opcodePtr, size_t offset)
{
    return ((opcodePtr[offset] << 24) | (opcodePtr[offset + 1] << 16) | (opcodePtr[offset + 2] << 8) | opcodePtr[offset + 3]);
}

#define MAX_PATTERN_SIZE (1 << 30)   /* Keep it positive */

#else
#error LINK_SIZE must be either 2, 3, or 4
#endif

static inline void putOpcodeValueAtOffsetAndAdvance(uschar*& opcodePtr, size_t offset, unsigned short value)
{
    putOpcodeValueAtOffset(opcodePtr, offset, value);
    opcodePtr += LINK_SIZE;
}

/* PCRE uses some other 2-byte quantities that do not change when the size of
offsets changes. There are used for repeat counts and for other things such as
capturing parenthesis numbers in back references. */

static inline void put2ByteOpcodeValueAtOffset(uschar* opcodePtr, size_t offset, unsigned short value)
{
    opcodePtr[offset] = value >> 8;
    opcodePtr[offset + 1] = value & 255;
}

static inline short get2ByteOpcodeValueAtOffset(const uschar* opcodePtr, size_t offset)
{
    return ((opcodePtr[offset] << 8) | opcodePtr[offset + 1]);
}

static inline void put2ByteOpcodeValueAtOffsetAndAdvance(uschar*& opcodePtr, size_t offset, unsigned short value)
{
    put2ByteOpcodeValueAtOffset(opcodePtr, offset, value);
    opcodePtr += 2;
}

#define LEAD_OFFSET (0xd800 - (0x10000 >> 10))
#define SURROGATE_OFFSET (0x10000 - (0xd800 << 10) - 0xdc00)

static inline bool isLeadingSurrogate(int c)
{
    return ((c & ~0x3ff) == 0xd800);
}

static inline bool isTrailingSurrogate(int c)
{
    return ((c & ~0x3ff) == 0xdc00);
}

static inline int decodeSurrogatePair(int leadingSurrogate, int trailingSurrogate)
{
    return ((leadingSurrogate << 10) + trailingSurrogate + SURROGATE_OFFSET);
}

static inline int getChar(const UChar* subjectPtr)
{
    int c = subjectPtr[0];
    if (isLeadingSurrogate(c))
        c = decodeSurrogatePair(c, subjectPtr[1]);
    return c;
}

static inline int getCharAndAdvance(const UChar*& subjectPtr)
{
    int c = *subjectPtr++;
    if (isLeadingSurrogate(c))
        c = decodeSurrogatePair(c, *subjectPtr++);
    return c;
}

static inline int getCharAndLength(const UChar*& subjectPtr, int& length)
{
    int c = subjectPtr[0];
    if (isLeadingSurrogate(c)) {
        c = decodeSurrogatePair(c, subjectPtr[1]);
        length = 2;
    } else
        length = 1;
    return c;
}

// FIXME: All (2) calls to this funtion should be removed and replaced with
// calls to getCharAndAdvance
static inline int getCharAndAdvanceIfSurrogate(const UChar*& subjectPtr)
{
    int c = subjectPtr[0];
    if (isLeadingSurrogate(c)) {
        c = decodeSurrogatePair(c, subjectPtr[1]);
        subjectPtr++;
    }
    return c;
}

// This flavor checks to make sure we don't walk off the end
// FIXME: This could also be removed and an end-aware getCharAndAdvance added instead.
static inline int getCharAndAdvanceIfSurrogate(const UChar*& subjectPtr, const UChar* end)
{
    int c = subjectPtr[0];
    if (isLeadingSurrogate(c)) {
        if (subjectPtr + 1 < end)
            c = decodeSurrogatePair(c, subjectPtr[1]);
        else
            c = decodeSurrogatePair(c, 0);
        subjectPtr++;
    }
    return c;
}

static inline int getPreviousChar(const UChar* subjectPtr)
{
    int valueAtSubjectMinusOne = subjectPtr[-1];
    if (isTrailingSurrogate(valueAtSubjectMinusOne))
        return decodeSurrogatePair(subjectPtr[-2], valueAtSubjectMinusOne);
    return valueAtSubjectMinusOne;
}

static inline void movePtrToPreviousChar(const UChar*& subjectPtr)
{
    subjectPtr--;
    if (isTrailingSurrogate(*subjectPtr))
        subjectPtr--;
}

static inline bool movePtrToNextChar(const UChar*& subjectPtr, const UChar* endSubject)
{
    if (subjectPtr < endSubject) {
        subjectPtr++;
        if (subjectPtr < endSubject && isTrailingSurrogate(*subjectPtr)) {
            subjectPtr++;
            return subjectPtr < endSubject;
        }
        return true;
    }
    return false;
}

static inline void movePtrToStartOfCurrentChar(const UChar*& subjectPtr)
{
    if (isTrailingSurrogate(*subjectPtr))
        subjectPtr--;
}

// FIXME: These are really more of a "compiled regexp state" than "regexp options"
enum RegExpOptions {
    UseFirstByteOptimizationOption = 0x40000000,  /* first_byte is set */
    UseRequiredByteOptimizationOption = 0x20000000,  /* req_byte is set */
    UseMultiLineFirstByteOptimizationOption = 0x10000000,  /* start after \n for multiline */
    IsAnchoredOption = 0x02000000,  /* can't use partial with this regex */
    IgnoreCaseOption = 0x00000001,
    MatchAcrossMultipleLinesOption = 0x00000002
};

/* Negative values for the firstchar and reqchar variables */

#define REQ_UNSET (-2)
#define REQ_NONE  (-1)

/* The maximum remaining length of subject we are prepared to search for a
req_byte match. */

#define REQ_BYTE_MAX 1000

/* Flags added to firstbyte or reqbyte; a "non-literal" item is either a
variable-length repeat, or a anything other than literal characters. */

#define REQ_IGNORE_CASE 0x0100    /* indicates should ignore case */
#define REQ_VARY     0x0200    /* reqbyte followed non-literal item */

/* Miscellaneous definitions */

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
definitions below, up to ESC_z. There's a dummy for OP_ANY_CHAR because it
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
    macro(ANY_CHAR) \
    \
    macro(CIRC) \
    macro(DOLL) \
    macro(CHAR) \
    macro(CHAR_IGNORING_CASE) \
    macro(ASCII_CHAR) \
    macro(ASCII_LETTER_IGNORING_CASE) \
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
  1 + LINK_SIZE,                   /* Alt                                    */ \
  1 + LINK_SIZE,                   /* Ket                                    */ \
  1 + LINK_SIZE,                   /* KetRmax                                */ \
  1 + LINK_SIZE,                   /* KetRmin                                */ \
  1 + LINK_SIZE,                   /* Assert                                 */ \
  1 + LINK_SIZE,                   /* Assert not                             */ \
  1 + LINK_SIZE,                   /* Once                                   */ \
  1, 1,                          /* BRAZERO, BRAMINZERO                    */ \
  3,                             /* BRANUMBER                              */ \
  1 + LINK_SIZE                    /* BRA                                    */ \


/* The index of names and the
code vector run on as long as necessary after the end. We store an explicit
offset to the name table so that if a regex is compiled on one host, saved, and
then run on another where the size of pointers is different, all might still
be well. For the case of compiled-on-4 and run-on-8, we include an extra
pointer that is always NULL.
*/

struct JSRegExp {
    pcre_uint32 size;               /* Total that was malloced */
    pcre_uint32 options;

    pcre_uint16 top_bracket;
    pcre_uint16 top_backref;
    
    // jsRegExpExecute && jsRegExpCompile currently only how to handle ASCII
    // chars for thse optimizations, however it would be trivial to add support
    // for optimized UChar first_byte/req_byte scans
    unsigned char first_byte;
    unsigned char req_byte;
};

/* Internal shared data tables. These are tables that are used by more than one
 of the exported public functions. They have to be "external" in the C sense,
 but are not part of the PCRE public API. The data for these tables is in the
 pcre_tables.c module. */

#define _pcre_utf8_table1_size 6

extern const int    _pcre_utf8_table1[6];
extern const int    _pcre_utf8_table2[6];
extern const int    _pcre_utf8_table3[6];
extern const uschar _pcre_utf8_table4[0x40];

extern const uschar _pcre_default_tables[tables_length];

static inline uschar toLowerCase(uschar c)
{
    static const uschar* lowerCaseChars = _pcre_default_tables + lcc_offset;
    return lowerCaseChars[c];
}

static inline uschar flipCase(uschar c)
{
    static const uschar* flippedCaseChars = _pcre_default_tables + fcc_offset;
    return flippedCaseChars[c];
}

static inline uschar classBitmapForChar(uschar c)
{
    static const uschar* charClassBitmaps = _pcre_default_tables + cbits_offset;
    return charClassBitmaps[c];
}

static inline uschar charTypeForChar(uschar c)
{
    const uschar* charTypeMap = _pcre_default_tables + ctypes_offset;
    return charTypeMap[c];
}

static inline bool isWordChar(UChar c)
{
    /* UTF8 Characters > 128 are assumed to be "non-word" characters. */
    return (c < 128 && (charTypeForChar(c) & ctype_word));
}

static inline bool isSpaceChar(UChar c)
{
    return (c < 128 && (charTypeForChar(c) & ctype_space));
}

/* Structure for passing "static" information around between the functions
doing the compiling, so that they are thread-safe. */

struct CompileData {
    CompileData() {
        start_code = 0;
        start_pattern = 0;
        top_backref = 0;
        backref_map = 0;
        req_varyopt = 0;
    }
  const uschar* start_code;     /* The start of the compiled code */
  const UChar* start_pattern;   /* The start of the pattern */
  int  top_backref;             /* Maximum back reference */
  unsigned int backref_map;     /* Bitmap of low back refs */
  int  req_varyopt;             /* "After variable item" flag for reqbyte */
};

/* Internal shared functions. These are functions that are used by more than
one of the exported public functions. They have to be "external" in the C
sense, but are not part of the PCRE public API. */

extern int         _pcre_ucp_othercase(const unsigned int);
extern bool        _pcre_xclass(int, const uschar*);

static inline bool isNewline(UChar nl)
{
    return (nl == 0xA || nl == 0xD || nl == 0x2028 || nl == 0x2029);
}

// FIXME: It's unclear to me if this moves the opcode ptr to the start of all branches
// or to the end of all branches -- ecs
// FIXME: This abstraction is poor since it assumes that you want to jump based on whatever
// the next value in the stream is, and *then* follow any OP_ALT branches.
static inline void moveOpcodePtrPastAnyAlternateBranches(const uschar*& opcodePtr)
{
    do {
        opcodePtr += getOpcodeValueAtOffset(opcodePtr, 1);
    } while (*opcodePtr == OP_ALT);
}

#endif

#endif

/* End of pcre_internal.h */
