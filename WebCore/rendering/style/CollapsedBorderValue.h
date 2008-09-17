/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
 *
 */

#ifndef CollapsedBorderValue_h
#define CollapsedBorderValue_h

#include "BorderValue.h"

namespace WebCore {

struct CollapsedBorderValue {
    CollapsedBorderValue()
        : border(0)
        , precedence(BOFF)
    {
    }

    CollapsedBorderValue(const BorderValue* b, EBorderPrecedence p)
        : border(b)
        , precedence(p)
    {
    }

    int width() const { return border && border->nonZero() ? border->width : 0; }
    EBorderStyle style() const { return border ? border->style() : BHIDDEN; }
    bool exists() const { return border; }
    Color color() const { return border ? border->color : Color(); }
    bool isTransparent() const { return border ? border->isTransparent() : true; }
    
    bool operator==(const CollapsedBorderValue& o) const
    {
        if (!border)
            return !o.border;
        if (!o.border)
            return false;
        return *border == *o.border && precedence == o.precedence;
    }
    
    const BorderValue* border;
    EBorderPrecedence precedence;    
};

} // namespace WebCore

#endif // CollapsedBorderValue_h
