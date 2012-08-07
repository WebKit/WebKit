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

#ifndef WebScrollbarLayer_h
#define WebScrollbarLayer_h

#include "WebLayer.h"
#include "WebScrollbarThemeGeometry.h"
#include "WebScrollbarThemePainter.h"

namespace WebCore {
class Scrollbar;
class ScrollbarLayerChromium;
}

namespace WebKit {

class WebScrollbarLayer : public WebLayer {
public:
    WebScrollbarLayer() { }
    WebScrollbarLayer(const WebScrollbarLayer& layer) : WebLayer(layer) { }
    virtual ~WebScrollbarLayer() { }
    WebScrollbarLayer& operator=(const WebScrollbarLayer& layer)
    {
        WebLayer::assign(layer);
        return *this;
    }

    WEBKIT_EXPORT void setScrollLayer(const WebLayer);

#if WEBKIT_IMPLEMENTATION
    static WebScrollbarLayer create(WebCore::Scrollbar*, WebScrollbarThemePainter, PassOwnPtr<WebScrollbarThemeGeometry>);
    explicit WebScrollbarLayer(const WTF::PassRefPtr<WebCore::ScrollbarLayerChromium>&);
    WebScrollbarLayer& operator=(const WTF::PassRefPtr<WebCore::ScrollbarLayerChromium>&);
    operator WTF::PassRefPtr<WebCore::ScrollbarLayerChromium>() const;
#endif
};

} // namespace WebKit

#endif // WebScrollbarLayer_h
