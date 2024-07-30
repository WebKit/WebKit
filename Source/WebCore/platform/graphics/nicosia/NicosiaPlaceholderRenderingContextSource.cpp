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
#include "PlaceholderRenderingContext.h"

#include "GLFence.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "NicosiaContentLayer.h"
#include "NicosiaPlatformLayer.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"
#include <wtf/Lock.h>

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(SKIA)
#include "GLContext.h"
#include "PlatformDisplay.h"
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h> // NOLINT
IGNORE_CLANG_WARNINGS_END
#endif

#if USE(NICOSIA) && ENABLE(OFFSCREEN_CANVAS)

namespace Nicosia {

using namespace WebCore;

class NicosiaPlaceholderRenderingContextSourceDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<NicosiaPlaceholderRenderingContextSourceDisplayDelegate> create(ContentLayer& nicosiaLayer)
    {
        return adoptRef(*new NicosiaPlaceholderRenderingContextSourceDisplayDelegate(nicosiaLayer));
    }

    // GraphicsLayerContentsDisplayDelegate overrides.
    PlatformLayer* platformLayer() const final { return m_nicosiaLayer.ptr(); }
private:
    NicosiaPlaceholderRenderingContextSourceDisplayDelegate(ContentLayer& nicosiaLayer)
        : m_nicosiaLayer(nicosiaLayer)
    {
    }
    Ref<ContentLayer> m_nicosiaLayer;
};

class NicosiaPlaceholderRenderingContextSource final : public WebCore::PlaceholderRenderingContextSource, public ContentLayer::Client {
public:
    NicosiaPlaceholderRenderingContextSource(PlaceholderRenderingContext&);
    ~NicosiaPlaceholderRenderingContextSource();

    // PlaceholderRenderingContextSource overrides.
    void setPlaceholderBuffer(WebCore::ImageBuffer&) final;
    void setContentsToLayer(GraphicsLayer&) final;

    // ContentLayerTextureMapperImpl::Client overrides.
    void swapBuffersIfNeeded() final;

private:
    Ref<ContentLayer> m_nicosiaLayer;
    Ref<NicosiaPlaceholderRenderingContextSourceDisplayDelegate> m_layerContentsDisplayDelegate;
    RefPtr<WebCore::NativeImage> m_image WTF_GUARDED_BY_LOCK(m_imageLock);
    mutable Lock m_imageLock;
};

NicosiaPlaceholderRenderingContextSource::NicosiaPlaceholderRenderingContextSource(PlaceholderRenderingContext& context)
    : PlaceholderRenderingContextSource(context)
    , m_nicosiaLayer(Nicosia::ContentLayer::create(*this, adoptRef(*new TextureMapperPlatformLayerProxyGL(TextureMapperPlatformLayerProxy::ContentType::OffscreenCanvas))))
    , m_layerContentsDisplayDelegate(NicosiaPlaceholderRenderingContextSourceDisplayDelegate::create(m_nicosiaLayer))
{
}

NicosiaPlaceholderRenderingContextSource::~NicosiaPlaceholderRenderingContextSource()
{
    m_nicosiaLayer->invalidateClient();
}

void NicosiaPlaceholderRenderingContextSource::setPlaceholderBuffer(ImageBuffer& buffer)
{
    PlaceholderRenderingContextSource::setPlaceholderBuffer(buffer);
    auto nativeImage = ImageBuffer::sinkIntoNativeImage(buffer.clone());
    if (!nativeImage)
        return;

    Locker locker { m_imageLock };
    if (!m_image) {
#if PLATFORM(GTK) || PLATFORM(WPE)
        std::unique_ptr<GLFence> fence;
#endif // PLATFORM(GTK) || PLATFORM(WPE)
#if USE(SKIA)
        unsigned textureID = 0;
        auto image = nativeImage->platformImage();
        if (image->isTextureBacked()) {
            auto& display = PlatformDisplay::sharedDisplay();
            if (!display.skiaGLContext()->makeContextCurrent())
                return;

            auto* grContext = display.skiaGrContext();
            RELEASE_ASSERT(grContext);
            grContext->flushAndSubmit(GLFence::isSupported() ? GrSyncCpu::kNo : GrSyncCpu::kYes);

            GrBackendTexture backendTexture;
            if (SkImages::GetBackendTextureFromImage(image, &backendTexture, false)) {
                GrGLTextureInfo textureInfo;
                if (GrBackendTextures::GetGLTextureInfo(backendTexture, &textureInfo))
                    textureID = textureInfo.fID;
            }

            if (!textureID)
                return;

#if PLATFORM(GTK) || PLATFORM(WPE)
            fence = GLFence::create();
#endif // PLATFORM(GTK) || PLATFORM(WPE)
        }
#endif

        downcast<TextureMapperPlatformLayerProxyGL>(m_nicosiaLayer->proxy()).scheduleUpdateOnCompositorThread([this
#if USE(SKIA)
        , textureID
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
        , fence = WTFMove(fence)
#endif
        ] () mutable {
            auto& proxy = m_nicosiaLayer->proxy();
            Locker locker { proxy.lock() };
            if (!proxy.isActive())
                return;

            RefPtr<BitmapTexture> texture;
            {
                Locker locker { m_imageLock };
                if (!m_image)
                    return;

                auto nativeImage = WTFMove(m_image);

                auto size = nativeImage->size();
                OptionSet<BitmapTexture::Flags> flags;
                if (nativeImage->hasAlpha())
                    flags.add(BitmapTexture::Flags::SupportsAlpha);
                texture = BitmapTexture::create(size, flags);

#if USE(CAIRO)
                auto* surface = nativeImage->platformImage().get();
                auto* imageData = cairo_image_surface_get_data(surface);
                texture->updateContents(imageData, IntRect(IntPoint(), size), IntPoint(), cairo_image_surface_get_stride(surface));
#elif USE(SKIA)
                auto image = nativeImage->platformImage();
                if (image->isTextureBacked()) {
#if PLATFORM(GTK) || PLATFORM(WPE)
                    fence->serverWait();
#endif // PLATFORM(GTK) || PLATFORM(WPE)
                    texture->copyFromExternalTexture(textureID);
#if PLATFORM(GTK) || PLATFORM(WPE)
                    fence = GLFence::create();
#endif // PLATFORM(GTK) || PLATFORM(WPE)
                } else {
                    SkPixmap pixmap;
                    if (image->peekPixels(&pixmap))
                        texture->updateContents(pixmap.addr(), IntRect(IntPoint(), size), IntPoint(), image->imageInfo().minRowBytes());
                }
#endif
            }

            auto layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture));
            layerBuffer->setExtraFlags(TextureMapperFlags::ShouldBlend);
#if PLATFORM(GTK) || PLATFORM(WPE)
            layerBuffer->setFence(WTFMove(fence));
#endif
            downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));

        });
    }
    m_image = WTFMove(nativeImage);
}

void NicosiaPlaceholderRenderingContextSource::swapBuffersIfNeeded()
{
}

void NicosiaPlaceholderRenderingContextSource::setContentsToLayer(GraphicsLayer& layer)
{
    layer.setContentsDisplayDelegate(m_layerContentsDisplayDelegate.ptr(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

} // namespace Nicosia

namespace WebCore {

Ref<PlaceholderRenderingContextSource> PlaceholderRenderingContextSource::create(PlaceholderRenderingContext& context)
{
    return adoptRef(*new Nicosia::NicosiaPlaceholderRenderingContextSource(context));
}

}

#endif // USE(NICOSIA) && ENABLE(OFFSCREEN_CANVAS)
