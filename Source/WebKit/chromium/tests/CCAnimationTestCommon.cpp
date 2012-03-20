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

#include "GraphicsLayer.h"
#include "LayerChromium.h"
#include "cc/CCLayerAnimationController.h"

using namespace WebCore;

namespace {

template <class Target>
void addOpacityTransition(Target& target, double duration, float startOpacity, float endOpacity)
{
    WebCore::KeyframeValueList values(AnimatedPropertyOpacity);
    if (duration > 0)
        values.insert(new FloatAnimationValue(0, startOpacity));
    values.insert(new FloatAnimationValue(duration, endOpacity));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;

    target.addAnimation(values, boxSize, animation.get(), 0, 0, 0);
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

WebCore::TransformationMatrix FakeTransformTransition::getValue(double time, const WebCore::IntSize& size) const
{
    return WebCore::TransformationMatrix();
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

void addOpacityTransitionToController(WebCore::CCLayerAnimationController& controller, double duration, float startOpacity, float endOpacity)
{
    addOpacityTransition(controller, duration, startOpacity, endOpacity);
}

void addOpacityTransitionToLayer(WebCore::LayerChromium& layer, double duration, float startOpacity, float endOpacity)
{
    addOpacityTransition(layer, duration, startOpacity, endOpacity);
}

} // namespace WebKitTests
