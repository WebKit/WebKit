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
#include "NicosiaImageBufferPipe.h"

#include "ImageBuffer.h"
#include "NativeImage.h"
#include "NicosiaPlatformLayer.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(NICOSIA)

namespace Nicosia {

using namespace WebCore;

NicosiaImageBufferPipeSource::NicosiaImageBufferPipeSource()
{
    m_nicosiaLayer = Nicosia::ContentLayer::create(*this);
}

NicosiaImageBufferPipeSource::~NicosiaImageBufferPipeSource()
{
    m_nicosiaLayer->invalidateClient();
}

void NicosiaImageBufferPipeSource::handle(ImageBuffer& buffer)
{
    auto clone = buffer.clone();
    if (!clone)
        return;

    Locker locker { m_imageBufferLock };

    if (!m_imageBuffer) {
        auto proxyOperation = [this] (TextureMapperPlatformLayerProxy& proxy) mutable {
            return downcast<TextureMapperPlatformLayerProxyGL>(proxy).scheduleUpdateOnCompositorThread([this] () mutable {
                auto& proxy = m_nicosiaLayer->proxy();
                Locker locker { proxy.lock() };

                if (!proxy.isActive())
                    return;

                RefPtr<BitmapTexture> texture;
                {
                    Locker locker { m_imageBufferLock };

                    if (!m_imageBuffer)
                        return;

                    auto nativeImage = ImageBuffer::sinkIntoNativeImage(WTFMove(m_imageBuffer));
                    if (!nativeImage)
                        return;

                    auto size = nativeImage->size();
                    OptionSet<BitmapTexture::Flags> flags;
                    if (nativeImage->hasAlpha())
                        flags.add(BitmapTexture::Flags::SupportsAlpha);
                    texture = BitmapTexture::create(size, flags);
#if USE(CAIRO)
                    auto* surface = nativeImage->platformImage().get();
                    auto* imageData = cairo_image_surface_get_data(surface);
                    texture->updateContents(imageData, IntRect(IntPoint(), size), IntPoint(), cairo_image_surface_get_stride(surface));
#else
                    notImplemented();
#endif
                }

                auto layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture));
                layerBuffer->setExtraFlags(TextureMapperFlags::ShouldBlend);
                downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
            });
        };
        proxyOperation(m_nicosiaLayer->proxy());
    }

    m_imageBuffer = WTFMove(clone);
}

void NicosiaImageBufferPipeSource::swapBuffersIfNeeded()
{
}

NicosiaImageBufferPipe::NicosiaImageBufferPipe()
    : m_source(adoptRef(*new NicosiaImageBufferPipeSource))
    , m_layerContentsDisplayDelegate(NicosiaImageBufferPipeSourceDisplayDelegate::create(m_source->platformLayer()))
{
}

RefPtr<ImageBufferPipe::Source> NicosiaImageBufferPipe::source() const
{
    return m_source.ptr();
}

void NicosiaImageBufferPipe::setContentsToLayer(GraphicsLayer& layer)
{
    layer.setContentsDisplayDelegate(m_layerContentsDisplayDelegate.ptr(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

} // namespace Nicosia

namespace WebCore {

RefPtr<ImageBufferPipe> ImageBufferPipe::create()
{
    return adoptRef(new Nicosia::NicosiaImageBufferPipe);
}

}

#endif // USE(NICOSIA)
