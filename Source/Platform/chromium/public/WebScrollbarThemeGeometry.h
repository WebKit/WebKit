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

namespace WebKit {

class WebScrollbar;

class WebScrollbarThemeGeometry {
public:
    virtual ~WebScrollbarThemeGeometry() { }

    virtual WebScrollbarThemeGeometry* clone() const = 0;
    virtual int thumbPosition(WebScrollbar*) = 0;
    virtual int thumbLength(WebScrollbar*) = 0;
    virtual int trackPosition(WebScrollbar*) = 0;
    virtual int trackLength(WebScrollbar*) = 0;
    virtual bool hasButtons(WebScrollbar*) = 0;
    virtual bool hasThumb(WebScrollbar*) = 0;
    virtual WebRect trackRect(WebScrollbar*) = 0;
    virtual WebRect thumbRect(WebScrollbar*) = 0;
    virtual int minimumThumbLength(WebScrollbar*) = 0;
    virtual int scrollbarThickness(WebScrollbar*) = 0;
    virtual WebRect backButtonStartRect(WebScrollbar*) = 0;
    virtual WebRect backButtonEndRect(WebScrollbar*) = 0;
    virtual WebRect forwardButtonStartRect(WebScrollbar*) = 0;
    virtual WebRect forwardButtonEndRect(WebScrollbar*) = 0;
    virtual WebRect constrainTrackRectToTrackPieces(WebScrollbar*, const WebRect&) = 0;
    virtual void splitTrack(WebScrollbar*, const WebRect& track, WebRect& startTrack, WebRect& thumb, WebRect& endTrack) = 0;
};

} // namespace WebKit

#endif
