/*
 * Copyright (C) 2020 Igalia S.L
 * Copyright (C) 2020 Metrological Group B.V.
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

#include "config.h"
#include "ImageBuffer.h"

#include "ImageBufferPipe.h"
#include "NicosiaContentLayerTextureMapperImpl.h"
#include "NicosiaPlatformLayer.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(NICOSIA)

namespace Nicosia {

using namespace WebCore;

class NicosiaImageBufferPipeSource final : public ImageBufferPipe::Source, public ContentLayerTextureMapperImpl::Client {
public:
    NicosiaImageBufferPipeSource();
    virtual ~NicosiaImageBufferPipeSource();

    void handle(std::unique_ptr<ImageBuffer>&&) final;

    PlatformLayer* platformLayer() const;

private:
    void swapBuffersIfNeeded() override;

    RefPtr<ContentLayer> m_nicosiaLayer;

    mutable Lock m_imageBufferLock;
    std::unique_ptr<ImageBuffer> m_imageBuffer;
};

class NicosiaImageBufferPipe final : public ImageBufferPipe {
public:
    NicosiaImageBufferPipe();
    virtual ~NicosiaImageBufferPipe() = default;

private:
    RefPtr<ImageBufferPipe::Source> source() const final;
    PlatformLayer* platformLayer() const final;

    RefPtr<NicosiaImageBufferPipeSource> m_source;
};

NicosiaImageBufferPipeSource::NicosiaImageBufferPipeSource()
{
    m_nicosiaLayer = Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this));
}

NicosiaImageBufferPipeSource::~NicosiaImageBufferPipeSource()
{
    downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).invalidateClient();
}

void NicosiaImageBufferPipeSource::handle(std::unique_ptr<ImageBuffer>&& buffer)
{
    if (!buffer)
        return;

    auto locker = holdLock(m_imageBufferLock);

    if (!m_imageBuffer) {
        auto proxyOperation = [this] (TextureMapperPlatformLayerProxy& proxy) mutable {
            return proxy.scheduleUpdateOnCompositorThread([this] () mutable {
                auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).proxy();
                LockHolder holder(proxy.lock());

                if (!proxy.isActive())
                    return;

                auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get());

                {
                    auto locker = holdLock(m_imageBufferLock);

                    if (!m_imageBuffer)
                        return;

                    auto nativeImage = ImageBuffer::sinkIntoNativeImage(WTFMove(m_imageBuffer));
                    auto size = nativeImageSize(nativeImage);

                    texture->reset(size, nativeImageHasAlpha(nativeImage) ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag);
#if USE(CAIRO)
                    auto* surface = nativeImage.get();
                    auto* imageData = reinterpret_cast<const char*>(cairo_image_surface_get_data(surface));
                    texture->updateContents(imageData, IntRect(IntPoint(), size), IntPoint(), cairo_image_surface_get_stride(surface));
#else
                    notImplemented();
#endif
                }

                auto layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture));
                layerBuffer->setExtraFlags(TextureMapperGL::ShouldBlend);
                proxy.pushNextBuffer(WTFMove(layerBuffer));
            });
        };
        proxyOperation(downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).proxy());
    }

    m_imageBuffer = WTFMove(buffer);
}

PlatformLayer* NicosiaImageBufferPipeSource::platformLayer() const
{
    return m_nicosiaLayer.get();
}

void NicosiaImageBufferPipeSource::swapBuffersIfNeeded()
{
}

NicosiaImageBufferPipe::NicosiaImageBufferPipe()
{
    m_source = adoptRef(new NicosiaImageBufferPipeSource);
}

RefPtr<ImageBufferPipe::Source> NicosiaImageBufferPipe::source() const
{
    return m_source;
}

PlatformLayer* NicosiaImageBufferPipe::platformLayer() const
{
    if (m_source)
        return m_source->platformLayer();
    return nullptr;
}

} // namespace Nicosia

namespace WebCore {

RefPtr<ImageBufferPipe> ImageBufferPipe::create()
{
    return adoptRef(new Nicosia::NicosiaImageBufferPipe);
}

}

#endif // USE(NICOSIA)
