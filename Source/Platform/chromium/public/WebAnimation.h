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

#ifndef WebAnimation_h
#define WebAnimation_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivateOwnPtr.h"

#if WEBKIT_IMPLEMENTATION
#include <wtf/Forward.h>
#endif

namespace WebCore {
class CCActiveAnimation;
}

namespace WebKit {

class WebAnimationCurve;

// A compositor driven animation.
class WebAnimation {
public:
    enum TargetProperty {
        TargetPropertyTransform = 0,
        TargetPropertyOpacity
    };

    // The caller takes ownership of the returned valuev
    WEBKIT_EXPORT static WebAnimation* create(const WebAnimationCurve&, TargetProperty);

    // An animationId is effectively the animation's name, and it is not unique.
    // Animations with the same groupId are run at the same time. An animation
    // may be uniquely identified by a combination of groupId and target property.
    WEBKIT_EXPORT static WebAnimation* create(const WebAnimationCurve&, int animationId, int groupId, TargetProperty);

    virtual ~WebAnimation() { }

    virtual TargetProperty targetProperty() const = 0;

    // This is the number of times that the animation will play. If this
    // value is zero the animation will not play. If it is negative, then
    // the animation will loop indefinitely.
    virtual int iterations() const = 0;
    virtual void setIterations(int) = 0;

    virtual double startTime() const = 0;
    virtual void setStartTime(double monotonicTime) = 0;

    virtual double timeOffset() const = 0;
    virtual void setTimeOffset(double monotonicTime) = 0;

    // If alternatesDirection is true, on odd numbered iterations we reverse the curve.
    virtual bool alternatesDirection() const = 0;
    virtual void setAlternatesDirection(bool) = 0;
};

} // namespace WebKit

#endif // WebAnimation_h
