/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "config.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"

namespace WebCore {

StylePropertyShorthand borderAbridgedShorthand()
{
    static const CSSPropertyID borderAbridgedProperties[] = { CSSPropertyBorderWidth, CSSPropertyBorderStyle, CSSPropertyBorderColor };
    static const StylePropertyShorthand propertiesForInitialization[] = { borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand() };
    return StylePropertyShorthand(CSSPropertyBorder, borderAbridgedProperties, propertiesForInitialization);
}

StylePropertyShorthand animationShorthandForParsing(CSSPropertyID propId)
{
    // Animation-name must come last, so that keywords for other properties in the shorthand
    // preferentially match those properties.
    static const CSSPropertyID animationPropertiesForParsing[] = {
        CSSPropertyAnimationDuration,
        CSSPropertyAnimationTimingFunction,
        CSSPropertyAnimationDelay,
        CSSPropertyAnimationIterationCount,
        CSSPropertyAnimationDirection,
        CSSPropertyAnimationFillMode,
        CSSPropertyAnimationPlayState,
        CSSPropertyAnimationName
    };

    static const CSSPropertyID prefixedAnimationPropertiesForParsing[] = {
        CSSPropertyWebkitAnimationDuration,
        CSSPropertyWebkitAnimationTimingFunction,
        CSSPropertyWebkitAnimationDelay,
        CSSPropertyWebkitAnimationIterationCount,
        CSSPropertyWebkitAnimationDirection,
        CSSPropertyWebkitAnimationFillMode,
        CSSPropertyWebkitAnimationPlayState,
        CSSPropertyWebkitAnimationName
    };

    if (propId == CSSPropertyAnimation)
        return StylePropertyShorthand(CSSPropertyAnimation, animationPropertiesForParsing);
    return StylePropertyShorthand(CSSPropertyWebkitAnimation, prefixedAnimationPropertiesForParsing);
}

bool isShorthandCSSProperty(CSSPropertyID id)
{
    return shorthandForProperty(id).length();
}

unsigned indexOfShorthandForLonghand(CSSPropertyID shorthandID, const StylePropertyShorthandVector& shorthands)
{
    for (unsigned i = 0, size = shorthands.size(); i < size; ++i) {
        if (shorthands[i].id() == shorthandID)
            return i;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
