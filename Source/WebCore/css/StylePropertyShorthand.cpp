/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

const StylePropertyShorthand& backgroundShorthand()
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
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, backgroundShorthand, (CSSPropertyBackground, backgroundProperties, WTF_ARRAY_LENGTH(backgroundProperties)));
    return backgroundShorthand;
}

const StylePropertyShorthand& backgroundPositionShorthand()
{
    static const CSSPropertyID backgroundPositionProperties[] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, backgroundPositionLonghands, (CSSPropertyBackgroundPosition, backgroundPositionProperties, WTF_ARRAY_LENGTH(backgroundPositionProperties)));
    return backgroundPositionLonghands;
}

const StylePropertyShorthand& backgroundRepeatShorthand()
{
    static const CSSPropertyID backgroundRepeatProperties[] = { CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, backgroundRepeatLonghands, (CSSPropertyBackgroundRepeat, backgroundRepeatProperties, WTF_ARRAY_LENGTH(backgroundRepeatProperties)));
    return backgroundRepeatLonghands;
}

const StylePropertyShorthand& borderShorthand()
{
    // Do not change the order of the following four shorthands, and keep them together.
    static const CSSPropertyID borderProperties[4][3] = {
        { CSSPropertyBorderTopColor, CSSPropertyBorderTopStyle, CSSPropertyBorderTopWidth },
        { CSSPropertyBorderRightColor, CSSPropertyBorderRightStyle, CSSPropertyBorderRightWidth },
        { CSSPropertyBorderBottomColor, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomWidth },
        { CSSPropertyBorderLeftColor, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftWidth }
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderLonghands, (CSSPropertyBorder, borderProperties[0], sizeof(borderProperties) / sizeof(borderProperties[0][0])));
    return borderLonghands;
}

const StylePropertyShorthand& borderAbridgedShorthand()
{
    static const CSSPropertyID borderAbridgedProperties[] = { CSSPropertyBorderWidth, CSSPropertyBorderStyle, CSSPropertyBorderColor };
    static const StylePropertyShorthand* propertiesForInitialization[] = {
        &borderWidthShorthand(),
        &borderStyleShorthand(),
        &borderColorShorthand(),
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderAbridgedLonghands,
        (CSSPropertyBorder, borderAbridgedProperties, propertiesForInitialization, WTF_ARRAY_LENGTH(borderAbridgedProperties)));
    return borderAbridgedLonghands;
}

const StylePropertyShorthand& borderBottomShorthand()
{
    static const CSSPropertyID borderBottomProperties[] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderBottomLonghands, (CSSPropertyBorderBottom, borderBottomProperties, WTF_ARRAY_LENGTH(borderBottomProperties)));
    return borderBottomLonghands;
}

const StylePropertyShorthand& borderColorShorthand()
{
    static const CSSPropertyID borderColorProperties[] = {
        CSSPropertyBorderTopColor,
        CSSPropertyBorderRightColor,
        CSSPropertyBorderBottomColor,
        CSSPropertyBorderLeftColor
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderColorLonghands, (CSSPropertyBorderColor, borderColorProperties, WTF_ARRAY_LENGTH(borderColorProperties)));
    return borderColorLonghands;
}

const StylePropertyShorthand& borderImageShorthand()
{
    static const CSSPropertyID borderImageProperties[] = {
        CSSPropertyBorderImageSource,
        CSSPropertyBorderImageSlice,
        CSSPropertyBorderImageWidth,
        CSSPropertyBorderImageOutset,
        CSSPropertyBorderImageRepeat
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderImageLonghands, (CSSPropertyBorderImage, borderImageProperties, WTF_ARRAY_LENGTH(borderImageProperties)));
    return borderImageLonghands;
}

const StylePropertyShorthand& borderLeftShorthand()
{
    static const CSSPropertyID borderLeftProperties[] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderLeftLonghands, (CSSPropertyBorderLeft, borderLeftProperties, WTF_ARRAY_LENGTH(borderLeftProperties)));
    return borderLeftLonghands;
}

const StylePropertyShorthand& borderRadiusShorthand()
{
    static const CSSPropertyID borderRadiusProperties[] = {
        CSSPropertyBorderTopLeftRadius,
        CSSPropertyBorderTopRightRadius,
        CSSPropertyBorderBottomRightRadius,
        CSSPropertyBorderBottomLeftRadius
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderRadiusLonghands, (CSSPropertyBorderRadius, borderRadiusProperties, WTF_ARRAY_LENGTH(borderRadiusProperties)));
    return borderRadiusLonghands;
}

const StylePropertyShorthand& webkitBorderRadiusShorthand()
{
    static const CSSPropertyID borderRadiusProperties[] = {
        CSSPropertyBorderTopLeftRadius,
        CSSPropertyBorderTopRightRadius,
        CSSPropertyBorderBottomRightRadius,
        CSSPropertyBorderBottomLeftRadius
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderRadiusLonghands, (CSSPropertyWebkitBorderRadius, borderRadiusProperties, WTF_ARRAY_LENGTH(borderRadiusProperties)));
    return borderRadiusLonghands;
}

const StylePropertyShorthand& borderRightShorthand()
{
    static const CSSPropertyID borderRightProperties[] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle, CSSPropertyBorderRightColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderRightLonghands, (CSSPropertyBorderRight, borderRightProperties, WTF_ARRAY_LENGTH(borderRightProperties)));
    return borderRightLonghands;
}

const StylePropertyShorthand& borderSpacingShorthand()
{
    static const CSSPropertyID borderSpacingProperties[] = { CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderVerticalSpacing };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderSpacingLonghands, (CSSPropertyBorderSpacing, borderSpacingProperties, WTF_ARRAY_LENGTH(borderSpacingProperties)));
    return borderSpacingLonghands;
}

const StylePropertyShorthand& borderStyleShorthand()
{
    static const CSSPropertyID borderStyleProperties[] = {
        CSSPropertyBorderTopStyle,
        CSSPropertyBorderRightStyle,
        CSSPropertyBorderBottomStyle,
        CSSPropertyBorderLeftStyle
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderStyleLonghands, (CSSPropertyBorderStyle, borderStyleProperties, WTF_ARRAY_LENGTH(borderStyleProperties)));
    return borderStyleLonghands;
}

const StylePropertyShorthand& borderTopShorthand()
{
    static const CSSPropertyID borderTopProperties[] = { CSSPropertyBorderTopWidth, CSSPropertyBorderTopStyle, CSSPropertyBorderTopColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderTopLonghands, (CSSPropertyBorderTop, borderTopProperties, WTF_ARRAY_LENGTH(borderTopProperties)));
    return borderTopLonghands;
}

const StylePropertyShorthand& borderWidthShorthand()
{
    static const CSSPropertyID borderWidthProperties[] = {
        CSSPropertyBorderTopWidth,
        CSSPropertyBorderRightWidth,
        CSSPropertyBorderBottomWidth,
        CSSPropertyBorderLeftWidth
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderWidthLonghands, (CSSPropertyBorderWidth, borderWidthProperties, WTF_ARRAY_LENGTH(borderWidthProperties)));
    return borderWidthLonghands;
}

const StylePropertyShorthand& listStyleShorthand()
{
    static const CSSPropertyID listStyleProperties[] = {
        CSSPropertyListStyleType,
        CSSPropertyListStylePosition,
        CSSPropertyListStyleImage
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, listStyleLonghands, (CSSPropertyListStyle, listStyleProperties, WTF_ARRAY_LENGTH(listStyleProperties)));
    return listStyleLonghands;
}

const StylePropertyShorthand& fontShorthand()
{
    static const CSSPropertyID fontProperties[] = {
        CSSPropertyFontFamily,
        CSSPropertyFontSize,
        CSSPropertyFontStyle,
        CSSPropertyFontVariant,
        CSSPropertyFontWeight,
        CSSPropertyLineHeight
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, fontLonghands, (CSSPropertyFont, fontProperties, WTF_ARRAY_LENGTH(fontProperties)));
    return fontLonghands;
}

const StylePropertyShorthand& marginShorthand()
{
    static const CSSPropertyID marginProperties[] = {
        CSSPropertyMarginTop,
        CSSPropertyMarginRight,
        CSSPropertyMarginBottom,
        CSSPropertyMarginLeft
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, marginLonghands, (CSSPropertyMargin, marginProperties, WTF_ARRAY_LENGTH(marginProperties)));
    return marginLonghands;
}

#if ENABLE(SVG)
const StylePropertyShorthand& markerShorthand()
{
    static const CSSPropertyID markerProperties[] = {
        CSSPropertyMarkerStart,
        CSSPropertyMarkerMid,
        CSSPropertyMarkerEnd
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, markerLonghands, (CSSPropertyMarker, markerProperties, WTF_ARRAY_LENGTH(markerProperties)));
    return markerLonghands;
}
#endif

const StylePropertyShorthand& outlineShorthand()
{
    static const CSSPropertyID outlineProperties[] = {
        CSSPropertyOutlineColor,
        CSSPropertyOutlineStyle,
        CSSPropertyOutlineWidth
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, outlineLonghands, (CSSPropertyOutline, outlineProperties, WTF_ARRAY_LENGTH(outlineProperties)));
    return outlineLonghands;
}

const StylePropertyShorthand& overflowShorthand()
{
    static const CSSPropertyID overflowProperties[] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, overflowLonghands, (CSSPropertyOverflow, overflowProperties, WTF_ARRAY_LENGTH(overflowProperties)));
    return overflowLonghands;
}

const StylePropertyShorthand& paddingShorthand()
{
    static const CSSPropertyID paddingProperties[] = {
        CSSPropertyPaddingTop,
        CSSPropertyPaddingRight,
        CSSPropertyPaddingBottom,
        CSSPropertyPaddingLeft
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, paddingLonghands, (CSSPropertyPadding, paddingProperties, WTF_ARRAY_LENGTH(paddingProperties)));
    return paddingLonghands;
}

const StylePropertyShorthand& transitionShorthand()
{
    static const CSSPropertyID transitionProperties[] = {
        CSSPropertyTransitionProperty,
        CSSPropertyTransitionDuration,
        CSSPropertyTransitionTimingFunction,
        CSSPropertyTransitionDelay
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, transitionLonghands, (CSSPropertyTransition, transitionProperties, WTF_ARRAY_LENGTH(transitionProperties)));
    return transitionLonghands;
}

const StylePropertyShorthand& webkitAnimationShorthand()
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
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitAnimationLonghands, (CSSPropertyWebkitAnimation, animationProperties, WTF_ARRAY_LENGTH(animationProperties)));
    return webkitAnimationLonghands;
}

const StylePropertyShorthand& webkitAnimationShorthandForParsing()
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

    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitAnimationLonghandsForParsing, (CSSPropertyWebkitAnimation, animationPropertiesForParsing, WTF_ARRAY_LENGTH(animationPropertiesForParsing)));
    return webkitAnimationLonghandsForParsing;
}

const StylePropertyShorthand& webkitBorderAfterShorthand()
{
    static const CSSPropertyID borderAfterProperties[] = { CSSPropertyWebkitBorderAfterWidth, CSSPropertyWebkitBorderAfterStyle, CSSPropertyWebkitBorderAfterColor  };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitBorderAfterLonghands, (CSSPropertyWebkitBorderAfter, borderAfterProperties, WTF_ARRAY_LENGTH(borderAfterProperties)));
    return webkitBorderAfterLonghands;
}

const StylePropertyShorthand& webkitBorderBeforeShorthand()
{
    static const CSSPropertyID borderBeforeProperties[] = { CSSPropertyWebkitBorderBeforeWidth, CSSPropertyWebkitBorderBeforeStyle, CSSPropertyWebkitBorderBeforeColor  };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitBorderBeforeLonghands, (CSSPropertyWebkitBorderBefore, borderBeforeProperties, WTF_ARRAY_LENGTH(borderBeforeProperties)));
    return webkitBorderBeforeLonghands;
}

const StylePropertyShorthand& webkitBorderEndShorthand()
{
    static const CSSPropertyID borderEndProperties[] = { CSSPropertyWebkitBorderEndWidth, CSSPropertyWebkitBorderEndStyle, CSSPropertyWebkitBorderEndColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitBorderEndLonghands, (CSSPropertyWebkitBorderEnd, borderEndProperties, WTF_ARRAY_LENGTH(borderEndProperties)));
    return webkitBorderEndLonghands;
}

const StylePropertyShorthand& webkitBorderStartShorthand()
{
    static const CSSPropertyID borderStartProperties[] = { CSSPropertyWebkitBorderStartWidth, CSSPropertyWebkitBorderStartStyle, CSSPropertyWebkitBorderStartColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitBorderStartLonghands, (CSSPropertyWebkitBorderStart, borderStartProperties, WTF_ARRAY_LENGTH(borderStartProperties)));
    return webkitBorderStartLonghands;
}

const StylePropertyShorthand& webkitColumnsShorthand()
{
    static const CSSPropertyID columnsProperties[] = { CSSPropertyWebkitColumnWidth, CSSPropertyWebkitColumnCount };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitColumnsLonghands, (CSSPropertyWebkitColumns, columnsProperties, WTF_ARRAY_LENGTH(columnsProperties)));
    return webkitColumnsLonghands;
}

const StylePropertyShorthand& webkitColumnRuleShorthand()
{
    static const CSSPropertyID columnRuleProperties[] = {
        CSSPropertyWebkitColumnRuleWidth,
        CSSPropertyWebkitColumnRuleStyle,
        CSSPropertyWebkitColumnRuleColor,
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitColumnRuleLonghands, (CSSPropertyWebkitColumnRule, columnRuleProperties, WTF_ARRAY_LENGTH(columnRuleProperties)));
    return webkitColumnRuleLonghands;
}

const StylePropertyShorthand& webkitFlexFlowShorthand()
{
    static const CSSPropertyID flexFlowProperties[] = { CSSPropertyWebkitFlexDirection, CSSPropertyWebkitFlexWrap };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitFlexFlowLonghands, (CSSPropertyWebkitFlexFlow, flexFlowProperties, WTF_ARRAY_LENGTH(flexFlowProperties)));
    return webkitFlexFlowLonghands;
}

const StylePropertyShorthand& webkitFlexShorthand()
{
    static const CSSPropertyID flexProperties[] = { CSSPropertyWebkitFlexGrow, CSSPropertyWebkitFlexShrink, CSSPropertyWebkitFlexBasis };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitFlexLonghands, (CSSPropertyWebkitFlex, flexProperties, WTF_ARRAY_LENGTH(flexProperties)));
    return webkitFlexLonghands;
}

const StylePropertyShorthand& webkitMarginCollapseShorthand()
{
    static const CSSPropertyID marginCollapseProperties[] = { CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginAfterCollapse };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitMarginCollapseLonghands, (CSSPropertyWebkitMarginCollapse, marginCollapseProperties, WTF_ARRAY_LENGTH(marginCollapseProperties)));
    return webkitMarginCollapseLonghands;
}

const StylePropertyShorthand& webkitGridColumnShorthand()
{
    static const CSSPropertyID webkitGridColumnProperties[] = {
        CSSPropertyWebkitGridColumnStart,
        CSSPropertyWebkitGridColumnEnd
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitGridColumnLonghands, (CSSPropertyWebkitGridColumn, webkitGridColumnProperties, WTF_ARRAY_LENGTH(webkitGridColumnProperties)));
    return webkitGridColumnLonghands;

}

const StylePropertyShorthand& webkitGridRowShorthand()
{
    static const CSSPropertyID webkitGridRowProperties[] = {
        CSSPropertyWebkitGridRowStart,
        CSSPropertyWebkitGridRowEnd
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitGridRowLonghands, (CSSPropertyWebkitGridRow, webkitGridRowProperties, WTF_ARRAY_LENGTH(webkitGridRowProperties)));
    return webkitGridRowLonghands;

}

const StylePropertyShorthand& webkitMarqueeShorthand()
{
    static const CSSPropertyID marqueeProperties[] = {
        CSSPropertyWebkitMarqueeDirection,
        CSSPropertyWebkitMarqueeIncrement,
        CSSPropertyWebkitMarqueeRepetition,
        CSSPropertyWebkitMarqueeStyle,
        CSSPropertyWebkitMarqueeSpeed
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitMarqueeLonghands, (CSSPropertyWebkitMarquee, marqueeProperties, WTF_ARRAY_LENGTH(marqueeProperties)));
    return webkitMarqueeLonghands;
}

const StylePropertyShorthand& webkitMaskShorthand()
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
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitMaskLonghands, (CSSPropertyWebkitMask, maskProperties, WTF_ARRAY_LENGTH(maskProperties)));
    return webkitMaskLonghands;
}

const StylePropertyShorthand& webkitMaskPositionShorthand()
{
    static const CSSPropertyID maskPositionProperties[] = { CSSPropertyWebkitMaskPositionX, CSSPropertyWebkitMaskPositionY };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitMaskPositionLonghands, (CSSPropertyWebkitMaskPosition, maskPositionProperties, WTF_ARRAY_LENGTH(maskPositionProperties)));
    return webkitMaskPositionLonghands;
}

const StylePropertyShorthand& webkitMaskRepeatShorthand()
{
    static const CSSPropertyID maskRepeatProperties[] = { CSSPropertyWebkitMaskRepeatX, CSSPropertyWebkitMaskRepeatY };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitMaskRepeatLonghands, (CSSPropertyWebkitMaskRepeat, maskRepeatProperties, WTF_ARRAY_LENGTH(maskRepeatProperties)));
    return webkitMaskRepeatLonghands;
}

#if ENABLE(CSS3_TEXT)
const StylePropertyShorthand& webkitTextDecorationShorthand()
{
    static const CSSPropertyID textDecorationProperties[] = {
        CSSPropertyWebkitTextDecorationLine,
        CSSPropertyWebkitTextDecorationStyle,
        CSSPropertyWebkitTextDecorationColor
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitTextDecorationLonghands, (CSSPropertyWebkitTextDecoration, textDecorationProperties, WTF_ARRAY_LENGTH(textDecorationProperties)));
    return webkitTextDecorationLonghands;
}
#endif

const StylePropertyShorthand& webkitTextEmphasisShorthand()
{
    static const CSSPropertyID textEmphasisProperties[] = {
        CSSPropertyWebkitTextEmphasisStyle,
        CSSPropertyWebkitTextEmphasisColor
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitTextEmphasisLonghands, (CSSPropertyWebkitTextEmphasis, textEmphasisProperties, WTF_ARRAY_LENGTH(textEmphasisProperties)));
    return webkitTextEmphasisLonghands;
}

const StylePropertyShorthand& webkitTextStrokeShorthand()
{
    static const CSSPropertyID textStrokeProperties[] = { CSSPropertyWebkitTextStrokeWidth, CSSPropertyWebkitTextStrokeColor };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitTextStrokeLonghands, (CSSPropertyWebkitTextStroke, textStrokeProperties, WTF_ARRAY_LENGTH(textStrokeProperties)));
    return webkitTextStrokeLonghands;
}

const StylePropertyShorthand& webkitTransitionShorthand()
{
    static const CSSPropertyID transitionProperties[] = {
        CSSPropertyWebkitTransitionProperty,
        CSSPropertyWebkitTransitionDuration,
        CSSPropertyWebkitTransitionTimingFunction,
        CSSPropertyWebkitTransitionDelay
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitTransitionLonghands, (CSSPropertyWebkitTransition, transitionProperties, WTF_ARRAY_LENGTH(transitionProperties)));
    return webkitTransitionLonghands;
}

const StylePropertyShorthand& webkitTransformOriginShorthand()
{
    static const CSSPropertyID transformOriginProperties[] = {
        CSSPropertyWebkitTransformOriginX,
        CSSPropertyWebkitTransformOriginY,
        CSSPropertyWebkitTransformOriginZ
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, webkitTransformOriginLonghands, (CSSPropertyWebkitTransformOrigin, transformOriginProperties, WTF_ARRAY_LENGTH(transformOriginProperties)));
    return webkitTransformOriginLonghands;
}

const StylePropertyShorthand& widthShorthand()
{
    static const CSSPropertyID widthProperties[] = {
        CSSPropertyMinWidth,
        CSSPropertyMaxWidth
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, widthLonghands, (CSSPropertyWidth, widthProperties, WTF_ARRAY_LENGTH(widthProperties)));
    return widthLonghands;
}

const StylePropertyShorthand& heightShorthand()
{
    static const CSSPropertyID heightProperties[] = {
        CSSPropertyMinHeight,
        CSSPropertyMaxHeight
    };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, heightLonghands, (CSSPropertyHeight, heightProperties, WTF_ARRAY_LENGTH(heightProperties)));
    return heightLonghands;
}

// Returns an empty list if the property is not a shorthand
const StylePropertyShorthand& shorthandForProperty(CSSPropertyID propertyID)
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
    case CSSPropertyWebkitGridColumn:
        return webkitGridColumnShorthand();
    case CSSPropertyWebkitGridRow:
        return webkitGridRowShorthand();
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
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextDecoration:
        return webkitTextDecorationShorthand();
#endif
    case CSSPropertyWebkitTextEmphasis:
        return webkitTextEmphasisShorthand();
    case CSSPropertyWebkitTextStroke:
        return webkitTextStrokeShorthand();
    case CSSPropertyWebkitTransition:
        return webkitTransitionShorthand();
    case CSSPropertyWebkitTransformOrigin:
        return webkitTransformOriginShorthand();
    default: {
        DEFINE_STATIC_LOCAL(StylePropertyShorthand, emptyShorthand, ());
        return emptyShorthand;
    }
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

// FIXME : We need to generate all of this.
typedef HashMap<CSSPropertyID, Vector<const StylePropertyShorthand*> > longhandsMap;
const Vector<const StylePropertyShorthand*> matchingShorthandsForLonghand(CSSPropertyID propertyID)
{
    DEFINE_STATIC_LOCAL(longhandsMap, map, ());
    if (map.isEmpty()) {
        Vector<const StylePropertyShorthand*, 1> background;
        background.uncheckedAppend(&backgroundShorthand());
        map.set(CSSPropertyBackgroundImage, background);
        map.set(CSSPropertyBackgroundSize, background);
        map.set(CSSPropertyBackgroundAttachment, background);
        map.set(CSSPropertyBackgroundOrigin, background);
        map.set(CSSPropertyBackgroundClip, background);
        map.set(CSSPropertyBackgroundColor, background);

        Vector<const StylePropertyShorthand*, 2> positionShorthands;
        positionShorthands.uncheckedAppend(&backgroundShorthand());
        positionShorthands.uncheckedAppend(&backgroundPositionShorthand());
        map.set(CSSPropertyBackgroundPositionX, positionShorthands);
        map.set(CSSPropertyBackgroundPositionY, positionShorthands);

        Vector<const StylePropertyShorthand*, 2> repeatShorthands;
        repeatShorthands.uncheckedAppend(&backgroundShorthand());
        repeatShorthands.uncheckedAppend(&backgroundRepeatShorthand());
        map.set(CSSPropertyBackgroundRepeatX, repeatShorthands);
        map.set(CSSPropertyBackgroundRepeatY, repeatShorthands);

        Vector<const StylePropertyShorthand*, 3> bottomWidthShorthands;
        bottomWidthShorthands.uncheckedAppend(&borderShorthand());
        bottomWidthShorthands.uncheckedAppend(&borderBottomShorthand());
        bottomWidthShorthands.uncheckedAppend(&borderWidthShorthand());
        map.set(CSSPropertyBorderBottomWidth, bottomWidthShorthands);

        Vector<const StylePropertyShorthand*, 3> topColorShorthands;
        topColorShorthands.uncheckedAppend(&borderShorthand());
        topColorShorthands.uncheckedAppend(&borderTopShorthand());
        topColorShorthands.uncheckedAppend(&borderColorShorthand());
        map.set(CSSPropertyBorderTopColor, topColorShorthands);

        Vector<const StylePropertyShorthand*, 3> rightColorShorthands;
        rightColorShorthands.uncheckedAppend(&borderShorthand());
        rightColorShorthands.uncheckedAppend(&borderRightShorthand());
        rightColorShorthands.uncheckedAppend(&borderColorShorthand());
        map.set(CSSPropertyBorderRightColor, rightColorShorthands);

        Vector<const StylePropertyShorthand*, 3> leftColorShorthands;
        leftColorShorthands.uncheckedAppend(&borderShorthand());
        leftColorShorthands.uncheckedAppend(&borderLeftShorthand());
        leftColorShorthands.uncheckedAppend(&borderColorShorthand());
        map.set(CSSPropertyBorderLeftColor, leftColorShorthands);

        Vector<const StylePropertyShorthand*, 3> bottomColorShorthands;
        bottomColorShorthands.uncheckedAppend(&borderShorthand());
        bottomColorShorthands.uncheckedAppend(&borderBottomShorthand());
        bottomColorShorthands.uncheckedAppend(&borderColorShorthand());
        map.set(CSSPropertyBorderBottomColor, bottomColorShorthands);

        Vector<const StylePropertyShorthand*, 1> borderImage;
        borderImage.uncheckedAppend(&borderImageShorthand());
        map.set(CSSPropertyBorderImageSource, borderImage);
        map.set(CSSPropertyBorderImageSlice, borderImage);
        map.set(CSSPropertyBorderImageWidth, borderImage);
        map.set(CSSPropertyBorderImageOutset, borderImage);
        map.set(CSSPropertyBorderImageRepeat, borderImage);

        Vector<const StylePropertyShorthand*, 3> leftWidthShorthands;
        leftWidthShorthands.uncheckedAppend(&borderShorthand());
        leftWidthShorthands.uncheckedAppend(&borderLeftShorthand());
        leftWidthShorthands.uncheckedAppend(&borderWidthShorthand());
        map.set(CSSPropertyBorderLeftWidth, leftWidthShorthands);

        Vector<const StylePropertyShorthand*, 2> radiusShorthands;
        radiusShorthands.uncheckedAppend(&borderRadiusShorthand());
        radiusShorthands.uncheckedAppend(&webkitBorderRadiusShorthand());
        map.set(CSSPropertyBorderTopLeftRadius, radiusShorthands);
        map.set(CSSPropertyBorderTopRightRadius, radiusShorthands);
        map.set(CSSPropertyBorderBottomRightRadius, radiusShorthands);
        map.set(CSSPropertyBorderBottomLeftRadius, radiusShorthands);

        Vector<const StylePropertyShorthand*, 3> rightWidthShorthands;
        rightWidthShorthands.uncheckedAppend(&borderShorthand());
        rightWidthShorthands.uncheckedAppend(&borderRightShorthand());
        rightWidthShorthands.uncheckedAppend(&borderWidthShorthand());
        map.set(CSSPropertyBorderRightWidth, rightWidthShorthands);

        Vector<const StylePropertyShorthand*, 1> spacingShorthand;
        spacingShorthand.uncheckedAppend(&borderSpacingShorthand());
        map.set(CSSPropertyWebkitBorderHorizontalSpacing, spacingShorthand);
        map.set(CSSPropertyWebkitBorderVerticalSpacing, spacingShorthand);

        Vector<const StylePropertyShorthand*, 3> topStyleShorthands;
        topStyleShorthands.uncheckedAppend(&borderShorthand());
        topStyleShorthands.uncheckedAppend(&borderTopShorthand());
        topStyleShorthands.uncheckedAppend(&borderStyleShorthand());
        map.set(CSSPropertyBorderTopStyle, topStyleShorthands);

        Vector<const StylePropertyShorthand*, 3> bottomStyleShorthands;
        bottomStyleShorthands.uncheckedAppend(&borderShorthand());
        bottomStyleShorthands.uncheckedAppend(&borderBottomShorthand());
        bottomStyleShorthands.uncheckedAppend(&borderStyleShorthand());
        map.set(CSSPropertyBorderBottomStyle, bottomStyleShorthands);

        Vector<const StylePropertyShorthand*, 3> leftStyleShorthands;
        leftStyleShorthands.uncheckedAppend(&borderShorthand());
        leftStyleShorthands.uncheckedAppend(&borderLeftShorthand());
        leftStyleShorthands.uncheckedAppend(&borderStyleShorthand());
        map.set(CSSPropertyBorderLeftStyle, leftStyleShorthands);

        Vector<const StylePropertyShorthand*, 3> rightStyleShorthands;
        rightStyleShorthands.uncheckedAppend(&borderShorthand());
        rightStyleShorthands.uncheckedAppend(&borderRightShorthand());
        rightStyleShorthands.uncheckedAppend(&borderStyleShorthand());
        map.set(CSSPropertyBorderRightStyle, rightStyleShorthands);

        Vector<const StylePropertyShorthand*, 3> topWidthShorthands;
        topWidthShorthands.uncheckedAppend(&borderShorthand());
        topWidthShorthands.uncheckedAppend(&borderTopShorthand());
        topWidthShorthands.uncheckedAppend(&borderWidthShorthand());
        map.set(CSSPropertyBorderTopWidth, topWidthShorthands);

        Vector<const StylePropertyShorthand*, 1> listStyle;
        listStyle.uncheckedAppend(&listStyleShorthand());
        map.set(CSSPropertyListStyleType, listStyle);
        map.set(CSSPropertyListStylePosition, listStyle);
        map.set(CSSPropertyListStyleImage, listStyle);

        Vector<const StylePropertyShorthand*, 1> font;
        font.uncheckedAppend(&fontShorthand());
        map.set(CSSPropertyFontFamily, font);
        map.set(CSSPropertyFontSize, font);
        map.set(CSSPropertyFontStyle, font);
        map.set(CSSPropertyFontVariant, font);
        map.set(CSSPropertyFontWeight, font);
        map.set(CSSPropertyLineHeight, font);

        Vector<const StylePropertyShorthand*, 1> margin;
        margin.uncheckedAppend(&marginShorthand());
        map.set(CSSPropertyMarginTop, margin);
        map.set(CSSPropertyMarginRight, margin);
        map.set(CSSPropertyMarginBottom, margin);
        map.set(CSSPropertyMarginLeft, margin);

#if ENABLE(SVG)
        Vector<const StylePropertyShorthand*, 1> marker;
        marker.uncheckedAppend(&markerShorthand());
        map.set(CSSPropertyMarkerStart, marker);
        map.set(CSSPropertyMarkerMid, marker);
        map.set(CSSPropertyMarkerEnd, marker);
#endif

        Vector<const StylePropertyShorthand*, 1> outline;
        outline.uncheckedAppend(&outlineShorthand());
        map.set(CSSPropertyOutlineColor, outline);
        map.set(CSSPropertyOutlineStyle, outline);
        map.set(CSSPropertyOutlineWidth, outline);

        Vector<const StylePropertyShorthand*, 1> padding;
        padding.uncheckedAppend(&paddingShorthand());
        map.set(CSSPropertyPaddingTop, padding);
        map.set(CSSPropertyPaddingRight, padding);
        map.set(CSSPropertyPaddingBottom, padding);
        map.set(CSSPropertyPaddingLeft, padding);

        Vector<const StylePropertyShorthand*, 1> overflow;
        overflow.uncheckedAppend(&overflowShorthand());
        map.set(CSSPropertyOverflowX, overflow);
        map.set(CSSPropertyOverflowY, overflow);

        Vector<const StylePropertyShorthand*, 1> transition;
        transition.uncheckedAppend(&transitionShorthand());
        map.set(CSSPropertyTransitionProperty, transition);
        map.set(CSSPropertyTransitionDuration, transition);
        map.set(CSSPropertyTransitionTimingFunction, transition);
        map.set(CSSPropertyTransitionDelay, transition);

        Vector<const StylePropertyShorthand*, 1> animation;
        animation.uncheckedAppend(&webkitAnimationShorthand());
        map.set(CSSPropertyWebkitAnimationName, animation);
        map.set(CSSPropertyWebkitAnimationDuration, animation);
        map.set(CSSPropertyWebkitAnimationTimingFunction, animation);
        map.set(CSSPropertyWebkitAnimationDelay, animation);
        map.set(CSSPropertyWebkitAnimationIterationCount, animation);
        map.set(CSSPropertyWebkitAnimationDirection, animation);
        map.set(CSSPropertyWebkitAnimationFillMode, animation);

        Vector<const StylePropertyShorthand*, 1> borderAfter;
        borderAfter.uncheckedAppend(&webkitBorderAfterShorthand());
        map.set(CSSPropertyWebkitBorderAfterWidth, borderAfter);
        map.set(CSSPropertyWebkitBorderAfterStyle, borderAfter);
        map.set(CSSPropertyWebkitBorderAfterColor, borderAfter);

        Vector<const StylePropertyShorthand*, 1> borderBefore;
        borderBefore.uncheckedAppend(&webkitBorderBeforeShorthand());
        map.set(CSSPropertyWebkitBorderBeforeWidth, borderBefore);
        map.set(CSSPropertyWebkitBorderBeforeStyle, borderBefore);
        map.set(CSSPropertyWebkitBorderBeforeColor, borderBefore);

        Vector<const StylePropertyShorthand*, 1> borderEnd;
        borderEnd.uncheckedAppend(&webkitBorderEndShorthand());
        map.set(CSSPropertyWebkitBorderEndWidth, borderEnd);
        map.set(CSSPropertyWebkitBorderEndStyle, borderEnd);
        map.set(CSSPropertyWebkitBorderEndColor, borderEnd);

        Vector<const StylePropertyShorthand*, 1> borderStart;
        borderStart.uncheckedAppend(&webkitBorderStartShorthand());
        map.set(CSSPropertyWebkitBorderStartWidth, borderStart);
        map.set(CSSPropertyWebkitBorderStartStyle, borderStart);
        map.set(CSSPropertyWebkitBorderStartColor, borderStart);

        Vector<const StylePropertyShorthand*, 1> columns;
        columns.uncheckedAppend(&webkitColumnsShorthand());
        map.set(CSSPropertyWebkitColumnWidth, columns);
        map.set(CSSPropertyWebkitColumnCount, columns);

        Vector<const StylePropertyShorthand*, 1> columnRule;
        columnRule.uncheckedAppend(&webkitColumnRuleShorthand());
        map.set(CSSPropertyWebkitColumnRuleWidth, columnRule);
        map.set(CSSPropertyWebkitColumnRuleStyle, columnRule);
        map.set(CSSPropertyWebkitColumnRuleColor, columnRule);

        Vector<const StylePropertyShorthand*, 1> flex;
        flex.uncheckedAppend(&webkitFlexShorthand());
        map.set(CSSPropertyWebkitFlexGrow, flex);
        map.set(CSSPropertyWebkitFlexShrink, flex);
        map.set(CSSPropertyWebkitFlexBasis, flex);

        Vector<const StylePropertyShorthand*, 1> flexFlow;
        flexFlow.uncheckedAppend(&webkitFlexFlowShorthand());
        map.set(CSSPropertyWebkitFlexDirection, flexFlow);
        map.set(CSSPropertyWebkitFlexWrap, flexFlow);

        Vector<const StylePropertyShorthand*, 1> gridColumn;
        gridColumn.uncheckedAppend(&webkitGridColumnShorthand());
        map.set(CSSPropertyWebkitGridColumnStart, gridColumn);
        map.set(CSSPropertyWebkitGridColumnEnd, gridColumn);

        Vector<const StylePropertyShorthand*, 1> gridRow;
        gridRow.uncheckedAppend(&webkitGridRowShorthand());
        map.set(CSSPropertyWebkitGridRowStart, gridRow);
        map.set(CSSPropertyWebkitGridRowEnd, gridRow);

        Vector<const StylePropertyShorthand*, 1> marginCollapse;
        marginCollapse.uncheckedAppend(&webkitMarginCollapseShorthand());
        map.set(CSSPropertyWebkitMarginBeforeCollapse, marginCollapse);
        map.set(CSSPropertyWebkitMarginAfterCollapse, marginCollapse);

        Vector<const StylePropertyShorthand*, 1> marquee;
        marquee.uncheckedAppend(&webkitMarqueeShorthand());
        map.set(CSSPropertyWebkitMarqueeDirection, marquee);
        map.set(CSSPropertyWebkitMarqueeIncrement, marquee);
        map.set(CSSPropertyWebkitMarqueeRepetition, marquee);
        map.set(CSSPropertyWebkitMarqueeStyle, marquee);
        map.set(CSSPropertyWebkitMarqueeSpeed, marquee);

        Vector<const StylePropertyShorthand*, 1> mask;
        mask.uncheckedAppend(&webkitMaskShorthand());
        map.set(CSSPropertyWebkitMaskImage, mask);
        map.set(CSSPropertyWebkitMaskSourceType, mask);
        map.set(CSSPropertyWebkitMaskSize, mask);
        map.set(CSSPropertyWebkitMaskOrigin, mask);
        map.set(CSSPropertyWebkitMaskClip, mask);

        Vector<const StylePropertyShorthand*, 1> maskPosition;
        maskPosition.uncheckedAppend(&webkitMaskPositionShorthand());
        map.set(CSSPropertyWebkitMaskPositionX, maskPosition);
        map.set(CSSPropertyWebkitMaskPositionY, maskPosition);

        Vector<const StylePropertyShorthand*, 1> maskRepeat;
        maskRepeat.uncheckedAppend(&webkitMaskRepeatShorthand());
        map.set(CSSPropertyWebkitMaskRepeatX, maskRepeat);
        map.set(CSSPropertyWebkitMaskRepeatY, maskRepeat);

#if ENABLE(CSS3_TEXT)
        Vector<const StylePropertyShorthand*, 3> textDecoration;
        textDecoration.uncheckedAppend(&webkitTextDecorationShorthand());
        map.set(CSSPropertyWebkitTextDecorationLine, textDecoration);
        map.set(CSSPropertyWebkitTextDecorationStyle, textDecoration);
        map.set(CSSPropertyWebkitTextDecorationColor, textDecoration);
#endif

        Vector<const StylePropertyShorthand*, 1> textEmphasis;
        textEmphasis.uncheckedAppend(&webkitTextEmphasisShorthand());
        map.set(CSSPropertyWebkitTextEmphasisStyle, textEmphasis);
        map.set(CSSPropertyWebkitTextEmphasisColor, textEmphasis);

        Vector<const StylePropertyShorthand*, 1> textStroke;
        textStroke.uncheckedAppend(&webkitTextStrokeShorthand());
        map.set(CSSPropertyWebkitTextStrokeWidth, textStroke);
        map.set(CSSPropertyWebkitTextStrokeColor, textStroke);

        Vector<const StylePropertyShorthand*, 1> webkitTransition;
        webkitTransition.uncheckedAppend(&webkitTransitionShorthand());
        map.set(CSSPropertyWebkitTransitionProperty, webkitTransition);
        map.set(CSSPropertyWebkitTransitionDuration, webkitTransition);
        map.set(CSSPropertyWebkitTransitionTimingFunction, webkitTransition);
        map.set(CSSPropertyWebkitTransitionDelay, webkitTransition);

        Vector<const StylePropertyShorthand*, 1> transform;
        transform.uncheckedAppend(&webkitTransformOriginShorthand());
        map.set(CSSPropertyWebkitTransformOriginX, transform);
        map.set(CSSPropertyWebkitTransformOriginY, transform);
        map.set(CSSPropertyWebkitTransformOriginZ, transform);

        Vector<const StylePropertyShorthand*, 1> width;
        width.uncheckedAppend(&widthShorthand());
        map.set(CSSPropertyMinWidth, width);
        map.set(CSSPropertyMaxWidth, width);

        Vector<const StylePropertyShorthand*, 1> height;
        height.uncheckedAppend(&heightShorthand());
        map.set(CSSPropertyMinHeight, height);
        map.set(CSSPropertyMaxHeight, height);
    }
    return map.get(propertyID);
}

unsigned indexOfShorthandForLonghand(CSSPropertyID shorthandID, const Vector<const StylePropertyShorthand*>& shorthands)
{
    for (unsigned i = 0; i < shorthands.size(); ++i) {
        if (shorthands.at(i)->id() == shorthandID)
            return i;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
