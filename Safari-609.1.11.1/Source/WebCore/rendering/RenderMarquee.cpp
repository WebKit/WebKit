/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"

#include "RenderMarquee.h"

#include "FrameView.h"
#include "HTMLMarqueeElement.h"
#include "HTMLNames.h"
#include "RenderLayer.h"
#include "RenderView.h"

namespace WebCore {

using namespace HTMLNames;

RenderMarquee::RenderMarquee(RenderLayer* layer)
    : m_layer(layer)
    , m_timer(*this, &RenderMarquee::timerFired)
{
    layer->setConstrainsScrollingToContentEdge(false);
}

RenderMarquee::~RenderMarquee() = default;

int RenderMarquee::marqueeSpeed() const
{
    int result = m_layer->renderer().style().marqueeSpeed();
    Element* element = m_layer->renderer().element();
    if (is<HTMLMarqueeElement>(element))
        result = std::max(result, downcast<HTMLMarqueeElement>(*element).minimumDelay());
    return result;
}

static MarqueeDirection reverseDirection(MarqueeDirection direction)
{
    switch (direction) {
    case MarqueeDirection::Auto:
        return MarqueeDirection::Auto;
    case MarqueeDirection::Left:
        return MarqueeDirection::Right;
    case MarqueeDirection::Right:
        return MarqueeDirection::Left;
    case MarqueeDirection::Up:
        return MarqueeDirection::Down;
    case MarqueeDirection::Down:
        return MarqueeDirection::Up;
    case MarqueeDirection::Backward:
        return MarqueeDirection::Forward;
    case MarqueeDirection::Forward:
        return MarqueeDirection::Backward;
    }
    return MarqueeDirection::Auto;
}

MarqueeDirection RenderMarquee::direction() const
{
    // FIXME: Support the CSS3 "auto" value for determining the direction of the marquee.
    // For now just map MarqueeDirection::Auto to MarqueeDirection::Backward
    MarqueeDirection result = m_layer->renderer().style().marqueeDirection();
    TextDirection dir = m_layer->renderer().style().direction();
    if (result == MarqueeDirection::Auto)
        result = MarqueeDirection::Backward;
    if (result == MarqueeDirection::Forward)
        result = (dir == TextDirection::LTR) ? MarqueeDirection::Right : MarqueeDirection::Left;
    if (result == MarqueeDirection::Backward)
        result = (dir == TextDirection::LTR) ? MarqueeDirection::Left : MarqueeDirection::Right;
    
    // Now we have the real direction.  Next we check to see if the increment is negative.
    // If so, then we reverse the direction.
    Length increment = m_layer->renderer().style().marqueeIncrement();
    if (increment.isNegative())
        result = reverseDirection(result);
    
    return result;
}

bool RenderMarquee::isHorizontal() const
{
    return direction() == MarqueeDirection::Left || direction() == MarqueeDirection::Right;
}

int RenderMarquee::computePosition(MarqueeDirection dir, bool stopAtContentEdge)
{
    RenderBox* box = m_layer->renderBox();
    ASSERT(box);
    auto& boxStyle = box->style();
    if (isHorizontal()) {
        bool ltr = boxStyle.isLeftToRightDirection();
        LayoutUnit clientWidth = box->clientWidth();
        LayoutUnit contentWidth = ltr ? box->maxPreferredLogicalWidth() : box->minPreferredLogicalWidth();
        if (ltr)
            contentWidth += (box->paddingRight() - box->borderLeft());
        else {
            contentWidth = box->width() - contentWidth;
            contentWidth += (box->paddingLeft() - box->borderRight());
        }
        if (dir == MarqueeDirection::Right) {
            if (stopAtContentEdge)
                return std::max<LayoutUnit>(0, ltr ? (contentWidth - clientWidth) : (clientWidth - contentWidth));

            return ltr ? contentWidth : clientWidth;
        }

        if (stopAtContentEdge)
            return std::min<LayoutUnit>(0, ltr ? (contentWidth - clientWidth) : (clientWidth - contentWidth));

        return ltr ? -clientWidth : -contentWidth;
    }

    // Vertical
    int contentHeight = box->layoutOverflowRect().maxY() - box->borderTop() + box->paddingBottom();
    int clientHeight = roundToInt(box->clientHeight());
    if (dir == MarqueeDirection::Up) {
        if (stopAtContentEdge)
            return std::min(contentHeight - clientHeight, 0);

        return -clientHeight;
    }

    if (stopAtContentEdge)
        return std::max(contentHeight - clientHeight, 0);

    return contentHeight;
}

void RenderMarquee::start()
{
    if (m_timer.isActive() || m_layer->renderer().style().marqueeIncrement().isZero())
        return;

    if (!m_suspended && !m_stopped) {
        if (isHorizontal())
            m_layer->scrollToOffset(ScrollOffset(m_start, 0), ScrollType::Programmatic, ScrollClamping::Unclamped);
        else
            m_layer->scrollToOffset(ScrollOffset(0, m_start), ScrollType::Programmatic, ScrollClamping::Unclamped);
    } else {
        m_suspended = false;
        m_stopped = false;
    }

    m_timer.startRepeating(1_ms * speed());
}

void RenderMarquee::suspend()
{
    m_timer.stop();
    m_suspended = true;
}

void RenderMarquee::stop()
{
    m_timer.stop();
    m_stopped = true;
}

void RenderMarquee::updateMarqueePosition()
{
    bool activate = (m_totalLoops <= 0 || m_currentLoop < m_totalLoops);
    if (activate) {
        MarqueeBehavior behavior = m_layer->renderer().style().marqueeBehavior();
        m_start = computePosition(direction(), behavior == MarqueeBehavior::Alternate);
        m_end = computePosition(reverseDirection(direction()), behavior == MarqueeBehavior::Alternate || behavior == MarqueeBehavior::Slide);
        if (!m_stopped)
            start();
    }
}

void RenderMarquee::updateMarqueeStyle()
{
    auto& style = m_layer->renderer().style();
    
    if (m_direction != style.marqueeDirection() || (m_totalLoops != style.marqueeLoopCount() && m_currentLoop >= m_totalLoops))
        m_currentLoop = 0; // When direction changes or our loopCount is a smaller number than our current loop, reset our loop.
    
    m_totalLoops = style.marqueeLoopCount();
    m_direction = style.marqueeDirection();
    
    if (m_layer->renderer().isHTMLMarquee()) {
        // Hack for WinIE.  In WinIE, a value of 0 or lower for the loop count for SLIDE means to only do
        // one loop.
        if (m_totalLoops <= 0 && style.marqueeBehavior() == MarqueeBehavior::Slide)
            m_totalLoops = 1;
    }
    
    if (speed() != marqueeSpeed()) {
        m_speed = marqueeSpeed();
        if (m_timer.isActive())
            m_timer.startRepeating(1_ms * speed());
    }
    
    // Check the loop count to see if we should now stop.
    bool activate = (m_totalLoops <= 0 || m_currentLoop < m_totalLoops);
    if (activate && !m_timer.isActive())
        m_layer->renderer().setNeedsLayout();
    else if (!activate && m_timer.isActive())
        m_timer.stop();
}

void RenderMarquee::timerFired()
{
    if (m_layer->renderer().view().needsLayout())
        return;
    
    if (m_reset) {
        m_reset = false;
        if (isHorizontal())
            m_layer->scrollToXOffset(m_start);
        else
            m_layer->scrollToYOffset(m_start);
        return;
    }
    
    const RenderStyle& style = m_layer->renderer().style();
    
    int endPoint = m_end;
    int range = m_end - m_start;
    int newPos;
    if (range == 0)
        newPos = m_end;
    else {  
        bool addIncrement = direction() == MarqueeDirection::Up || direction() == MarqueeDirection::Left;
        bool isReversed = style.marqueeBehavior() == MarqueeBehavior::Alternate && m_currentLoop % 2;
        if (isReversed) {
            // We're going in the reverse direction.
            endPoint = m_start;
            range = -range;
            addIncrement = !addIncrement;
        }
        bool positive = range > 0;
        int clientSize = (isHorizontal() ? roundToInt(m_layer->renderBox()->clientWidth()) : roundToInt(m_layer->renderBox()->clientHeight()));
        int increment = abs(intValueForLength(m_layer->renderer().style().marqueeIncrement(), clientSize));
        int currentPos = (isHorizontal() ? m_layer->scrollOffset().x() : m_layer->scrollOffset().y());
        newPos =  currentPos + (addIncrement ? increment : -increment);
        if (positive)
            newPos = std::min(newPos, endPoint);
        else
            newPos = std::max(newPos, endPoint);
    }

    if (newPos == endPoint) {
        m_currentLoop++;
        if (m_totalLoops > 0 && m_currentLoop >= m_totalLoops)
            m_timer.stop();
        else if (style.marqueeBehavior() != MarqueeBehavior::Alternate)
            m_reset = true;
    }
    
    if (isHorizontal())
        m_layer->scrollToXOffset(newPos);
    else
        m_layer->scrollToYOffset(newPos);
}

} // namespace WebCore
