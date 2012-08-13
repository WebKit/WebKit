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

#ifndef WebAnimationImpl_h
#define WebAnimationImpl_h

#include <public/WebAnimation.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class CCActiveAnimation;
}

namespace WebKit {

class WebAnimationImpl : public WebAnimation {
public:
    explicit WebAnimationImpl(PassOwnPtr<WebCore::CCActiveAnimation> animation)
        : m_animation(animation)
    {
    }
    virtual ~WebAnimationImpl() { }

    // WebAnimation implementation
    virtual TargetProperty targetProperty() const OVERRIDE;
    virtual int iterations() const OVERRIDE;
    virtual void setIterations(int) OVERRIDE;
    virtual double startTime() const OVERRIDE;
    virtual void setStartTime(double monotonicTime) OVERRIDE;
    virtual double timeOffset() const OVERRIDE;
    virtual void setTimeOffset(double monotonicTime) OVERRIDE;
    virtual bool alternatesDirection() const OVERRIDE;
    virtual void setAlternatesDirection(bool) OVERRIDE;

    PassOwnPtr<WebCore::CCActiveAnimation> cloneToCCAnimation();
private:
    OwnPtr<WebCore::CCActiveAnimation> m_animation;
};

}

#endif // WebAnimationImpl_h

