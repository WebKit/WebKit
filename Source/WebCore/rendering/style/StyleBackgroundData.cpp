/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "StyleBackgroundData.h"

#include "RenderStyle.h"
#include "RenderStyleConstants.h"

namespace WebCore {

StyleBackgroundData::StyleBackgroundData()
    : background(FillLayer::create(FillLayerType::Background))
    , color(RenderStyle::initialBackgroundColor())
{
}

inline StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& other)
    : RefCounted<StyleBackgroundData>()
    , background(other.background)
    , color(other.color)
    , outline(other.outline)
{
}

Ref<StyleBackgroundData> StyleBackgroundData::copy() const
{
    return adoptRef(*new StyleBackgroundData(*this));
}

bool StyleBackgroundData::operator==(const StyleBackgroundData& other) const
{
    return background == other.background && color == other.color && outline == other.outline;
}

bool StyleBackgroundData::isEquivalentForPainting(const StyleBackgroundData& other) const
{
    if (background != other.background || color != other.color)
        return false;
    if (!outline.isVisible() && !other.outline.isVisible())
        return true;
    return outline == other.outline;
}

void StyleBackgroundData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || *background != FillLayer::create(FillLayerType::Background).get())
        ts.dumpProperty("background-image", background);
    if (behavior == DumpStyleValues::All || color != RenderStyle::initialBackgroundColor())
        ts.dumpProperty("background-color", color);
    if (behavior == DumpStyleValues::All || outline != OutlineValue())
        ts.dumpProperty("outline", outline);
}

TextStream& operator<<(TextStream& ts, const StyleBackgroundData& backgroundData)
{
    backgroundData.dump(ts);
    return ts;
}

} // namespace WebCore
