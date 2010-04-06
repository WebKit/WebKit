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
#if ENABLE(PROGRESS_TAG)

#include "RenderProgress.h"

#include "HTMLProgressElement.h"
#include "RenderTheme.h"
#include <wtf/CurrentTime.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderProgress::RenderProgress(HTMLProgressElement* element)
    : RenderBlock(element)
    , m_position(-1)
    , m_animationStartTime(0)
    , m_animationRepeatInterval(0)
    , m_animationDuration(0)
    , m_animating(false)
    , m_animationTimer(this, &RenderProgress::animationTimerFired)
{
}

void RenderProgress::layout()
{
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    calcWidth();
    calcHeight();

    m_overflow.clear();

    updateAnimationState();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderProgress::updateFromElement()
{
    HTMLProgressElement* element = static_cast<HTMLProgressElement*>(node());
    if (m_position == element->position())
        return;
    m_position = element->position();

    updateAnimationState();

    repaint();
}

double RenderProgress::animationProgress()
{
    return m_animating ? (fmod((currentTime() - m_animationStartTime), m_animationDuration) / m_animationDuration) : 0;
}

void RenderProgress::animationTimerFired(Timer<RenderProgress>*)
{
    repaint();
}

void RenderProgress::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.phase == PaintPhaseBlockBackground) {
        if (!m_animationTimer.isActive() && m_animating)
            m_animationTimer.startOneShot(m_animationRepeatInterval);
    }

    RenderBlock::paint(paintInfo, tx, ty);
}

void RenderProgress::updateAnimationState()
{
    m_animationDuration = theme()->animationDurationForProgressBar(this);
    m_animationRepeatInterval = theme()->animationRepeatIntervalForProgressBar(this);

    bool animating = m_animationDuration > 0;
    if (animating == m_animating)
        return;

    m_animating = animating;
    if (m_animating) {
        m_animationStartTime = currentTime();
        m_animationTimer.startOneShot(m_animationRepeatInterval);
    } else
        m_animationTimer.stop();
}

} // namespace WebCore
#endif
