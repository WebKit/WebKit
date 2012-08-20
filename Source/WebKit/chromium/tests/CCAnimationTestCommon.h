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

#include "CCActiveAnimation.h"
#include "CCAnimationCurve.h"
#include "CCLayerAnimationController.h"
#include "IntSize.h"

#include <wtf/OwnPtr.h>

namespace WebCore {
class CCLayerImpl;
class LayerChromium;
}

namespace WebKitTests {

class FakeFloatAnimationCurve : public WebCore::CCFloatAnimationCurve {
public:
    FakeFloatAnimationCurve();
    virtual ~FakeFloatAnimationCurve();

    virtual double duration() const OVERRIDE { return 1; }
    virtual float getValue(double now) const OVERRIDE { return 0; }
    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const OVERRIDE;
};

class FakeTransformTransition : public WebCore::CCTransformAnimationCurve {
public:
    FakeTransformTransition(double duration);
    virtual ~FakeTransformTransition();

    virtual double duration() const OVERRIDE { return m_duration; }
    virtual WebKit::WebTransformationMatrix getValue(double time) const OVERRIDE;

    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const OVERRIDE;

private:
    double m_duration;
};

class FakeFloatTransition : public WebCore::CCFloatAnimationCurve {
public:
    FakeFloatTransition(double duration, float from, float to);
    virtual ~FakeFloatTransition();

    virtual double duration() const OVERRIDE { return m_duration; }
    virtual float getValue(double time) const OVERRIDE;

    virtual PassOwnPtr<WebCore::CCAnimationCurve> clone() const OVERRIDE;

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
    virtual int id() const OVERRIDE { return 0; }
    virtual void setOpacityFromAnimation(float opacity) OVERRIDE { m_opacity = opacity; }
    virtual float opacity() const OVERRIDE { return m_opacity; }
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix& transform) OVERRIDE { m_transform = transform; }
    virtual const WebKit::WebTransformationMatrix& transform() const OVERRIDE { return m_transform; }

private:
    float m_opacity;
    WebKit::WebTransformationMatrix m_transform;
};

void addOpacityTransitionToController(WebCore::CCLayerAnimationController&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addAnimatedTransformToController(WebCore::CCLayerAnimationController&, double duration, int deltaX, int deltaY);

void addOpacityTransitionToLayer(WebCore::LayerChromium&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);
void addOpacityTransitionToLayer(WebCore::CCLayerImpl&, double duration, float startOpacity, float endOpacity, bool useTimingFunction);

void addAnimatedTransformToLayer(WebCore::LayerChromium&, double duration, int deltaX, int deltaY);
void addAnimatedTransformToLayer(WebCore::CCLayerImpl&, double duration, int deltaX, int deltaY);

} // namespace WebKitTests

#endif // CCAnimationTesctCommon_h
