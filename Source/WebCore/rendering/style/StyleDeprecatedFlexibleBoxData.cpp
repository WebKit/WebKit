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
#include "StyleDeprecatedFlexibleBoxData.h"

#include "RenderStyle.h"

namespace WebCore {

StyleDeprecatedFlexibleBoxData::StyleDeprecatedFlexibleBoxData()
    : flex(RenderStyle::initialBoxFlex())
    , flexGroup(RenderStyle::initialBoxFlexGroup())
    , ordinalGroup(RenderStyle::initialBoxOrdinalGroup())
    , align(static_cast<unsigned>(RenderStyle::initialBoxAlign()))
    , pack(static_cast<unsigned>(RenderStyle::initialBoxPack()))
    , orient(static_cast<unsigned>(RenderStyle::initialBoxOrient()))
    , lines(static_cast<unsigned>(RenderStyle::initialBoxLines()))
{
}

inline StyleDeprecatedFlexibleBoxData::StyleDeprecatedFlexibleBoxData(const StyleDeprecatedFlexibleBoxData& other)
    : RefCounted<StyleDeprecatedFlexibleBoxData>()
    , flex(other.flex)
    , flexGroup(other.flexGroup)
    , ordinalGroup(other.ordinalGroup)
    , align(other.align)
    , pack(other.pack)
    , orient(other.orient)
    , lines(other.lines)
{
}

Ref<StyleDeprecatedFlexibleBoxData> StyleDeprecatedFlexibleBoxData::copy() const
{
    return adoptRef(*new StyleDeprecatedFlexibleBoxData(*this));
}

bool StyleDeprecatedFlexibleBoxData::operator==(const StyleDeprecatedFlexibleBoxData& other) const
{
    return flex == other.flex && flexGroup == other.flexGroup
        && ordinalGroup == other.ordinalGroup && align == other.align
        && pack == other.pack && orient == other.orient && lines == other.lines;
}

} // namespace WebCore
