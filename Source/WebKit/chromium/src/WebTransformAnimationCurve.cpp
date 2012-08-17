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

#include <public/WebTransformAnimationCurve.h>

#include "CCKeyframedAnimationCurve.h"
#include "CCTimingFunction.h"
#include "WebAnimationCurveCommon.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

void WebTransformAnimationCurve::add(const WebTransformKeyframe& keyframe)
{
    add(keyframe, TimingFunctionTypeEase);
}

void WebTransformAnimationCurve::add(const WebTransformKeyframe& keyframe, TimingFunctionType type)
{
    m_private->addKeyframe(WebCore::CCTransformKeyframe::create(keyframe.time, keyframe.value, createTimingFunction(type)));
}

void WebTransformAnimationCurve::add(const WebTransformKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    m_private->addKeyframe(WebCore::CCTransformKeyframe::create(keyframe.time, keyframe.value, WebCore::CCCubicBezierTimingFunction::create(x1, y1, x2, y2)));
}

WebTransformationMatrix WebTransformAnimationCurve::getValue(double time) const
{
    return m_private->getValue(time);
}

WebTransformAnimationCurve::operator PassOwnPtr<WebCore::CCAnimationCurve>() const
{
    return m_private->clone();
}

void WebTransformAnimationCurve::initialize()
{
    m_private.reset(WebCore::CCKeyframedTransformAnimationCurve::create().leakPtr());
}

void WebTransformAnimationCurve::destroy()
{
    m_private.reset(0);
}

} // namespace WebKit
