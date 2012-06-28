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

#ifndef WebPluginScrollbar_h
#define WebPluginScrollbar_h

#include <public/WebCanvas.h>
#include <public/WebScrollbar.h>

namespace WebKit {

class WebInputEvent;
class WebPluginContainer;
class WebPluginScrollbarClient;
struct WebRect;

class WebPluginScrollbar : public WebScrollbar {
public:
    // Creates a WebPluginScrollbar for use by a plugin. The plugin container and
    // client are guaranteed to outlive this object.
    WEBKIT_EXPORT static WebPluginScrollbar* createForPlugin(WebScrollbar::Orientation,
                                                             WebPluginContainer*,
                                                             WebPluginScrollbarClient*);

    virtual ~WebPluginScrollbar() { }

    // Gets the thickness of the scrollbar in pixels.
    WEBKIT_EXPORT static int defaultThickness();

    // Sets the rectangle of the scrollbar.
    virtual void setLocation(const WebRect&) = 0;

    // Sets the size of the scrollable region in pixels, i.e. if a document is
    // 800x10000 pixels and the viewport is 1000x1000 pixels, then setLocation
    // for the vertical scrollbar would have passed in a rectangle like:
    //            (800 - defaultThickness(), 0) (defaultThickness() x 10000)
    // and setDocumentSize(10000)
    virtual void setDocumentSize(int) = 0;

    // Sets the current value.
    virtual void setValue(int position) = 0;

    // Scroll back or forward with the given granularity.
    virtual void scroll(ScrollDirection, ScrollGranularity, float multiplier) = 0;

    // Paint the given rectangle.
    virtual void paint(WebCanvas*, const WebRect&) = 0;

    // Returns true iff the given event was used.
    virtual bool handleInputEvent(const WebInputEvent&) = 0;
};

} // namespace WebKit

#endif
