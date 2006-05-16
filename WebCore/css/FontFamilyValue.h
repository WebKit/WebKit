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

#ifndef FontFamilyValue_H
#define FontFamilyValue_H

#include "CSSPrimitiveValue.h"
#include "DeprecatedString.h"

namespace WebCore {

class FontFamilyValue : public CSSPrimitiveValue
{
public:
    FontFamilyValue(const DeprecatedString& string);
    const DeprecatedString& fontName() const { return parsedFontName; }
    int genericFamilyType() const { return m_genericFamilyType; }

    virtual String cssText() const;

    DeprecatedString parsedFontName;
private:
    int m_genericFamilyType;
};

} // namespace

#endif
