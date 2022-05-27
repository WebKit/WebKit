/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextureMapperPlatformLayerProxyDMABuf.h"

#if USE(COORDINATED_GRAPHICS) && USE(TEXTURE_MAPPER_DMABUF)

#include "PlatformDisplay.h"
#include "TextureMapperGL.h"
#include "TextureMapperGLHeaders.h"
#include "TextureMapperLayer.h"
#include <fcntl.h>
#include <initializer_list>
#include <unistd.h>

#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

// This has to be included after the EGL headers.
#include "DMABufEGLUtilities.h"

namespace WebCore {

static PFNEGLCREATEIMAGEKHRPROC createImageKHR()
{
    static PFNEGLCREATEIMAGEKHRPROC s_createImageKHR;
    static std::once_flag s_flag;
    std::call_once(s_flag,
        [&] {
            s_createImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
        });
    return s_createImageKHR;
}

static PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR()
{
    static PFNEGLDESTROYIMAGEKHRPROC s_destroyImageKHR;
    static std::once_flag s_flag;
    std::call_once(s_flag,
        [&] {
            s_destroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
        });
    return s_destroyImageKHR;
}

struct TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::EGLImageData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    ~EGLImageData()
    {
        if (numImages) {
            glDeleteTextures(numImages, texture.data());

            for (unsigned i = 0; i < numImages; ++i) {
                if (image[i] != EGL_NO_IMAGE_KHR)
                    destroyImageKHR()(PlatformDisplay::sharedDisplayForCompositing().eglDisplay(), image[i]);
            }
        }
    }

    uint32_t width { 0 };
    uint32_t height { 0 };
    unsigned numImages { 0 };
    std::array<GLuint, DMABufFormat::c_maxPlanes> texture { 0, 0, 0, 0 };
    std::array<EGLImageKHR, DMABufFormat::c_maxPlanes> image { EGL_NO_IMAGE_KHR, EGL_NO_IMAGE_KHR, EGL_NO_IMAGE_KHR, EGL_NO_IMAGE_KHR };
};

TextureMapperPlatformLayerProxyDMABuf::TextureMapperPlatformLayerProxyDMABuf() = default;
TextureMapperPlatformLayerProxyDMABuf::~TextureMapperPlatformLayerProxyDMABuf() = default;

void TextureMapperPlatformLayerProxyDMABuf::activateOnCompositingThread(Compositor* compositor, TextureMapperLayer* targetLayer)
{
#if ASSERT_ENABLED
    if (!m_compositorThread)
        m_compositorThread = &Thread::current();
#endif
    ASSERT(m_compositorThread == &Thread::current());
    ASSERT(compositor);
    ASSERT(targetLayer);

    {
        Locker locker { m_lock };
        m_compositor = compositor;
        m_targetLayer = targetLayer;
    }
}

void TextureMapperPlatformLayerProxyDMABuf::invalidate()
{
    ASSERT(m_compositorThread == &Thread::current());
#if ASSERT_ENABLED
    m_compositorThread = nullptr;
#endif

    Locker locker { m_lock };

    m_pendingLayer = nullptr;
    m_committedLayer = nullptr;
    m_layers = { };

    m_compositor = nullptr;
    m_targetLayer = nullptr;
}

void TextureMapperPlatformLayerProxyDMABuf::swapBuffer()
{
    Locker locker { m_lock };
    if (!m_targetLayer || !m_pendingLayer)
        return;

    auto previousLayer = WTFMove(m_committedLayer);
    m_committedLayer = WTFMove(m_pendingLayer);
    m_targetLayer->setContentsLayer(m_committedLayer.get());

    // The previous and just-committed layers shouldn't be the same. But if they are, don't perform a release.
    ASSERT(!previousLayer || previousLayer != m_committedLayer);
    if (previousLayer && previousLayer != m_committedLayer)
        previousLayer->release();

    if (!m_committedLayer->m_imageData)
        m_committedLayer->m_imageData = DMABufLayer::createEGLImageData(m_committedLayer->m_object);
    m_committedLayer->m_age = 0;

    // Remove any stale layers, e.g. if a layer has gone unused for >c_maximumAge swaps or doesn't match the latest size.
    auto& committedObject = m_committedLayer->m_object;
    auto isStaleLayer =
        [&](auto& it)
        {
            auto& layer = it.value.get();
            return layer.m_age > DMABufLayer::c_maximumAge
                || !(layer.m_object.width == committedObject.width && layer.m_object.height == committedObject.height);
        };

    bool hasStaleLayers = false;
    for (auto it = m_layers.begin(); it != m_layers.end(); ++it) {
        ++it->value->m_age;
        hasStaleLayers |= isStaleLayer(*it);
    }

    if (hasStaleLayers)
        m_layers.removeIf(isStaleLayer);
}

void TextureMapperPlatformLayerProxyDMABuf::pushDMABuf(Ref<DMABufLayer>&& dmabufLayer)
{
    ASSERT(m_lock.isHeld());

    // The pending and just-pushed layers shouldn't be the same. But if they are, don't perform a release.
    ASSERT(!m_pendingLayer || m_pendingLayer != dmabufLayer.ptr());
    if (m_pendingLayer && m_pendingLayer != dmabufLayer.ptr())
        m_pendingLayer->release();

    m_pendingLayer = WTFMove(dmabufLayer);
    if (m_compositor)
        m_compositor->onNewBufferAvailable();
}

TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::DMABufLayer(DMABufObject&& object, TextureMapperGL::Flags flags)
    : m_object(WTFMove(object))
    , m_flags(flags)
{ }

TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::~DMABufLayer() = default;

void TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    if (!m_imageData)
        return;

    // TODO: this is the BT.601 colorspace conversion matrix. The exact desired colorspace should be included
    // in the DMABufObject, and the relevant matrix decided based on it. BT.601 should remain the default.
    static constexpr std::array<GLfloat, 16> s_yuvToRGB {
        1.164383561643836f,  0.0f,                1.596026785714286f, -0.874202217873451f,
        1.164383561643836f, -0.391762290094914f, -0.812967647237771f,  0.531667823499146f,
        1.164383561643836f,  2.017232142857143f, -0.0f,               -1.085630789302022f,
        0.0f,                0.0f,                0.0f,                1.0f,
    };

    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);
    auto& data = *m_imageData;

    switch (m_object.format.fourcc) {
    case DMABufFormat::FourCC::XRGB8888:
    case DMABufFormat::FourCC::XBGR8888:
    case DMABufFormat::FourCC::ARGB8888:
    case DMABufFormat::FourCC::ABGR8888:
        texmapGL.drawTexture(data.texture[0], m_flags, IntSize(data.width, data.height), targetRect, modelViewMatrix, opacity);
        break;
    case DMABufFormat::FourCC::I420:
    case DMABufFormat::FourCC::Y444:
    case DMABufFormat::FourCC::Y41B:
    case DMABufFormat::FourCC::Y42B:
        texmapGL.drawTexturePlanarYUV(std::array<GLuint, 3> { data.texture[0], data.texture[1], data.texture[2] },
            s_yuvToRGB, m_flags, IntSize(data.width, data.height), targetRect, modelViewMatrix, opacity, std::nullopt);
        break;
    case DMABufFormat::FourCC::YV12:
        texmapGL.drawTexturePlanarYUV(std::array<GLuint, 3> { data.texture[0], data.texture[2], data.texture[1] },
            s_yuvToRGB, m_flags, IntSize(data.width, data.height), targetRect, modelViewMatrix, opacity, std::nullopt);
        break;
    case DMABufFormat::FourCC::A420:
        texmapGL.drawTexturePlanarYUV(std::array<GLuint, 3> { data.texture[0], data.texture[1], data.texture[2] },
            s_yuvToRGB, m_flags, IntSize(data.width, data.height), targetRect, modelViewMatrix, opacity, data.texture[3]);
        break;
    case DMABufFormat::FourCC::NV12:
    case DMABufFormat::FourCC::NV21:
        texmapGL.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { data.texture[0], data.texture[1] },
            (m_object.format.fourcc == DMABufFormat::FourCC::NV21),
            s_yuvToRGB, m_flags, IntSize(data.width, data.height), targetRect, modelViewMatrix, opacity);
        break;
    case DMABufFormat::FourCC::YUY2:
    case DMABufFormat::FourCC::UYVY:
    case DMABufFormat::FourCC::VUYA:
    case DMABufFormat::FourCC::YVYU:
        texmapGL.drawTexturePackedYUV(data.texture[0],
            s_yuvToRGB, m_flags, IntSize(data.width, data.height), targetRect, modelViewMatrix, opacity);
        break;
    default:
        break;
    }
}

std::unique_ptr<TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::EGLImageData> TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::createEGLImageData(DMABufObject& object)
{
    using EGLImageData = TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::EGLImageData;

    auto& platformDisplay = PlatformDisplay::sharedDisplayForCompositing();
    EGLDisplay eglDisplay = platformDisplay.eglDisplay();

    EGLImageKHR image[DMABufFormat::c_maxPlanes];
    for (unsigned i = 0; i < object.format.numPlanes; ++i) {
        auto attributes = DMABufEGLUtilities::constructEGLCreateImageAttributes(object, i,
            DMABufEGLUtilities::PlaneModifiersUsage { platformDisplay.eglExtensions().EXT_image_dma_buf_import_modifiers });
        image[i] = createImageKHR()(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes.data());
    }

    auto imageData = makeUnique<EGLImageData>();
    auto& data = *imageData;
    data.width = object.width;
    data.height = object.height;
    data.numImages = object.format.numPlanes;
    glGenTextures(data.numImages, data.texture.data());
    for (unsigned i = 0; i < data.numImages; ++i) {
        data.image[i] = image[i];

        glBindTexture(GL_TEXTURE_2D, data.texture[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, data.image[i]);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return imageData;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(TEXTURE_MAPPER_DMABUF)
