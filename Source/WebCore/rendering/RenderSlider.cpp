/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "RenderSlider.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "LocalFrame.h"
#include "MouseEvent.h"
#include "Node.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "SliderThumbElement.h"
#include "StepRange.h"
#include "StyleResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSlider);

const int RenderSlider::defaultTrackLength = 129;

RenderSlider::RenderSlider(HTMLInputElement& element, RenderStyle&& style)
    : RenderFlexibleBox(Type::Slider, element, WTFMove(style))
{
    // We assume RenderSlider works only with <input type=range>.
    ASSERT(element.isRangeControl());
    ASSERT(isRenderSlider());
}

RenderSlider::~RenderSlider() = default;

HTMLInputElement& RenderSlider::element() const
{
    return downcast<HTMLInputElement>(nodeForNonAnonymous());
}

Ref<HTMLInputElement> RenderSlider::protectedElement() const
{
    return downcast<HTMLInputElement>(nodeForNonAnonymous());
}

void RenderSlider::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
        }
        return;
    }
    maxLogicalWidth = defaultTrackLength * style().usedZoom();
    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isCalculated())
        minLogicalWidth = std::max(0_lu, Style::evaluate(logicalWidth, 0_lu));
    else if (!logicalWidth.isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderSlider::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (auto fixedLogicalWidth = style().logicalWidth().tryFixed())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), writingMode().isHorizontal() ? horizontalBorderAndPaddingExtent() : verticalBorderAndPaddingExtent());

    clearNeedsPreferredWidthsUpdate();
}

bool RenderSlider::inDragMode() const
{
    return protectedElement()->protectedSliderThumbElement()->active();
}

double RenderSlider::valueRatio() const
{
    Ref element = this->element();

    auto min = element->minimum();
    auto max = element->maximum();
    auto value = element->valueAsNumber();

    if (max <= min)
        return 0;
    return (value - min) / (max - min);
}

} // namespace WebCore
