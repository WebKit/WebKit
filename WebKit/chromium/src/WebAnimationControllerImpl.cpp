/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAnimationControllerImpl.h"

#include "AnimationController.h"
#include "Element.h"

#include "WebElement.h"
#include "WebFrameImpl.h"
#include "WebString.h"

using namespace WebCore;

namespace WebKit {

WebAnimationControllerImpl::WebAnimationControllerImpl(WebFrameImpl* frameImpl)
    : m_frameImpl(frameImpl)
{
    ASSERT(m_frameImpl);
}

AnimationController* WebAnimationControllerImpl::animationController() const
{
    if (!m_frameImpl->frame())
        return 0;
    return m_frameImpl->frame()->animation();
}

bool WebAnimationControllerImpl::pauseAnimationAtTime(WebElement& element,
                                                      const WebString& animationName,
                                                      double time)
{
    AnimationController* controller = animationController();
    if (!controller)
        return 0;
    return controller->pauseAnimationAtTime(PassRefPtr<Element>(element)->renderer(),
                                            animationName,
                                            time);
}

bool WebAnimationControllerImpl::pauseTransitionAtTime(WebElement& element,
                                                       const WebString& propertyName,
                                                       double time)
{
    AnimationController* controller = animationController();
    if (!controller)
        return 0;
    return controller->pauseTransitionAtTime(PassRefPtr<Element>(element)->renderer(),
                                             propertyName,
                                             time);
}

unsigned WebAnimationControllerImpl::numberOfActiveAnimations() const
{
    AnimationController* controller = animationController();
    if (!controller)
        return 0;
    return controller->numberOfActiveAnimations();
}

} // namespace WebKit
