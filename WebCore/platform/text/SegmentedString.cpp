/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "SegmentedString.h"

namespace WebCore {

SegmentedString::SegmentedString(const SegmentedString &other) :
    m_pushedChar1(other.m_pushedChar1), m_pushedChar2(other.m_pushedChar2), m_currentString(other.m_currentString),
    m_substrings(other.m_substrings), m_composite(other.m_composite)
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
    m_composite = other.m_composite;
    if (other.m_currentChar == &other.m_pushedChar1)
        m_currentChar = &m_pushedChar1;
    else if (other.m_currentChar == &other.m_pushedChar2)
        m_currentChar = &m_pushedChar2;
    else
        m_currentChar = other.m_currentChar;
    return *this;
}

unsigned SegmentedString::length() const
{
    unsigned length = m_currentString.m_length;
    if (m_pushedChar1) {
        ++length;
        if (m_pushedChar2)
            ++length;
    }
    if (m_composite) {
        Deque<SegmentedSubstring>::const_iterator it = m_substrings.begin();
        Deque<SegmentedSubstring>::const_iterator e = m_substrings.end();
        for (; it != e; ++it)
            length += it->m_length;
    }
    return length;
}

void SegmentedString::setExcludeLineNumbers()
{
    if (m_composite) {
        Deque<SegmentedSubstring>::iterator it = m_substrings.begin();
        Deque<SegmentedSubstring>::iterator e = m_substrings.end();
        for (; it != e; ++it)
            it->setExcludeLineNumbers();
    } else
        m_currentString.setExcludeLineNumbers();
}

void SegmentedString::clear()
{
    m_pushedChar1 = 0;
    m_pushedChar2 = 0;
    m_currentChar = 0;
    m_currentString.clear();
    m_substrings.clear();
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
    ASSERT(!escaped());
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
    ASSERT(!s.escaped());
    append(s.m_currentString);
    if (s.m_composite) {
        Deque<SegmentedSubstring>::const_iterator it = s.m_substrings.begin();
        Deque<SegmentedSubstring>::const_iterator e = s.m_substrings.end();
        for (; it != e; ++it)
            append(*it);
    }
    m_currentChar = m_pushedChar1 ? &m_pushedChar1 : m_currentString.m_current;
}

void SegmentedString::prepend(const SegmentedString &s)
{
    ASSERT(!escaped());
    ASSERT(!s.escaped());
    if (s.m_composite) {
        Deque<SegmentedSubstring>::const_reverse_iterator it = s.m_substrings.rbegin();
        Deque<SegmentedSubstring>::const_reverse_iterator e = s.m_substrings.rend();
        for (; it != e; ++it)
            prepend(*it);
    }
    prepend(s.m_currentString);
    m_currentChar = m_pushedChar1 ? &m_pushedChar1 : m_currentString.m_current;
}

void SegmentedString::advanceSubstring()
{
    if (m_composite) {
        m_currentString = m_substrings.first();
        m_substrings.removeFirst();
        if (m_substrings.isEmpty())
            m_composite = false;
    } else {
        m_currentString.clear();
    }
}

String SegmentedString::toString() const
{
    String result;
    if (m_pushedChar1) {
        result.append(m_pushedChar1);
        if (m_pushedChar2)
            result.append(m_pushedChar2);
    }
    m_currentString.appendTo(result);
    if (m_composite) {
        Deque<SegmentedSubstring>::const_iterator it = m_substrings.begin();
        Deque<SegmentedSubstring>::const_iterator e = m_substrings.end();
        for (; it != e; ++it)
            it->appendTo(result);
    }
    return result;
}

void SegmentedString::advanceSlowCase()
{
    if (m_pushedChar1) {
        m_pushedChar1 = m_pushedChar2;
        m_pushedChar2 = 0;
    } else if (m_currentString.m_current) {
        ++m_currentString.m_current;
        if (--m_currentString.m_length == 0)
            advanceSubstring();
    }
    m_currentChar = m_pushedChar1 ? &m_pushedChar1 : m_currentString.m_current;
}

void SegmentedString::advanceSlowCase(int& lineNumber)
{
    if (m_pushedChar1) {
        m_pushedChar1 = m_pushedChar2;
        m_pushedChar2 = 0;
    } else if (m_currentString.m_current) {
        if (*m_currentString.m_current++ == '\n' && m_currentString.doNotExcludeLineNumbers())
            ++lineNumber;
        if (--m_currentString.m_length == 0)
            advanceSubstring();
    }
    m_currentChar = m_pushedChar1 ? &m_pushedChar1 : m_currentString.m_current;
}

}
