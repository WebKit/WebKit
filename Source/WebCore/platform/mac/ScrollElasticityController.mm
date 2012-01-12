/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollElasticityController.h"

#if ENABLE(RUBBER_BANDING)

namespace WebCore {

static const float rubberbandStiffness = 20;
static const float rubberbandAmplitude = 0.31f;
static const float rubberbandPeriod = 1.6f;

static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, float elapsedTime)
{
    float amplitude = rubberbandAmplitude;
    float period = rubberbandPeriod;
    float criticalDampeningFactor = expf((-elapsedTime * rubberbandStiffness) / period);

    return (initialPosition + (-initialVelocity * elapsedTime * amplitude)) * criticalDampeningFactor;
}

static float reboundDeltaForElasticDelta(float delta)
{
    return delta * rubberbandStiffness;
}

ScrollElasticityController::ScrollElasticityController(ScrollElasticityControllerClient* client)
    : m_client(client)
    , m_inScrollGesture(false)
    , m_momentumScrollInProgress(false)
    , m_ignoreMomentumScrolls(false)
    , m_lastMomentumScrollTimestamp(0)
    , m_startTime(0)
    , m_snapRubberbandTimerIsActive(false)
{
}

void ScrollElasticityController::beginScrollGesture()
{
    m_inScrollGesture = true;
    m_momentumScrollInProgress = false;
    m_ignoreMomentumScrolls = false;
    m_lastMomentumScrollTimestamp = 0;
    m_momentumVelocity = FloatSize();
    
    IntSize stretchAmount = m_client->stretchAmount();
    m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(stretchAmount.width()));
    m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(stretchAmount.height()));
    
    m_overflowScrollDelta = FloatSize();

    stopSnapRubberbandTimer();
}

static inline float roundTowardZero(float num)
{
    return num > 0 ? ceilf(num - 0.5f) : floorf(num + 0.5f);
}

static inline float roundToDevicePixelTowardZero(float num)
{
    float roundedNum = roundf(num);
    if (fabs(num - roundedNum) < 0.125)
        num = roundedNum;

    return roundTowardZero(num);
}

void ScrollElasticityController::snapRubberBandTimerFired()
{
    if (!m_momentumScrollInProgress || m_ignoreMomentumScrolls) {
        CFTimeInterval timeDelta = [NSDate timeIntervalSinceReferenceDate] - m_startTime;

        if (m_startStretch == FloatSize()) {
            m_startStretch = m_client->stretchAmount();
            if (m_startStretch == FloatSize()) {
                m_client->stopSnapRubberbandTimer();

                m_stretchScrollForce = FloatSize();
                m_startTime = 0;
                m_startStretch = FloatSize();
                m_origOrigin = FloatPoint();
                m_origVelocity = FloatSize();
                m_snapRubberbandTimerIsActive = false;

                return;
            }

            m_origOrigin = m_client->absoluteScrollPosition() - m_startStretch;
            m_origVelocity = m_momentumVelocity;

            // Just like normal scrolling, prefer vertical rubberbanding
            if (fabsf(m_origVelocity.height()) >= fabsf(m_origVelocity.width()))
                m_origVelocity.setWidth(0);

            // Don't rubber-band horizontally if it's not possible to scroll horizontally
            if (!m_client->canScrollHorizontally())
                m_origVelocity.setWidth(0);

            // Don't rubber-band vertically if it's not possible to scroll vertically
            if (!m_client->canScrollVertically())
                m_origVelocity.setHeight(0);
        }

        FloatPoint delta(roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.width(), -m_origVelocity.width(), (float)timeDelta)),
                         roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.height(), -m_origVelocity.height(), (float)timeDelta)));

        if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
            FloatPoint newOrigin = m_origOrigin + delta;

            m_client->immediateScrollByWithoutContentEdgeConstraints(FloatSize(delta.x(), delta.y()) - m_client->stretchAmount());

            FloatSize newStretch = m_client->stretchAmount();

            m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            m_client->immediateScrollBy(m_origOrigin - m_client->absoluteScrollPosition());
            m_client->stopSnapRubberbandTimer();

            m_stretchScrollForce = FloatSize();
            m_startTime = 0;
            m_startStretch = FloatSize();
            m_origOrigin = FloatPoint();
            m_origVelocity = FloatSize();
            m_snapRubberbandTimerIsActive = false;
        }
    } else {
        m_startTime = [NSDate timeIntervalSinceReferenceDate];
        m_startStretch = FloatSize();
    }
}

void ScrollElasticityController::stopSnapRubberbandTimer()
{
    m_client->stopSnapRubberbandTimer();
    m_snapRubberbandTimerIsActive = false;
}

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)
