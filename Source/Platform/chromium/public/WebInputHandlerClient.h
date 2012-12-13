/* Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebInputHandlerClient_h
#define WebInputHandlerClient_h

#include "WebCommon.h"
#include "WebPoint.h"
#include "WebSize.h"

namespace WebKit {

class WebInputHandlerClient {
public:
    enum ScrollStatus {
        ScrollStatusOnMainThread,
        ScrollStatusStarted,
        ScrollStatusIgnored
    };
    enum ScrollInputType {
        ScrollInputTypeGesture,
        ScrollInputTypeWheel
    };

    // Selects a layer to be scrolled at a given point in window coordinates.
    // Returns ScrollStarted if the layer at the coordinates can be scrolled,
    // ScrollOnMainThread if the scroll event should instead be delegated to the
    // main thread, or ScrollIgnored if there is nothing to be scrolled at the
    // given coordinates.
    virtual ScrollStatus scrollBegin(WebPoint, ScrollInputType) = 0;

    // Scroll the selected layer starting at the given window coordinate. If
    // there is no room to move the layer in the requested direction, its first
    // ancestor layer that can be scrolled will be moved instead. If there is no
    // such layer to be moved, this returns false. Returns true otherwise.
    // Should only be called if scrollBegin() returned ScrollStarted.
    virtual bool scrollByIfPossible(WebPoint, WebSize) = 0;

    // Stop scrolling the selected layer. Should only be called if scrollBegin()
    // returned ScrollStarted.
    virtual void scrollEnd() = 0;

    virtual void pinchGestureBegin() = 0;
    virtual void pinchGestureUpdate(float magnifyDelta, WebPoint anchor) = 0;
    virtual void pinchGestureEnd() = 0;

    virtual void startPageScaleAnimation(WebSize targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTime,
                                         double duration) = 0;

    // Request another callback to WebInputHandler::animate().
    virtual void scheduleAnimation() = 0;

    // Returns whether there are any touch event handlers registered on the
    // given WebPoint.
    virtual bool haveTouchEventHandlersAt(WebPoint) = 0;

    // Indicate that the final input event for the current vsync interval was received.
    virtual void didReceiveLastInputEventForVSync() { }

protected:
    virtual ~WebInputHandlerClient() { }
};

}

#endif
