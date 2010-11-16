/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 */

#ifndef CSSParserValues_h
#define CSSParserValues_h

#include <wtf/text/AtomicString.h>

namespace WebCore {

class CSSValue;

struct CSSParserString {
    UChar* characters;
    int length;

    void lower();

    operator String() const { return String(characters, length); }
    operator AtomicString() const { return AtomicString(characters, length); }
};

struct CSSParserFunction;

struct CSSParserValue {
    int id;
    bool isInt;
    union {
        double fValue;
        int iValue;
        CSSParserString string;
        CSSParserFunction* function;
    };
    enum {
        Operator = 0x100000,
        Function = 0x100001,
        Q_EMS    = 0x100002
    };
    int unit;
    
    
    PassRefPtr<CSSValue> createCSSValue();
};

class CSSParserValueList : public FastAllocBase {
public:
    CSSParserValueList()
        : m_current(0)
    {
    }
    ~CSSParserValueList();
    
    void addValue(const CSSParserValue&);
    void deleteValueAt(unsigned);

    unsigned size() const { return m_values.size(); }
    CSSParserValue* current() { return m_current < m_values.size() ? &m_values[m_current] : 0; }
    CSSParserValue* next() { ++m_current; return current(); }

    CSSParserValue* valueAt(unsigned i) { return i < m_values.size() ? &m_values[i] : 0; }
        
    void clear() { m_values.clear(); }

private:
    unsigned m_current;
    Vector<CSSParserValue, 4> m_values;
};

struct CSSParserFunction : FastAllocBase {
    CSSParserString name;
    OwnPtr<CSSParserValueList> args;
};

}

#endif
