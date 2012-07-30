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

#ifndef WebScrollbarThemeGeometry_h
#define WebScrollbarThemeGeometry_h

#include "WebRect.h"

namespace WebCore {
class ScrollbarThemeComposite;
};

namespace WebKit {

class WebScrollbar;

class WebScrollbarThemeGeometry {
public:
    WebScrollbarThemeGeometry() : m_theme(0) { }

    WebScrollbarThemeGeometry(const WebScrollbarThemeGeometry& geometry) { assign(geometry); }
    virtual ~WebScrollbarThemeGeometry() { }
    WebScrollbarThemeGeometry& operator=(const WebScrollbarThemeGeometry& geometry)
    {
        assign(geometry);
        return *this;
    }

    bool isNull() const { return m_theme; }
    WEBKIT_EXPORT void assign(const WebScrollbarThemeGeometry&);

    WEBKIT_EXPORT int thumbPosition(WebScrollbar*);
    WEBKIT_EXPORT int thumbLength(WebScrollbar*);
    WEBKIT_EXPORT int trackPosition(WebScrollbar*);
    WEBKIT_EXPORT int trackLength(WebScrollbar*);
    WEBKIT_EXPORT bool hasButtons(WebScrollbar*);
    WEBKIT_EXPORT bool hasThumb(WebScrollbar*);
    WEBKIT_EXPORT WebRect trackRect(WebScrollbar*);
    WEBKIT_EXPORT WebRect thumbRect(WebScrollbar*);
    WEBKIT_EXPORT int minimumThumbLength(WebScrollbar*);
    WEBKIT_EXPORT int scrollbarThickness(WebScrollbar*);
    WEBKIT_EXPORT WebRect backButtonStartRect(WebScrollbar*);
    WEBKIT_EXPORT WebRect backButtonEndRect(WebScrollbar*);
    WEBKIT_EXPORT WebRect forwardButtonStartRect(WebScrollbar*);
    WEBKIT_EXPORT WebRect forwardButtonEndRect(WebScrollbar*);
    WEBKIT_EXPORT WebRect constrainTrackRectToTrackPieces(WebScrollbar*, const WebRect&);
    WEBKIT_EXPORT void splitTrack(WebScrollbar*, const WebRect& track, WebRect& startTrack, WebRect& thumb, WebRect& endTrack);

#if WEBKIT_IMPLEMENTATION
    explicit WebScrollbarThemeGeometry(WebCore::ScrollbarThemeComposite*);
#endif

private:
    // The theme is not owned by this class. It is assumed that the theme is a
    // static pointer and its lifetime is essentially infinite. Only thread-safe
    // functions on the theme can be called by this theme.
    WebCore::ScrollbarThemeComposite* m_theme;
};

} // namespace WebKit

#endif
