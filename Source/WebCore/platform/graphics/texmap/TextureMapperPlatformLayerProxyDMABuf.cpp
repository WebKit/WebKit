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
#include "TextureMapperGLHeaders.h"
#include "TextureMapperLayer.h"
#include <fcntl.h>
#include <initializer_list>
#include <unistd.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

// This has to be included after the EGL headers.
#include "DMABufEGLUtilities.h"

namespace WebCore {

struct TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::EGLImageData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    ~EGLImageData()
    {
        if (numImages) {
            auto& platformDisplay = PlatformDisplay::sharedDisplayForCompositing();
            glDeleteTextures(numImages, texture.data());

            for (unsigned i = 0; i < numImages; ++i) {
                if (image[i] != EGL_NO_IMAGE_KHR)
                    platformDisplay.destroyEGLImage(image[i]);
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
        if (m_targetLayer)
            m_targetLayer->setContentsLayer(nullptr);
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
    if (m_targetLayer)
        m_targetLayer->setContentsLayer(nullptr);
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

TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::DMABufLayer(DMABufObject&& object, OptionSet<TextureMapperFlags> flags)
    : m_object(WTFMove(object))
    , m_flags(flags)
{
}

TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::~DMABufLayer() = default;

void TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    if (!m_imageData)
        return;

    static constexpr std::array<GLfloat, 16> s_bt601ConversionMatrix {
        1.164383561643836,  0.0,                1.596026785714286, -0.874202217873451,
        1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146,
        1.164383561643836,  2.017232142857143,  0.0,               -1.085630789302022,
        0.0,                0.0,                0.0,                1.0,
    };

    static constexpr std::array<GLfloat, 16> s_bt709ConversionMatrix {
        1.164383561643836,  0.0,                1.792741071428571, -0.972945075016308,
        1.164383561643836, -0.213248614273730, -0.532909328559444,  0.301482665475862,
        1.164383561643836,  2.112401785714286,  0.0,               -1.133402217873451,
        0.0,                0.0,                0.0,                1.0,
    };

    static constexpr std::array<GLfloat, 16> s_bt2020ConversionMatrix {
        1.164383561643836,  0.0,                1.678674107142857, -0.915687932159165,
        1.164383561643836, -0.187326104219343, -0.650424318505057,  0.347458498519301,
        1.164383561643836,  2.141772321428571,  0.0,               -1.148145075016308,
        0.0,                0.0,                0.0,                1.0,
    };

    static constexpr std::array<GLfloat, 16> s_smpte240MConversionMatrix {
        1.164383561643836,  0.0,                1.793651785714286, -0.973402217873451,
        1.164383561643836, -0.256532845251675, -0.542724809537390,  0.328136638536074,
        1.164383561643836,  2.07984375,         0.0,               -1.117059360730593,
        0.0,                0.0,                0.0,                1.0,
    };

    // Based on the specified colorspace, a YUV-to-RGB matrix is chosen. The default is the BT.601 matrix.
    // Invalid or SRGB colorspace defaults to that as well, but in case of RGBA-like formats, the matrix
    // of course goes unused. This is complemented with the below assert that for those formats the specified
    // colorspace is either invalid or SRGB.
    const std::array<GLfloat, 16>& yuvToRGB =
        [&] {
            switch (m_object.colorSpace) {
            case DMABufColorSpace::Invalid:
            case DMABufColorSpace::SRGB:
                break;
            case DMABufColorSpace::BT601:
                return s_bt601ConversionMatrix;
            case DMABufColorSpace::BT709:
                return s_bt709ConversionMatrix;
            case DMABufColorSpace::BT2020:
                return s_bt2020ConversionMatrix;
            case DMABufColorSpace::SMPTE240M:
                return s_smpte240MConversionMatrix;
            }
            return s_bt601ConversionMatrix;
        }();

    auto& data = *m_imageData;

    switch (m_object.format.fourcc) {
    case DMABufFormat::FourCC::XRGB8888:
    case DMABufFormat::FourCC::XBGR8888:
    case DMABufFormat::FourCC::ARGB8888:
    case DMABufFormat::FourCC::ABGR8888:
        // Either no colorspace or the SRGB colorspace was defined for this object. Other options are not meaningful.
        ASSERT(m_object.colorSpace == DMABufColorSpace::Invalid || m_object.colorSpace == DMABufColorSpace::SRGB);
        textureMapper.drawTexture(data.texture[0], m_flags, targetRect, modelViewMatrix, opacity);
        break;
    case DMABufFormat::FourCC::I420:
    case DMABufFormat::FourCC::Y444:
    case DMABufFormat::FourCC::Y41B:
    case DMABufFormat::FourCC::Y42B:
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { data.texture[0], data.texture[1], data.texture[2] },
            yuvToRGB, m_flags, targetRect, modelViewMatrix, opacity, std::nullopt);
        break;
    case DMABufFormat::FourCC::YV12:
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { data.texture[0], data.texture[2], data.texture[1] },
            yuvToRGB, m_flags, targetRect, modelViewMatrix, opacity, std::nullopt);
        break;
    case DMABufFormat::FourCC::A420:
        textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { data.texture[0], data.texture[1], data.texture[2] },
            yuvToRGB, m_flags, targetRect, modelViewMatrix, opacity, data.texture[3]);
        break;
    case DMABufFormat::FourCC::NV12:
    case DMABufFormat::FourCC::NV21:
        textureMapper.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { data.texture[0], data.texture[1] },
            (m_object.format.fourcc == DMABufFormat::FourCC::NV21),
            yuvToRGB, m_flags, targetRect, modelViewMatrix, opacity);
        break;
    case DMABufFormat::FourCC::YUY2:
    case DMABufFormat::FourCC::YVYU:
    case DMABufFormat::FourCC::UYVY:
    case DMABufFormat::FourCC::VYUY:
    case DMABufFormat::FourCC::VUYA:
    case DMABufFormat::FourCC::AYUV:
        textureMapper.drawTexturePackedYUV(data.texture[0],
            yuvToRGB, m_flags, targetRect, modelViewMatrix, opacity);
        break;
    case DMABufFormat::FourCC::P010:
    case DMABufFormat::FourCC::P016:
        // These HDR formats have 10 bits color depth, but since we support only 8 bits color depth, we
        // threat it as a regular semi-planar YUV format, thus ignoring the two least significant
        // bits when rendering.
        textureMapper.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { data.texture[0], data.texture[1] },
            false, yuvToRGB, m_flags, targetRect, modelViewMatrix, opacity);
        break;
    default:
        break;
    }
}

std::unique_ptr<TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::EGLImageData> TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::createEGLImageData(DMABufObject& object)
{
    using EGLImageData = TextureMapperPlatformLayerProxyDMABuf::DMABufLayer::EGLImageData;

    auto& platformDisplay = PlatformDisplay::sharedDisplayForCompositing();

    EGLImageKHR image[DMABufFormat::c_maxPlanes];
    for (unsigned i = 0; i < object.format.numPlanes; ++i) {
        auto attributes = DMABufEGLUtilities::constructEGLCreateImageAttributes(object, i,
            DMABufEGLUtilities::PlaneModifiersUsage { platformDisplay.eglExtensions().EXT_image_dma_buf_import_modifiers });
        image[i] = platformDisplay.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
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
