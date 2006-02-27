/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "FormDataList.h"
#include "TextEncoding.h"

namespace DOM {

FormDataList::FormDataList(const TextEncoding& c)
    : m_encoding(c)
{
}

void FormDataList::appendString(const QCString &s)
{
    m_list.append(s);
}

// Change plain CR and plain LF to CRLF pairs.
static QCString fixLineBreaks(const QCString &s)
{
    // Compute the length.
    unsigned newLen = 0;
    const char *p = s.data();
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
    QCString result(newLen + 1);
    char *q = result.data();
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

void FormDataList::appendString(const QString &s)
{
    QCString cstr = fixLineBreaks(m_encoding.fromUnicode(s, true));
    cstr.truncate(cstr.length());
    m_list.append(cstr);
}

void FormDataList::appendFile(const DOMString &key, const DOMString &filename)
{
    appendString(key.qstring());
    m_list.append(filename.qstring());
}

} // namespace
