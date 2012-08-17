/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCScrollbarAnimationControllerLinearFade.h"

#include "CCScrollbarLayerImpl.h"

namespace WebCore {

PassOwnPtr<CCScrollbarAnimationControllerLinearFade> CCScrollbarAnimationControllerLinearFade::create(CCLayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength)
{
    return adoptPtr(new CCScrollbarAnimationControllerLinearFade(scrollLayer, fadeoutDelay, fadeoutLength));
}

CCScrollbarAnimationControllerLinearFade::CCScrollbarAnimationControllerLinearFade(CCLayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength)
    : CCScrollbarAnimationController(scrollLayer)
    , m_lastAwakenTime(-100000000) // arbitrary invalid timestamp
    , m_pinchGestureInEffect(false)
    , m_fadeoutDelay(fadeoutDelay)
    , m_fadeoutLength(fadeoutLength)
{
}

CCScrollbarAnimationControllerLinearFade::~CCScrollbarAnimationControllerLinearFade()
{
}

bool CCScrollbarAnimationControllerLinearFade::animate(double monotonicTime)
{
    float opacity = opacityAtTime(monotonicTime);
    if (horizontalScrollbarLayer())
        horizontalScrollbarLayer()->setOpacity(opacity);
    if (verticalScrollbarLayer())
        verticalScrollbarLayer()->setOpacity(opacity);
    return opacity;
}

void CCScrollbarAnimationControllerLinearFade::didPinchGestureUpdateAtTime(double)
{
    m_pinchGestureInEffect = true;
}

void CCScrollbarAnimationControllerLinearFade::didPinchGestureEndAtTime(double monotonicTime)
{
    m_pinchGestureInEffect = false;
    m_lastAwakenTime = monotonicTime;
}

void CCScrollbarAnimationControllerLinearFade::updateScrollOffsetAtTime(CCLayerImpl* scrollLayer, double monotonicTime)
{
    FloatPoint previousPos = currentPos();
    CCScrollbarAnimationController::updateScrollOffsetAtTime(scrollLayer, monotonicTime);

    if (previousPos == currentPos())
        return;

    m_lastAwakenTime = monotonicTime;
}

float CCScrollbarAnimationControllerLinearFade::opacityAtTime(double monotonicTime)
{
    if (m_pinchGestureInEffect)
        return 1;

    double delta = monotonicTime - m_lastAwakenTime;

    if (delta <= m_fadeoutDelay)
        return 1;
    if (delta < m_fadeoutDelay + m_fadeoutLength)
        return (m_fadeoutDelay + m_fadeoutLength - delta) / m_fadeoutLength;
    return 0;
}

} // namespace WebCore

