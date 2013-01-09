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

#ifndef WebPluginContainer_h
#define WebPluginContainer_h

#include <public/WebCommon.h>

struct NPObject;

namespace WebKit {

class WebElement;
class WebPlugin;
class WebString;
class WebURL;
class WebURLRequest;
class WebLayer;
struct WebPoint;
struct WebRect;

class WebPluginContainer {
public:
    enum TouchEventRequestType {
        TouchEventRequestTypeNone,
        TouchEventRequestTypeRaw,
        TouchEventRequestTypeSynthesizedMouse,
    };

    // Returns the element containing this plugin.
    virtual WebElement element() = 0;

    virtual void invalidate() = 0;
    virtual void invalidateRect(const WebRect&) = 0;
    virtual void scrollRect(int dx, int dy, const WebRect&) = 0;

    // Causes the container to report its current geometry via
    // WebPlugin::updateGeometry.
    virtual void reportGeometry() = 0;

    // Drop any references to script objects allocated by the plugin.
    // These are objects derived from WebPlugin::scriptableObject.  This is
    // called when the plugin is being destroyed or if it needs to be
    // re-initialized.
    virtual void clearScriptObjects() = 0;

    // Returns the scriptable object associated with the DOM element
    // containing the plugin.
    virtual NPObject* scriptableObjectForElement() = 0;

    // Executes a "javascript:" URL on behalf of the plugin in the context
    // of the frame containing the plugin.  Returns the result of script
    // execution, if any.
    virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed) = 0;

    // Loads an URL in the specified frame (or the frame containing this
    // plugin if target is empty).  If notifyNeeded is true, then upon
    // completion, WebPlugin::didFinishLoadingFrameRequest is called if the
    // load was successful or WebPlugin::didFailLoadingFrameRequest is
    // called if the load failed.  The given notifyData is passed along to
    // the callback.
    virtual void loadFrameRequest(
        const WebURLRequest&, const WebString& target, bool notifyNeeded, void* notifyData) = 0;

    // Notifies that the zoom level has changed.
    // Note, this does NOT affect pageScaleFactor or pageZoomFactor
    virtual void zoomLevelChanged(double zoomLevel) = 0;

    // Determines whether the given rectangle in this plugin is above all other
    // content. The rectangle is in the plugin's coordinate system.
    virtual bool isRectTopmost(const WebRect&) = 0;

    // Notifies when the plugin changes the kind of touch-events it accepts.
    virtual void requestTouchEventType(TouchEventRequestType) = 0;

    // Notifies when the plugin starts/stops accepting wheel events. Without
    // calling the function with true, the container might not always able to
    // receive wheel events in some cases (such as when threaded compositing
    // is in use but a scroll bar is not in use).
    virtual void setWantsWheelEvents(bool) = 0;

    // Converts view's window coordinates to plugin's local coordinates.
    virtual WebPoint windowToLocalPoint(const WebPoint&) = 0;

    virtual WebPlugin* plugin() = 0;
    virtual void setPlugin(WebPlugin*) = 0;

    virtual float deviceScaleFactor() = 0;
    virtual float pageScaleFactor() = 0;
    virtual float pageZoomFactor() = 0;

    // Sets the layer representing the plugin for compositing. The
    // WebPluginContainer does *not* take ownership.
    virtual void setWebLayer(WebLayer*) = 0;

protected:
    ~WebPluginContainer() { }
};

} // namespace WebKit

#endif
