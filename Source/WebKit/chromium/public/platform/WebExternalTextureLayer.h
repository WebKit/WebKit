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

#ifndef WebExternalTextureLayer_h
#define WebExternalTextureLayer_h

#include "WebCommon.h"
#include "WebFloatRect.h"
#include "WebLayer.h"

namespace WebKit {
class WebExternalTextureLayerImpl;
class WebLayerClient;

// This class represents a layer that renders a texture that is generated
// externally (not managed by the WebLayerTreeView).
// The texture will be used by the WebLayerTreeView during compositing passes.
// When in single-thread mode, this means during WebLayerTreeView::composite().
// When using the threaded compositor, this can mean at an arbitrary time until
// the WebLayerTreeView is destroyed.
class WebExternalTextureLayer : public WebLayer {
public:
    WEBKIT_EXPORT static WebExternalTextureLayer create(WebLayerClient*);

    WebExternalTextureLayer() { }
    WebExternalTextureLayer(const WebExternalTextureLayer& layer) : WebLayer(layer) { }
    virtual ~WebExternalTextureLayer() { }
    WebExternalTextureLayer& operator=(const WebExternalTextureLayer& layer)
    {
        WebLayer::assign(layer);
        return *this;
    }

    // Sets the texture id that represents the layer, in the namespace of the
    // compositor context.
    WEBKIT_EXPORT void setTextureId(unsigned);
    WEBKIT_EXPORT unsigned textureId() const;

    // Sets whether or not the texture should be flipped in the Y direction when
    // rendered.
    WEBKIT_EXPORT void setFlipped(bool);
    WEBKIT_EXPORT bool flipped() const;

    // Sets the rect in UV space of the texture that is mapped to the layer
    // bounds.
    WEBKIT_EXPORT void setUVRect(const WebFloatRect&);
    WEBKIT_EXPORT WebFloatRect uvRect() const;

    // Marks a region of the layer as needing a display. These regions are
    // collected in a union until the display occurs.
    WEBKIT_EXPORT void invalidateRect(const WebFloatRect&);

#if WEBKIT_IMPLEMENTATION
    WebExternalTextureLayer(const WTF::PassRefPtr<WebExternalTextureLayerImpl>&);
    WebExternalTextureLayer& operator=(const WTF::PassRefPtr<WebExternalTextureLayerImpl>&);
    operator WTF::PassRefPtr<WebExternalTextureLayerImpl>() const;
#endif
};

} // namespace WebKit

#endif // WebExternalTextureLayer_h
