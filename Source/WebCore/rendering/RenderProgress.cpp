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
#include "RenderProgress.h"

#include "HTMLProgressElement.h"
#include "RenderTheme.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderProgress);

RenderProgress::RenderProgress(HTMLElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
    , m_position(HTMLProgressElement::InvalidPosition)
    , m_animationTimer(*this, &RenderProgress::animationTimerFired)
{
}

RenderProgress::~RenderProgress() = default;

void RenderProgress::updateFromElement()
{
    HTMLProgressElement* element = progressElement();
    if (m_position == element->position())
        return;
    m_position = element->position();

    updateAnimationState();
    repaint();
    RenderBlockFlow::updateFromElement();
}

RenderBox::LogicalExtentComputedValues RenderProgress::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    auto computedValues = RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
    LayoutRect frame = frameRect();
    if (isHorizontalWritingMode())
        frame.setHeight(computedValues.m_extent);
    else
        frame.setWidth(computedValues.m_extent);
    IntSize frameSize = theme().progressBarRectForBounds(*this, snappedIntRect(frame)).size();
    computedValues.m_extent = isHorizontalWritingMode() ? frameSize.height() : frameSize.width();
    return computedValues;
}

double RenderProgress::animationProgress() const
{
    ASSERT(m_animationDuration > 0_s);
    return m_animating ? (fmod((MonotonicTime::now() - m_animationStartTime).seconds(), m_animationDuration.seconds()) / m_animationDuration.seconds()) : 0;
}

bool RenderProgress::isDeterminate() const
{
    return (HTMLProgressElement::IndeterminatePosition != position()
            && HTMLProgressElement::InvalidPosition != position());
}

void RenderProgress::animationTimerFired()
{
    repaint();
    if (!m_animationTimer.isActive() && m_animating)
        m_animationTimer.startOneShot(m_animationRepeatInterval);
}

void RenderProgress::updateAnimationState()
{
    m_animationDuration = theme().animationDurationForProgressBar(*this);
    m_animationRepeatInterval = theme().animationRepeatIntervalForProgressBar(*this);

    bool animating = style().hasAppearance() && m_animationRepeatInterval > 0_s;
    if (animating == m_animating)
        return;

    m_animating = animating;
    if (m_animating) {
        m_animationStartTime = MonotonicTime::now();
        m_animationTimer.startOneShot(m_animationRepeatInterval);
    } else
        m_animationTimer.stop();
}

HTMLProgressElement* RenderProgress::progressElement() const
{
    if (!element())
        return nullptr;

    if (is<HTMLProgressElement>(*element()))
        return downcast<HTMLProgressElement>(element());

    ASSERT(element()->shadowHost());
    return downcast<HTMLProgressElement>(element()->shadowHost());
}    

} // namespace WebCore

