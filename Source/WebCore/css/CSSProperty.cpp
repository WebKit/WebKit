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
#include "RenderStyleConstants.h"
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
    return shorthand.properties()[mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide)];
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
    static NeverDestroyed<StylePropertyShorthand> borderDirections(CSSPropertyBorder, properties);
    return borderDirections;
}

CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID propertyID, TextDirection direction, WritingMode writingMode)
{
    switch (propertyID) {
    case CSSPropertyWebkitMarginEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, marginShorthand());
    case CSSPropertyWebkitMarginStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, marginShorthand());
    case CSSPropertyWebkitMarginBefore:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, marginShorthand());
    case CSSPropertyWebkitMarginAfter:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, marginShorthand());
    case CSSPropertyWebkitPaddingEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, paddingShorthand());
    case CSSPropertyWebkitPaddingStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, paddingShorthand());
    case CSSPropertyWebkitPaddingBefore:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, paddingShorthand());
    case CSSPropertyWebkitPaddingAfter:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, paddingShorthand());
    case CSSPropertyWebkitBorderEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderDirections());
    case CSSPropertyWebkitBorderStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderDirections());
    case CSSPropertyWebkitBorderBefore:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderDirections());
    case CSSPropertyWebkitBorderAfter:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderDirections());
    case CSSPropertyWebkitBorderEndColor:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderColorShorthand());
    case CSSPropertyWebkitBorderStartColor:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderColorShorthand());
    case CSSPropertyWebkitBorderBeforeColor:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderColorShorthand());
    case CSSPropertyWebkitBorderAfterColor:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderColorShorthand());
    case CSSPropertyWebkitBorderEndStyle:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderStartStyle:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderBeforeStyle:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderAfterStyle:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderEndWidth:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderWidthShorthand());
    case CSSPropertyWebkitBorderStartWidth:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderWidthShorthand());
    case CSSPropertyWebkitBorderBeforeWidth:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderWidthShorthand());
    case CSSPropertyWebkitBorderAfterWidth:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderWidthShorthand());
    case CSSPropertyWebkitLogicalWidth: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitLogicalHeight: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMinLogicalWidth: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMinLogicalHeight: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMaxLogicalWidth: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMaxLogicalHeight: {
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
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
#endif
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
        return true;
    default:
        return false;
    }
}

bool CSSProperty::isDirectionAwareProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
        return true;
    default:
        return false;
    }
}

} // namespace WebCore
