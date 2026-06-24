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
#include "ElementInlinesLight.h"
#include "Event.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "LocalFrame.h"
#include "MouseEvent.h"
#include "Node.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "SliderThumbElement.h"
#include "StepRange.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSlider);

const int RenderSlider::defaultTrackLength = 129;

RenderSlider::RenderSlider(HTMLInputElement& element, Style::ComputedStyle&& style)
    : RenderFlexibleBox(Type::Slider, element, WTF::move(style))
{
    // We assume RenderSlider works only with <input type=range>.
    ASSERT(element.isRangeControl());
    ASSERT(isRenderSlider());
}

RenderSlider::~RenderSlider() = default;

HTMLInputElement& NODELETE RenderSlider::element() const
{
    return downcast<HTMLInputElement>(nodeForNonAnonymous());
}

std::pair<LayoutUnit, LayoutUnit> RenderSlider::computeIntrinsicLogicalWidths() const
{
    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth())
            return { width.value(), width.value() };
        return { };
    }
    auto minLogicalWidth = LayoutUnit { };
    auto maxLogicalWidth = LayoutUnit { defaultTrackLength * style().usedZoom() };
    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isCalculated())
        minLogicalWidth = std::max(0_lu, Style::evaluate<LayoutUnit>(logicalWidth, 0_lu, style().usedZoomForLength()));
    else if (!logicalWidth.isPercent())
        minLogicalWidth = maxLogicalWidth;

    return { minLogicalWidth, maxLogicalWidth };
}

void RenderSlider::computeIntrinsicLogicalWidthContributions()
{
    m_minContentLogicalWidthContribution = 0_lu;
    m_maxContentLogicalWidthContribution = 0_lu;

    if (auto fixedLogicalWidth = style().logicalWidth().tryFixed()) {
        m_maxContentLogicalWidthContribution = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
        m_minContentLogicalWidthContribution = m_maxContentLogicalWidthContribution;
    } else
        std::tie(m_minContentLogicalWidthContribution, m_maxContentLogicalWidthContribution) = computeIntrinsicLogicalWidths();

    constrainIntrinsicLogicalWidthsByMinMax(m_minContentLogicalWidthContribution, m_maxContentLogicalWidthContribution);

    clearContentLogicalWidthsInvalidation();
}

bool RenderSlider::inDragMode() const
{
    return protect(protect(element())->sliderThumbElement())->active();
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
