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

#include "config.h"
#include "SegmentedString.h"

namespace WebCore {

SegmentedString::SegmentedString(const SegmentedString &other) :
    m_pushedChar1(other.m_pushedChar1), m_pushedChar2(other.m_pushedChar2), m_currentString(other.m_currentString),
    m_substrings(other.m_substrings), m_lines(other.m_lines), m_composite(other.m_composite)
{
    if (other.m_currentChar == &other.m_pushedChar1)
        m_currentChar = &m_pushedChar1;
    else if (other.m_currentChar == &other.m_pushedChar2)
        m_currentChar = &m_pushedChar2;
    else
        m_currentChar = other.m_currentChar;
}

const SegmentedString& SegmentedString::operator=(const SegmentedString &other)
{
    m_pushedChar1 = other.m_pushedChar1;
    m_pushedChar2 = other.m_pushedChar2;
    m_currentString = other.m_currentString;
    m_substrings = other.m_substrings;
    m_lines = other.m_lines;
    m_composite = other.m_composite;
    if (other.m_currentChar == &other.m_pushedChar1)
        m_currentChar = &m_pushedChar1;
    else if (other.m_currentChar == &other.m_pushedChar2)
        m_currentChar = &m_pushedChar2;
    else
        m_currentChar = other.m_currentChar;
    return *this;
}

uint SegmentedString::length() const
{
    uint length = m_currentString.m_length;
    if (!m_pushedChar1.isNull()) {
        ++length;
        if (!m_pushedChar2.isNull())
            ++length;
    }
    if (m_composite) {
        QValueListConstIterator<SegmentedSubstring> i = m_substrings.begin();
        QValueListConstIterator<SegmentedSubstring> e = m_substrings.end();
        for (; i != e; ++i)
            length += (*i).m_length;
    }
    return length;
}

void SegmentedString::clear()
{
    m_pushedChar1 = 0;
    m_pushedChar2 = 0;
    m_currentChar = 0;
    m_currentString.clear();
    m_substrings.clear();
    m_lines = 0;
    m_composite = false;
}

void SegmentedString::append(const SegmentedSubstring &s)
{
    if (s.m_length) {
        if (!m_currentString.m_length) {
            m_currentString = s;
        } else {
            m_substrings.append(s);
            m_composite = true;
        }
    }
}

void SegmentedString::prepend(const SegmentedSubstring &s)
{
    assert(!escaped());
    if (s.m_length) {
        if (!m_currentString.m_length)
            m_currentString = s;
        else {
            // Shift our m_currentString into our list.
            m_substrings.prepend(m_currentString);
            m_currentString = s;
            m_composite = true;
        }
    }
}

void SegmentedString::append(const SegmentedString &s)
{
    assert(!s.escaped());
    append(s.m_currentString);
    if (s.m_composite) {
        QValueListConstIterator<SegmentedSubstring> i = s.m_substrings.begin();
        QValueListConstIterator<SegmentedSubstring> e = s.m_substrings.end();
        for (; i != e; ++i)
            append(*i);
    }
    m_currentChar = m_pushedChar1.isNull() ? m_currentString.m_current : &m_pushedChar1;
}

void SegmentedString::prepend(const SegmentedString &s)
{
    assert(!escaped());
    assert(!s.escaped());
    if (s.m_composite) {
        QValueListConstIterator<SegmentedSubstring> i = s.m_substrings.fromLast();
        QValueListConstIterator<SegmentedSubstring> e = s.m_substrings.end();
        for (; i != e; --i)
            prepend(*i);
    }
    prepend(s.m_currentString);
    m_currentChar = m_pushedChar1.isNull() ? m_currentString.m_current : &m_pushedChar1;
}

void SegmentedString::advanceSubstring()
{
    if (m_composite) {
        m_currentString = m_substrings.first();
        m_substrings.remove(m_substrings.begin());
        if (m_substrings.isEmpty())
            m_composite = false;
    } else {
        m_currentString.clear();
    }
}

QString SegmentedString::toString() const
{
    QString result;
    if (!m_pushedChar1.isNull()) {
        result.append(m_pushedChar1);
        if (!m_pushedChar2.isNull())
            result.append(m_pushedChar2);
    }
    m_currentString.appendTo(result);
    if (m_composite) {
        QValueListConstIterator<SegmentedSubstring> i = m_substrings.begin();
        QValueListConstIterator<SegmentedSubstring> e = m_substrings.end();
        for (; i != e; ++i)
            (*i).appendTo(result);
    }
    return result;
}

}
