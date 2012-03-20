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

#ifndef CCAnimationTestCommon_h
#define CCAnimationTestCommon_h

#include "cc/CCAnimationCurve.h"
#include "cc/CCLayerAnimationController.h"

#include <wtf/OwnPtr.h>

namespace WebCore {
class LayerChromium;
}

namespace WebKitTests {

class FakeFloatAnimationCurve : public WebCore::CCFloatAnimationCurve {
public:
    FakeFloatAnimationCurve();
    virtual ~FakeFloatAnimationCurve();

    virtual double duration() const { return 1; }
    virtual float getValue(double now) const { return 0; }
    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const;
};

class FakeTransformTransition : public WebCore::CCTransformAnimationCurve {
public:
    FakeTransformTransition(double duration);
    virtual ~FakeTransformTransition();

    virtual double duration() const { return m_duration; }
    virtual WebCore::TransformationMatrix getValue(double time, const WebCore::IntSize&) const;

    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const;

private:
    double m_duration;
};

class FakeFloatTransition : public WebCore::CCFloatAnimationCurve {
public:
    FakeFloatTransition(double duration, float from, float to);
    virtual ~FakeFloatTransition();

    virtual double duration() const { return m_duration; }
    virtual float getValue(double time) const;

    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const;

private:
    double m_duration;
    float m_from;
    float m_to;
};

class FakeLayerAnimationControllerClient : public WebCore::CCLayerAnimationControllerClient {
public:
    FakeLayerAnimationControllerClient();
    virtual ~FakeLayerAnimationControllerClient();

    // CCLayerAnimationControllerClient implementation
    virtual int id() const { return 0; }
    virtual void setOpacityFromAnimation(float opacity) { m_opacity = opacity; }
    virtual float opacity() const { return m_opacity; }
    virtual void setTransformFromAnimation(const WebCore::TransformationMatrix& transform) { m_transform = transform; }
    virtual const WebCore::TransformationMatrix& transform() const { return m_transform; }
    virtual const WebCore::IntSize& bounds() const { return m_bounds; }

private:
    float m_opacity;
    WebCore::TransformationMatrix m_transform;
    WebCore::IntSize m_bounds;
};

void addOpacityTransitionToController(WebCore::CCLayerAnimationController&, double duration, float startOpacity, float endOpacity);

void addOpacityTransitionToLayer(WebCore::LayerChromium&, double duration, float startOpacity, float endOpacity);

} // namespace WebKitTests

#endif // CCAnimationTesctCommon_h
