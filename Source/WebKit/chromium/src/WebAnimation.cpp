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

#include <public/WebAnimation.h>

#include "AnimationIdVendor.h"
#include "cc/CCActiveAnimation.h"
#include "cc/CCAnimationCurve.h"
#include <public/WebAnimationCurve.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using WebCore::AnimationIdVendor;
using WebCore::CCActiveAnimation;

namespace WebKit {

int WebAnimation::iterations() const
{
    return m_private->iterations();
}

void WebAnimation::setIterations(int n)
{
    m_private->setIterations(n);
}

double WebAnimation::startTime() const
{
    return m_private->startTime();
}

void WebAnimation::setStartTime(double monotonicTime)
{
    m_private->setStartTime(monotonicTime);
}

double WebAnimation::timeOffset() const
{
    return m_private->timeOffset();
}

void WebAnimation::setTimeOffset(double monotonicTime)
{
    m_private->setTimeOffset(monotonicTime);
}

bool WebAnimation::alternatesDirection() const
{
    return m_private->alternatesDirection();
}

void WebAnimation::setAlternatesDirection(bool alternates)
{
    m_private->setAlternatesDirection(alternates);
}

WebAnimation::operator PassOwnPtr<WebCore::CCActiveAnimation>() const
{
    OwnPtr<WebCore::CCActiveAnimation> toReturn(m_private->cloneForImplThread());
    toReturn->setNeedsSynchronizedStartTime(true);
    return toReturn.release();
}

void WebAnimation::initialize(const WebAnimationCurve& curve, TargetProperty targetProperty)
{
    m_private.reset(CCActiveAnimation::create(curve,
                                              AnimationIdVendor::getNextAnimationId(),
                                              AnimationIdVendor::getNextGroupId(),
                                              static_cast<WebCore::CCActiveAnimation::TargetProperty>(targetProperty)).leakPtr());
}

void WebAnimation::destroy()
{
    m_private.reset(0);
}

} // namespace WebKit
