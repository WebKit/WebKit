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

StylePropertyShorthand animationShorthandForParsing()
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

    return StylePropertyShorthand(CSSPropertyAnimation, animationPropertiesForParsing);
}

StylePropertyShorthand transitionShorthandForParsing()
{
    // Similar to animations, we have property after timing-function and delay after
    // duration.
    static const CSSPropertyID transitionProperties[] = {
        CSSPropertyTransitionDuration, CSSPropertyTransitionTimingFunction,
        CSSPropertyTransitionDelay, CSSPropertyTransitionProperty};
    return StylePropertyShorthand(CSSPropertyTransition, transitionProperties);
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
