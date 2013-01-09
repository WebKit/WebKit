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

#include "WebCompositionUnderline.h"
#include "WebTextDirection.h"
#include "WebTextInputInfo.h"
#include <public/WebCanvas.h>
#include <public/WebCommon.h>
#include <public/WebRect.h>
#include <public/WebSize.h>

namespace WebKit {

class WebInputEvent;
class WebLayerTreeView;
class WebMouseEvent;
class WebString;
struct WebPoint;
struct WebRenderingStats;
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

    // Called to notify the WebWidget of entering/exiting fullscreen mode. The
    // resize method may be called between will{Enter,Exit}FullScreen and
    // did{Enter,Exit}FullScreen.
    virtual void willEnterFullScreen() { }
    virtual void didEnterFullScreen() { }
    virtual void willExitFullScreen() { }
    virtual void didExitFullScreen() { }

    // Called to update imperative animation state. This should be called before
    // paint, although the client can rate-limit these calls.
    virtual void animate(double ignored) { }

    // Called to layout the WebWidget. This MUST be called before Paint,
    // and it may result in calls to WebWidgetClient::didInvalidateRect.
    virtual void layout() { }

    // Called to toggle the WebWidget in or out of force compositing mode. This
    // should be called before paint.
    virtual void enterForceCompositingMode(bool enter) { }

    enum PaintOptions {
        // Attempt to fulfill the painting request by reading back from the
        // compositor, assuming we're using a compositor to render.
        ReadbackFromCompositorIfAvailable,

        // Force the widget to rerender onto the canvas using software. This
        // mode ignores 3d transforms and ignores GPU-resident content, such
        // as video, canvas, and WebGL.
        //
        // Note: This option exists on OS(ANDROID) and will hopefully be
        //       removed once the link disambiguation feature renders using
        //       the compositor.
        ForceSoftwareRenderingAndIgnoreGPUResidentContent,
    };

    // Called to paint the rectangular region within the WebWidget
    // onto the specified canvas at (viewPort.x,viewPort.y). You MUST call
    // Layout before calling this method. It is okay to call paint
    // multiple times once layout has been called, assuming no other
    // changes are made to the WebWidget (e.g., once events are
    // processed, it should be assumed that another call to layout is
    // warranted before painting again).
    virtual void paint(WebCanvas*, const WebRect& viewPort, PaintOptions = ReadbackFromCompositorIfAvailable) { }

    // In non-threaded compositing mode, triggers compositing of the current
    // layers onto the screen. You MUST call Layout before calling this method,
    // for the same reasons described in the paint method above
    //
    // In threaded compositing mode, indicates that the widget should update
    // itself, for example due to window damage. The redraw will begin
    // asynchronously and perform layout and animation internally. Do not call
    // animate or layout in this case.
    virtual void composite(bool finish) = 0;

    // Returns true if we've started tracking repaint rectangles.
    virtual bool isTrackingRepaints() const { return false; }

    // Indicates that the compositing surface associated with this WebWidget is
    // ready to use.
    virtual void setCompositorSurfaceReady() = 0;

    // Temporary method for the embedder to notify the WebWidget that the widget
    // has taken damage, e.g. due to a window expose. This method will be
    // removed when the WebWidget inversion patch lands --- http://crbug.com/112837
    virtual void setNeedsRedraw() { }

    // Temporary method for the embedder to check for throttled input. When this
    // is true, the WebWidget is indicating that it would prefer to not receive
    // additional input events until
    // WebWidgetClient::didBecomeReadyForAdditionalInput is called.
    //
    // This method will be removed when the WebWidget inversion patch lands ---
    // http://crbug.com/112837
    virtual bool isInputThrottled() const { return false; }

    // Called to inform the WebWidget of a change in theme.
    // Implementors that cache rendered copies of widgets need to re-render
    // on receiving this message
    virtual void themeChanged() { }

    // Called to inform the WebWidget of an input event. Returns true if
    // the event has been processed, false otherwise.
    virtual bool handleInputEvent(const WebInputEvent&) { return false; }

    // Check whether the given point hits any registered touch event handlers.
    virtual bool hasTouchEventHandlersAt(const WebPoint&) { return true; }

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

    // Returns information about the current text input of this WebWidget.
    virtual WebTextInputInfo textInputInfo() { return WebTextInputInfo(); }

    // Returns the current text input type of this WebWidget.
    // FIXME: Remove this method. It's redundant with textInputInfo().
    virtual WebTextInputType textInputType() { return WebTextInputTypeNone; }

    // Returns the anchor and focus bounds of the current selection.
    // If the selection range is empty, it returns the caret bounds.
    virtual bool selectionBounds(WebRect& anchor, WebRect& focus) const { return false; }

    // Returns the text direction at the start and end bounds of the current selection.
    // If the selection range is empty, it returns false.
    virtual bool selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const { return false; }

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

    // Calling WebWidgetClient::requestPointerLock() will result in one
    // return call to didAcquirePointerLock() or didNotAcquirePointerLock().
    virtual void didAcquirePointerLock() { }
    virtual void didNotAcquirePointerLock() { }

    // Pointer lock was held, but has been lost. This may be due to a
    // request via WebWidgetClient::requestPointerUnlock(), or for other
    // reasons such as the user exiting lock, window focus changing, etc.
    virtual void didLosePointerLock() { }

    // Informs the WebWidget that the resizer rect changed. Happens for example
    // on mac, when a widget appears below the WebWidget without changing the
    // WebWidget's size (WebWidget::resize() automatically checks the resizer
    // rect.)
    virtual void didChangeWindowResizerRect() { }

    // Instrumentation method that marks beginning of frame update that includes
    // things like animate()/layout()/paint()/composite().
    virtual void instrumentBeginFrame() { }
    // Cancels the effect of instrumentBeginFrame() in case there were no events
    // following the call to instrumentBeginFrame().
    virtual void instrumentCancelFrame() { }

    // Fills in a WebRenderingStats struct containing information about rendering, e.g. count of frames rendered, time spent painting.
    // This call is relatively expensive in threaded compositing mode, as it blocks on the compositor thread.
    // It is safe to call in software mode, but will only give stats for rendering done in compositing mode.
    virtual void renderingStats(WebRenderingStats&) const { }

    // The page background color. Can be used for filling in areas without
    // content.
    virtual WebColor backgroundColor() const { return 0xFFFFFFFF; /* SK_ColorWHITE */ }

protected:
    ~WebWidget() { }
};

} // namespace WebKit

#endif
