/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef CSSValueList_h
#define CSSValueList_h

#include "CSSValue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSValueList : public CSSValue {
public:
    CSSValueList(bool isSpaceSeparated = false);
    virtual ~CSSValueList();

    unsigned length() const { return m_values.size(); }
    CSSValue* item (unsigned index) { return m_values[index].get(); }

    virtual bool isValueList() { return true; }

    virtual unsigned short cssValueType() const;

    void append(PassRefPtr<CSSValue>);
    virtual String cssText() const;

protected:
    Vector<RefPtr<CSSValue> > m_values;
    bool m_isSpaceSeparated;
};

} // namespace WebCore

#endif // CSSValueList_h
