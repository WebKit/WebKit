/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentTouch.h"

#if ENABLE(TOUCH_EVENTS)

#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "Touch.h"
#include "TouchList.h"
#include "WindowProxy.h"

namespace WebCore {

Ref<Touch> DocumentTouch::createTouch(Document& document, RefPtr<WindowProxy>&& window, EventTarget* target, int identifier, double pageX, double pageY, double screenX, double screenY, double radiusX, double radiusY, float rotationAngle, float force)
{
    RefPtr<LocalFrame> frame;
    if (window)
        frame = dynamicDowncast<LocalFrame>(window->frame());
    if (!frame)
        frame = document.frame();

    // Match behavior from when these types were integers, and avoid surprises from someone explicitly
    // passing Infinity/NaN.
    if (!std::isfinite(pageX))
        pageX = 0;
    if (!std::isfinite(pageY))
        pageY = 0;
    if (!std::isfinite(screenX))
        screenX = 0;
    if (!std::isfinite(screenY))
        screenY = 0;
    if (!std::isfinite(radiusX))
        radiusX = 0;
    if (!std::isfinite(radiusY))
        radiusY = 0;
    if (!std::isfinite(rotationAngle))
        rotationAngle = 0;
    if (!std::isfinite(force))
        force = 0;

    // FIXME: It's not clear from the documentation at
    // http://developer.apple.com/library/safari/#documentation/UserExperience/Reference/DocumentAdditionsReference/DocumentAdditions/DocumentAdditions.html
    // when this method should throw and nor is it by inspection of iOS behavior. It would be nice to verify any cases where it throws under iOS
    // and implement them here. See https://bugs.webkit.org/show_bug.cgi?id=47819
    return Touch::create(frame.get(), target, identifier, DoublePoint(screenX, screenY), DoublePoint(pageX, pageY), DoubleSize(radiusX, radiusY), rotationAngle, force);
}

Ref<TouchList> DocumentTouch::createTouchList(Document&, FixedVector<std::reference_wrapper<Touch>>&& touches)
{
    return TouchList::create(WTFMove(touches));
}

}

#endif
