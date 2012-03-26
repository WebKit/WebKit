/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "CSSPropertyLonghand.h"

#include "CSSPropertyNames.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

// FIXME: Add CSSPropertyBackgroundSize to the shorthand.
const CSSPropertyLonghand& backgroundLonghand()
{
    static const int backgroundProperties[] = {
        CSSPropertyBackgroundColor,
        CSSPropertyBackgroundImage,
        CSSPropertyBackgroundRepeatX,
        CSSPropertyBackgroundRepeatY,
        CSSPropertyBackgroundAttachment,
        CSSPropertyBackgroundPositionX,
        CSSPropertyBackgroundPositionY,
        CSSPropertyBackgroundClip,
        CSSPropertyBackgroundOrigin
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, backgroundLonghand, (backgroundProperties, WTF_ARRAY_LENGTH(backgroundProperties)));
    return backgroundLonghand;
}

const CSSPropertyLonghand& backgroundPositionLonghand()
{
    static const int backgroundPositionProperties[] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, backgroundPositionLonghand, (backgroundPositionProperties, WTF_ARRAY_LENGTH(backgroundPositionProperties)));
    return backgroundPositionLonghand;
}

const CSSPropertyLonghand& backgroundRepeatLonghand()
{
    static const int backgroundRepeatProperties[] = { CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, backgroundRepeatLonghand, (backgroundRepeatProperties, WTF_ARRAY_LENGTH(backgroundRepeatProperties)));
    return backgroundRepeatLonghand;
}

const CSSPropertyLonghand& borderLonghand()
{
    // Do not change the order of the following four shorthands, and keep them together.
    static const int borderProperties[4][3] = {
        { CSSPropertyBorderTopColor, CSSPropertyBorderTopStyle, CSSPropertyBorderTopWidth },
        { CSSPropertyBorderRightColor, CSSPropertyBorderRightStyle, CSSPropertyBorderRightWidth },
        { CSSPropertyBorderBottomColor, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomWidth },
        { CSSPropertyBorderLeftColor, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftWidth }
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderLonghand, (borderProperties[0], sizeof(borderProperties) / sizeof(borderProperties[0][0])));
    return borderLonghand;
}

const CSSPropertyLonghand& borderAbridgedLonghand()
{
    static const int borderAbridgedProperties[] = { CSSPropertyBorderWidth, CSSPropertyBorderStyle, CSSPropertyBorderColor };
    static const CSSPropertyLonghand* propertiesForInitialization[] = {
        &borderWidthLonghand(),
        &borderStyleLonghand(),
        &borderColorLonghand(),
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderAbridgedLonghand,
        (borderAbridgedProperties, propertiesForInitialization, WTF_ARRAY_LENGTH(borderAbridgedProperties)));
    return borderAbridgedLonghand;
}

const CSSPropertyLonghand& borderBottomLonghand()
{
    static const int borderBottomProperties[] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderBottomLonghand, (borderBottomProperties, WTF_ARRAY_LENGTH(borderBottomProperties)));
    return borderBottomLonghand;
}

const CSSPropertyLonghand& borderColorLonghand()
{
    static const int borderColorProperties[] = {
        CSSPropertyBorderTopColor,
        CSSPropertyBorderRightColor,
        CSSPropertyBorderBottomColor,
        CSSPropertyBorderLeftColor
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderColorLonghand, (borderColorProperties, WTF_ARRAY_LENGTH(borderColorProperties)));
    return borderColorLonghand;
}

const CSSPropertyLonghand& borderImageLonghand()
{
    static const int borderImageProperties[] = {
        CSSPropertyBorderImageSource,
        CSSPropertyBorderImageSlice,
        CSSPropertyBorderImageWidth,
        CSSPropertyBorderImageOutset,
        CSSPropertyBorderImageRepeat
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderImageLonghand, (borderImageProperties, WTF_ARRAY_LENGTH(borderImageProperties)));
    return borderImageLonghand;
}

const CSSPropertyLonghand& borderLeftLonghand()
{
    static const int borderLeftProperties[] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderLeftLonghand, (borderLeftProperties, WTF_ARRAY_LENGTH(borderLeftProperties)));
    return borderLeftLonghand;
}

const CSSPropertyLonghand& borderRadiusLonghand()
{
    static const int borderRadiusProperties[] = {
        CSSPropertyBorderTopRightRadius,
        CSSPropertyBorderTopLeftRadius,
        CSSPropertyBorderBottomLeftRadius,
        CSSPropertyBorderBottomRightRadius
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderRadiusLonghand, (borderRadiusProperties, WTF_ARRAY_LENGTH(borderRadiusProperties)));
    return borderRadiusLonghand;
}

const CSSPropertyLonghand& borderRightLonghand()
{
    static const int borderRightProperties[] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle, CSSPropertyBorderRightColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderRightLonghand, (borderRightProperties, WTF_ARRAY_LENGTH(borderRightProperties)));
    return borderRightLonghand;
}

const CSSPropertyLonghand& borderSpacingLonghand()
{
    static const int borderSpacingProperties[] = { CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderVerticalSpacing };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderSpacingLonghand, (borderSpacingProperties, WTF_ARRAY_LENGTH(borderSpacingProperties)));
    return borderSpacingLonghand;
}

const CSSPropertyLonghand& borderStyleLonghand()
{
    static const int borderStyleProperties[] = {
        CSSPropertyBorderTopStyle,
        CSSPropertyBorderRightStyle,
        CSSPropertyBorderBottomStyle,
        CSSPropertyBorderLeftStyle
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderStyleLonghand, (borderStyleProperties, WTF_ARRAY_LENGTH(borderStyleProperties)));
    return borderStyleLonghand;
}

const CSSPropertyLonghand& borderTopLonghand()
{
    static const int borderTopProperties[] = { CSSPropertyBorderTopWidth, CSSPropertyBorderTopStyle, CSSPropertyBorderTopColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderTopLonghand, (borderTopProperties, WTF_ARRAY_LENGTH(borderTopProperties)));
    return borderTopLonghand;
}

const CSSPropertyLonghand& borderWidthLonghand()
{
    static const int borderWidthProperties[] = {
        CSSPropertyBorderTopWidth,
        CSSPropertyBorderRightWidth,
        CSSPropertyBorderBottomWidth,
        CSSPropertyBorderLeftWidth
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, borderWidthLonghand, (borderWidthProperties, WTF_ARRAY_LENGTH(borderWidthProperties)));
    return borderWidthLonghand;
}

const CSSPropertyLonghand& listStyleLonghand()
{
    static const int listStyleProperties[] = {
        CSSPropertyListStyleType,
        CSSPropertyListStylePosition,
        CSSPropertyListStyleImage
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, listStyleLonghand, (listStyleProperties, WTF_ARRAY_LENGTH(listStyleProperties)));
    return listStyleLonghand;
}

const CSSPropertyLonghand& fontLonghand()
{
    static const int fontProperties[] = {
        CSSPropertyFontFamily,
        CSSPropertyFontSize,
        CSSPropertyFontStyle,
        CSSPropertyFontVariant,
        CSSPropertyFontWeight,
        CSSPropertyLineHeight
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, fontLonghand, (fontProperties, WTF_ARRAY_LENGTH(fontProperties)));
    return fontLonghand;
}

const CSSPropertyLonghand& marginLonghand()
{
    static const int marginProperties[] = {
        CSSPropertyMarginTop,
        CSSPropertyMarginRight,
        CSSPropertyMarginBottom,
        CSSPropertyMarginLeft
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, marginLonghand, (marginProperties, WTF_ARRAY_LENGTH(marginProperties)));
    return marginLonghand;
}

const CSSPropertyLonghand& outlineLonghand()
{
    static const int outlineProperties[] = {
        CSSPropertyOutlineWidth,
        CSSPropertyOutlineStyle,
        CSSPropertyOutlineColor,
        CSSPropertyOutlineOffset
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, outlineLonghand, (outlineProperties, WTF_ARRAY_LENGTH(outlineProperties)));
    return outlineLonghand;
}

const CSSPropertyLonghand& overflowLonghand()
{
    static const int overflowProperties[] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, overflowLonghand, (overflowProperties, WTF_ARRAY_LENGTH(overflowProperties)));
    return overflowLonghand;
}

const CSSPropertyLonghand& paddingLonghand()
{
    static const int paddingProperties[] = {
        CSSPropertyPaddingTop,
        CSSPropertyPaddingRight,
        CSSPropertyPaddingBottom,
        CSSPropertyPaddingLeft
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, paddingLonghand, (paddingProperties, WTF_ARRAY_LENGTH(paddingProperties)));
    return paddingLonghand;
}

const CSSPropertyLonghand& webkitAnimationLonghand()
{
    static const int animationProperties[] = {
        CSSPropertyWebkitAnimationName,
        CSSPropertyWebkitAnimationDuration,
        CSSPropertyWebkitAnimationTimingFunction,
        CSSPropertyWebkitAnimationDelay,
        CSSPropertyWebkitAnimationIterationCount,
        CSSPropertyWebkitAnimationDirection,
        CSSPropertyWebkitAnimationFillMode
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitAnimationLonghand, (animationProperties, WTF_ARRAY_LENGTH(animationProperties)));
    return webkitAnimationLonghand;
}

const CSSPropertyLonghand& webkitBorderAfterLonghand()
{
    static const int borderAfterProperties[] = { CSSPropertyWebkitBorderAfterWidth, CSSPropertyWebkitBorderAfterStyle, CSSPropertyWebkitBorderAfterColor  };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitBorderAfterLonghand, (borderAfterProperties, WTF_ARRAY_LENGTH(borderAfterProperties)));
    return webkitBorderAfterLonghand;
}

const CSSPropertyLonghand& webkitBorderBeforeLonghand()
{
    static const int borderBeforeProperties[] = { CSSPropertyWebkitBorderBeforeWidth, CSSPropertyWebkitBorderBeforeStyle, CSSPropertyWebkitBorderBeforeColor  };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitBorderBeforeLonghand, (borderBeforeProperties, WTF_ARRAY_LENGTH(borderBeforeProperties)));
    return webkitBorderBeforeLonghand;
}

const CSSPropertyLonghand& webkitBorderEndLonghand()
{
    static const int borderEndProperties[] = { CSSPropertyWebkitBorderEndWidth, CSSPropertyWebkitBorderEndStyle, CSSPropertyWebkitBorderEndColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitBorderEndLonghand, (borderEndProperties, WTF_ARRAY_LENGTH(borderEndProperties)));
    return webkitBorderEndLonghand;
}

const CSSPropertyLonghand& webkitBorderStartLonghand()
{
    static const int borderStartProperties[] = { CSSPropertyWebkitBorderStartWidth, CSSPropertyWebkitBorderStartStyle, CSSPropertyWebkitBorderStartColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitBorderStartLonghand, (borderStartProperties, WTF_ARRAY_LENGTH(borderStartProperties)));
    return webkitBorderStartLonghand;
}

const CSSPropertyLonghand& webkitColumnsLonghand()
{
    static const int columnsProperties[] = { CSSPropertyWebkitColumnWidth, CSSPropertyWebkitColumnCount };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitColumnsLonghand, (columnsProperties, WTF_ARRAY_LENGTH(columnsProperties)));
    return webkitColumnsLonghand;
}

const CSSPropertyLonghand& webkitColumnRuleLonghand()
{
    static const int columnRuleProperties[] = {
        CSSPropertyWebkitColumnRuleWidth,
        CSSPropertyWebkitColumnRuleStyle,
        CSSPropertyWebkitColumnRuleColor,
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitColumnRuleLonghand, (columnRuleProperties, WTF_ARRAY_LENGTH(columnRuleProperties)));
    return webkitColumnRuleLonghand;
}

const CSSPropertyLonghand& webkitFlexFlowLonghand()
{
    static const int flexFlowProperties[] = { CSSPropertyWebkitFlexDirection, CSSPropertyWebkitFlexWrap };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitFlexFlowLonghand, (flexFlowProperties, WTF_ARRAY_LENGTH(flexFlowProperties)));
    return webkitFlexFlowLonghand;
}

const CSSPropertyLonghand& webkitMarginCollapseLonghand()
{
    static const int marginCollapseProperties[] = { CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginAfterCollapse };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitMarginCollapseLonghand, (marginCollapseProperties, WTF_ARRAY_LENGTH(marginCollapseProperties)));
    return webkitMarginCollapseLonghand;
}

const CSSPropertyLonghand& webkitMarqueeLonghand()
{
    static const int marqueeProperties[] = {
        CSSPropertyWebkitMarqueeDirection,
        CSSPropertyWebkitMarqueeIncrement,
        CSSPropertyWebkitMarqueeRepetition,
        CSSPropertyWebkitMarqueeStyle,
        CSSPropertyWebkitMarqueeSpeed
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitMarqueeLonghand, (marqueeProperties, WTF_ARRAY_LENGTH(marqueeProperties)));
    return webkitMarqueeLonghand;
}

const CSSPropertyLonghand& webkitMaskLonghand()
{
    static const int maskProperties[] = {
        CSSPropertyWebkitMaskImage,
        CSSPropertyWebkitMaskRepeatX,
        CSSPropertyWebkitMaskRepeatY,
        CSSPropertyWebkitMaskAttachment,
        CSSPropertyWebkitMaskPositionX,
        CSSPropertyWebkitMaskPositionY,
        CSSPropertyWebkitMaskClip,
        CSSPropertyWebkitMaskOrigin
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitMaskLonghand, (maskProperties, WTF_ARRAY_LENGTH(maskProperties)));
    return webkitMaskLonghand;
}

const CSSPropertyLonghand& webkitMaskPositionLonghand()
{
    static const int maskPositionProperties[] = { CSSPropertyWebkitMaskPositionX, CSSPropertyWebkitMaskPositionY };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitMaskPositionLonghand, (maskPositionProperties, WTF_ARRAY_LENGTH(maskPositionProperties)));
    return webkitMaskPositionLonghand;
}

const CSSPropertyLonghand& webkitMaskRepeatLonghand()
{
    static const int maskRepeatProperties[] = { CSSPropertyWebkitMaskRepeatX, CSSPropertyWebkitMaskRepeatY };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitMaskRepeatLonghand, (maskRepeatProperties, WTF_ARRAY_LENGTH(maskRepeatProperties)));
    return webkitMaskRepeatLonghand;
}

const CSSPropertyLonghand& webkitTextEmphasisLonghand()
{
    static const int textEmphasisProperties[] = {
        CSSPropertyWebkitTextEmphasisStyle,
        CSSPropertyWebkitTextEmphasisColor
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitTextEmphasisLonghand, (textEmphasisProperties, WTF_ARRAY_LENGTH(textEmphasisProperties)));
    return webkitTextEmphasisLonghand;
}

const CSSPropertyLonghand& webkitTextStrokeLonghand()
{
    static const int textStrokeProperties[] = { CSSPropertyWebkitTextStrokeWidth, CSSPropertyWebkitTextStrokeColor };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitTextStrokeLonghand, (textStrokeProperties, WTF_ARRAY_LENGTH(textStrokeProperties)));
    return webkitTextStrokeLonghand;
}

const CSSPropertyLonghand& webkitTransitionLonghand()
{
    static const int transitionProperties[] = {
        CSSPropertyWebkitTransitionProperty,
        CSSPropertyWebkitTransitionDuration,
        CSSPropertyWebkitTransitionTimingFunction,
        CSSPropertyWebkitTransitionDelay
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitTransitionLonghand, (transitionProperties, WTF_ARRAY_LENGTH(transitionProperties)));
    return webkitTransitionLonghand;
}

const CSSPropertyLonghand& webkitTransformOriginLonghand()
{
    static const int transformOriginProperties[] = {
        CSSPropertyWebkitTransformOriginX,
        CSSPropertyWebkitTransformOriginY,
        CSSPropertyWebkitTransformOriginZ
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitTransformOriginLonghand, (transformOriginProperties, WTF_ARRAY_LENGTH(transformOriginProperties)));
    return webkitTransformOriginLonghand;
}

const CSSPropertyLonghand& webkitWrapLonghand()
{
    static const int webkitWrapProperties[] = {
        CSSPropertyWebkitWrapFlow,
        CSSPropertyWebkitWrapMargin,
        CSSPropertyWebkitWrapPadding
    };
    DEFINE_STATIC_LOCAL(CSSPropertyLonghand, webkitWrapLonghand, (webkitWrapProperties, WTF_ARRAY_LENGTH(webkitWrapProperties)));
    return webkitWrapLonghand;
}

// Returns an empty list if the property is not a shorthand
const CSSPropertyLonghand& longhandForProperty(int propertyID)
{
    switch (propertyID) {
    case CSSPropertyBackground:
        return backgroundLonghand();
    case CSSPropertyBackgroundPosition:
        return backgroundPositionLonghand();
    case CSSPropertyBackgroundRepeat:
        return backgroundRepeatLonghand();
    case CSSPropertyBorder:
        return borderLonghand();
    case CSSPropertyBorderBottom:
        return borderBottomLonghand();
    case CSSPropertyBorderColor:
        return borderColorLonghand();
    case CSSPropertyBorderImage:
        return borderImageLonghand();
    case CSSPropertyBorderLeft:
        return borderLeftLonghand();
    case CSSPropertyBorderRadius:
        return borderRadiusLonghand();
    case CSSPropertyBorderRight:
        return borderRightLonghand();
    case CSSPropertyBorderSpacing:
        return borderSpacingLonghand();
    case CSSPropertyBorderStyle:
        return borderStyleLonghand();
    case CSSPropertyBorderTop:
        return borderTopLonghand();
    case CSSPropertyBorderWidth:
        return borderWidthLonghand();
    case CSSPropertyListStyle:
        return listStyleLonghand();
    case CSSPropertyFont:
        return fontLonghand();
    case CSSPropertyMargin:
        return marginLonghand();
    case CSSPropertyOutline:
        return outlineLonghand();
    case CSSPropertyOverflow:
        return overflowLonghand();
    case CSSPropertyPadding:
        return paddingLonghand();
    case CSSPropertyWebkitAnimation:
        return webkitAnimationLonghand();
    case CSSPropertyWebkitBorderAfter:
        return webkitBorderAfterLonghand();
    case CSSPropertyWebkitBorderBefore:
        return webkitBorderBeforeLonghand();
    case CSSPropertyWebkitBorderEnd:
        return webkitBorderEndLonghand();
    case CSSPropertyWebkitBorderStart:
        return webkitBorderStartLonghand();
    case CSSPropertyWebkitBorderRadius:
        return borderRadiusLonghand();
    case CSSPropertyWebkitColumns:
        return webkitColumnsLonghand();
    case CSSPropertyWebkitColumnRule:
        return webkitColumnRuleLonghand();
    case CSSPropertyWebkitFlexFlow:
        return webkitFlexFlowLonghand();
    case CSSPropertyWebkitMarginCollapse:
        return webkitMarginCollapseLonghand();
    case CSSPropertyWebkitMarquee:
        return webkitMarqueeLonghand();
    case CSSPropertyWebkitMask:
        return webkitMaskLonghand();
    case CSSPropertyWebkitMaskPosition:
        return webkitMaskPositionLonghand();
    case CSSPropertyWebkitMaskRepeat:
        return webkitMaskRepeatLonghand();
    case CSSPropertyWebkitTextEmphasis:
        return webkitTextEmphasisLonghand();
    case CSSPropertyWebkitTextStroke:
        return webkitTextStrokeLonghand();
    case CSSPropertyWebkitTransition:
        return webkitTransitionLonghand();
    case CSSPropertyWebkitTransformOrigin:
        return webkitTransformOriginLonghand();
    case CSSPropertyWebkitWrap:
        return webkitWrapLonghand();
    default: {
        DEFINE_STATIC_LOCAL(CSSPropertyLonghand, defaultLonghand, ());
        return defaultLonghand;
    }
    }
}

} // namespace WebCore
