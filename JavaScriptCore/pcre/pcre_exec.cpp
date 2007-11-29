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

struct eptrblock {
  struct eptrblock* epb_prev;
  UChar* epb_saved_eptr;
};

/* Structure for remembering the local variables in a private frame */



#ifndef USE_COMPUTED_GOTO_FOR_MATCH_RECURSION
typedef int ReturnLocation;
#else
typedef void* ReturnLocation;
#endif

struct MatchFrame {
  ReturnLocation returnLocation;

  struct MatchFrame* previousFrame;

  /* Function arguments that may change */

  UChar* eptr;
  const uschar* ecode;
  int offset_top;
  eptrblock* eptrb;

  /* Function local variables */

  const uschar* data;
  const uschar* next;
  const UChar* pp;
  const uschar* prev;
  const UChar* saved_eptr;

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
};

/* Structure for passing "static" information around between the functions
doing traditional NFA matching, so that they are thread-safe. */

typedef struct match_data {
  unsigned long int match_call_count;      /* As it says */
  int   *offset_vector;         /* Offset vector */
  int    offset_end;            /* One past the end */
  int    offset_max;            /* The maximum usable for return data */
  const uschar *lcc;            /* Points to lower casing table */
  const uschar *ctypes;         /* Points to table of type maps */
  bool   offset_overflow;       /* Set if too many extractions */
  UChar*  start_subject;         /* Start of the subject string */
  UChar*  end_subject;           /* End of the subject string */
  UChar*  end_match_ptr;         /* Subject position at end match */
  int    end_offset_top;        /* Highwater mark at end of match */
  bool   multiline;
  bool   caseless;
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
pchars(const UChar* p, int length, bool is_subject, match_data *md)
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

static bool
match_ref(int offset, UChar* eptr, int length, match_data *md)
{
UChar* p = md->start_subject + md->offset_vector[offset];

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
    UChar c = *p++;
    int othercase = _pcre_ucp_othercase(c);
    UChar d = *eptr++;
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
#define RRETURN_LABEL *stack.currentFrame->returnLocation

#endif

#define RMATCH(num, ra, rb, rc)\
  {\
    stack.pushNewFrame((ra), (rb), RMATCH_WHERE(num)); \
    is_group_start = (rc);\
    ++rdepth;\
    DPRINTF(("restarting from line %d\n", __LINE__));\
    goto RECURSE;\
RRETURN_##num:\
    stack.popCurrentFrame(); \
    --rdepth;\
    DPRINTF(("did a goto back to line %d\n", __LINE__));\
  }
 
#define RRETURN goto RRETURN_LABEL

#define RRETURN_NO_MATCH \
  {\
    is_match = false;\
    RRETURN;\
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

struct MatchStack {
    MatchStack()
    {
        framesEnd = frames + sizeof(frames) / sizeof(frames[0]);
        currentFrame = frames;
    }
    
    /* The value 16 here is large enough that most regular expressions don't require
     any calls to pcre_stack_malloc, yet the amount of stack used for the array is
     modest enough that we don't run out of stack. */
    MatchFrame frames[16];
    MatchFrame* framesEnd;
    MatchFrame* currentFrame;
    
    inline bool canUseStackBufferForNextFrame()
    {
        return (currentFrame >= frames && currentFrame + 1 < framesEnd);
    }
    
    inline MatchFrame* allocateNextFrame()
    {
        if (canUseStackBufferForNextFrame())
            return currentFrame + 1;
        return new MatchFrame;
    }
    
    inline void pushNewFrame(const uschar* ecode, eptrblock* eptrb, ReturnLocation returnLocation)
    {
        MatchFrame* newframe = allocateNextFrame();
        newframe->previousFrame = currentFrame;

        newframe->eptr = currentFrame->eptr;
        newframe->offset_top = currentFrame->offset_top;
        newframe->ecode = ecode;
        newframe->eptrb = eptrb;
        newframe->returnLocation = returnLocation;

        currentFrame = newframe;
    }
    
    inline bool frameIsStackAllocated(MatchFrame* frame)
    {
        return (frame >= frames && frame < framesEnd);
    }
    
    inline void popCurrentFrame()
    {
        MatchFrame* oldFrame = currentFrame;
        currentFrame = currentFrame->previousFrame;
        if (!frameIsStackAllocated(oldFrame))
            delete oldFrame;
    }

    void unrollAnyHeapAllocatedFrames()
    {
        while (!frameIsStackAllocated(currentFrame)) {
            MatchFrame* oldFrame = currentFrame;
            currentFrame = currentFrame->previousFrame;
            delete oldFrame;
        }
    }
};

static int matchError(int errorCode, MatchStack& stack)
{
    stack.unrollAnyHeapAllocatedFrames();
    return errorCode;
}

/* Get the next UTF-8 character, not advancing the pointer, incrementing length
 if there are extra bytes. This is called when we know we are in UTF-8 mode. */

static inline void getUTF8CharAndIncrementLength(int& c, const uschar* eptr, int& len)
{
    c = *eptr;
    if ((c & 0xc0) == 0xc0) {
        int gcaa = _pcre_utf8_table4[c & 0x3f];  /* Number of additional bytes */
        int gcss = 6 * gcaa;
        c = (c & _pcre_utf8_table3[gcaa]) << gcss;
        for (int gcii = 1; gcii <= gcaa; gcii++) {
            gcss -= 6;
            c |= (eptr[gcii] & 0x3f) << gcss;
        }
        len += gcaa;
    }
}

static int match(UChar* eptr, const uschar* ecode, int offset_top, match_data* md)
{
    int is_match = false;
    int i;
    int c;
    
    unsigned rdepth = 0;
    
    bool cur_is_word;
    bool prev_is_word;
    bool is_group_start = true;
    int min;
    bool minimize = false; /* Initialization not really needed, but some compilers think so. */
    
    MatchStack stack;

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
    stack.currentFrame->returnLocation = &&RETURN;
#else
    stack.currentFrame->returnLocation = 0;
#endif
    
    stack.currentFrame->eptr = eptr;
    stack.currentFrame->ecode = ecode;
    stack.currentFrame->offset_top = offset_top;
    stack.currentFrame->eptrb = NULL;
    
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
    
    if (md->match_call_count++ >= MATCH_LIMIT)
        return matchError(JSRegExpErrorMatchLimit, stack);
    if (rdepth >= MATCH_LIMIT_RECURSION)
        return matchError(JSRegExpErrorRecursionLimit, stack);
    
    /* At the start of a bracketed group, add the current subject pointer to the
     stack of such pointers, to be re-instated at the end of the group when we hit
     the closing ket. When match() is called in other circumstances, we don't add to
     this stack. */
    
    if (is_group_start) {
        stack.currentFrame->newptrb.epb_prev = stack.currentFrame->eptrb;
        stack.currentFrame->newptrb.epb_saved_eptr = stack.currentFrame->eptr;
        stack.currentFrame->eptrb = &stack.currentFrame->newptrb;
    }
    
    /* Now start processing the operations. */
    
#ifndef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
    while (true)
#endif
    {
        
#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
#define BEGIN_OPCODE(opcode) LABEL_OP_##opcode
#define NEXT_OPCODE goto *opcode_jump_table[*stack.currentFrame->ecode]
#else
#define BEGIN_OPCODE(opcode) case OP_##opcode
#define NEXT_OPCODE continue
#endif
        
#ifdef USE_COMPUTED_GOTO_FOR_MATCH_OPCODE_LOOP
        NEXT_OPCODE;
#else
        switch (*stack.currentFrame->ecode)
#endif
        {
                /* Non-capturing bracket: optimized */
                
                BEGIN_OPCODE(BRA):
            NON_CAPTURING_BRACKET:
                DPRINTF(("start bracket 0\n"));
                do {
                    RMATCH(2, stack.currentFrame->ecode + 1 + LINK_SIZE, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                    stack.currentFrame->ecode += GET(stack.currentFrame->ecode, 1);
                } while (*stack.currentFrame->ecode == OP_ALT);
                DPRINTF(("bracket 0 failed\n"));
                RRETURN;
                
                /* Skip over large extraction number data if encountered. */
                
                BEGIN_OPCODE(BRANUMBER):
                stack.currentFrame->ecode += 3;
                NEXT_OPCODE;
                
                /* End of the pattern. */
                
                BEGIN_OPCODE(END):
                md->end_match_ptr = stack.currentFrame->eptr;          /* Record where we ended */
                md->end_offset_top = stack.currentFrame->offset_top;   /* and how many extracts were taken */
                is_match = true;
                RRETURN;
                
                /* Assertion brackets. Check the alternative branches in turn - the
                 matching won't pass the KET for an assertion. If any one branch matches,
                 the assertion is true. Lookbehind assertions have an OP_REVERSE item at the
                 start of each branch to move the current point backwards, so the code at
                 this level is identical to the lookahead case. */
                
                BEGIN_OPCODE(ASSERT):
                do {
                    RMATCH(6, stack.currentFrame->ecode + 1 + LINK_SIZE, NULL, match_isgroup);
                    if (is_match)
                        break;
                    stack.currentFrame->ecode += GET(stack.currentFrame->ecode, 1);
                } while (*stack.currentFrame->ecode == OP_ALT);
                if (*stack.currentFrame->ecode == OP_KET)
                    RRETURN_NO_MATCH;
                
                /* Continue from after the assertion, updating the offsets high water
                 mark, since extracts may have been taken during the assertion. */
                
                do stack.currentFrame->ecode += GET(stack.currentFrame->ecode,1); while (*stack.currentFrame->ecode == OP_ALT);
                stack.currentFrame->ecode += 1 + LINK_SIZE;
                stack.currentFrame->offset_top = md->end_offset_top;
                NEXT_OPCODE;
                
                /* Negative assertion: all branches must fail to match */
                
                BEGIN_OPCODE(ASSERT_NOT):
                do {
                    RMATCH(7, stack.currentFrame->ecode + 1 + LINK_SIZE, NULL, match_isgroup);
                    if (is_match)
                        RRETURN_NO_MATCH;
                    stack.currentFrame->ecode += GET(stack.currentFrame->ecode,1);
                } while (*stack.currentFrame->ecode == OP_ALT);
                
                stack.currentFrame->ecode += 1 + LINK_SIZE;
                NEXT_OPCODE;
                
                /* "Once" brackets are like assertion brackets except that after a match,
                 the point in the subject string is not moved back. Thus there can never be
                 a move back into the brackets. Friedl calls these "atomic" subpatterns.
                 Check the alternative branches in turn - the matching won't pass the KET
                 for this kind of subpattern. If any one branch matches, we carry on as at
                 the end of a normal bracket, leaving the subject pointer. */
                
                BEGIN_OPCODE(ONCE):
                stack.currentFrame->prev = stack.currentFrame->ecode;
                stack.currentFrame->saved_eptr = stack.currentFrame->eptr;
                
                do {
                    RMATCH(9, stack.currentFrame->ecode + 1 + LINK_SIZE, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        break;
                    stack.currentFrame->ecode += GET(stack.currentFrame->ecode,1);
                } while (*stack.currentFrame->ecode == OP_ALT);
                
                /* If hit the end of the group (which could be repeated), fail */
                
                if (*stack.currentFrame->ecode != OP_ONCE && *stack.currentFrame->ecode != OP_ALT)
                    RRETURN;
                
                /* Continue as from after the assertion, updating the offsets high water
                 mark, since extracts may have been taken. */
                
                do stack.currentFrame->ecode += GET(stack.currentFrame->ecode,1); while (*stack.currentFrame->ecode == OP_ALT);
                
                stack.currentFrame->offset_top = md->end_offset_top;
                stack.currentFrame->eptr = md->end_match_ptr;
                
                /* For a non-repeating ket, just continue at this level. This also
                 happens for a repeating ket if no characters were matched in the group.
                 This is the forcible breaking of infinite loops as implemented in Perl
                 5.005. If there is an options reset, it will get obeyed in the normal
                 course of events. */
                
                if (*stack.currentFrame->ecode == OP_KET || stack.currentFrame->eptr == stack.currentFrame->saved_eptr) {
                    stack.currentFrame->ecode += 1+LINK_SIZE;
                    NEXT_OPCODE;
                }
                
                /* The repeating kets try the rest of the pattern or restart from the
                 preceding bracket, in the appropriate order. We need to reset any options
                 that changed within the bracket before re-running it, so check the next
                 opcode. */
                
                if (*stack.currentFrame->ecode == OP_KETRMIN) {
                    RMATCH(10, stack.currentFrame->ecode + 1 + LINK_SIZE, stack.currentFrame->eptrb, 0);
                    if (is_match)
                        RRETURN;
                    RMATCH(11, stack.currentFrame->prev, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                } else { /* OP_KETRMAX */
                    RMATCH(12, stack.currentFrame->prev, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                    RMATCH(13, stack.currentFrame->ecode + 1+LINK_SIZE, stack.currentFrame->eptrb, 0);
                    if (is_match)
                        RRETURN;
                }
                RRETURN;
                
                /* An alternation is the end of a branch; scan along to find the end of the
                 bracketed group and go to there. */
                
                BEGIN_OPCODE(ALT):
                do stack.currentFrame->ecode += GET(stack.currentFrame->ecode,1); while (*stack.currentFrame->ecode == OP_ALT);
                NEXT_OPCODE;
                
                /* BRAZERO and BRAMINZERO occur just before a bracket group, indicating
                 that it may occur zero times. It may repeat infinitely, or not at all -
                 i.e. it could be ()* or ()? in the pattern. Brackets with fixed upper
                 repeat limits are compiled as a number of copies, with the optional ones
                 preceded by BRAZERO or BRAMINZERO. */
                
                BEGIN_OPCODE(BRAZERO):
                {
                    stack.currentFrame->next = stack.currentFrame->ecode+1;
                    RMATCH(14, stack.currentFrame->next, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                    do stack.currentFrame->next += GET(stack.currentFrame->next,1); while (*stack.currentFrame->next == OP_ALT);
                    stack.currentFrame->ecode = stack.currentFrame->next + 1+LINK_SIZE;
                }
                NEXT_OPCODE;
                
                BEGIN_OPCODE(BRAMINZERO):
                {
                    stack.currentFrame->next = stack.currentFrame->ecode+1;
                    do stack.currentFrame->next += GET(stack.currentFrame->next,1); while (*stack.currentFrame->next == OP_ALT);
                    RMATCH(15, stack.currentFrame->next + 1+LINK_SIZE, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                    stack.currentFrame->ecode++;
                }
                NEXT_OPCODE;
                
                /* End of a group, repeated or non-repeating. If we are at the end of
                 an assertion "group", stop matching and return MATCH_MATCH, but record the
                 current high water mark for use by positive assertions. Do this also
                 for the "once" (not-backup up) groups. */
                
                BEGIN_OPCODE(KET):
                BEGIN_OPCODE(KETRMIN):
                BEGIN_OPCODE(KETRMAX):
                stack.currentFrame->prev = stack.currentFrame->ecode - GET(stack.currentFrame->ecode, 1);
                stack.currentFrame->saved_eptr = stack.currentFrame->eptrb->epb_saved_eptr;
                
                /* Back up the stack of bracket start pointers. */
                
                stack.currentFrame->eptrb = stack.currentFrame->eptrb->epb_prev;
                
                if (*stack.currentFrame->prev == OP_ASSERT || *stack.currentFrame->prev == OP_ASSERT_NOT || *stack.currentFrame->prev == OP_ONCE) {
                    md->end_match_ptr = stack.currentFrame->eptr;      /* For ONCE */
                    md->end_offset_top = stack.currentFrame->offset_top;
                    is_match = true;
                    RRETURN;
                }
                
                /* In all other cases except a conditional group we have to check the
                 group number back at the start and if necessary complete handling an
                 extraction by setting the offsets and bumping the high water mark. */
                
                stack.currentFrame->number = *stack.currentFrame->prev - OP_BRA;
                
                /* For extended extraction brackets (large number), we have to fish out
                 the number from a dummy opcode at the start. */
                
                if (stack.currentFrame->number > EXTRACT_BASIC_MAX)
                    stack.currentFrame->number = GET2(stack.currentFrame->prev, 2+LINK_SIZE);
                stack.currentFrame->offset = stack.currentFrame->number << 1;
                
#ifdef DEBUG
                printf("end bracket %d", stack.currentFrame->number);
                printf("\n");
#endif
                
                /* Test for a numbered group. This includes groups called as a result
                 of recursion. Note that whole-pattern recursion is coded as a recurse
                 into group 0, so it won't be picked up here. Instead, we catch it when
                 the OP_END is reached. */
                
                if (stack.currentFrame->number > 0) {
                    if (stack.currentFrame->offset >= md->offset_max)
                        md->offset_overflow = true;
                    else {
                        md->offset_vector[stack.currentFrame->offset] =
                        md->offset_vector[md->offset_end - stack.currentFrame->number];
                        md->offset_vector[stack.currentFrame->offset+1] = stack.currentFrame->eptr - md->start_subject;
                        if (stack.currentFrame->offset_top <= stack.currentFrame->offset)
                            stack.currentFrame->offset_top = stack.currentFrame->offset + 2;
                    }
                }
                
                /* For a non-repeating ket, just continue at this level. This also
                 happens for a repeating ket if no characters were matched in the group.
                 This is the forcible breaking of infinite loops as implemented in Perl
                 5.005. If there is an options reset, it will get obeyed in the normal
                 course of events. */
                
                if (*stack.currentFrame->ecode == OP_KET || stack.currentFrame->eptr == stack.currentFrame->saved_eptr) {
                    stack.currentFrame->ecode += 1 + LINK_SIZE;
                    NEXT_OPCODE;
                }
                
                /* The repeating kets try the rest of the pattern or restart from the
                 preceding bracket, in the appropriate order. */
                
                if (*stack.currentFrame->ecode == OP_KETRMIN) {
                    RMATCH(16, stack.currentFrame->ecode + 1+LINK_SIZE, stack.currentFrame->eptrb, 0);
                    if (is_match)
                        RRETURN;
                    RMATCH(17, stack.currentFrame->prev, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                } else { /* OP_KETRMAX */
                    RMATCH(18, stack.currentFrame->prev, stack.currentFrame->eptrb, match_isgroup);
                    if (is_match)
                        RRETURN;
                    RMATCH(19, stack.currentFrame->ecode + 1+LINK_SIZE, stack.currentFrame->eptrb, 0);
                    if (is_match)
                        RRETURN;
                }
                RRETURN;
                
                /* Start of subject, or after internal newline if multiline. */
                
                BEGIN_OPCODE(CIRC):
                if (stack.currentFrame->eptr != md->start_subject && (!md->multiline || !isNewline(stack.currentFrame->eptr[-1])))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                /* End of subject, or before internal newline if multiline. */
                
                BEGIN_OPCODE(DOLL):
                if (stack.currentFrame->eptr < md->end_subject && (!md->multiline || !isNewline(*stack.currentFrame->eptr)))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                /* Word boundary assertions */
                
                BEGIN_OPCODE(NOT_WORD_BOUNDARY):
                BEGIN_OPCODE(WORD_BOUNDARY):
                /* Find out if the previous and current characters are "word" characters.
                 It takes a bit more work in UTF-8 mode. Characters > 128 are assumed to
                 be "non-word" characters. */
                
                if (stack.currentFrame->eptr == md->start_subject)
                    prev_is_word = false;
                else {
                    const UChar* lastptr = stack.currentFrame->eptr - 1;
                    while(isTrailingSurrogate(*lastptr))
                        lastptr--;
                    getChar(c, lastptr);
                    prev_is_word = c < 128 && (md->ctypes[c] & ctype_word) != 0;
                }
                if (stack.currentFrame->eptr >= md->end_subject)
                    cur_is_word = false;
                else {
                    getChar(c, stack.currentFrame->eptr);
                    cur_is_word = c < 128 && (md->ctypes[c] & ctype_word) != 0;
                }
                
                /* Now see if the situation is what we want */
                
                if ((*stack.currentFrame->ecode++ == OP_WORD_BOUNDARY) ? cur_is_word == prev_is_word : cur_is_word != prev_is_word)
                    RRETURN_NO_MATCH;
                NEXT_OPCODE;
                
                /* Match a single character type; inline for speed */
                
                BEGIN_OPCODE(ANY):
                if (stack.currentFrame->eptr < md->end_subject && isNewline(*stack.currentFrame->eptr))
                    RRETURN_NO_MATCH;
                if (stack.currentFrame->eptr++ >= md->end_subject)
                    RRETURN_NO_MATCH;
                while (stack.currentFrame->eptr < md->end_subject && isTrailingSurrogate(*stack.currentFrame->eptr))
                    stack.currentFrame->eptr++;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                BEGIN_OPCODE(NOT_DIGIT):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (isASCIIDigit(c))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                BEGIN_OPCODE(DIGIT):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (!isASCIIDigit(c))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                BEGIN_OPCODE(NOT_WHITESPACE):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (c < 128 && (md->ctypes[c] & ctype_space))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                BEGIN_OPCODE(WHITESPACE):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (c >= 128 || !(md->ctypes[c] & ctype_space))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                BEGIN_OPCODE(NOT_WORDCHAR):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (c < 128 && (md->ctypes[c] & ctype_word))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                BEGIN_OPCODE(WORDCHAR):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (c >= 128 || !(md->ctypes[c] & ctype_word))
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                NEXT_OPCODE;
                
                /* Match a back reference, possibly repeatedly. Look past the end of the
                 item to see if there is repeat information following. The code is similar
                 to that for character classes, but repeated for efficiency. Then obey
                 similar code to character type repeats - written out again for speed.
                 However, if the referenced string is the empty string, always treat
                 it as matched, any number of times (otherwise there could be infinite
                 loops). */
                
                BEGIN_OPCODE(REF):
                stack.currentFrame->offset = GET2(stack.currentFrame->ecode, 1) << 1;               /* Doubled ref number */
                stack.currentFrame->ecode += 3;                                 /* Advance past item */
                
                /* If the reference is unset, set the length to be longer than the amount
                 of subject left; this ensures that every attempt at a match fails. We
                 can't just fail here, because of the possibility of quantifiers with zero
                 minima. */
                
                if (stack.currentFrame->offset >= stack.currentFrame->offset_top || md->offset_vector[stack.currentFrame->offset] < 0)
                    stack.currentFrame->length = 0;
                else
                    stack.currentFrame->length = md->offset_vector[stack.currentFrame->offset+1] - md->offset_vector[stack.currentFrame->offset];
                
                /* Set up for repetition, or handle the non-repeated case */
                
                switch (*stack.currentFrame->ecode) {
                case OP_CRSTAR:
                case OP_CRMINSTAR:
                case OP_CRPLUS:
                case OP_CRMINPLUS:
                case OP_CRQUERY:
                case OP_CRMINQUERY:
                    c = *stack.currentFrame->ecode++ - OP_CRSTAR;
                    minimize = (c & 1) != 0;
                    min = rep_min[c];                 /* Pick up values from tables; */
                    stack.currentFrame->max = rep_max[c];                 /* zero for max => infinity */
                    if (stack.currentFrame->max == 0)
                        stack.currentFrame->max = INT_MAX;
                    break;
                    
                case OP_CRRANGE:
                case OP_CRMINRANGE:
                    minimize = (*stack.currentFrame->ecode == OP_CRMINRANGE);
                    min = GET2(stack.currentFrame->ecode, 1);
                    stack.currentFrame->max = GET2(stack.currentFrame->ecode, 3);
                    if (stack.currentFrame->max == 0)
                        stack.currentFrame->max = INT_MAX;
                    stack.currentFrame->ecode += 5;
                    break;
                
                default:               /* No repeat follows */
                    if (!match_ref(stack.currentFrame->offset, stack.currentFrame->eptr, stack.currentFrame->length, md))
                        RRETURN_NO_MATCH;
                    stack.currentFrame->eptr += stack.currentFrame->length;
                    NEXT_OPCODE;
                }
                
                /* If the length of the reference is zero, just continue with the
                 main loop. */
                
                if (stack.currentFrame->length == 0)
                    NEXT_OPCODE;
                
                /* First, ensure the minimum number of matches are present. */
                
                for (i = 1; i <= min; i++) {
                    if (!match_ref(stack.currentFrame->offset, stack.currentFrame->eptr, stack.currentFrame->length, md))
                        RRETURN_NO_MATCH;
                    stack.currentFrame->eptr += stack.currentFrame->length;
                }
                
                /* If min = max, continue at the same level without recursion.
                 They are not both allowed to be zero. */
                
                if (min == stack.currentFrame->max)
                    NEXT_OPCODE;
                
                /* If minimizing, keep trying and advancing the pointer */
                
                if (minimize) {
                    for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                        RMATCH(20, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        if (stack.currentFrame->fi >= stack.currentFrame->max || !match_ref(stack.currentFrame->offset, stack.currentFrame->eptr, stack.currentFrame->length, md))
                            RRETURN;
                        stack.currentFrame->eptr += stack.currentFrame->length;
                    }
                    /* Control never reaches here */
                }
                
                /* If maximizing, find the longest string and work backwards */
                
                else {
                    stack.currentFrame->pp = stack.currentFrame->eptr;
                    for (i = min; i < stack.currentFrame->max; i++) {
                        if (!match_ref(stack.currentFrame->offset, stack.currentFrame->eptr, stack.currentFrame->length, md))
                            break;
                        stack.currentFrame->eptr += stack.currentFrame->length;
                    }
                    while (stack.currentFrame->eptr >= stack.currentFrame->pp) {
                        RMATCH(21, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        stack.currentFrame->eptr -= stack.currentFrame->length;
                    }
                    RRETURN_NO_MATCH;
                }
                /* Control never reaches here */
                
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
                stack.currentFrame->data = stack.currentFrame->ecode + 1;                /* Save for matching */
                stack.currentFrame->ecode += 33;                     /* Advance past the item */
                
                switch (*stack.currentFrame->ecode) {
                case OP_CRSTAR:
                case OP_CRMINSTAR:
                case OP_CRPLUS:
                case OP_CRMINPLUS:
                case OP_CRQUERY:
                case OP_CRMINQUERY:
                    c = *stack.currentFrame->ecode++ - OP_CRSTAR;
                    minimize = (c & 1) != 0;
                    min = rep_min[c];                 /* Pick up values from tables; */
                    stack.currentFrame->max = rep_max[c];                 /* zero for max => infinity */
                    if (stack.currentFrame->max == 0)
                        stack.currentFrame->max = INT_MAX;
                    break;
                    
                case OP_CRRANGE:
                case OP_CRMINRANGE:
                    minimize = (*stack.currentFrame->ecode == OP_CRMINRANGE);
                    min = GET2(stack.currentFrame->ecode, 1);
                    stack.currentFrame->max = GET2(stack.currentFrame->ecode, 3);
                    if (stack.currentFrame->max == 0)
                        stack.currentFrame->max = INT_MAX;
                    stack.currentFrame->ecode += 5;
                    break;
                    
                default:               /* No repeat follows */
                    min = stack.currentFrame->max = 1;
                    break;
                }
                
                /* First, ensure the minimum number of matches are present. */
                
                for (i = 1; i <= min; i++) {
                    if (stack.currentFrame->eptr >= md->end_subject)
                        RRETURN_NO_MATCH;
                    GETCHARINC(c, stack.currentFrame->eptr);
                    if (c > 255) {
                        if (stack.currentFrame->data[-1] == OP_CLASS)
                            RRETURN_NO_MATCH;
                    } else {
                        if ((stack.currentFrame->data[c/8] & (1 << (c&7))) == 0)
                            RRETURN_NO_MATCH;
                    }
                }
                
                /* If max == min we can continue with the main loop without the
                 need to recurse. */
                
                if (min == stack.currentFrame->max)
                    NEXT_OPCODE;      
                
                /* If minimizing, keep testing the rest of the expression and advancing
                 the pointer while it matches the class. */
                if (minimize) {
                    {
                        for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                            RMATCH(22, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject)
                                RRETURN;
                            GETCHARINC(c, stack.currentFrame->eptr);
                            if (c > 255) {
                                if (stack.currentFrame->data[-1] == OP_CLASS)
                                    RRETURN;
                            } else {
                                if ((stack.currentFrame->data[c/8] & (1 << (c&7))) == 0)
                                    RRETURN;
                            }
                        }
                    }
                    /* Control never reaches here */
                }
                /* If maximizing, find the longest possible run, then work backwards. */
                else {
                    stack.currentFrame->pp = stack.currentFrame->eptr;
                    
                    for (i = min; i < stack.currentFrame->max; i++) {
                        int len = 1;
                        if (stack.currentFrame->eptr >= md->end_subject)
                            break;
                        GETCHARLEN(c, stack.currentFrame->eptr, len);
                        if (c > 255) {
                            if (stack.currentFrame->data[-1] == OP_CLASS)
                                break;
                        } else {
                            if ((stack.currentFrame->data[c/8] & (1 << (c&7))) == 0)
                                break;
                        }
                        stack.currentFrame->eptr += len;
                    }
                    for (;;) {
                        RMATCH(24, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        if (stack.currentFrame->eptr-- == stack.currentFrame->pp)
                            break;        /* Stop if tried at original pos */
                        BACKCHAR(stack.currentFrame->eptr);
                    }
                    
                    RRETURN;
                }
                /* Control never reaches here */
                
                /* Match an extended character class. This opcode is encountered only
                 in UTF-8 mode, because that's the only time it is compiled. */
                
                BEGIN_OPCODE(XCLASS):
                stack.currentFrame->data = stack.currentFrame->ecode + 1 + LINK_SIZE;                /* Save for matching */
                stack.currentFrame->ecode += GET(stack.currentFrame->ecode, 1);                      /* Advance past the item */
                
                switch (*stack.currentFrame->ecode) {
                case OP_CRSTAR:
                case OP_CRMINSTAR:
                case OP_CRPLUS:
                case OP_CRMINPLUS:
                case OP_CRQUERY:
                case OP_CRMINQUERY:
                    c = *stack.currentFrame->ecode++ - OP_CRSTAR;
                    minimize = (c & 1) != 0;
                    min = rep_min[c];                 /* Pick up values from tables; */
                    stack.currentFrame->max = rep_max[c];                 /* zero for max => infinity */
                    if (stack.currentFrame->max == 0)
                        stack.currentFrame->max = INT_MAX;
                    break;
                    
                case OP_CRRANGE:
                case OP_CRMINRANGE:
                    minimize = (*stack.currentFrame->ecode == OP_CRMINRANGE);
                    min = GET2(stack.currentFrame->ecode, 1);
                    stack.currentFrame->max = GET2(stack.currentFrame->ecode, 3);
                    if (stack.currentFrame->max == 0)
                        stack.currentFrame->max = INT_MAX;
                    stack.currentFrame->ecode += 5;
                    break;
                    
                default:               /* No repeat follows */
                    min = stack.currentFrame->max = 1;
            }
                
                /* First, ensure the minimum number of matches are present. */
                
                for (i = 1; i <= min; i++) {
                    if (stack.currentFrame->eptr >= md->end_subject)
                        RRETURN_NO_MATCH;
                    GETCHARINC(c, stack.currentFrame->eptr);
                    if (!_pcre_xclass(c, stack.currentFrame->data))
                        RRETURN_NO_MATCH;
                }
                
                /* If max == min we can continue with the main loop without the
                 need to recurse. */
                
                if (min == stack.currentFrame->max)
                    NEXT_OPCODE;
                
                /* If minimizing, keep testing the rest of the expression and advancing
                 the pointer while it matches the class. */
                
                if (minimize) {
                    for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                        RMATCH(26, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject)
                            RRETURN;
                        GETCHARINC(c, stack.currentFrame->eptr);
                        if (!_pcre_xclass(c, stack.currentFrame->data))
                            RRETURN;
                    }
                    /* Control never reaches here */
                }
                
                /* If maximizing, find the longest possible run, then work backwards. */
                
                else {
                    stack.currentFrame->pp = stack.currentFrame->eptr;
                    for (i = min; i < stack.currentFrame->max; i++) {
                        int len = 1;
                        if (stack.currentFrame->eptr >= md->end_subject)
                            break;
                        GETCHARLEN(c, stack.currentFrame->eptr, len);
                        if (!_pcre_xclass(c, stack.currentFrame->data))
                            break;
                        stack.currentFrame->eptr += len;
                    }
                    for(;;) {
                        RMATCH(27, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        if (stack.currentFrame->eptr-- == stack.currentFrame->pp)
                            break;        /* Stop if tried at original pos */
                        BACKCHAR(stack.currentFrame->eptr)
                    }
                    RRETURN;
                }
                
                /* Control never reaches here */
                
                /* Match a single character, casefully */
                
                BEGIN_OPCODE(CHAR):
                stack.currentFrame->length = 1;
                stack.currentFrame->ecode++;
                getUTF8CharAndIncrementLength(stack.currentFrame->fc, stack.currentFrame->ecode, stack.currentFrame->length);
            {
                int dc;
                stack.currentFrame->ecode += stack.currentFrame->length;
                switch (md->end_subject - stack.currentFrame->eptr) {
                case 0:
                    RRETURN_NO_MATCH;
                case 1:
                    dc = *stack.currentFrame->eptr++;
                    if (isLeadingSurrogate(dc))
                        RRETURN_NO_MATCH;
                    break;
                    default:
                    GETCHARINC(dc, stack.currentFrame->eptr);
                }
                if (stack.currentFrame->fc != dc)
                    RRETURN_NO_MATCH;
            }
                NEXT_OPCODE;
                
                /* Match a single character, caselessly */
                
                BEGIN_OPCODE(CHARNC):
                stack.currentFrame->length = 1;
                stack.currentFrame->ecode++;
                getUTF8CharAndIncrementLength(stack.currentFrame->fc, stack.currentFrame->ecode, stack.currentFrame->length);
                
                if (md->end_subject - stack.currentFrame->eptr == 0)
                    RRETURN_NO_MATCH;
                
            {
                int dc;
                if (md->end_subject - stack.currentFrame->eptr == 1) {
                    dc = *stack.currentFrame->eptr++;
                    if (isLeadingSurrogate(dc))
                        RRETURN_NO_MATCH;
                } else
                    GETCHARINC(dc, stack.currentFrame->eptr);
                stack.currentFrame->ecode += stack.currentFrame->length;
                
                /* If we have Unicode property support, we can use it to test the other
                 case of the character, if there is one. */
                
                if (stack.currentFrame->fc != dc) {
                    if (dc != _pcre_ucp_othercase(stack.currentFrame->fc))
                        RRETURN_NO_MATCH;
                }
            }
                NEXT_OPCODE;
                
                /* Match a single ASCII character. */
                
                BEGIN_OPCODE(ASCII_CHAR):
                if (md->end_subject == stack.currentFrame->eptr)
                    RRETURN_NO_MATCH;
                if (*stack.currentFrame->eptr != stack.currentFrame->ecode[1])
                    RRETURN_NO_MATCH;
                ++stack.currentFrame->eptr;
                stack.currentFrame->ecode += 2;
                NEXT_OPCODE;
                
                /* Match one of two cases of an ASCII character. */
                
                BEGIN_OPCODE(ASCII_LETTER_NC):
                if (md->end_subject == stack.currentFrame->eptr)
                    RRETURN_NO_MATCH;
                if ((*stack.currentFrame->eptr | 0x20) != stack.currentFrame->ecode[1])
                    RRETURN_NO_MATCH;
                ++stack.currentFrame->eptr;
                stack.currentFrame->ecode += 2;
                NEXT_OPCODE;
                
                /* Match a single character repeatedly; different opcodes share code. */
                
                BEGIN_OPCODE(EXACT):
                min = stack.currentFrame->max = GET2(stack.currentFrame->ecode, 1);
                minimize = false;
                stack.currentFrame->ecode += 3;
                goto REPEATCHAR;
                
                BEGIN_OPCODE(UPTO):
                BEGIN_OPCODE(MINUPTO):
                min = 0;
                stack.currentFrame->max = GET2(stack.currentFrame->ecode, 1);
                minimize = *stack.currentFrame->ecode == OP_MINUPTO;
                stack.currentFrame->ecode += 3;
                goto REPEATCHAR;
                
                BEGIN_OPCODE(STAR):
                BEGIN_OPCODE(MINSTAR):
                BEGIN_OPCODE(PLUS):
                BEGIN_OPCODE(MINPLUS):
                BEGIN_OPCODE(QUERY):
                BEGIN_OPCODE(MINQUERY):
                c = *stack.currentFrame->ecode++ - OP_STAR;
                minimize = (c & 1) != 0;
                min = rep_min[c];                 /* Pick up values from tables; */
                stack.currentFrame->max = rep_max[c];                 /* zero for max => infinity */
                if (stack.currentFrame->max == 0)
                    stack.currentFrame->max = INT_MAX;
                
                /* Common code for all repeated single-character matches. We can give
                 up quickly if there are fewer than the minimum number of characters left in
                 the subject. */
                
            REPEATCHAR:
                
                stack.currentFrame->length = 1;
                getUTF8CharAndIncrementLength(stack.currentFrame->fc, stack.currentFrame->ecode, stack.currentFrame->length);
                if (min * (stack.currentFrame->fc > 0xFFFF ? 2 : 1) > md->end_subject - stack.currentFrame->eptr)
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode += stack.currentFrame->length;
                
                if (stack.currentFrame->fc <= 0xFFFF) {
                    int othercase = md->caseless ? _pcre_ucp_othercase(stack.currentFrame->fc) : -1;
                    
                    for (i = 1; i <= min; i++) {
                        if (*stack.currentFrame->eptr != stack.currentFrame->fc && *stack.currentFrame->eptr != othercase)
                            RRETURN_NO_MATCH;
                        ++stack.currentFrame->eptr;
                    }
                    
                    if (min == stack.currentFrame->max)
                        NEXT_OPCODE;
                    
                    if (minimize) {
                        stack.currentFrame->repeat_othercase = othercase;
                        for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                            RMATCH(28, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject)
                                RRETURN;
                            if (*stack.currentFrame->eptr != stack.currentFrame->fc && *stack.currentFrame->eptr != stack.currentFrame->repeat_othercase)
                                RRETURN;
                            ++stack.currentFrame->eptr;
                        }
                        /* Control never reaches here */
                    } else {
                        stack.currentFrame->pp = stack.currentFrame->eptr;
                        for (i = min; i < stack.currentFrame->max; i++) {
                            if (stack.currentFrame->eptr >= md->end_subject)
                                break;
                            if (*stack.currentFrame->eptr != stack.currentFrame->fc && *stack.currentFrame->eptr != othercase)
                                break;
                            ++stack.currentFrame->eptr;
                        }
                        while (stack.currentFrame->eptr >= stack.currentFrame->pp) {
                            RMATCH(29, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            --stack.currentFrame->eptr;
                        }
                        RRETURN_NO_MATCH;
                    }
                    /* Control never reaches here */
                } else {
                    /* No case on surrogate pairs, so no need to bother with "othercase". */
                    
                    for (i = 1; i <= min; i++) {
                        int nc;
                        getChar(nc, stack.currentFrame->eptr);
                        if (nc != stack.currentFrame->fc)
                            RRETURN_NO_MATCH;
                        stack.currentFrame->eptr += 2;
                    }
                    
                    if (min == stack.currentFrame->max)
                        NEXT_OPCODE;
                    
                    if (minimize) {
                        for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                            int nc;
                            RMATCH(30, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject)
                                RRETURN;
                            getChar(nc, stack.currentFrame->eptr);
                            if (*stack.currentFrame->eptr != stack.currentFrame->fc)
                                RRETURN;
                            stack.currentFrame->eptr += 2;
                        }
                        /* Control never reaches here */
                    } else {
                        stack.currentFrame->pp = stack.currentFrame->eptr;
                        for (i = min; i < stack.currentFrame->max; i++) {
                            int nc;
                            if (stack.currentFrame->eptr > md->end_subject - 2)
                                break;
                            getChar(nc, stack.currentFrame->eptr);
                            if (*stack.currentFrame->eptr != stack.currentFrame->fc)
                                break;
                            stack.currentFrame->eptr += 2;
                        }
                        while (stack.currentFrame->eptr >= stack.currentFrame->pp) {
                            RMATCH(31, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            stack.currentFrame->eptr -= 2;
                        }
                        RRETURN_NO_MATCH;
                    }
                    /* Control never reaches here */
                }
                /* Control never reaches here */
                
                /* Match a negated single one-byte character. The character we are
                 checking can be multibyte. */
                
                BEGIN_OPCODE(NOT):
                if (stack.currentFrame->eptr >= md->end_subject)
                    RRETURN_NO_MATCH;
                stack.currentFrame->ecode++;
                GETCHARINCTEST(c, stack.currentFrame->eptr);
                if (md->caseless) {
                    if (c < 128)
                        c = md->lcc[c];
                    if (md->lcc[*stack.currentFrame->ecode++] == c)
                        RRETURN_NO_MATCH;
                } else {
                    if (*stack.currentFrame->ecode++ == c)
                        RRETURN_NO_MATCH;
                }
                NEXT_OPCODE;
                
                /* Match a negated single one-byte character repeatedly. This is almost a
                 repeat of the code for a repeated single character, but I haven't found a
                 nice way of commoning these up that doesn't require a test of the
                 positive/negative option for each character match. Maybe that wouldn't add
                 very much to the time taken, but character matching *is* what this is all
                 about... */
                
                BEGIN_OPCODE(NOTEXACT):
                min = stack.currentFrame->max = GET2(stack.currentFrame->ecode, 1);
                minimize = false;
                stack.currentFrame->ecode += 3;
                goto REPEATNOTCHAR;
                
                BEGIN_OPCODE(NOTUPTO):
                BEGIN_OPCODE(NOTMINUPTO):
                min = 0;
                stack.currentFrame->max = GET2(stack.currentFrame->ecode, 1);
                minimize = *stack.currentFrame->ecode == OP_NOTMINUPTO;
                stack.currentFrame->ecode += 3;
                goto REPEATNOTCHAR;
                
                BEGIN_OPCODE(NOTSTAR):
                BEGIN_OPCODE(NOTMINSTAR):
                BEGIN_OPCODE(NOTPLUS):
                BEGIN_OPCODE(NOTMINPLUS):
                BEGIN_OPCODE(NOTQUERY):
                BEGIN_OPCODE(NOTMINQUERY):
                c = *stack.currentFrame->ecode++ - OP_NOTSTAR;
                minimize = (c & 1) != 0;
                min = rep_min[c];                 /* Pick up values from tables; */
                stack.currentFrame->max = rep_max[c];                 /* zero for max => infinity */
                if (stack.currentFrame->max == 0) stack.currentFrame->max = INT_MAX;
                
                /* Common code for all repeated single-byte matches. We can give up quickly
                 if there are fewer than the minimum number of bytes left in the
                 subject. */
                
            REPEATNOTCHAR:
                if (min > md->end_subject - stack.currentFrame->eptr)
                    RRETURN_NO_MATCH;
                stack.currentFrame->fc = *stack.currentFrame->ecode++;
                
                /* The code is duplicated for the caseless and caseful cases, for speed,
                 since matching characters is likely to be quite common. First, ensure the
                 minimum number of matches are present. If min = max, continue at the same
                 level without recursing. Otherwise, if minimizing, keep trying the rest of
                 the expression and advancing one matching character if failing, up to the
                 maximum. Alternatively, if maximizing, find the maximum number of
                 characters and work backwards. */
                
                DPRINTF(("negative matching %c{%d,%d}\n", stack.currentFrame->fc, min, stack.currentFrame->max));
                
                if (md->caseless) {
                    if (stack.currentFrame->fc < 128)
                        stack.currentFrame->fc = md->lcc[stack.currentFrame->fc];
                    
                    {
                        int d;
                        for (i = 1; i <= min; i++) {
                            GETCHARINC(d, stack.currentFrame->eptr);
                            if (d < 128)
                                d = md->lcc[d];
                            if (stack.currentFrame->fc == d)
                                RRETURN_NO_MATCH;
                        }
                    }
                    
                    if (min == stack.currentFrame->max)
                        NEXT_OPCODE;      
                    
                    if (minimize) {
                        int d;
                        for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                            RMATCH(38, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            GETCHARINC(d, stack.currentFrame->eptr);
                            if (d < 128)
                                d = md->lcc[d];
                            if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject || stack.currentFrame->fc == d)
                                RRETURN;
                        }
                        /* Control never reaches here */
                    }
                    
                    /* Maximize case */
                    
                    else {
                        stack.currentFrame->pp = stack.currentFrame->eptr;
                        
                        {
                            int d;
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(d, stack.currentFrame->eptr, len);
                                if (d < 128)
                                    d = md->lcc[d];
                                if (stack.currentFrame->fc == d)
                                    break;
                                stack.currentFrame->eptr += len;
                            }
                            for (;;) {
                                RMATCH(40, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                                if (is_match)
                                    RRETURN;
                                if (stack.currentFrame->eptr-- == stack.currentFrame->pp)
                                    break;        /* Stop if tried at original pos */
                                BACKCHAR(stack.currentFrame->eptr);
                            }
                        }
                        
                        RRETURN;
                    }
                    /* Control never reaches here */
                }
                
                /* Caseful comparisons */
                
                else {
                    {
                        int d;
                        for (i = 1; i <= min; i++) {
                            GETCHARINC(d, stack.currentFrame->eptr);
                            if (stack.currentFrame->fc == d)
                                RRETURN_NO_MATCH;
                        }
                    }
                    
                    if (min == stack.currentFrame->max)
                        NEXT_OPCODE;
                    
                    if (minimize) {
                        int d;
                        for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                            RMATCH(42, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                            if (is_match)
                                RRETURN;
                            GETCHARINC(d, stack.currentFrame->eptr);
                            if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject || stack.currentFrame->fc == d)
                                RRETURN;
                        }
                        /* Control never reaches here */
                    }
                    
                    /* Maximize case */
                    
                    else {
                        stack.currentFrame->pp = stack.currentFrame->eptr;
                        
                        {
                            int d;
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(d, stack.currentFrame->eptr, len);
                                if (stack.currentFrame->fc == d)
                                    break;
                                stack.currentFrame->eptr += len;
                            }
                            for (;;) {
                                RMATCH(44, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                                if (is_match)
                                    RRETURN;
                                if (stack.currentFrame->eptr-- == stack.currentFrame->pp)
                                    break;        /* Stop if tried at original pos */
                                BACKCHAR(stack.currentFrame->eptr);
                            }
                        }
                        
                        RRETURN;
                    }
                }
                /* Control never reaches here */
                
                /* Match a single character type repeatedly; several different opcodes
                 share code. This is very similar to the code for single characters, but we
                 repeat it in the interests of efficiency. */
                
                BEGIN_OPCODE(TYPEEXACT):
                min = stack.currentFrame->max = GET2(stack.currentFrame->ecode, 1);
                minimize = true;
                stack.currentFrame->ecode += 3;
                goto REPEATTYPE;
                
                BEGIN_OPCODE(TYPEUPTO):
                BEGIN_OPCODE(TYPEMINUPTO):
                min = 0;
                stack.currentFrame->max = GET2(stack.currentFrame->ecode, 1);
                minimize = *stack.currentFrame->ecode == OP_TYPEMINUPTO;
                stack.currentFrame->ecode += 3;
                goto REPEATTYPE;
                
                BEGIN_OPCODE(TYPESTAR):
                BEGIN_OPCODE(TYPEMINSTAR):
                BEGIN_OPCODE(TYPEPLUS):
                BEGIN_OPCODE(TYPEMINPLUS):
                BEGIN_OPCODE(TYPEQUERY):
                BEGIN_OPCODE(TYPEMINQUERY):
                c = *stack.currentFrame->ecode++ - OP_TYPESTAR;
                minimize = (c & 1) != 0;
                min = rep_min[c];                 /* Pick up values from tables; */
                stack.currentFrame->max = rep_max[c];                 /* zero for max => infinity */
                if (stack.currentFrame->max == 0)
                    stack.currentFrame->max = INT_MAX;
                
                /* Common code for all repeated single character type matches. Note that
                 in UTF-8 mode, '.' matches a character of any length, but for the other
                 character types, the valid characters are all one-byte long. */
                
            REPEATTYPE:
                stack.currentFrame->ctype = *stack.currentFrame->ecode++;      /* Code for the character type */
                
                /* First, ensure the minimum number of matches are present. Use inline
                 code for maximizing the speed, and do the type test once at the start
                 (i.e. keep it out of the loop). Also we can test that there are at least
                 the minimum number of bytes before we start. This isn't as effective in
                 UTF-8 mode, but it does no harm. Separate the UTF-8 code completely as that
                 is tidier. Also separate the UCP code, which can be the same for both UTF-8
                 and single-bytes. */
                
                if (min > md->end_subject - stack.currentFrame->eptr)
                    RRETURN_NO_MATCH;
                if (min > 0) {
                    switch(stack.currentFrame->ctype) {
                        case OP_ANY:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject || isNewline(*stack.currentFrame->eptr))
                                    RRETURN_NO_MATCH;
                                ++stack.currentFrame->eptr;
                                while (stack.currentFrame->eptr < md->end_subject && isTrailingSurrogate(*stack.currentFrame->eptr))
                                    stack.currentFrame->eptr++;
                            }
                            break;
                            
                            case OP_NOT_DIGIT:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    RRETURN_NO_MATCH;
                                GETCHARINC(c, stack.currentFrame->eptr);
                                if (isASCIIDigit(c))
                                    RRETURN_NO_MATCH;
                            }
                            break;
                            
                            case OP_DIGIT:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject || !isASCIIDigit(*stack.currentFrame->eptr++))
                                    RRETURN_NO_MATCH;
                                /* No need to skip more bytes - we know it's a 1-byte character */
                            }
                            break;
                            
                            case OP_NOT_WHITESPACE:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject ||
                                    (*stack.currentFrame->eptr < 128 && (md->ctypes[*stack.currentFrame->eptr] & ctype_space) != 0))
                                    RRETURN_NO_MATCH;
                                while (++stack.currentFrame->eptr < md->end_subject && isTrailingSurrogate(*stack.currentFrame->eptr)) { }
                            }
                            break;
                            
                            case OP_WHITESPACE:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject ||
                                    *stack.currentFrame->eptr >= 128 || (md->ctypes[*stack.currentFrame->eptr++] & ctype_space) == 0)
                                    RRETURN_NO_MATCH;
                                /* No need to skip more bytes - we know it's a 1-byte character */
                            }
                            break;
                            
                            case OP_NOT_WORDCHAR:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject ||
                                    (*stack.currentFrame->eptr < 128 && (md->ctypes[*stack.currentFrame->eptr] & ctype_word) != 0))
                                    RRETURN_NO_MATCH;
                                while (++stack.currentFrame->eptr < md->end_subject && isTrailingSurrogate(*stack.currentFrame->eptr)) { }
                            }
                            break;
                            
                            case OP_WORDCHAR:
                            for (i = 1; i <= min; i++) {
                                if (stack.currentFrame->eptr >= md->end_subject ||
                                    *stack.currentFrame->eptr >= 128 || (md->ctypes[*stack.currentFrame->eptr++] & ctype_word) == 0)
                                    RRETURN_NO_MATCH;
                                /* No need to skip more bytes - we know it's a 1-byte character */
                            }
                            break;
                            
                            default:
                            ASSERT_NOT_REACHED();
                            return matchError(JSRegExpErrorInternal, stack);
                    }  /* End switch(stack.currentFrame->ctype) */
                }
                
                /* If min = max, continue at the same level without recursing */
                
                if (min == stack.currentFrame->max)
                    NEXT_OPCODE;    
                
                /* If minimizing, we have to test the rest of the pattern before each
                 subsequent match. */
                
                if (minimize) {
                    for (stack.currentFrame->fi = min;; stack.currentFrame->fi++) {
                        RMATCH(48, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        if (stack.currentFrame->fi >= stack.currentFrame->max || stack.currentFrame->eptr >= md->end_subject)
                            RRETURN;
                        
                        GETCHARINC(c, stack.currentFrame->eptr);
                        switch(stack.currentFrame->ctype) {
                        case OP_ANY:
                            if (isNewline(c))
                                RRETURN;
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
                            if (c < 128 && (md->ctypes[c] & ctype_space))
                                RRETURN;
                            break;
                            
                        case OP_WHITESPACE:
                            if  (c >= 128 || !(md->ctypes[c] & ctype_space))
                                RRETURN;
                            break;
                            
                        case OP_NOT_WORDCHAR:
                            if (c < 128 && (md->ctypes[c] & ctype_word))
                                RRETURN;
                            break;
                            
                        case OP_WORDCHAR:
                            if (c >= 128 || !(md->ctypes[c] & ctype_word))
                                RRETURN;
                            break;
                            
                        default:
                            ASSERT_NOT_REACHED();
                            return matchError(JSRegExpErrorInternal, stack);
                        }
                    }
                    /* Control never reaches here */
                }
                
                /* If maximizing it is worth using inline code for speed, doing the type
                 test once at the start (i.e. keep it out of the loop). */
                
                else {
                    stack.currentFrame->pp = stack.currentFrame->eptr;  /* Remember where we started */
                    
                    switch(stack.currentFrame->ctype) {
                        case OP_ANY:
                            
                            /* Special code is required for UTF8, but when the maximum is unlimited
                             we don't need it, so we repeat the non-UTF8 code. This is probably
                             worth it, because .* is quite a common idiom. */
                            
                            if (stack.currentFrame->max < INT_MAX) {
                                for (i = min; i < stack.currentFrame->max; i++) {
                                    if (stack.currentFrame->eptr >= md->end_subject || isNewline(*stack.currentFrame->eptr))
                                        break;
                                    stack.currentFrame->eptr++;
                                    while (stack.currentFrame->eptr < md->end_subject && (*stack.currentFrame->eptr & 0xc0) == 0x80)
                                        stack.currentFrame->eptr++;
                                }
                            }
                            
                            /* Handle unlimited UTF-8 repeat */
                            
                            else {
                                for (i = min; i < stack.currentFrame->max; i++) {
                                    if (stack.currentFrame->eptr >= md->end_subject || isNewline(*stack.currentFrame->eptr))
                                        break;
                                    stack.currentFrame->eptr++;
                                }
                                break;
                            }
                            break;
                            
                            case OP_NOT_DIGIT:
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(c, stack.currentFrame->eptr, len);
                                if (isASCIIDigit(c))
                                    break;
                                stack.currentFrame->eptr+= len;
                            }
                            break;
                            
                            case OP_DIGIT:
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(c, stack.currentFrame->eptr, len);
                                if (!isASCIIDigit(c))
                                    break;
                                stack.currentFrame->eptr+= len;
                            }
                            break;
                            
                            case OP_NOT_WHITESPACE:
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(c, stack.currentFrame->eptr, len);
                                if (c < 128 && (md->ctypes[c] & ctype_space))
                                    break;
                                stack.currentFrame->eptr+= len;
                            }
                            break;
                            
                            case OP_WHITESPACE:
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(c, stack.currentFrame->eptr, len);
                                if (c >= 128 || !(md->ctypes[c] & ctype_space))
                                    break;
                                stack.currentFrame->eptr+= len;
                            }
                            break;
                            
                            case OP_NOT_WORDCHAR:
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(c, stack.currentFrame->eptr, len);
                                if (c < 128 && (md->ctypes[c] & ctype_word))
                                    break;
                                stack.currentFrame->eptr+= len;
                            }
                            break;
                            
                            case OP_WORDCHAR:
                            for (i = min; i < stack.currentFrame->max; i++) {
                                int len = 1;
                                if (stack.currentFrame->eptr >= md->end_subject)
                                    break;
                                GETCHARLEN(c, stack.currentFrame->eptr, len);
                                if (c >= 128 || !(md->ctypes[c] & ctype_word))
                                    break;
                                stack.currentFrame->eptr+= len;
                            }
                            break;
                            
                            default:
                            ASSERT_NOT_REACHED();
                            return matchError(JSRegExpErrorInternal, stack);
                    }
                    
                    /* stack.currentFrame->eptr is now past the end of the maximum run */
                    
                    for (;;) {
                        RMATCH(52, stack.currentFrame->ecode, stack.currentFrame->eptrb, 0);
                        if (is_match)
                            RRETURN;
                        if (stack.currentFrame->eptr-- == stack.currentFrame->pp)
                            break;        /* Stop if tried at original pos */
                        BACKCHAR(stack.currentFrame->eptr);
                    }
                    
                    /* Get here if we can't make it match with any permitted repetitions */
                    
                    RRETURN;
                }
                /* Control never reaches here */
                
                BEGIN_OPCODE(CRMINPLUS):
                BEGIN_OPCODE(CRMINQUERY):
                BEGIN_OPCODE(CRMINRANGE):
                BEGIN_OPCODE(CRMINSTAR):
                BEGIN_OPCODE(CRPLUS):
                BEGIN_OPCODE(CRQUERY):
                BEGIN_OPCODE(CRRANGE):
                BEGIN_OPCODE(CRSTAR):
                ASSERT_NOT_REACHED();
                return matchError(JSRegExpErrorInternal, stack);
                
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
                
                ASSERT(*stack.currentFrame->ecode > OP_BRA);
                
                stack.currentFrame->number = *stack.currentFrame->ecode - OP_BRA;
                
                /* For extended extraction brackets (large number), we have to fish out the
                 number from a dummy opcode at the start. */
                
                if (stack.currentFrame->number > EXTRACT_BASIC_MAX)
                    stack.currentFrame->number = GET2(stack.currentFrame->ecode, 2+LINK_SIZE);
                stack.currentFrame->offset = stack.currentFrame->number << 1;
                
#ifdef DEBUG
                printf("start bracket %d subject=", stack.currentFrame->number);
                pchars(stack.currentFrame->eptr, 16, true, md);
                printf("\n");
#endif
                
                if (stack.currentFrame->offset < md->offset_max) {
                    stack.currentFrame->save_offset1 = md->offset_vector[stack.currentFrame->offset];
                    stack.currentFrame->save_offset2 = md->offset_vector[stack.currentFrame->offset + 1];
                    stack.currentFrame->save_offset3 = md->offset_vector[md->offset_end - stack.currentFrame->number];
                    
                    DPRINTF(("saving %d %d %d\n", stack.currentFrame->save_offset1, stack.currentFrame->save_offset2, stack.currentFrame->save_offset3));
                    md->offset_vector[md->offset_end - stack.currentFrame->number] = stack.currentFrame->eptr - md->start_subject;
                    
                    do {
                        RMATCH(1, stack.currentFrame->ecode + 1 + LINK_SIZE, stack.currentFrame->eptrb, match_isgroup);
                        if (is_match) RRETURN;
                        stack.currentFrame->ecode += GET(stack.currentFrame->ecode, 1);
                    } while (*stack.currentFrame->ecode == OP_ALT);
                    
                    DPRINTF(("bracket %d failed\n", stack.currentFrame->number));
                    
                    md->offset_vector[stack.currentFrame->offset] = stack.currentFrame->save_offset1;
                    md->offset_vector[stack.currentFrame->offset + 1] = stack.currentFrame->save_offset2;
                    md->offset_vector[md->offset_end - stack.currentFrame->number] = stack.currentFrame->save_offset3;
                    
                    RRETURN;
                }
                
                /* Insufficient room for saving captured contents */
                
                goto NON_CAPTURING_BRACKET;
        }
        
        /* Do not stick any code in here without much thought; it is assumed
         that "continue" in the code above comes out to here to repeat the main
         loop. */
        
    } /* End of main loop */
    
    ASSERT_NOT_REACHED();
    
#ifndef USE_COMPUTED_GOTO_FOR_MATCH_RECURSION
    
RRETURN_SWITCH:
    switch (stack.currentFrame->returnLocation)
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
    
    ASSERT_NOT_REACHED();
    return matchError(JSRegExpErrorInternal, stack);
    
#endif
    
RETURN:
    return is_match ? MATCH_MATCH : MATCH_NOMATCH;
}


/*************************************************
*         Execute a Regular Expression           *
*************************************************/

/* This function applies a compiled re to a subject string and picks out
portions of the string if it matches. Two elements in the vector are set for
each substring: the offsets to the start and end of the substring.

Arguments:
  re              points to the compiled expression
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

int jsRegExpExecute(const JSRegExp* re,
                    const UChar* subject, int length, int start_offset, int* offsets,
                    int offsetcount)
{
    ASSERT(re);
    ASSERT(subject);
    ASSERT(offsetcount >= 0);
    ASSERT(offsets || offsetcount == 0);
    
    match_data match_block;
    match_block.start_subject = (UChar*)subject;
    match_block.end_subject = match_block.start_subject + length;
    UChar* end_subject = match_block.end_subject;
    
    match_block.lcc = _pcre_default_tables + lcc_offset;
    match_block.ctypes = _pcre_default_tables + ctypes_offset;
    
    match_block.multiline = (re->options & PCRE_MULTILINE) != 0;
    match_block.caseless = (re->options & PCRE_CASELESS) != 0;
    
    /* If the expression has got more back references than the offsets supplied can
     hold, we get a temporary chunk of working store to use during the matching.
     Otherwise, we can use the vector supplied, rounding down its size to a multiple
     of 3. */
    
    int ocount = offsetcount - (offsetcount % 3);
    
    bool using_temporary_offsets = false;
    if (re->top_backref > 0 && re->top_backref >= ocount/3) {
        ocount = re->top_backref * 3 + 3;
        match_block.offset_vector = new int[ocount];
        if (!match_block.offset_vector)
            return JSRegExpErrorNoMemory;
        using_temporary_offsets = true;
    } else
        match_block.offset_vector = offsets;
    
    match_block.offset_end = ocount;
    match_block.offset_max = (2*ocount)/3;
    match_block.offset_overflow = false;
    
    /* Compute the minimum number of offsets that we need to reset each time. Doing
     this makes a huge difference to execution time when there aren't many brackets
     in the pattern. */
    
    int resetcount = 2 + re->top_bracket * 2;
    if (resetcount > offsetcount)
        resetcount = ocount;
    
    /* Reset the working variable associated with each extraction. These should
     never be used unless previously set, but they get saved and restored, and so we
     initialize them to avoid reading uninitialized locations. */
    
    if (match_block.offset_vector) {
        int* iptr = match_block.offset_vector + ocount;
        int* iend = iptr - resetcount/2 + 1;
        while (--iptr >= iend)
            *iptr = -1;
    }
    
    /* Set up the first character to match, if available. The first_byte value is
     never set for an anchored regular expression, but the anchoring may be forced
     at run time, so we have to test for anchoring. The first char may be unset for
     an unanchored pattern, of course. If there's no first char and the pattern was
     studied, there may be a bitmap of possible first characters. */
    
    bool first_byte_caseless = false;
    int first_byte = -1;
    if (re->options & PCRE_FIRSTSET) {
        first_byte = re->first_byte & 255;
        if ((first_byte_caseless = (re->first_byte & REQ_CASELESS)))
            first_byte = match_block.lcc[first_byte];
    }
    
    /* For anchored or unanchored matches, there may be a "last known required
     character" set. */
    
    bool req_byte_caseless = false;
    int req_byte = -1;
    int req_byte2 = -1;
    if (re->options & PCRE_REQCHSET) {
        req_byte = re->req_byte & 255;
        req_byte_caseless = (re->req_byte & REQ_CASELESS) != 0;
        req_byte2 = (_pcre_default_tables + fcc_offset)[req_byte];  /* case flipped */
    }
    
    /* Loop for handling unanchored repeated matching attempts; for anchored regexs
     the loop runs just once. */
    
    UChar* start_match = (UChar*)subject + start_offset;
    UChar* req_byte_ptr = start_match - 1;
    bool startline = re->options & PCRE_STARTLINE;
    
    do {
        UChar* save_end_subject = end_subject;
        
        /* Reset the maximum number of extractions we might see. */
        
        if (match_block.offset_vector) {
            int* iptr = match_block.offset_vector;
            int* iend = iptr + resetcount;
            while (iptr < iend)
                *iptr++ = -1;
        }
        
        /* Advance to a unique first char if possible. If firstline is true, the
         start of the match is constrained to the first line of a multiline string.
         Implement this by temporarily adjusting end_subject so that we stop scanning
         at a newline. If the match fails at the newline, later code breaks this loop.
         */
        
        /* Now test for a unique first byte */
        
        if (first_byte >= 0) {
            UChar first_char = first_byte;
            if (first_byte_caseless)
                while (start_match < end_subject) {
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
        
        else if (startline) {
            if (start_match > match_block.start_subject + start_offset) {
                while (start_match < end_subject && !isNewline(start_match[-1]))
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
        
        if (req_byte >= 0 && end_subject - start_match < REQ_BYTE_MAX) {
            UChar* p = start_match + ((first_byte >= 0)? 1 : 0);
            
            /* We don't need to repeat the search if we haven't yet reached the
             place we found it at last time. */
            
            if (p > req_byte_ptr) {
                if (req_byte_caseless) {
                    while (p < end_subject) {
                        int pp = *p++;
                        if (pp == req_byte || pp == req_byte2) {
                            p--;
                            break;
                        }
                    }
                } else {
                    while (p < end_subject) {
                        if (*p++ == req_byte) {
                            p--;
                            break;
                        }
                    }
                }
                
                /* If we can't find the required character, break the matching loop */
                
                if (p >= end_subject)
                    break;
                
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
        
        
        /* The code starts after the JSRegExp block and the capture name table. */
        const uschar* start_code = (const uschar*)(re + 1);
        
        int returnCode = match(start_match, start_code, 2, &match_block);
        
        /* When the result is no match, if the subject's first character was a
         newline and the PCRE_FIRSTLINE option is set, break (which will return
         PCRE_ERROR_NOMATCH). The option requests that a match occur before the first
         newline in the subject. Otherwise, advance the pointer to the next character
         and continue - but the continuation will actually happen only when the
         pattern is not anchored. */
        
        if (returnCode == MATCH_NOMATCH) {
            start_match++;
            while(start_match < end_subject && isTrailingSurrogate(*start_match))
                start_match++;
            continue;
        }
        
        if (returnCode != MATCH_MATCH) {
            DPRINTF((">>>> error: returning %d\n", rc));
            return returnCode;
        }
        
        /* We have a match! Copy the offset information from temporary store if
         necessary */
        
        if (using_temporary_offsets) {
            if (offsetcount >= 4) {
                memcpy(offsets + 2, match_block.offset_vector + 2, (offsetcount - 2) * sizeof(int));
                DPRINTF(("Copied offsets from temporary memory\n"));
            }
            if (match_block.end_offset_top > offsetcount)
                match_block.offset_overflow = true;
            
            DPRINTF(("Freeing temporary memory\n"));
            delete [] match_block.offset_vector;
        }
        
        returnCode = match_block.offset_overflow? 0 : match_block.end_offset_top/2;
        
        if (offsetcount < 2)
            returnCode = 0;
        else {
            offsets[0] = start_match - match_block.start_subject;
            offsets[1] = match_block.end_match_ptr - match_block.start_subject;
        }
        
        DPRINTF((">>>> returning %d\n", rc));
        return returnCode;
    } while (start_match <= end_subject);
    
    if (using_temporary_offsets) {
        DPRINTF(("Freeing temporary memory\n"));
        delete [] match_block.offset_vector;
    }
    
    DPRINTF((">>>> returning PCRE_ERROR_NOMATCH\n"));
    return JSRegExpErrorNoMatch;
}
