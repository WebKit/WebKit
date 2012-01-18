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

#ifndef CCVideoLayerImpl_h
#define CCVideoLayerImpl_h

#include "ManagedTexture.h"
#include "ShaderChromium.h"
#include "VideoFrameChromium.h"
#include "VideoFrameProvider.h"
#include "VideoLayerChromium.h"
#include "cc/CCLayerImpl.h"

namespace WebCore {

template<class VertexShader, class FragmentShader> class ProgramBinding;

class CCVideoLayerImpl : public CCLayerImpl
                       , public VideoFrameProvider::Client {
public:
    static PassRefPtr<CCVideoLayerImpl> create(int id, VideoFrameProvider* provider)
    {
        return adoptRef(new CCVideoLayerImpl(id, provider));
    }
    virtual ~CCVideoLayerImpl();

    virtual void appendQuads(CCQuadList&, const CCSharedQuadState*);

    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexFlipAlpha> RGBAProgram;
    typedef ProgramBinding<VertexShaderPosTexYUVStretch, FragmentShaderYUVVideo> YUVProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexFlipAlpha> NativeTextureProgram;

    virtual void draw(LayerRendererChromium*);

    virtual void dumpLayerProperties(TextStream&, int indent) const;

    // VideoFrameProvider::Client implementation (callable on any thread).
    virtual void stopUsingProvider();

private:
    explicit CCVideoLayerImpl(int, VideoFrameProvider*);

    virtual const char* layerTypeAsString() const { return "VideoLayer"; }

    bool copyFrameToTextures(const VideoFrameChromium*, GC3Denum format, LayerRendererChromium*);
    void copyPlaneToTexture(LayerRendererChromium*, const void* plane, int index);
    bool reserveTextures(const VideoFrameChromium*, GC3Denum format, LayerRendererChromium*);
    void drawYUV(LayerRendererChromium*) const;
    void drawRGBA(LayerRendererChromium*) const;
    void drawNativeTexture(LayerRendererChromium*) const;
    template<class Program> void drawCommon(LayerRendererChromium*, Program*, float widthScaleFactor, Platform3DObject textureId) const;

    Mutex m_providerMutex; // Guards m_provider below.
    VideoFrameProvider* m_provider;

    static const float yuv2RGB[9];
    static const float yuvAdjust[3];

    struct Texture {
        OwnPtr<ManagedTexture> m_texture;
        IntSize m_visibleSize;
    };
    enum { MaxPlanes = 3 };
    Texture m_textures[MaxPlanes];
    int m_planes;

    Platform3DObject m_nativeTextureId;
    IntSize m_nativeTextureSize;
};

}

#endif // CCVideoLayerImpl_h
