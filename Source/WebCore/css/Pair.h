/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2015 Apple Inc.
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

#ifndef Pair_h
#define Pair_h

#include <wtf/RefCounted.h>
#include "CSSPrimitiveValue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// A primitive value representing a pair.  This is useful for properties like border-radius, background-size/position,
// and border-spacing (all of which are space-separated sets of two values).  At the moment we are only using it for
// border-radius and background-size, but (FIXME) border-spacing and background-position could be converted over to use
// it (eliminating some extra -webkit- internal properties).
class Pair : public RefCounted<Pair> {
public:
    static Ref<Pair> create()
    {
        return adoptRef(*new Pair);
    }
    static Ref<Pair> create(RefPtr<CSSPrimitiveValue>&& first, RefPtr<CSSPrimitiveValue>&& second)
    {
        return adoptRef(*new Pair(WTF::move(first), WTF::move(second)));
    }
    virtual ~Pair() { }

    CSSPrimitiveValue* first() const { return m_first.get(); }
    CSSPrimitiveValue* second() const { return m_second.get(); }

    void setFirst(RefPtr<CSSPrimitiveValue>&& first) { m_first = WTF::move(first); }
    void setSecond(RefPtr<CSSPrimitiveValue>&& second) { m_second = WTF::move(second); }

    String cssText() const
    {
    
        return generateCSSString(first()->cssText(), second()->cssText());
    }

    bool equals(const Pair& other) const { return compareCSSValuePtr(m_first, other.m_first) && compareCSSValuePtr(m_second, other.m_second); }

private:
    Pair() : m_first(nullptr), m_second(nullptr) { }
    Pair(RefPtr<CSSPrimitiveValue>&& first, RefPtr<CSSPrimitiveValue>&& second) : m_first(WTF::move(first)), m_second(WTF::move(second)) { }

    static String generateCSSString(const String& first, const String& second)
    {
        if (first == second)
            return first;
        return first + ' ' + second;
    }

    RefPtr<CSSPrimitiveValue> m_first;
    RefPtr<CSSPrimitiveValue> m_second;
};

} // namespace

#endif
