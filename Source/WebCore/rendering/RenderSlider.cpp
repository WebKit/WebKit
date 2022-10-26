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
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MouseEvent.h"
#include "Node.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "SliderThumbElement.h"
#include "StepRange.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSlider);

const int RenderSlider::defaultTrackLength = 129;

RenderSlider::RenderSlider(HTMLInputElement& element, RenderStyle&& style)
    : RenderFlexibleBox(element, WTFMove(style))
{
    // We assume RenderSlider works only with <input type=range>.
    ASSERT(element.isRangeControl());
}

RenderSlider::~RenderSlider() = default;

HTMLInputElement& RenderSlider::element() const
{
    return downcast<HTMLInputElement>(nodeForNonAnonymous());
}

LayoutUnit RenderSlider::baselinePosition(FontBaseline, bool /*firstLine*/, LineDirectionMode, LinePositionMode) const
{
    // FIXME: Patch this function for writing-mode.
    return height() + marginTop();
}

void RenderSlider::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (shouldApplySizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
        }
        return;
    }
    maxLogicalWidth = defaultTrackLength * style().effectiveZoom();
    if (!style().width().isPercentOrCalculated())
        minLogicalWidth = maxLogicalWidth;
}

void RenderSlider::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style().width().isFixed() && style().width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().width());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().minWidth(), style().maxWidth(), horizontalBorderAndPaddingExtent());

    setPreferredLogicalWidthsDirty(false); 
}

bool RenderSlider::inDragMode() const
{
    return element().sliderThumbElement()->active();
}

double RenderSlider::valueRatio() const
{
    auto& element = this->element();

    auto min = element.minimum();
    auto max = element.maximum();
    auto value = element.valueAsNumber();

    if (max <= min)
        return 0;
    return (value - min) / (max - min);
}

} // namespace WebCore
