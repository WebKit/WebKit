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

// FIXME: How much of this file can we generate?

namespace WebCore {

StylePropertyShorthand backgroundShorthand()
{
    static const CSSPropertyID backgroundProperties[] = {
        CSSPropertyBackgroundImage,
        CSSPropertyBackgroundPositionX,
        CSSPropertyBackgroundPositionY,
        CSSPropertyBackgroundSize,
        CSSPropertyBackgroundRepeatX,
        CSSPropertyBackgroundRepeatY,
        CSSPropertyBackgroundAttachment,
        CSSPropertyBackgroundOrigin,
        CSSPropertyBackgroundClip,
        CSSPropertyBackgroundColor
    };
    return StylePropertyShorthand(CSSPropertyBackground, backgroundProperties, WTF_ARRAY_LENGTH(backgroundProperties));
}

StylePropertyShorthand backgroundPositionShorthand()
{
    static const CSSPropertyID backgroundPositionProperties[] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
    return StylePropertyShorthand(CSSPropertyBackgroundPosition, backgroundPositionProperties, WTF_ARRAY_LENGTH(backgroundPositionProperties));
}

StylePropertyShorthand backgroundRepeatShorthand()
{
    static const CSSPropertyID backgroundRepeatProperties[] = { CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY };
    return StylePropertyShorthand(CSSPropertyBackgroundRepeat, backgroundRepeatProperties, WTF_ARRAY_LENGTH(backgroundRepeatProperties));
}

StylePropertyShorthand borderShorthand()
{
    // Do not change the order of the following four shorthands, and keep them together.
    static const CSSPropertyID borderProperties[4][3] = {
        { CSSPropertyBorderTopColor, CSSPropertyBorderTopStyle, CSSPropertyBorderTopWidth },
        { CSSPropertyBorderRightColor, CSSPropertyBorderRightStyle, CSSPropertyBorderRightWidth },
        { CSSPropertyBorderBottomColor, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomWidth },
        { CSSPropertyBorderLeftColor, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftWidth }
    };
    return StylePropertyShorthand(CSSPropertyBorder, borderProperties[0], sizeof(borderProperties) / sizeof(borderProperties[0][0]));
}

StylePropertyShorthand borderAbridgedShorthand()
{
    static const CSSPropertyID borderAbridgedProperties[] = { CSSPropertyBorderWidth, CSSPropertyBorderStyle, CSSPropertyBorderColor };
    static const StylePropertyShorthand propertiesForInitialization[] = { borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand() };
    return StylePropertyShorthand(CSSPropertyBorder, borderAbridgedProperties, propertiesForInitialization, WTF_ARRAY_LENGTH(borderAbridgedProperties));
}

StylePropertyShorthand borderBottomShorthand()
{
    static const CSSPropertyID borderBottomProperties[] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomColor };
    return StylePropertyShorthand(CSSPropertyBorderBottom, borderBottomProperties, WTF_ARRAY_LENGTH(borderBottomProperties));
}

StylePropertyShorthand borderColorShorthand()
{
    static const CSSPropertyID borderColorProperties[] = {
        CSSPropertyBorderTopColor,
        CSSPropertyBorderRightColor,
        CSSPropertyBorderBottomColor,
        CSSPropertyBorderLeftColor
    };
    return StylePropertyShorthand(CSSPropertyBorderColor, borderColorProperties, WTF_ARRAY_LENGTH(borderColorProperties));
}

StylePropertyShorthand borderImageShorthand()
{
    static const CSSPropertyID borderImageProperties[] = {
        CSSPropertyBorderImageSource,
        CSSPropertyBorderImageSlice,
        CSSPropertyBorderImageWidth,
        CSSPropertyBorderImageOutset,
        CSSPropertyBorderImageRepeat
    };
    return StylePropertyShorthand(CSSPropertyBorderImage, borderImageProperties, WTF_ARRAY_LENGTH(borderImageProperties));
}

StylePropertyShorthand borderLeftShorthand()
{
    static const CSSPropertyID borderLeftProperties[] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftColor };
    return StylePropertyShorthand(CSSPropertyBorderLeft, borderLeftProperties, WTF_ARRAY_LENGTH(borderLeftProperties));
}

StylePropertyShorthand borderRadiusShorthand()
{
    static const CSSPropertyID borderRadiusProperties[] = {
        CSSPropertyBorderTopLeftRadius,
        CSSPropertyBorderTopRightRadius,
        CSSPropertyBorderBottomRightRadius,
        CSSPropertyBorderBottomLeftRadius
    };
    return StylePropertyShorthand(CSSPropertyBorderRadius, borderRadiusProperties, WTF_ARRAY_LENGTH(borderRadiusProperties));
}

StylePropertyShorthand webkitBorderRadiusShorthand()
{
    static const CSSPropertyID borderRadiusProperties[] = {
        CSSPropertyBorderTopLeftRadius,
        CSSPropertyBorderTopRightRadius,
        CSSPropertyBorderBottomRightRadius,
        CSSPropertyBorderBottomLeftRadius
    };
    return StylePropertyShorthand(CSSPropertyWebkitBorderRadius, borderRadiusProperties, WTF_ARRAY_LENGTH(borderRadiusProperties));
}

StylePropertyShorthand borderRightShorthand()
{
    static const CSSPropertyID borderRightProperties[] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle, CSSPropertyBorderRightColor };
    return StylePropertyShorthand(CSSPropertyBorderRight, borderRightProperties, WTF_ARRAY_LENGTH(borderRightProperties));
}

StylePropertyShorthand borderSpacingShorthand()
{
    static const CSSPropertyID borderSpacingProperties[] = { CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderVerticalSpacing };
    return StylePropertyShorthand(CSSPropertyBorderSpacing, borderSpacingProperties, WTF_ARRAY_LENGTH(borderSpacingProperties));
}

StylePropertyShorthand borderStyleShorthand()
{
    static const CSSPropertyID borderStyleProperties[] = {
        CSSPropertyBorderTopStyle,
        CSSPropertyBorderRightStyle,
        CSSPropertyBorderBottomStyle,
        CSSPropertyBorderLeftStyle
    };
    return StylePropertyShorthand(CSSPropertyBorderStyle, borderStyleProperties, WTF_ARRAY_LENGTH(borderStyleProperties));
}

StylePropertyShorthand borderTopShorthand()
{
    static const CSSPropertyID borderTopProperties[] = { CSSPropertyBorderTopWidth, CSSPropertyBorderTopStyle, CSSPropertyBorderTopColor };
    return StylePropertyShorthand(CSSPropertyBorderTop, borderTopProperties, WTF_ARRAY_LENGTH(borderTopProperties));
}

StylePropertyShorthand borderWidthShorthand()
{
    static const CSSPropertyID borderWidthProperties[] = {
        CSSPropertyBorderTopWidth,
        CSSPropertyBorderRightWidth,
        CSSPropertyBorderBottomWidth,
        CSSPropertyBorderLeftWidth
    };
    return StylePropertyShorthand(CSSPropertyBorderWidth, borderWidthProperties, WTF_ARRAY_LENGTH(borderWidthProperties));
}

StylePropertyShorthand listStyleShorthand()
{
    static const CSSPropertyID listStyleProperties[] = {
        CSSPropertyListStyleType,
        CSSPropertyListStylePosition,
        CSSPropertyListStyleImage
    };
    return StylePropertyShorthand(CSSPropertyListStyle, listStyleProperties, WTF_ARRAY_LENGTH(listStyleProperties));
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
    return StylePropertyShorthand(CSSPropertyFont, fontProperties, WTF_ARRAY_LENGTH(fontProperties));
}

StylePropertyShorthand marginShorthand()
{
    static const CSSPropertyID marginProperties[] = {
        CSSPropertyMarginTop,
        CSSPropertyMarginRight,
        CSSPropertyMarginBottom,
        CSSPropertyMarginLeft
    };
    return StylePropertyShorthand(CSSPropertyMargin, marginProperties, WTF_ARRAY_LENGTH(marginProperties));
}

StylePropertyShorthand markerShorthand()
{
    static const CSSPropertyID markerProperties[] = {
        CSSPropertyMarkerStart,
        CSSPropertyMarkerMid,
        CSSPropertyMarkerEnd
    };
    return StylePropertyShorthand(CSSPropertyMarker, markerProperties, WTF_ARRAY_LENGTH(markerProperties));
}

StylePropertyShorthand outlineShorthand()
{
    static const CSSPropertyID outlineProperties[] = {
        CSSPropertyOutlineColor,
        CSSPropertyOutlineStyle,
        CSSPropertyOutlineWidth
    };
    return StylePropertyShorthand(CSSPropertyOutline, outlineProperties, WTF_ARRAY_LENGTH(outlineProperties));
}

StylePropertyShorthand overflowShorthand()
{
    static const CSSPropertyID overflowProperties[] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
    return StylePropertyShorthand(CSSPropertyOverflow, overflowProperties, WTF_ARRAY_LENGTH(overflowProperties));
}

StylePropertyShorthand paddingShorthand()
{
    static const CSSPropertyID paddingProperties[] = {
        CSSPropertyPaddingTop,
        CSSPropertyPaddingRight,
        CSSPropertyPaddingBottom,
        CSSPropertyPaddingLeft
    };
    return StylePropertyShorthand(CSSPropertyPadding, paddingProperties, WTF_ARRAY_LENGTH(paddingProperties));
}

StylePropertyShorthand transitionShorthand()
{
    static const CSSPropertyID transitionProperties[] = {
        CSSPropertyTransitionProperty,
        CSSPropertyTransitionDuration,
        CSSPropertyTransitionTimingFunction,
        CSSPropertyTransitionDelay
    };
    return StylePropertyShorthand(CSSPropertyTransition, transitionProperties, WTF_ARRAY_LENGTH(transitionProperties));
}

StylePropertyShorthand webkitAnimationShorthand()
{
    static const CSSPropertyID animationProperties[] = {
        CSSPropertyWebkitAnimationName,
        CSSPropertyWebkitAnimationDuration,
        CSSPropertyWebkitAnimationTimingFunction,
        CSSPropertyWebkitAnimationDelay,
        CSSPropertyWebkitAnimationIterationCount,
        CSSPropertyWebkitAnimationDirection,
        CSSPropertyWebkitAnimationFillMode
    };
    return StylePropertyShorthand(CSSPropertyWebkitAnimation, animationProperties, WTF_ARRAY_LENGTH(animationProperties));
}

StylePropertyShorthand webkitAnimationShorthandForParsing()
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
        CSSPropertyWebkitAnimationDuration,
        CSSPropertyWebkitAnimationTimingFunction,
        CSSPropertyWebkitAnimationDelay,
        CSSPropertyWebkitAnimationIterationCount,
        CSSPropertyWebkitAnimationDirection,
        CSSPropertyWebkitAnimationFillMode,
        CSSPropertyWebkitAnimationName
    };

    return StylePropertyShorthand(CSSPropertyWebkitAnimation, animationPropertiesForParsing, WTF_ARRAY_LENGTH(animationPropertiesForParsing));
}

StylePropertyShorthand webkitBorderAfterShorthand()
{
    static const CSSPropertyID borderAfterProperties[] = { CSSPropertyWebkitBorderAfterWidth, CSSPropertyWebkitBorderAfterStyle, CSSPropertyWebkitBorderAfterColor  };
    return StylePropertyShorthand(CSSPropertyWebkitBorderAfter, borderAfterProperties, WTF_ARRAY_LENGTH(borderAfterProperties));
}

StylePropertyShorthand webkitBorderBeforeShorthand()
{
    static const CSSPropertyID borderBeforeProperties[] = { CSSPropertyWebkitBorderBeforeWidth, CSSPropertyWebkitBorderBeforeStyle, CSSPropertyWebkitBorderBeforeColor  };
    return StylePropertyShorthand(CSSPropertyWebkitBorderBefore, borderBeforeProperties, WTF_ARRAY_LENGTH(borderBeforeProperties));
}

StylePropertyShorthand webkitBorderEndShorthand()
{
    static const CSSPropertyID borderEndProperties[] = { CSSPropertyWebkitBorderEndWidth, CSSPropertyWebkitBorderEndStyle, CSSPropertyWebkitBorderEndColor };
    return StylePropertyShorthand(CSSPropertyWebkitBorderEnd, borderEndProperties, WTF_ARRAY_LENGTH(borderEndProperties));
}

StylePropertyShorthand webkitBorderStartShorthand()
{
    static const CSSPropertyID borderStartProperties[] = { CSSPropertyWebkitBorderStartWidth, CSSPropertyWebkitBorderStartStyle, CSSPropertyWebkitBorderStartColor };
    return StylePropertyShorthand(CSSPropertyWebkitBorderStart, borderStartProperties, WTF_ARRAY_LENGTH(borderStartProperties));
}

StylePropertyShorthand webkitColumnsShorthand()
{
    static const CSSPropertyID columnsProperties[] = { CSSPropertyWebkitColumnWidth, CSSPropertyWebkitColumnCount };
    return StylePropertyShorthand(CSSPropertyWebkitColumns, columnsProperties, WTF_ARRAY_LENGTH(columnsProperties));
}

StylePropertyShorthand webkitColumnRuleShorthand()
{
    static const CSSPropertyID columnRuleProperties[] = {
        CSSPropertyWebkitColumnRuleWidth,
        CSSPropertyWebkitColumnRuleStyle,
        CSSPropertyWebkitColumnRuleColor,
    };
    return StylePropertyShorthand(CSSPropertyWebkitColumnRule, columnRuleProperties, WTF_ARRAY_LENGTH(columnRuleProperties));
}

StylePropertyShorthand webkitFlexFlowShorthand()
{
    static const CSSPropertyID flexFlowProperties[] = { CSSPropertyWebkitFlexDirection, CSSPropertyWebkitFlexWrap };
    return StylePropertyShorthand(CSSPropertyWebkitFlexFlow, flexFlowProperties, WTF_ARRAY_LENGTH(flexFlowProperties));
}

StylePropertyShorthand webkitFlexShorthand()
{
    static const CSSPropertyID flexProperties[] = { CSSPropertyWebkitFlexGrow, CSSPropertyWebkitFlexShrink, CSSPropertyWebkitFlexBasis };
    return StylePropertyShorthand(CSSPropertyWebkitFlex, flexProperties, WTF_ARRAY_LENGTH(flexProperties));
}

StylePropertyShorthand webkitMarginCollapseShorthand()
{
    static const CSSPropertyID marginCollapseProperties[] = { CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginAfterCollapse };
    return StylePropertyShorthand(CSSPropertyWebkitMarginCollapse, marginCollapseProperties, WTF_ARRAY_LENGTH(marginCollapseProperties));
}

#if ENABLE(CSS_GRID_LAYOUT)
StylePropertyShorthand webkitGridAreaShorthand()
{
    static const CSSPropertyID webkitGridAreaProperties[] = {
        CSSPropertyWebkitGridRowStart,
        CSSPropertyWebkitGridColumnStart,
        CSSPropertyWebkitGridRowEnd,
        CSSPropertyWebkitGridColumnEnd
    };
    return StylePropertyShorthand(CSSPropertyWebkitGridArea, webkitGridAreaProperties, WTF_ARRAY_LENGTH(webkitGridAreaProperties));
}

StylePropertyShorthand webkitGridColumnShorthand()
{
    static const CSSPropertyID webkitGridColumnProperties[] = {
        CSSPropertyWebkitGridColumnStart,
        CSSPropertyWebkitGridColumnEnd
    };
    return StylePropertyShorthand(CSSPropertyWebkitGridColumn, webkitGridColumnProperties, WTF_ARRAY_LENGTH(webkitGridColumnProperties));

}

StylePropertyShorthand webkitGridRowShorthand()
{
    static const CSSPropertyID webkitGridRowProperties[] = {
        CSSPropertyWebkitGridRowStart,
        CSSPropertyWebkitGridRowEnd
    };
    return StylePropertyShorthand(CSSPropertyWebkitGridRow, webkitGridRowProperties, WTF_ARRAY_LENGTH(webkitGridRowProperties));

}
#endif /* ENABLE(CSS_GRID_LAYOUT) */

StylePropertyShorthand webkitMarqueeShorthand()
{
    static const CSSPropertyID marqueeProperties[] = {
        CSSPropertyWebkitMarqueeDirection,
        CSSPropertyWebkitMarqueeIncrement,
        CSSPropertyWebkitMarqueeRepetition,
        CSSPropertyWebkitMarqueeStyle,
        CSSPropertyWebkitMarqueeSpeed
    };
    return StylePropertyShorthand(CSSPropertyWebkitMarquee, marqueeProperties, WTF_ARRAY_LENGTH(marqueeProperties));
}

StylePropertyShorthand webkitMaskShorthand()
{
    static const CSSPropertyID maskProperties[] = {
        CSSPropertyWebkitMaskImage,
        CSSPropertyWebkitMaskSourceType,
        CSSPropertyWebkitMaskPositionX,
        CSSPropertyWebkitMaskPositionY,
        CSSPropertyWebkitMaskSize,
        CSSPropertyWebkitMaskRepeatX,
        CSSPropertyWebkitMaskRepeatY,
        CSSPropertyWebkitMaskOrigin,
        CSSPropertyWebkitMaskClip
    };
    return StylePropertyShorthand(CSSPropertyWebkitMask, maskProperties, WTF_ARRAY_LENGTH(maskProperties));
}

StylePropertyShorthand webkitMaskPositionShorthand()
{
    static const CSSPropertyID maskPositionProperties[] = { CSSPropertyWebkitMaskPositionX, CSSPropertyWebkitMaskPositionY };
    return StylePropertyShorthand(CSSPropertyWebkitMaskPosition, maskPositionProperties, WTF_ARRAY_LENGTH(maskPositionProperties));
}

StylePropertyShorthand webkitMaskRepeatShorthand()
{
    static const CSSPropertyID maskRepeatProperties[] = { CSSPropertyWebkitMaskRepeatX, CSSPropertyWebkitMaskRepeatY };
    return StylePropertyShorthand(CSSPropertyWebkitMaskRepeat, maskRepeatProperties, WTF_ARRAY_LENGTH(maskRepeatProperties));
}

StylePropertyShorthand webkitTextDecorationShorthand()
{
    static const CSSPropertyID textDecorationProperties[] = {
        CSSPropertyWebkitTextDecorationLine,
        CSSPropertyWebkitTextDecorationStyle,
        CSSPropertyWebkitTextDecorationColor
    };
    return StylePropertyShorthand(CSSPropertyWebkitTextDecoration, textDecorationProperties, WTF_ARRAY_LENGTH(textDecorationProperties));
}

StylePropertyShorthand webkitTextEmphasisShorthand()
{
    static const CSSPropertyID textEmphasisProperties[] = {
        CSSPropertyWebkitTextEmphasisStyle,
        CSSPropertyWebkitTextEmphasisColor
    };
    return StylePropertyShorthand(CSSPropertyWebkitTextEmphasis, textEmphasisProperties, WTF_ARRAY_LENGTH(textEmphasisProperties));
}

StylePropertyShorthand webkitTextStrokeShorthand()
{
    static const CSSPropertyID textStrokeProperties[] = { CSSPropertyWebkitTextStrokeWidth, CSSPropertyWebkitTextStrokeColor };
    return StylePropertyShorthand(CSSPropertyWebkitTextStroke, textStrokeProperties, WTF_ARRAY_LENGTH(textStrokeProperties));
}

StylePropertyShorthand webkitTransitionShorthand()
{
    static const CSSPropertyID transitionProperties[] = {
        CSSPropertyWebkitTransitionProperty,
        CSSPropertyWebkitTransitionDuration,
        CSSPropertyWebkitTransitionTimingFunction,
        CSSPropertyWebkitTransitionDelay
    };
    return StylePropertyShorthand(CSSPropertyWebkitTransition, transitionProperties, WTF_ARRAY_LENGTH(transitionProperties));
}

StylePropertyShorthand webkitTransformOriginShorthand()
{
    static const CSSPropertyID transformOriginProperties[] = {
        CSSPropertyWebkitTransformOriginX,
        CSSPropertyWebkitTransformOriginY,
        CSSPropertyWebkitTransformOriginZ
    };
    return StylePropertyShorthand(CSSPropertyWebkitTransformOrigin, transformOriginProperties, WTF_ARRAY_LENGTH(transformOriginProperties));
}

StylePropertyShorthand widthShorthand()
{
    static const CSSPropertyID widthProperties[] = {
        CSSPropertyMinWidth,
        CSSPropertyMaxWidth
    };
    return StylePropertyShorthand(CSSPropertyWidth, widthProperties, WTF_ARRAY_LENGTH(widthProperties));
}

StylePropertyShorthand heightShorthand()
{
    static const CSSPropertyID heightProperties[] = {
        CSSPropertyMinHeight,
        CSSPropertyMaxHeight
    };
    return StylePropertyShorthand(CSSPropertyHeight, heightProperties, WTF_ARRAY_LENGTH(heightProperties));
}

// Returns an empty list if the property is not a shorthand.
StylePropertyShorthand shorthandForProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
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
    case CSSPropertyWebkitColumns:
        return webkitColumnsShorthand();
    case CSSPropertyWebkitColumnRule:
        return webkitColumnRuleShorthand();
    case CSSPropertyWebkitFlex:
        return webkitFlexShorthand();
    case CSSPropertyWebkitFlexFlow:
        return webkitFlexFlowShorthand();
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridArea:
        return webkitGridAreaShorthand();
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

Vector<StylePropertyShorthand> matchingShorthandsForLonghand(CSSPropertyID propertyID)
{
    switch (propertyID) {
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
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWebkitColumnCount:
        return makeVector(webkitColumnsShorthand());
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleColor:
        return makeVector(webkitColumnRuleShorthand());
    case CSSPropertyWebkitFlexGrow:
    case CSSPropertyWebkitFlexShrink:
    case CSSPropertyWebkitFlexBasis:
        return makeVector(webkitFlexShorthand());
    case CSSPropertyWebkitFlexDirection:
    case CSSPropertyWebkitFlexWrap:
        return makeVector(webkitFlexFlowShorthand());
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridColumnStart:
    case CSSPropertyWebkitGridColumnEnd:
        return makeVector(webkitGridColumnShorthand());
    case CSSPropertyWebkitGridRowStart:
    case CSSPropertyWebkitGridRowEnd:
        return makeVector(webkitGridRowShorthand());
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
