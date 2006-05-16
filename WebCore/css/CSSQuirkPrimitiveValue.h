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

#ifndef CSSQuirkPrimitiveValue_H
#define CSSQuirkPrimitiveValue_H

#include "CSSPrimitiveValue.h"

namespace WebCore {

// This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
// The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em
// in a stylesheet.  When the quirky value is used, if you're in quirks mode, the margin will
// collapse away inside a table cell.
class CSSQuirkPrimitiveValue : public CSSPrimitiveValue
{
public:
    CSSQuirkPrimitiveValue(double num, UnitTypes type)
        : CSSPrimitiveValue(num, type) {}

    virtual bool isQuirkValue() { return true; }
};

} // namespace

#endif
