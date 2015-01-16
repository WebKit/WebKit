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

#include <wtf/StdLibExtras.h>

namespace WebCore {

StylePropertyShorthand borderAbridgedShorthand()
{
    static const CSSPropertyID borderAbridgedProperties[] = { CSSPropertyBorderWidth, CSSPropertyBorderStyle, CSSPropertyBorderColor };
    static const StylePropertyShorthand propertiesForInitialization[] = { borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand() };
    return StylePropertyShorthand(CSSPropertyBorder, borderAbridgedProperties, propertiesForInitialization);
}

StylePropertyShorthand fontShorthand()
{
    static const CSSPropertyID fontProperties[] = {
        CSSPropertyFontFamily,
        CSSPropertyFontSize,
        CSSPropertyFontStyle,
        CSSPropertyFontVariant,
        CSSPropertyFontWeight,
        CSSPropertyLineHeight
    };
    return StylePropertyShorthand(CSSPropertyFont, fontProperties);
}

StylePropertyShorthand animationShorthandForParsing(CSSPropertyID propId)
{
    // When we parse the animation shorthand we need to look for animation-name
    // last because otherwise it might match against the keywords for fill mode,
    // timing functions and infinite iteration. This means that animation names
    // that are the same as keywords (e.g. 'forwards') won't always match in the
    // shorthand. In that case the authors should be using longhands (or
    // reconsidering their approach). This is covered by the animations spec
    // bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=14790
    // And in the spec (editor's draft) at:
    // http://dev.w3.org/csswg/css3-animations/#animation-shorthand-property
    static const CSSPropertyID animationPropertiesForParsing[] = {
        CSSPropertyAnimationDuration,
        CSSPropertyAnimationTimingFunction,
        CSSPropertyAnimationDelay,
        CSSPropertyAnimationIterationCount,
        CSSPropertyAnimationDirection,
        CSSPropertyAnimationFillMode,
        CSSPropertyAnimationName
    };

    static const CSSPropertyID prefixedAnimationPropertiesForParsing[] = {
        CSSPropertyWebkitAnimationDuration,
        CSSPropertyWebkitAnimationTimingFunction,
        CSSPropertyWebkitAnimationDelay,
        CSSPropertyWebkitAnimationIterationCount,
        CSSPropertyWebkitAnimationDirection,
        CSSPropertyWebkitAnimationFillMode,
        CSSPropertyWebkitAnimationName
    };

    if (propId == CSSPropertyAnimation)
        return StylePropertyShorthand(CSSPropertyAnimation, animationPropertiesForParsing);
    return StylePropertyShorthand(CSSPropertyWebkitAnimation, prefixedAnimationPropertiesForParsing);
}

StylePropertyShorthand widthShorthand()
{
    static const CSSPropertyID widthProperties[] = {
        CSSPropertyMinWidth,
        CSSPropertyMaxWidth
    };
    return StylePropertyShorthand(CSSPropertyWidth, widthProperties);
}

StylePropertyShorthand heightShorthand()
{
    static const CSSPropertyID heightProperties[] = {
        CSSPropertyMinHeight,
        CSSPropertyMaxHeight
    };
    return StylePropertyShorthand(CSSPropertyHeight, heightProperties);
}

// FIXME: This function should be generated from CSSPropertyNames.in.
// Returns an empty list if the property is not a shorthand.
StylePropertyShorthand shorthandForProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyAnimation:
        return animationShorthand();
    case CSSPropertyBackground:
        return backgroundShorthand();
    case CSSPropertyBackgroundPosition:
        return backgroundPositionShorthand();
    case CSSPropertyBackgroundRepeat:
        return backgroundRepeatShorthand();
    case CSSPropertyBorder:
        return borderShorthand();
    case CSSPropertyBorderBottom:
        return borderBottomShorthand();
    case CSSPropertyBorderColor:
        return borderColorShorthand();
    case CSSPropertyBorderImage:
        return borderImageShorthand();
    case CSSPropertyBorderLeft:
        return borderLeftShorthand();
    case CSSPropertyBorderRadius:
        return borderRadiusShorthand();
    case CSSPropertyBorderRight:
        return borderRightShorthand();
    case CSSPropertyBorderSpacing:
        return borderSpacingShorthand();
    case CSSPropertyBorderStyle:
        return borderStyleShorthand();
    case CSSPropertyBorderTop:
        return borderTopShorthand();
    case CSSPropertyBorderWidth:
        return borderWidthShorthand();
    case CSSPropertyListStyle:
        return listStyleShorthand();
    case CSSPropertyFont:
        return fontShorthand();
    case CSSPropertyMargin:
        return marginShorthand();
    case CSSPropertyOutline:
        return outlineShorthand();
    case CSSPropertyOverflow:
        return overflowShorthand();
    case CSSPropertyPadding:
        return paddingShorthand();
    case CSSPropertyTransition:
        return transitionShorthand();
    case CSSPropertyWebkitAnimation:
        return webkitAnimationShorthand();
    case CSSPropertyWebkitBorderAfter:
        return webkitBorderAfterShorthand();
    case CSSPropertyWebkitBorderBefore:
        return webkitBorderBeforeShorthand();
    case CSSPropertyWebkitBorderEnd:
        return webkitBorderEndShorthand();
    case CSSPropertyWebkitBorderStart:
        return webkitBorderStartShorthand();
    case CSSPropertyWebkitBorderRadius:
        return borderRadiusShorthand();
    case CSSPropertyColumns:
        return columnsShorthand();
    case CSSPropertyColumnRule:
        return columnRuleShorthand();
    case CSSPropertyFlex:
        return flexShorthand();
    case CSSPropertyFlexFlow:
        return flexFlowShorthand();
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridArea:
        return webkitGridAreaShorthand();
    case CSSPropertyWebkitGridTemplate:
        return webkitGridTemplateShorthand();
    case CSSPropertyWebkitGrid:
        return webkitGridShorthand();
    case CSSPropertyWebkitGridColumn:
        return webkitGridColumnShorthand();
    case CSSPropertyWebkitGridRow:
        return webkitGridRowShorthand();
#endif
    case CSSPropertyWebkitMarginCollapse:
        return webkitMarginCollapseShorthand();
    case CSSPropertyWebkitMarquee:
        return webkitMarqueeShorthand();
    case CSSPropertyWebkitMask:
        return webkitMaskShorthand();
    case CSSPropertyWebkitMaskPosition:
        return webkitMaskPositionShorthand();
    case CSSPropertyWebkitMaskRepeat:
        return webkitMaskRepeatShorthand();
    case CSSPropertyWebkitPerspectiveOrigin:
        return webkitPerspectiveOriginShorthand();
    case CSSPropertyWebkitTextEmphasis:
        return webkitTextEmphasisShorthand();
    case CSSPropertyWebkitTextStroke:
        return webkitTextStrokeShorthand();
    case CSSPropertyWebkitTransition:
        return webkitTransitionShorthand();
    case CSSPropertyWebkitTransformOrigin:
        return webkitTransformOriginShorthand();
    case CSSPropertyWebkitTextDecoration:
        return webkitTextDecorationShorthand();
    case CSSPropertyMarker:
        return markerShorthand();
    default:
        return StylePropertyShorthand();
    }
}

bool isExpandedShorthand(CSSPropertyID id)
{
    // The system fonts bypass the normal style resolution by using RenderTheme,
    // thus we need to special case it here. FIXME: This is a violation of CSS 3 Fonts
    // as we should still be able to change the longhands.
    // DON'T ADD ANY SHORTHAND HERE UNLESS IT ISN'T ALWAYS EXPANDED AT PARSE TIME (which is wrong).
    if (id == CSSPropertyFont)
        return false;

    return shorthandForProperty(id).length();
}

static Vector<StylePropertyShorthand> makeVector(const StylePropertyShorthand& a)
{
    return Vector<StylePropertyShorthand>(1, a);
}

static Vector<StylePropertyShorthand> makeVector(const StylePropertyShorthand& a, const StylePropertyShorthand& b)
{
    Vector<StylePropertyShorthand> vector;
    vector.reserveInitialCapacity(2);
    vector.uncheckedAppend(a);
    vector.uncheckedAppend(b);
    return vector;
}

static Vector<StylePropertyShorthand> makeVector(const StylePropertyShorthand& a, const StylePropertyShorthand& b, const StylePropertyShorthand& c)
{
    Vector<StylePropertyShorthand> vector;
    vector.reserveInitialCapacity(3);
    vector.uncheckedAppend(a);
    vector.uncheckedAppend(b);
    vector.uncheckedAppend(c);
    return vector;
}

// FIXME: This function should be generated from CSSPropertyNames.in.
Vector<StylePropertyShorthand> matchingShorthandsForLonghand(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationFillMode:
        return makeVector(animationShorthand());
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
        return makeVector(backgroundShorthand());
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
        return makeVector(backgroundShorthand(), backgroundPositionShorthand());
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
        return makeVector(backgroundShorthand(), backgroundRepeatShorthand());
    case CSSPropertyBorderBottomWidth:
        return makeVector(borderShorthand(), borderBottomShorthand(), borderWidthShorthand());
    case CSSPropertyBorderTopColor:
        return makeVector(borderShorthand(), borderTopShorthand(), borderColorShorthand());
    case CSSPropertyBorderRightColor:
        return makeVector(borderShorthand(), borderRightShorthand(), borderColorShorthand());
    case CSSPropertyBorderLeftColor:
        return makeVector(borderShorthand(), borderLeftShorthand(), borderColorShorthand());
    case CSSPropertyBorderBottomColor:
        return makeVector(borderShorthand(), borderBottomShorthand(), borderColorShorthand());
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
        return makeVector(borderImageShorthand());
    case CSSPropertyBorderLeftWidth:
        return makeVector(borderShorthand(), borderLeftShorthand(), borderWidthShorthand());
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomLeftRadius:
        return makeVector(borderRadiusShorthand(), webkitBorderRadiusShorthand());
    case CSSPropertyBorderRightWidth:
        return makeVector(borderShorthand(), borderRightShorthand(), borderWidthShorthand());
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        return makeVector(borderSpacingShorthand());
    case CSSPropertyBorderTopStyle:
        return makeVector(borderShorthand(), borderTopShorthand(), borderStyleShorthand());
    case CSSPropertyBorderBottomStyle:
        return makeVector(borderShorthand(), borderBottomShorthand(), borderStyleShorthand());
    case CSSPropertyBorderLeftStyle:
        return makeVector(borderShorthand(), borderLeftShorthand(), borderStyleShorthand());
    case CSSPropertyBorderRightStyle:
        return makeVector(borderShorthand(), borderRightShorthand(), borderStyleShorthand());
    case CSSPropertyBorderTopWidth:
        return makeVector(borderShorthand(), borderTopShorthand(), borderWidthShorthand());
    case CSSPropertyListStyleType:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleImage:
        return makeVector(listStyleShorthand());
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
        return makeVector(fontShorthand());
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
        return makeVector(marginShorthand());
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
        return makeVector(outlineShorthand());
    case CSSPropertyPaddingTop:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
        return makeVector(paddingShorthand());
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
        return makeVector(overflowShorthand());
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyTransitionDelay:
        return makeVector(transitionShorthand());
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationFillMode:
        return makeVector(webkitAnimationShorthand());
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterColor:
        return makeVector(webkitBorderAfterShorthand());
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeColor:
        return makeVector(webkitBorderBeforeShorthand());
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndColor:
        return makeVector(webkitBorderEndShorthand());
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartColor:
        return makeVector(webkitBorderStartShorthand());
    case CSSPropertyColumnWidth:
    case CSSPropertyColumnCount:
        return makeVector(columnsShorthand());
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyColumnRuleColor:
        return makeVector(columnRuleShorthand());
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFlexBasis:
        return makeVector(flexShorthand());
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
        return makeVector(flexFlowShorthand());
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridColumnStart:
    case CSSPropertyWebkitGridColumnEnd:
        return makeVector(webkitGridColumnShorthand());
    case CSSPropertyWebkitGridRowStart:
    case CSSPropertyWebkitGridRowEnd:
        return makeVector(webkitGridRowShorthand());
    case CSSPropertyWebkitGridTemplateColumns:
    case CSSPropertyWebkitGridTemplateRows:
    case CSSPropertyWebkitGridTemplateAreas:
        return makeVector(webkitGridTemplateShorthand(), webkitGridShorthand());
    case CSSPropertyWebkitGridAutoFlow:
    case CSSPropertyWebkitGridAutoColumns:
    case CSSPropertyWebkitGridAutoRows:
        return makeVector(webkitGridShorthand());
#endif
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
        return makeVector(webkitMarginCollapseShorthand());
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeIncrement:
    case CSSPropertyWebkitMarqueeRepetition:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitMarqueeSpeed:
        return makeVector(webkitMarqueeShorthand());
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskSourceType:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskClip:
        return makeVector(webkitMaskShorthand());
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return makeVector(webkitMaskPositionShorthand());
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
        return makeVector(webkitMaskRepeatShorthand());
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
        return makeVector(webkitPerspectiveOriginShorthand());
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextEmphasisColor:
        return makeVector(webkitTextEmphasisShorthand());
    case CSSPropertyWebkitTextStrokeWidth:
    case CSSPropertyWebkitTextStrokeColor:
        return makeVector(webkitTextStrokeShorthand());
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitTransitionDelay:
        return makeVector(webkitTransitionShorthand());
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
        return makeVector(webkitTransformOriginShorthand());
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
        return makeVector(widthShorthand());
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
        return makeVector(heightShorthand());
    case CSSPropertyWebkitTextDecorationLine:
    case CSSPropertyWebkitTextDecorationStyle:
    case CSSPropertyWebkitTextDecorationColor:
        return makeVector(webkitTextDecorationShorthand());
    case CSSPropertyMarkerStart:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerEnd:
        return makeVector(markerShorthand());
    default:
        break;
    }

    return Vector<StylePropertyShorthand>();
}

unsigned indexOfShorthandForLonghand(CSSPropertyID shorthandID, const Vector<StylePropertyShorthand>& shorthands)
{
    for (unsigned i = 0, size = shorthands.size(); i < size; ++i) {
        if (shorthands[i].id() == shorthandID)
            return i;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
