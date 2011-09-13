/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef VideoLayerChromium_h
#define VideoLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "ManagedTexture.h"
#include "VideoFrameProvider.h"

namespace WebCore {

// A Layer that contains a Video element.
class VideoLayerChromium : public LayerChromium {
public:

    static PassRefPtr<VideoLayerChromium> create(GraphicsLayerChromium* owner = 0,
                                                 VideoFrameProvider* = 0);
    virtual ~VideoLayerChromium();

    virtual PassRefPtr<CCLayerImpl> createCCLayerImpl();

    virtual void updateCompositorResources(GraphicsContext3D*);
    virtual bool drawsContent() const { return true; }

    // This function is called by VideoFrameProvider. When this method is called
    // putCurrentFrame() must be called to return the frame currently held.
    void releaseCurrentFrame();

    virtual void pushPropertiesTo(CCLayerImpl*);

    virtual void setLayerTreeHost(CCLayerTreeHost*);

protected:
    virtual void cleanupResources();
    virtual const char* layerTypeAsString() const { return "VideoLayer"; }

private:
    struct Texture {
        IntSize m_visibleSize;
        OwnPtr<ManagedTexture> m_texture;
    };

    VideoLayerChromium(GraphicsLayerChromium* owner, VideoFrameProvider*);

    static GC3Denum determineTextureFormat(const VideoFrameChromium*);
    static IntSize computeVisibleSize(const VideoFrameChromium*, unsigned plane);
    bool reserveTextures(const VideoFrameChromium*, GC3Denum textureFormat);
    void updateTexture(GraphicsContext3D*, Texture&, const void*) const;

    void resetFrameParameters();

    bool m_skipsDraw;
    VideoFrameChromium::Format m_frameFormat;
    VideoFrameProvider* m_provider;

    Texture m_textures[3];

    // This will be null for the entire duration of video playback if hardware
    // decoding is not being used.
    VideoFrameChromium* m_currentFrame;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
