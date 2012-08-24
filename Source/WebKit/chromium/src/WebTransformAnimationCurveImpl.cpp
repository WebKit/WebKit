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

#include "WebTransformAnimationCurveImpl.h"

#include "CCKeyframedAnimationCurve.h"
#include "CCTimingFunction.h"
#include "WebAnimationCurveCommon.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

WebTransformAnimationCurve* WebTransformAnimationCurve::create()
{
    return new WebTransformAnimationCurveImpl(WebCore::CCKeyframedTransformAnimationCurve::create());
}

WebTransformAnimationCurveImpl::WebTransformAnimationCurveImpl(PassOwnPtr<WebCore::CCKeyframedTransformAnimationCurve> curve)
    : m_curve(curve)
{
}

WebTransformAnimationCurveImpl::~WebTransformAnimationCurveImpl()
{
}

WebAnimationCurve::AnimationCurveType WebTransformAnimationCurveImpl::type() const
{
    return WebAnimationCurve::AnimationCurveTypeTransform;
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe)
{
    add(keyframe, TimingFunctionTypeEase);
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe, TimingFunctionType type)
{
    m_curve->addKeyframe(WebCore::CCTransformKeyframe::create(keyframe.time, keyframe.value, createTimingFunction(type)));
}

void WebTransformAnimationCurveImpl::add(const WebTransformKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    m_curve->addKeyframe(WebCore::CCTransformKeyframe::create(keyframe.time, keyframe.value, WebCore::CCCubicBezierTimingFunction::create(x1, y1, x2, y2)));
}

WebTransformationMatrix WebTransformAnimationCurveImpl::getValue(double time) const
{
    return m_curve->getValue(time);
}

PassOwnPtr<WebCore::CCAnimationCurve> WebTransformAnimationCurveImpl::cloneToCCAnimationCurve() const
{
    return m_curve->clone();
}

} // namespace WebKit
