/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 */

#ifndef CSSProperty_H
#define CSSProperty_H

#include "CSSValue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSProperty {
public:
    CSSProperty(int propID, PassRefPtr<CSSValue> value, bool important = false, int shorthandID = 0, bool implicit = false)
        : m_id(propID)
        , m_shorthandID(shorthandID)
        , m_important(important)
        , m_implicit(implicit)
        , m_value(value)
    {
    }
    
    CSSProperty &operator=(const CSSProperty& o)
    {
        m_id = o.m_id;
        m_shorthandID = o.m_shorthandID;
        m_important = o.m_important;
        m_value = o.m_value;
        return *this;
    }
    
    int id() const { return m_id; }
    int shorthandID() const { return m_shorthandID; }
    
    bool isImportant() const { return m_important; }
    bool isImplicit() const { return m_implicit; }

    CSSValue* value() const { return m_value.get(); }
    
    String cssText() const;

    // make sure the following fits in 4 bytes.
    int m_id;
    int m_shorthandID;  // If this property was set as part of a shorthand, gives the shorthand.
    bool m_important : 1;
    bool m_implicit : 1; // Whether or not the property was set implicitly as the result of a shorthand.

    friend bool operator==(const CSSProperty &, const CSSProperty &);

    RefPtr<CSSValue> m_value;
};

} // namespace

#endif
