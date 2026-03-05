/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BackgroundData);

BackgroundData::BackgroundData()
    : background(CSS::Keyword::None { })
    , backgroundColor(ComputedStyle::initialBackgroundColor())
{
}

inline BackgroundData::BackgroundData(const BackgroundData& other)
    : RefCounted<BackgroundData>()
    , background(other.background)
    , backgroundColor(other.backgroundColor)
    , outline(other.outline)
{
}

Ref<BackgroundData> BackgroundData::copy() const
{
    return adoptRef(*new BackgroundData(*this));
}

bool BackgroundData::operator==(const BackgroundData& other) const
{
    return background == other.background
        && backgroundColor == other.backgroundColor
        && outline == other.outline;
}

bool BackgroundData::containsCurrentColor() const
{
    return backgroundColor.containsCurrentColor()
        || outline.outlineColor.containsCurrentColor();
}

void BackgroundData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || background != BackgroundLayers { CSS::Keyword::None { } })
        ts.dumpProperty("background-image"_s, background);
    if (behavior == DumpStyleValues::All || backgroundColor != ComputedStyle::initialBackgroundColor())
        ts.dumpProperty("background-color"_s, backgroundColor);
    if (behavior == DumpStyleValues::All || outline != OutlineValue())
        ts.dumpProperty("outline"_s, outline);
}

#if !LOG_DISABLED
void BackgroundData::dumpDifferences(TextStream& ts, const BackgroundData& other) const
{
    LOG_IF_DIFFERENT(background);
    LOG_IF_DIFFERENT(backgroundColor);
    LOG_IF_DIFFERENT(outline);
}
#endif

TextStream& operator<<(TextStream& ts, const BackgroundData& backgroundData)
{
    backgroundData.dump(ts);
    return ts;
}

} // namespace Style
} // namespace WebCore
