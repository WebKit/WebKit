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
#include "ScrollAnimator.h"

#include "ScrollbarClient.h"
#include <algorithm>

namespace WebCore {

#if !OS(WINDOWS)
ScrollAnimator* ScrollAnimator::create(ScrollbarClient* client)
{
    return new ScrollAnimator(client);
}
#endif

ScrollAnimator::ScrollAnimator(ScrollbarClient* client)
    : m_client(client)
    , m_currentPosX(0)
    , m_currentPosY(0)
{
}

ScrollAnimator::~ScrollAnimator()
{
}

bool ScrollAnimator::scroll(ScrollbarOrientation orientation, ScrollGranularity, float step, float multiplier)
{
    float* currentPos = (orientation == HorizontalScrollbar) ? &m_currentPosX : &m_currentPosY;
    float newPos = std::max(std::min(*currentPos + (step * multiplier), static_cast<float>(m_client->scrollSize(orientation))), 0.0f);
    if (*currentPos == newPos)
        return false;
    *currentPos = newPos;
    m_client->setScrollOffsetFromAnimation(IntPoint(m_currentPosX, m_currentPosY));
    return true;
}

void ScrollAnimator::setScrollPositionAndStopAnimation(ScrollbarOrientation orientation, float pos)
{
    if (orientation == HorizontalScrollbar)
        m_currentPosX = pos;
    else
        m_currentPosY = pos;
}

} // namespace WebCore
