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
    if (id() == CSSPropertyWebkitVariableDeclarationBlock)
        return m_value->cssText() + ";";
    return String(getPropertyName(static_cast<CSSPropertyID>(id()))) + ": " + m_value->cssText() + (isImportant() ? " !important" : "") + "; ";
}

bool operator==(const CSSProperty& a, const CSSProperty& b)
{
    return a.m_id == b.m_id && a.m_important == b.m_important && a.m_value == b.m_value;
}

enum LogicalBoxSide { BeforeSide, EndSide, AfterSide, StartSide };
enum PhysicalBoxSide { TopSide, RightSide, BottomSide, LeftSide };

static int resolveToPhysicalProperty(TextDirection direction, EBlockFlowDirection blockDirection, LogicalBoxSide logicalSide, const int* properties)
{
    if (direction == LTR) {
        if (blockDirection == TopToBottomBlockFlow) {
            // The common case. The logical and physical box sides match.
            // Left = Start, Right = End, Before = Top, After = Bottom
            return properties[logicalSide];
        }
        
        if (blockDirection == BottomToTopBlockFlow) {
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
        
        if (blockDirection == LeftToRightBlockFlow) {
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

    if (blockDirection == TopToBottomBlockFlow) {
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
    
    if (blockDirection == BottomToTopBlockFlow) {
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
    
    if (blockDirection == LeftToRightBlockFlow) {
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

static int resolveToPhysicalProperty(EBlockFlowDirection blockDirection, LogicalExtent logicalSide, const int* properties)
{
    if (blockDirection == TopToBottomBlockFlow || blockDirection == BottomToTopBlockFlow)
        return properties[logicalSide];
    return logicalSide == LogicalWidth ? properties[1] : properties[0];
}
        
int CSSProperty::resolveDirectionAwareProperty(int propertyID, TextDirection direction, EBlockFlowDirection blockDirection)
{
    switch (static_cast<CSSPropertyID>(propertyID)) {
    case CSSPropertyWebkitMarginEnd: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, blockDirection, EndSide, properties);
    }
    case CSSPropertyWebkitMarginStart: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, blockDirection, StartSide, properties);
    }
    case CSSPropertyWebkitMarginBefore: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, blockDirection, BeforeSide, properties);
    }
    case CSSPropertyWebkitMarginAfter: {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return resolveToPhysicalProperty(direction, blockDirection, AfterSide, properties);
    }
    case CSSPropertyWebkitPaddingEnd: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, blockDirection, EndSide, properties);
    }
    case CSSPropertyWebkitPaddingStart: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, blockDirection, StartSide, properties);
    }
    case CSSPropertyWebkitPaddingBefore: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, blockDirection, BeforeSide, properties);
    }
    case CSSPropertyWebkitPaddingAfter: {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return resolveToPhysicalProperty(direction, blockDirection, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEnd: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, blockDirection, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStart: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, blockDirection, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBefore: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, blockDirection, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfter: {
        const int properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        return resolveToPhysicalProperty(direction, blockDirection, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEndColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, blockDirection, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStartColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, blockDirection, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBeforeColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, blockDirection, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfterColor: {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return resolveToPhysicalProperty(direction, blockDirection, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEndStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, blockDirection, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStartStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, blockDirection, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBeforeStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, blockDirection, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfterStyle: {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return resolveToPhysicalProperty(direction, blockDirection, AfterSide, properties);
    }
    case CSSPropertyWebkitBorderEndWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, blockDirection, EndSide, properties);
    }
    case CSSPropertyWebkitBorderStartWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, blockDirection, StartSide, properties);
    }
    case CSSPropertyWebkitBorderBeforeWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, blockDirection, BeforeSide, properties);
    }
    case CSSPropertyWebkitBorderAfterWidth: {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return resolveToPhysicalProperty(direction, blockDirection, AfterSide, properties);
    }
    case CSSPropertyWebkitLogicalWidth: {
        const int properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(blockDirection, LogicalWidth, properties);
    }
    case CSSPropertyWebkitLogicalHeight: {
        const int properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(blockDirection, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMinLogicalWidth: {
        const int properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(blockDirection, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMinLogicalHeight: {
        const int properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(blockDirection, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMaxLogicalWidth: {
        const int properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(blockDirection, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMaxLogicalHeight: {
        const int properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(blockDirection, LogicalHeight, properties);
    }
    default:
        return propertyID;
    }
}

} // namespace WebCore
