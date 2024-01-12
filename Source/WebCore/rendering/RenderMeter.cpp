/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#include "config.h"
#include "RenderMeter.h"

#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderTheme.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMeter);

RenderMeter::RenderMeter(HTMLElement& element, RenderStyle&& style)
    : RenderBlockFlow(Type::Meter, element, WTFMove(style))
{
    ASSERT(isRenderMeter());
}

RenderMeter::~RenderMeter() = default;

HTMLMeterElement* RenderMeter::meterElement() const
{
    ASSERT(element());

    if (auto* meterElement = dynamicDowncast<HTMLMeterElement>(*element()))
        return meterElement;

    ASSERT(element()->shadowHost());
    return downcast<HTMLMeterElement>(element()->shadowHost());
}

void RenderMeter::updateLogicalWidth()
{
    RenderBox::updateLogicalWidth();

    auto frameSize = theme().meterSizeForBounds(*this, snappedIntRect(frameRect()));
    setLogicalWidth(LayoutUnit(isHorizontalWritingMode() ? frameSize.width() : frameSize.height()));
}

RenderBox::LogicalExtentComputedValues RenderMeter::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    auto computedValues = RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
    LayoutRect frame = frameRect();
    if (isHorizontalWritingMode())
        frame.setHeight(computedValues.m_extent);
    else
        frame.setWidth(computedValues.m_extent);
    auto frameSize = theme().meterSizeForBounds(*this, snappedIntRect(frame));
    computedValues.m_extent = isHorizontalWritingMode() ? frameSize.height() : frameSize.width();
    return computedValues;
}

void RenderMeter::updateFromElement()
{
    repaint();
}

} // namespace WebCore
