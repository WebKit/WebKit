/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "GraphicsContextGLTextureMapperGBM.h"

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "ANGLEHeaders.h"
#include "DMABufBuffer.h"
#include "DRMDeviceManager.h"
#include "GBMVersioning.h"
#include "GLFence.h"
#include "PlatformDisplay.h"
#include <drm_fourcc.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebCore {

RefPtr<GraphicsContextGLTextureMapperGBM> GraphicsContextGLTextureMapperGBM::create(GraphicsContextGLAttributes&& attributes, RefPtr<GraphicsLayerContentsDisplayDelegateGBM>&& delegate)
{
    auto context = adoptRef(new GraphicsContextGLTextureMapperGBM(WTFMove(attributes), WTFMove(delegate)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLTextureMapperGBM::GraphicsContextGLTextureMapperGBM(GraphicsContextGLAttributes&& attributes, RefPtr<GraphicsLayerContentsDisplayDelegateGBM>&& delegate)
    : GraphicsContextGLTextureMapperANGLE(WTFMove(attributes))
{
    m_layerContentsDisplayDelegate = WTFMove(delegate);
}

GraphicsContextGLTextureMapperGBM::~GraphicsContextGLTextureMapperGBM()
{
    freeDrawingBuffers();
}

bool GraphicsContextGLTextureMapperGBM::platformInitialize()
{
    auto isOpaqueFormat = [](uint32_t fourcc) -> bool {
        return fourcc != DRM_FORMAT_ARGB8888
            && fourcc != DRM_FORMAT_RGBA8888
            && fourcc != DRM_FORMAT_ABGR8888
            && fourcc != DRM_FORMAT_BGRA8888
            && fourcc != DRM_FORMAT_ARGB2101010
            && fourcc != DRM_FORMAT_ABGR2101010
            && fourcc != DRM_FORMAT_ARGB16161616F
            && fourcc != DRM_FORMAT_ABGR16161616F;
    };

    bool isOpaque = !contextAttributes().alpha;
    const auto& supportedFormats = PlatformDisplay::sharedDisplay().dmabufFormats();
    for (const auto& format : supportedFormats) {
        bool matchesOpacity = isOpaqueFormat(format.fourcc) == isOpaque;
        if (!matchesOpacity && m_drawingBufferFormat.fourcc)
            continue;

        m_drawingBufferFormat.fourcc = format.fourcc;
        m_drawingBufferFormat.modifiers = format.modifiers;
        if (matchesOpacity)
            break;
    }

    if (!m_drawingBufferFormat.fourcc)
        return false;

    return true;
}

bool GraphicsContextGLTextureMapperGBM::platformInitializeExtensions()
{
    if (!enableExtension("GL_OES_EGL_image"_s))
        return false;

    const auto& eglExtensions = PlatformDisplay::sharedDisplay().eglExtensions();
    return eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import;
}

GraphicsContextGLTextureMapperGBM::DrawingBuffer GraphicsContextGLTextureMapperGBM::createDrawingBuffer() const
{
    auto* device = DRMDeviceManager::singleton().mainGBMDeviceNode(DRMDeviceManager::NodeType::Render);
    if (!device)
        return { };

    const auto size = getInternalFramebufferSize();
    struct gbm_bo* bo = nullptr;
    bool disableModifiers = m_drawingBufferFormat.modifiers.size() == 1 && m_drawingBufferFormat.modifiers[0] == DRM_FORMAT_MOD_INVALID;
    if (!disableModifiers && !m_drawingBufferFormat.modifiers.isEmpty())
        bo = gbm_bo_create_with_modifiers2(device, size.width(), size.height(), m_drawingBufferFormat.fourcc, m_drawingBufferFormat.modifiers.data(), m_drawingBufferFormat.modifiers.size(), GBM_BO_USE_RENDERING);
    if (!bo)
        bo = gbm_bo_create(device, size.width(), size.height(), m_drawingBufferFormat.fourcc, GBM_BO_USE_RENDERING);
    if (!bo)
        return { };

    Vector<UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;
    uint32_t format = gbm_bo_get_format(bo);
    int planeCount = gbm_bo_get_plane_count(bo);
    uint64_t modifier = disableModifiers ? DRM_FORMAT_MOD_INVALID : gbm_bo_get_modifier(bo);

    Vector<EGLint> attributes = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(format),
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    fds.append(UnixFileDescriptor { gbm_bo_get_fd_for_plane(bo, planeIndex), UnixFileDescriptor::Adopt }); \
    offsets.append(gbm_bo_get_offset(bo, planeIndex)); \
    strides.append(gbm_bo_get_stride_for_plane(bo, planeIndex)); \
    std::array<EGLint, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, fds.last().value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLint>(offsets.last()), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLint>(strides.last()) \
    }; \
    attributes.append(std::span<const EGLint> { planeAttributes }); \
    if (modifier != DRM_FORMAT_MOD_INVALID) { \
        std::array<EGLint, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLint>(modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLint>(modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLint> { modifierAttributes }); \
    } \
    }

    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBS

    attributes.append(EGL_NONE);

    auto* image = EGL_CreateImageKHR(m_displayObj, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes.data());
    gbm_bo_destroy(bo);
    if (!image)
        return { };

    return { DMABufBuffer::create(size, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier), image };
}

void GraphicsContextGLTextureMapperGBM::freeDrawingBuffers()
{
    auto destroyBuffer = [this](DrawingBuffer& buffer) {
        if (!buffer.image)
            return;

        EGL_DestroyImageKHR(m_displayObj, buffer.image);
        buffer.image = nullptr;
        buffer.dmabuf = nullptr;
    };
    destroyBuffer(m_drawingBuffer);
    destroyBuffer(m_displayBuffer);
}

bool GraphicsContextGLTextureMapperGBM::bindNextDrawingBuffer()
{
    std::swap(m_drawingBuffer, m_displayBuffer);

    if (!m_drawingBuffer.dmabuf) {
        auto buffer = createDrawingBuffer();
        if (!buffer.dmabuf)
            return false;

        m_drawingBuffer = WTFMove(buffer);
    }

    auto [textureTarget, textureBinding] = drawingBufferTextureBindingPoint();
    ScopedRestoreTextureBinding restoreBinding(textureBinding, textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
    GL_BindTexture(textureTarget, m_texture);
    GL_EGLImageTargetTexture2DOES(textureTarget, m_drawingBuffer.image);
    return true;
}

bool GraphicsContextGLTextureMapperGBM::reshapeDrawingBuffer()
{
    freeDrawingBuffers();
    return bindNextDrawingBuffer();
}

void GraphicsContextGLTextureMapperGBM::prepareForDisplay()
{
    std::unique_ptr<GLFence> fence;
    prepareForDisplayWithFinishedSignal([&fence] {
        fence = GLFence::create();
    });

    RELEASE_ASSERT(m_layerContentsDisplayDelegate);
    static_cast<GraphicsLayerContentsDisplayDelegateGBM*>(m_layerContentsDisplayDelegate.get())->setDisplayBuffer(m_displayBuffer.dmabuf, WTFMove(fence));
}

void GraphicsContextGLTextureMapperGBM::prepareForDisplayWithFinishedSignal(Function<void()>&& finishedSignalCreator)
{
    if (!makeContextCurrent())
        return;

    if (!m_drawingBuffer.dmabuf)
        return;

    prepareTexture();
    finishedSignalCreator();

    if (!bindNextDrawingBuffer()) {
        forceContextLost();
        return;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
