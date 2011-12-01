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

#include "platform/WebCommon.h"
#include "WebLayer.h"

namespace WebKit {
class WebContentLayerClient;
class WebContentLayerImpl;
struct WebFloatRect;
class WebLayerClient;

class WebContentLayer : public WebLayer {
public:
    WEBKIT_EXPORT static WebContentLayer create(WebLayerClient*, WebContentLayerClient*);

    WebContentLayer() { }
    WebContentLayer(const WebContentLayer& layer) : WebLayer(layer) { }
    virtual ~WebContentLayer() { }
    WebContentLayer& operator=(const WebContentLayer& layer)
    {
        WebLayer::assign(layer);
        return *this;
    }

    // Sets whether the layer draws its content when compositing.
    WEBKIT_EXPORT void setDrawsContent(bool);
    WEBKIT_EXPORT bool drawsContent() const;

    // Sets a region of the layer as invalid, i.e. needs to update its content.
    // The visible area of the dirty rect will be passed to one or more calls to
    // WebContentLayerClient::paintContents before the compositing pass occurs.
    WEBKIT_EXPORT void invalidateRect(const WebFloatRect&);

    // Sets the entire layer as invalid, i.e. needs to update its content.
    WEBKIT_EXPORT void invalidate();

#if WEBKIT_IMPLEMENTATION
    WebContentLayer(const WTF::PassRefPtr<WebContentLayerImpl>&);
    WebContentLayer& operator=(const WTF::PassRefPtr<WebContentLayerImpl>&);
    operator WTF::PassRefPtr<WebContentLayerImpl>() const;
#endif
};

} // namespace WebKit

#endif // WebContentLayer_h
