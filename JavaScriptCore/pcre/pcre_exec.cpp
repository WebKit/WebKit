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

/* This module contains jsRegExpExecute(), the externally visible function
that does pattern matching using an NFA algorithm, following the rules from
the JavaScript specification. There are also some supporting functions. */

#include "config.h"

#include "pcre_internal.h"

#include <wtf/ASCIICType.h>
#include <wtf/Vector.h>

using namespace WTF;

#ifdef __GNUC__
#define USE_COMPUTED_GOTO_FOR_MATCH_RECURSION
//#define USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
#endif

/* Avoid warnings on Windows. */
#undef min
#undef max

/* Structure for building a chain of data that actually lives on the
stack, for holding the values of the subject pointer at the start of each
subpattern, so as to detect when an empty string has been matched by a
subpattern - to break infinite loops. When NO_RECURSE is set, these blocks
are on the heap, not on the stack. */

typedef struct eptrblock {
  struct eptrblock *epb_prev;
  USPTR epb_saved_eptr;
} eptrblock;

/* Structure for remembering the local variables in a private frame */

typedef struct matchframe {
  /* Where to jump back to */
#ifndef USE_COMPUTED_GOTO_FOR_MATCH_RECURSION
  int where;
#else
  void *where;
#endif

  struct matchframe *prevframe;

  /* Function arguments that may change */

  const pcre_uchar *eptr;
  const uschar *ecode;
  int offset_top;
  eptrblock *eptrb;

  /* Function local variables */

  const uschar *data;
  const uschar *next;
  const pcre_uchar *pp;
  const uschar *prev;
  const pcre_uchar *saved_eptr;

  int repeat_othercase;

  int ctype;
  int fc;
  int fi;
  int length;
  int max;
  int number;
  int offset;
  int save_offset1, save_offset2, save_offset3;

  eptrblock newptrb;
} matchframe;

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
  USPTR  end_match_ptr;         /* Subject position at end match */
  int    end_offset_top;        /* Highwater mark at end of match */
  BOOL   multiline;
  BOOL   caseless;
} match_data;

#define match_isgroup      true    /* Set if start of bracketed group */

/* Non-error returns from the match() function. Error returns are externally
defined PCRE_ERROR_xxx codes, which are all negative. */

#define MATCH_MATCH        1
#define MATCH_NOMATCH      0

/* Min and max values for the common repeats; for the maxima, 0 => infinity */

static const char rep_min[] = { 0, 0, 1, 1, 0, 0 };
static const char rep_max[] = { 0, 0, 0, 0, 1, 1 };



#ifdef DEBUG
/*************************************************
*        Debugging function to print chars       *
*************************************************/

/* Print a sequence of chars in printable format, stopping at the end of the
subject if the requested.

Arguments:
  p           points to characters
  length      number to print
  is_subject  true if printing from within md->start_subject
  md          pointer to matching data block, if is_subject is true

Returns:     nothing
*/

static void
pchars(const pcre_uchar *p, int length, BOOL is_subject, match_data *md)
{
int c;
if (is_subject && length > md->end_subject - p) length = md->end_subject - p;
while (length-- > 0)
  if (isprint(c = *(p++))) printf("%c", c);
  else if (c < 256) printf("\\x%02x", c);
  else printf("\\x{%x}", c);
}
#endif



/*************************************************
*          Match a back-reference                *
*************************************************/

/* If a back reference hasn't been set, the length that is passed is greater
than the number of characters left in the string, so the match fails.

Arguments:
  offset      index into the offset vector
  eptr        points into the subject
  length      length to be matched
  md          points to match data block

Returns:      true if matched
*/

static BOOL
match_ref(int offset, register USPTR eptr, int length, match_data *md)
{
USPTR p = md->start_subject + md->offset_vector[offset];

#ifdef DEBUG
if (eptr >= md->end_subject)
  printf("matching subject <null>");
else
  {
  printf("matching subject ");
  pchars(eptr, length, true, md);
  }
printf(" against backref ");
pchars(p, length, false, md);
printf("\n");
#endif

/* Always fail if not enough characters left */

if (length > md->end_subject - eptr) return false;

/* Separate the caselesss case for speed */

if (md->caseless)
  {
  while (length-- > 0)
    {
    pcre_uchar c = *p++;
    int othercase = _pcre_ucp_othercase(c);
    pcre_uchar d = *eptr++;
    if (c != d && othercase != d) return false;
    }
  }
else
  { while (length-- > 0) if (*p++ != *eptr++) return false; }

return true;
}



/***************************************************************************
****************************************************************************
                   RECURSION IN THE match() FUNCTION

The original match() function was highly recursive. The current version
still has the remnants of the original in that recursive processing of the
regular expression is triggered by invoking a macro named RMATCH. This is
no longer really much like a recursive call to match() itself.
****************************************************************************
***************************************************************************/

/* These versions of the macros use the stack, as normal. There are debugging
versions and production versions. */

#ifndef USE_COMPUTED_GOTO_FOR_MATCH_RECURSION

/* Use numbered labels and switch statement at the bottom of the match function. */

#define RMATCH_WHERE(num) num
#define RRETURN_LABEL RRETURN_SWITCH

#else

/* Use GCC's computed goto extension. */

/* For one test case this is more than 40% faster than the switch statement.
We could avoid the use of the num argument entirely by using local labels,
but using it for the GCC case as well as the non-GCC case allows us to share
a bit more code and notice if we use conflicting numbers.*/

#define RMATCH_WHERE(num) &&RRETURN_##num
#define RRETURN_LABEL *frame->where

#endif

#define RMATCH(num, ra, rb, rc)\
  {\
  if (frame >= stackframes && frame + 1 < stackframesend)\
    newframe = frame + 1;\
  else\
    newframe = new matchframe;\
  newframe->eptr = frame->eptr;\
  newframe->ecode = (ra);\
  newframe->offset_top = frame->offset_top;\
  newframe->eptrb = (rb);\
  is_group_start = (rc);\
  ++rdepth;\
  newframe->prevframe = frame;\
  frame = newframe;\
  frame->where = RMATCH_WHERE(num);\
  DPRINTF(("restarting from line %d\n", __LINE__));\
  goto RECURSE;\
RRETURN_##num:\
  newframe = frame;\
  frame = frame->prevframe;\
  if (!(newframe >= stackframes && newframe < stackframesend))\
    delete newframe;\
  --rdepth;\
  DPRINTF(("did a goto back to line %d\n", __LINE__));\
  }
 
#define RRETURN goto RRETURN_LABEL

#define RRETURN_NO_MATCH \
  {\
    is_match = false;\
    RRETURN;\
  }

#define RRETURN_ERROR(error) \
  { \
    i = (error); \
    goto RETURN_ERROR; \
  }

/*************************************************
*         Match from current position            *
*************************************************/

/* On entry ecode points to the first opcode, and eptr to the first character
in the subject string, while eptrb holds the value of eptr at the start of the
last bracketed group - used for breaking infinite loops matching zero-length
strings. This function is called recursively in many circumstances. Whenever it
returns a negative (error) response, the outer incarnation must also return the
same response.

Arguments:
   eptr        pointer in subject
   ecode       position in code
   offset_top  current top pointer
   md          pointer to "static" info for the match

Returns:       MATCH_MATCH if matched            )  these values are >= 0
               MATCH_NOMATCH if failed to match  )
               a negative PCRE_ERROR_xxx value if aborted by an error condition
                 (e.g. stopped by repeated call or recursion limit)
*/

static int match(USPTR eptr, const uschar *ecode, int offset_top, match_data *md)
{
register int is_match = false;
register int i;
register int c;

unsigned rdepth = 0;

BOOL cur_is_word;
BOOL prev_is_word;
BOOL is_group_start = true;
int min;
BOOL minimize = false; /* Initialization not really needed, but some compilers think so. */

/* The value 16 here is large enough that most regular expressions don't require
any calls to pcre_stack_malloc, yet the amount of stack used for the array is
modest enough that we don't run out of stack. */
matchframe stackframes[16];
matchframe *stackframesend = stackframes + sizeof(stackframes) / sizeof(stackframes[0]);

matchframe *frame = stackframes;
matchframe *newframe;

/* The opcode jump table. */
#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
#define EMIT_JUMP_TABLE_ENTRY(opcode) &&LABEL_OP_##opcode,
static void* opcode_jump_table[256] = { FOR_EACH_OPCODE(EMIT_JUMP_TABLE_ENTRY) };
#undef EMIT_JUMP_TABLE_ENTRY
#endif

/* One-time setup of the opcode jump table. */
#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
i = 255;
while (!opcode_jump_table[i])
  opcode_jump_table[i--] = &&CAPTURING_BRACKET;
#endif

#ifdef USE_COMPUTED_GOTO_FOR_MATCH_RECURSION
frame->where = &&RETURN;
#else
frame->where = 0;
#endif

frame->eptr = eptr;
frame->ecode = ecode;
frame->offset_top = offset_top;
frame->eptrb = NULL;

/* This is where control jumps back to to effect "recursion" */

RECURSE:

/* OK, now we can get on with the real code of the function. Recursive calls
are specified by the macro RMATCH and RRETURN is used to return. When
NO_RECURSE is *not* defined, these just turn into a recursive call to match()
and a "return", respectively (possibly with some debugging if DEBUG is
defined). However, RMATCH isn't like a function call because it's quite a
complicated macro. It has to be used in one particular way. This shouldn't,
however, impact performance when true recursion is being used. */

/* First check that we haven't called match() too many times, or that we
haven't exceeded the recursive call limit. */

if (md->match_call_count++ >= MATCH_LIMIT) RRETURN_ERROR(JSRegExpErrorMatchLimit);
if (rdepth >= MATCH_LIMIT_RECURSION) RRETURN_ERROR(JSRegExpErrorRecursionLimit);

/* At the start of a bracketed group, add the current subject pointer to the
stack of such pointers, to be re-instated at the end of the group when we hit
the closing ket. When match() is called in other circumstances, we don't add to
this stack. */

if (is_group_start)
  {
  frame->newptrb.epb_prev = frame->eptrb;
  frame->newptrb.epb_saved_eptr = frame->eptr;
  frame->eptrb = &frame->newptrb;
  }

/* Now start processing the operations. */

#ifndef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
for (;;)
#endif
  {

#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
  #define BEGIN_OPCODE(opcode) LABEL_OP_##opcode
  #define NEXT_OPCODE goto *opcode_jump_table[*frame->ecode]
#else
  #define BEGIN_OPCODE(opcode) case OP_##opcode
  #define NEXT_OPCODE continue
#endif

#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
  NEXT_OPCODE;
#else
  switch (*frame->ecode)
#endif
    {
    /* Non-capturing bracket: optimized */

    BEGIN_OPCODE(BRA):
    NON_CAPTURING_BRACKET:
    DPRINTF(("start bracket 0\n"));
    do
      {
      RMATCH(2, frame->ecode + 1 + LINK_SIZE, frame->eptrb, match_isgroup);
      if (is_match) RRETURN;
      frame->ecode += GET(frame->ecode, 1);
      }
    while (*frame->ecode == OP_ALT);
    DPRINTF(("bracket 0 failed\n"));
    RRETURN;

    /* End of the pattern. */

    BEGIN_OPCODE(END):
    md->end_match_ptr = frame->eptr;          /* Record where we ended */
    md->end_offset_top = frame->offset_top;   /* and how many extracts were taken */
    is_match = true;
    RRETURN;

    /* Assertion brackets. Check the alternative branches in turn - the
    matching won't pass the KET for an assertion. If any one branch matches,
    the assertion is true. Lookbehind assertions have an OP_REVERSE item at the
    start of each branch to move the current point backwards, so the code at
    this level is identical to the lookahead case. */

    BEGIN_OPCODE(ASSERT):
    do
      {
      RMATCH(6, frame->ecode + 1 + LINK_SIZE, NULL, match_isgroup);
      if (is_match) break;
      frame->ecode += GET(frame->ecode, 1);
      }
    while (*frame->ecode == OP_ALT);
    if (*frame->ecode == OP_KET) RRETURN_NO_MATCH;

    /* Continue from after the assertion, updating the offsets high water
    mark, since extracts may have been taken during the assertion. */

    do frame->ecode += GET(frame->ecode,1); while (*frame->ecode == OP_ALT);
    frame->ecode += 1 + LINK_SIZE;
    frame->offset_top = md->end_offset_top;
    NEXT_OPCODE;

    /* Negative assertion: all branches must fail to match */

    BEGIN_OPCODE(ASSERT_NOT):
    do
      {
      RMATCH(7, frame->ecode + 1 + LINK_SIZE, NULL, match_isgroup);
      if (is_match) RRETURN_NO_MATCH;
      frame->ecode += GET(frame->ecode,1);
      }
    while (*frame->ecode == OP_ALT);

    frame->ecode += 1 + LINK_SIZE;
    NEXT_OPCODE;

    /* "Once" brackets are like assertion brackets except that after a match,
    the point in the subject string is not moved back. Thus there can never be
    a move back into the brackets. Friedl calls these "atomic" subpatterns.
    Check the alternative branches in turn - the matching won't pass the KET
    for this kind of subpattern. If any one branch matches, we carry on as at
    the end of a normal bracket, leaving the subject pointer. */

    BEGIN_OPCODE(ONCE):
      frame->prev = frame->ecode;
      frame->saved_eptr = frame->eptr;

      do
        {
        RMATCH(9, frame->ecode + 1 + LINK_SIZE, frame->eptrb, match_isgroup);
        if (is_match) break;
        frame->ecode += GET(frame->ecode,1);
        }
      while (*frame->ecode == OP_ALT);

      /* If hit the end of the group (which could be repeated), fail */

      if (*frame->ecode != OP_ONCE && *frame->ecode != OP_ALT) RRETURN;

      /* Continue as from after the assertion, updating the offsets high water
      mark, since extracts may have been taken. */

      do frame->ecode += GET(frame->ecode,1); while (*frame->ecode == OP_ALT);

      frame->offset_top = md->end_offset_top;
      frame->eptr = md->end_match_ptr;

      /* For a non-repeating ket, just continue at this level. This also
      happens for a repeating ket if no characters were matched in the group.
      This is the forcible breaking of infinite loops as implemented in Perl
      5.005. If there is an options reset, it will get obeyed in the normal
      course of events. */

      if (*frame->ecode == OP_KET || frame->eptr == frame->saved_eptr)
        {
        frame->ecode += 1+LINK_SIZE;
        NEXT_OPCODE;
        }

      /* The repeating kets try the rest of the pattern or restart from the
      preceding bracket, in the appropriate order. We need to reset any options
      that changed within the bracket before re-running it, so check the next
      opcode. */

      if (*frame->ecode == OP_KETRMIN)
        {
        RMATCH(10, frame->ecode + 1 + LINK_SIZE, frame->eptrb, 0);
        if (is_match) RRETURN;
        RMATCH(11, frame->prev, frame->eptrb, match_isgroup);
        if (is_match) RRETURN;
        }
      else  /* OP_KETRMAX */
        {
        RMATCH(12, frame->prev, frame->eptrb, match_isgroup);
        if (is_match) RRETURN;
        RMATCH(13, frame->ecode + 1+LINK_SIZE, frame->eptrb, 0);
        if (is_match) RRETURN;
        }
    RRETURN;

    /* An alternation is the end of a branch; scan along to find the end of the
    bracketed group and go to there. */

    BEGIN_OPCODE(ALT):
    do frame->ecode += GET(frame->ecode,1); while (*frame->ecode == OP_ALT);
    NEXT_OPCODE;

    /* BRAZERO and BRAMINZERO occur just before a bracket group, indicating
    that it may occur zero times. It may repeat infinitely, or not at all -
    i.e. it could be ()* or ()? in the pattern. Brackets with fixed upper
    repeat limits are compiled as a number of copies, with the optional ones
    preceded by BRAZERO or BRAMINZERO. */

    BEGIN_OPCODE(BRAZERO):
      {
      frame->next = frame->ecode+1;
      RMATCH(14, frame->next, frame->eptrb, match_isgroup);
      if (is_match) RRETURN;
      do frame->next += GET(frame->next,1); while (*frame->next == OP_ALT);
      frame->ecode = frame->next + 1+LINK_SIZE;
      }
    NEXT_OPCODE;

    BEGIN_OPCODE(BRAMINZERO):
      {
      frame->next = frame->ecode+1;
      do frame->next += GET(frame->next,1); while (*frame->next == OP_ALT);
      RMATCH(15, frame->next + 1+LINK_SIZE, frame->eptrb, match_isgroup);
      if (is_match) RRETURN;
      frame->ecode++;
      }
    NEXT_OPCODE;

    /* End of a group, repeated or non-repeating. If we are at the end of
    an assertion "group", stop matching and return MATCH_MATCH, but record the
    current high water mark for use by positive assertions. Do this also
    for the "once" (not-backup up) groups. */

    BEGIN_OPCODE(KET):
    BEGIN_OPCODE(KETRMIN):
    BEGIN_OPCODE(KETRMAX):
      {
      frame->prev = frame->ecode - GET(frame->ecode, 1);
      frame->saved_eptr = frame->eptrb->epb_saved_eptr;

      /* Back up the stack of bracket start pointers. */

      frame->eptrb = frame->eptrb->epb_prev;

      if (*frame->prev == OP_ASSERT || *frame->prev == OP_ASSERT_NOT || *frame->prev == OP_ONCE)
        {
        md->end_match_ptr = frame->eptr;      /* For ONCE */
        md->end_offset_top = frame->offset_top;
        is_match = true;
        RRETURN;
        }

      /* In all other cases except a conditional group we have to check the
      group number back at the start and if necessary complete handling an
      extraction by setting the offsets and bumping the high water mark. */

        frame->number = *frame->prev - OP_BRA;

        /* For extended extraction brackets (large number), we have to fish out
        the number from a dummy opcode at the start. */

        if (frame->number > EXTRACT_BASIC_MAX) frame->number = GET2(frame->prev, 2+LINK_SIZE);
        frame->offset = frame->number << 1;

#ifdef DEBUG
        printf("end bracket %d", frame->number);
        printf("\n");
#endif

        /* Test for a numbered group. This includes groups called as a result
        of recursion. Note that whole-pattern recursion is coded as a recurse
        into group 0, so it won't be picked up here. Instead, we catch it when
        the OP_END is reached. */

        if (frame->number > 0)
          {
          if (frame->offset >= md->offset_max) md->offset_overflow = true; else
            {
            md->offset_vector[frame->offset] =
              md->offset_vector[md->offset_end - frame->number];
            md->offset_vector[frame->offset+1] = frame->eptr - md->start_subject;
            if (frame->offset_top <= frame->offset) frame->offset_top = frame->offset + 2;
            }
          }

      /* For a non-repeating ket, just continue at this level. This also
      happens for a repeating ket if no characters were matched in the group.
      This is the forcible breaking of infinite loops as implemented in Perl
      5.005. If there is an options reset, it will get obeyed in the normal
      course of events. */

      if (*frame->ecode == OP_KET || frame->eptr == frame->saved_eptr)
        {
        frame->ecode += 1 + LINK_SIZE;
        NEXT_OPCODE;
        }

      /* The repeating kets try the rest of the pattern or restart from the
      preceding bracket, in the appropriate order. */

      if (*frame->ecode == OP_KETRMIN)
        {
        RMATCH(16, frame->ecode + 1+LINK_SIZE, frame->eptrb, 0);
        if (is_match) RRETURN;
        RMATCH(17, frame->prev, frame->eptrb, match_isgroup);
        if (is_match) RRETURN;
        }
      else  /* OP_KETRMAX */
        {
        RMATCH(18, frame->prev, frame->eptrb, match_isgroup);
        if (is_match) RRETURN;
        RMATCH(19, frame->ecode + 1+LINK_SIZE, frame->eptrb, 0);
        if (is_match) RRETURN;
        }
      }
    RRETURN;

    /* Start of subject unless notbol, or after internal newline if multiline */

    BEGIN_OPCODE(CIRC):
    if (md->multiline)
      {
      if (frame->eptr != md->start_subject && !IS_NEWLINE(frame->eptr[-1]))
        RRETURN_NO_MATCH;
      frame->ecode++;
      NEXT_OPCODE;
      }
    if (frame->eptr != md->start_subject) RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    /* Assert before internal newline if multiline, or before a terminating
    newline unless endonly is set, else end of subject unless noteol is set. */

    BEGIN_OPCODE(DOLL):
    if (md->multiline)
      {
      if (frame->eptr < md->end_subject)
        { if (!IS_NEWLINE(*frame->eptr)) RRETURN_NO_MATCH; }
      frame->ecode++;
      }
    else
      {
      if (frame->eptr < md->end_subject - 1 ||
         (frame->eptr == md->end_subject - 1 && !IS_NEWLINE(*frame->eptr)))
        RRETURN_NO_MATCH;
      frame->ecode++;
      }
    NEXT_OPCODE;

    /* Word boundary assertions */

    BEGIN_OPCODE(NOT_WORD_BOUNDARY):
    BEGIN_OPCODE(WORD_BOUNDARY):
      {
      /* Find out if the previous and current characters are "word" characters.
      It takes a bit more work in UTF-8 mode. Characters > 128 are assumed to
      be "non-word" characters. */

        {
        if (frame->eptr == md->start_subject) prev_is_word = false; else
          {
          const pcre_uchar *lastptr = frame->eptr - 1;
          while(ISMIDCHAR(*lastptr)) lastptr--;
          GETCHAR(c, lastptr);
          prev_is_word = c < 128 && (md->ctypes[c] & ctype_word) != 0;
          }
        if (frame->eptr >= md->end_subject) cur_is_word = false; else
          {
          GETCHAR(c, frame->eptr);
          cur_is_word = c < 128 && (md->ctypes[c] & ctype_word) != 0;
          }
        }

      /* Now see if the situation is what we want */

      if ((*frame->ecode++ == OP_WORD_BOUNDARY)?
           cur_is_word == prev_is_word : cur_is_word != prev_is_word)
        RRETURN_NO_MATCH;
      }
    NEXT_OPCODE;

    /* Match a single character type; inline for speed */

    BEGIN_OPCODE(ANY):
    if (frame->eptr < md->end_subject && IS_NEWLINE(*frame->eptr))
      RRETURN_NO_MATCH;
    if (frame->eptr++ >= md->end_subject) RRETURN_NO_MATCH;
      while (frame->eptr < md->end_subject && ISMIDCHAR(*frame->eptr)) frame->eptr++;
    frame->ecode++;
    NEXT_OPCODE;

    BEGIN_OPCODE(NOT_DIGIT):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    GETCHARINCTEST(c, frame->eptr);
    if (isASCIIDigit(c))
      RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    BEGIN_OPCODE(DIGIT):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    GETCHARINCTEST(c, frame->eptr);
    if (!isASCIIDigit(c))
      RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    BEGIN_OPCODE(NOT_WHITESPACE):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    GETCHARINCTEST(c, frame->eptr);
    if (c < 128 && (md->ctypes[c] & ctype_space) != 0)
      RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    BEGIN_OPCODE(WHITESPACE):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    GETCHARINCTEST(c, frame->eptr);
    if (c >= 128 || (md->ctypes[c] & ctype_space) == 0)
      RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    BEGIN_OPCODE(NOT_WORDCHAR):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    GETCHARINCTEST(c, frame->eptr);
    if (c < 128 && (md->ctypes[c] & ctype_word) != 0)
      RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    BEGIN_OPCODE(WORDCHAR):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    GETCHARINCTEST(c, frame->eptr);
    if (c >= 128 || (md->ctypes[c] & ctype_word) == 0)
      RRETURN_NO_MATCH;
    frame->ecode++;
    NEXT_OPCODE;

    /* Match a back reference, possibly repeatedly. Look past the end of the
    item to see if there is repeat information following. The code is similar
    to that for character classes, but repeated for efficiency. Then obey
    similar code to character type repeats - written out again for speed.
    However, if the referenced string is the empty string, always treat
    it as matched, any number of times (otherwise there could be infinite
    loops). */

    BEGIN_OPCODE(REF):
      frame->offset = GET2(frame->ecode, 1) << 1;               /* Doubled ref number */
      frame->ecode += 3;                                 /* Advance past item */

      /* If the reference is unset, set the length to be longer than the amount
      of subject left; this ensures that every attempt at a match fails. We
      can't just fail here, because of the possibility of quantifiers with zero
      minima. */

      frame->length = (frame->offset >= frame->offset_top || md->offset_vector[frame->offset] < 0)?
        0 :
        md->offset_vector[frame->offset+1] - md->offset_vector[frame->offset];

      /* Set up for repetition, or handle the non-repeated case */

      switch (*frame->ecode)
        {
        case OP_CRSTAR:
        case OP_CRMINSTAR:
        case OP_CRPLUS:
        case OP_CRMINPLUS:
        case OP_CRQUERY:
        case OP_CRMINQUERY:
        c = *frame->ecode++ - OP_CRSTAR;
        minimize = (c & 1) != 0;
        min = rep_min[c];                 /* Pick up values from tables; */
        frame->max = rep_max[c];                 /* zero for max => infinity */
        if (frame->max == 0) frame->max = INT_MAX;
        break;

        case OP_CRRANGE:
        case OP_CRMINRANGE:
        minimize = (*frame->ecode == OP_CRMINRANGE);
        min = GET2(frame->ecode, 1);
        frame->max = GET2(frame->ecode, 3);
        if (frame->max == 0) frame->max = INT_MAX;
        frame->ecode += 5;
        break;

        default:               /* No repeat follows */
        if (!match_ref(frame->offset, frame->eptr, frame->length, md)) RRETURN_NO_MATCH;
        frame->eptr += frame->length;
        NEXT_OPCODE;
        }

      /* If the length of the reference is zero, just continue with the
      main loop. */

      if (frame->length == 0)
        NEXT_OPCODE;

      /* First, ensure the minimum number of matches are present. */

      for (i = 1; i <= min; i++)
        {
        if (!match_ref(frame->offset, frame->eptr, frame->length, md)) RRETURN_NO_MATCH;
        frame->eptr += frame->length;
        }

      /* If min = max, continue at the same level without recursion.
      They are not both allowed to be zero. */

      if (min == frame->max)
        NEXT_OPCODE;

      /* If minimizing, keep trying and advancing the pointer */

      if (minimize)
        {
        for (frame->fi = min;; frame->fi++)
          {
          RMATCH(20, frame->ecode, frame->eptrb, 0);
          if (is_match) RRETURN;
          if (frame->fi >= frame->max || !match_ref(frame->offset, frame->eptr, frame->length, md))
            RRETURN;
          frame->eptr += frame->length;
          }
        /* Control never gets here */
        }

      /* If maximizing, find the longest string and work backwards */

      else
        {
        frame->pp = frame->eptr;
        for (i = min; i < frame->max; i++)
          {
          if (!match_ref(frame->offset, frame->eptr, frame->length, md)) break;
          frame->eptr += frame->length;
          }
        while (frame->eptr >= frame->pp)
          {
          RMATCH(21, frame->ecode, frame->eptrb, 0);
          if (is_match) RRETURN;
          frame->eptr -= frame->length;
          }
        RRETURN_NO_MATCH;
        }
    /* Control never gets here */

    /* Match a bit-mapped character class, possibly repeatedly. This op code is
    used when all the characters in the class have values in the range 0-255,
    and either the matching is caseful, or the characters are in the range
    0-127 when UTF-8 processing is enabled. The only difference between
    OP_CLASS and OP_NCLASS occurs when a data character outside the range is
    encountered.

    First, look past the end of the item to see if there is repeat information
    following. Then obey similar code to character type repeats - written out
    again for speed. */

    BEGIN_OPCODE(NCLASS):
    BEGIN_OPCODE(CLASS):
      frame->data = frame->ecode + 1;                /* Save for matching */
      frame->ecode += 33;                     /* Advance past the item */

      switch (*frame->ecode)
        {
        case OP_CRSTAR:
        case OP_CRMINSTAR:
        case OP_CRPLUS:
        case OP_CRMINPLUS:
        case OP_CRQUERY:
        case OP_CRMINQUERY:
        c = *frame->ecode++ - OP_CRSTAR;
        minimize = (c & 1) != 0;
        min = rep_min[c];                 /* Pick up values from tables; */
        frame->max = rep_max[c];                 /* zero for max => infinity */
        if (frame->max == 0) frame->max = INT_MAX;
        break;

        case OP_CRRANGE:
        case OP_CRMINRANGE:
        minimize = (*frame->ecode == OP_CRMINRANGE);
        min = GET2(frame->ecode, 1);
        frame->max = GET2(frame->ecode, 3);
        if (frame->max == 0) frame->max = INT_MAX;
        frame->ecode += 5;
        break;

        default:               /* No repeat follows */
        min = frame->max = 1;
        break;
        }

      /* First, ensure the minimum number of matches are present. */

        {
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
          GETCHARINC(c, frame->eptr);
          if (c > 255)
            {
            if (frame->data[-1] == OP_CLASS) RRETURN_NO_MATCH;
            }
          else
            {
            if ((frame->data[c/8] & (1 << (c&7))) == 0) RRETURN_NO_MATCH;
            }
          }
        }

      /* If max == min we can continue with the main loop without the
      need to recurse. */

      if (min == frame->max)
        NEXT_OPCODE;      

      /* If minimizing, keep testing the rest of the expression and advancing
      the pointer while it matches the class. */
      if (minimize)
        {
          {
          for (frame->fi = min;; frame->fi++)
            {
            RMATCH(22, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            if (frame->fi >= frame->max || frame->eptr >= md->end_subject) RRETURN;
            GETCHARINC(c, frame->eptr);
            if (c > 255)
              {
              if (frame->data[-1] == OP_CLASS) RRETURN;
              }
            else
              {
              if ((frame->data[c/8] & (1 << (c&7))) == 0) RRETURN;
              }
            }
          }
        /* Control never gets here */
        }
      /* If maximizing, find the longest possible run, then work backwards. */
      else
        {
        frame->pp = frame->eptr;

          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (c > 255)
              {
              if (frame->data[-1] == OP_CLASS) break;
              }
            else
              {
              if ((frame->data[c/8] & (1 << (c&7))) == 0) break;
              }
            frame->eptr += len;
            }
          for (;;)
            {
            RMATCH(24, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            if (frame->eptr-- == frame->pp) break;        /* Stop if tried at original pos */
            BACKCHAR(frame->eptr);
            }

        RRETURN;
        }
    /* Control never gets here */

    /* Match an extended character class. This opcode is encountered only
    in UTF-8 mode, because that's the only time it is compiled. */

    BEGIN_OPCODE(XCLASS):
      frame->data = frame->ecode + 1 + LINK_SIZE;                /* Save for matching */
      frame->ecode += GET(frame->ecode, 1);                      /* Advance past the item */

      switch (*frame->ecode)
        {
        case OP_CRSTAR:
        case OP_CRMINSTAR:
        case OP_CRPLUS:
        case OP_CRMINPLUS:
        case OP_CRQUERY:
        case OP_CRMINQUERY:
        c = *frame->ecode++ - OP_CRSTAR;
        minimize = (c & 1) != 0;
        min = rep_min[c];                 /* Pick up values from tables; */
        frame->max = rep_max[c];                 /* zero for max => infinity */
        if (frame->max == 0) frame->max = INT_MAX;
        break;

        case OP_CRRANGE:
        case OP_CRMINRANGE:
        minimize = (*frame->ecode == OP_CRMINRANGE);
        min = GET2(frame->ecode, 1);
        frame->max = GET2(frame->ecode, 3);
        if (frame->max == 0) frame->max = INT_MAX;
        frame->ecode += 5;
        break;

        default:               /* No repeat follows */
        min = frame->max = 1;
        }

      /* First, ensure the minimum number of matches are present. */

      for (i = 1; i <= min; i++)
        {
        if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
        GETCHARINC(c, frame->eptr);
        if (!_pcre_xclass(c, frame->data)) RRETURN_NO_MATCH;
        }

      /* If max == min we can continue with the main loop without the
      need to recurse. */

      if (min == frame->max)
        NEXT_OPCODE;

      /* If minimizing, keep testing the rest of the expression and advancing
      the pointer while it matches the class. */

      if (minimize)
        {
        for (frame->fi = min;; frame->fi++)
          {
          RMATCH(26, frame->ecode, frame->eptrb, 0);
          if (is_match) RRETURN;
          if (frame->fi >= frame->max || frame->eptr >= md->end_subject) RRETURN;
          GETCHARINC(c, frame->eptr);
          if (!_pcre_xclass(c, frame->data)) RRETURN;
          }
        /* Control never gets here */
        }

      /* If maximizing, find the longest possible run, then work backwards. */

      else
        {
        frame->pp = frame->eptr;
        for (i = min; i < frame->max; i++)
          {
          int len = 1;
          if (frame->eptr >= md->end_subject) break;
          GETCHARLEN(c, frame->eptr, len);
          if (!_pcre_xclass(c, frame->data)) break;
          frame->eptr += len;
          }
        for(;;)
          {
          RMATCH(27, frame->ecode, frame->eptrb, 0);
          if (is_match) RRETURN;
          if (frame->eptr-- == frame->pp) break;        /* Stop if tried at original pos */
          BACKCHAR(frame->eptr)
          }
        RRETURN;
        }

    /* Control never gets here */

    /* Match a single character, casefully */

    BEGIN_OPCODE(CHAR):
      frame->length = 1;
      frame->ecode++;
      GETUTF8CHARLEN(frame->fc, frame->ecode, frame->length);
      {
        int dc;
        frame->ecode += frame->length;
        switch (md->end_subject - frame->eptr)
        {
          case 0:
            RRETURN_NO_MATCH;
          case 1:
            dc = *frame->eptr++;
            if (IS_LEADING_SURROGATE(dc))
              RRETURN_NO_MATCH;
            break;
          default:
            GETCHARINC(dc, frame->eptr);
        }
        if (frame->fc != dc) RRETURN_NO_MATCH;
      }
    NEXT_OPCODE;

    /* Match a single character, caselessly */

    BEGIN_OPCODE(CHARNC):
      frame->length = 1;
      frame->ecode++;
      GETUTF8CHARLEN(frame->fc, frame->ecode, frame->length);

      if (md->end_subject - frame->eptr == 0) RRETURN_NO_MATCH;

        {
        int dc;
        if (md->end_subject - frame->eptr == 1) {
          dc = *frame->eptr++;
          if (IS_LEADING_SURROGATE(dc))
            RRETURN_NO_MATCH;
        } else
        GETCHARINC(dc, frame->eptr);
        frame->ecode += frame->length;

        /* If we have Unicode property support, we can use it to test the other
        case of the character, if there is one. */

        if (frame->fc != dc)
          {
          if (dc != _pcre_ucp_othercase(frame->fc))
            RRETURN_NO_MATCH;
          }
        }
    NEXT_OPCODE;

    /* Match a single ASCII character. */

    BEGIN_OPCODE(ASCII_CHAR):
    if (md->end_subject == frame->eptr)
      RRETURN_NO_MATCH;
    if (*frame->eptr != frame->ecode[1])
      RRETURN_NO_MATCH;
    ++frame->eptr;
    frame->ecode += 2;
    NEXT_OPCODE;

    /* Match one of two cases of an ASCII character. */

    BEGIN_OPCODE(ASCII_LETTER_NC):
    if (md->end_subject == frame->eptr)
      RRETURN_NO_MATCH;
    if ((*frame->eptr | 0x20) != frame->ecode[1])
      RRETURN_NO_MATCH;
    ++frame->eptr;
    frame->ecode += 2;
    NEXT_OPCODE;

    /* Match a single character repeatedly; different opcodes share code. */

    BEGIN_OPCODE(EXACT):
    min = frame->max = GET2(frame->ecode, 1);
    minimize = false;
    frame->ecode += 3;
    goto REPEATCHAR;

    BEGIN_OPCODE(UPTO):
    BEGIN_OPCODE(MINUPTO):
    min = 0;
    frame->max = GET2(frame->ecode, 1);
    minimize = *frame->ecode == OP_MINUPTO;
    frame->ecode += 3;
    goto REPEATCHAR;

    BEGIN_OPCODE(STAR):
    BEGIN_OPCODE(MINSTAR):
    BEGIN_OPCODE(PLUS):
    BEGIN_OPCODE(MINPLUS):
    BEGIN_OPCODE(QUERY):
    BEGIN_OPCODE(MINQUERY):
    c = *frame->ecode++ - OP_STAR;
    minimize = (c & 1) != 0;
    min = rep_min[c];                 /* Pick up values from tables; */
    frame->max = rep_max[c];                 /* zero for max => infinity */
    if (frame->max == 0) frame->max = INT_MAX;

    /* Common code for all repeated single-character matches. We can give
    up quickly if there are fewer than the minimum number of characters left in
    the subject. */

    REPEATCHAR:

      frame->length = 1;
      GETUTF8CHARLEN(frame->fc, frame->ecode, frame->length);
      if (min * (frame->fc > 0xFFFF ? 2 : 1) > md->end_subject - frame->eptr) RRETURN_NO_MATCH;
      frame->ecode += frame->length;

      if (frame->fc <= 0xFFFF)
        {
        int othercase = md->caseless ? _pcre_ucp_othercase(frame->fc) : -1;

        for (i = 1; i <= min; i++)
          {
          if (*frame->eptr != frame->fc && *frame->eptr != othercase) RRETURN_NO_MATCH;
          ++frame->eptr;
          }

        if (min == frame->max)
          NEXT_OPCODE;

        if (minimize)
          {
          frame->repeat_othercase = othercase;
          for (frame->fi = min;; frame->fi++)
            {
            RMATCH(28, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            if (frame->fi >= frame->max || frame->eptr >= md->end_subject) RRETURN;
            if (*frame->eptr != frame->fc && *frame->eptr != frame->repeat_othercase) RRETURN;
            ++frame->eptr;
            }
          /* Control never gets here */
          }
        else
          {
          frame->pp = frame->eptr;
          for (i = min; i < frame->max; i++)
            {
            if (frame->eptr >= md->end_subject) break;
            if (*frame->eptr != frame->fc && *frame->eptr != othercase) break;
            ++frame->eptr;
            }
          while (frame->eptr >= frame->pp)
           {
           RMATCH(29, frame->ecode, frame->eptrb, 0);
           if (is_match) RRETURN;
           --frame->eptr;
           }
          RRETURN_NO_MATCH;
          }
        /* Control never gets here */
        }
      else
        {
        /* No case on surrogate pairs, so no need to bother with "othercase". */

        for (i = 1; i <= min; i++)
          {
          int nc;
          GETCHAR(nc, frame->eptr);
          if (nc != frame->fc) RRETURN_NO_MATCH;
          frame->eptr += 2;
          }

        if (min == frame->max)
          NEXT_OPCODE;

        if (minimize)
          {
          for (frame->fi = min;; frame->fi++)
            {
            int nc;
            RMATCH(30, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            if (frame->fi >= frame->max || frame->eptr >= md->end_subject) RRETURN;
            GETCHAR(nc, frame->eptr);
            if (*frame->eptr != frame->fc) RRETURN;
            frame->eptr += 2;
            }
          /* Control never gets here */
          }
        else
          {
          frame->pp = frame->eptr;
          for (i = min; i < frame->max; i++)
            {
            int nc;
            if (frame->eptr > md->end_subject - 2) break;
            GETCHAR(nc, frame->eptr);
            if (*frame->eptr != frame->fc) break;
            frame->eptr += 2;
            }
          while (frame->eptr >= frame->pp)
           {
           RMATCH(31, frame->ecode, frame->eptrb, 0);
           if (is_match) RRETURN;
           frame->eptr -= 2;
           }
          RRETURN_NO_MATCH;
          }
          /* Control never gets here */
        }
    /* Control never gets here */

    /* Match a negated single one-byte character. The character we are
    checking can be multibyte. */

    BEGIN_OPCODE(NOT):
    if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
    frame->ecode++;
    GETCHARINCTEST(c, frame->eptr);
    if (md->caseless)
      {
      if (c < 128)
        c = md->lcc[c];
      if (md->lcc[*frame->ecode++] == c) RRETURN_NO_MATCH;
      }
    else
      {
      if (*frame->ecode++ == c) RRETURN_NO_MATCH;
      }
    NEXT_OPCODE;

    /* Match a negated single one-byte character repeatedly. This is almost a
    repeat of the code for a repeated single character, but I haven't found a
    nice way of commoning these up that doesn't require a test of the
    positive/negative option for each character match. Maybe that wouldn't add
    very much to the time taken, but character matching *is* what this is all
    about... */

    BEGIN_OPCODE(NOTEXACT):
    min = frame->max = GET2(frame->ecode, 1);
    minimize = false;
    frame->ecode += 3;
    goto REPEATNOTCHAR;

    BEGIN_OPCODE(NOTUPTO):
    BEGIN_OPCODE(NOTMINUPTO):
    min = 0;
    frame->max = GET2(frame->ecode, 1);
    minimize = *frame->ecode == OP_NOTMINUPTO;
    frame->ecode += 3;
    goto REPEATNOTCHAR;

    BEGIN_OPCODE(NOTSTAR):
    BEGIN_OPCODE(NOTMINSTAR):
    BEGIN_OPCODE(NOTPLUS):
    BEGIN_OPCODE(NOTMINPLUS):
    BEGIN_OPCODE(NOTQUERY):
    BEGIN_OPCODE(NOTMINQUERY):
    c = *frame->ecode++ - OP_NOTSTAR;
    minimize = (c & 1) != 0;
    min = rep_min[c];                 /* Pick up values from tables; */
    frame->max = rep_max[c];                 /* zero for max => infinity */
    if (frame->max == 0) frame->max = INT_MAX;

    /* Common code for all repeated single-byte matches. We can give up quickly
    if there are fewer than the minimum number of bytes left in the
    subject. */

    REPEATNOTCHAR:
    if (min > md->end_subject - frame->eptr) RRETURN_NO_MATCH;
    frame->fc = *frame->ecode++;

    /* The code is duplicated for the caseless and caseful cases, for speed,
    since matching characters is likely to be quite common. First, ensure the
    minimum number of matches are present. If min = max, continue at the same
    level without recursing. Otherwise, if minimizing, keep trying the rest of
    the expression and advancing one matching character if failing, up to the
    maximum. Alternatively, if maximizing, find the maximum number of
    characters and work backwards. */

    DPRINTF(("negative matching %c{%d,%d}\n", frame->fc, min, frame->max));

    if (md->caseless)
      {
      if (frame->fc < 128)
        frame->fc = md->lcc[frame->fc];

        {
        register int d;
        for (i = 1; i <= min; i++)
          {
          GETCHARINC(d, frame->eptr);
          if (d < 128) d = md->lcc[d];
          if (frame->fc == d) RRETURN_NO_MATCH;
          }
        }

      if (min == frame->max)
        NEXT_OPCODE;      

      if (minimize)
        {
          {
          register int d;
          for (frame->fi = min;; frame->fi++)
            {
            RMATCH(38, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            GETCHARINC(d, frame->eptr);
            if (d < 128) d = md->lcc[d];
            if (frame->fi >= frame->max || frame->eptr >= md->end_subject || frame->fc == d)
              RRETURN;
            }
          }
        /* Control never gets here */
        }

      /* Maximize case */

      else
        {
        frame->pp = frame->eptr;

          {
          register int d;
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(d, frame->eptr, len);
            if (d < 128) d = md->lcc[d];
            if (frame->fc == d) break;
            frame->eptr += len;
            }
          for(;;)
            {
            RMATCH(40, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            if (frame->eptr-- == frame->pp) break;        /* Stop if tried at original pos */
            BACKCHAR(frame->eptr);
            }
          }

        RRETURN;
        }
      /* Control never gets here */
      }

    /* Caseful comparisons */

    else
      {
        {
        register int d;
        for (i = 1; i <= min; i++)
          {
          GETCHARINC(d, frame->eptr);
          if (frame->fc == d) RRETURN_NO_MATCH;
          }
        }

      if (min == frame->max)
        NEXT_OPCODE;

      if (minimize)
        {
          {
          register int d;
          for (frame->fi = min;; frame->fi++)
            {
            RMATCH(42, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            GETCHARINC(d, frame->eptr);
            if (frame->fi >= frame->max || frame->eptr >= md->end_subject || frame->fc == d)
              RRETURN;
            }
          }
        /* Control never gets here */
        }

      /* Maximize case */

      else
        {
        frame->pp = frame->eptr;

          {
          register int d;
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(d, frame->eptr, len);
            if (frame->fc == d) break;
            frame->eptr += len;
            }
          for(;;)
            {
            RMATCH(44, frame->ecode, frame->eptrb, 0);
            if (is_match) RRETURN;
            if (frame->eptr-- == frame->pp) break;        /* Stop if tried at original pos */
            BACKCHAR(frame->eptr);
            }
          }

        RRETURN;
        }
      }
    /* Control never gets here */

    /* Match a single character type repeatedly; several different opcodes
    share code. This is very similar to the code for single characters, but we
    repeat it in the interests of efficiency. */

    BEGIN_OPCODE(TYPEEXACT):
    min = frame->max = GET2(frame->ecode, 1);
    minimize = true;
    frame->ecode += 3;
    goto REPEATTYPE;

    BEGIN_OPCODE(TYPEUPTO):
    BEGIN_OPCODE(TYPEMINUPTO):
    min = 0;
    frame->max = GET2(frame->ecode, 1);
    minimize = *frame->ecode == OP_TYPEMINUPTO;
    frame->ecode += 3;
    goto REPEATTYPE;

    BEGIN_OPCODE(TYPESTAR):
    BEGIN_OPCODE(TYPEMINSTAR):
    BEGIN_OPCODE(TYPEPLUS):
    BEGIN_OPCODE(TYPEMINPLUS):
    BEGIN_OPCODE(TYPEQUERY):
    BEGIN_OPCODE(TYPEMINQUERY):
    c = *frame->ecode++ - OP_TYPESTAR;
    minimize = (c & 1) != 0;
    min = rep_min[c];                 /* Pick up values from tables; */
    frame->max = rep_max[c];                 /* zero for max => infinity */
    if (frame->max == 0) frame->max = INT_MAX;

    /* Common code for all repeated single character type matches. Note that
    in UTF-8 mode, '.' matches a character of any length, but for the other
    character types, the valid characters are all one-byte long. */

    REPEATTYPE:
    frame->ctype = *frame->ecode++;      /* Code for the character type */

    /* First, ensure the minimum number of matches are present. Use inline
    code for maximizing the speed, and do the type test once at the start
    (i.e. keep it out of the loop). Also we can test that there are at least
    the minimum number of bytes before we start. This isn't as effective in
    UTF-8 mode, but it does no harm. Separate the UTF-8 code completely as that
    is tidier. Also separate the UCP code, which can be the same for both UTF-8
    and single-bytes. */

    if (min > md->end_subject - frame->eptr) RRETURN_NO_MATCH;
    if (min > 0)
      {
      switch(frame->ctype)
        {
        case OP_ANY:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject || IS_NEWLINE(*frame->eptr))
            RRETURN_NO_MATCH;
          ++frame->eptr;
          while (frame->eptr < md->end_subject && ISMIDCHAR(*frame->eptr)) frame->eptr++;
          }
        break;

        case OP_NOT_DIGIT:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject) RRETURN_NO_MATCH;
          GETCHARINC(c, frame->eptr);
          if (isASCIIDigit(c))
            RRETURN_NO_MATCH;
          }
        break;

        case OP_DIGIT:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject || !isASCIIDigit(*frame->eptr++))
            RRETURN_NO_MATCH;
          /* No need to skip more bytes - we know it's a 1-byte character */
          }
        break;

        case OP_NOT_WHITESPACE:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject ||
             (*frame->eptr < 128 && (md->ctypes[*frame->eptr] & ctype_space) != 0))
            RRETURN_NO_MATCH;
          while (++frame->eptr < md->end_subject && ISMIDCHAR(*frame->eptr));
          }
        break;

        case OP_WHITESPACE:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject ||
             *frame->eptr >= 128 || (md->ctypes[*frame->eptr++] & ctype_space) == 0)
            RRETURN_NO_MATCH;
          /* No need to skip more bytes - we know it's a 1-byte character */
          }
        break;

        case OP_NOT_WORDCHAR:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject ||
             (*frame->eptr < 128 && (md->ctypes[*frame->eptr] & ctype_word) != 0))
            RRETURN_NO_MATCH;
          while (++frame->eptr < md->end_subject && ISMIDCHAR(*frame->eptr));
          }
        break;

        case OP_WORDCHAR:
        for (i = 1; i <= min; i++)
          {
          if (frame->eptr >= md->end_subject ||
             *frame->eptr >= 128 || (md->ctypes[*frame->eptr++] & ctype_word) == 0)
            RRETURN_NO_MATCH;
          /* No need to skip more bytes - we know it's a 1-byte character */
          }
        break;

        default:
        ASSERT_NOT_REACHED();
        RRETURN_ERROR(JSRegExpErrorInternal);
        }  /* End switch(frame->ctype) */
      }

    /* If min = max, continue at the same level without recursing */

    if (min == frame->max)
      NEXT_OPCODE;    

    /* If minimizing, we have to test the rest of the pattern before each
    subsequent match. */

    if (minimize)
      {
        {
        for (frame->fi = min;; frame->fi++)
          {
          RMATCH(48, frame->ecode, frame->eptrb, 0);
          if (is_match) RRETURN;
          if (frame->fi >= frame->max || frame->eptr >= md->end_subject) RRETURN;

          GETCHARINC(c, frame->eptr);
          switch(frame->ctype)
            {
            case OP_ANY:
            if (IS_NEWLINE(c)) RRETURN;
            break;

            case OP_NOT_DIGIT:
            if (isASCIIDigit(c))
              RRETURN;
            break;

            case OP_DIGIT:
            if (!isASCIIDigit(c))
              RRETURN;
            break;

            case OP_NOT_WHITESPACE:
            if (c < 128 && (md->ctypes[c] & ctype_space) != 0)
              RRETURN;
            break;

            case OP_WHITESPACE:
            if  (c >= 128 || (md->ctypes[c] & ctype_space) == 0)
              RRETURN;
            break;

            case OP_NOT_WORDCHAR:
            if (c < 128 && (md->ctypes[c] & ctype_word) != 0)
              RRETURN;
            break;

            case OP_WORDCHAR:
            if (c >= 128 || (md->ctypes[c] & ctype_word) == 0)
              RRETURN;
            break;

            default:
            ASSERT_NOT_REACHED();
            RRETURN_ERROR(JSRegExpErrorInternal);
            }
          }
        }
      /* Control never gets here */
      }

    /* If maximizing it is worth using inline code for speed, doing the type
    test once at the start (i.e. keep it out of the loop). */

    else
      {
      frame->pp = frame->eptr;  /* Remember where we started */

        switch(frame->ctype)
          {
          case OP_ANY:

          /* Special code is required for UTF8, but when the maximum is unlimited
          we don't need it, so we repeat the non-UTF8 code. This is probably
          worth it, because .* is quite a common idiom. */

          if (frame->max < INT_MAX)
            {
              {
              for (i = min; i < frame->max; i++)
                {
                if (frame->eptr >= md->end_subject || IS_NEWLINE(*frame->eptr)) break;
                frame->eptr++;
                while (frame->eptr < md->end_subject && (*frame->eptr & 0xc0) == 0x80) frame->eptr++;
                }
              }
            }

          /* Handle unlimited UTF-8 repeat */

          else
            {
              {
              for (i = min; i < frame->max; i++)
                {
                if (frame->eptr >= md->end_subject || IS_NEWLINE(*frame->eptr)) break;
                frame->eptr++;
                }
              break;
              }
            }
          break;

          case OP_NOT_DIGIT:
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (isASCIIDigit(c)) break;
            frame->eptr+= len;
            }
          break;

          case OP_DIGIT:
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (!isASCIIDigit(c)) break;
            frame->eptr+= len;
            }
          break;

          case OP_NOT_WHITESPACE:
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (c < 128 && (md->ctypes[c] & ctype_space) != 0) break;
            frame->eptr+= len;
            }
          break;

          case OP_WHITESPACE:
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (c >= 128 ||(md->ctypes[c] & ctype_space) == 0) break;
            frame->eptr+= len;
            }
          break;

          case OP_NOT_WORDCHAR:
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (c < 128 && (md->ctypes[c] & ctype_word) != 0) break;
            frame->eptr+= len;
            }
          break;

          case OP_WORDCHAR:
          for (i = min; i < frame->max; i++)
            {
            int len = 1;
            if (frame->eptr >= md->end_subject) break;
            GETCHARLEN(c, frame->eptr, len);
            if (c >= 128 || (md->ctypes[c] & ctype_word) == 0) break;
            frame->eptr+= len;
            }
          break;

          default:
          ASSERT_NOT_REACHED();
          RRETURN_ERROR(JSRegExpErrorInternal);
          }

        /* frame->eptr is now past the end of the maximum run */

        for(;;)
          {
          RMATCH(52, frame->ecode, frame->eptrb, 0);
          if (is_match) RRETURN;
          if (frame->eptr-- == frame->pp) break;        /* Stop if tried at original pos */
          BACKCHAR(frame->eptr);
          }

      /* Get here if we can't make it match with any permitted repetitions */

      RRETURN;
      }
    /* Control never gets here */

    BEGIN_OPCODE(BRANUMBER):
    BEGIN_OPCODE(CRMINPLUS):
    BEGIN_OPCODE(CRMINQUERY):
    BEGIN_OPCODE(CRMINRANGE):
    BEGIN_OPCODE(CRMINSTAR):
    BEGIN_OPCODE(CRPLUS):
    BEGIN_OPCODE(CRQUERY):
    BEGIN_OPCODE(CRRANGE):
    BEGIN_OPCODE(CRSTAR):
      ASSERT_NOT_REACHED();
      RRETURN_ERROR(JSRegExpErrorInternal);

#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
    CAPTURING_BRACKET:
#else
    default:
#endif
      /* Opening capturing bracket. If there is space in the offset vector, save
      the current subject position in the working slot at the top of the vector. We
      mustn't change the current values of the data slot, because they may be set
      from a previous iteration of this group, and be referred to by a reference
      inside the group.

      If the bracket fails to match, we need to restore this value and also the
      values of the final offsets, in case they were set by a previous iteration of
      the same bracket.

      If there isn't enough space in the offset vector, treat this as if it were a
      non-capturing bracket. Don't worry about setting the flag for the error case
      here; that is handled in the code for KET. */

      ASSERT(*frame->ecode > OP_BRA);

        frame->number = *frame->ecode - OP_BRA;

        /* For extended extraction brackets (large number), we have to fish out the
        number from a dummy opcode at the start. */

        if (frame->number > EXTRACT_BASIC_MAX)
          frame->number = GET2(frame->ecode, 2+LINK_SIZE);
        frame->offset = frame->number << 1;

#ifdef DEBUG
        printf("start bracket %d subject=", frame->number);
        pchars(frame->eptr, 16, true, md);
        printf("\n");
#endif

        if (frame->offset < md->offset_max)
          {
          frame->save_offset1 = md->offset_vector[frame->offset];
          frame->save_offset2 = md->offset_vector[frame->offset + 1];
          frame->save_offset3 = md->offset_vector[md->offset_end - frame->number];

          DPRINTF(("saving %d %d %d\n", frame->save_offset1, frame->save_offset2, frame->save_offset3));
          md->offset_vector[md->offset_end - frame->number] = frame->eptr - md->start_subject;

          do
            {
            RMATCH(1, frame->ecode + 1 + LINK_SIZE, frame->eptrb, match_isgroup);
            if (is_match) RRETURN;
            frame->ecode += GET(frame->ecode, 1);
            }
          while (*frame->ecode == OP_ALT);

          DPRINTF(("bracket %d failed\n", frame->number));

          md->offset_vector[frame->offset] = frame->save_offset1;
          md->offset_vector[frame->offset + 1] = frame->save_offset2;
          md->offset_vector[md->offset_end - frame->number] = frame->save_offset3;

          RRETURN;
          }

        /* Insufficient room for saving captured contents */

        goto NON_CAPTURING_BRACKET;
    }

  /* Do not stick any code in here without much thought; it is assumed
  that "continue" in the code above comes out to here to repeat the main
  loop. */

  } /* End of main loop */

/* Control never reaches here */

#ifndef USE_COMPUTED_GOTO_FOR_MATCH_RECURSION

RRETURN_SWITCH:
switch (frame->where)
  {
  case 0: goto RETURN;
  case 1: goto RRETURN_1;
  case 2: goto RRETURN_2;
  case 6: goto RRETURN_6;
  case 7: goto RRETURN_7;
  case 9: goto RRETURN_9;
  case 10: goto RRETURN_10;
  case 11: goto RRETURN_11;
  case 12: goto RRETURN_12;
  case 13: goto RRETURN_13;
  case 14: goto RRETURN_14;
  case 15: goto RRETURN_15;
  case 16: goto RRETURN_16;
  case 17: goto RRETURN_17;
  case 18: goto RRETURN_18;
  case 19: goto RRETURN_19;
  case 20: goto RRETURN_20;
  case 21: goto RRETURN_21;
  case 22: goto RRETURN_22;
  case 24: goto RRETURN_24;
  case 26: goto RRETURN_26;
  case 27: goto RRETURN_27;
  case 28: goto RRETURN_28;
  case 29: goto RRETURN_29;
  case 30: goto RRETURN_30;
  case 31: goto RRETURN_31;
  case 38: goto RRETURN_38;
  case 40: goto RRETURN_40;
  case 42: goto RRETURN_42;
  case 44: goto RRETURN_44;
  case 48: goto RRETURN_48;
  case 52: goto RRETURN_52;
  }

abort();
RRETURN_ERROR(JSRegExpErrorInternal);

#endif

RETURN:
    return is_match ? MATCH_MATCH : MATCH_NOMATCH;

RETURN_ERROR:
    while (!(frame >= stackframes && frame < stackframesend)) {
        newframe = frame->prevframe;
        delete frame;
        frame = newframe;
    }
    return i;
}


/*************************************************
*         Execute a Regular Expression           *
*************************************************/

/* This function applies a compiled re to a subject string and picks out
portions of the string if it matches. Two elements in the vector are set for
each substring: the offsets to the start and end of the substring.

Arguments:
  argument_re     points to the compiled expression
  extra_data      points to extra data or is NULL
  subject         points to the subject string
  length          length of subject string (may contain binary zeros)
  start_offset    where to start in the subject string
  options         option bits
  offsets         points to a vector of ints to be filled in with offsets
  offsetcount     the number of elements in the vector

Returns:          > 0 => success; value is the number of elements filled in
                  = 0 => success, but offsets is not big enough
                   -1 => failed to match
                 < -1 => some kind of unexpected problem
*/

int
jsRegExpExecute(const pcre *argument_re,
  const UChar* subject, int length, int start_offset, int *offsets,
  int offsetcount)
{
int rc, resetcount, ocount;
int first_byte = -1;
int req_byte = -1;
int req_byte2 = -1;
BOOL using_temporary_offsets = false;
BOOL first_byte_caseless = false;
BOOL startline;
BOOL req_byte_caseless = false;
match_data match_block;
USPTR start_match = (USPTR)subject + start_offset;
USPTR end_subject;
USPTR req_byte_ptr = start_match - 1;
const uschar *start_code;

const real_pcre *external_re = (const real_pcre *)argument_re;
const real_pcre *re = external_re;

/* Plausibility checks */

ASSERT(re);
ASSERT(subject);
ASSERT(offsetcount >= 0);
ASSERT(offsets || offsetcount == 0);

/* Set up other data */

startline = (re->options & PCRE_STARTLINE) != 0;

/* The code starts after the real_pcre block and the capture name table. */

start_code = (const uschar *)(external_re + 1);

match_block.start_subject = (USPTR)subject;
match_block.end_subject = match_block.start_subject + length;
end_subject = match_block.end_subject;

match_block.lcc = _pcre_default_tables + lcc_offset;
match_block.ctypes = _pcre_default_tables + ctypes_offset;

match_block.multiline = (re->options & PCRE_MULTILINE) != 0;
match_block.caseless = (re->options & PCRE_CASELESS) != 0;

/* If the expression has got more back references than the offsets supplied can
hold, we get a temporary chunk of working store to use during the matching.
Otherwise, we can use the vector supplied, rounding down its size to a multiple
of 3. */

ocount = offsetcount - (offsetcount % 3);

if (re->top_backref > 0 && re->top_backref >= ocount/3)
  {
  ocount = re->top_backref * 3 + 3;
  match_block.offset_vector = new int[ocount];
  if (match_block.offset_vector == NULL) return JSRegExpErrorNoMemory;
  using_temporary_offsets = true;
  DPRINTF(("Got memory to hold back references\n"));
  }
else match_block.offset_vector = offsets;

match_block.offset_end = ocount;
match_block.offset_max = (2*ocount)/3;
match_block.offset_overflow = false;

/* Compute the minimum number of offsets that we need to reset each time. Doing
this makes a huge difference to execution time when there aren't many brackets
in the pattern. */

resetcount = 2 + re->top_bracket * 2;
if (resetcount > offsetcount) resetcount = ocount;

/* Reset the working variable associated with each extraction. These should
never be used unless previously set, but they get saved and restored, and so we
initialize them to avoid reading uninitialized locations. */

if (match_block.offset_vector != NULL)
  {
  register int *iptr = match_block.offset_vector + ocount;
  register int *iend = iptr - resetcount/2 + 1;
  while (--iptr >= iend) *iptr = -1;
  }

/* Set up the first character to match, if available. The first_byte value is
never set for an anchored regular expression, but the anchoring may be forced
at run time, so we have to test for anchoring. The first char may be unset for
an unanchored pattern, of course. If there's no first char and the pattern was
studied, there may be a bitmap of possible first characters. */

  if ((re->options & PCRE_FIRSTSET) != 0)
    {
    first_byte = re->first_byte & 255;
    if ((first_byte_caseless = ((re->first_byte & REQ_CASELESS) != 0)) == true)
      first_byte = match_block.lcc[first_byte];
    }

/* For anchored or unanchored matches, there may be a "last known required
character" set. */

if ((re->options & PCRE_REQCHSET) != 0)
  {
  req_byte = re->req_byte & 255;
  req_byte_caseless = (re->req_byte & REQ_CASELESS) != 0;
  req_byte2 = (_pcre_default_tables + fcc_offset)[req_byte];  /* case flipped */
  }

/* Loop for handling unanchored repeated matching attempts; for anchored regexs
the loop runs just once. */

do
  {
  USPTR save_end_subject = end_subject;

  /* Reset the maximum number of extractions we might see. */

  if (match_block.offset_vector != NULL)
    {
    register int *iptr = match_block.offset_vector;
    register int *iend = iptr + resetcount;
    while (iptr < iend) *iptr++ = -1;
    }

  /* Advance to a unique first char if possible. If firstline is true, the
  start of the match is constrained to the first line of a multiline string.
  Implement this by temporarily adjusting end_subject so that we stop scanning
  at a newline. If the match fails at the newline, later code breaks this loop.
  */

  /* Now test for a unique first byte */

  if (first_byte >= 0)
    {
    pcre_uchar first_char = first_byte;
    if (first_byte_caseless)
      while (start_match < end_subject)
        {
        int sm = *start_match;
        if (sm > 127)
          break;
        if (match_block.lcc[sm] == first_char)
          break;
        start_match++;
        }
    else
      while (start_match < end_subject && *start_match != first_char)
        start_match++;
    }

  /* Or to just after \n for a multiline match if possible */

  else if (startline)
    {
    if (start_match > match_block.start_subject + start_offset)
      {
      while (start_match < end_subject && !IS_NEWLINE(start_match[-1]))
        start_match++;
      }
    }

  /* Restore fudged end_subject */

  end_subject = save_end_subject;

#ifdef DEBUG  /* Sigh. Some compilers never learn. */
  printf(">>>> Match against: ");
  pchars(start_match, end_subject - start_match, true, &match_block);
  printf("\n");
#endif

  /* If req_byte is set, we know that that character must appear in the subject
  for the match to succeed. If the first character is set, req_byte must be
  later in the subject; otherwise the test starts at the match point. This
  optimization can save a huge amount of backtracking in patterns with nested
  unlimited repeats that aren't going to match. Writing separate code for
  cased/caseless versions makes it go faster, as does using an autoincrement
  and backing off on a match.

  HOWEVER: when the subject string is very, very long, searching to its end can
  take a long time, and give bad performance on quite ordinary patterns. This
  showed up when somebody was matching /^C/ on a 32-megabyte string... so we
  don't do this when the string is sufficiently long.

  ALSO: this processing is disabled when partial matching is requested.
  */

  if (req_byte >= 0 &&
      end_subject - start_match < REQ_BYTE_MAX)
    {
    register USPTR p = start_match + ((first_byte >= 0)? 1 : 0);

    /* We don't need to repeat the search if we haven't yet reached the
    place we found it at last time. */

    if (p > req_byte_ptr)
      {
      if (req_byte_caseless)
        {
        while (p < end_subject)
          {
          register int pp = *p++;
          if (pp == req_byte || pp == req_byte2) { p--; break; }
          }
        }
      else
        {
        while (p < end_subject)
          {
          if (*p++ == req_byte) { p--; break; }
          }
        }

      /* If we can't find the required character, break the matching loop */

      if (p >= end_subject) break;

      /* If we have found the required character, save the point where we
      found it, so that we don't search again next time round the loop if
      the start hasn't passed this character yet. */

      req_byte_ptr = p;
      }
    }

  /* When a match occurs, substrings will be set for all internal extractions;
  we just need to set up the whole thing as substring 0 before returning. If
  there were too many extractions, set the return code to zero. In the case
  where we had to get some local store to hold offsets for backreferences, copy
  those back references that we can. In this case there need not be overflow
  if certain parts of the pattern were not used. */

  match_block.match_call_count = 0;

  rc = match(start_match, start_code, 2, &match_block);

  /* When the result is no match, if the subject's first character was a
  newline and the PCRE_FIRSTLINE option is set, break (which will return
  PCRE_ERROR_NOMATCH). The option requests that a match occur before the first
  newline in the subject. Otherwise, advance the pointer to the next character
  and continue - but the continuation will actually happen only when the
  pattern is not anchored. */

  if (rc == MATCH_NOMATCH)
    {
    start_match++;
      while(start_match < end_subject && ISMIDCHAR(*start_match))
        start_match++;
    continue;
    }

  if (rc != MATCH_MATCH)
    {
    DPRINTF((">>>> error: returning %d\n", rc));
    return rc;
    }

  /* We have a match! Copy the offset information from temporary store if
  necessary */

  if (using_temporary_offsets)
    {
    if (offsetcount >= 4)
      {
      memcpy(offsets + 2, match_block.offset_vector + 2,
        (offsetcount - 2) * sizeof(int));
      DPRINTF(("Copied offsets from temporary memory\n"));
      }
    if (match_block.end_offset_top > offsetcount)
      match_block.offset_overflow = true;

    DPRINTF(("Freeing temporary memory\n"));
    delete [] match_block.offset_vector;
    }

  rc = match_block.offset_overflow? 0 : match_block.end_offset_top/2;

  if (offsetcount < 2) rc = 0; else
    {
    offsets[0] = start_match - match_block.start_subject;
    offsets[1] = match_block.end_match_ptr - match_block.start_subject;
    }

  DPRINTF((">>>> returning %d\n", rc));
  return rc;
  }

/* This "while" is the end of the "do" above */

while (start_match <= end_subject);

if (using_temporary_offsets)
  {
  DPRINTF(("Freeing temporary memory\n"));
  delete [] match_block.offset_vector;
  }

  DPRINTF((">>>> returning PCRE_ERROR_NOMATCH\n"));
  return JSRegExpErrorNoMatch;
}
