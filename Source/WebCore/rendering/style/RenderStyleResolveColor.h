/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "FilterOperations.h"
#include "PaintPhase.h"
#include "RenderStyle.h"
#include "RenderStyleConstants.h"
#include "RenderStyleInlines.h"
#include "SVGRenderStyle.h"
#include "StyleColor.h"

namespace WebCore {

template <CSSPropertyID colorProperty, bool visitedLink>
StyleColor RenderStyle::unresolvedColorForProperty() const
{
    switch (colorProperty) {
    case CSSPropertyAccentColor:
        return accentColor();
    case CSSPropertyColor:
        return visitedLink ? visitedLinkColor() : color();
    case CSSPropertyBackgroundColor:
        return visitedLink ? visitedLinkBackgroundColor() : backgroundColor();
    case CSSPropertyBorderBottomColor:
        return visitedLink ? visitedLinkBorderBottomColor() : borderBottomColor();
    case CSSPropertyBorderLeftColor:
        return visitedLink ? visitedLinkBorderLeftColor() : borderLeftColor();
    case CSSPropertyBorderRightColor:
        return visitedLink ? visitedLinkBorderRightColor() : borderRightColor();
    case CSSPropertyBorderTopColor:
        return visitedLink ? visitedLinkBorderTopColor() : borderTopColor();
    case CSSPropertyFill:
        return fillPaintColor();
    case CSSPropertyFloodColor:
        return floodColor();
    case CSSPropertyLightingColor:
        return lightingColor();
    case CSSPropertyOutlineColor:
        return visitedLink ? visitedLinkOutlineColor() : outlineColor();
    case CSSPropertyStopColor:
        return stopColor();
    case CSSPropertyStroke:
        return strokePaintColor();
    case CSSPropertyStrokeColor:
        return visitedLink ? visitedLinkStrokeColor() : strokeColor();
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
        return { }; // FIXME
    case CSSPropertyColumnRuleColor:
        return visitedLink ? visitedLinkColumnRuleColor() : columnRuleColor();
    case CSSPropertyTextEmphasisColor:
        return visitedLink ? visitedLinkTextEmphasisColor() : textEmphasisColor();
    case CSSPropertyWebkitTextFillColor:
        return visitedLink ? visitedLinkTextFillColor() : textFillColor();
    case CSSPropertyWebkitTextStrokeColor:
        return visitedLink ? visitedLinkTextStrokeColor() : textStrokeColor();
    case CSSPropertyTextDecorationColor:
        return visitedLink ? visitedLinkTextDecorationColor() : textDecorationColor();
    case CSSPropertyCaretColor:
        return visitedLink ? visitedLinkCaretColor() : caretColor();
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

template <bool visitedLink>
Color RenderStyle::colorResolvingCurrentColor(const StyleColor& color) const
{
    return color.resolveColor(visitedLink ? visitedLinkColor() : this->color());
}

template <CSSPropertyID colorProperty, bool visitedLink>
Color RenderStyle::colorResolvingCurrentColor() const
{
    auto result = unresolvedColorForProperty<colorProperty, visitedLink>();

    if constexpr (colorProperty == CSSPropertyTextDecorationColor) {
        if (result.isCurrentColor()) {
            if (hasPositiveStrokeWidth()) {
                // Prefer stroke color if possible but not if it's fully transparent.
                auto strokeColor = colorResolvingCurrentColor(usedStrokeColorProperty(), visitedLink);
                if (strokeColor.isVisible())
                    return strokeColor;
            }
            return colorResolvingCurrentColor<CSSPropertyWebkitTextFillColor, visitedLink>();
        }
    }

    return colorResolvingCurrentColor(result, visitedLink);
}

template <CSSPropertyID colorProperty>
Color RenderStyle::visitedDependentColor(OptionSet<PaintBehavior> paintBehavior) const
{
    Color unvisitedColor = colorResolvingCurrentColor<colorProperty, false>();
    if (LIKELY(insideLink() != InsideLink::InsideVisited))
        return unvisitedColor;

    if (paintBehavior.contains(PaintBehavior::DontShowVisitedLinks))
        return unvisitedColor;

    if (isInSubtreeWithBlendMode())
        return unvisitedColor;

    Color visitedColor = colorResolvingCurrentColor<colorProperty, true>();

    // FIXME: Technically someone could explicitly specify the color transparent, but for now we'll just
    // assume that if the background color is transparent that it wasn't set. Note that it's weird that
    // we're returning unvisited info for a visited link, but given our restriction that the alpha values
    // have to match, it makes more sense to return the unvisited background color if specified than it
    // does to return black. This behavior matches what Firefox 4 does as well.
    if (colorProperty == CSSPropertyBackgroundColor && visitedColor == Color::transparentBlack)
        return unvisitedColor;

    // Take the alpha from the unvisited color, but get the RGB values from the visited color.
    return visitedColor.colorWithAlpha(unvisitedColor.alphaAsFloat());
}

template <CSSPropertyID colorProperty>
Color RenderStyle::visitedDependentColorWithColorFilter(OptionSet<PaintBehavior> paintBehavior) const
{
    if (!hasAppleColorFilter())
        return visitedDependentColor<colorProperty>(paintBehavior);

    return colorByApplyingColorFilter(visitedDependentColor<colorProperty>(paintBehavior));
}

}
