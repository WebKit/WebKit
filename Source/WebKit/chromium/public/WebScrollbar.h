/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebScrollbar_h
#define WebScrollbar_h

#include "platform/WebCanvas.h"
#include "platform/WebCommon.h"

namespace WebKit {

class WebInputEvent;
class WebPluginContainer;
class WebScrollbarClient;
struct WebRect;

class WebScrollbar {
public:
    enum Orientation {
        Horizontal,
        Vertical
    };

    enum ScrollDirection {
        ScrollBackward,
        ScrollForward
    };

    enum ScrollGranularity {
        ScrollByLine,
        ScrollByPage,
        ScrollByDocument,
        ScrollByPixel
    };

    // Creates a WebScrollbar for use by a plugin. The plugin container and
    // client are guaranteed to outlive this object.
    WEBKIT_EXPORT static WebScrollbar* createForPlugin(Orientation,
                                                       WebPluginContainer*,
                                                       WebScrollbarClient*);

    virtual ~WebScrollbar() {}

    // Gets the thickness of the scrollbar in pixels.
    WEBKIT_EXPORT static int defaultThickness();

    // Return true if this is an overlay scrollbar.
    virtual bool isOverlay() const = 0;

    // Sets the rectangle of the scrollbar.
    virtual void setLocation(const WebRect&) = 0;

    // Gets the current value (i.e. position inside the region).
    virtual int value() const = 0;

    // Sets the current value.
    virtual void setValue(int position) = 0;

    // Sets the size of the scrollable region in pixels.  i.e. if a document is
    // 800x10000 pixels and the viewport is 1000x1000 pixels, then setLocation
    // for the vertical scrollbar would have passed in a rectangle like:
    //            (800 - defaultThickness(), 0) (defaultThickness() x 10000)
    // and setDocumentSize(10000)
    virtual void setDocumentSize(int size) = 0;

    // Scroll back or forward with the given granularity.
    virtual void scroll(ScrollDirection, ScrollGranularity, float multiplier) = 0;

    // Paint the given rectangle.
    virtual void paint(WebCanvas*, const WebRect&) = 0;

    // Returns true iff the given event was used.
    virtual bool handleInputEvent(const WebInputEvent&) = 0;
};

} // namespace WebKit

#endif
