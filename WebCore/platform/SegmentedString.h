/*
    This file is part of the KDE libraries

    Copyright (C) 2004, 2005, 2006 Apple Computer

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef SegmentedString_h
#define SegmentedString_h

#include "DeprecatedString.h"
#include "DeprecatedValueList.h"
#include "PlatformString.h"
#include <assert.h>

namespace WebCore {

class SegmentedString;

class SegmentedSubstring {
private:
    friend class SegmentedString;
    
    SegmentedSubstring() : m_length(0), m_current(0) {}
    SegmentedSubstring(const DeprecatedString &str) : m_string(str), m_length(str.length()) {
        m_current = m_length == 0 ? 0 : reinterpret_cast<const UChar*>(m_string.stableUnicode());
    }

    SegmentedSubstring(const UChar* str, int length) : m_length(length), m_current(length == 0 ? 0 : str) {}

    void clear() { m_length = 0; m_current = 0; }
    
    void appendTo(DeprecatedString& str) const {
        if (reinterpret_cast<const UChar*>(m_string.unicode()) == m_current) {
            if (str.isEmpty())
                str = m_string;
            else
                str.append(m_string);
        } else {
            str.insert(str.length(), reinterpret_cast<const QChar*>(m_current), m_length);
        }
    }

    DeprecatedString m_string;
    int m_length;
    const UChar* m_current;
};

class SegmentedString {
public:
    SegmentedString()
        : m_pushedChar1(0), m_pushedChar2(0), m_currentChar(0)
        , m_lines(0), m_composite(false) {}
    SegmentedString(const UChar* str, int length) : m_pushedChar1(0), m_pushedChar2(0)
        , m_currentString(str, length), m_currentChar(m_currentString.m_current)
        , m_lines(0), m_composite(false) {}
    SegmentedString(const DeprecatedString &str)
        : m_pushedChar1(0), m_pushedChar2(0), m_currentString(str)
        , m_currentChar(m_currentString.m_current)
        , m_lines(0), m_composite(false) {}
    SegmentedString(const SegmentedString&);

    const SegmentedString& operator=(const SegmentedString&);

    void clear();

    void append(const SegmentedString &);
    void prepend(const SegmentedString &);
    
    void push(UChar c) {
        if (!m_pushedChar1) {
            m_pushedChar1 = c;
            m_currentChar = m_pushedChar1 ? &m_pushedChar1 : m_currentString.m_current;
        } else {
            assert(!m_pushedChar2);
            m_pushedChar2 = c;
        }
    }
    
    bool isEmpty() const { return !current(); }
    unsigned length() const;

    void advance() {
        if (m_pushedChar1) {
            m_pushedChar1 = m_pushedChar2;
            m_pushedChar2 = 0;
        } else if (m_currentString.m_current) {
            m_lines += *m_currentString.m_current++ == '\n';
            if (--m_currentString.m_length == 0)
                advanceSubstring();
        }
        m_currentChar = m_pushedChar1 ? &m_pushedChar1 : m_currentString.m_current;
    }
    
    bool escaped() const { return m_pushedChar1; }

    int lineCount() const { return m_lines; }
    void resetLineCount() { m_lines = 0; }
    
    DeprecatedString toString() const;

    void operator++() { advance(); }
    const UChar& operator*() const { return *current(); }
    const UChar* operator->() const { return current(); }
    
private:
    void append(const SegmentedSubstring &);
    void prepend(const SegmentedSubstring &);

    void advanceSubstring();
    const UChar* current() const { return m_currentChar; }

    UChar m_pushedChar1;
    UChar m_pushedChar2;
    SegmentedSubstring m_currentString;
    const UChar* m_currentChar;
    DeprecatedValueList<SegmentedSubstring> m_substrings;
    int m_lines;
    bool m_composite;
};

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::SegmentedString;

#endif
