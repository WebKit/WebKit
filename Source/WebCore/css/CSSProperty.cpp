/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Inc.
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

#include "CSSValueList.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

struct SameSizeAsCSSProperty {
    uint32_t bitfields;
    void* value;
};

COMPILE_ASSERT(sizeof(CSSProperty) == sizeof(SameSizeAsCSSProperty), CSSProperty_should_stay_small);

CSSPropertyID StylePropertyMetadata::shorthandID() const
{
    if (!m_isSetFromShorthand)
        return CSSPropertyInvalid;

    auto shorthands = matchingShorthandsForLonghand(static_cast<CSSPropertyID>(m_propertyID));
    ASSERT(shorthands.size() && m_indexInShorthandsVector >= 0 && m_indexInShorthandsVector < shorthands.size());
    return shorthands[m_indexInShorthandsVector].id();
}

void CSSProperty::wrapValueInCommaSeparatedList()
{
    auto list = CSSValueList::createCommaSeparated();
    list.get().append(m_value.releaseNonNull());
    m_value = WTFMove(list);
}

static CSSPropertyID resolveToPhysicalProperty(TextDirection direction, WritingMode writingMode, LogicalBoxSide logicalSide, const StylePropertyShorthand& shorthand)
{
    return shorthand.properties()[static_cast<size_t>(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide))];
}

enum LogicalExtent { LogicalWidth, LogicalHeight };

static CSSPropertyID resolveToPhysicalProperty(WritingMode writingMode, LogicalExtent logicalSide, const CSSPropertyID* properties)
{
    if (writingMode == TopToBottomWritingMode || writingMode == BottomToTopWritingMode)
        return properties[logicalSide];
    return logicalSide == LogicalWidth ? properties[1] : properties[0];
}

static const StylePropertyShorthand& borderDirections()
{
    static const CSSPropertyID properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
    static const StylePropertyShorthand borderDirections(CSSPropertyBorder, properties);
    return borderDirections;
}

CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID propertyID, TextDirection direction, WritingMode writingMode)
{
    switch (propertyID) {
    case CSSPropertyInsetInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, insetShorthand());
    case CSSPropertyInsetInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, insetShorthand());
    case CSSPropertyInsetBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, insetShorthand());
    case CSSPropertyInsetBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, insetShorthand());
    case CSSPropertyMarginInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, marginShorthand());
    case CSSPropertyMarginInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, marginShorthand());
    case CSSPropertyMarginBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, marginShorthand());
    case CSSPropertyMarginBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, marginShorthand());
    case CSSPropertyPaddingInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, paddingShorthand());
    case CSSPropertyPaddingInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, paddingShorthand());
    case CSSPropertyPaddingBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, paddingShorthand());
    case CSSPropertyPaddingBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, paddingShorthand());
    case CSSPropertyBorderInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, borderDirections());
    case CSSPropertyBorderInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, borderDirections());
    case CSSPropertyBorderBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, borderDirections());
    case CSSPropertyBorderBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, borderDirections());
    case CSSPropertyBorderInlineEndColor:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, borderColorShorthand());
    case CSSPropertyBorderInlineStartColor:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, borderColorShorthand());
    case CSSPropertyBorderBlockStartColor:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, borderColorShorthand());
    case CSSPropertyBorderBlockEndColor:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, borderColorShorthand());
    case CSSPropertyBorderInlineEndStyle:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, borderStyleShorthand());
    case CSSPropertyBorderInlineStartStyle:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, borderStyleShorthand());
    case CSSPropertyBorderBlockStartStyle:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, borderStyleShorthand());
    case CSSPropertyBorderBlockEndStyle:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, borderStyleShorthand());
    case CSSPropertyBorderInlineEndWidth:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::End, borderWidthShorthand());
    case CSSPropertyBorderInlineStartWidth:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Start, borderWidthShorthand());
    case CSSPropertyBorderBlockStartWidth:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::Before, borderWidthShorthand());
    case CSSPropertyBorderBlockEndWidth:
        return resolveToPhysicalProperty(direction, writingMode, LogicalBoxSide::After, borderWidthShorthand());
    case CSSPropertyInlineSize: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyBlockSize: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyMinInlineSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyMinBlockSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyMaxInlineSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyMaxBlockSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    default:
        return propertyID;
    }
}

bool CSSProperty::isDescriptorOnly(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
    case CSSPropertyFontDisplay:
        return true;
    default:
        return false;
    }
}

bool CSSProperty::isDirectionAwareProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyBorderBlockEnd:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyBorderBlockStart:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderInlineEnd:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStart:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyPaddingInlineEnd:
    case CSSPropertyPaddingInlineStart:
    case CSSPropertyPaddingBlockStart:
    case CSSPropertyPaddingBlockEnd:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxBlockSize:
        return true;
    default:
        return false;
    }
}

bool CSSProperty::isColorProperty(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyFill:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyStopColor:
    case CSSPropertyStroke:
    case CSSPropertyStrokeColor:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyCaretColor:
        return true;
    default:
        return false;
    }
}

} // namespace WebCore
