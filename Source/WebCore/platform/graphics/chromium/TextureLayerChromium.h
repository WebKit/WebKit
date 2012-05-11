/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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


#ifndef TextureLayerChromium_h
#define TextureLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"

namespace WebCore {

class TextureLayerChromiumClient {
public:
    // Called to prepare this layer's texture for compositing. The client may queue a texture
    // upload or copy on the CCTextureUpdater.
    // Returns the texture ID to be used for compositing.
    virtual unsigned prepareTexture(CCTextureUpdater&) = 0;

    // Returns the context that is providing the texture. Used for rate limiting and detecting lost context.
    virtual GraphicsContext3D* context() = 0;

protected:
    virtual ~TextureLayerChromiumClient() { }
};

// A Layer containing a the rendered output of a plugin instance.
class TextureLayerChromium : public LayerChromium {
public:
    // If this texture layer requires special preparation logic for each frame driven by
    // the compositor, pass in a non-nil client. Pass in a nil client pointer if texture updates
    // are driven by an external process.
    static PassRefPtr<TextureLayerChromium> create(TextureLayerChromiumClient*);
    virtual ~TextureLayerChromium();

    void clearClient() { m_client = 0; }

    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

    // Sets whether this texture should be Y-flipped at draw time. Defaults to true.
    void setFlipped(bool);

    // Sets a UV transform to be used at draw time. Defaults to (0, 0, 1, 1).
    void setUVRect(const FloatRect&);

    // Sets whether the alpha channel is premultiplied or unpremultiplied. Defaults to true.
    void setPremultipliedAlpha(bool);

    // Sets whether this context should rate limit on damage to prevent too many frames from
    // being queued up before the compositor gets a chance to run. Requires a non-nil client.
    // Defaults to false.
    void setRateLimitContext(bool);

    // Code path for plugins which supply their own texture ID.
    void setTextureId(unsigned);

    virtual void setNeedsDisplayRect(const FloatRect&) OVERRIDE;

    virtual void setLayerTreeHost(CCLayerTreeHost*) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void update(CCTextureUpdater&, const CCOcclusionTracker*) OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

protected:
    explicit TextureLayerChromium(TextureLayerChromiumClient*);

private:
    TextureLayerChromiumClient* m_client;

    bool m_flipped;
    FloatRect m_uvRect;
    bool m_premultipliedAlpha;
    bool m_rateLimitContext;
    bool m_contextLost;

    unsigned m_textureId;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
