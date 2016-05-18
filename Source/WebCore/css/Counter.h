/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSPrimitiveValue.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class Counter : public RefCounted<Counter> {
public:
    static Ref<Counter> create(RefPtr<CSSPrimitiveValue>&& identifier, RefPtr<CSSPrimitiveValue>&& listStyle, RefPtr<CSSPrimitiveValue>&& separator)
    {
        return adoptRef(*new Counter(WTFMove(identifier), WTFMove(listStyle), WTFMove(separator)));
    }

    String identifier() const { return m_identifier ? m_identifier->getStringValue() : String(); }
    String listStyle() const { return m_listStyle ? m_listStyle->getStringValue() : String(); }
    String separator() const { return m_separator ? m_separator->getStringValue() : String(); }

    CSSValueID listStyleIdent() const { return m_listStyle ? m_listStyle->getValueID() : CSSValueInvalid; }

    void setIdentifier(RefPtr<CSSPrimitiveValue>&& identifier) { m_identifier = WTFMove(identifier); }
    void setListStyle(RefPtr<CSSPrimitiveValue>&& listStyle) { m_listStyle = WTFMove(listStyle); }
    void setSeparator(RefPtr<CSSPrimitiveValue>&& separator) { m_separator = WTFMove(separator); }

    bool equals(const Counter& other) const
    {
        return identifier() == other.identifier()
            && listStyle() == other.listStyle()
            && separator() == other.separator();
    }
    
    Ref<Counter> cloneForCSSOM() const
    {
        return create(m_identifier ? m_identifier->cloneForCSSOM() : nullptr
            , m_listStyle ? m_listStyle->cloneForCSSOM() : nullptr
            , m_separator ? m_separator->cloneForCSSOM() : nullptr);
    }

private:
    Counter(RefPtr<CSSPrimitiveValue>&& identifier, RefPtr<CSSPrimitiveValue>&& listStyle, RefPtr<CSSPrimitiveValue>&& separator)
        : m_identifier(WTFMove(identifier))
        , m_listStyle(WTFMove(listStyle))
        , m_separator(WTFMove(separator))
    {
    }

    RefPtr<CSSPrimitiveValue> m_identifier; // string
    RefPtr<CSSPrimitiveValue> m_listStyle; // ident
    RefPtr<CSSPrimitiveValue> m_separator; // string
};

} // namespace WebCore
