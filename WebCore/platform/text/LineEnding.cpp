/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LineEnding.h"

#include "PlatformString.h"
#include <wtf/text/CString.h>

namespace WebCore {

// Normalize all line-endings to CRLF.
CString normalizeLineEndingsToCRLF(const CString& from)
{
    size_t newLen = 0;
    const char* p = from.data();
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p != '\n') {
                // Turn CR into CRLF.
                newLen += 2;
            }
        } else if (c == '\n') {
            // Turn LF into CRLF.
            newLen += 2;
        } else {
            // Leave other characters alone.
            newLen += 1;
        }
    }
    if (newLen == from.length())
        return from;
    if (newLen < from.length())
        return CString();

    // Make a copy of the string.
    p = from.data();
    char* q;
    CString result = CString::newUninitialized(newLen, q);
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p != '\n') {
                // Turn CR into CRLF.
                *q++ = '\r';
                *q++ = '\n';
            }
        } else if (c == '\n') {
            // Turn LF into CRLF.
            *q++ = '\r';
            *q++ = '\n';
        } else {
            // Leave other characters alone.
            *q++ = c;
        }
    }
    return result;
}

// Normalize all line-endings to CR or LF.
static CString normalizeToCROrLF(const CString& from, bool toCR)
{
    size_t newLen = 0;
    bool needFix = false;
    const char* p = from.data();
    char fromEndingChar = toCR ? '\n' : '\r';
    char toEndingChar = toCR ? '\r' : '\n';
    while (char c = *p++) {
        if (c == '\r' && *p == '\n') {
            // Turn CRLF into CR or LF.
            p++;
            needFix = true;
        } else if (c == fromEndingChar) {
            // Turn CR/LF into LF/CR.
            needFix = true;
        }
        newLen += 1;
    }
    if (!needFix)
        return from;

    // Make a copy of the string.
    p = from.data();
    char* q;
    CString result = CString::newUninitialized(newLen, q);
    while (char c = *p++) {
        if (c == '\r' && *p == '\n') {
            // Turn CRLF or CR into CR or LF.
            p++;
            *q++ = toEndingChar;
        } else if (c == fromEndingChar) {
            // Turn CR/LF into LF/CR.
            *q++ = toEndingChar;
        } else {
            // Leave other characters alone.
            *q++ = c;
        }
    }
    return result;
}

// Normalize all line-endings to CR.
CString normalizeLineEndingsToCR(const CString& from)
{
    return normalizeToCROrLF(from, true);
}

// Normalize all line-endings to LF.
CString normalizeLineEndingsToLF(const CString& from)
{
    return normalizeToCROrLF(from, false);
}

CString normalizeLineEndingsToNative(const CString& from)
{
#if OS(WINDOWS)
    return normalizeLineEndingsToCRLF(from);
#else
    return normalizeLineEndingsToLF(from);
#endif
}

} // namespace WebCore
