/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebContentLayer_h
#define WebContentLayer_h

#include "WebCommon.h"
#include "WebScrollableLayer.h"

namespace WebCore {
class ContentLayerChromium;
}

namespace WebKit {
class WebContentLayerClient;
class WebContentLayerImpl;

class WebContentLayer : public WebScrollableLayer {
public:
    WEBKIT_EXPORT static WebContentLayer create(WebContentLayerClient*);

    WebContentLayer() { }
    WebContentLayer(const WebContentLayer& layer) : WebScrollableLayer(layer) { }
    virtual ~WebContentLayer() { }
    WebContentLayer& operator=(const WebContentLayer& layer)
    {
        WebLayer::assign(layer);
        return *this;
    }

    // Called when the WebContentLayerClient is going away and should not be used.
    WEBKIT_EXPORT void clearClient();

    // Set to true if the backside of this layer's contents should be visible when composited.
    // Defaults to false.
    WEBKIT_EXPORT void setDoubleSided(bool);

    // Set to apply a scale factor used when painting and drawing this layer's content. Defaults to 1.0.
    WEBKIT_EXPORT void setContentsScale(float);

    // Set to render text in this layer with LCD antialiasing. Only set if you know that this layer will be
    // drawn in a way where this makes sense - i.e. opaque background, not rotated or scaled, etc.
    // Defaults to false;
    WEBKIT_EXPORT void setUseLCDText(bool);

    // Set to draw a system-defined checkerboard if the compositor would otherwise draw a tile in this layer
    // and the actual contents are unavailable. If false, the compositor will draw the layer's background color
    // for these tiles.
    // Defaults to false.
    WEBKIT_EXPORT void setDrawCheckerboardForMissingTiles(bool);

#if WEBKIT_IMPLEMENTATION
    WebContentLayer(const WTF::PassRefPtr<WebCore::ContentLayerChromium>&);
    WebContentLayer& operator=(const WTF::PassRefPtr<WebCore::ContentLayerChromium>&);
    operator WTF::PassRefPtr<WebCore::ContentLayerChromium>() const;
#endif
};

} // namespace WebKit

#endif // WebContentLayer_h
