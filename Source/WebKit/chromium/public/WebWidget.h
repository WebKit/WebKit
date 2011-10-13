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
#include "WebCompositionUnderline.h"
#include "WebRect.h"
#include "WebSize.h"
#include "WebTextInputType.h"
#include "WebTextDirection.h"

namespace WebKit {

class WebInputEvent;
class WebString;
struct WebPoint;
template <typename T> class WebVector;

class WebWidget {
public:
    // This method closes and deletes the WebWidget.
    virtual void close() { }

    // Returns the current size of the WebWidget.
    virtual WebSize size() { return WebSize(); }

    // Used to group a series of resize events. For example, if the user
    // drags a resizer then willStartLiveResize will be called, followed by a
    // sequence of resize events, ending with willEndLiveResize when the user
    // lets go of the resizer.
    virtual void willStartLiveResize() { }

    // Called to resize the WebWidget.
    virtual void resize(const WebSize&) { }

    // Ends a group of resize events that was started with a call to
    // willStartLiveResize.
    virtual void willEndLiveResize() { }

    // Called to update imperative animation state. This should be called before
    // paint, although the client can rate-limit these calls. When
    // frameBeginTime is 0.0, the WebWidget will determine the frame begin time
    // itself.
    virtual void animate(double frameBeginTime) { }

    // Called to layout the WebWidget.  This MUST be called before Paint,
    // and it may result in calls to WebWidgetClient::didInvalidateRect.
    virtual void layout() { }

    // Called to paint the rectangular region within the WebWidget
    // onto the specified canvas at (viewPort.x,viewPort.y). You MUST call
    // Layout before calling this method.  It is okay to call paint
    // multiple times once layout has been called, assuming no other
    // changes are made to the WebWidget (e.g., once events are
    // processed, it should be assumed that another call to layout is
    // warranted before painting again).
    virtual void paint(WebCanvas*, const WebRect& viewPort) { }

    // In non-threaded compositing mode, triggers compositing of the current
    // layers onto the screen. You MUST call Layout before calling this method, for the same
    // reasons described in the paint method above
    //
    // In threaded compositing mode, indicates that the widget should update
    // itself, for example due to window damage. The redraw will begin
    // asynchronously and perform layout and animation internally. Do not call
    // animate or layout in this case.
    virtual void composite(bool finish) = 0;

    // Called to inform the WebWidget of a change in theme.
    // Implementors that cache rendered copies of widgets need to re-render
    // on receiving this message
    virtual void themeChanged() { }

    // Called to inform the WebWidget of an input event.  Returns true if
    // the event has been processed, false otherwise.
    virtual bool handleInputEvent(const WebInputEvent&) { return false; }

    // Called to inform the WebWidget that mouse capture was lost.
    virtual void mouseCaptureLost() { }

    // Called to inform the WebWidget that it has gained or lost keyboard focus.
    virtual void setFocus(bool) { }

    // Called to inform the WebWidget of a new composition text.
    // If selectionStart and selectionEnd has the same value, then it indicates
    // the input caret position. If the text is empty, then the existing
    // composition text will be cancelled.
    // Returns true if the composition text was set successfully.
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd) { return false; }

    // Called to inform the WebWidget to confirm an ongoing composition.
    // This method is same as confirmComposition(WebString());
    // Returns true if there is an ongoing composition.
    virtual bool confirmComposition() { return false; }

    // Called to inform the WebWidget to confirm an ongoing composition with a
    // new composition text. If the text is empty then the current composition
    // text is confirmed. If there is no ongoing composition, then deletes the
    // current selection and inserts the text. This method has no effect if
    // there is no ongoing composition and the text is empty.
    // Returns true if there is an ongoing composition or the text is inserted.
    virtual bool confirmComposition(const WebString& text) { return false; }

    // Fetches the character range of the current composition, also called the
    // "marked range." Returns true and fills the out-paramters on success;
    // returns false on failure.
    virtual bool compositionRange(size_t* location, size_t* length) { return false; }

    // Returns the current text input type of this WebWidget.
    virtual WebTextInputType textInputType() { return WebKit::WebTextInputTypeNone; }

    // Returns the plain text around the edit caret and the focus index in the
    // text. If selection exists, it will return the anchor index as well,
    // otherwise the anchor index will be the same value of the focus index.
    virtual bool getSelectionOffsetsAndTextInEditableContent(WebString&, size_t& focus, size_t& anchor) const { return false; }

    // FIXME: It has been replaced by selectionBounds. Remove it when chromium
    // switches to selectionBounds.
    // Returns the current caret bounds of this WebWidget. The selection bounds
    // will be returned if a selection range is available.
    virtual WebRect caretOrSelectionBounds() { return WebRect(); }

    // FIXME: It has been replaced by selectionBounds. Remove it when chromium
    // switches to selectionBounds.
    // Returns the start and end point for the current selection, aligned to the
    // bottom of the selected line. start and end are the logical beginning and
    // ending positions of the selection. Visually, start may lie after end.
    virtual bool selectionRange(WebPoint& start, WebPoint& end) const { return false; }

    // Returns the start and end bounds of the current selection.
    // If the selection range is empty, it returns the caret bounds.
    virtual bool selectionBounds(WebRect& start, WebRect& end) const { return false; }

    // Fetch the current selection range of this WebWidget. If there is no
    // selection, it will output a 0-length range with the location at the
    // caret. Returns true and fills the out-paramters on success; returns false
    // on failure.
    virtual bool caretOrSelectionRange(size_t* location, size_t* length) { return false; }

    // Changes the text direction of the selected input node.
    virtual void setTextDirection(WebTextDirection) { }

    // Returns true if the WebWidget uses GPU accelerated compositing
    // to render its contents.
    virtual bool isAcceleratedCompositingActive() const { return false; }

protected:
    ~WebWidget() { }
};

} // namespace WebKit

#endif
