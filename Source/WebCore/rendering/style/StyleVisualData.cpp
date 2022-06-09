/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleVisualData.h"

#include "RenderStyle.h"

namespace WebCore {

StyleVisualData::StyleVisualData()
    : hasClip(false)
    , textDecoration(RenderStyle::initialTextDecoration().toRaw())
    , zoom(RenderStyle::initialZoom())
{
}

StyleVisualData::~StyleVisualData() = default;

inline StyleVisualData::StyleVisualData(const StyleVisualData& other)
    : RefCounted<StyleVisualData>()
    , clip(other.clip)
    , hasClip(other.hasClip)
    , textDecoration(other.textDecoration)
    , zoom(RenderStyle::initialZoom())
{
}

Ref<StyleVisualData> StyleVisualData::copy() const
{
    return adoptRef(*new StyleVisualData(*this));
}

} // namespace WebCore
