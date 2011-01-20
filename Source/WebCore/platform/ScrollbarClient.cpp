/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollbarClient.h"

#include "FloatPoint.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimator.h"

namespace WebCore {

ScrollbarClient::ScrollbarClient()
    : m_scrollAnimator(ScrollAnimator::create(this))
{
}

ScrollbarClient::~ScrollbarClient()
{
}

bool ScrollbarClient::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    ScrollbarOrientation orientation;
    Scrollbar* scrollbar;
    if (direction == ScrollUp || direction == ScrollDown) {
        orientation = VerticalScrollbar;
        scrollbar = verticalScrollbar();
    } else {
        orientation = HorizontalScrollbar;
        scrollbar = horizontalScrollbar();
    }

    if (!scrollbar)
        return false;

    float step = 0;
    switch (granularity) {
    case ScrollByLine:
        step = scrollbar->lineStep();
        break;
    case ScrollByPage:
        step = scrollbar->pageStep();
        break;
    case ScrollByDocument:
        step = scrollbar->totalSize();
        break;
    case ScrollByPixel:
        step = scrollbar->pixelStep();
        break;
    }

    if (direction == ScrollUp || direction == ScrollLeft)
        multiplier = -multiplier;

    return m_scrollAnimator->scroll(orientation, granularity, step, multiplier);
}

void ScrollbarClient::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    m_scrollAnimator->scrollToOffsetWithoutAnimation(offset);
}

void ScrollbarClient::scrollToOffsetWithoutAnimation(ScrollbarOrientation orientation, float offset)
{
    if (orientation == HorizontalScrollbar)
        scrollToXOffsetWithoutAnimation(offset);
    else
        scrollToYOffsetWithoutAnimation(offset);
}

void ScrollbarClient::scrollToXOffsetWithoutAnimation(float x)
{
    scrollToOffsetWithoutAnimation(FloatPoint(x, m_scrollAnimator->currentPosition().y()));
}

void ScrollbarClient::scrollToYOffsetWithoutAnimation(float y)
{
    scrollToOffsetWithoutAnimation(FloatPoint(m_scrollAnimator->currentPosition().x(), y));
}

void ScrollbarClient::setScrollOffsetFromAnimation(const IntPoint& offset)
{
    // Tell the derived class to scroll its contents.
    setScrollOffset(offset);

    // Tell the scrollbars to update their thumb postions.
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar())
        horizontalScrollbar->offsetDidChange();
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar())
        verticalScrollbar->offsetDidChange();
}

} // namespace WebCore
