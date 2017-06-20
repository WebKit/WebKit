/*
 * Copyright (C) 2004, 2007-2008, 2011-2013, 2015-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "URL.h"

#include "DecodeEscapeSequences.h"
#include "MIMETypeRegistry.h"
#include "TextEncoding.h"
#include "URLParser.h"
#include "UUID.h"
#include <stdio.h>
#include <unicode/uidna.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

// FIXME: This file makes too much use of the + operator on String.
// We either have to optimize that operator so it doesn't involve
// so many allocations, or change this to use StringBuffer instead.

using namespace WTF;

namespace WebCore {

typedef Vector<char, 512> CharBuffer;
typedef Vector<UChar, 512> UCharBuffer;

static const unsigned invalidPortNumber = 0xFFFF;

enum URLCharacterClasses {
    // alpha 
    SchemeFirstChar = 1 << 0,

    // ( alpha | digit | "+" | "-" | "." )
    SchemeChar = 1 << 1,

    // mark        = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
    // unreserved  = alphanum | mark
    // ( unreserved | escaped | ";" | ":" | "&" | "=" | "+" | "$" | "," )
    UserInfoChar = 1 << 2,

    // alnum | "." | "-" | "%"
    // The above is what the specification says, but we are lenient to
    // match existing practice and also allow:
    // "_"
    HostnameChar = 1 << 3,

    // hexdigit | ":" | "%"
    IPv6Char = 1 << 4,

    // "#" | "?" | "/" | nul
    PathSegmentEndChar = 1 << 5,

    // not allowed in path
    BadChar = 1 << 6,

    // "\t" | "\n" | "\r"
    TabNewline = 1 << 7
};

static const unsigned char characterClassTable[256] = {
    /* 0 nul */ PathSegmentEndChar,    /* 1 soh */ BadChar,
    /* 2 stx */ BadChar,    /* 3 etx */ BadChar,
    /* 4 eot */ BadChar,    /* 5 enq */ BadChar,    /* 6 ack */ BadChar,    /* 7 bel */ BadChar,
    /* 8 bs */ BadChar,     /* 9 ht */ BadChar | TabNewline,                /* 10 nl */ BadChar | TabNewline,
    /* 11 vt */ BadChar,    /* 12 np */ BadChar,    /* 13 cr */ BadChar | TabNewline,
    /* 14 so */ BadChar,    /* 15 si */ BadChar,
    /* 16 dle */ BadChar,   /* 17 dc1 */ BadChar,   /* 18 dc2 */ BadChar,   /* 19 dc3 */ BadChar,
    /* 20 dc4 */ BadChar,   /* 21 nak */ BadChar,   /* 22 syn */ BadChar,   /* 23 etb */ BadChar,
    /* 24 can */ BadChar,   /* 25 em */ BadChar,    /* 26 sub */ BadChar,   /* 27 esc */ BadChar,
    /* 28 fs */ BadChar,    /* 29 gs */ BadChar,    /* 30 rs */ BadChar,    /* 31 us */ BadChar,
    /* 32 sp */ BadChar,    /* 33  ! */ UserInfoChar,
    /* 34  " */ BadChar,    /* 35  # */ PathSegmentEndChar | BadChar,
    /* 36  $ */ UserInfoChar,    /* 37  % */ UserInfoChar | HostnameChar | IPv6Char | BadChar,
    /* 38  & */ UserInfoChar,    /* 39  ' */ UserInfoChar,
    /* 40  ( */ UserInfoChar,    /* 41  ) */ UserInfoChar,
    /* 42  * */ UserInfoChar,    /* 43  + */ SchemeChar | UserInfoChar,
    /* 44  , */ UserInfoChar,
    /* 45  - */ SchemeChar | UserInfoChar | HostnameChar,
    /* 46  . */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 47  / */ PathSegmentEndChar,
    /* 48  0 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 49  1 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char,    
    /* 50  2 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 51  3 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 52  4 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 53  5 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 54  6 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 55  7 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 56  8 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 57  9 */ SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 58  : */ UserInfoChar | IPv6Char,    /* 59  ; */ UserInfoChar,
    /* 60  < */ BadChar,    /* 61  = */ UserInfoChar,
    /* 62  > */ BadChar,    /* 63  ? */ PathSegmentEndChar | BadChar,
    /* 64  @ */ 0,
    /* 65  A */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,    
    /* 66  B */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 67  C */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 68  D */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 69  E */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 70  F */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 71  G */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 72  H */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 73  I */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 74  J */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 75  K */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 76  L */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 77  M */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 78  N */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 79  O */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 80  P */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 81  Q */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 82  R */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 83  S */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 84  T */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 85  U */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 86  V */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 87  W */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 88  X */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 89  Y */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 90  Z */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 91  [ */ 0,
    /* 92  \ */ 0,    /* 93  ] */ 0,
    /* 94  ^ */ 0,
    /* 95  _ */ UserInfoChar | HostnameChar,
    /* 96  ` */ 0,
    /* 97  a */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 98  b */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 99  c */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 100  d */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 101  e */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char,
    /* 102  f */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | IPv6Char, 
    /* 103  g */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 104  h */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 105  i */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 106  j */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 107  k */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 108  l */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 109  m */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 110  n */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 111  o */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 112  p */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 113  q */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 114  r */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 115  s */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 116  t */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 117  u */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 118  v */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 119  w */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 120  x */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 121  y */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 122  z */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 123  { */ 0,
    /* 124  | */ 0,   /* 125  } */ 0,   /* 126  ~ */ UserInfoChar,   /* 127 del */ BadChar,
    /* 128 */ BadChar, /* 129 */ BadChar, /* 130 */ BadChar, /* 131 */ BadChar,
    /* 132 */ BadChar, /* 133 */ BadChar, /* 134 */ BadChar, /* 135 */ BadChar,
    /* 136 */ BadChar, /* 137 */ BadChar, /* 138 */ BadChar, /* 139 */ BadChar,
    /* 140 */ BadChar, /* 141 */ BadChar, /* 142 */ BadChar, /* 143 */ BadChar,
    /* 144 */ BadChar, /* 145 */ BadChar, /* 146 */ BadChar, /* 147 */ BadChar,
    /* 148 */ BadChar, /* 149 */ BadChar, /* 150 */ BadChar, /* 151 */ BadChar,
    /* 152 */ BadChar, /* 153 */ BadChar, /* 154 */ BadChar, /* 155 */ BadChar,
    /* 156 */ BadChar, /* 157 */ BadChar, /* 158 */ BadChar, /* 159 */ BadChar,
    /* 160 */ BadChar, /* 161 */ BadChar, /* 162 */ BadChar, /* 163 */ BadChar,
    /* 164 */ BadChar, /* 165 */ BadChar, /* 166 */ BadChar, /* 167 */ BadChar,
    /* 168 */ BadChar, /* 169 */ BadChar, /* 170 */ BadChar, /* 171 */ BadChar,
    /* 172 */ BadChar, /* 173 */ BadChar, /* 174 */ BadChar, /* 175 */ BadChar,
    /* 176 */ BadChar, /* 177 */ BadChar, /* 178 */ BadChar, /* 179 */ BadChar,
    /* 180 */ BadChar, /* 181 */ BadChar, /* 182 */ BadChar, /* 183 */ BadChar,
    /* 184 */ BadChar, /* 185 */ BadChar, /* 186 */ BadChar, /* 187 */ BadChar,
    /* 188 */ BadChar, /* 189 */ BadChar, /* 190 */ BadChar, /* 191 */ BadChar,
    /* 192 */ BadChar, /* 193 */ BadChar, /* 194 */ BadChar, /* 195 */ BadChar,
    /* 196 */ BadChar, /* 197 */ BadChar, /* 198 */ BadChar, /* 199 */ BadChar,
    /* 200 */ BadChar, /* 201 */ BadChar, /* 202 */ BadChar, /* 203 */ BadChar,
    /* 204 */ BadChar, /* 205 */ BadChar, /* 206 */ BadChar, /* 207 */ BadChar,
    /* 208 */ BadChar, /* 209 */ BadChar, /* 210 */ BadChar, /* 211 */ BadChar,
    /* 212 */ BadChar, /* 213 */ BadChar, /* 214 */ BadChar, /* 215 */ BadChar,
    /* 216 */ BadChar, /* 217 */ BadChar, /* 218 */ BadChar, /* 219 */ BadChar,
    /* 220 */ BadChar, /* 221 */ BadChar, /* 222 */ BadChar, /* 223 */ BadChar,
    /* 224 */ BadChar, /* 225 */ BadChar, /* 226 */ BadChar, /* 227 */ BadChar,
    /* 228 */ BadChar, /* 229 */ BadChar, /* 230 */ BadChar, /* 231 */ BadChar,
    /* 232 */ BadChar, /* 233 */ BadChar, /* 234 */ BadChar, /* 235 */ BadChar,
    /* 236 */ BadChar, /* 237 */ BadChar, /* 238 */ BadChar, /* 239 */ BadChar,
    /* 240 */ BadChar, /* 241 */ BadChar, /* 242 */ BadChar, /* 243 */ BadChar,
    /* 244 */ BadChar, /* 245 */ BadChar, /* 246 */ BadChar, /* 247 */ BadChar,
    /* 248 */ BadChar, /* 249 */ BadChar, /* 250 */ BadChar, /* 251 */ BadChar,
    /* 252 */ BadChar, /* 253 */ BadChar, /* 254 */ BadChar, /* 255 */ BadChar
};

enum PercentEncodeCharacterClass {
    // Class names match the URL Standard; each class is a superset of the previous one.
    PercentEncodeSimple = 255,
    PercentEncodeDefault = 127,
    PercentEncodePassword = 63,
    PercentEncodeUsername = 31,
};

static const unsigned char percentEncodeClassTable[256] = {
    /* 0 nul */ PercentEncodeSimple,    /* 1 soh */ PercentEncodeSimple,    /* 2 stx */ PercentEncodeSimple,    /* 3 etx */ PercentEncodeSimple,
    /* 4 eot */ PercentEncodeSimple,    /* 5 enq */ PercentEncodeSimple,    /* 6 ack */ PercentEncodeSimple,    /* 7 bel */ PercentEncodeSimple,
    /* 8 bs */ PercentEncodeSimple,     /* 9 ht */ PercentEncodeSimple,     /* 10 nl */ PercentEncodeSimple,    /* 11 vt */ PercentEncodeSimple,
    /* 12 np */ PercentEncodeSimple,    /* 13 cr */ PercentEncodeSimple,    /* 14 so */ PercentEncodeSimple,    /* 15 si */ PercentEncodeSimple,
    /* 16 dle */ PercentEncodeSimple,   /* 17 dc1 */ PercentEncodeSimple,   /* 18 dc2 */ PercentEncodeSimple,   /* 19 dc3 */ PercentEncodeSimple,
    /* 20 dc4 */ PercentEncodeSimple,   /* 21 nak */ PercentEncodeSimple,   /* 22 syn */ PercentEncodeSimple,   /* 23 etb */ PercentEncodeSimple,
    /* 24 can */ PercentEncodeSimple,   /* 25 em */ PercentEncodeSimple,    /* 26 sub */ PercentEncodeSimple,   /* 27 esc */ PercentEncodeSimple,
    /* 28 fs */ PercentEncodeSimple,    /* 29 gs */ PercentEncodeSimple,    /* 30 rs */ PercentEncodeSimple,    /* 31 us */ PercentEncodeSimple,
    /* 32 sp */ PercentEncodeDefault,
    /* 33  ! */ 0,
    /* 34  " */ PercentEncodeDefault,
    /* 35  # */ PercentEncodeDefault,
    /* 36  $ */ 0,
    /* 37  % */ 0,
    /* 38  & */ 0,
    /* 39  ' */ 0,
    /* 40  ( */ 0,
    /* 41  ) */ 0,
    /* 42  * */ 0,
    /* 43  + */ 0,
    /* 44  , */ 0,
    /* 45  - */ 0,
    /* 46  . */ 0,
    /* 47  / */ PercentEncodePassword,
    /* 48  0 */ 0,    /* 49  1 */ 0,    /* 50  2 */ 0,    /* 51  3 */ 0,
    /* 52  4 */ 0,    /* 53  5 */ 0,    /* 54  6 */ 0,    /* 55  7 */ 0,
    /* 56  8 */ 0,    /* 57  9 */ 0,
    /* 58  : */ PercentEncodeUsername,
    /* 59  ; */ 0,
    /* 60  < */ PercentEncodeDefault,
    /* 61  = */ 0,
    /* 62  > */ PercentEncodeDefault,
    /* 63  ? */ PercentEncodeDefault,
    /* 64  @ */ PercentEncodePassword,
    /* 65  A */ 0,    /* 66  B */ 0,    /* 67  C */ 0,    /* 68  D */ 0,
    /* 69  E */ 0,    /* 70  F */ 0,    /* 71  G */ 0,    /* 72  H */ 0,
    /* 73  I */ 0,    /* 74  J */ 0,    /* 75  K */ 0,    /* 76  L */ 0,
    /* 77  M */ 0,    /* 78  N */ 0,    /* 79  O */ 0,    /* 80  P */ 0,
    /* 81  Q */ 0,    /* 82  R */ 0,    /* 83  S */ 0,    /* 84  T */ 0,
    /* 85  U */ 0,    /* 86  V */ 0,    /* 87  W */ 0,    /* 88  X */ 0,
    /* 89  Y */ 0,    /* 90  Z */ 0,
    /* 91  [ */ 0,
    /* 92  \ */ PercentEncodePassword,
    /* 93  ] */ 0,
    /* 94  ^ */ 0,
    /* 95  _ */ 0,
    /* 96  ` */ PercentEncodeDefault,
    /* 97  a */ 0,    /* 98  b */ 0,    /* 99  c */ 0,    /* 100  d */ 0,
    /* 101  e */ 0,    /* 102  f */ 0,    /* 103  g */ 0,    /* 104  h */ 0,
    /* 105  i */ 0,    /* 106  j */ 0,    /* 107  k */ 0,    /* 108  l */ 0,
    /* 109  m */ 0,    /* 110  n */ 0,    /* 111  o */ 0,    /* 112  p */ 0,
    /* 113  q */ 0,    /* 114  r */ 0,    /* 115  s */ 0,    /* 116  t */ 0,
    /* 117  u */ 0,    /* 118  v */ 0,    /* 119  w */ 0,    /* 120  x */ 0,
    /* 121  y */ 0,    /* 122  z */ 0,
    /* 123  { */ 0,
    /* 124  | */ 0,
    /* 125  } */ 0,
    /* 126  ~ */ 0,
    /* 127 del */ PercentEncodeSimple,
    /* 128 */ PercentEncodeSimple, /* 129 */ PercentEncodeSimple, /* 130 */ PercentEncodeSimple, /* 131 */ PercentEncodeSimple,
    /* 132 */ PercentEncodeSimple, /* 133 */ PercentEncodeSimple, /* 134 */ PercentEncodeSimple, /* 135 */ PercentEncodeSimple,
    /* 136 */ PercentEncodeSimple, /* 137 */ PercentEncodeSimple, /* 138 */ PercentEncodeSimple, /* 139 */ PercentEncodeSimple,
    /* 140 */ PercentEncodeSimple, /* 141 */ PercentEncodeSimple, /* 142 */ PercentEncodeSimple, /* 143 */ PercentEncodeSimple,
    /* 144 */ PercentEncodeSimple, /* 145 */ PercentEncodeSimple, /* 146 */ PercentEncodeSimple, /* 147 */ PercentEncodeSimple,
    /* 148 */ PercentEncodeSimple, /* 149 */ PercentEncodeSimple, /* 150 */ PercentEncodeSimple, /* 151 */ PercentEncodeSimple,
    /* 152 */ PercentEncodeSimple, /* 153 */ PercentEncodeSimple, /* 154 */ PercentEncodeSimple, /* 155 */ PercentEncodeSimple,
    /* 156 */ PercentEncodeSimple, /* 157 */ PercentEncodeSimple, /* 158 */ PercentEncodeSimple, /* 159 */ PercentEncodeSimple,
    /* 160 */ PercentEncodeSimple, /* 161 */ PercentEncodeSimple, /* 162 */ PercentEncodeSimple, /* 163 */ PercentEncodeSimple,
    /* 164 */ PercentEncodeSimple, /* 165 */ PercentEncodeSimple, /* 166 */ PercentEncodeSimple, /* 167 */ PercentEncodeSimple,
    /* 168 */ PercentEncodeSimple, /* 169 */ PercentEncodeSimple, /* 170 */ PercentEncodeSimple, /* 171 */ PercentEncodeSimple,
    /* 172 */ PercentEncodeSimple, /* 173 */ PercentEncodeSimple, /* 174 */ PercentEncodeSimple, /* 175 */ PercentEncodeSimple,
    /* 176 */ PercentEncodeSimple, /* 177 */ PercentEncodeSimple, /* 178 */ PercentEncodeSimple, /* 179 */ PercentEncodeSimple,
    /* 180 */ PercentEncodeSimple, /* 181 */ PercentEncodeSimple, /* 182 */ PercentEncodeSimple, /* 183 */ PercentEncodeSimple,
    /* 184 */ PercentEncodeSimple, /* 185 */ PercentEncodeSimple, /* 186 */ PercentEncodeSimple, /* 187 */ PercentEncodeSimple,
    /* 188 */ PercentEncodeSimple, /* 189 */ PercentEncodeSimple, /* 190 */ PercentEncodeSimple, /* 191 */ PercentEncodeSimple,
    /* 192 */ PercentEncodeSimple, /* 193 */ PercentEncodeSimple, /* 194 */ PercentEncodeSimple, /* 195 */ PercentEncodeSimple,
    /* 196 */ PercentEncodeSimple, /* 197 */ PercentEncodeSimple, /* 198 */ PercentEncodeSimple, /* 199 */ PercentEncodeSimple,
    /* 200 */ PercentEncodeSimple, /* 201 */ PercentEncodeSimple, /* 202 */ PercentEncodeSimple, /* 203 */ PercentEncodeSimple,
    /* 204 */ PercentEncodeSimple, /* 205 */ PercentEncodeSimple, /* 206 */ PercentEncodeSimple, /* 207 */ PercentEncodeSimple,
    /* 208 */ PercentEncodeSimple, /* 209 */ PercentEncodeSimple, /* 210 */ PercentEncodeSimple, /* 211 */ PercentEncodeSimple,
    /* 212 */ PercentEncodeSimple, /* 213 */ PercentEncodeSimple, /* 214 */ PercentEncodeSimple, /* 215 */ PercentEncodeSimple,
    /* 216 */ PercentEncodeSimple, /* 217 */ PercentEncodeSimple, /* 218 */ PercentEncodeSimple, /* 219 */ PercentEncodeSimple,
    /* 220 */ PercentEncodeSimple, /* 221 */ PercentEncodeSimple, /* 222 */ PercentEncodeSimple, /* 223 */ PercentEncodeSimple,
    /* 224 */ PercentEncodeSimple, /* 225 */ PercentEncodeSimple, /* 226 */ PercentEncodeSimple, /* 227 */ PercentEncodeSimple,
    /* 228 */ PercentEncodeSimple, /* 229 */ PercentEncodeSimple, /* 230 */ PercentEncodeSimple, /* 231 */ PercentEncodeSimple,
    /* 232 */ PercentEncodeSimple, /* 233 */ PercentEncodeSimple, /* 234 */ PercentEncodeSimple, /* 235 */ PercentEncodeSimple,
    /* 236 */ PercentEncodeSimple, /* 237 */ PercentEncodeSimple, /* 238 */ PercentEncodeSimple, /* 239 */ PercentEncodeSimple,
    /* 240 */ PercentEncodeSimple, /* 241 */ PercentEncodeSimple, /* 242 */ PercentEncodeSimple, /* 243 */ PercentEncodeSimple,
    /* 244 */ PercentEncodeSimple, /* 245 */ PercentEncodeSimple, /* 246 */ PercentEncodeSimple, /* 247 */ PercentEncodeSimple,
    /* 248 */ PercentEncodeSimple, /* 249 */ PercentEncodeSimple, /* 250 */ PercentEncodeSimple, /* 251 */ PercentEncodeSimple,
    /* 252 */ PercentEncodeSimple, /* 253 */ PercentEncodeSimple, /* 254 */ PercentEncodeSimple, /* 255 */ PercentEncodeSimple
};

static inline bool isSchemeFirstChar(UChar c) { return c <= 0xff && (characterClassTable[c] & SchemeFirstChar); }
static inline bool isSchemeChar(UChar c) { return c <= 0xff && (characterClassTable[c] & SchemeChar); }
static inline bool isBadChar(unsigned char c) { return characterClassTable[c] & BadChar; }
static inline bool isTabNewline(UChar c) { return c <= 0xff && (characterClassTable[c] & TabNewline); }

String encodeWithURLEscapeSequences(const String& notEncodedString, PercentEncodeCharacterClass whatToEncode);

// Copies the source to the destination, assuming all the source characters are
// ASCII. The destination buffer must be large enough. Null characters are allowed
// in the source string, and no attempt is made to null-terminate the result.
static void copyASCII(const String& string, char* dest)
{
    if (string.isEmpty())
        return;

    if (string.is8Bit())
        memcpy(dest, string.characters8(), string.length());
    else {
        const UChar* src = string.characters16();
        size_t length = string.length();
        for (size_t i = 0; i < length; i++)
            dest[i] = static_cast<char>(src[i]);
    }
}

inline bool URL::protocolIs(const String& string, const char* protocol)
{
    return WebCore::protocolIs(string, protocol);
}

void URL::invalidate()
{
    m_isValid = false;
    m_protocolIsInHTTPFamily = false;
    m_cannotBeABaseURL = false;
    m_schemeEnd = 0;
    m_userStart = 0;
    m_userEnd = 0;
    m_passwordEnd = 0;
    m_hostEnd = 0;
    m_portEnd = 0;
    m_pathEnd = 0;
    m_pathAfterLastSlash = 0;
    m_queryEnd = 0;
    m_fragmentEnd = 0;
}

URL::URL(ParsedURLStringTag, const String& url)
{
    URLParser parser(url);
    *this = parser.result();

#if OS(WINDOWS)
    // FIXME(148598): Work around Windows local file handling bug in CFNetwork
    ASSERT(isLocalFile() || url == m_string);
#else
    ASSERT(url == m_string);
#endif
}

URL::URL(const URL& base, const String& relative)
{
    URLParser parser(relative, base);
    *this = parser.result();
}

URL::URL(const URL& base, const String& relative, const TextEncoding& encoding)
{
    // For UTF-{7,16,32}, we want to use UTF-8 for the query part as
    // we do when submitting a form. A form with GET method
    // has its contents added to a URL as query params and it makes sense
    // to be consistent.
    URLParser parser(relative, base, encoding.encodingForFormSubmission());
    *this = parser.result();
}

static bool shouldTrimFromURL(UChar c)
{
    // Browsers ignore leading/trailing whitespace and control
    // characters from URLs.  Note that c is an *unsigned* char here
    // so this comparison should only catch control characters.
    return c <= ' ';
}

URL URL::isolatedCopy() const
{
    URL result = *this;
    result.m_string = result.m_string.isolatedCopy();
    return result;
}

String URL::lastPathComponent() const
{
    if (!hasPath())
        return String();

    unsigned end = m_pathEnd - 1;
    if (m_string[end] == '/')
        --end;

    size_t start = m_string.reverseFind('/', end);
    if (start < static_cast<unsigned>(m_portEnd))
        return String();
    ++start;

    return m_string.substring(start, end - start + 1);
}

StringView URL::protocol() const
{
    return StringView(m_string).substring(0, m_schemeEnd);
}

String URL::host() const
{
    unsigned start = hostStart();
    return m_string.substring(start, m_hostEnd - start);
}

std::optional<uint16_t> URL::port() const
{
    if (!m_portEnd || m_hostEnd >= m_portEnd - 1)
        return std::nullopt;

    bool ok = false;
    unsigned number;
    if (m_string.is8Bit())
        number = charactersToUIntStrict(m_string.characters8() + m_hostEnd + 1, m_portEnd - m_hostEnd - 1, &ok);
    else
        number = charactersToUIntStrict(m_string.characters16() + m_hostEnd + 1, m_portEnd - m_hostEnd - 1, &ok);
    if (!ok || number > std::numeric_limits<uint16_t>::max())
        return std::nullopt;
    return number;
}

String URL::hostAndPort() const
{
    if (auto port = this->port())
        return host() + ':' + String::number(port.value());
    return host();
}

String URL::protocolHostAndPort() const
{
    String result = m_string.substring(0, m_portEnd);

    if (m_passwordEnd - m_userStart > 0) {
        const int allowForTrailingAtSign = 1;
        result.remove(m_userStart, m_passwordEnd - m_userStart + allowForTrailingAtSign);
    }

    return result;
}

String URL::user() const
{
    return decodeURLEscapeSequences(m_string.substring(m_userStart, m_userEnd - m_userStart));
}

String URL::pass() const
{
    if (m_passwordEnd == m_userEnd)
        return String();

    return decodeURLEscapeSequences(m_string.substring(m_userEnd + 1, m_passwordEnd - m_userEnd - 1));
}

String URL::encodedUser() const
{
    return m_string.substring(m_userStart, m_userEnd - m_userStart);
}

String URL::encodedPass() const
{
    if (m_passwordEnd == m_userEnd)
        return String();

    return m_string.substring(m_userEnd + 1, m_passwordEnd - m_userEnd - 1);
}

String URL::fragmentIdentifier() const
{
    if (m_fragmentEnd == m_queryEnd)
        return String();

    return m_string.substring(m_queryEnd + 1, m_fragmentEnd - (m_queryEnd + 1));
}

bool URL::hasFragmentIdentifier() const
{
    return m_fragmentEnd != m_queryEnd;
}

String URL::baseAsString() const
{
    return m_string.left(m_pathAfterLastSlash);
}

#if !USE(CF)

String URL::fileSystemPath() const
{
    if (!isValid() || !isLocalFile())
        return String();

    return decodeURLEscapeSequences(path());
}

#endif

#ifdef NDEBUG

static inline void assertProtocolIsGood(StringView)
{
}

#else

static void assertProtocolIsGood(StringView protocol)
{
    // FIXME: We probably don't need this function any more.
    // The isASCIIAlphaCaselessEqual function asserts that passed-in characters
    // are ones it can handle; the older code did not and relied on these checks.
    for (auto character : protocol.codeUnits()) {
        ASSERT(isASCII(character));
        ASSERT(character > ' ');
        ASSERT(!isASCIIUpper(character));
        ASSERT(toASCIILowerUnchecked(character) == character);
    }
}

#endif

static Lock& defaultPortForProtocolMapForTestingLock()
{
    static NeverDestroyed<Lock> lock;
    return lock;
}

using DefaultPortForProtocolMapForTesting = HashMap<String, uint16_t>;
static DefaultPortForProtocolMapForTesting*& defaultPortForProtocolMapForTesting()
{
    static DefaultPortForProtocolMapForTesting* defaultPortForProtocolMap;
    return defaultPortForProtocolMap;
}

static DefaultPortForProtocolMapForTesting& ensureDefaultPortForProtocolMapForTesting()
{
    DefaultPortForProtocolMapForTesting*& defaultPortForProtocolMap = defaultPortForProtocolMapForTesting();
    if (!defaultPortForProtocolMap)
        defaultPortForProtocolMap = new DefaultPortForProtocolMapForTesting;
    return *defaultPortForProtocolMap;
}

void registerDefaultPortForProtocolForTesting(uint16_t port, const String& protocol)
{
    LockHolder locker(defaultPortForProtocolMapForTestingLock());
    ensureDefaultPortForProtocolMapForTesting().add(protocol, port);
}

void clearDefaultPortForProtocolMapForTesting()
{
    LockHolder locker(defaultPortForProtocolMapForTestingLock());
    if (auto* map = defaultPortForProtocolMapForTesting())
        map->clear();
}

std::optional<uint16_t> defaultPortForProtocol(StringView protocol)
{
    if (auto* overrideMap = defaultPortForProtocolMapForTesting()) {
        LockHolder locker(defaultPortForProtocolMapForTestingLock());
        ASSERT(overrideMap); // No need to null check again here since overrideMap cannot become null after being non-null.
        auto iterator = overrideMap->find(protocol.toStringWithoutCopying());
        if (iterator != overrideMap->end())
            return iterator->value;
    }
    return URLParser::defaultPortForProtocol(protocol);
}

bool isDefaultPortForProtocol(uint16_t port, StringView protocol)
{
    return defaultPortForProtocol(protocol) == port;
}

bool URL::protocolIs(const char* protocol) const
{
    assertProtocolIsGood(StringView(reinterpret_cast<const LChar*>(protocol), strlen(protocol)));

    // JavaScript URLs are "valid" and should be executed even if URL decides they are invalid.
    // The free function protocolIsJavaScript() should be used instead. 
    ASSERT(!equalLettersIgnoringASCIICase(StringView(protocol), "javascript"));

    if (!m_isValid)
        return false;

    // Do the comparison without making a new string object.
    for (unsigned i = 0; i < m_schemeEnd; ++i) {
        if (!protocol[i] || !isASCIIAlphaCaselessEqual(m_string[i], protocol[i]))
            return false;
    }
    return !protocol[m_schemeEnd]; // We should have consumed all characters in the argument.
}

bool URL::protocolIs(StringView protocol) const
{
    assertProtocolIsGood(protocol);

    if (!m_isValid)
        return false;
    
    if (m_schemeEnd != protocol.length())
        return false;

    // Do the comparison without making a new string object.
    for (unsigned i = 0; i < m_schemeEnd; ++i) {
        if (!isASCIIAlphaCaselessEqual(m_string[i], protocol[i]))
            return false;
    }
    return true;
}

String URL::query() const
{
    if (m_queryEnd == m_pathEnd)
        return String();

    return m_string.substring(m_pathEnd + 1, m_queryEnd - (m_pathEnd + 1)); 
}

String URL::path() const
{
    return m_string.substring(m_portEnd, m_pathEnd - m_portEnd);
}

bool URL::setProtocol(const String& s)
{
    // Firefox and IE remove everything after the first ':'.
    size_t separatorPosition = s.find(':');
    String newProtocol = s.substring(0, separatorPosition);

    if (!isValidProtocol(newProtocol))
        return false;

    if (!m_isValid) {
        URLParser parser(makeString(newProtocol, ":", m_string));
        *this = parser.result();
        return true;
    }

    URLParser parser(makeString(newProtocol, m_string.substring(m_schemeEnd)));
    *this = parser.result();
    return true;
}

static bool containsOnlyASCII(StringView string)
{
    if (string.is8Bit())
        return charactersAreAllASCII(string.characters8(), string.length());
    return charactersAreAllASCII(string.characters16(), string.length());
}
    
// Appends the punycoded hostname identified by the given string and length to
// the output buffer. The result will not be null terminated.
// Return value of false means error in encoding.
static bool appendEncodedHostname(UCharBuffer& buffer, StringView string)
{
    // Needs to be big enough to hold an IDN-encoded name.
    // For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
    const unsigned hostnameBufferLength = 2048;
    
    if (string.length() > hostnameBufferLength || containsOnlyASCII(string)) {
        append(buffer, string);
        return true;
    }
    
    UChar hostnameBuffer[hostnameBufferLength];
    UErrorCode error = U_ZERO_ERROR;
    
#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    int32_t numCharactersConverted = uidna_IDNToASCII(string.upconvertedCharacters(), string.length(), hostnameBuffer,
        hostnameBufferLength, UIDNA_ALLOW_UNASSIGNED, 0, &error);
#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic pop
#endif
    
    if (error == U_ZERO_ERROR) {
        buffer.append(hostnameBuffer, numCharactersConverted);
        return true;
    }
    return false;
}
    
void URL::setHost(const String& s)
{
    if (!m_isValid)
        return;

    auto colonIndex = s.find(':');
    if (colonIndex != notFound)
        return;

    UCharBuffer encodedHostName;
    if (!appendEncodedHostname(encodedHostName, s))
        return;
    
    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1;
    
    StringBuilder builder;
    builder.append(m_string.left(hostStart()));
    if (slashSlashNeeded)
        builder.appendLiteral("//");
    builder.append(StringView(encodedHostName.data(), encodedHostName.size()));
    builder.append(m_string.substring(m_hostEnd));

    URLParser parser(builder.toString());
    *this = parser.result();
}

void URL::removePort()
{
    if (m_hostEnd == m_portEnd)
        return;
    URLParser parser(m_string.left(m_hostEnd) + m_string.substring(m_portEnd));
    *this = parser.result();
}

void URL::setPort(unsigned short i)
{
    if (!m_isValid)
        return;

    bool colonNeeded = m_portEnd == m_hostEnd;
    unsigned portStart = (colonNeeded ? m_hostEnd : m_hostEnd + 1);

    URLParser parser(makeString(m_string.left(portStart), (colonNeeded ? ":" : ""), String::number(i), m_string.substring(m_portEnd)));
    *this = parser.result();
}

void URL::setHostAndPort(const String& hostAndPort)
{
    if (!m_isValid)
        return;

    StringView hostName(hostAndPort);
    StringView port;
    
    auto colonIndex = hostName.find(':');
    if (colonIndex != notFound) {
        port = hostName.substring(colonIndex + 1);
        bool ok;
        int portInt = port.toIntStrict(ok);
        if (!ok || portInt < 0)
            return;
        hostName = hostName.substring(0, colonIndex);
    }

    if (hostName.isEmpty())
        return;

    UCharBuffer encodedHostName;
    if (!appendEncodedHostname(encodedHostName, hostName))
        return;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1;

    StringBuilder builder;
    builder.append(m_string.left(hostStart()));
    if (slashSlashNeeded)
        builder.appendLiteral("//");
    builder.append(StringView(encodedHostName.data(), encodedHostName.size()));
    if (!port.isEmpty()) {
        builder.appendLiteral(":");
        builder.append(port);
    }
    builder.append(m_string.substring(m_portEnd));

    URLParser parser(builder.toString());
    *this = parser.result();
}

void URL::setUser(const String& user)
{
    if (!m_isValid)
        return;

    // FIXME: Non-ASCII characters must be encoded and escaped to match parse() expectations,
    // and to avoid changing more than just the user login.

    unsigned end = m_userEnd;
    if (!user.isEmpty()) {
        String u = encodeWithURLEscapeSequences(user, PercentEncodeUsername);
        if (m_userStart == m_schemeEnd + 1)
            u = "//" + u;
        // Add '@' if we didn't have one before.
        if (end == m_hostEnd || (end == m_passwordEnd && m_string[end] != '@'))
            u.append('@');
        URLParser parser(makeString(m_string.left(m_userStart), u, m_string.substring(end)));
        *this = parser.result();
    } else {
        // Remove '@' if we now have neither user nor password.
        if (m_userEnd == m_passwordEnd && end != m_hostEnd && m_string[end] == '@')
            end += 1;
        // We don't want to parse in the extremely common case where we are not going to make a change.
        if (m_userStart != end) {
            URLParser parser(makeString(m_string.left(m_userStart), m_string.substring(end)));
            *this = parser.result();
        }
    }
}

void URL::setPass(const String& password)
{
    if (!m_isValid)
        return;

    unsigned end = m_passwordEnd;
    if (!password.isEmpty()) {
        String p = ":" + encodeWithURLEscapeSequences(password, PercentEncodePassword) + "@";
        if (m_userEnd == m_schemeEnd + 1)
            p = "//" + p;
        // Eat the existing '@' since we are going to add our own.
        if (end != m_hostEnd && m_string[end] == '@')
            end += 1;
        URLParser parser(makeString(m_string.left(m_userEnd), p, m_string.substring(end)));
        *this = parser.result();
    } else {
        // Remove '@' if we now have neither user nor password.
        if (m_userStart == m_userEnd && end != m_hostEnd && m_string[end] == '@')
            end += 1;
        // We don't want to parse in the extremely common case where we are not going to make a change.
        if (m_userEnd != end) {
            URLParser parser(makeString(m_string.left(m_userEnd), m_string.substring(end)));
            *this = parser.result();
        }
    }
}

void URL::setFragmentIdentifier(StringView identifier)
{
    if (!m_isValid)
        return;

    // FIXME: Optimize the case where the identifier already happens to be equal to what was passed?
    // FIXME: Is it correct to do this without encoding and escaping non-ASCII characters?
    *this = URLParser { makeString(StringView { m_string }.substring(0, m_queryEnd), '#', identifier) }.result();
}

void URL::removeFragmentIdentifier()
{
    if (!m_isValid) {
        ASSERT(!m_fragmentEnd);
        ASSERT(!m_queryEnd);
        return;
    }
    if (m_fragmentEnd > m_queryEnd)
        m_string = m_string.left(m_queryEnd);
    m_fragmentEnd = m_queryEnd;
}
    
void URL::setQuery(const String& query)
{
    if (!m_isValid)
        return;

    // FIXME: '#' and non-ASCII characters must be encoded and escaped.
    // Usually, the query is encoded using document encoding, not UTF-8, but we don't have
    // access to the document in this function.
    // https://webkit.org/b/161176
    if ((query.isEmpty() || query[0] != '?') && !query.isNull()) {
        URLParser parser(makeString(m_string.left(m_pathEnd), "?", query, m_string.substring(m_queryEnd)));
        *this = parser.result();
    } else {
        URLParser parser(makeString(m_string.left(m_pathEnd), query, m_string.substring(m_queryEnd)));
        *this = parser.result();
    }

}

void URL::setPath(const String& s)
{
    if (!m_isValid)
        return;

    // FIXME: encodeWithURLEscapeSequences does not correctly escape '#' and '?', so fragment and query parts
    // may be inadvertently affected.
    String path = s;
    if (path.isEmpty() || path[0] != '/')
        path = "/" + path;

    URLParser parser(makeString(m_string.left(m_portEnd), encodeWithURLEscapeSequences(path), m_string.substring(m_pathEnd)));
    *this = parser.result();
}

String decodeURLEscapeSequences(const String& string)
{
    return decodeEscapeSequences<URLEscapeSequence>(string, UTF8Encoding());
}

String decodeURLEscapeSequences(const String& string, const TextEncoding& encoding)
{
    return decodeEscapeSequences<URLEscapeSequence>(string, encoding);
}

// Caution: This function does not bounds check.
static void appendEscapedChar(char*& buffer, unsigned char c)
{
    *buffer++ = '%';
    placeByteAsHex(c, buffer);
}

String URL::serialize(bool omitFragment) const
{
    if (omitFragment)
        return m_string.left(m_queryEnd);
    return m_string;
}

#if PLATFORM(IOS)

static bool shouldCanonicalizeScheme = true;

void enableURLSchemeCanonicalization(bool enableSchemeCanonicalization)
{
    shouldCanonicalizeScheme = enableSchemeCanonicalization;
}

#endif

template<size_t length>
static inline bool equal(const char* a, const char (&b)[length])
{
#if PLATFORM(IOS)
    if (!shouldCanonicalizeScheme) {
        for (size_t i = 0; i < length; ++i) {
            if (toASCIILower(a[i]) != b[i])
                return false;
        }
        return true;
    }
#endif
    for (size_t i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

template<size_t lengthB>
static inline bool equal(const char* stringA, size_t lengthA, const char (&stringB)[lengthB])
{
    return lengthA == lengthB && equal(stringA, stringB);
}

bool equalIgnoringFragmentIdentifier(const URL& a, const URL& b)
{
    if (a.m_queryEnd != b.m_queryEnd)
        return false;
    unsigned queryLength = a.m_queryEnd;
    for (unsigned i = 0; i < queryLength; ++i)
        if (a.string()[i] != b.string()[i])
            return false;
    return true;
}

bool protocolHostAndPortAreEqual(const URL& a, const URL& b)
{
    if (a.m_schemeEnd != b.m_schemeEnd)
        return false;

    unsigned hostStartA = a.hostStart();
    unsigned hostLengthA = a.hostEnd() - hostStartA;
    unsigned hostStartB = b.hostStart();
    unsigned hostLengthB = b.hostEnd() - b.hostStart();
    if (hostLengthA != hostLengthB)
        return false;

    // Check the scheme
    for (unsigned i = 0; i < a.m_schemeEnd; ++i) {
        if (a.string()[i] != b.string()[i])
            return false;
    }

    // And the host
    for (unsigned i = 0; i < hostLengthA; ++i) {
        if (a.string()[hostStartA + i] != b.string()[hostStartB + i])
            return false;
    }

    if (a.port() != b.port())
        return false;

    return true;
}

bool hostsAreEqual(const URL& a, const URL& b)
{
    unsigned hostStartA = a.hostStart();
    unsigned hostLengthA = a.hostEnd() - hostStartA;
    unsigned hostStartB = b.hostStart();
    unsigned hostLengthB = b.hostEnd() - hostStartB;
    if (hostLengthA != hostLengthB)
        return false;

    for (unsigned i = 0; i < hostLengthA; ++i) {
        if (a.string()[hostStartA + i] != b.string()[hostStartB + i])
            return false;
    }

    return true;
}

String encodeWithURLEscapeSequences(const String& notEncodedString, PercentEncodeCharacterClass whatToEncode)
{
    CString asUTF8 = notEncodedString.utf8();

    CharBuffer buffer(asUTF8.length() * 3 + 1);
    char* p = buffer.data();

    const char* str = asUTF8.data();
    const char* strEnd = str + asUTF8.length();
    while (str < strEnd) {
        unsigned char c = *str++;
        if (percentEncodeClassTable[c] >= whatToEncode)
            appendEscapedChar(p, c);
        else
            *p++ = c;
    }

    ASSERT(p - buffer.data() <= static_cast<int>(buffer.size()));

    return String(buffer.data(), p - buffer.data());
}

String encodeWithURLEscapeSequences(const String& notEncodedString)
{
    CString asUTF8 = notEncodedString.utf8();

    CharBuffer buffer(asUTF8.length() * 3 + 1);
    char* p = buffer.data();

    const char* str = asUTF8.data();
    const char* strEnd = str + asUTF8.length();
    while (str < strEnd) {
        unsigned char c = *str++;
        if (isBadChar(c))
            appendEscapedChar(p, c);
        else
            *p++ = c;
    }

    ASSERT(p - buffer.data() <= static_cast<int>(buffer.size()));

    return String(buffer.data(), p - buffer.data());
}

bool URL::isHierarchical() const
{
    if (!m_isValid)
        return false;
    ASSERT(m_string[m_schemeEnd] == ':');
    return m_string[m_schemeEnd + 1] == '/';
}

void URL::copyToBuffer(Vector<char, 512>& buffer) const
{
    // FIXME: This throws away the high bytes of all the characters in the string!
    // That's fine for a valid URL, which is all ASCII, but not for invalid URLs.
    buffer.resize(m_string.length());
    copyASCII(m_string, buffer.data());
}

// FIXME: Why is this different than protocolIs(StringView, const char*)?
bool protocolIs(const String& url, const char* protocol)
{
    // Do the comparison without making a new string object.
    assertProtocolIsGood(StringView(reinterpret_cast<const LChar*>(protocol), strlen(protocol)));
    bool isLeading = true;
    for (unsigned i = 0, j = 0; url[i]; ++i) {
        // skip leading whitespace and control characters.
        if (isLeading && shouldTrimFromURL(url[i]))
            continue;
        isLeading = false;

        // skip any tabs and newlines.
        if (isTabNewline(url[i]))
            continue;

        if (!protocol[j])
            return url[i] == ':';
        if (!isASCIIAlphaCaselessEqual(url[i], protocol[j]))
            return false;

        ++j;
    }

    return false;
}

bool isValidProtocol(const String& protocol)
{
    // RFC3986: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    if (protocol.isEmpty())
        return false;
    if (!isSchemeFirstChar(protocol[0]))
        return false;
    unsigned protocolLength = protocol.length();
    for (unsigned i = 1; i < protocolLength; i++) {
        if (!isSchemeChar(protocol[i]))
            return false;
    }
    return true;
}

#ifndef NDEBUG

void URL::print() const
{
    printf("%s\n", m_string.utf8().data());
}

#endif

String URL::strippedForUseAsReferrer() const
{
    URL referrer(*this);
    referrer.setUser(String());
    referrer.setPass(String());
    referrer.removeFragmentIdentifier();
    return referrer.string();
}

bool URL::isLocalFile() const
{
    // Including feed here might be a bad idea since drag and drop uses this check
    // and including feed would allow feeds to potentially let someone's blog
    // read the contents of the clipboard on a drag, even without a drop.
    // Likewise with using the FrameLoader::shouldTreatURLAsLocal() function.
    return protocolIs("file");
}

bool protocolIsJavaScript(const String& url)
{
    return protocolIs(url, "javascript");
}

bool protocolIsInHTTPFamily(const String& url)
{
    // Do the comparison without making a new string object.
    return isASCIIAlphaCaselessEqual(url[0], 'h')
        && isASCIIAlphaCaselessEqual(url[1], 't')
        && isASCIIAlphaCaselessEqual(url[2], 't')
        && isASCIIAlphaCaselessEqual(url[3], 'p')
        && (url[4] == ':' || (isASCIIAlphaCaselessEqual(url[4], 's') && url[5] == ':'));
}

const URL& blankURL()
{
    static NeverDestroyed<URL> staticBlankURL(ParsedURLString, "about:blank");
    return staticBlankURL;
}

bool URL::isBlankURL() const
{
    return protocolIs("about");
}

bool portAllowed(const URL& url)
{
    std::optional<uint16_t> port = url.port();

    // Since most URLs don't have a port, return early for the "no port" case.
    if (!port)
        return true;

    // This blocked port list matches the port blocking that Mozilla implements.
    // See http://www.mozilla.org/projects/netlib/PortBanning.html for more information.
    static const uint16_t blockedPortList[] = {
        1,    // tcpmux
        7,    // echo
        9,    // discard
        11,   // systat
        13,   // daytime
        15,   // netstat
        17,   // qotd
        19,   // chargen
        20,   // FTP-data
        21,   // FTP-control
        22,   // SSH
        23,   // telnet
        25,   // SMTP
        37,   // time
        42,   // name
        43,   // nicname
        53,   // domain
        77,   // priv-rjs
        79,   // finger
        87,   // ttylink
        95,   // supdup
        101,  // hostriame
        102,  // iso-tsap
        103,  // gppitnp
        104,  // acr-nema
        109,  // POP2
        110,  // POP3
        111,  // sunrpc
        113,  // auth
        115,  // SFTP
        117,  // uucp-path
        119,  // nntp
        123,  // NTP
        135,  // loc-srv / epmap
        139,  // netbios
        143,  // IMAP2
        179,  // BGP
        389,  // LDAP
        465,  // SMTP+SSL
        512,  // print / exec
        513,  // login
        514,  // shell
        515,  // printer
        526,  // tempo
        530,  // courier
        531,  // Chat
        532,  // netnews
        540,  // UUCP
        556,  // remotefs
        563,  // NNTP+SSL
        587,  // ESMTP
        601,  // syslog-conn
        636,  // LDAP+SSL
        993,  // IMAP+SSL
        995,  // POP3+SSL
        2049, // NFS
        3659, // apple-sasl / PasswordServer [Apple addition]
        4045, // lockd
        4190, // ManageSieve [Apple addition]
        6000, // X11
        6665, // Alternate IRC [Apple addition]
        6666, // Alternate IRC [Apple addition]
        6667, // Standard IRC [Apple addition]
        6668, // Alternate IRC [Apple addition]
        6669, // Alternate IRC [Apple addition]
        invalidPortNumber, // Used to block all invalid port numbers
    };
    const unsigned short* const blockedPortListEnd = blockedPortList + WTF_ARRAY_LENGTH(blockedPortList);

#ifndef NDEBUG
    // The port list must be sorted for binary_search to work.
    static bool checkedPortList = false;
    if (!checkedPortList) {
        for (const unsigned short* p = blockedPortList; p != blockedPortListEnd - 1; ++p)
            ASSERT(*p < *(p + 1));
        checkedPortList = true;
    }
#endif

    // If the port is not in the blocked port list, allow it.
    if (!std::binary_search(blockedPortList, blockedPortListEnd, port.value()))
        return true;

    // Allow ports 21 and 22 for FTP URLs, as Mozilla does.
    if ((port.value() == 21 || port.value() == 22) && url.protocolIs("ftp"))
        return true;

    // Allow any port number in a file URL, since the port number is ignored.
    if (url.protocolIs("file"))
        return true;

    return false;
}

String mimeTypeFromDataURL(const String& url)
{
    ASSERT(protocolIs(url, "data"));

    // FIXME: What's the right behavior when the URL has a comma first, but a semicolon later?
    // Currently this code will break at the semicolon in that case. Not sure that's correct.
    auto index = url.find(';', 5);
    if (index == notFound)
        index = url.find(',', 5);
    if (index == notFound) {
        // FIXME: There was an old comment here that made it sound like this should be returning text/plain.
        // But we have been returning empty string here for some time, so not changing its behavior at this time.
        return emptyString();
    }
    if (index == 5)
        return ASCIILiteral("text/plain");
    ASSERT(index >= 5);
    return url.substring(5, index - 5).convertToASCIILowercase();
}

String mimeTypeFromURL(const URL& url)
{
    String decodedPath = decodeURLEscapeSequences(url.path());
    String extension = decodedPath.substring(decodedPath.reverseFind('.') + 1);

    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    return MIMETypeRegistry::getMIMETypeForExtension(extension);
}

String URL::stringCenterEllipsizedToLength(unsigned length) const
{
    if (string().length() <= length)
        return string();

    return string().left(length / 2 - 1) + "..." + string().right(length / 2 - 2);
}

URL URL::fakeURLWithRelativePart(const String& relativePart)
{
    return URL(URL(), "webkit-fake-url://" + createCanonicalUUIDString() + '/' + relativePart);
}

URL URL::fileURLWithFileSystemPath(const String& filePath)
{
    return URL(URL(), "file:///" + filePath);
}

}
