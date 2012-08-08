/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFlingAnimatorToGestureCurveAdapter_h
#define WebFlingAnimatorToGestureCurveAdapter_h

#include "FloatPoint.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "PlatformGestureCurve.h"
#include "PlatformGestureCurveTarget.h"
#include <public/Platform.h>
#include <public/WebFlingAnimator.h>

namespace WebKit {

class WebFlingAnimatorToGestureCurveAdapter : public WebCore::PlatformGestureCurve {
public:
    static PassOwnPtr<PlatformGestureCurve> create(const WebCore::FloatPoint& velocity, const WebCore::IntRect& range, PassOwnPtr<WebFlingAnimator> animator)
    {
        return adoptPtr(new WebFlingAnimatorToGestureCurveAdapter(velocity, range, animator));
    }

    // WebCore::PlatformGestureCurve implementation:
    virtual const char* debugName() const OVERRIDE { return "WebFlingAnimatorToGestureCurveAdapter"; }
    virtual bool apply(double time, WebCore::PlatformGestureCurveTarget* target) OVERRIDE
    {
        if (!m_animator->updatePosition())
            return false;

        WebCore::IntPoint currentPosition = m_animator->getCurrentPosition();
        target->scrollBy(WebCore::IntPoint(currentPosition - m_lastPosition));
        m_lastPosition = currentPosition;
        return true;
    }

private:
    WebFlingAnimatorToGestureCurveAdapter(const WebCore::FloatPoint& velocity, const WebCore::IntRect& range, PassOwnPtr<WebFlingAnimator> animator)
        : m_animator(animator)
    {
        m_animator->startFling(velocity, range);
    }

    OwnPtr<WebFlingAnimator> m_animator;
    WebCore::IntPoint m_lastPosition;
};

}

#endif // WebFlingAnimatorToGestureCurveAdapter_h
