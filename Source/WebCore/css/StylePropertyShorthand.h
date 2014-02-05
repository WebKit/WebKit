/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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

    StylePropertyShorthand(CSSPropertyID id, const CSSPropertyID* properties, const StylePropertyShorthand* propertiesForInitialization, unsigned numProperties)
        : m_properties(properties)
        , m_propertiesForInitialization(propertiesForInitialization)
        , m_length(numProperties)
        , m_shorthandID(id)
    {
    }

    const CSSPropertyID* properties() const { return m_properties; }
    const StylePropertyShorthand* propertiesForInitialization() const { return m_propertiesForInitialization; }
    unsigned length() const { return m_length; }
    CSSPropertyID id() const { return m_shorthandID; }

private:
    const CSSPropertyID* m_properties;
    const StylePropertyShorthand* m_propertiesForInitialization;
    unsigned m_length;
    CSSPropertyID m_shorthandID;
};

StylePropertyShorthand backgroundShorthand();
StylePropertyShorthand backgroundPositionShorthand();
StylePropertyShorthand backgroundRepeatShorthand();
StylePropertyShorthand borderShorthand();
StylePropertyShorthand borderAbridgedShorthand();
StylePropertyShorthand borderBottomShorthand();
StylePropertyShorthand borderColorShorthand();
StylePropertyShorthand borderImageShorthand();
StylePropertyShorthand borderLeftShorthand();
StylePropertyShorthand borderRadiusShorthand();
StylePropertyShorthand borderRightShorthand();
StylePropertyShorthand borderSpacingShorthand();
StylePropertyShorthand borderStyleShorthand();
StylePropertyShorthand borderTopShorthand();
StylePropertyShorthand borderWidthShorthand();
StylePropertyShorthand listStyleShorthand();
StylePropertyShorthand fontShorthand();
StylePropertyShorthand heightShorthand();
StylePropertyShorthand marginShorthand();
StylePropertyShorthand outlineShorthand();
StylePropertyShorthand overflowShorthand();
StylePropertyShorthand paddingShorthand();
StylePropertyShorthand transitionShorthand();
StylePropertyShorthand webkitAnimationShorthand();
StylePropertyShorthand webkitAnimationShorthandForParsing();
StylePropertyShorthand webkitBorderAfterShorthand();
StylePropertyShorthand webkitBorderBeforeShorthand();
StylePropertyShorthand webkitBorderEndShorthand();
StylePropertyShorthand webkitBorderRadiusShorthand();
StylePropertyShorthand webkitBorderStartShorthand();
StylePropertyShorthand webkitColumnsShorthand();
StylePropertyShorthand webkitColumnRuleShorthand();
StylePropertyShorthand webkitFlexFlowShorthand();
StylePropertyShorthand webkitFlexShorthand();
StylePropertyShorthand webkitGridAreaShorthand();
StylePropertyShorthand webkitGridColumnShorthand();
StylePropertyShorthand webkitGridRowShorthand();
StylePropertyShorthand webkitMarginCollapseShorthand();
StylePropertyShorthand webkitMarqueeShorthand();
StylePropertyShorthand webkitMaskShorthand();
StylePropertyShorthand webkitMaskPositionShorthand();
StylePropertyShorthand webkitMaskRepeatShorthand();
StylePropertyShorthand webkitTextEmphasisShorthand();
StylePropertyShorthand webkitTextStrokeShorthand();
StylePropertyShorthand webkitTransitionShorthand();
StylePropertyShorthand webkitTransformOriginShorthand();
StylePropertyShorthand widthShorthand();

StylePropertyShorthand webkitTextDecorationShorthand();

StylePropertyShorthand markerShorthand();

// Returns empty value if the property is not a shorthand.
StylePropertyShorthand shorthandForProperty(CSSPropertyID);

// Return the list of shorthands for a given longhand.
Vector<StylePropertyShorthand> matchingShorthandsForLonghand(CSSPropertyID);
unsigned indexOfShorthandForLonghand(CSSPropertyID, const Vector<StylePropertyShorthand>&);

bool isExpandedShorthand(CSSPropertyID);

} // namespace WebCore

#endif // StylePropertyShorthand_h
