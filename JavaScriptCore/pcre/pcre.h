/* This is the public header file for JavaScriptCore's variant of the PCRE
library. While this library started out as a copy of PCRE, many of the
features of PCRE have been removed. This library now supports only the
regular expression features required by the JavaScript language specification,
and has only the functions needed by JavaScriptCore and the rest of WebKit.

           Copyright (c) 1997-2005 University of Cambridge
           Copyright (c) 2004, 2005, 2006, 2007 Apple Inc.

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

// FIXME: Header needs to be renamed.

#ifndef PCRE_H
#define PCRE_H

#ifdef __cplusplus
extern "C" {
#endif

/* jsRegExpCompile options */
/* FIXME: Use two booleans instead? */

#define JS_REGEXP_CASELESS      0x00000001
#define JS_REGEXP_MULTILINE     0x00000002

/* jsRegExpExecute error codes */
/* FIXME: Use const after changing this to C++? */

#define JS_REGEXP_ERROR_NOMATCH         (-1)
#define JS_REGEXP_ERROR_NOMEMORY        (-6)
#define JS_REGEXP_ERROR_MATCHLIMIT      (-8)
#define JS_REGEXP_ERROR_INTERNAL       (-14)
#define JS_REGEXP_ERROR_RECURSIONLIMIT (-21)

/* FIXME: Merge with WTF's UChar? */
typedef unsigned short JSRegExpChar;

typedef struct JSRegExp JSRegExp;

JSRegExp* jsRegExpCompile(const JSRegExpChar* pattern, int patternLength, int options, unsigned* numSubpatterns, const char** errorMessage);
int jsRegExpExecute(const JSRegExp*, const JSRegExpChar*, int, int, int*, int);
void jsRegExpFree(JSRegExp*);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
