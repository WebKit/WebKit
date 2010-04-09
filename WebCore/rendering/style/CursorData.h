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

#ifndef CursorData_h
#define CursorData_h

#include "CachedImage.h"
#include "CachedResourceHandle.h"
#include "IntPoint.h"

namespace WebCore {

class CursorData {
public:
    CursorData(CachedImage* image, const IntPoint& hotSpot)
        : m_image(image)
        , m_hotSpot(hotSpot)
    {
    }

    bool operator==(const CursorData& o) const
    {
        return m_hotSpot == o.m_hotSpot && m_image == o.m_image;
    }

    bool operator!=(const CursorData& o) const
    {
        return !(*this == o);
    }

    const CachedImage* image() const { return m_image.get(); }    
    const IntPoint& hotSpot() const { return m_hotSpot; }
    
private:
    CachedResourceHandle<CachedImage> m_image;
    IntPoint m_hotSpot; // for CSS3 support
};

} // namespace WebCore

#endif // CursorData_h
