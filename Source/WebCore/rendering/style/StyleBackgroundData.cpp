/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "BorderData.h"
#include "RenderStyleConstants.h"
#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleBackgroundData);

StyleBackgroundData::StyleBackgroundData()
    : background(RenderStyle::initialBackgroundLayers())
    , backgroundColor(RenderStyle::initialBackgroundColor())
{
}

inline StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& other)
    : RefCounted<StyleBackgroundData>()
    , background(other.background)
    , backgroundColor(other.backgroundColor)
    , outline(other.outline)
{
}

Ref<StyleBackgroundData> StyleBackgroundData::copy() const
{
    return adoptRef(*new StyleBackgroundData(*this));
}

bool StyleBackgroundData::operator==(const StyleBackgroundData& other) const
{
    return background == other.background
        && backgroundColor == other.backgroundColor
        && outline == other.outline;
}

bool StyleBackgroundData::isEquivalentForPainting(const StyleBackgroundData& other, bool currentColorDiffers) const
{
    if (this == &other) {
        ASSERT(currentColorDiffers);
        return !containsCurrentColor();
    }

    if (background != other.background || backgroundColor != other.backgroundColor)
        return false;
    if (currentColorDiffers && backgroundColor.containsCurrentColor())
        return false;
    if (!outline.isVisible() && !other.outline.isVisible())
        return true;
    if (currentColorDiffers && outline.color().containsCurrentColor())
        return false;
    return outline == other.outline;
}

bool StyleBackgroundData::containsCurrentColor() const
{
    return backgroundColor.containsCurrentColor()
        || outline.color().containsCurrentColor();
}

void StyleBackgroundData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || background != RenderStyle::initialBackgroundLayers())
        ts.dumpProperty("background-image"_s, background);
    if (behavior == DumpStyleValues::All || backgroundColor != RenderStyle::initialBackgroundColor())
        ts.dumpProperty("background-color"_s, backgroundColor);
    if (behavior == DumpStyleValues::All || outline != OutlineValue())
        ts.dumpProperty("outline"_s, outline);
}

#if !LOG_DISABLED
void StyleBackgroundData::dumpDifferences(TextStream& ts, const StyleBackgroundData& other) const
{
    LOG_IF_DIFFERENT(background);
    LOG_IF_DIFFERENT(backgroundColor);
    LOG_IF_DIFFERENT(outline);
}
#endif

TextStream& operator<<(TextStream& ts, const StyleBackgroundData& backgroundData)
{
    backgroundData.dump(ts);
    return ts;
}

} // namespace WebCore
