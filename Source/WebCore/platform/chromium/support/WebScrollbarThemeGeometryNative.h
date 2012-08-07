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

#ifndef WebScrollbarThemeGeometryNative_h
#define WebScrollbarThemeGeometryNative_h

#include <public/WebRect.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class ScrollbarThemeComposite;
}

namespace WebKit {

class WebScrollbar;

class WebScrollbarThemeGeometryNative : public WebScrollbarThemeGeometry {
public:
    static PassOwnPtr<WebScrollbarThemeGeometryNative> create(WebCore::ScrollbarThemeComposite*);

    // WebScrollbarThemeGeometry overrides
    virtual WebScrollbarThemeGeometryNative* clone() const OVERRIDE;
    virtual int thumbPosition(WebScrollbar*) OVERRIDE;
    virtual int thumbLength(WebScrollbar*) OVERRIDE;
    virtual int trackPosition(WebScrollbar*) OVERRIDE;
    virtual int trackLength(WebScrollbar*) OVERRIDE;
    virtual bool hasButtons(WebScrollbar*) OVERRIDE;
    virtual bool hasThumb(WebScrollbar*) OVERRIDE;
    virtual WebRect trackRect(WebScrollbar*) OVERRIDE;
    virtual WebRect thumbRect(WebScrollbar*) OVERRIDE;
    virtual int minimumThumbLength(WebScrollbar*) OVERRIDE;
    virtual int scrollbarThickness(WebScrollbar*) OVERRIDE;
    virtual WebRect backButtonStartRect(WebScrollbar*) OVERRIDE;
    virtual WebRect backButtonEndRect(WebScrollbar*) OVERRIDE;
    virtual WebRect forwardButtonStartRect(WebScrollbar*) OVERRIDE;
    virtual WebRect forwardButtonEndRect(WebScrollbar*) OVERRIDE;
    virtual WebRect constrainTrackRectToTrackPieces(WebScrollbar*, const WebRect&) OVERRIDE;
    virtual void splitTrack(WebScrollbar*, const WebRect& track, WebRect& startTrack, WebRect& thumb, WebRect& endTrack) OVERRIDE;

private:
    explicit WebScrollbarThemeGeometryNative(WebCore::ScrollbarThemeComposite*);

    // The theme is not owned by this class. It is assumed that the theme is a
    // static pointer and its lifetime is essentially infinite. Only thread-safe
    // functions on the theme can be called by this theme.
    WebCore::ScrollbarThemeComposite* m_theme;
};

} // namespace WebKit

#endif
