/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef StylePropertyShorthand_h
#define StylePropertyShorthand_h

#include "CSSPropertyNames.h"
#include <wtf/Vector.h>

namespace WebCore {

class StylePropertyShorthand {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StylePropertyShorthand()
        : m_properties(0)
        , m_propertiesForInitialization(0)
        , m_length(0)
        , m_shorthandID(CSSPropertyInvalid)
    {
    }

    StylePropertyShorthand(CSSPropertyID id, const CSSPropertyID* properties, unsigned numProperties)
        : m_properties(properties)
        , m_propertiesForInitialization(0)
        , m_length(numProperties)
        , m_shorthandID(id)
    {
    }

    StylePropertyShorthand(CSSPropertyID id, const CSSPropertyID* properties, const StylePropertyShorthand** propertiesForInitialization, unsigned numProperties)
        : m_properties(properties)
        , m_propertiesForInitialization(propertiesForInitialization)
        , m_length(numProperties)
        , m_shorthandID(id)
    {
    }

    const CSSPropertyID* properties() const { return m_properties; }
    const StylePropertyShorthand** propertiesForInitialization() const { return m_propertiesForInitialization; }
    unsigned length() const { return m_length; }
    CSSPropertyID id() const { return m_shorthandID; }

private:
    const CSSPropertyID* m_properties;
    const StylePropertyShorthand** m_propertiesForInitialization;
    unsigned m_length;
    CSSPropertyID m_shorthandID;
};

const StylePropertyShorthand& backgroundShorthand();
const StylePropertyShorthand& backgroundPositionShorthand();
const StylePropertyShorthand& backgroundRepeatShorthand();
const StylePropertyShorthand& borderShorthand();
const StylePropertyShorthand& borderAbridgedShorthand();
const StylePropertyShorthand& borderBottomShorthand();
const StylePropertyShorthand& borderColorShorthand();
const StylePropertyShorthand& borderImageShorthand();
const StylePropertyShorthand& borderLeftShorthand();
const StylePropertyShorthand& borderRadiusShorthand();
const StylePropertyShorthand& borderRightShorthand();
const StylePropertyShorthand& borderSpacingShorthand();
const StylePropertyShorthand& borderStyleShorthand();
const StylePropertyShorthand& borderTopShorthand();
const StylePropertyShorthand& borderWidthShorthand();
const StylePropertyShorthand& listStyleShorthand();
const StylePropertyShorthand& fontShorthand();
const StylePropertyShorthand& heightShorthand();
const StylePropertyShorthand& marginShorthand();
#if ENABLE(SVG)
const StylePropertyShorthand& markerShorthand();
#endif
const StylePropertyShorthand& outlineShorthand();
const StylePropertyShorthand& overflowShorthand();
const StylePropertyShorthand& paddingShorthand();
const StylePropertyShorthand& transitionShorthand();
const StylePropertyShorthand& webkitAnimationShorthand();
const StylePropertyShorthand& webkitAnimationShorthandForParsing();
const StylePropertyShorthand& webkitBorderAfterShorthand();
const StylePropertyShorthand& webkitBorderBeforeShorthand();
const StylePropertyShorthand& webkitBorderEndShorthand();
const StylePropertyShorthand& webkitBorderRadiusShorthand();
const StylePropertyShorthand& webkitBorderStartShorthand();
const StylePropertyShorthand& webkitColumnsShorthand();
const StylePropertyShorthand& webkitColumnRuleShorthand();
const StylePropertyShorthand& webkitFlexFlowShorthand();
const StylePropertyShorthand& webkitFlexShorthand();
const StylePropertyShorthand& webkitGridColumnShorthand();
const StylePropertyShorthand& webkitGridRowShorthand();
const StylePropertyShorthand& webkitMarginCollapseShorthand();
const StylePropertyShorthand& webkitMarqueeShorthand();
const StylePropertyShorthand& webkitMaskShorthand();
const StylePropertyShorthand& webkitMaskPositionShorthand();
const StylePropertyShorthand& webkitMaskRepeatShorthand();
const StylePropertyShorthand& webkitTextEmphasisShorthand();
const StylePropertyShorthand& webkitTextStrokeShorthand();
const StylePropertyShorthand& webkitTransitionShorthand();
const StylePropertyShorthand& webkitTransformOriginShorthand();
const StylePropertyShorthand& widthShorthand();

#if ENABLE(CSS3_TEXT)
const StylePropertyShorthand& webkitTextDecorationShorthand();
#endif

// Returns an empty list if the property is not a shorthand.
const StylePropertyShorthand& shorthandForProperty(CSSPropertyID);

// Return the list of shorthands for a given longhand.
const Vector<const StylePropertyShorthand*> matchingShorthandsForLonghand(CSSPropertyID);
unsigned indexOfShorthandForLonghand(CSSPropertyID, const Vector<const StylePropertyShorthand*>&);

bool isExpandedShorthand(CSSPropertyID);

} // namespace WebCore

#endif // StylePropertyShorthand_h
