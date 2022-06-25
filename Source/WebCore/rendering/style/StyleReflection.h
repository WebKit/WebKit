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

#pragma once

#include "Length.h"
#include "NinePieceImage.h"
#include "RenderStyleConstants.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class StyleReflection : public RefCounted<StyleReflection> {
public:
    static Ref<StyleReflection> create()
    {
        return adoptRef(*new StyleReflection);
    }

    bool operator==(const StyleReflection& o) const
    {
        return m_direction == o.m_direction && m_offset == o.m_offset && m_mask == o.m_mask;
    }
    bool operator!=(const StyleReflection& o) const { return !(*this == o); }

    ReflectionDirection direction() const { return m_direction; }
    const Length& offset() const { return m_offset; }
    const NinePieceImage& mask() const { return m_mask; }

    void setDirection(ReflectionDirection dir) { m_direction = dir; }
    void setOffset(Length offset) { m_offset = WTFMove(offset); }
    void setMask(const NinePieceImage& image) { m_mask = image; }

private:
    StyleReflection()
        : m_offset(0, LengthType::Fixed)
        , m_mask(NinePieceImage::Type::Mask)
    {
    }
    
    ReflectionDirection m_direction { ReflectionDirection::Below };
    Length m_offset;
    NinePieceImage m_mask;
};

} // namespace WebCore
