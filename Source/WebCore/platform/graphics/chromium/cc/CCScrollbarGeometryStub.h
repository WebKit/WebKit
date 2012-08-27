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

#ifndef CCScrollbarGeometryStub_h
#define CCScrollbarGeometryStub_h

#include <public/WebScrollbarThemeGeometry.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

// This subclass wraps an existing scrollbar geometry class so that
// another class can derive from it and override specific functions, while
// passing through the remaining ones.
class CCScrollbarGeometryStub : public WebKit::WebScrollbarThemeGeometry {
public:
    virtual ~CCScrollbarGeometryStub();

    // Allow derived classes to update themselves from a scrollbar.
    void update(WebKit::WebScrollbar*) { }

    // WebScrollbarThemeGeometry interface
    virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE;
    virtual int thumbPosition(WebKit::WebScrollbar*) OVERRIDE;
    virtual int thumbLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual int trackPosition(WebKit::WebScrollbar*) OVERRIDE;
    virtual int trackLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual bool hasButtons(WebKit::WebScrollbar*) OVERRIDE;
    virtual bool hasThumb(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect trackRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect thumbRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual int minimumThumbLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual int scrollbarThickness(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect backButtonStartRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect backButtonEndRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect forwardButtonStartRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect forwardButtonEndRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect constrainTrackRectToTrackPieces(WebKit::WebScrollbar*, const WebKit::WebRect&) OVERRIDE;
    virtual void splitTrack(WebKit::WebScrollbar*, const WebKit::WebRect& track, WebKit::WebRect& startTrack, WebKit::WebRect& thumb, WebKit::WebRect& endTrack) OVERRIDE;

protected:
    explicit CCScrollbarGeometryStub(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);

private:
    OwnPtr<WebKit::WebScrollbarThemeGeometry> m_geometry;
};

}

#endif
