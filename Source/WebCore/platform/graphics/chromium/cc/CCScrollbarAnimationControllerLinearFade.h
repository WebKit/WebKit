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

#ifndef CCScrollbarAnimationControllerLinearFade_h
#define CCScrollbarAnimationControllerLinearFade_h

#include "CCScrollbarAnimationController.h"

namespace WebCore {

class CCScrollbarAnimationControllerLinearFade : public CCScrollbarAnimationController {
public:
    static PassOwnPtr<CCScrollbarAnimationControllerLinearFade> create(CCLayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength);

    virtual ~CCScrollbarAnimationControllerLinearFade();

    virtual bool animate(double monotonicTime) OVERRIDE;

    virtual void didPinchGestureUpdateAtTime(double monotonicTime) OVERRIDE;
    virtual void didPinchGestureEndAtTime(double monotonicTime) OVERRIDE;
    virtual void updateScrollOffsetAtTime(CCLayerImpl* scrollLayer, double monotonicTime) OVERRIDE;

protected:
    CCScrollbarAnimationControllerLinearFade(CCLayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength);

private:
    float opacityAtTime(double monotonicTime);

    double m_lastAwakenTime;
    bool m_pinchGestureInEffect;

    double m_fadeoutDelay;
    double m_fadeoutLength;
};

} // namespace WebCore

#endif // CCScrollbarAnimationControllerLinearFade_h
