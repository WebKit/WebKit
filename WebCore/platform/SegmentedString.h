/*
    This file is part of the KDE libraries

    Copyright (C) 2004-6 Apple Computer

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
//----------------------------------------------------------------------------
//
// KDE HTML Widget -- String class

#ifndef KHTMLSTRING_H
#define KHTMLSTRING_H

#include "PlatformString.h"
#include <assert.h>
#include "DeprecatedValueList.h"

namespace WebCore
{

class SegmentedString;

class SegmentedSubstring
{
private:
    friend class SegmentedString;
    
    SegmentedSubstring() : m_length(0), m_current(0) {}
    SegmentedSubstring(const DeprecatedString &str) : m_string(str), m_length(str.length()) {
        m_current = m_length == 0 ? 0 : m_string.stableUnicode();
    }

    SegmentedSubstring(const QChar *str, int length) : m_length(length), m_current(length == 0 ? 0 : str) {}

    void clear() { m_length = 0; m_current = 0; }
    
    void appendTo(DeprecatedString &str) const {
        if (m_string.unicode() == m_current) {
            if (str.isEmpty())
                str = m_string;
            else
                str.append(m_string);
        } else {
            str.insert(str.length(), m_current, m_length);
        }
    }

    DeprecatedString m_string;
    int m_length;
    const QChar *m_current;
};

class SegmentedString
{
public:
    SegmentedString() : m_currentChar(0), m_lines(0), m_composite(false) {}
    SegmentedString(const QChar *str, int length) : m_currentString(str, length), m_currentChar(m_currentString.m_current), m_lines(0), m_composite(false) {}
    SegmentedString(const DeprecatedString &str) : m_currentString(str), m_currentChar(m_currentString.m_current), m_lines(0), m_composite(false) {}
    SegmentedString(const SegmentedString&);

    const SegmentedString& operator=(const SegmentedString&);

    void clear();

    void append(const SegmentedString &);
    void prepend(const SegmentedString &);
    
    void push(QChar c) {
        if (m_pushedChar1.isNull()) {
            m_pushedChar1 = c;
            m_currentChar = m_pushedChar1.isNull() ? m_currentString.m_current : &m_pushedChar1;
        } else {
            assert(m_pushedChar2.isNull());
            m_pushedChar2 = c;
        }
    }
    
    bool isEmpty() const { return !current(); }
    unsigned length() const;

    void advance() {
        if (!m_pushedChar1.isNull()) {
            m_pushedChar1 = m_pushedChar2;
            m_pushedChar2 = 0;
        } else if (m_currentString.m_current) {
            m_lines += *m_currentString.m_current++ == '\n';
            if (--m_currentString.m_length == 0)
                advanceSubstring();
        }
        m_currentChar = m_pushedChar1.isNull() ? m_currentString.m_current: &m_pushedChar1;
    }
    
    bool escaped() const { return !m_pushedChar1.isNull(); }

    int lineCount() const { return m_lines; }
    void resetLineCount() { m_lines = 0; }
    
    DeprecatedString toString() const;

    void operator++() { advance(); }
    const QChar &operator*() const { return *current(); }
    const QChar *operator->() const { return current(); }
    
private:
    void append(const SegmentedSubstring &);
    void prepend(const SegmentedSubstring &);

    void advanceSubstring();
    const QChar *current() const { return m_currentChar; }

    QChar m_pushedChar1;
    QChar m_pushedChar2;
    SegmentedSubstring m_currentString;
    const QChar *m_currentChar;
    DeprecatedValueList<SegmentedSubstring> m_substrings;
    int m_lines;
    bool m_composite;
};

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::SegmentedString;

#endif
