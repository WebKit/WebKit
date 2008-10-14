/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FormDataList.h"

namespace WebCore {

FormDataList::FormDataList(const TextEncoding& c)
    : m_encoding(c)
{
}

void FormDataList::appendString(const CString& s)
{
    m_list.append(s);
}

// Change plain CR and plain LF to CRLF pairs.
static CString fixLineBreaks(const CString& s)
{
    // Compute the length.
    unsigned newLen = 0;
    const char* p = s.data();
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
    if (newLen == s.length()) {
        return s;
    }
    
    // Make a copy of the string.
    p = s.data();
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

void FormDataList::appendString(const String& s)
{
    CString cstr = fixLineBreaks(m_encoding.encode(s.characters(), s.length(), EntitiesForUnencodables));
    m_list.append(cstr);
}

} // namespace
