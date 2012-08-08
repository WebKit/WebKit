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

#include "CCAnimationTestCommon.h"

#include "LayerChromium.h"
#include "cc/CCKeyframedAnimationCurve.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerImpl.h"

#include <public/WebTransformOperations.h>

using namespace WebCore;

namespace {

template <class Target>
void addOpacityTransition(Target& target, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create());

    if (duration > 0)
        curve->addKeyframe(CCFloatKeyframe::create(0, startOpacity, useTimingFunction ? nullptr : CCEaseTimingFunction::create()));
    curve->addKeyframe(CCFloatKeyframe::create(duration, endOpacity, nullptr));

    OwnPtr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.release(), 0, 0, CCActiveAnimation::Opacity));
    animation->setNeedsSynchronizedStartTime(true);

    target.addAnimation(animation.release());
}

template <class Target>
void addAnimatedTransform(Target& target, double duration, int deltaX, int deltaY)
{
    static int id = 0;
    OwnPtr<CCKeyframedTransformAnimationCurve> curve(CCKeyframedTransformAnimationCurve::create());

    if (duration > 0) {
        WebKit::WebTransformOperations startOperations;
        startOperations.appendTranslate(deltaX, deltaY, 0);
        curve->addKeyframe(CCTransformKeyframe::create(0, startOperations, nullptr));
    }

    WebKit::WebTransformOperations operations;
    operations.appendTranslate(deltaX, deltaY, 0);
    curve->addKeyframe(CCTransformKeyframe::create(duration, operations, nullptr));

    OwnPtr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.release(), id++, 0, CCActiveAnimation::Transform));
    animation->setNeedsSynchronizedStartTime(true);

    target.addAnimation(animation.release());
}

} // namespace

namespace WebKitTests {

FakeFloatAnimationCurve::FakeFloatAnimationCurve()
{
}

FakeFloatAnimationCurve::~FakeFloatAnimationCurve()
{
}

PassOwnPtr<WebCore::CCAnimationCurve> FakeFloatAnimationCurve::clone() const
{
    return adoptPtr(new FakeFloatAnimationCurve);
}

FakeTransformTransition::FakeTransformTransition(double duration)
    : m_duration(duration)
{
}

FakeTransformTransition::~FakeTransformTransition()
{
}

WebKit::WebTransformationMatrix FakeTransformTransition::getValue(double time) const
{
    return WebKit::WebTransformationMatrix();
}

PassOwnPtr<WebCore::CCAnimationCurve> FakeTransformTransition::clone() const
{
    return adoptPtr(new FakeTransformTransition(*this));
}


FakeFloatTransition::FakeFloatTransition(double duration, float from, float to)
    : m_duration(duration)
    , m_from(from)
    , m_to(to)
{
}

FakeFloatTransition::~FakeFloatTransition()
{
}

float FakeFloatTransition::getValue(double time) const
{
    time /= m_duration;
    if (time >= 1)
        time = 1;
    return (1 - time) * m_from + time * m_to;
}

FakeLayerAnimationControllerClient::FakeLayerAnimationControllerClient()
    : m_opacity(0)
{
}

FakeLayerAnimationControllerClient::~FakeLayerAnimationControllerClient()
{
}

PassOwnPtr<WebCore::CCAnimationCurve> FakeFloatTransition::clone() const
{
    return adoptPtr(new FakeFloatTransition(*this));
}

void addOpacityTransitionToController(WebCore::CCLayerAnimationController& controller, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(controller, duration, startOpacity, endOpacity, useTimingFunction);
}

void addAnimatedTransformToController(WebCore::CCLayerAnimationController& controller, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(controller, duration, deltaX, deltaY);
}

void addOpacityTransitionToLayer(WebCore::LayerChromium& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(layer, duration, startOpacity, endOpacity, useTimingFunction);
}

void addOpacityTransitionToLayer(WebCore::CCLayerImpl& layer, double duration, float startOpacity, float endOpacity, bool useTimingFunction)
{
    addOpacityTransition(*layer.layerAnimationController(), duration, startOpacity, endOpacity, useTimingFunction);
}

void addAnimatedTransformToLayer(WebCore::LayerChromium& layer, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(layer, duration, deltaX, deltaY);
}

void addAnimatedTransformToLayer(WebCore::CCLayerImpl& layer, double duration, int deltaX, int deltaY)
{
    addAnimatedTransform(*layer.layerAnimationController(), duration, deltaX, deltaY);
}

} // namespace WebKitTests
