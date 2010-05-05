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

#ifndef WebWidget_h
#define WebWidget_h

#include "WebCanvas.h"
#include "WebCommon.h"
#include "WebCompositionCommand.h"
#include "WebTextDirection.h"

namespace WebKit {

class WebInputEvent;
class WebString;
struct WebRect;
struct WebSize;

class WebWidget {
public:
    // This method closes and deletes the WebWidget.
    virtual void close() = 0;

    // Returns the current size of the WebWidget.
    virtual WebSize size() = 0;

    // Called to resize the WebWidget.
    virtual void resize(const WebSize&) = 0;

    // Called to layout the WebWidget.  This MUST be called before Paint,
    // and it may result in calls to WebWidgetClient::didInvalidateRect.
    virtual void layout() = 0;

    // Called to paint the specified region of the WebWidget onto the given
    // canvas.  You MUST call Layout before calling this method.  It is
    // okay to call paint multiple times once layout has been called,
    // assuming no other changes are made to the WebWidget (e.g., once
    // events are processed, it should be assumed that another call to
    // layout is warranted before painting again).
    virtual void paint(WebCanvas*, const WebRect&) = 0;

    // Called to inform the WebWidget of an input event.  Returns true if
    // the event has been processed, false otherwise.
    virtual bool handleInputEvent(const WebInputEvent&) = 0;

    // Called to inform the WebWidget that mouse capture was lost.
    virtual void mouseCaptureLost() = 0;

    // Called to inform the WebWidget that it has gained or lost keyboard focus.
    virtual void setFocus(bool) = 0;

    // Called to inform the WebWidget of a composition event.
    virtual bool handleCompositionEvent(WebCompositionCommand command,
                                        int cursorPosition,
                                        int targetStart,
                                        int targetEnd,
                                        const WebString& text) = 0;

    // Retrieve the status of this WebWidget required by IME APIs.  Upon
    // success enabled and caretBounds are set.
    virtual bool queryCompositionStatus(bool* enabled, WebRect* caretBounds) = 0;

    // Changes the text direction of the selected input node.
    virtual void setTextDirection(WebTextDirection) = 0;

    // Returns true if the WebWidget uses GPU accelerated compositing
    // to render its contents.
    virtual bool isAcceleratedCompositingActive() const = 0;

protected:
    ~WebWidget() { }
};

} // namespace WebKit

#endif
