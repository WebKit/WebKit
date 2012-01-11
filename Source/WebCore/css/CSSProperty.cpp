/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "CSSProperty.h"

#include "CSSPropertyNames.h"
#include "PlatformString.h"
#include "RenderStyleConstants.h"

namespace WebCore {

String CSSProperty::cssText() const
{
    return String(getPropertyName(static_cast<CSSPropertyID>(id()))) + ": " + m_value->cssText() + (isImportant() ? " !important" : "") + "; ";
}

enum LogicalBoxSide { BeforeSide, EndSide, AfterSide, StartSide };
enum PhysicalBoxSide { TopSide, RightSide, BottomSide, LeftSide };

static int resolveToPhysicalProperty(TextDirection direction, WritingMode writingMode, LogicalBoxSide logicalSide, const int* properties)
{
    if (direction == LTR) {
        if (writingMode == TopToBottomWritingMode) {
            // The common case. The logical and physical box sides match.
            // Left = Start, Right = End, Before = Top, After = Bottom
            return properties[logicalSide];
        }

        if (writingMode == BottomToTopWritingMode) {
            // Start = Left, End = Right, Before = Bottom, After = Top.
            switch (logicalSide) {
            case StartSide:
                return properties[LeftSide];
            case EndSide:
                return properties[RightSide];
            case BeforeSide:
                return properties[BottomSide];
            default:
                return properties[TopSide];
            }
        }

        if (writingMode == LeftToRightWritingMode) {
            // Start = Top, End = Bottom, Before = Left, After = Right.
            switch (logicalSide) {
            case StartSide:
                return properties[TopSide];
            case EndSide:
                return properties[BottomSide];
            case BeforeSide:
                return properties[LeftSide];
            default:
                return properties[RightSide];
            }
        }

        // Start = Top, End = Bottom, Before = Right, After = Left
        switch (logicalSide) {
        case StartSide:
            return properties[TopSide];
        case EndSide:
            return properties[BottomSide];
        case BeforeSide:
            return properties[RightSide];
        default:
            return properties[LeftSide];
        }
    }

    if (writingMode == TopToBottomWritingMode) {
        // Start = Right, End = Left, Before = Top, After = Bottom
        switch (logicalSide) {
        case StartSide:
            return properties[RightSide];
        case EndSide:
            return properties[LeftSide];
        case BeforeSide:
            return properties[TopSide];
        default:
            return properties[BottomSide];
        }
    }

    if (writingMode == BottomToTopWritingMode) {
        // Start = Right, End = Left, Before = Bottom, After = Top
        switch (logicalSide) {
        case StartSide:
            return properties[RightSide];
        case EndSide:
            return properties[LeftSide];
        case BeforeSide:
            return properties[BottomSide];
        default:
            return properties[TopSide];
        }
    }

    if (writingMode == LeftToRightWritingMode) {
        // Start = Bottom, End = Top, Before = Left, After = Right
        switch (logicalSide) {
        case StartSide:
            return properties[BottomSide];
        case EndSide:
            return properties[TopSide];
        case BeforeSide:
            return properties[LeftSide];
        default:
            return properties[RightSide];
        }
    }

    // Start = Bottom, End = Top, Before = Right, After = Left
    switch (logicalSide) {
    case StartSide:
        return properties[BottomSide];
    case EndSide:
        return properties[TopSide];
    case BeforeSide:
        return properties[RightSide];
    default:
        return properties[LeftSide];
    }
}

enum LogicalExtent { LogicalWidth, LogicalHeight };

static int resolveToPhysicalProperty(WritingMode writingMode, LogicalExtent logicalSide, const int* properties)
{
    if (writingMode == TopToBottomWritingMode || writingMode == BottomToTopWritingMode)
        return properties[logicalSide];
    return logicalSide == LogicalWidth ? properties[1] : properties[0];
}

int CSSProperty::resolveDirectionAwareProperty(int propertyID, TextDirection direction, WritingMode writingMode)
{
    switch (static_cast<CSSPropertyID>(propertyID)) {
    case CSSPropertyWebkitMarginEnd: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, writingMode, EndSide, properties);
    }
    case CSSPropertyWebkitMarginStart: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, writingMode, StartSide, properties);
    }
    case CSSPropertyWebkitMarginBefore: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, properties);
    }
    case CSSPropertyWebkitMarginAfter: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, properties);
    }
    case CSSPropertyWebkitPaddingEnd: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, writingMode, EndSide, properties);
    }
    case CSSPropertyWebkitPaddingStart: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, writingMode, StartSide, properties);
    }
    case CSSPropertyWebkitPaddingBefore: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, properties);
    }
    case CSSPropertyWebkitPaddingAfter: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEnd: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, writingMode, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStart: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, writingMode, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBefore: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfter: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEndColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, writingMode, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStartColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, writingMode, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBeforeColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfterColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEndStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, writingMode, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStartStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, writingMode, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBeforeStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfterStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEndWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, writingMode, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStartWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, writingMode, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBeforeWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfterWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, properties);
    }
    case CSSPropertyWebkitLogicalWidth: {
        const int properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitLogicalHeight: {
        const int properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMinLogicalWidth: {
        const int properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMinLogicalHeight: {
        const int properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMaxLogicalWidth: {
        const int properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMaxLogicalHeight: {
        const int properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    default:
        return propertyID;
    }
}

bool CSSProperty::isInheritedProperty(unsigned propertyID)
{
    switch (static_cast<CSSPropertyID>(propertyID)) {
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderSpacing:
    case CSSPropertyCaptionSide:
    case CSSPropertyColor:
    case CSSPropertyCursor:
    case CSSPropertyDirection:
    case CSSPropertyEmptyCells:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyImageRendering:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyle:
    case CSSPropertyListStyleImage:
    case CSSPropertyListStyleType:
    case CSSPropertyListStylePosition:
    case CSSPropertyOrphans:
    case CSSPropertyPointerEvents:
    case CSSPropertyQuotes:
    case CSSPropertyResize:
    case CSSPropertySpeak:
    case CSSPropertyTextAlign:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextIndent:
    case CSSPropertyTextRendering:
    case CSSPropertyTextShadow:
    case CSSPropertyTextTransform:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAspectRatio:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitColorCorrection:
    case CSSPropertyWebkitFontFeatureSettings:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitLocale:
    case CSSPropertyWebkitHighlight:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitHyphenateLimitAfter:
    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitLines:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineBoxContain:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineGrid:
    case CSSPropertyWebkitLineGridSnap:
    case CSSPropertyWebkitNbspMode:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextOrientation:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextSizeAdjust:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTextStrokeWidth:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWidows:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
    case CSSPropertyWordWrap:
#if ENABLE(SVG)
    case CSSPropertyClipRule:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyColorRendering:
    case CSSPropertyFill:
    case CSSPropertyFillOpacity:
    case CSSPropertyFillRule:
    case CSSPropertyGlyphOrientationHorizontal:
    case CSSPropertyGlyphOrientationVertical:
    case CSSPropertyKerning:
    case CSSPropertyMarker:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerStart:
    case CSSPropertyStroke:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyStrokeWidth:
    case CSSPropertyShapeRendering:
    case CSSPropertyTextAnchor:
    case CSSPropertyWritingMode:
#endif
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor:
#endif
        return true;
    case CSSPropertyDisplay:
    case CSSPropertyZoom:
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBorder:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRadius:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBorderWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxShadow:
    case CSSPropertyBoxSizing:
    case CSSPropertyClear:
    case CSSPropertyClip:
    case CSSPropertyContent:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyFloat:
    case CSSPropertyFontStretch:
    case CSSPropertyHeight:
    case CSSPropertyLeft:
    case CSSPropertyMargin:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyOverflow:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPadding:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPage:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPosition:
    case CSSPropertyRight:
    case CSSPropertySize:
    case CSSPropertySrc:
    case CSSPropertyTableLayout:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextOverline:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderline:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyTop:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyUnicodeRange:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWidth:
    case CSSPropertyZIndex:
    case CSSPropertyWebkitAnimation:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxFlex:
    case CSSPropertyWebkitBoxFlexGroup:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitBoxReflect:
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyWebkitColumnAxis:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnRule:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnSpan:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWebkitColumns:
#if ENABLE(CSS_FILTERS)
    case CSSPropertyWebkitFilter:
#endif
    case CSSPropertyWebkitFlexOrder:
    case CSSPropertyWebkitFlexPack:
    case CSSPropertyWebkitFlexItemAlign:
    case CSSPropertyWebkitFlexDirection:
    case CSSPropertyWebkitFlexFlow:
    case CSSPropertyWebkitFlexWrap:
    case CSSPropertyWebkitFontSizeDelta:
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridColumns:
    case CSSPropertyWebkitGridRows:
#endif
    case CSSPropertyWebkitLineClamp:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarquee:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeIncrement:
    case CSSPropertyWebkitMarqueeRepetition:
    case CSSPropertyWebkitMarqueeSpeed:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskAttachment:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageRepeat:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitTransform:
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitTransition:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitFlowInto:
    case CSSPropertyWebkitFlowFrom:
    case CSSPropertyWebkitRegionOverflow:
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
    case CSSPropertyWebkitRegionBreakInside:
    case CSSPropertyWebkitWrap:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitWrapMargin:
    case CSSPropertyWebkitWrapPadding:
    case CSSPropertyWebkitWrapShapeInside:
    case CSSPropertyWebkitWrapShapeOutside:
    case CSSPropertyWebkitWrapThrough:
#if ENABLE(SVG)
    case CSSPropertyClipPath:
    case CSSPropertyMask:
    case CSSPropertyEnableBackground:
    case CSSPropertyFilter:
    case CSSPropertyFloodColor:
    case CSSPropertyFloodOpacity:
    case CSSPropertyLightingColor:
    case CSSPropertyStopColor:
    case CSSPropertyStopOpacity:
    case CSSPropertyColorProfile:
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyBaselineShift:
    case CSSPropertyDominantBaseline:
    case CSSPropertyVectorEffect:
    case CSSPropertyWebkitSvgShadow:
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPropertyWebkitDashboardRegion:
#endif
        return false;
    case CSSPropertyInvalid:
        ASSERT_NOT_REACHED();
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
