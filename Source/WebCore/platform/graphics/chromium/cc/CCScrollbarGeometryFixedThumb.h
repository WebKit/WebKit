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

#ifndef CCScrollbarGeometryFixedThumb_h
#define CCScrollbarGeometryFixedThumb_h

#include "CCScrollbarGeometryStub.h"

namespace WebCore {

// This scrollbar geometry class behaves exactly like a normal geometry except
// it always returns a fixed thumb length. This allows a page to zoom (changing
// the total size of the scrollable area, changing the thumb length) while not
// requiring the thumb resource to be repainted.
class CCScrollbarGeometryFixedThumb : public CCScrollbarGeometryStub {
public:
    static PassOwnPtr<CCScrollbarGeometryFixedThumb> create(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);
    virtual ~CCScrollbarGeometryFixedThumb();

    // Update thumb length from scrollbar
    void update(WebKit::WebScrollbar*) OVERRIDE;

    // WebScrollbarThemeGeometry interface
    virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE;
    virtual int thumbLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual int thumbPosition(WebKit::WebScrollbar*) OVERRIDE;
    virtual void splitTrack(WebKit::WebScrollbar*, const WebKit::WebRect& track, WebKit::WebRect& startTrack, WebKit::WebRect& thumb, WebKit::WebRect& endTrack) OVERRIDE;

private:
    explicit CCScrollbarGeometryFixedThumb(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);

    IntSize m_thumbSize;
};

}

#endif
