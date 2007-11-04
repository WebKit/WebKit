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


/* This module contains the external function pcre_compile(), along with
supporting internal functions that are not used by other modules. */


#include "pcre_internal.h"


/* WARNING: These macros evaluate their parameters more than once. */
#define DIGITAB(x) ((x) < 128 ? digitab[(x)] : 0)


/* When DEBUG is defined, we need the pcre_printint() function, which is also
used by pcretest. DEBUG is not defined when building a production library. */

#ifdef DEBUG
#include "pcre_printint.src"
#endif



/*************************************************
*      Code parameters and static tables         *
*************************************************/

/* Maximum number of items on the nested bracket stacks at compile time. This
applies to the nesting of all kinds of parentheses. It does not limit
un-nested, non-capturing parentheses. This number can be made bigger if
necessary - it is used to dimension one int and one unsigned char vector at
compile time. */

#define BRASTACK_SIZE 200


/* Table for handling escaped characters in the range '0'-'z'. Positive returns
are simple data values; negative values are for special things like \d and so
on. Zero means further processing is needed (for things like \x), or the escape
is invalid. */

static const short int escapes[] = {
     0,      0,      0,      0,      0,      0,      0,      0,   /* 0 - 7 */
     0,      0,    ':',    ';',    '<',    '=',    '>',    '?',   /* 8 - ? */
   '@',      0, -ESC_B,      0, -ESC_D,      0,      0,      0,   /* @ - G */
     0,      0,      0,      0,      0,      0,      0,      0,   /* H - O */
     0,      0,      0, -ESC_S,      0,      0,      0, -ESC_W,   /* P - W */
     0,      0,      0,    '[',   '\\',    ']',    '^',    '_',   /* X - _ */
   '`',      7, -ESC_b,      0, -ESC_d,      0,  ESC_f,      0,   /* ` - g */
     0,      0,      0,      0,      0,      0,  ESC_n,      0,   /* h - o */
     0,      0,  ESC_r, -ESC_s,  ESC_tee,    0,  ESC_v, -ESC_w,   /* p - w */
     0,      0,      0                                            /* x - z */
};


/* The texts of compile-time error messages. These are "char *" because they
are passed to the outside world. */

static const char * const error_texts[] = {
  "no error",
  "\\ at end of pattern",
  "\\c at end of pattern",
  "unrecognized character follows \\",
  "numbers out of order in {} quantifier",
  /* 5 */
  "number too big in {} quantifier",
  "missing terminating ] for character class",
  "invalid escape sequence in character class",
  "range out of order in character class",
  "nothing to repeat",
  /* 10 */
  "operand of unlimited repeat could match the empty string",
  "internal error: unexpected repeat",
  "unrecognized character after (?",
  "POSIX named classes are supported only within a class",
  "missing )",
  /* 15 */
  "reference to non-existent subpattern",
  "erroffset passed as NULL",
  "unknown option bit(s) set",
  "missing ) after comment",
  "parentheses nested too deeply",
  /* 20 */
  "regular expression too large",
  "failed to get memory",
  "unmatched parentheses",
  "internal error: code overflow",
  "unrecognized character after (?<",
  /* 25 */
  "lookbehind assertion is not fixed length",
  "malformed number after (?(",
  "conditional group contains more than two branches",
  "assertion expected after (?(",
  "(?R or (?digits must be followed by )",
  /* 30 */
  "unknown POSIX class name",
  "POSIX collating elements are not supported",
  "this version of PCRE is not compiled with PCRE_UTF8 support",
  "spare error",
  "character value in \\x{...} sequence is too large",
  /* 35 */
  "invalid condition (?(0)",
  "\\C not allowed in lookbehind assertion",
  "PCRE does not support \\L, \\l, \\N, \\U, or \\u",
  "number after (?C is > 255",
  "closing ) for (?C expected",
  /* 40 */
  "recursive call could loop indefinitely",
  "unrecognized character after (?P",
  "syntax error after (?P",
  "two named groups have the same name",
  "invalid UTF-16 string",
  /* 45 */
  "support for \\P, \\p, and \\X has not been compiled",
  "malformed \\P or \\p sequence",
  "unknown property name after \\P or \\p"
};


/* Table to identify digits and hex digits. This is used when compiling
patterns. Note that the tables in chartables are dependent on the locale, and
may mark arbitrary characters as digits - but the PCRE compiling code expects
to handle only 0-9, a-z, and A-Z as digits when compiling. That is why we have
a private table here. It costs 256 bytes, but it is a lot faster than doing
character value tests (at least in some simple cases I timed), and in some
applications one wants PCRE to compile efficiently as well as match
efficiently.

For convenience, we use the same bit definitions as in chartables:

  0x04   decimal digit
  0x08   hexadecimal digit

Then we can use ctype_digit and ctype_xdigit in the code. */

static const unsigned char digitab[] =
  {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*   0-  7 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*   8- 15 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  16- 23 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  24- 31 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*    - '  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  ( - /  */
  0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c, /*  0 - 7  */
  0x0c,0x0c,0x00,0x00,0x00,0x00,0x00,0x00, /*  8 - ?  */
  0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x00, /*  @ - G  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  H - O  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  P - W  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  X - _  */
  0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x00, /*  ` - g  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  h - o  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  p - w  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /*  x -127 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 128-135 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 136-143 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 144-151 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 152-159 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 160-167 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 168-175 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 176-183 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 184-191 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 192-199 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 200-207 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 208-215 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 216-223 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 224-231 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 232-239 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 240-247 */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};/* 248-255 */


/* Definition to allow mutual recursion */

static BOOL
  compile_regex(int, int *, uschar **, const pcre_uchar **, const pcre_uchar *, int *, int,
    int *, int *, branch_chain *, compile_data *);



/*************************************************
*            Handle escapes                      *
*************************************************/

/* This function is called when a \ has been encountered. It either returns a
positive value for a simple escape such as \n, or a negative value which
encodes one of the more complicated things such as \d. When UTF-8 is enabled,
a positive value greater than 255 may be returned. On entry, ptr is pointing at
the \. On exit, it is on the final character of the escape sequence.

Arguments:
  ptrptr         points to the pattern position pointer
  errorcodeptr   points to the errorcode variable
  bracount       number of previous extracting brackets
  options        the options bits
  isclass        TRUE if inside a character class

Returns:         zero or positive => a data character
                 negative => a special escape sequence
                 on error, errorptr is set
*/

static int
check_escape(const pcre_uchar **ptrptr, const pcre_uchar *patternEnd, int *errorcodeptr, int bracount,
  BOOL isclass)
{
const pcre_uchar *ptr = *ptrptr + 1;
int c, i;

/* If backslash is at the end of the pattern, it's an error. */
if (ptr == patternEnd) {
    *errorcodeptr = ERR1;
    *ptrptr = ptr;
    return 0;
}

c = *ptr;

if (0) { } /* Matches with else below; to make merging easier. */

/* Non-alphamerics are literals. For digits or letters, do an initial lookup in
a table. A non-zero result is something that can be returned immediately.
Otherwise further processing may be required. */

else if (c < '0' || c > 'z') {}                           /* Not alphameric */
else if ((i = escapes[c - '0']) != 0) c = i;

/* Escapes that need further processing, or are illegal. */

else
  {
  const pcre_uchar *oldptr;
  switch (c)
    {
    /* A number of Perl escapes are not handled by PCRE. We give an explicit
    error. */

    /* The handling of escape sequences consisting of a string of digits
    starting with one that is not zero is not straightforward. By experiment,
    the way Perl works seems to be as follows:

    Outside a character class, the digits are read as a decimal number. If the
    number is less than 10, or if there are that many previous extracting
    left brackets, then it is a back reference. Otherwise, up to three octal
    digits are read to form an escaped byte. Thus \123 is likely to be octal
    123 (cf \0123, which is octal 012 followed by the literal 3). If the octal
    value is greater than 377, the least significant 8 bits are taken. Inside a
    character class, \ followed by a digit is always an octal number. */

    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':

    if (!isclass)
      {
      oldptr = ptr;
      c -= '0';
      while (ptr + 1 < patternEnd && (DIGITAB(ptr[1]) & ctype_digit) != 0)
        c = c * 10 + *(++ptr) - '0';
      if (c < 10 || c <= bracount)
        {
        c = -(ESC_REF + c);
        break;
        }
      ptr = oldptr;      /* Put the pointer back and fall through */
      }

    /* Handle an octal number following \. If the first digit is 8 or 9, Perl
    generates a binary zero byte and treats the digit as a following literal.
    Thus we have to pull back the pointer by one. */

    if ((c = *ptr) >= '8')
      {
      ptr--;
      c = 0;
      break;
      }

    /* \0 always starts an octal number, but we may drop through to here with a
    larger first octal digit. */

    case '0':
    c -= '0';
    while (i++ < 2 && ptr + 1 < patternEnd && ptr[1] >= '0' && ptr[1] <= '7')
        c = c * 8 + *(++ptr) - '0';
    c &= 255;     /* Take least significant 8 bits */
    break;

    /* \x is complicated. \x{ddd} is a character number which can be greater
    than 0xff in utf8 mode, but only if the ddd are hex digits. If not, { is
    treated as a data character. */

    case 'x':
    if (ptr + 1 < patternEnd && ptr[1] == '{')
      {
      const pcre_uchar *pt = ptr + 2;
      int count = 0;

      c = 0;
      while (pt < patternEnd && (DIGITAB(*pt) & ctype_xdigit) != 0)
        {
        register int cc = *pt++;
        if (c == 0 && cc == '0') continue;     /* Leading zeroes */
        count++;

        if (cc >= 'a') cc -= 32;               /* Convert to upper case */
        c = (c << 4) + cc - ((cc < 'A')? '0' : ('A' - 10));
        }

      if (pt < patternEnd && *pt == '}')
        {
        if (c < 0 || count > 8) *errorcodeptr = ERR34;
        else if (c >= 0xD800 && c <= 0xDFFF) *errorcodeptr = ERR34; // half of surrogate pair
        else if (c >= 0xFDD0 && c <= 0xFDEF) *errorcodeptr = ERR34; // ?
        else if (c == 0xFFFE) *errorcodeptr = ERR34; // not a character
        else if (c == 0xFFFF)  *errorcodeptr = ERR34; // not a character
        else if (c > 0x10FFFF) *errorcodeptr = ERR34; // out of Unicode character range
        ptr = pt;
        break;
        }

      /* If the sequence of hex digits does not end with '}', then we don't
      recognize this construct; fall through to the normal \x handling. */
      }

    /* Read just a single-byte hex-defined char */

    c = 0;
    while (i++ < 2 && ptr + 1 < patternEnd && (DIGITAB(ptr[1]) & ctype_xdigit) != 0)
      {
      int cc;                               /* Some compilers don't like ++ */
      cc = *(++ptr);                        /* in initializers */
      if (cc >= 'a') cc -= 32;              /* Convert to upper case */
      c = c * 16 + cc - ((cc < 'A')? '0' : ('A' - 10));
      }
    break;

    case 'u': {
    const pcre_uchar *pt = ptr;
    c = 0;
    while (i++ < 4)
      {
      if (pt + 1 >= patternEnd || (DIGITAB(pt[1]) & ctype_xdigit) == 0)
        {
        pt = ptr;
        c = 'u';
        break;
        }
      else
        {
        int cc;                              /* Some compilers don't like ++ */
        cc = *(++pt);                        /* in initializers */
        if (cc >= 'a') cc -= 32;             /* Convert to upper case */
        c = c * 16 + cc - ((cc < 'A')? '0' : ('A' - 10));
        }
      }
    ptr = pt;
    break;
    }

    /* Other special escapes not starting with a digit are straightforward */

    case 'c':
    if (++ptr == patternEnd)
      {
      *errorcodeptr = ERR2;
      return 0;
      }
    c = *ptr;

    /* A letter is upper-cased; then the 0x40 bit is flipped. This coding
    is ASCII-specific, but then the whole concept of \cx is ASCII-specific.
    (However, an EBCDIC equivalent has now been added.) */

    if (c >= 'a' && c <= 'z') c -= 32;
    c ^= 0x40;
    break;

    default:
    break;
    }
  }

*ptrptr = ptr;
return c;
}



/*************************************************
*            Check for counted repeat            *
*************************************************/

/* This function is called when a '{' is encountered in a place where it might
start a quantifier. It looks ahead to see if it really is a quantifier or not.
It is only a quantifier if it is one of the forms {ddd} {ddd,} or {ddd,ddd}
where the ddds are digits.

Arguments:
  p         pointer to the first char after '{'

Returns:    TRUE or FALSE
*/

static BOOL
is_counted_repeat(const pcre_uchar *p, const pcre_uchar *patternEnd)
{
if (p >= patternEnd || (DIGITAB(*p) & ctype_digit) == 0)
    return FALSE;
p++;
while (p < patternEnd && (DIGITAB(*p) & ctype_digit) != 0)
    p++;
if (p < patternEnd && *p == '}')
    return TRUE;

if (p >= patternEnd || *p++ != ',')
    return FALSE;
if (p < patternEnd && *p == '}')
    return TRUE;

if (p >= patternEnd || (DIGITAB(*p) & ctype_digit) == 0)
    return FALSE;
p++;
while (p < patternEnd && (DIGITAB(*p) & ctype_digit) != 0)
    p++;

return (p < patternEnd && *p == '}');
}



/*************************************************
*         Read repeat counts                     *
*************************************************/

/* Read an item of the form {n,m} and return the values. This is called only
after is_counted_repeat() has confirmed that a repeat-count quantifier exists,
so the syntax is guaranteed to be correct, but we need to check the values.

Arguments:
  p              pointer to first char after '{'
  minp           pointer to int for min
  maxp           pointer to int for max
                 returned as -1 if no max
  errorcodeptr   points to error code variable

Returns:         pointer to '}' on success;
                 current ptr on error, with errorcodeptr set non-zero
*/

static const pcre_uchar *
read_repeat_counts(const pcre_uchar *p, int *minp, int *maxp, int *errorcodeptr)
{
int min = 0;
int max = -1;

/* Read the minimum value and do a paranoid check: a negative value indicates
an integer overflow. */

while ((DIGITAB(*p) & ctype_digit) != 0) min = min * 10 + *p++ - '0';
if (min < 0 || min > 65535)
  {
  *errorcodeptr = ERR5;
  return p;
  }

/* Read the maximum value if there is one, and again do a paranoid on its size.
Also, max must not be less than min. */

if (*p == '}') max = min; else
  {
  if (*(++p) != '}')
    {
    max = 0;
    while((DIGITAB(*p) & ctype_digit) != 0) max = max * 10 + *p++ - '0';
    if (max < 0 || max > 65535)
      {
      *errorcodeptr = ERR5;
      return p;
      }
    if (max < min)
      {
      *errorcodeptr = ERR4;
      return p;
      }
    }
  }

/* Fill in the required variables, and pass back the pointer to the terminating
'}'. */

*minp = min;
*maxp = max;
return p;
}



/*************************************************
*      Find first significant op code            *
*************************************************/

/* This is called by several functions that scan a compiled expression looking
for a fixed first character, or an anchoring op code etc. It skips over things
that do not influence this. For some calls, a change of option is important.
For some calls, it makes sense to skip negative forward and all backward
assertions, and also the \b assertion; for others it does not.

Arguments:
  code         pointer to the start of the group
  skipassert   TRUE if certain assertions are to be skipped

Returns:       pointer to the first significant opcode
*/

static const uschar*
first_significant_code(const uschar *code, BOOL skipassert)
{
for (;;)
  {
  switch ((int)*code)
    {
    case OP_ASSERT_NOT:
    if (!skipassert) return code;
    do code += GET(code, 1); while (*code == OP_ALT);
    code += _pcre_OP_lengths[*code];
    break;

    case OP_WORD_BOUNDARY:
    case OP_NOT_WORD_BOUNDARY:
    if (!skipassert) return code;
    /* Fall through */

    case OP_BRANUMBER:
    code += _pcre_OP_lengths[*code];
    break;

    default:
    return code;
    }
  }
/* Control never reaches here */
}




/*************************************************
*        Find the fixed length of a pattern      *
*************************************************/

/* Scan a pattern and compute the fixed length of subject that will match it,
if the length is fixed. This is needed for dealing with backward assertions.
In UTF8 mode, the result is in characters rather than bytes.

Arguments:
  code     points to the start of the pattern (the bracket)
  options  the compiling options

Returns:   the fixed length, or -1 if there is no fixed length,
             or -2 if \C was encountered
*/

static int
find_fixedlength(uschar *code, int options)
{
int length = -1;

register int branchlength = 0;
register uschar *cc = code + 1 + LINK_SIZE;

/* Scan along the opcodes for this branch. If we get to the end of the
branch, check the length against that of the other branches. */

for (;;)
  {
  int d;
  register int op = *cc;
  if (op >= OP_BRA) op = OP_BRA;

  switch (op)
    {
    case OP_BRA:
    case OP_ONCE:
    d = find_fixedlength(cc, options);
    if (d < 0) return d;
    branchlength += d;
    do cc += GET(cc, 1); while (*cc == OP_ALT);
    cc += 1 + LINK_SIZE;
    break;

    /* Reached end of a branch; if it's a ket it is the end of a nested
    call. If it's ALT it is an alternation in a nested call. If it is
    END it's the end of the outer call. All can be handled by the same code. */

    case OP_ALT:
    case OP_KET:
    case OP_KETRMAX:
    case OP_KETRMIN:
    case OP_END:
    if (length < 0) length = branchlength;
      else if (length != branchlength) return -1;
    if (*cc != OP_ALT) return length;
    cc += 1 + LINK_SIZE;
    branchlength = 0;
    break;

    /* Skip over assertive subpatterns */

    case OP_ASSERT:
    case OP_ASSERT_NOT:
    do cc += GET(cc, 1); while (*cc == OP_ALT);
    /* Fall through */

    /* Skip over things that don't match chars */

    case OP_BRANUMBER:
    case OP_CIRC:
    case OP_DOLL:
    case OP_NOT_WORD_BOUNDARY:
    case OP_WORD_BOUNDARY:
    cc += _pcre_OP_lengths[*cc];
    break;

    /* Handle literal characters */

    case OP_CHAR:
    case OP_CHARNC:
    branchlength++;
    cc += 2;
      while ((*cc & 0xc0) == 0x80) cc++;
    break;

    /* Handle exact repetitions. The count is already in characters, but we
    need to skip over a multibyte character in UTF8 mode.  */

    case OP_EXACT:
    branchlength += GET2(cc,1);
    cc += 4;
      while((*cc & 0x80) == 0x80) cc++;
    break;

    case OP_TYPEEXACT:
    branchlength += GET2(cc,1);
    cc += 4;
    break;

    /* Handle single-char matchers */

    case OP_NOT_DIGIT:
    case OP_DIGIT:
    case OP_NOT_WHITESPACE:
    case OP_WHITESPACE:
    case OP_NOT_WORDCHAR:
    case OP_WORDCHAR:
    case OP_ANY:
    branchlength++;
    cc++;
    break;

    /* Check a class for variable quantification */

    case OP_XCLASS:
    cc += GET(cc, 1) - 33;
    /* Fall through */

    case OP_CLASS:
    case OP_NCLASS:
    cc += 33;

    switch (*cc)
      {
      case OP_CRSTAR:
      case OP_CRMINSTAR:
      case OP_CRQUERY:
      case OP_CRMINQUERY:
      return -1;

      case OP_CRRANGE:
      case OP_CRMINRANGE:
      if (GET2(cc,1) != GET2(cc,3)) return -1;
      branchlength += GET2(cc,1);
      cc += 5;
      break;

      default:
      branchlength++;
      }
    break;

    /* Anything else is variable length */

    default:
    return -1;
    }
  }
/* Control never gets here */
}




/*************************************************
*    Scan compiled branch for non-emptiness      *
*************************************************/

/* This function scans through a branch of a compiled pattern to see whether it
can match the empty string or not. It is called only from could_be_empty()
below. Note that first_significant_code() skips over assertions. If we hit an
unclosed bracket, we return "empty" - this means we've struck an inner bracket
whose current branch will already have been scanned.

Arguments:
  code        points to start of search
  endcode     points to where to stop

Returns:      TRUE if what is matched could be empty
*/

static BOOL
could_be_empty_branch(const uschar *code, const uschar *endcode)
{
register int c;
for (code = first_significant_code(code + 1 + LINK_SIZE, TRUE);
     code < endcode;
     code = first_significant_code(code + _pcre_OP_lengths[c], TRUE))
  {
  const uschar *ccode;

  c = *code;

  if (c >= OP_BRA)
    {
    BOOL empty_branch;
    if (GET(code, 1) == 0) return TRUE;    /* Hit unclosed bracket */

    /* Scan a closed bracket */

    empty_branch = FALSE;
    do
      {
      if (!empty_branch && could_be_empty_branch(code, endcode))
        empty_branch = TRUE;
      code += GET(code, 1);
      }
    while (*code == OP_ALT);
    if (!empty_branch) return FALSE;   /* All branches are non-empty */
    code += 1 + LINK_SIZE;
    c = *code;
    }

  else switch (c)
    {
    /* Check for quantifiers after a class */

    case OP_XCLASS:
    ccode = code + GET(code, 1);
    goto CHECK_CLASS_REPEAT;

    case OP_CLASS:
    case OP_NCLASS:
    ccode = code + 33;

    CHECK_CLASS_REPEAT:

    switch (*ccode)
      {
      case OP_CRSTAR:            /* These could be empty; continue */
      case OP_CRMINSTAR:
      case OP_CRQUERY:
      case OP_CRMINQUERY:
      break;

      default:                   /* Non-repeat => class must match */
      case OP_CRPLUS:            /* These repeats aren't empty */
      case OP_CRMINPLUS:
      return FALSE;

      case OP_CRRANGE:
      case OP_CRMINRANGE:
      if (GET2(ccode, 1) > 0) return FALSE;  /* Minimum > 0 */
      break;
      }
    break;

    /* Opcodes that must match a character */

    case OP_NOT_DIGIT:
    case OP_DIGIT:
    case OP_NOT_WHITESPACE:
    case OP_WHITESPACE:
    case OP_NOT_WORDCHAR:
    case OP_WORDCHAR:
    case OP_ANY:
    case OP_CHAR:
    case OP_CHARNC:
    case OP_NOT:
    case OP_PLUS:
    case OP_MINPLUS:
    case OP_EXACT:
    case OP_NOTPLUS:
    case OP_NOTMINPLUS:
    case OP_NOTEXACT:
    case OP_TYPEPLUS:
    case OP_TYPEMINPLUS:
    case OP_TYPEEXACT:
    return FALSE;

    /* End of branch */

    case OP_KET:
    case OP_KETRMAX:
    case OP_KETRMIN:
    case OP_ALT:
    return TRUE;

    /* In UTF-8 mode, STAR, MINSTAR, QUERY, MINQUERY, UPTO, and MINUPTO  may be
    followed by a multibyte character */

    case OP_STAR:
    case OP_MINSTAR:
    case OP_QUERY:
    case OP_MINQUERY:
    case OP_UPTO:
    case OP_MINUPTO:
    while ((code[2] & 0xc0) == 0x80) code++;
    break;
    }
  }

return TRUE;
}



/*************************************************
*         Complete a callout item                *
*************************************************/

/* A callout item contains the length of the next item in the pattern, which
we can't fill in till after we have reached the relevant point. This is used
for both automatic and manual callouts.

Arguments:
  previous_callout   points to previous callout item
  ptr                current pattern pointer
  cd                 pointers to tables etc

Returns:             nothing
*/

static void
complete_callout(uschar *previous_callout, const pcre_uchar *ptr, compile_data *cd)
{
int length = ptr - cd->start_pattern - GET(previous_callout, 2);
PUT(previous_callout, 2 + LINK_SIZE, length);
}



/*************************************************
*           Get othercase range                  *
*************************************************/

/* This function is passed the start and end of a class range, in UTF-8 mode
with UCP support. It searches up the characters, looking for internal ranges of
characters in the "other" case. Each call returns the next one, updating the
start address.

Arguments:
  cptr        points to starting character value; updated
  d           end value
  ocptr       where to put start of othercase range
  odptr       where to put end of othercase range

Yield:        TRUE when range returned; FALSE when no more
*/

static BOOL
get_othercase_range(int *cptr, int d, int *ocptr, int *odptr)
{
int c, othercase = 0, next;

for (c = *cptr; c <= d; c++)
  { if ((othercase = _pcre_ucp_othercase(c)) >= 0) break; }

if (c > d) return FALSE;

*ocptr = othercase;
next = othercase + 1;

for (++c; c <= d; c++)
  {
  if (_pcre_ucp_othercase(c) != next) break;
  next++;
  }

*odptr = next - 1;
*cptr = c;

return TRUE;
}


/*************************************************
*           Compile one branch                   *
*************************************************/

/* Scan the pattern, compiling it into the code vector. If the options are
changed during the branch, the pointer is used to change the external options
bits.

Arguments:
  optionsptr     pointer to the option bits
  brackets       points to number of extracting brackets used
  codeptr        points to the pointer to the current code point
  ptrptr         points to the current pattern pointer
  errorcodeptr   points to error code variable
  firstbyteptr   set to initial literal character, or < 0 (REQ_UNSET, REQ_NONE)
  reqbyteptr     set to the last literal character required, else < 0
  bcptr          points to current branch chain
  cd             contains pointers to tables etc.

Returns:         TRUE on success
                 FALSE, with *errorcodeptr set non-zero on error
*/

static BOOL
compile_branch(int *optionsptr, int *brackets, uschar **codeptr,
  const pcre_uchar **ptrptr, const pcre_uchar *patternEnd, int *errorcodeptr, int *firstbyteptr,
  int *reqbyteptr, branch_chain *bcptr, compile_data *cd)
{
int repeat_type, op_type;
int repeat_min = 0, repeat_max = 0;      /* To please picky compilers */
int bravalue = 0;
int firstbyte, reqbyte;
int zeroreqbyte, zerofirstbyte;
int req_caseopt, reqvary, tempreqvary;
int options = *optionsptr;
int after_manual_callout = 0;
register int c;
register uschar *code = *codeptr;
uschar *tempcode;
BOOL groupsetfirstbyte = FALSE;
const pcre_uchar *ptr = *ptrptr;
const pcre_uchar *tempptr;
uschar *previous = NULL;
uschar *previous_callout = NULL;
uschar classbits[32];

BOOL class_utf8;
uschar *class_utf8data;
uschar utf8_char[6];

/* Initialize no first byte, no required byte. REQ_UNSET means "no char
matching encountered yet". It gets changed to REQ_NONE if we hit something that
matches a non-fixed char first char; reqbyte just remains unset if we never
find one.

When we hit a repeat whose minimum is zero, we may have to adjust these values
to take the zero repeat into account. This is implemented by setting them to
zerofirstbyte and zeroreqbyte when such a repeat is encountered. The individual
item types that can be repeated set these backoff variables appropriately. */

firstbyte = reqbyte = zerofirstbyte = zeroreqbyte = REQ_UNSET;

/* The variable req_caseopt contains either the REQ_CASELESS value or zero,
according to the current setting of the caseless flag. REQ_CASELESS is a bit
value > 255. It is added into the firstbyte or reqbyte variables to record the
case status of the value. This is used only for ASCII characters. */

req_caseopt = ((options & PCRE_CASELESS) != 0)? REQ_CASELESS : 0;

/* Switch on next character until the end of the branch */

for (;; ptr++)
  {
  BOOL negate_class;
  BOOL should_flip_negation; /* If a negative special such as \S is used, we should negate the whole class to properly support Unicode. */
  BOOL possessive_quantifier;
  BOOL is_quantifier;
  int class_charcount;
  int class_lastchar;
  int newoptions;
  int skipbytes;
  int subreqbyte;
  int subfirstbyte;
  int mclength;
  uschar mcbuffer[8];

  /* Next byte in the pattern */

  c = ptr < patternEnd ? *ptr : 0;

  /* Fill in length of a previous callout, except when the next thing is
  a quantifier. */

  is_quantifier = c == '*' || c == '+' || c == '?' ||
    (c == '{' && is_counted_repeat(ptr+1, patternEnd));

  if (!is_quantifier && previous_callout != NULL &&
       after_manual_callout-- <= 0)
    {
    complete_callout(previous_callout, ptr, cd);
    previous_callout = NULL;
    }

  switch(c)
    {
    /* The branch terminates at end of string, |, or ). */

    case 0:
        if (ptr < patternEnd)
            goto NORMAL_CHAR;
        // End of string; fall through
    case '|':
    case ')':
    *firstbyteptr = firstbyte;
    *reqbyteptr = reqbyte;
    *codeptr = code;
    *ptrptr = ptr;
    return TRUE;

    /* Handle single-character metacharacters. In multiline mode, ^ disables
    the setting of any following char as a first character. */

    case '^':
    if ((options & PCRE_MULTILINE) != 0)
      {
      if (firstbyte == REQ_UNSET) firstbyte = REQ_NONE;
      }
    previous = NULL;
    *code++ = OP_CIRC;
    break;

    case '$':
    previous = NULL;
    *code++ = OP_DOLL;
    break;

    /* There can never be a first char if '.' is first, whatever happens about
    repeats. The value of reqbyte doesn't change either. */

    case '.':
    if (firstbyte == REQ_UNSET) firstbyte = REQ_NONE;
    zerofirstbyte = firstbyte;
    zeroreqbyte = reqbyte;
    previous = code;
    *code++ = OP_ANY;
    break;

    /* Character classes. If the included characters are all < 256, we build a
    32-byte bitmap of the permitted characters, except in the special case
    where there is only one such character. For negated classes, we build the
    map as usual, then invert it at the end. However, we use a different opcode
    so that data characters > 255 can be handled correctly.

    If the class contains characters outside the 0-255 range, a different
    opcode is compiled. It may optionally have a bit map for characters < 256,
    but those above are are explicitly listed afterwards. A flag byte tells
    whether the bitmap is present, and whether this is a negated class or not.
    */

    case '[':
    previous = code;
    should_flip_negation = FALSE;

    /* PCRE supports POSIX class stuff inside a class. Perl gives an error if
    they are encountered at the top level, so we'll do that too. */

    /* If the first character is '^', set the negation flag and skip it. */

    if ((c = *(++ptr)) == '^')
      {
      negate_class = TRUE;
      c = *(++ptr);
      }
    else
      {
      negate_class = FALSE;
      }

    /* Keep a count of chars with values < 256 so that we can optimize the case
    of just a single character (as long as it's < 256). For higher valued UTF-8
    characters, we don't yet do any optimization. */

    class_charcount = 0;
    class_lastchar = -1;

    class_utf8 = FALSE;                       /* No chars >= 256 */
    class_utf8data = code + LINK_SIZE + 34;   /* For UTF-8 items */

    /* Initialize the 32-char bit map to all zeros. We have to build the
    map in a temporary bit of store, in case the class contains only 1
    character (< 256), because in that case the compiled code doesn't use the
    bit map. */

    memset(classbits, 0, 32 * sizeof(uschar));

    /* Process characters until ] is reached. By writing this as a "do" it
    means that an initial ] is taken as a data character. The first pass
    through the regex checked the overall syntax, so we don't need to be very
    strict here. At the start of the loop, c contains the first byte of the
    character. */

    do
      {
      if (c > 127)
        {                           /* Braces are required because the */
        GETCHARLEN(c, ptr, ptr);    /* macro generates multiple statements */
        }

      /* Backslash may introduce a single character, or it may introduce one
      of the specials, which just set a flag. Escaped items are checked for
      validity in the pre-compiling pass. The sequence \b is a special case.
      Inside a class (and only there) it is treated as backspace. Elsewhere
      it marks a word boundary. Other escapes have preset maps ready to
      or into the one we are building. We assume they have more than one
      character in them, so set class_charcount bigger than one. */

      if (c == '\\')
        {
        c = check_escape(&ptr, patternEnd, errorcodeptr, *brackets, TRUE);

        if (-c == ESC_b) c = '\b';       /* \b is backslash in a class */
        else if (-c == ESC_X) c = 'X';   /* \X is literal X in a class */

        if (c < 0)
          {
          register const uschar *cbits = cd->cbits;
          class_charcount += 2;     /* Greater than 1 is what matters */
          switch (-c)
            {
            case ESC_d:
            for (c = 0; c < 32; c++) classbits[c] |= cbits[c+cbit_digit];
            continue;

            case ESC_D:
            should_flip_negation = TRUE;
            for (c = 0; c < 32; c++) classbits[c] |= ~cbits[c+cbit_digit];
            continue;

            case ESC_w:
            for (c = 0; c < 32; c++) classbits[c] |= cbits[c+cbit_word];
            continue;

            case ESC_W:
            should_flip_negation = TRUE;
            for (c = 0; c < 32; c++) classbits[c] |= ~cbits[c+cbit_word];
            continue;

            case ESC_s:
            for (c = 0; c < 32; c++) classbits[c] |= cbits[c+cbit_space];
            continue;

            case ESC_S:
            should_flip_negation = TRUE;
            for (c = 0; c < 32; c++) classbits[c] |= ~cbits[c+cbit_space];
            continue;

            /* Unrecognized escapes are faulted if PCRE is running in its
            strict mode. By default, for compatibility with Perl, they are
            treated as literals. */

            default:
            c = *ptr;              /* The final character */
            class_charcount -= 2;  /* Undo the default count from above */
            }
          }

        /* Fall through if we have a single character (c >= 0). This may be
        > 256 in UTF-8 mode. */

        }   /* End of backslash handling */

      /* A single character may be followed by '-' to form a range. However,
      Perl does not permit ']' to be the end of the range. A '-' character
      here is treated as a literal. */

      if (ptr[1] == '-' && ptr[2] != ']')
        {
        int d;
        ptr += 2;

        GETCHARLEN(d, ptr, ptr);

        /* The second part of a range can be a single-character escape, but
        not any of the other escapes. Perl 5.6 treats a hyphen as a literal
        in such circumstances. */

        if (d == '\\')
          {
          const pcre_uchar *oldptr = ptr;
          d = check_escape(&ptr, patternEnd, errorcodeptr, *brackets, TRUE);

          /* \b is backslash; \X is literal X; any other special means the '-'
          was literal */

          if (d < 0)
            {
            if (d == -ESC_b) d = '\b';
            else if (d == -ESC_X) d = 'X'; else
              {
              ptr = oldptr - 2;
              goto LONE_SINGLE_CHARACTER;  /* A few lines below */
              }
            }
          }

        /* The check that the two values are in the correct order happens in
        the pre-pass. Optimize one-character ranges */

        if (d == c) goto LONE_SINGLE_CHARACTER;  /* A few lines below */

        /* In UTF-8 mode, if the upper limit is > 255, or > 127 for caseless
        matching, we have to use an XCLASS with extra data items. Caseless
        matching for characters > 127 is available only if UCP support is
        available. */

        if ((d > 255 || ((options & PCRE_CASELESS) != 0 && d > 127)))
          {
          class_utf8 = TRUE;

          /* With UCP support, we can find the other case equivalents of
          the relevant characters. There may be several ranges. Optimize how
          they fit with the basic range. */

          if ((options & PCRE_CASELESS) != 0)
            {
            int occ, ocd;
            int cc = c;
            int origd = d;
            while (get_othercase_range(&cc, origd, &occ, &ocd))
              {
              if (occ >= c && ocd <= d) continue;  /* Skip embedded ranges */

              if (occ < c  && ocd >= c - 1)        /* Extend the basic range */
                {                                  /* if there is overlap,   */
                c = occ;                           /* noting that if occ < c */
                continue;                          /* we can't have ocd > d  */
                }                                  /* because a subrange is  */
              if (ocd > d && occ <= d + 1)         /* always shorter than    */
                {                                  /* the basic range.       */
                d = ocd;
                continue;
                }

              if (occ == ocd)
                {
                *class_utf8data++ = XCL_SINGLE;
                }
              else
                {
                *class_utf8data++ = XCL_RANGE;
                class_utf8data += _pcre_ord2utf8(occ, class_utf8data);
                }
              class_utf8data += _pcre_ord2utf8(ocd, class_utf8data);
              }
            }

          /* Now record the original range, possibly modified for UCP caseless
          overlapping ranges. */

          *class_utf8data++ = XCL_RANGE;
          class_utf8data += _pcre_ord2utf8(c, class_utf8data);
          class_utf8data += _pcre_ord2utf8(d, class_utf8data);

          /* With UCP support, we are done. Without UCP support, there is no
          caseless matching for UTF-8 characters > 127; we can use the bit map
          for the smaller ones. */

          continue;    /* With next character in the class */
          }

        /* We use the bit map for all cases when not in UTF-8 mode; else
        ranges that lie entirely within 0-127 when there is UCP support; else
        for partial ranges without UCP support. */

        for (; c <= d; c++)
          {
          classbits[c/8] |= (1 << (c&7));
          if ((options & PCRE_CASELESS) != 0)
            {
            int uc = cd->fcc[c];           /* flip case */
            classbits[uc/8] |= (1 << (uc&7));
            }
          class_charcount++;                /* in case a one-char range */
          class_lastchar = c;
          }

        continue;   /* Go get the next char in the class */
        }

      /* Handle a lone single character - we can get here for a normal
      non-escape char, or after \ that introduces a single character or for an
      apparent range that isn't. */

      LONE_SINGLE_CHARACTER:

      /* Handle a character that cannot go in the bit map */

      if ((c > 255 || ((options & PCRE_CASELESS) != 0 && c > 127)))
        {
        class_utf8 = TRUE;
        *class_utf8data++ = XCL_SINGLE;
        class_utf8data += _pcre_ord2utf8(c, class_utf8data);

        if ((options & PCRE_CASELESS) != 0)
          {
          int othercase;
          if ((othercase = _pcre_ucp_othercase(c)) >= 0)
            {
            *class_utf8data++ = XCL_SINGLE;
            class_utf8data += _pcre_ord2utf8(othercase, class_utf8data);
            }
          }

        }
      else

      /* Handle a single-byte character */
        {
        classbits[c/8] |= (1 << (c&7));
        if ((options & PCRE_CASELESS) != 0)
          {
          c = cd->fcc[c];   /* flip case */
          classbits[c/8] |= (1 << (c&7));
          }
        class_charcount++;
        class_lastchar = c;
        }
      }

    /* Loop until ']' reached; the check for end of string happens inside the
    loop. This "while" is the end of the "do" above. */

    while ((c = *(++ptr)) != ']');

    /* If class_charcount is 1, we saw precisely one character whose value is
    less than 256. In non-UTF-8 mode we can always optimize. In UTF-8 mode, we
    can optimize the negative case only if there were no characters >= 128
    because OP_NOT and the related opcodes like OP_NOTSTAR operate on
    single-bytes only. This is an historical hangover. Maybe one day we can
    tidy these opcodes to handle multi-byte characters.

    The optimization throws away the bit map. We turn the item into a
    1-character OP_CHAR[NC] if it's positive, or OP_NOT if it's negative. Note
    that OP_NOT does not support multibyte characters. In the positive case, it
    can cause firstbyte to be set. Otherwise, there can be no first char if
    this item is first, whatever repeat count may follow. In the case of
    reqbyte, save the previous value for reinstating. */

    if (class_charcount == 1 &&
          (!class_utf8 && (!negate_class || class_lastchar < 128)))
      {
      zeroreqbyte = reqbyte;

      /* The OP_NOT opcode works on one-byte characters only. */

      if (negate_class)
        {
        if (firstbyte == REQ_UNSET) firstbyte = REQ_NONE;
        zerofirstbyte = firstbyte;
        *code++ = OP_NOT;
        *code++ = class_lastchar;
        break;
        }

      /* For a single, positive character, get the value into mcbuffer, and
      then we can handle this with the normal one-character code. */

      if (class_lastchar > 127)
        mclength = _pcre_ord2utf8(class_lastchar, mcbuffer);
      else
        {
        mcbuffer[0] = class_lastchar;
        mclength = 1;
        }
      goto ONE_CHAR;
      }       /* End of 1-char optimization */

    /* The general case - not the one-char optimization. If this is the first
    thing in the branch, there can be no first char setting, whatever the
    repeat count. Any reqbyte setting must remain unchanged after any kind of
    repeat. */

    if (firstbyte == REQ_UNSET) firstbyte = REQ_NONE;
    zerofirstbyte = firstbyte;
    zeroreqbyte = reqbyte;

    /* If there are characters with values > 255, we have to compile an
    extended class, with its own opcode. If there are no characters < 256,
    we can omit the bitmap. */

    if (class_utf8 && !should_flip_negation)
      {
      *class_utf8data++ = XCL_END;    /* Marks the end of extra data */
      *code++ = OP_XCLASS;
      code += LINK_SIZE;
      *code = negate_class? XCL_NOT : 0;

      /* If the map is required, install it, and move on to the end of
      the extra data */

      if (class_charcount > 0)
        {
        *code++ |= XCL_MAP;
        memcpy(code, classbits, 32);
        code = class_utf8data;
        }

      /* If the map is not required, slide down the extra data. */

      else
        {
        int len = class_utf8data - (code + 33);
        memmove(code + 1, code + 33, len);
        code += len + 1;
        }

      /* Now fill in the complete length of the item */

      PUT(previous, 1, code - previous);
      break;   /* End of class handling */
      }

    /* If there are no characters > 255, negate the 32-byte map if necessary,
    and copy it into the code vector. If this is the first thing in the branch,
    there can be no first char setting, whatever the repeat count. Any reqbyte
    setting must remain unchanged after any kind of repeat. */

    *code++ = (negate_class == should_flip_negation) ? OP_CLASS : OP_NCLASS;
    if (negate_class)
      {
      for (c = 0; c < 32; c++) code[c] = ~classbits[c];
      }
    else
      {
      memcpy(code, classbits, 32);
      }
    code += 32;
    break;

    /* Various kinds of repeat; '{' is not necessarily a quantifier, but this
    has been tested above. */

    case '{':
    if (!is_quantifier) goto NORMAL_CHAR;
    ptr = read_repeat_counts(ptr+1, &repeat_min, &repeat_max, errorcodeptr);
    if (*errorcodeptr != 0) goto FAILED;
    goto REPEAT;

    case '*':
    repeat_min = 0;
    repeat_max = -1;
    goto REPEAT;

    case '+':
    repeat_min = 1;
    repeat_max = -1;
    goto REPEAT;

    case '?':
    repeat_min = 0;
    repeat_max = 1;

    REPEAT:
    if (previous == NULL)
      {
      *errorcodeptr = ERR9;
      goto FAILED;
      }

    if (repeat_min == 0)
      {
      firstbyte = zerofirstbyte;    /* Adjust for zero repeat */
      reqbyte = zeroreqbyte;        /* Ditto */
      }

    /* Remember whether this is a variable length repeat */

    reqvary = (repeat_min == repeat_max)? 0 : REQ_VARY;

    op_type = 0;                    /* Default single-char op codes */
    possessive_quantifier = FALSE;  /* Default not possessive quantifier */

    /* Save start of previous item, in case we have to move it up to make space
    for an inserted OP_ONCE for the additional '+' extension. */

    tempcode = previous;

    /* If the next character is '+', we have a possessive quantifier. This
    implies greediness, whatever the setting of the PCRE_UNGREEDY option.
    If the next character is '?' this is a minimizing repeat, by default,
    but if PCRE_UNGREEDY is set, it works the other way round. We change the
    repeat type to the non-default. */

    if (ptr + 1 < patternEnd && ptr[1] == '+')
      {
      repeat_type = 0;                  /* Force greedy */
      possessive_quantifier = TRUE;
      ptr++;
      }
    else if (ptr + 1 < patternEnd && ptr[1] == '?')
      {
      repeat_type = 1;
      ptr++;
      }
    else repeat_type = 0;

    /* If previous was a character match, abolish the item and generate a
    repeat item instead. If a char item has a minumum of more than one, ensure
    that it is set in reqbyte - it might not be if a sequence such as x{3} is
    the first thing in a branch because the x will have gone into firstbyte
    instead.  */

    if (*previous == OP_CHAR || *previous == OP_CHARNC)
      {
      /* Deal with UTF-8 characters that take up more than one byte. It's
      easier to write this out separately than try to macrify it. Use c to
      hold the length of the character in bytes, plus 0x80 to flag that it's a
      length rather than a small character. */

      if ((code[-1] & 0x80) != 0)
        {
        uschar *lastchar = code - 1;
        while((*lastchar & 0xc0) == 0x80) lastchar--;
        c = code - lastchar;            /* Length of UTF-8 character */
        memcpy(utf8_char, lastchar, c); /* Save the char */
        c |= 0x80;                      /* Flag c as a length */
        }
      else

      /* Handle the case of a single byte - either with no UTF8 support, or
      with UTF-8 disabled, or for a UTF-8 character < 128. */

        {
        c = code[-1];
        if (repeat_min > 1) reqbyte = c | req_caseopt | cd->req_varyopt;
        }

      goto OUTPUT_SINGLE_REPEAT;   /* Code shared with single character types */
      }

    /* If previous was a single negated character ([^a] or similar), we use
    one of the special opcodes, replacing it. The code is shared with single-
    character repeats by setting opt_type to add a suitable offset into
    repeat_type. OP_NOT is currently used only for single-byte chars. */

    else if (*previous == OP_NOT)
      {
      op_type = OP_NOTSTAR - OP_STAR;  /* Use "not" opcodes */
      c = previous[1];
      goto OUTPUT_SINGLE_REPEAT;
      }

    /* If previous was a character type match (\d or similar), abolish it and
    create a suitable repeat item. The code is shared with single-character
    repeats by setting op_type to add a suitable offset into repeat_type. */

    else if (*previous <= OP_ANY)
      {
      uschar *oldcode;
      int prop_type, prop_value;
      op_type = OP_TYPESTAR - OP_STAR;  /* Use type opcodes */
      c = *previous;

      OUTPUT_SINGLE_REPEAT:
      prop_type = prop_value = -1;

      oldcode = code;
      code = previous;                  /* Usually overwrite previous item */

      /* If the maximum is zero then the minimum must also be zero; Perl allows
      this case, so we do too - by simply omitting the item altogether. */

      if (repeat_max == 0) goto END_REPEAT;

      /* Combine the op_type with the repeat_type */

      repeat_type += op_type;

      /* A minimum of zero is handled either as the special case * or ?, or as
      an UPTO, with the maximum given. */

      if (repeat_min == 0)
        {
        if (repeat_max == -1) *code++ = OP_STAR + repeat_type;
          else if (repeat_max == 1) *code++ = OP_QUERY + repeat_type;
        else
          {
          *code++ = OP_UPTO + repeat_type;
          PUT2INC(code, 0, repeat_max);
          }
        }

      /* A repeat minimum of 1 is optimized into some special cases. If the
      maximum is unlimited, we use OP_PLUS. Otherwise, the original item it
      left in place and, if the maximum is greater than 1, we use OP_UPTO with
      one less than the maximum. */

      else if (repeat_min == 1)
        {
        if (repeat_max == -1)
          *code++ = OP_PLUS + repeat_type;
        else
          {
          code = oldcode;                 /* leave previous item in place */
          if (repeat_max == 1) goto END_REPEAT;
          *code++ = OP_UPTO + repeat_type;
          PUT2INC(code, 0, repeat_max - 1);
          }
        }

      /* The case {n,n} is just an EXACT, while the general case {n,m} is
      handled as an EXACT followed by an UPTO. */

      else
        {
        *code++ = OP_EXACT + op_type;  /* NB EXACT doesn't have repeat_type */
        PUT2INC(code, 0, repeat_min);

        /* If the maximum is unlimited, insert an OP_STAR. Before doing so,
        we have to insert the character for the previous code. For a repeated
        Unicode property match, there are two extra bytes that define the
        required property. In UTF-8 mode, long characters have their length in
        c, with the 0x80 bit as a flag. */

        if (repeat_max < 0)
          {
          if (c >= 128)
            {
            memcpy(code, utf8_char, c & 7);
            code += c & 7;
            }
          else
            {
            *code++ = c;
            if (prop_type >= 0)
              {
              *code++ = prop_type;
              *code++ = prop_value;
              }
            }
          *code++ = OP_STAR + repeat_type;
          }

        /* Else insert an UPTO if the max is greater than the min, again
        preceded by the character, for the previously inserted code. */

        else if (repeat_max != repeat_min)
          {
          if (c >= 128)
            {
            memcpy(code, utf8_char, c & 7);
            code += c & 7;
            }
          else
          *code++ = c;
          if (prop_type >= 0)
            {
            *code++ = prop_type;
            *code++ = prop_value;
            }
          repeat_max -= repeat_min;
          *code++ = OP_UPTO + repeat_type;
          PUT2INC(code, 0, repeat_max);
          }
        }

      /* The character or character type itself comes last in all cases. */

      if (c >= 128)
        {
        memcpy(code, utf8_char, c & 7);
        code += c & 7;
        }
      else
      *code++ = c;

      /* For a repeated Unicode property match, there are two extra bytes that
      define the required property. */

      if (prop_type >= 0)
        {
        *code++ = prop_type;
        *code++ = prop_value;
        }
      }

    /* If previous was a character class or a back reference, we put the repeat
    stuff after it, but just skip the item if the repeat was {0,0}. */

    else if (*previous == OP_CLASS ||
             *previous == OP_NCLASS ||
             *previous == OP_XCLASS ||
             *previous == OP_REF)
      {
      if (repeat_max == 0)
        {
        code = previous;
        goto END_REPEAT;
        }

      if (repeat_min == 0 && repeat_max == -1)
        *code++ = OP_CRSTAR + repeat_type;
      else if (repeat_min == 1 && repeat_max == -1)
        *code++ = OP_CRPLUS + repeat_type;
      else if (repeat_min == 0 && repeat_max == 1)
        *code++ = OP_CRQUERY + repeat_type;
      else
        {
        *code++ = OP_CRRANGE + repeat_type;
        PUT2INC(code, 0, repeat_min);
        if (repeat_max == -1) repeat_max = 0;  /* 2-byte encoding for max */
        PUT2INC(code, 0, repeat_max);
        }
      }

    /* If previous was a bracket group, we may have to replicate it in certain
    cases. */

    else if (*previous >= OP_BRA || *previous == OP_ONCE)
      {
      register int i;
      int ketoffset = 0;
      int len = code - previous;
      uschar *bralink = NULL;

      /* If the maximum repeat count is unlimited, find the end of the bracket
      by scanning through from the start, and compute the offset back to it
      from the current code pointer. There may be an OP_OPT setting following
      the final KET, so we can't find the end just by going back from the code
      pointer. */

      if (repeat_max == -1)
        {
        register uschar *ket = previous;
        do ket += GET(ket, 1); while (*ket != OP_KET);
        ketoffset = code - ket;
        }

      /* The case of a zero minimum is special because of the need to stick
      OP_BRAZERO in front of it, and because the group appears once in the
      data, whereas in other cases it appears the minimum number of times. For
      this reason, it is simplest to treat this case separately, as otherwise
      the code gets far too messy. There are several special subcases when the
      minimum is zero. */

      if (repeat_min == 0)
        {
        /* If the maximum is also zero, we just omit the group from the output
        altogether. */

        if (repeat_max == 0)
          {
          code = previous;
          goto END_REPEAT;
          }

        /* If the maximum is 1 or unlimited, we just have to stick in the
        BRAZERO and do no more at this point. However, we do need to adjust
        any OP_RECURSE calls inside the group that refer to the group itself or
        any internal group, because the offset is from the start of the whole
        regex. Temporarily terminate the pattern while doing this. */

        if (repeat_max <= 1)
          {
          *code = OP_END;
          memmove(previous+1, previous, len);
          code++;
          *previous++ = OP_BRAZERO + repeat_type;
          }

        /* If the maximum is greater than 1 and limited, we have to replicate
        in a nested fashion, sticking OP_BRAZERO before each set of brackets.
        The first one has to be handled carefully because it's the original
        copy, which has to be moved up. The remainder can be handled by code
        that is common with the non-zero minimum case below. We have to
        adjust the value or repeat_max, since one less copy is required. Once
        again, we may have to adjust any OP_RECURSE calls inside the group. */

        else
          {
          int offset;
          *code = OP_END;
          memmove(previous + 2 + LINK_SIZE, previous, len);
          code += 2 + LINK_SIZE;
          *previous++ = OP_BRAZERO + repeat_type;
          *previous++ = OP_BRA;

          /* We chain together the bracket offset fields that have to be
          filled in later when the ends of the brackets are reached. */

          offset = (bralink == NULL)? 0 : previous - bralink;
          bralink = previous;
          PUTINC(previous, 0, offset);
          }

        repeat_max--;
        }

      /* If the minimum is greater than zero, replicate the group as many
      times as necessary, and adjust the maximum to the number of subsequent
      copies that we need. If we set a first char from the group, and didn't
      set a required char, copy the latter from the former. */

      else
        {
        if (repeat_min > 1)
          {
          if (groupsetfirstbyte && reqbyte < 0) reqbyte = firstbyte;
          for (i = 1; i < repeat_min; i++)
            {
            memcpy(code, previous, len);
            code += len;
            }
          }
        if (repeat_max > 0) repeat_max -= repeat_min;
        }

      /* This code is common to both the zero and non-zero minimum cases. If
      the maximum is limited, it replicates the group in a nested fashion,
      remembering the bracket starts on a stack. In the case of a zero minimum,
      the first one was set up above. In all cases the repeat_max now specifies
      the number of additional copies needed. */

      if (repeat_max >= 0)
        {
        for (i = repeat_max - 1; i >= 0; i--)
          {
          *code++ = OP_BRAZERO + repeat_type;

          /* All but the final copy start a new nesting, maintaining the
          chain of brackets outstanding. */

          if (i != 0)
            {
            int offset;
            *code++ = OP_BRA;
            offset = (bralink == NULL)? 0 : code - bralink;
            bralink = code;
            PUTINC(code, 0, offset);
            }

          memcpy(code, previous, len);
          code += len;
          }

        /* Now chain through the pending brackets, and fill in their length
        fields (which are holding the chain links pro tem). */

        while (bralink != NULL)
          {
          int oldlinkoffset;
          int offset = code - bralink + 1;
          uschar *bra = code - offset;
          oldlinkoffset = GET(bra, 1);
          bralink = (oldlinkoffset == 0)? NULL : bralink - oldlinkoffset;
          *code++ = OP_KET;
          PUTINC(code, 0, offset);
          PUT(bra, 1, offset);
          }
        }

      /* If the maximum is unlimited, set a repeater in the final copy. We
      can't just offset backwards from the current code point, because we
      don't know if there's been an options resetting after the ket. The
      correct offset was computed above. */

      else code[-ketoffset] = OP_KETRMAX + repeat_type;
      }

    /* Else there's some kind of shambles */

    else
      {
      *errorcodeptr = ERR11;
      goto FAILED;
      }

    /* If the character following a repeat is '+', we wrap the entire repeated
    item inside OP_ONCE brackets. This is just syntactic sugar, taken from
    Sun's Java package. The repeated item starts at tempcode, not at previous,
    which might be the first part of a string whose (former) last char we
    repeated. However, we don't support '+' after a greediness '?'. */

    if (possessive_quantifier)
      {
      int len = code - tempcode;
      memmove(tempcode + 1+LINK_SIZE, tempcode, len);
      code += 1 + LINK_SIZE;
      len += 1 + LINK_SIZE;
      tempcode[0] = OP_ONCE;
      *code++ = OP_KET;
      PUTINC(code, 0, len);
      PUT(tempcode, 1, len);
      }

    /* In all case we no longer have a previous item. We also set the
    "follows varying string" flag for subsequently encountered reqbytes if
    it isn't already set and we have just passed a varying length item. */

    END_REPEAT:
    previous = NULL;
    cd->req_varyopt |= reqvary;
    break;


    /* Start of nested bracket sub-expression, or comment or lookahead or
    lookbehind or option setting or condition. First deal with special things
    that can come after a bracket; all are introduced by ?, and the appearance
    of any of them means that this is not a referencing group. They were
    checked for validity in the first pass over the string, so we don't have to
    check for syntax errors here.  */

    case '(':
    newoptions = options;
    skipbytes = 0;

    if (*(++ptr) == '?')
      {
      switch (*(++ptr))
        {
        case ':':                 /* Non-extracting bracket */
        bravalue = OP_BRA;
        ptr++;
        break;

        case '=':                 /* Positive lookahead */
        bravalue = OP_ASSERT;
        ptr++;
        break;

        case '!':                 /* Negative lookahead */
        bravalue = OP_ASSERT_NOT;
        ptr++;
        break;

        /* Character after (? not specially recognized */

        default:                  /* Option setting */
        *errorcodeptr = ERR12;
        goto FAILED;
        }
      }

    /* Else we have a referencing group; adjust the opcode. If the bracket
    number is greater than EXTRACT_BASIC_MAX, we set the opcode one higher, and
    arrange for the true number to follow later, in an OP_BRANUMBER item. */

    else
      {
      if (++(*brackets) > EXTRACT_BASIC_MAX)
        {
        bravalue = OP_BRA + EXTRACT_BASIC_MAX + 1;
        code[1+LINK_SIZE] = OP_BRANUMBER;
        PUT2(code, 2+LINK_SIZE, *brackets);
        skipbytes = 3;
        }
      else bravalue = OP_BRA + *brackets;
      }

    /* Process nested bracketed re. Assertions may not be repeated, but other
    kinds can be. We copy code into a non-register variable in order to be able
    to pass its address because some compilers complain otherwise. Pass in a
    new setting for the ims options if they have changed. */

    previous = (bravalue >= OP_ONCE)? code : NULL;
    *code = bravalue;
    tempcode = code;
    tempreqvary = cd->req_varyopt;     /* Save value before bracket */

    if (!compile_regex(
         newoptions,                   /* The complete new option state */
         brackets,                     /* Extracting bracket count */
         &tempcode,                    /* Where to put code (updated) */
         &ptr,                         /* Input pointer (updated) */
         patternEnd,
         errorcodeptr,                 /* Where to put an error message */
         skipbytes,                    /* Skip over OP_COND/OP_BRANUMBER */
         &subfirstbyte,                /* For possible first char */
         &subreqbyte,                  /* For possible last char */
         bcptr,                        /* Current branch chain */
         cd))                          /* Tables block */
      goto FAILED;

    /* At the end of compiling, code is still pointing to the start of the
    group, while tempcode has been updated to point past the end of the group
    and any option resetting that may follow it. The pattern pointer (ptr)
    is on the bracket. */

    /* Handle updating of the required and first characters. Update for normal
    brackets of all kinds, and conditions with two branches (see code above).
    If the bracket is followed by a quantifier with zero repeat, we have to
    back off. Hence the definition of zeroreqbyte and zerofirstbyte outside the
    main loop so that they can be accessed for the back off. */

    zeroreqbyte = reqbyte;
    zerofirstbyte = firstbyte;
    groupsetfirstbyte = FALSE;

    if (bravalue >= OP_BRA || bravalue == OP_ONCE)
      {
      /* If we have not yet set a firstbyte in this branch, take it from the
      subpattern, remembering that it was set here so that a repeat of more
      than one can replicate it as reqbyte if necessary. If the subpattern has
      no firstbyte, set "none" for the whole branch. In both cases, a zero
      repeat forces firstbyte to "none". */

      if (firstbyte == REQ_UNSET)
        {
        if (subfirstbyte >= 0)
          {
          firstbyte = subfirstbyte;
          groupsetfirstbyte = TRUE;
          }
        else firstbyte = REQ_NONE;
        zerofirstbyte = REQ_NONE;
        }

      /* If firstbyte was previously set, convert the subpattern's firstbyte
      into reqbyte if there wasn't one, using the vary flag that was in
      existence beforehand. */

      else if (subfirstbyte >= 0 && subreqbyte < 0)
        subreqbyte = subfirstbyte | tempreqvary;

      /* If the subpattern set a required byte (or set a first byte that isn't
      really the first byte - see above), set it. */

      if (subreqbyte >= 0) reqbyte = subreqbyte;
      }

    /* For a forward assertion, we take the reqbyte, if set. This can be
    helpful if the pattern that follows the assertion doesn't set a different
    char. For example, it's useful for /(?=abcde).+/. We can't set firstbyte
    for an assertion, however because it leads to incorrect effect for patterns
    such as /(?=a)a.+/ when the "real" "a" would then become a reqbyte instead
    of a firstbyte. This is overcome by a scan at the end if there's no
    firstbyte, looking for an asserted first char. */

    else if (bravalue == OP_ASSERT && subreqbyte >= 0) reqbyte = subreqbyte;

    /* Now update the main code pointer to the end of the group. */

    code = tempcode;

    /* Error if hit end of pattern */

    if (ptr >= patternEnd || *ptr != ')')
      {
      *errorcodeptr = ERR14;
      goto FAILED;
      }
    break;

    /* Check \ for being a real metacharacter; if not, fall through and handle
    it as a data character at the start of a string. Escape items are checked
    for validity in the pre-compiling pass. */

    case '\\':
    tempptr = ptr;
    c = check_escape(&ptr, patternEnd, errorcodeptr, *brackets, FALSE);

    /* Handle metacharacters introduced by \. For ones like \d, the ESC_ values
    are arranged to be the negation of the corresponding OP_values. For the
    back references, the values are ESC_REF plus the reference number. Only
    back references and those types that consume a character may be repeated.
    We can test for values between ESC_b and ESC_Z for the latter; this may
    have to change if any new ones are ever created. */

    if (c < 0)
      {
      /* For metasequences that actually match a character, we disable the
      setting of a first character if it hasn't already been set. */

      if (firstbyte == REQ_UNSET && -c > ESC_b && -c < ESC_Z)
        firstbyte = REQ_NONE;

      /* Set values to reset to if this is followed by a zero repeat. */

      zerofirstbyte = firstbyte;
      zeroreqbyte = reqbyte;

      /* Back references are handled specially */

      if (-c >= ESC_REF)
        {
        int number = -c - ESC_REF;
        previous = code;
        *code++ = OP_REF;
        PUT2INC(code, 0, number);
        }

      /* For the rest, we can obtain the OP value by negating the escape
      value */

      else
        {
        previous = (-c > ESC_b && -c < ESC_Z)? code : NULL;
        *code++ = -c;
        }
      continue;
      }

    /* We have a data character whose value is in c. In UTF-8 mode it may have
    a value > 127. We set its representation in the length/buffer, and then
    handle it as a data character. */

    if (c > 127)
      mclength = _pcre_ord2utf8(c, mcbuffer);
    else
     {
     mcbuffer[0] = c;
     mclength = 1;
     }

    goto ONE_CHAR;

    /* Handle a literal character. It is guaranteed not to be whitespace or #
    when the extended flag is set. If we are in UTF-8 mode, it may be a
    multi-byte literal character. */

    default:
    NORMAL_CHAR:
    if (c < 128)
      {
       mclength = 1;
       mcbuffer[0] = c;
     }
    else
      mclength = _pcre_ord2utf8(c, mcbuffer);

    /* At this point we have the character's bytes in mcbuffer, and the length
    in mclength. When not in UTF-8 mode, the length is always 1. */

    ONE_CHAR:
    previous = code;
    *code++ = ((options & PCRE_CASELESS) != 0)? OP_CHARNC : OP_CHAR;
    for (c = 0; c < mclength; c++) *code++ = mcbuffer[c];

    /* Set the first and required bytes appropriately. If no previous first
    byte, set it from this character, but revert to none on a zero repeat.
    Otherwise, leave the firstbyte value alone, and don't change it on a zero
    repeat. */

    if (firstbyte == REQ_UNSET)
      {
      zerofirstbyte = REQ_NONE;
      zeroreqbyte = reqbyte;

      /* If the character is more than one byte long, we can set firstbyte
      only if it is not to be matched caselessly. */

      if (mclength == 1 || req_caseopt == 0)
        {
        firstbyte = mcbuffer[0] | req_caseopt;
        if (mclength != 1) reqbyte = code[-1] | cd->req_varyopt;
        }
      else firstbyte = reqbyte = REQ_NONE;
      }

    /* firstbyte was previously set; we can set reqbyte only the length is
    1 or the matching is caseful. */

    else
      {
      zerofirstbyte = firstbyte;
      zeroreqbyte = reqbyte;
      if (mclength == 1 || req_caseopt == 0)
        reqbyte = code[-1] | req_caseopt | cd->req_varyopt;
      }

    break;            /* End of literal character handling */
    }
  }                   /* end of big loop */

/* Control never reaches here by falling through, only by a goto for all the
error states. Pass back the position in the pattern so that it can be displayed
to the user for diagnosing the error. */

FAILED:
*ptrptr = ptr;
return FALSE;
}




/*************************************************
*     Compile sequence of alternatives           *
*************************************************/

/* On entry, ptr is pointing past the bracket character, but on return
it points to the closing bracket, or vertical bar, or end of string.
The code variable is pointing at the byte into which the BRA operator has been
stored. If the ims options are changed at the start (for a (?ims: group) or
during any branch, we need to insert an OP_OPT item at the start of every
following branch to ensure they get set correctly at run time, and also pass
the new options into every subsequent branch compile.

Argument:
  options        option bits, including any changes for this subpattern
  brackets       -> int containing the number of extracting brackets used
  codeptr        -> the address of the current code pointer
  ptrptr         -> the address of the current pattern pointer
  errorcodeptr   -> pointer to error code variable
  skipbytes      skip this many bytes at start (for OP_COND, OP_BRANUMBER)
  firstbyteptr   place to put the first required character, or a negative number
  reqbyteptr     place to put the last required character, or a negative number
  bcptr          pointer to the chain of currently open branches
  cd             points to the data block with tables pointers etc.

Returns:      TRUE on success
*/

static BOOL
compile_regex(int options, int *brackets, uschar **codeptr,
  const pcre_uchar **ptrptr, const pcre_uchar *patternEnd, int *errorcodeptr, int skipbytes,
  int *firstbyteptr, int *reqbyteptr, branch_chain *bcptr, compile_data *cd)
{
const pcre_uchar *ptr = *ptrptr;
uschar *code = *codeptr;
uschar *last_branch = code;
uschar *start_bracket = code;
int firstbyte, reqbyte;
int branchfirstbyte, branchreqbyte;
branch_chain bc;

bc.outer = bcptr;
bc.current = code;

firstbyte = reqbyte = REQ_UNSET;

/* Offset is set zero to mark that this bracket is still open */

PUT(code, 1, 0);
code += 1 + LINK_SIZE + skipbytes;

/* Loop for each alternative branch */

for (;;)
  {
  /* Now compile the branch */

  if (!compile_branch(&options, brackets, &code, &ptr, patternEnd, errorcodeptr,
        &branchfirstbyte, &branchreqbyte, &bc, cd))
    {
    *ptrptr = ptr;
    return FALSE;
    }

  /* If this is the first branch, the firstbyte and reqbyte values for the
  branch become the values for the regex. */

  if (*last_branch != OP_ALT)
    {
    firstbyte = branchfirstbyte;
    reqbyte = branchreqbyte;
    }

  /* If this is not the first branch, the first char and reqbyte have to
  match the values from all the previous branches, except that if the previous
  value for reqbyte didn't have REQ_VARY set, it can still match, and we set
  REQ_VARY for the regex. */

  else
    {
    /* If we previously had a firstbyte, but it doesn't match the new branch,
    we have to abandon the firstbyte for the regex, but if there was previously
    no reqbyte, it takes on the value of the old firstbyte. */

    if (firstbyte >= 0 && firstbyte != branchfirstbyte)
      {
      if (reqbyte < 0) reqbyte = firstbyte;
      firstbyte = REQ_NONE;
      }

    /* If we (now or from before) have no firstbyte, a firstbyte from the
    branch becomes a reqbyte if there isn't a branch reqbyte. */

    if (firstbyte < 0 && branchfirstbyte >= 0 && branchreqbyte < 0)
        branchreqbyte = branchfirstbyte;

    /* Now ensure that the reqbytes match */

    if ((reqbyte & ~REQ_VARY) != (branchreqbyte & ~REQ_VARY))
      reqbyte = REQ_NONE;
    else reqbyte |= branchreqbyte;   /* To "or" REQ_VARY */
    }

  /* Reached end of expression, either ')' or end of pattern. Go back through
  the alternative branches and reverse the chain of offsets, with the field in
  the BRA item now becoming an offset to the first alternative. If there are
  no alternatives, it points to the end of the group. The length in the
  terminating ket is always the length of the whole bracketed item. If any of
  the ims options were changed inside the group, compile a resetting op-code
  following, except at the very end of the pattern. Return leaving the pointer
  at the terminating char. */

  if (ptr >= patternEnd || *ptr != '|')
    {
    int length = code - last_branch;
    do
      {
      int prev_length = GET(last_branch, 1);
      PUT(last_branch, 1, length);
      length = prev_length;
      last_branch -= length;
      }
    while (length > 0);

    /* Fill in the ket */

    *code = OP_KET;
    PUT(code, 1, code - start_bracket);
    code += 1 + LINK_SIZE;

    /* Set values to pass back */

    *codeptr = code;
    *ptrptr = ptr;
    *firstbyteptr = firstbyte;
    *reqbyteptr = reqbyte;
    return TRUE;
    }

  /* Another branch follows; insert an "or" node. Its length field points back
  to the previous branch while the bracket remains open. At the end the chain
  is reversed. It's done like this so that the start of the bracket has a
  zero offset until it is closed, making it possible to detect recursion. */

  *code = OP_ALT;
  PUT(code, 1, code - last_branch);
  bc.current = last_branch = code;
  code += 1 + LINK_SIZE;
  ptr++;
  }
/* Control never reaches here */
}




/*************************************************
*          Check for anchored expression         *
*************************************************/

/* Try to find out if this is an anchored regular expression. Consider each
alternative branch. If they all start with OP_SOD or OP_CIRC, or with a bracket
all of whose alternatives start with OP_SOD or OP_CIRC (recurse ad lib), then
it's anchored. However, if this is a multiline pattern, then only OP_SOD
counts, since OP_CIRC can match in the middle.

We can also consider a regex to be anchored if OP_SOM starts all its branches.
This is the code for \G, which means "match at start of match position, taking
into account the match offset".

A branch is also implicitly anchored if it starts with .* and DOTALL is set,
because that will try the rest of the pattern at all possible matching points,
so there is no point trying again.... er ....

.... except when the .* appears inside capturing parentheses, and there is a
subsequent back reference to those parentheses. We haven't enough information
to catch that case precisely.

At first, the best we could do was to detect when .* was in capturing brackets
and the highest back reference was greater than or equal to that level.
However, by keeping a bitmap of the first 31 back references, we can catch some
of the more common cases more precisely.

Arguments:
  code           points to start of expression (the bracket)
  options        points to the options setting
  bracket_map    a bitmap of which brackets we are inside while testing; this
                  handles up to substring 31; after that we just have to take
                  the less precise approach
  backref_map    the back reference bitmap

Returns:     TRUE or FALSE
*/

static BOOL
is_anchored(register const uschar *code, int *options, unsigned int bracket_map,
  unsigned int backref_map)
{
do {
   const uschar *scode =
     first_significant_code(code + 1+LINK_SIZE, FALSE);
   register int op = *scode;

   /* Capturing brackets */

   if (op > OP_BRA)
     {
     int new_map;
     op -= OP_BRA;
     if (op > EXTRACT_BASIC_MAX) op = GET2(scode, 2+LINK_SIZE);
     new_map = bracket_map | ((op < 32)? (1 << op) : 1);
     if (!is_anchored(scode, options, new_map, backref_map)) return FALSE;
     }

   /* Other brackets */

   else if (op == OP_BRA || op == OP_ASSERT || op == OP_ONCE)
     {
     if (!is_anchored(scode, options, bracket_map, backref_map)) return FALSE;
     }

   /* Check for explicit anchoring */

   else if (((*options & PCRE_MULTILINE) != 0 || op != OP_CIRC))
     return FALSE;
   code += GET(code, 1);
   }
while (*code == OP_ALT);   /* Loop for each alternative */
return TRUE;
}



/*************************************************
*         Check for starting with ^ or .*        *
*************************************************/

/* This is called to find out if every branch starts with ^ or .* so that
"first char" processing can be done to speed things up in multiline
matching and for non-DOTALL patterns that start with .* (which must start at
the beginning or after \n). As in the case of is_anchored() (see above), we
have to take account of back references to capturing brackets that contain .*
because in that case we can't make the assumption.

Arguments:
  code           points to start of expression (the bracket)
  bracket_map    a bitmap of which brackets we are inside while testing; this
                  handles up to substring 31; after that we just have to take
                  the less precise approach
  backref_map    the back reference bitmap

Returns:         TRUE or FALSE
*/

static BOOL
is_startline(const uschar *code, unsigned int bracket_map,
  unsigned int backref_map)
{
do {
   const uschar *scode = first_significant_code(code + 1+LINK_SIZE, FALSE);
   register int op = *scode;

   /* Capturing brackets */

   if (op > OP_BRA)
     {
     int new_map;
     op -= OP_BRA;
     if (op > EXTRACT_BASIC_MAX) op = GET2(scode, 2+LINK_SIZE);
     new_map = bracket_map | ((op < 32)? (1 << op) : 1);
     if (!is_startline(scode, new_map, backref_map)) return FALSE;
     }

   /* Other brackets */

   else if (op == OP_BRA || op == OP_ASSERT || op == OP_ONCE)
     { if (!is_startline(scode, bracket_map, backref_map)) return FALSE; }

   /* .* means "start at start or after \n" if it isn't in brackets that
   may be referenced. */

   else if (op == OP_TYPESTAR || op == OP_TYPEMINSTAR)
     {
     if (scode[1] != OP_ANY || (bracket_map & backref_map) != 0) return FALSE;
     }

   /* Check for explicit circumflex */

   else if (op != OP_CIRC) return FALSE;

   /* Move on to the next alternative */

   code += GET(code, 1);
   }
while (*code == OP_ALT);  /* Loop for each alternative */
return TRUE;
}



/*************************************************
*       Check for asserted fixed first char      *
*************************************************/

/* During compilation, the "first char" settings from forward assertions are
discarded, because they can cause conflicts with actual literals that follow.
However, if we end up without a first char setting for an unanchored pattern,
it is worth scanning the regex to see if there is an initial asserted first
char. If all branches start with the same asserted char, or with a bracket all
of whose alternatives start with the same asserted char (recurse ad lib), then
we return that char, otherwise -1.

Arguments:
  code       points to start of expression (the bracket)
  options    pointer to the options (used to check casing changes)
  inassert   TRUE if in an assertion

Returns:     -1 or the fixed first char
*/

static int
find_firstassertedchar(const uschar *code, int *options, BOOL inassert)
{
register int c = -1;
do {
   int d;
   const uschar *scode =
     first_significant_code(code + 1+LINK_SIZE, TRUE);
   register int op = *scode;

   if (op >= OP_BRA) op = OP_BRA;

   switch(op)
     {
     default:
     return -1;

     case OP_BRA:
     case OP_ASSERT:
     case OP_ONCE:
     if ((d = find_firstassertedchar(scode, options, op == OP_ASSERT)) < 0)
       return -1;
     if (c < 0) c = d; else if (c != d) return -1;
     break;

     case OP_EXACT:       /* Fall through */
     scode += 2;

     case OP_CHAR:
     case OP_CHARNC:
     case OP_PLUS:
     case OP_MINPLUS:
     if (!inassert) return -1;
     if (c < 0)
       {
       c = scode[1];
       if ((*options & PCRE_CASELESS) != 0) c |= REQ_CASELESS;
       }
     else if (c != scode[1]) return -1;
     break;
     }

   code += GET(code, 1);
   }
while (*code == OP_ALT);
return c;
}



/*************************************************
*        Compile a Regular Expression            *
*************************************************/

/* This function takes a string and returns a pointer to a block of store
holding a compiled version of the expression. The original API for this
function had no error code return variable; it is retained for backwards
compatibility. The new function is given a new name.

Arguments:
  pattern       the regular expression
  options       various option bits
  errorcodeptr  pointer to error code variable (pcre_compile2() only)
                  can be NULL if you don't want a code value
  errorptr      pointer to pointer to error text
  erroroffset   ptr offset in pattern where error was detected
  tables        pointer to character tables or NULL

Returns:        pointer to compiled data block, or NULL on error,
                with errorptr and erroroffset set
*/

pcre *
jsRegExpCompile(const pcre_char* pattern, int patternLength, int options, unsigned* numSubpatterns, const char** errorptr)
{
real_pcre *re;
int length = 1 + LINK_SIZE;      /* For initial BRA plus length */
int c, firstbyte, reqbyte;
int bracount = 0;
int branch_extra = 0;
int branch_newextra;
int item_count = -1;
int name_count = 0;
int max_name_size = 0;
int lastitemlength = 0;
int errorcode = 0;
BOOL class_utf8;
BOOL capturing;
unsigned int brastackptr = 0;
size_t size;
uschar *code;
const uschar *codestart;
const pcre_uchar *ptr;
const pcre_uchar *patternEnd;
compile_data compile_block;
int brastack[BRASTACK_SIZE];
uschar bralenstack[BRASTACK_SIZE];

/* We can't pass back an error message if errorptr is NULL; I guess the best we
can do is just return NULL, but we can set a code value if there is a code
pointer. */

if (errorptr == NULL)
  {
  return NULL;
  }

*errorptr = NULL;

/* Set up pointers to the individual character tables */

compile_block.lcc = _pcre_default_tables + lcc_offset;
compile_block.fcc = _pcre_default_tables + fcc_offset;
compile_block.cbits = _pcre_default_tables + cbits_offset;
compile_block.ctypes = _pcre_default_tables + ctypes_offset;

/* Maximum back reference and backref bitmap. This is updated for numeric
references during the first pass, but for named references during the actual
compile pass. The bitmap records up to 31 back references to help in deciding
whether (.*) can be treated as anchored or not. */

compile_block.top_backref = 0;
compile_block.backref_map = 0;

/* Reflect pattern for debugging output */

DPRINTF(("------------------------------------------------------------------\n"));

/* The first thing to do is to make a pass over the pattern to compute the
amount of store required to hold the compiled code. This does not have to be
perfect as long as errors are overestimates. At the same time we can detect any
flag settings right at the start, and extract them. Make an attempt to correct
for any counted white space if an "extended" flag setting appears late in the
pattern. We can't be so clever for #-comments. */

ptr = (const pcre_uchar *)(pattern - 1);
patternEnd = (const pcre_uchar *)(pattern + patternLength);

while (++ptr < patternEnd)
  {
  int min = 0, max = 0;
  int class_optcount;
  int bracket_length;
  int duplength;

  c = *ptr;
  
  /* If we are inside a \Q...\E sequence, all chars are literal */

  item_count++;    /* Is zero for the first non-comment item */

  switch(c)
    {
    /* A backslashed item may be an escaped data character or it may be a
    character type. */

    case '\\':
    c = check_escape(&ptr, patternEnd, &errorcode, bracount, FALSE);
    if (errorcode != 0) goto PCRE_ERROR_RETURN;

    lastitemlength = 1;     /* Default length of last item for repeats */

    if (c >= 0)             /* Data character */
      {
      length += 2;          /* For a one-byte character */

      if (c > 127)
        {
        int i;
        for (i = 0; i < _pcre_utf8_table1_size; i++)
          if (c <= _pcre_utf8_table1[i]) break;
        length += i;
        lastitemlength += i;
        }

      continue;
      }

    /* Other escapes need one byte */

    length++;

    /* A back reference needs an additional 2 bytes, plus either one or 5
    bytes for a repeat. We also need to keep the value of the highest
    back reference. */

    if (c <= -ESC_REF)
      {
      int refnum = -c - ESC_REF;
      compile_block.backref_map |= (refnum < 32)? (1 << refnum) : 1;
      if (refnum > compile_block.top_backref)
        compile_block.top_backref = refnum;
      length += 2;   /* For single back reference */
      if (ptr[1] == '{' && is_counted_repeat(ptr+2, patternEnd))
        {
        ptr = read_repeat_counts(ptr+2, &min, &max, &errorcode);
        if (errorcode != 0) goto PCRE_ERROR_RETURN;
        if ((min == 0 && (max == 1 || max == -1)) ||
          (min == 1 && max == -1))
            length++;
        else length += 5;
        if (ptr[1] == '?') ptr++;
            ptr++;
        }
      }
    continue;

    case '^':     /* Single-byte metacharacters */
    case '.':
    case '$':
    length++;
    lastitemlength = 1;
    continue;

    case '*':            /* These repeats won't be after brackets; */
    case '+':            /* those are handled separately */
    case '?':
    length++;
    goto POSESSIVE;      /* A few lines below */

    /* This covers the cases of braced repeats after a single char, metachar,
    class, or back reference. */

    case '{':
    if (!is_counted_repeat(ptr+1, patternEnd)) goto NORMAL_CHAR;
    ptr = read_repeat_counts(ptr+1, &min, &max, &errorcode);
    if (errorcode != 0) goto PCRE_ERROR_RETURN;

    /* These special cases just insert one extra opcode */

    if ((min == 0 && (max == 1 || max == -1)) ||
      (min == 1 && max == -1))
        length++;

    /* These cases might insert additional copies of a preceding character. */

    else
      {
      if (min != 1)
        {
        length -= lastitemlength;   /* Uncount the original char or metachar */
        if (min > 0) length += 3 + lastitemlength;
        }
      length += lastitemlength + ((max > 0)? 3 : 1);
      }

    if (ptr[1] == '?') ptr++;      /* Needs no extra length */

    POSESSIVE:                     /* Test for possessive quantifier */
    if (ptr[1] == '+')
      {
      ptr++;
      length += 2 + 2*LINK_SIZE;   /* Allow for atomic brackets */
      }
    continue;

    /* An alternation contains an offset to the next branch or ket. If any ims
    options changed in the previous branch(es), and/or if we are in a
    lookbehind assertion, extra space will be needed at the start of the
    branch. This is handled by branch_extra. */

    case '|':
    length += 1 + LINK_SIZE + branch_extra;
    continue;

    /* A character class uses 33 characters provided that all the character
    values are less than 256. Otherwise, it uses a bit map for low valued
    characters, and individual items for others. Don't worry about character
    types that aren't allowed in classes - they'll get picked up during the
    compile. A character class that contains only one single-byte character
    uses 2 or 3 bytes, depending on whether it is negated or not. Notice this
    where we can. (In UTF-8 mode we can do this only for chars < 128.) */

    case '[':
    if (*(++ptr) == '^')
      {
      class_optcount = 10;  /* Greater than one */
      ptr++;
      }
    else class_optcount = 0;

    class_utf8 = FALSE;

    /* Written as a "do" so that an initial ']' is taken as data */

    if (*ptr != 0) do
      {
      /* Outside \Q...\E, check for escapes */

      if (*ptr == '\\')
        {
        c = check_escape(&ptr, patternEnd, &errorcode, bracount, TRUE);
        if (errorcode != 0) goto PCRE_ERROR_RETURN;

        /* \b is backspace inside a class; \X is literal */

        if (-c == ESC_b) c = '\b';
        else if (-c == ESC_X) c = 'X';

        /* Handle escapes that turn into characters */

        if (c >= 0) goto NON_SPECIAL_CHARACTER;

        /* Escapes that are meta-things. The normal ones just affect the
        bit map, but Unicode properties require an XCLASS extended item. */

        else
          {
          class_optcount = 10;         /* \d, \s etc; make sure > 1 */
          if (-c == ESC_p || -c == ESC_P)
            {
            if (!class_utf8)
              {
              class_utf8 = TRUE;
              length += LINK_SIZE + 2;
              }
            length += 3;
            }
          }
        }

      /* Anything else increments the possible optimization count. We have to
      detect ranges here so that we can compute the number of extra ranges for
      caseless wide characters when UCP support is available. If there are wide
      characters, we are going to have to use an XCLASS, even for single
      characters. */

      else
        {
        int d;

          {
          int extra = 0;
          GETCHARLENEND(c, ptr, patternEnd, extra);
          ptr += extra;
          }

        /* Come here from handling \ above when it escapes to a char value */

        NON_SPECIAL_CHARACTER:
        class_optcount++;

        d = -1;
        if (ptr + 1 < patternEnd && ptr[1] == '-')
          {
          pcre_uchar const *hyptr = ptr++;
          if (ptr + 1 < patternEnd && ptr[1] == '\\')
            {
            ptr++;
            d = check_escape(&ptr, patternEnd, &errorcode, bracount, TRUE);
            if (errorcode != 0) goto PCRE_ERROR_RETURN;
            if (-d == ESC_b) d = '\b';        /* backspace */
            else if (-d == ESC_X) d = 'X';    /* literal X in a class */
            }
          else if (ptr + 1 < patternEnd && ptr[1] != ']')
            {
            ptr++;
              {
              int extra = 0;
              GETCHARLENEND(d, ptr, patternEnd, extra);
              ptr += extra;
              }
            }
          if (d < 0) ptr = hyptr;      /* go back to hyphen as data */
          }

        /* If d >= 0 we have a range. In UTF-8 mode, if the end is > 255, or >
        127 for caseless matching, we will need to use an XCLASS. */

        if (d >= 0)
          {
          class_optcount = 10;     /* Ensure > 1 */
          if (d < c)
            {
            errorcode = ERR8;
            goto PCRE_ERROR_RETURN;
            }

          if ((d > 255 || ((options & PCRE_CASELESS) != 0 && d > 127)))
            {
            uschar buffer[6];
            if (!class_utf8)         /* Allow for XCLASS overhead */
              {
              class_utf8 = TRUE;
              length += LINK_SIZE + 2;
              }

            /* If we have UCP support, find out how many extra ranges are
            needed to map the other case of characters within this range. We
            have to mimic the range optimization here, because extending the
            range upwards might push d over a boundary that makes is use
            another byte in the UTF-8 representation. */

            if ((options & PCRE_CASELESS) != 0)
              {
              int occ, ocd;
              int cc = c;
              int origd = d;
              while (get_othercase_range(&cc, origd, &occ, &ocd))
                {
                if (occ >= c && ocd <= d) continue;   /* Skip embedded */

                if (occ < c  && ocd >= c - 1)  /* Extend the basic range */
                  {                            /* if there is overlap,   */
                  c = occ;                     /* noting that if occ < c */
                  continue;                    /* we can't have ocd > d  */
                  }                            /* because a subrange is  */
                if (ocd > d && occ <= d + 1)   /* always shorter than    */
                  {                            /* the basic range.       */
                  d = ocd;
                  continue;
                  }

                /* An extra item is needed */

                length += 1 + _pcre_ord2utf8(occ, buffer) +
                  ((occ == ocd)? 0 : _pcre_ord2utf8(ocd, buffer));
                }
              }

            /* The length of the (possibly extended) range */

            length += 1 + _pcre_ord2utf8(c, buffer) + _pcre_ord2utf8(d, buffer);
            }

          }

        /* We have a single character. There is nothing to be done unless we
        are in UTF-8 mode. If the char is > 255, or 127 when caseless, we must
        allow for an XCL_SINGLE item, doubled for caselessness if there is UCP
        support. */

        else
          {
          if ((c > 255 || ((options & PCRE_CASELESS) != 0 && c > 127)))
            {
            uschar buffer[6];
            class_optcount = 10;     /* Ensure > 1 */
            if (!class_utf8)         /* Allow for XCLASS overhead */
              {
              class_utf8 = TRUE;
              length += LINK_SIZE + 2;
              }
            length += (((options & PCRE_CASELESS) != 0)? 2 : 1) *
              (1 + _pcre_ord2utf8(c, buffer));
            }
          }
        }
      }
    while (++ptr < patternEnd && *ptr != ']'); /* Concludes "do" above */

    if (ptr >= patternEnd)                          /* Missing terminating ']' */
      {
      errorcode = ERR6;
      goto PCRE_ERROR_RETURN;
      }

    /* We can optimize when there was only one optimizable character. Repeats
    for positive and negated single one-byte chars are handled by the general
    code. Here, we handle repeats for the class opcodes. */

    if (class_optcount == 1) length += 3; else
      {
      length += 33;

      /* A repeat needs either 1 or 5 bytes. If it is a possessive quantifier,
      we also need extra for wrapping the whole thing in a sub-pattern. */

      if (ptr + 1 < patternEnd && ptr[1] == '{' && is_counted_repeat(ptr+2, patternEnd))
        {
        ptr = read_repeat_counts(ptr+2, &min, &max, &errorcode);
        if (errorcode != 0) goto PCRE_ERROR_RETURN;
        if ((min == 0 && (max == 1 || max == -1)) ||
          (min == 1 && max == -1))
            length++;
        else length += 5;
        if (ptr + 1 < patternEnd && ptr[1] == '+')
          {
          ptr++;
          length += 2 + 2*LINK_SIZE;
          }
        else if (ptr + 1 < patternEnd && ptr[1] == '?') ptr++;
        }
      }
    continue;

    /* Brackets may be genuine groups or special things */

    case '(':
    branch_newextra = 0;
    bracket_length = 1 + LINK_SIZE;
    capturing = FALSE;

    /* Handle special forms of bracket, which all start (? */

    if (ptr + 1 < patternEnd && ptr[1] == '?')
      {
      switch (c = (ptr + 2 < patternEnd ? ptr[2] : 0))
        {
        /* Non-referencing groups and lookaheads just move the pointer on, and
        then behave like a non-special bracket, except that they don't increment
        the count of extracting brackets. Ditto for the "once only" bracket,
        which is in Perl from version 5.005. */

        case ':':
        case '=':
        case '!':
        ptr += 2;
        break;

        /* Else loop checking valid options until ) is met. Anything else is an
        error. If we are without any brackets, i.e. at top level, the settings
        act as if specified in the options, so massage the options immediately.
        This is for backward compatibility with Perl 5.004. */

        default:
        errorcode = ERR12;
        goto PCRE_ERROR_RETURN;
        }
      }

    else capturing = 1;

    /* Capturing brackets must be counted so we can process escapes in a
    Perlish way. If the number exceeds EXTRACT_BASIC_MAX we are going to need
    an additional 3 bytes of memory per capturing bracket. */

    if (capturing)
      {
      bracount++;
      if (bracount > EXTRACT_BASIC_MAX) bracket_length += 3;
      }

    /* Save length for computing whole length at end if there's a repeat that
    requires duplication of the group. Also save the current value of
    branch_extra, and start the new group with the new value. If non-zero, this
    will either be 2 for a (?imsx: group, or 3 for a lookbehind assertion. */

    if (brastackptr >= sizeof(brastack)/sizeof(int))
      {
      errorcode = ERR19;
      goto PCRE_ERROR_RETURN;
      }

    bralenstack[brastackptr] = branch_extra;
    branch_extra = branch_newextra;

    brastack[brastackptr++] = length;
    length += bracket_length;
    continue;

    /* Handle ket. Look for subsequent max/min; for certain sets of values we
    have to replicate this bracket up to that many times. If brastackptr is
    0 this is an unmatched bracket which will generate an error, but take care
    not to try to access brastack[-1] when computing the length and restoring
    the branch_extra value. */

    case ')':
    length += 1 + LINK_SIZE;
    if (brastackptr > 0)
      {
      duplength = length - brastack[--brastackptr];
      branch_extra = bralenstack[brastackptr];
      }
    else duplength = 0;

    /* Leave ptr at the final char; for read_repeat_counts this happens
    automatically; for the others we need an increment. */

    if (ptr + 1 < patternEnd && (c = ptr[1]) == '{' && is_counted_repeat(ptr+2, patternEnd))
      {
      ptr = read_repeat_counts(ptr+2, &min, &max, &errorcode);
      if (errorcode != 0) goto PCRE_ERROR_RETURN;
      }
    else if (c == '*') { min = 0; max = -1; ptr++; }
    else if (c == '+') { min = 1; max = -1; ptr++; }
    else if (c == '?') { min = 0; max = 1;  ptr++; }
    else { min = 1; max = 1; }

    /* If the minimum is zero, we have to allow for an OP_BRAZERO before the
    group, and if the maximum is greater than zero, we have to replicate
    maxval-1 times; each replication acquires an OP_BRAZERO plus a nesting
    bracket set. */

    if (min == 0)
      {
      length++;
      if (max > 0) length += (max - 1) * (duplength + 3 + 2*LINK_SIZE);
      }

    /* When the minimum is greater than zero, we have to replicate up to
    minval-1 times, with no additions required in the copies. Then, if there
    is a limited maximum we have to replicate up to maxval-1 times allowing
    for a BRAZERO item before each optional copy and nesting brackets for all
    but one of the optional copies. */

    else
      {
      length += (min - 1) * duplength;
      if (max > min)   /* Need this test as max=-1 means no limit */
        length += (max - min) * (duplength + 3 + 2*LINK_SIZE)
          - (2 + 2*LINK_SIZE);
      }

    /* Allow space for once brackets for "possessive quantifier" */

    if (ptr + 1 < patternEnd && ptr[1] == '+')
      {
      ptr++;
      length += 2 + 2*LINK_SIZE;
      }
    continue;

    /* Non-special character. It won't be space or # in extended mode, so it is
    always a genuine character. If we are in a \Q...\E sequence, check for the
    end; if not, we have a literal. */

    default:
    NORMAL_CHAR:

    length += 2;          /* For a one-byte character */
    lastitemlength = 1;   /* Default length of last item for repeats */

    /* In UTF-8 mode, check for additional bytes. */

    if (c > 127)
      {
        if (IS_LEADING_SURROGATE(c))
          {
          c = DECODE_SURROGATE_PAIR(c, ptr < patternEnd ? *ptr : 0);
          ++ptr;
          }

        {
          int i;
          for (i = 0; i < _pcre_utf8_table1_size; i++)
            if (c <= _pcre_utf8_table1[i]) break;
          length += i;
          lastitemlength += i;
        }
      }

    continue;
    }
  }

length += 2 + LINK_SIZE;    /* For final KET and END */

if (length > MAX_PATTERN_SIZE)
  {
  errorcode = ERR20;
  goto PCRE_EARLY_ERROR_RETURN;
  }

/* Compute the size of data block needed and get it, either from malloc or
externally provided function. */

size = length + sizeof(real_pcre) + name_count * (max_name_size + 3);
re = (real_pcre *)(pcre_malloc)(size);

if (re == NULL)
  {
  errorcode = ERR21;
  goto PCRE_EARLY_ERROR_RETURN;
  }

/* Put in the magic number, and save the sizes, options, and character table
pointer. NULL is used for the default character tables. The nullpad field is at
the end; it's there to help in the case when a regex compiled on a system with
4-byte pointers is run on another with 8-byte pointers. */

re->size = (pcre_uint32)size;
re->options = options;
re->name_table_offset = sizeof(real_pcre);
re->name_entry_size = max_name_size + 3;
re->name_count = name_count;
re->ref_count = 0;
re->tables = 0;
re->nullpad = NULL;

/* The starting points of the name/number translation table and of the code are
passed around in the compile data block. */

compile_block.names_found = 0;
compile_block.name_entry_size = max_name_size + 3;
compile_block.name_table = (uschar *)re + re->name_table_offset;
codestart = compile_block.name_table + re->name_entry_size * re->name_count;
compile_block.start_code = codestart;
compile_block.start_pattern = (const pcre_uchar *)pattern;
compile_block.req_varyopt = 0;

/* Set up a starting, non-extracting bracket, then compile the expression. On
error, errorcode will be set non-zero, so we don't need to look at the result
of the function here. */

ptr = (const pcre_uchar *)pattern;
code = (uschar *)codestart;
*code = OP_BRA;
bracount = 0;
(void)compile_regex(options, &bracount, &code, &ptr,
  patternEnd,
  &errorcode, 0, &firstbyte, &reqbyte, NULL, &compile_block);
re->top_bracket = bracount;
re->top_backref = compile_block.top_backref;

/* If not reached end of pattern on success, there's an excess bracket. */

if (errorcode == 0 && ptr < patternEnd) errorcode = ERR22;

/* Fill in the terminating state and check for disastrous overflow, but
if debugging, leave the test till after things are printed out. */

*code++ = OP_END;

#ifndef DEBUG
if (code - codestart > length) errorcode = ERR23;
#endif

/* Give an error if there's back reference to a non-existent capturing
subpattern. */

if (re->top_backref > re->top_bracket) errorcode = ERR15;

/* Failed to compile, or error while post-processing */

if (errorcode != 0)
  {
  (pcre_free)(re);
  PCRE_ERROR_RETURN:
  PCRE_EARLY_ERROR_RETURN:
  *errorptr = error_texts[errorcode];
  return NULL;
  }

/* If the anchored option was not passed, set the flag if we can determine that
the pattern is anchored by virtue of ^ characters or \A or anything else (such
as starting with .* when DOTALL is set).

Otherwise, if we know what the first character has to be, save it, because that
speeds up unanchored matches no end. If not, see if we can set the
PCRE_STARTLINE flag. This is helpful for multiline matches when all branches
start with ^. and also when all branches start with .* for non-DOTALL matches.
*/

  {
  int temp_options = options;
  if (is_anchored(codestart, &temp_options, 0, compile_block.backref_map))
    re->options |= PCRE_ANCHORED;
  else
    {
    if (firstbyte < 0)
      firstbyte = find_firstassertedchar(codestart, &temp_options, FALSE);
    if (firstbyte >= 0)   /* Remove caseless flag for non-caseable chars */
      {
      int ch = firstbyte & 255;
      if (ch < 127)
      { /* Strange indentation to aid in merging. */
      re->first_byte = ((firstbyte & REQ_CASELESS) != 0 &&
         compile_block.fcc[ch] == ch)? ch : firstbyte;
      re->options |= PCRE_FIRSTSET;
      }
      }
    else if (is_startline(codestart, 0, compile_block.backref_map))
      re->options |= PCRE_STARTLINE;
    }
  }

/* For an anchored pattern, we use the "required byte" only if it follows a
variable length item in the regex. Remove the caseless flag for non-caseable
bytes. */

if (reqbyte >= 0 &&
     ((re->options & PCRE_ANCHORED) == 0 || (reqbyte & REQ_VARY) != 0))
  {
  int ch = reqbyte & 255;
  if (ch < 127)
  { /* Strange indentation to aid in merging. */
  re->req_byte = ((reqbyte & REQ_CASELESS) != 0 &&
      compile_block.fcc[ch] == ch)? (reqbyte & ~REQ_CASELESS) : reqbyte;
  re->options |= PCRE_REQCHSET;
  }
  }

/* Print out the compiled data if debugging is enabled. This is never the
case when building a production library. */

#ifdef DEBUG

printf("Length = %d top_bracket = %d top_backref = %d\n",
  length, re->top_bracket, re->top_backref);

if (re->options != 0)
  {
  printf("%s%s%s\n",
    ((re->options & PCRE_ANCHORED) != 0)? "anchored " : "",
    ((re->options & PCRE_CASELESS) != 0)? "caseless " : "",
    ((re->options & PCRE_MULTILINE) != 0)? "multiline " : "");
  }

if ((re->options & PCRE_FIRSTSET) != 0)
  {
  int ch = re->first_byte & 255;
  const char *caseless = ((re->first_byte & REQ_CASELESS) == 0)?
    "" : " (caseless)";
  if (isprint(ch)) printf("First char = %c%s\n", ch, caseless);
    else printf("First char = \\x%02x%s\n", ch, caseless);
  }

if ((re->options & PCRE_REQCHSET) != 0)
  {
  int ch = re->req_byte & 255;
  const char *caseless = ((re->req_byte & REQ_CASELESS) == 0)?
    "" : " (caseless)";
  if (isprint(ch)) printf("Req char = %c%s\n", ch, caseless);
    else printf("Req char = \\x%02x%s\n", ch, caseless);
  }

pcre_printint(re, stdout);

/* This check is done here in the debugging case so that the code that
was compiled can be seen. */

if (code - codestart > length)
  {
  (pcre_free)(re);
  *errorptr = error_texts[ERR23];
  return NULL;
  }
#endif

if (numSubpatterns)
    *numSubpatterns = re->top_bracket;
return (pcre *)re;
}

void jsRegExpFree(JSRegExp* re)
{
    pcre_free(re);
}

/* End of pcre_compile.c */
