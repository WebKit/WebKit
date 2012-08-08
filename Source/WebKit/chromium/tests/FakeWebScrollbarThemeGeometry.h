/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FakeWebScrollbarThemeGeometry_h
#define FakeWebScrollbarThemeGeometry_h

#include <public/WebScrollbarThemeGeometry.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class FakeWebScrollbarThemeGeometry : public WebKit::WebScrollbarThemeGeometry {
public:
    static PassOwnPtr<WebKit::WebScrollbarThemeGeometry> create() { return adoptPtr(new WebKit::FakeWebScrollbarThemeGeometry()); }

    virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE
    {
        return new FakeWebScrollbarThemeGeometry();
    }

    virtual int thumbPosition(WebScrollbar*) OVERRIDE { return 0; }
    virtual int thumbLength(WebScrollbar*) OVERRIDE { return 0; }
    virtual int trackPosition(WebScrollbar*) OVERRIDE { return 0; }
    virtual int trackLength(WebScrollbar*) OVERRIDE { return 0; }
    virtual bool hasButtons(WebScrollbar*) OVERRIDE { return false; }
    virtual bool hasThumb(WebScrollbar*) OVERRIDE { return false; }
    virtual WebRect trackRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect thumbRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual int minimumThumbLength(WebScrollbar*) OVERRIDE { return 0; }
    virtual int scrollbarThickness(WebScrollbar*) OVERRIDE { return 0; }
    virtual WebRect backButtonStartRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect backButtonEndRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect forwardButtonStartRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect forwardButtonEndRect(WebScrollbar*) OVERRIDE { return WebRect(); }
    virtual WebRect constrainTrackRectToTrackPieces(WebScrollbar*, const WebRect&) OVERRIDE { return WebRect(); }

    virtual void splitTrack(WebScrollbar*, const WebRect& track, WebRect& startTrack, WebRect& thumb, WebRect& endTrack) OVERRIDE
    {
        startTrack = WebRect();
        thumb = WebRect();
        endTrack = WebRect();
    }
};

} // namespace WebKit

#endif // FakeWebScrollbarThemeGeometry_h
