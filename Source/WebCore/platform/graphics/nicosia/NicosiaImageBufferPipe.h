/*
 * Copyright (C) 2020,2022 Igalia S.L
 * Copyright (C) 2020,2022 Metrological Group B.V.
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

#pragma once

#include "GraphicsLayerContentsDisplayDelegate.h"
#include "ImageBufferPipe.h"
#include "NicosiaContentLayer.h"

namespace Nicosia {

class NicosiaImageBufferPipeSourceDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<NicosiaImageBufferPipeSourceDisplayDelegate> create(RefPtr<ContentLayer>&& nicosiaLayer)
    {
        return adoptRef(*new NicosiaImageBufferPipeSourceDisplayDelegate(WTFMove(nicosiaLayer)));
    }

    // GraphicsLayerContentsDisplayDelegate overrides.
    PlatformLayer* platformLayer() const final { return m_nicosiaLayer.get(); }
private:
    NicosiaImageBufferPipeSourceDisplayDelegate(RefPtr<ContentLayer>&& nicosiaLayer)
        : m_nicosiaLayer(WTFMove(nicosiaLayer))
    {
    }

    RefPtr<ContentLayer> m_nicosiaLayer;
};

class NicosiaImageBufferPipeSource : public WebCore::ImageBufferPipe::Source, public ContentLayer::Client {
public:
    NicosiaImageBufferPipeSource();
    virtual ~NicosiaImageBufferPipeSource();

    // ImageBufferPipe::Source overrides.
    void handle(WebCore::ImageBuffer&) final;

    // ContentLayerTextureMapperImpl::Client overrides.
    void swapBuffersIfNeeded() override;

    ContentLayer* platformLayer() const { return m_nicosiaLayer.get(); }
private:
    RefPtr<ContentLayer> m_nicosiaLayer;

    mutable Lock m_imageBufferLock;
    RefPtr<WebCore::ImageBuffer> m_imageBuffer;
};

class NicosiaImageBufferPipe final : public WebCore::ImageBufferPipe {
public:
    NicosiaImageBufferPipe();

    // ImageBufferPipe overrides.
    RefPtr<WebCore::ImageBufferPipe::Source> source() const final;
    void setContentsToLayer(WebCore::GraphicsLayer&) final;
private:
    Ref<NicosiaImageBufferPipeSource> m_source;
    Ref<NicosiaImageBufferPipeSourceDisplayDelegate> m_layerContentsDisplayDelegate;
};

}
