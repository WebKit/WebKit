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

#ifndef FontValue_H
#define FontValue_H

#include "CSSValue.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSValueList;

class FontValue : public CSSValue
{
public:
    virtual String cssText() const;
    
    virtual bool isFontValue() { return true; }

    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> variant;
    RefPtr<CSSPrimitiveValue> weight;
    RefPtr<CSSPrimitiveValue> size;
    RefPtr<CSSPrimitiveValue> lineHeight;
    RefPtr<CSSValueList> family;
};

} // namespace

#endif
