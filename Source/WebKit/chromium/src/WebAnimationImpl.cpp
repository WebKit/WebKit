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

#include "WebAnimationImpl.h"

#include "AnimationIdVendor.h"
#include "CCActiveAnimation.h"
#include "CCAnimationCurve.h"
#include <public/WebAnimation.h>
#include <public/WebAnimationCurve.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using WebCore::AnimationIdVendor;
using WebCore::CCActiveAnimation;

namespace WebKit {

WebAnimation* WebAnimation::create(const WebAnimationCurve& curve, TargetProperty targetProperty)
{
    return WebAnimation::create(curve, AnimationIdVendor::getNextAnimationId(), AnimationIdVendor::getNextGroupId(), targetProperty);
}

WebAnimation* WebAnimation::create(const WebAnimationCurve& curve, int animationId, int groupId, TargetProperty targetProperty)
{
    return new WebAnimationImpl(CCActiveAnimation::create(curve, animationId, groupId, static_cast<WebCore::CCActiveAnimation::TargetProperty>(targetProperty)));
}

WebAnimation::TargetProperty WebAnimationImpl::targetProperty() const
{
    return static_cast<WebAnimationImpl::TargetProperty>(m_animation->targetProperty());
}

int WebAnimationImpl::iterations() const
{
    return m_animation->iterations();
}

void WebAnimationImpl::setIterations(int n)
{
    m_animation->setIterations(n);
}

double WebAnimationImpl::startTime() const
{
    return m_animation->startTime();
}

void WebAnimationImpl::setStartTime(double monotonicTime)
{
    m_animation->setStartTime(monotonicTime);
}

double WebAnimationImpl::timeOffset() const
{
    return m_animation->timeOffset();
}

void WebAnimationImpl::setTimeOffset(double monotonicTime)
{
    m_animation->setTimeOffset(monotonicTime);
}

bool WebAnimationImpl::alternatesDirection() const
{
    return m_animation->alternatesDirection();
}

void WebAnimationImpl::setAlternatesDirection(bool alternates)
{
    m_animation->setAlternatesDirection(alternates);
}

PassOwnPtr<WebCore::CCActiveAnimation> WebAnimationImpl::cloneToCCAnimation()
{
    OwnPtr<WebCore::CCActiveAnimation> toReturn(m_animation->cloneForImplThread());
    toReturn->setNeedsSynchronizedStartTime(true);
    return toReturn.release();
}

} // namespace WebKit
