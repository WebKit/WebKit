/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006, Rob Buis <buis@kde.org>
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

#ifndef CSSCursorImageValue_h
#define CSSCursorImageValue_h

#include "CSSImageValue.h"
#include "IntPoint.h"

namespace WebCore {

class CSSCursorImageValue : public CSSImageValue {
public:
    CSSCursorImageValue(const String& url, const IntPoint& hotspot, StyleBase*);
    virtual ~CSSCursorImageValue();

    IntPoint hotspot() const { return m_hotspot; }

protected:
    IntPoint m_hotspot;
};

} // namespace WebCore

#endif // CSSCursorImageValue_h
