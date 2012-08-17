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


#ifndef LayerTextureUpdater_h
#define LayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)

#include "CCPrioritizedTexture.h"
#include "GraphicsTypes3D.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class IntRect;
class IntSize;
class TextureManager;
struct CCRenderingStats;

class LayerTextureUpdater : public RefCounted<LayerTextureUpdater> {
public:
    // Allows texture uploaders to store per-tile resources.
    class Texture {
    public:
        virtual ~Texture() { }

        CCPrioritizedTexture* texture() { return m_texture.get(); }
        void swapTextureWith(OwnPtr<CCPrioritizedTexture>& texture) { m_texture.swap(texture); }
        virtual void prepareRect(const IntRect& /* sourceRect */, CCRenderingStats&) { }
        virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) = 0;
    protected:
        explicit Texture(PassOwnPtr<CCPrioritizedTexture> texture) : m_texture(texture) { }

    private:
        OwnPtr<CCPrioritizedTexture> m_texture;
    };

    virtual ~LayerTextureUpdater() { }

    enum SampledTexelFormat {
        SampledTexelFormatRGBA,
        SampledTexelFormatBGRA,
        SampledTexelFormatInvalid,
    };
    virtual PassOwnPtr<Texture> createTexture(CCPrioritizedTextureManager*) = 0;
    // Returns the format of the texel uploaded by this interface.
    // This format should not be confused by texture internal format.
    // This format specifies the component order in the sampled texel.
    // If the format is TexelFormatBGRA, vec4.x is blue and vec4.z is red.
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) = 0;
    // The |resultingOpaqueRect| gives back a region of the layer that was painted opaque. If the layer is marked opaque in the updater,
    // then this region should be ignored in preference for the entire layer's area.
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats&) { }

    // Set true by the layer when it is known that the entire output is going to be opaque.
    virtual void setOpaque(bool) { }
};

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerTextureUpdater_h
