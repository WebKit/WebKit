/*
 * Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "KURL.h"

#include "CString.h"
#include "PlatformString.h"
#include "RegularExpression.h"
#include "TextEncoding.h"
#include <wtf/Vector.h>
#if USE(ICU_UNICODE)
#include <unicode/uidna.h>
#elif USE(QT4_UNICODE)
#include <QUrl>
#endif

using namespace std;
using namespace WTF;

namespace WebCore {

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

    // digit | "A" | "B" | "C" | "D" | "E" | "F" | "a" | "b" | "c" | "d" | "e" | "f"
    HexDigitChar = 1 << 6,

    // not allowed in path
    BadChar = 1 << 7

};

static const char hexDigits[17] = "0123456789ABCDEF";

static const unsigned char characterClassTable[256] = {
    /* 0 nul */ PathSegmentEndChar,    /* 1 soh */ BadChar,
    /* 2 stx */ BadChar,    /* 3 etx */ BadChar,    
    /* 4 eot */ BadChar,    /* 5 enq */ BadChar,    /* 6 ack */ BadChar,    /* 7 bel */ BadChar,
    /* 8 bs */ BadChar,     /* 9 ht */ BadChar,    /* 10 nl */ BadChar,    /* 11 vt */ BadChar,
    /* 12 np */ BadChar,    /* 13 cr */ BadChar,    /* 14 so */ BadChar,    /* 15 si */ BadChar,
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
    /* 46  . */ SchemeChar | UserInfoChar | HostnameChar,   
    /* 47  / */ PathSegmentEndChar,
    /* 48  0 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 49  1 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,    
    /* 50  2 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 51  3 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 52  4 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 53  5 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 54  6 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 55  7 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 56  8 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 57  9 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 58  : */ UserInfoChar | IPv6Char,    /* 59  ; */ UserInfoChar,
    /* 60  < */ BadChar,    /* 61  = */ UserInfoChar,
    /* 62  > */ BadChar,    /* 63  ? */ PathSegmentEndChar | BadChar,
    /* 64  @ */ 0,
    /* 65  A */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,    
    /* 66  B */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 67  C */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 68  D */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 69  E */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 70  F */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
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
    /* 97  a */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 98  b */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 99  c */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 100  d */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 101  e */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 102  f */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
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

static int copyPathRemovingDots(char* dst, const char* src, int srcStart, int srcEnd);
static char* encodeRelativeString(const KURL &base, const DeprecatedString& rel, const TextEncoding&);
static DeprecatedString substituteBackslashes(const DeprecatedString &string);

static inline bool isSchemeFirstChar(unsigned char c) { return characterClassTable[c] & SchemeFirstChar; }
static inline bool isSchemeChar(unsigned char c) { return characterClassTable[c] & SchemeChar; }
static inline bool isUserInfoChar(unsigned char c) { return characterClassTable[c] & UserInfoChar; }
static inline bool isHostnameChar(unsigned char c) { return characterClassTable[c] & HostnameChar; }
static inline bool isIPv6Char(unsigned char c) { return characterClassTable[c] & IPv6Char; }
static inline bool isPathSegmentEndChar(unsigned char c) { return characterClassTable[c] & PathSegmentEndChar; }
static inline bool isBadChar(unsigned char c) { return characterClassTable[c] & BadChar; }
static inline bool isHexDigit(unsigned char c) { return characterClassTable[c] & HexDigitChar; }

static inline int hexDigitValue(unsigned char c)
{
    ASSERT(isHexDigit(c));
    if (c < 'A')
        return c - '0';
    return (c - 'A' + 10) & 0xF; // handle both upper and lower case without a branch
}

// KURL

KURL::KURL() : m_isValid(false)
{
}

KURL::KURL(const char *url)
{
    if (url && url[0] == '/') {
         // 5 for "file:", 1 for terminator
        size_t urlLength = strlen(url) + 1;
        Vector<char, 2048> buffer(urlLength + 5);
        buffer[0] = 'f';
        buffer[1] = 'i';
        buffer[2] = 'l';
        buffer[3] = 'e';
        buffer[4] = ':';
        memcpy(&buffer[5], url, urlLength);
        parse(buffer.data(), 0);
    } else
        parse(url, 0);
}

KURL::KURL(const DeprecatedString &url)
{
    if (!url.isEmpty() && url[0] == '/') {
        // 5 for "file:", 1 for terminator
        Vector<char, 2048> buffer(url.length() + 6);
        buffer[0] = 'f';
        buffer[1] = 'i';
        buffer[2] = 'l';
        buffer[3] = 'e';
        buffer[4] = ':';
        url.copyLatin1(&buffer[5]);
        parse(buffer.data(), 0);
    } else
        parse(url.ascii(), &url);
}

KURL::KURL(const KURL& base, const DeprecatedString& relative)
{
    init(base, relative, UTF8Encoding());
}

KURL::KURL(const KURL& base, const DeprecatedString& relative, const TextEncoding& encoding)
{
    init(base, relative, encoding);
}

void KURL::init(const KURL &base, const DeprecatedString &relative, const TextEncoding& encoding)
{
    // Allow at least absolute URLs to resolve against an empty URL.
    if (!base.m_isValid && !base.isEmpty()) {
        m_isValid = false;
        return;
    }
    
    bool absolute = false;

    // for compatibility with Win IE, we must treat backslashes as if they were slashes, as long as we're not dealing with the javascript: schema
    DeprecatedString substitutedRelative;
    bool shouldSubstituteBackslashes = relative.contains('\\') && !(relative.startsWith("javascript:", false) || relative.startsWith("data:", false));
    if (shouldSubstituteBackslashes)
        substitutedRelative = substituteBackslashes(relative);

    const DeprecatedString &rel = shouldSubstituteBackslashes ? substitutedRelative : relative;
    
    bool allASCII = rel.isAllASCII();
    char *strBuffer;
    const char *str;
    if (allASCII) {
        strBuffer = 0;
        str = rel.ascii();
    } else {
        strBuffer = encodeRelativeString(base, rel, encoding);
        str = strBuffer;
    }
    
    // workaround for sites that put leading whitespace whitespace on
    // URL references
    bool strippedStart = false;
    while (*str == ' ') {
        str++;
        strippedStart = true;
    }

    // workaround for trailing whitespace - a bit more complicated cause we have to copy
    // it would be even better to replace null-termination with a length parameter
    int len = strlen(str);
    int charsToChopOffEnd = 0;
    for (int pos = len - 1; pos >= 0 && str[pos] == ' '; pos--) {
        charsToChopOffEnd++;
    }
    if (charsToChopOffEnd > 0) {
        char *newStrBuffer = (char *)fastMalloc((len + 1) - charsToChopOffEnd);
        strncpy(newStrBuffer, str, len - charsToChopOffEnd);
        newStrBuffer[len - charsToChopOffEnd] = '\0';
        fastFree(strBuffer);
        strBuffer = newStrBuffer;
        str = strBuffer;
    }

    // According to the RFC, the reference should be interpreted as an
    // absolute URI if possible, using the "leftmost, longest"
    // algorithm. If the URI reference is absolute it will have a
    // scheme, meaning that it will have a colon before the first
    // non-scheme element.
    const char *p = str;
    if (isSchemeFirstChar(*p)) {
        ++p;
        while (isSchemeChar(*p)) {
            ++p;
        }
        if (*p == ':') {
            if (p[1] != '/' && base.protocol().lower() == DeprecatedString(str, p - str).lower() && base.isHierarchical())
                str = p + 1;
            else
                absolute = true;
        }
    }

    if (absolute) {
        parse(str, (allASCII && !strippedStart && (charsToChopOffEnd == 0)) ? &rel : 0);
    } else {
        // If the base is empty or opaque (e.g. data: or javascript:), then the URL is invalid
        // unless the relative URL is a single fragment.
        if (!base.isHierarchical()) {
            if (str[0] == '#') {
                DeprecatedString newURL = base.urlString.left(base.queryEndPos) + str;
                parse(newURL.ascii(), &newURL);
            } else
                m_isValid = false;
            
            if (strBuffer)
                fastFree(strBuffer);
            return;
        }

        switch(str[0]) {
        case '\0':
            // the reference must be empty - the RFC says this is a
            // reference to the same document
            {
                *this = base;
                break;
            }
        case '#':
            // must be fragment-only reference
            {
                DeprecatedString newURL = base.urlString.left(base.queryEndPos) + str;
                parse(newURL.ascii(), &newURL);
                break;
            }
        case '?':
            // query-only reference, special case needed for non-URL results
            {
                DeprecatedString newURL = base.urlString.left(base.pathEndPos) + str;
                parse(newURL.ascii(), &newURL);
                break;
            }
        case '/':
            // must be net-path or absolute-path reference
            {
                if (str[1] == '/') {
                    // net-path
                    DeprecatedString newURL = base.urlString.left(base.schemeEndPos + 1) + str;
                    parse(newURL.ascii(), &newURL);
                } else {
                    // abs-path
                    DeprecatedString newURL = base.urlString.left(base.portEndPos) + str;
                    parse(newURL.ascii(), &newURL);
                }
                break;
            }
        default:
            {
                // must be relative-path reference

                // Base part plus relative part plus one possible slash added in between plus terminating \0 byte.
                Vector<char, 2048> buffer(base.pathEndPos + 1 + strlen(str) + 1);

                char *bufferPos = buffer.data();
                
                // first copy everything before the path from the base
                const char *baseString = base.urlString.ascii();
                const char *baseStringStart = baseString;
                const char *pathStart = baseStringStart + base.portEndPos;
                while (baseStringStart < pathStart) {
                    *bufferPos++ = *baseStringStart++;
                }
                char *bufferPathStart = bufferPos;

                // now copy the base path 
                const char *baseStringEnd = baseString + base.pathEndPos;
                
                // go back to the last slash
                while (baseStringEnd > baseStringStart && baseStringEnd[-1] != '/') {
                    baseStringEnd--;
                }
                
                if (baseStringEnd == baseStringStart) {
                    // no path in base, add a path separator if necessary
                    if (base.schemeEndPos + 1 != base.pathEndPos && *str != '\0' && *str != '?' && *str != '#') {
                        *bufferPos++ = '/';
                    }
                } else {
                    bufferPos += copyPathRemovingDots(bufferPos, baseStringStart, 0, baseStringEnd - baseStringStart);
                }

                const char *relStringStart = str;
                const char *relStringPos = relStringStart;
                
                while (*relStringPos != '\0' && *relStringPos != '?' && *relStringPos != '#') {
                    if (relStringPos[0] == '.' && bufferPos[-1] == '/') {
                        if (isPathSegmentEndChar(relStringPos[1])) {
                            // skip over "." segment
                            relStringPos += 1;
                            if (relStringPos[0] == '/') {
                                relStringPos++;
                            }
                            continue;
                        } else if (relStringPos[1] == '.' && isPathSegmentEndChar(relStringPos[2])) {
                            // skip over ".." segment and rewind the last segment
                            // the RFC leaves it up to the app to decide what to do with excess
                            // ".." segments - we choose to drop them since some web content
                            // relies on this.
                            relStringPos += 2;
                            if (relStringPos[0] == '/') {
                                relStringPos++;
                            }
                            if (bufferPos > bufferPathStart + 1) {
                                bufferPos--;
                            }
                            while (bufferPos > bufferPathStart + 1  && bufferPos[-1] != '/') {
                                bufferPos--;
                            }
                            continue;
                        }
                    }
                    
                    *bufferPos = *relStringPos;
                    relStringPos++;
                    bufferPos++;
                }

                // all done with the path work, now copy any remainder
                // of the relative reference; this will also add a null terminator
                strcpy(bufferPos, relStringPos);

                parse(buffer.data(), 0);
                
                ASSERT(strlen(buffer.data()) + 1 <= buffer.size());
                break;
            }
        }
    }
    
    if (strBuffer) {
        fastFree(strBuffer);
    }
}

bool KURL::hasPath() const
{
    return m_isValid && pathEndPos != portEndPos;
}

DeprecatedString KURL::lastPathComponent() const
{
    if (!hasPath())
        return DeprecatedString();

    int end = pathEndPos - 1;
    if (urlString[end] == '/')
        --end;

    int start = urlString.findRev('/', end);
    if (start < portEndPos)
        return DeprecatedString();
    ++start;

    return urlString.mid(start, end - start + 1);
}

DeprecatedString KURL::protocol() const
{
    if (!m_isValid) {
        return DeprecatedString();
    }

    return urlString.left(schemeEndPos);
}

DeprecatedString KURL::host() const
{
    if (!m_isValid) {
        return DeprecatedString();
    }

    int start = (passwordEndPos == userStartPos) ? passwordEndPos : passwordEndPos + 1;
    return decode_string(urlString.mid(start, hostEndPos - start));
}

unsigned short int KURL::port() const
{
    if (!m_isValid) {
        return 0;
    }

    if (hostEndPos != portEndPos) {
        bool ok;
        unsigned short result = urlString.mid(hostEndPos + 1, portEndPos - hostEndPos - 1).toUShort(&ok);
        if (!ok) {
            result = 0;
        }
        return result;
    }

    return 0;
}

DeprecatedString KURL::pass() const
{
    if (!m_isValid) {
        return DeprecatedString();
    }

    if (passwordEndPos == userEndPos) {
        return DeprecatedString();
    }

    return decode_string(urlString.mid(userEndPos + 1, passwordEndPos - userEndPos - 1)); 
}

DeprecatedString KURL::user() const
{
    if (!m_isValid) {
        return DeprecatedString();
    }

    return decode_string(urlString.mid(userStartPos, userEndPos - userStartPos));
}

DeprecatedString KURL::ref() const
{
    if (!m_isValid || fragmentEndPos == queryEndPos) {
        return DeprecatedString();
    }

    return urlString.mid(queryEndPos + 1, fragmentEndPos - (queryEndPos + 1));
}

bool KURL::hasRef() const
{
    return m_isValid && fragmentEndPos != queryEndPos;
}

DeprecatedString KURL::query() const
{
    if (!m_isValid) {
        return DeprecatedString();
    }

    return urlString.mid(pathEndPos, queryEndPos - pathEndPos); 
}

DeprecatedString KURL::path() const
{
    if (!m_isValid) {
        return DeprecatedString();
    }

    return decode_string(urlString.mid(portEndPos, pathEndPos - portEndPos)); 
}

void KURL::setProtocol(const DeprecatedString &s)
{
    if (!m_isValid) {
        DeprecatedString newURL = s + ":" + urlString;
        parse(newURL.ascii(), &newURL);
        return;
    }

    DeprecatedString newURL = s + urlString.mid(schemeEndPos);
    parse(newURL.ascii(), &newURL);
}

void KURL::setHost(const DeprecatedString &s)
{
    if (m_isValid) {
        bool slashSlashNeeded = userStartPos == schemeEndPos + 1;
        int hostStart = (passwordEndPos == userStartPos) ? passwordEndPos : passwordEndPos + 1;
        
        DeprecatedString newURL = urlString.left(hostStart) + (slashSlashNeeded ? "//" : DeprecatedString()) + s + urlString.mid(hostEndPos);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setPort(unsigned short i)
{
    if (m_isValid) {
        bool colonNeeded = portEndPos == hostEndPos;
        int portStart = (colonNeeded ? hostEndPos : hostEndPos + 1);
        DeprecatedString newURL = urlString.left(portStart) + (colonNeeded ? ":" : DeprecatedString()) + DeprecatedString::number(i) + urlString.mid(portEndPos);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setHostAndPort(const DeprecatedString& hostAndPort)
{
    if (m_isValid) {
        bool slashSlashNeeded = userStartPos == schemeEndPos + 1;
        int hostStart = (passwordEndPos == userStartPos) ? passwordEndPos : passwordEndPos + 1;
        
        DeprecatedString newURL = urlString.left(hostStart) + (slashSlashNeeded ? "//" : DeprecatedString()) + hostAndPort + urlString.mid(portEndPos);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setUser(const DeprecatedString &user)
{
    if (m_isValid) {
        DeprecatedString u;
        int end = userEndPos;
        if (!user.isEmpty()) {
            u = user;
            if (userStartPos == schemeEndPos + 1)
                u = "//" + u;
            // Add '@' if we didn't have one before.
            if (end == hostEndPos || (end == passwordEndPos && urlString[end] != '@'))
                u += '@';
        } else {
            // Remove '@' if we now have neither user nor password.
            if (userEndPos == passwordEndPos && end != hostEndPos && urlString[end] == '@')
                end += 1;
        }
        const DeprecatedString newURL = urlString.left(userStartPos) + u + urlString.mid(end);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setPass(const DeprecatedString &password)
{
    if (m_isValid) {
        DeprecatedString p;
        int end = passwordEndPos;
        if (!password.isEmpty()) {
            p = ':' + password + '@';
            if (userEndPos == schemeEndPos + 1)
                p = "//" + p;
            // Eat the existing '@' since we are going to add our own.
            if (end != hostEndPos && urlString[end] == '@')
                end += 1;
        } else {
            // Remove '@' if we now have neither user nor password.
            if (userStartPos == userEndPos && end != hostEndPos && urlString[end] == '@')
                end += 1;
        }
        const DeprecatedString newURL = urlString.left(userEndPos) + p + urlString.mid(end);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setRef(const DeprecatedString &s)
{
    if (m_isValid) {
        DeprecatedString newURL = urlString.left(queryEndPos) + (s.isEmpty() ? DeprecatedString() : "#" + s);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setQuery(const DeprecatedString &query)
{
    if (m_isValid) {
        DeprecatedString q;
        if (!query.isNull() && (query.isEmpty() || query[0] != '?')) {
            q = "?" + query;
        } else {
            q = query;
        }

        DeprecatedString newURL = urlString.left(pathEndPos) + q + urlString.mid(queryEndPos);
        parse(newURL.ascii(), &newURL);
    }
}

void KURL::setPath(const DeprecatedString &s)
{
    if (m_isValid) {
        DeprecatedString newURL = urlString.left(portEndPos) + encode_string(s) + urlString.mid(pathEndPos);
        parse(newURL.ascii(), &newURL);
    }
}

DeprecatedString KURL::prettyURL() const
{
    if (!m_isValid) {
        return urlString;
    }

    DeprecatedString result = protocol() + ":";

    DeprecatedString authority;

    if (hostEndPos != passwordEndPos) {
        if (userEndPos != userStartPos) {
            authority += user();
            authority += "@";
        }
        authority += host();
        if (port() != 0) {
            authority += ":";
            authority += DeprecatedString::number(port());
        }
    }

    if (!authority.isEmpty())
        result += "//" + authority;
    else if (protocol() == "file")
        result += "//";

    result += path();
    result += query();

    if (fragmentEndPos != queryEndPos) {
        result += "#" + ref();
    }

    return result;
}

DeprecatedString KURL::decode_string(const DeprecatedString& urlString)
{
    return decode_string(urlString, UTF8Encoding());
}

DeprecatedString KURL::decode_string(const DeprecatedString& urlString, const TextEncoding& encoding)
{
    DeprecatedString result("");

    Vector<char, 2048> buffer(0);

    int length = urlString.length();
    int decodedPosition = 0;
    int searchPosition = 0;
    int encodedRunPosition;
    while ((encodedRunPosition = urlString.find('%', searchPosition)) >= 0) {
        // Find the sequence of %-escape codes.
        int encodedRunEnd = encodedRunPosition;
        while (length - encodedRunEnd >= 3
                && urlString[encodedRunEnd] == '%'
                && isHexDigit(urlString[encodedRunEnd + 1].latin1())
                && isHexDigit(urlString[encodedRunEnd + 2].latin1()))
            encodedRunEnd += 3;
        if (encodedRunEnd == encodedRunPosition) {
            ++searchPosition;
            continue;
        }
        searchPosition = encodedRunEnd;

        // Copy the entire %-escape sequence into an 8-bit buffer.
        int encodedRunLength = encodedRunEnd - encodedRunPosition;
        buffer.clear();
        buffer.grow(encodedRunLength + 1);
        urlString.copyLatin1(buffer.data(), encodedRunPosition, encodedRunLength);

        // Decode the %-escapes into bytes.
        char *p = buffer.data();
        const char *q = buffer.data();
        while (*q) {
            *p++ = (hexDigitValue(q[1]) << 4) | hexDigitValue(q[2]);
            q += 3;
        }

        // Decode the bytes into Unicode characters.
        String decoded = (encoding.isValid() ? encoding : UTF8Encoding()).decode(buffer.data(), p - buffer.data());
        if (decoded.isEmpty())
            continue;

        // Build up the string with what we just skipped and what we just decoded.
        result.append(urlString.mid(decodedPosition, encodedRunPosition - decodedPosition));
        result.append(reinterpret_cast<const DeprecatedChar*>(decoded.characters()), decoded.length());
        decodedPosition = encodedRunEnd;
    }

    result.append(urlString.mid(decodedPosition, length - decodedPosition));
    return result;
}

bool KURL::isLocalFile() const
{
    // Including feed here might be a bad idea since drag and drop uses this check
    // and including feed would allow feeds to potentially let someone's blog
    // read the contents of the clipboard on a drag, even without a drop.
    // Likewise with using the FrameLoader::shouldTreatURLAsLocal() function.
    return equalIgnoringCase(protocol(), "file");
}

static void appendEscapingBadChars(char*& buffer, const char *strStart, size_t length)
{
    char *p = buffer;

    const char *str = strStart;
    const char *strEnd = strStart + length;
    while (str < strEnd) {
        unsigned char c = *str++;
        if (isBadChar(c)) {
            if (c == '%' || c == '?') {
                *p++ = c;
            } else if (c != 0x09 && c != 0x0a && c != 0x0d) {
                *p++ = '%';
                *p++ = hexDigits[c >> 4];
                *p++ = hexDigits[c & 0xF];
            }
        } else {
            *p++ = c;
        }
    }
    
    buffer = p;
}

// copy a path, accounting for "." and ".." segments
static int copyPathRemovingDots(char *dst, const char *src, int srcStart, int srcEnd)
{
    char *bufferPathStart = dst;

    // empty path is a special case, and need not have a leading slash
    if (srcStart != srcEnd) {
        const char *baseStringStart = src + srcStart;
        const char *baseStringEnd = src + srcEnd;
        const char *baseStringPos = baseStringStart;

        // this code is unprepared for paths that do not begin with a
        // slash and we should always have one in the source string
        ASSERT(baseStringPos[0] == '/');

        // copy the leading slash into the destination
        *dst = *baseStringPos;
        baseStringPos++;
        dst++;

        while (baseStringPos < baseStringEnd) {
            if (baseStringPos[0] == '.' && dst[-1] == '/') {
                if (baseStringPos[1] == '/' || baseStringPos + 1 == baseStringEnd) {
                    // skip over "." segment
                    baseStringPos += 2;
                    continue;
                } else if (baseStringPos[1] == '.' && (baseStringPos[2] == '/' ||
                                       baseStringPos + 2 == baseStringEnd)) {
                    // skip over ".." segment and rewind the last segment
                    // the RFC leaves it up to the app to decide what to do with excess
                    // ".." segments - we choose to drop them since some web content
                    // relies on this.
                    baseStringPos += 3;
                    if (dst > bufferPathStart + 1) {
                        dst--;
                    }
                    // Note that these two while blocks differ subtly.
                    // The first helps to remove multiple adjoining slashes as we rewind.
                    // The +1 to bufferPathStart in the first while block prevents eating a leading slash
                    while (dst > bufferPathStart + 1 && dst[-1] == '/') {
                        dst--;
                    }
                    while (dst > bufferPathStart && dst[-1] != '/') {
                        dst--;
                    }
                    continue;
                }
            }

            *dst = *baseStringPos;
            baseStringPos++;
            dst++;
        }
    }
    *dst = '\0';
    return dst - bufferPathStart;
}

static inline bool hasSlashDotOrDotDot(const char *str)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(str);
    if (!*p)
        return false;
    unsigned char pc = *p;
    while (unsigned char c = *++p) {
        if (c == '.' && (pc == '/' || pc == '.'))
            return true;
        pc = c;
    }
    return false;
}

static inline bool matchLetter(char c, char lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

void KURL::parse(const char *url, const DeprecatedString *originalString)
{
    m_isValid = true;

    if (!url || url[0] == '\0') {
        // valid URL must be non-empty
        m_isValid = false;
        urlString = url;
        return;
    }

    if (!isSchemeFirstChar(url[0])) {
        // scheme must start with an alphabetic character
        m_isValid = false;
        urlString = url;
        return;
    }

    int schemeEnd = 0;
 
    while (isSchemeChar(url[schemeEnd])) {
        schemeEnd++;
    }

    if (url[schemeEnd] != ':') {
        m_isValid = false;
        urlString = url;
        return;
    }

    int userStart = schemeEnd + 1;
    int userEnd;
    int passwordStart;
    int passwordEnd;
    int hostStart;
    int hostEnd;
    int portStart;
    int portEnd;

    bool hierarchical = url[schemeEnd + 1] == '/';

    if (hierarchical && url[schemeEnd + 2] == '/') {
        // part after the scheme must be a net_path, parse the authority section

        // FIXME: authority characters may be scanned twice
        userStart += 2;
        userEnd = userStart;

        int colonPos = 0;
        while (isUserInfoChar(url[userEnd])) {
            if (url[userEnd] == ':' && colonPos == 0) {
                colonPos = userEnd;
            }
            userEnd++;
        }
        
        if (url[userEnd] == '@') {
            // actual end of the userinfo, start on the host
            if (colonPos != 0) {
                passwordEnd = userEnd;
                userEnd = colonPos;
                passwordStart = colonPos + 1;
            } else {
                passwordStart = passwordEnd = userEnd;
            }
            hostStart = passwordEnd + 1;
        } else if (url[userEnd] == '[' || isPathSegmentEndChar(url[userEnd])) {
            // hit the end of the authority, must have been no user
            // or looks like an IPv6 hostname
            // either way, try to parse it as a hostname
            userEnd = userStart;
            passwordStart = passwordEnd = userEnd;
            hostStart = userStart;
        } else {
            // invalid character
            m_isValid = false;
            urlString = url;
            return;
        }

        hostEnd = hostStart;

        // IPV6 IP address
        if (url[hostEnd] == '[') {
            hostEnd++;
            while (isIPv6Char(url[hostEnd])) {
                hostEnd++;
            }
            if (url[hostEnd] == ']') {
                hostEnd++;
            } else {
                // invalid character
                m_isValid = false;
                urlString = url;
                return;
            }
        } else {
            while (isHostnameChar(url[hostEnd])) {
                hostEnd++;
            }
        }
        
        if (url[hostEnd] == ':') {
            portStart = portEnd = hostEnd + 1;
 
            // possible start of port
            portEnd = portStart;
            while (isASCIIDigit(url[portEnd])) {
                portEnd++;
            }
        } else {
            portStart = portEnd = hostEnd;
        }

        if (!isPathSegmentEndChar(url[portEnd])) {
            // invalid character
            m_isValid = false;
            urlString = url;
            return;
        }
    } else {
        // the part after the scheme must be an opaque_part or an abs_path
        userEnd = userStart;
        passwordStart = passwordEnd = userEnd;
        hostStart = hostEnd = passwordEnd;
        portStart = portEnd = hostEnd;
    }
        
    int pathStart = portEnd;
    int pathEnd = pathStart;
    int queryStart;
    int queryEnd;
    int fragmentStart;
    int fragmentEnd;

    if (!hierarchical) {
        while (url[pathEnd] != '\0' && url[pathEnd] != '?' && url[pathEnd] != '#')
            pathEnd++;
        
        queryStart = pathEnd;
        queryEnd = queryStart;
        if (url[queryStart] == '?') {
            while (url[queryEnd] != '\0' && url[queryEnd] != '#')
                queryEnd++;
        }
        
        fragmentStart = queryEnd;
        fragmentEnd = fragmentStart;
        if (url[fragmentStart] == '#') {
            fragmentStart++;
            fragmentEnd = fragmentStart;
            while (url[fragmentEnd] != '\0')
                fragmentEnd++;
        }        
    }
    else {
        while (url[pathEnd] != '\0' && url[pathEnd] != '?' && url[pathEnd] != '#') {
            pathEnd++;
        }
    
        queryStart = pathEnd;
        queryEnd = queryStart;
        if (url[queryStart] == '?') {
            while (url[queryEnd] != '\0' && url[queryEnd] != '#') {
                queryEnd++;
            }
        }
        
        fragmentStart = queryEnd;
        fragmentEnd = fragmentStart;
        if (url[fragmentStart] == '#') {
            fragmentStart++;
            fragmentEnd = fragmentStart;
            while (url[fragmentEnd] != '\0') {
                fragmentEnd++;
            }
        }
    }

    // assemble it all, remembering the real ranges

    Vector<char, 4096> buffer(fragmentEnd * 3 + 1);

    char *p = buffer.data();
    const char *strPtr = url;

    // copy in the scheme
    const char *schemeEndPtr = url + schemeEnd;
    while (strPtr < schemeEndPtr) {
        *p++ = *strPtr++;
    }
    schemeEndPos = p - buffer.data();

    // Check if we're http or https.
    bool isHTTPorHTTPS = matchLetter(url[0], 'h')
        && matchLetter(url[1], 't')
        && matchLetter(url[2], 't')
        && matchLetter(url[3], 'p')
        && (url[4] == ':'
            || (matchLetter(url[4], 's') && url[5] == ':'));

    bool hostIsLocalHost = portEnd - userStart == 9
        && matchLetter(url[userStart], 'l')
        && matchLetter(url[userStart+1], 'o')
        && matchLetter(url[userStart+2], 'c')
        && matchLetter(url[userStart+3], 'a')
        && matchLetter(url[userStart+4], 'l')
        && matchLetter(url[userStart+5], 'h')
        && matchLetter(url[userStart+6], 'o')
        && matchLetter(url[userStart+7], 's')
        && matchLetter(url[userStart+8], 't');

    bool isFile = matchLetter(url[0], 'f')
        && matchLetter(url[1], 'i')
        && matchLetter(url[2], 'l')
        && matchLetter(url[3], 'e')
        && url[4] == ':';
        
    // File URLs need a host part unless it is just file:// or file://localhost
    bool degenFilePath = pathStart == pathEnd
        && (hostStart == hostEnd
            || hostIsLocalHost);
    
    bool haveNonHostAuthorityPart = userStart != userEnd || passwordStart != passwordEnd || portStart != portEnd;

    // add ":" after scheme
    *p++ = ':';

    // if we have at least one authority part or a file URL - add "//" and authority
    if (isFile ? !degenFilePath
               : (haveNonHostAuthorityPart || hostStart != hostEnd)) {

//if ((isFile && !degenFilePath) || haveNonHostAuthorityPart || hostStart != hostEnd) {
// still adds // for file://localhost, file://

//if (!(isFile && degenFilePath) && (haveNonHostAuthorityPart || hostStart != hostEnd)) {
//doesn't add // for things like file:///foo

        *p++ = '/';
        *p++ = '/';

        userStartPos = p - buffer.data();

        // copy in the user
        strPtr = url + userStart;
        const char *userEndPtr = url + userEnd;
        while (strPtr < userEndPtr) {
            *p++ = *strPtr++;
        }
        userEndPos = p - buffer.data();
        
        // copy in the password
        if (passwordEnd != passwordStart) {
            *p++ = ':';
            strPtr = url + passwordStart;
            const char *passwordEndPtr = url + passwordEnd;
            while (strPtr < passwordEndPtr) {
                *p++ = *strPtr++;
            }
        }
        passwordEndPos = p - buffer.data();
        
        // If we had any user info, add "@"
        if (p - buffer.data() != userStartPos) {
            *p++ = '@';
        }
        
        // copy in the host, except in the case of a file URL with authority="localhost"
        if (!(isFile && hostIsLocalHost && !haveNonHostAuthorityPart)) {
            strPtr = url + hostStart;
            const char *hostEndPtr = url + hostEnd;
            while (strPtr < hostEndPtr) {
                *p++ = *strPtr++;
            }
        }
        hostEndPos = p - buffer.data();
        
        // copy in the port
        if (hostEnd != portStart) {
            *p++ = ':';
            strPtr = url + portStart;
            const char *portEndPtr = url + portEnd;
            while (strPtr < portEndPtr) {
                *p++ = *strPtr++;
            }
        }
        portEndPos = p - buffer.data();
    } else {
        userStartPos = userEndPos = passwordEndPos = hostEndPos = portEndPos = p - buffer.data();
    }

    // For canonicalization, ensure we have a '/' for no path.
    // Only do this for http and https.
    if (isHTTPorHTTPS && pathEnd - pathStart == 0) {
        *p++ = '/';
    }
       
    // add path, escaping bad characters
    
    if (hierarchical && hasSlashDotOrDotDot(url)) {
        Vector<char, 4096> path_buffer(pathEnd - pathStart + 1);
        copyPathRemovingDots(path_buffer.data(), url, pathStart, pathEnd);
        appendEscapingBadChars(p, path_buffer.data(), strlen(path_buffer.data()));
    } else
        appendEscapingBadChars(p, url + pathStart, pathEnd - pathStart);

    pathEndPos = p - buffer.data();
    
    
    // add query, escaping bad characters
    appendEscapingBadChars(p, url + queryStart, queryEnd - queryStart);
    queryEndPos = p - buffer.data();
    
    // add fragment, escaping bad characters
    if (fragmentEnd != queryEnd) {
        *p++ = '#';
        appendEscapingBadChars(p, url + fragmentStart, fragmentEnd - fragmentStart);
    }
    fragmentEndPos = p - buffer.data();

    // If we didn't end up actually changing the original string and
    // it started as a DeprecatedString, just reuse it, to avoid extra
    // allocation.
    if (originalString && strncmp(buffer.data(), url, fragmentEndPos) == 0) {
        urlString = *originalString;
    } else
        urlString = DeprecatedString(buffer.data(), fragmentEndPos);

    ASSERT(p - buffer.data() <= (int)buffer.size());
}

bool operator==(const KURL &a, const KURL &b)
{
    return a.urlString == b.urlString;
}

bool equalIgnoringRef(const KURL& a, const KURL& b)
{
    return a.urlString.left(a.queryEndPos) == b.urlString.left(b.queryEndPos);
}

DeprecatedString KURL::encode_string(const DeprecatedString& notEncodedString)
{
    DeprecatedCString asUTF8 = notEncodedString.utf8();
    
    Vector<char, 4096> buffer(asUTF8.length() * 3 + 1);
    char *p = buffer.data();

    const char *str = asUTF8;
    const char *strEnd = str + asUTF8.length();
    while (str < strEnd) {
        unsigned char c = *str++;
        if (isBadChar(c)) {
            *p++ = '%';
            *p++ = hexDigits[c >> 4];
            *p++ = hexDigits[c & 0xF];
        } else
            *p++ = c;
    }
    
    DeprecatedString result(buffer.data(), p - buffer.data());
    
    ASSERT(p - buffer.data() <= (int)buffer.size());

    return result;
}

static DeprecatedString encodeHostname(const DeprecatedString &s)
{
    // Needs to be big enough to hold an IDN-encoded name.
    // For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
    const unsigned hostnameBufferLength = 2048;

    if (s.isAllASCII() || s.length() > hostnameBufferLength)
        return s;

#if USE(ICU_UNICODE)
    UChar buffer[hostnameBufferLength];    
    UErrorCode error = U_ZERO_ERROR;
    int32_t numCharactersConverted = uidna_IDNToASCII
        (reinterpret_cast<const UChar *>(s.unicode()), s.length(), buffer, hostnameBufferLength, UIDNA_ALLOW_UNASSIGNED, 0, &error);
    if (error != U_ZERO_ERROR) {
        return s;
    }
    return DeprecatedString(reinterpret_cast<DeprecatedChar *>(buffer), numCharactersConverted);
#elif USE(QT4_UNICODE)
    QByteArray result = QUrl::toAce(s);
    return DeprecatedString(result.constData(), result.length());
#endif
}

static Vector<pair<int, int> > findHostnamesInMailToURL(const DeprecatedString &s)
{
    // In a mailto: URL, host names come after a '@' character and end with a '>' or ',' or '?' or end of string character.
    // Skip quoted strings so that characters in them don't confuse us.
    // When we find a '?' character, we are past the part of the URL that contains host names.

    Vector<pair<int, int> > a;

    int p = 0;
    while (1) {
        // Find start of host name or of quoted string.
        int hostnameOrStringStart = s.find(RegularExpression("[\"@?]"), p);
        if (hostnameOrStringStart == -1) {
            return a;
        }
        DeprecatedChar c = s[hostnameOrStringStart];
        p = hostnameOrStringStart + 1;

        if (c == '?') {
            return a;
        }
        
        if (c == '@') {
            // Find end of host name.
            int hostnameStart = p;
            int hostnameEnd = s.find(RegularExpression("[>,?]"), p);
            bool done;
            if (hostnameEnd == -1) {
                hostnameEnd = s.length();
                done = true;
            } else {
                p = hostnameEnd;
                done = false;
            }

            a.append(make_pair(hostnameStart, hostnameEnd));

            if (done)
                return a;
        } else {
            // Skip quoted string.
            ASSERT(c == '"');
            while (1) {
                int escapedCharacterOrStringEnd = s.find(RegularExpression("[\"\\]"), p);
                if (escapedCharacterOrStringEnd == -1)
                    return a;

                c = s[escapedCharacterOrStringEnd];
                p = escapedCharacterOrStringEnd + 1;
                
                // If we are the end of the string, then break from the string loop back to the host name loop.
                if (c == '"')
                    break;
                
                // Skip escaped character.
                ASSERT(c == '\\');
                if (p == static_cast<int>(s.length()))
                    return a;

                ++p;
            }
        }
    }
}

static bool findHostnameInHierarchicalURL(const DeprecatedString &s, int &startOffset, int &endOffset)
{
    // Find the host name in a hierarchical URL.
    // It comes after a "://" sequence, with scheme characters preceding.
    // If ends with the end of the string or a ":" or a path segment ending character.
    // If there is a "@" character, the host part is just the part after the "@".
    int separator = s.find("://");
    if (separator <= 0) {
        return false;
    }

    // Check that all characters before the :// are valid scheme characters.
    if (!isSchemeFirstChar(s[0].latin1())) {
        return false;
    }
    for (int i = 1; i < separator; ++i) {
        if (!isSchemeChar(s[i].latin1())) {
            return false;
        }
    }

    // Start after the separator.
    int authorityStart = separator + 3;

    // Find terminating character.
    int length = s.length();
    int hostnameEnd = length;
    for (int i = authorityStart; i < length; ++i) {
        char c = s[i].latin1();
        if (c == ':' || (isPathSegmentEndChar(c) && c != '\0')) {
            hostnameEnd = i;
            break;
        }
    }

    // Find "@" for the start of the host name.
    int userInfoTerminator = s.find('@', authorityStart);
    int hostnameStart;
    if (userInfoTerminator == -1 || userInfoTerminator > hostnameEnd) {
        hostnameStart = authorityStart;
    } else {
        hostnameStart = userInfoTerminator + 1;
    }

    startOffset = hostnameStart;
    endOffset = hostnameEnd;
    return true;
}

static DeprecatedString encodeHostnames(const DeprecatedString &s)
{
    if (s.startsWith("mailto:", false)) {
        const Vector<pair<int, int> > hostnameRanges = findHostnamesInMailToURL(s);
        int n = hostnameRanges.size();
        if (n != 0) {
            DeprecatedString result;
            unsigned p = 0;
            for (int i = 0; i < n; ++i) {
                const pair<int, int> &r = hostnameRanges[i];
                result += s.mid(p, r.first);
                result += encodeHostname(s.mid(r.first, r.second - r.first));
                p = r.second;
            }
            result += s.mid(p);
            return result;
        }
    } else {
        int hostStart, hostEnd;
        if (findHostnameInHierarchicalURL(s, hostStart, hostEnd)) {
            return s.left(hostStart) + encodeHostname(s.mid(hostStart, hostEnd - hostStart)) + s.mid(hostEnd); 
        }
    }
    return s;
}

static char *encodeRelativeString(const KURL &base, const DeprecatedString &rel, const TextEncoding& encoding)
{
    DeprecatedString s = encodeHostnames(rel);

    char *strBuffer;

    TextEncoding pathEncoding(UTF8Encoding());
    TextEncoding otherEncoding = (encoding.isValid() && !rel.startsWith("mailto:", false)) ? encoding : UTF8Encoding();
    
    int pathEnd = -1;
    if (pathEncoding != otherEncoding) {
        pathEnd = s.find(RegularExpression("[?#]"));
    }
    if (pathEnd == -1) {
        CString decoded = pathEncoding.encode(reinterpret_cast<const UChar*>(s.unicode()), s.length());
        int decodedLength = decoded.length();
        strBuffer = static_cast<char *>(fastMalloc(decodedLength + 1));
        memcpy(strBuffer, decoded.data(), decodedLength);
        strBuffer[decodedLength] = 0;
    } else {
        int length = s.length();
        CString pathDecoded = pathEncoding.encode(reinterpret_cast<const UChar*>(s.unicode()), pathEnd);
        CString otherDecoded = otherEncoding.encode(reinterpret_cast<const UChar*>(s.unicode()) + pathEnd, length - pathEnd);
        int pathDecodedLength = pathDecoded.length();
        int otherDecodedLength = otherDecoded.length();
        strBuffer = static_cast<char *>(fastMalloc(pathDecodedLength + otherDecodedLength + 1));
        memcpy(strBuffer, pathDecoded.data(), pathDecodedLength);
        memcpy(strBuffer + pathDecodedLength, otherDecoded.data(), otherDecodedLength);
        strBuffer[pathDecodedLength + otherDecodedLength] = 0;
    }

    return strBuffer;
}

static DeprecatedString substituteBackslashes(const DeprecatedString &string)
{
    int questionPos = string.find('?');
    int hashPos = string.find('#');
    unsigned pathEnd;
    
    if (hashPos >= 0 && (questionPos < 0 || questionPos > hashPos)) {
        pathEnd = hashPos;
    } else if (questionPos >= 0) {
        pathEnd = questionPos;
    } else {
        pathEnd = string.length();
    }

    return string.left(pathEnd).replace('\\','/') + string.mid(pathEnd);
}

bool KURL::isHierarchical() const
{
    if (!m_isValid)
        return false;
    ASSERT(urlString[schemeEndPos] == ':');
    return urlString[schemeEndPos + 1] == '/';
}

#ifndef NDEBUG
void KURL::print() const
{
    printf("%s\n", urlString.ascii());
}
#endif

}
