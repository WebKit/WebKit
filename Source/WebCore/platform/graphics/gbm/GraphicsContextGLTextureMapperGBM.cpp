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
#include "CoordinatedPlatformLayerBufferDMABuf.h"
#include "DMABufBuffer.h"
#include "DRMDeviceManager.h"
#include "GBMDevice.h"
#include "GBMVersioning.h"
#include "GLFence.h"
#include "Logging.h"
#include "PlatformDisplay.h"
#include "TextureMapperFlags.h"
#include <drm_fourcc.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebCore {

bool GraphicsContextGLTextureMapperGBM::checkRequirements()
{
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.type() != PlatformDisplay::Type::GBM)
        return false;

    const auto& eglExtensions = display.eglExtensions();
    if (!eglExtensions.KHR_image_base || !eglExtensions.EXT_image_dma_buf_import)
        return false;

    static const char* disableGBM = getenv("WEBKIT_WEBGL_DISABLE_GBM");
    return !disableGBM || *disableGBM == '0';
}

RefPtr<GraphicsContextGLTextureMapperGBM> GraphicsContextGLTextureMapperGBM::create(GraphicsContextGLAttributes&& attributes, RefPtr<GraphicsLayerContentsDisplayDelegate>&& delegate)
{
    auto context = adoptRef(new GraphicsContextGLTextureMapperGBM(WTF::move(attributes), WTF::move(delegate)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLTextureMapperGBM::GraphicsContextGLTextureMapperGBM(GraphicsContextGLAttributes&& attributes, RefPtr<GraphicsLayerContentsDisplayDelegate>&& delegate)
    : GraphicsContextGLTextureMapperANGLE(WTF::move(attributes))
{
    m_layerContentsDisplayDelegate = WTF::move(delegate);
}

GraphicsContextGLTextureMapperGBM::~GraphicsContextGLTextureMapperGBM()
{
    freeDrawingBuffers();
}

bool GraphicsContextGLTextureMapperGBM::platformInitialize()
{
    auto isOpaqueFormat = [](FourCC fourcc) -> bool {
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
    const auto& supportedFormats = PlatformDisplay::sharedDisplay().bufferFormats();
    for (const auto& format : supportedFormats) {
        bool matchesOpacity = isOpaqueFormat(format.fourcc) == isOpaque;
        if (!matchesOpacity && m_drawingBufferFormat.fourcc)
            continue;

        m_drawingBufferFormat.fourcc = format.fourcc.value;
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
    if (!enableExtensionsImpl({ "GL_OES_EGL_image"_s }))
        return false;

    const auto& eglExtensions = PlatformDisplay::sharedDisplay().eglExtensions();
    return eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import;
}

GraphicsContextGLTextureMapperGBM::DrawingBuffer GraphicsContextGLTextureMapperGBM::createDrawingBuffer() const
{
    auto gbmDevice = DRMDeviceManager::singleton().mainGBMDevice(DRMDeviceManager::NodeType::Render);
    if (!gbmDevice)
        return { };

    const auto size = getInternalFramebufferSize();
    struct gbm_bo* bo = nullptr;
    auto enableModifiers = m_drawingBufferFormat.modifiers.size() == 1 && m_drawingBufferFormat.modifiers[0] == DRM_FORMAT_MOD_INVALID
        ? DMABufBufferAttributes::EnableModifiers::No : DMABufBufferAttributes::EnableModifiers::Yes;
    if (enableModifiers == DMABufBufferAttributes::EnableModifiers::Yes && !m_drawingBufferFormat.modifiers.isEmpty())
        bo = gbm_bo_create_with_modifiers2(gbmDevice->device(), size.width(), size.height(), m_drawingBufferFormat.fourcc, m_drawingBufferFormat.modifiers.span().data(), m_drawingBufferFormat.modifiers.size(), GBM_BO_USE_RENDERING);
    if (!bo)
        bo = gbm_bo_create(gbmDevice->device(), size.width(), size.height(), m_drawingBufferFormat.fourcc, GBM_BO_USE_RENDERING);
    if (!bo)
        return { };

    auto dmaBufAttributes = DMABufBufferAttributes::fromGBMBufferObject(bo, enableModifiers);
    if (!dmaBufAttributes) {
        gbm_bo_destroy(bo);
        return { };
    }

    Ref dmaBuf = DMABufBuffer::create(WTF::move(*dmaBufAttributes));
    auto eglAttributes = DMABufBuffer::buildEGLImageAttributes(dmaBuf->attributes());
    gbm_bo_destroy(bo);

    if (!eglAttributes)
        return { };

    auto image = EGL_CreateImageKHR(m_displayObj, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, eglAttributes->span().data());
    if (!image)
        return { };

    return { WTF::move(dmaBuf), image };
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

        m_drawingBuffer = WTF::move(buffer);
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

UnixFileDescriptor GraphicsContextGLTextureMapperGBM::createExportedFence() const
{
    const auto& eglExtensions = PlatformDisplay::sharedDisplay().eglExtensions();
    if (!eglExtensions.KHR_fence_sync || !eglExtensions.ANDROID_native_fence_sync)
        return { };

    auto sync = EGL_CreateSyncKHR(m_displayObj, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (sync == EGL_NO_SYNC)
        return { };

    GL_Flush();
    auto fd = UnixFileDescriptor { EGL_DupNativeFenceFDANDROID(m_displayObj, sync), UnixFileDescriptor::Adopt };
    EGL_DestroySyncKHR(m_displayObj, sync);
    return fd;
}

void GraphicsContextGLTextureMapperGBM::prepareForDisplay()
{
    UnixFileDescriptor fenceFD;
    std::unique_ptr<GLFence> fence;
    prepareForDisplayWithFinishedSignal([this, &fenceFD, &fence] {
        fenceFD = createExportedFence();
        if (!fenceFD) {
            GL_Flush();
            fence = GLFence::create(PlatformDisplay::sharedDisplay().glDisplay());
        }
    });

    if (!m_displayBuffer.dmabuf)
        return;

    RELEASE_ASSERT(m_layerContentsDisplayDelegate);
    OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
    if (contextAttributes().alpha)
        flags.add(TextureMapperFlags::ShouldBlend);
    std::unique_ptr<CoordinatedPlatformLayerBuffer> buffer;
    if (fenceFD)
        buffer = CoordinatedPlatformLayerBufferDMABuf::create(protect(*m_displayBuffer.dmabuf), flags, WTF::move(fenceFD));
    else
        buffer = CoordinatedPlatformLayerBufferDMABuf::create(protect(*m_displayBuffer.dmabuf), flags, WTF::move(fence));
    m_layerContentsDisplayDelegate->setDisplayBuffer(WTF::move(buffer));
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

#if ENABLE(WEBXR)
GCGLExternalImage GraphicsContextGLTextureMapperGBM::createExternalImage(ExternalImageSource&& source, GCGLenum, GCGLint)
{
    GraphicsContextGLExternalImageSource imageSource = WTF::move(source);

    if (m_displayObj == EGL_NO_DISPLAY) {
        addError(GCGLErrorCode::InvalidOperation);
        LOG(XR, "invalid display %d", EGL_GetError());
        return { };
    }

    DMABufBufferAttributes dmaBufAttributes { imageSource.size, imageSource.fourcc, WTF::move(imageSource.fds), WTF::move(imageSource.offsets), WTF::move(imageSource.strides), imageSource.modifier };
    auto enableModifiers = PlatformDisplay::sharedDisplay().eglExtensions().EXT_image_dma_buf_import_modifiers
        ? DMABufBufferAttributes::EnableModifiers::Yes : DMABufBufferAttributes::EnableModifiers::No;
    auto eglAttributes = DMABufBuffer::buildEGLImageAttributes(dmaBufAttributes, enableModifiers);
    if (!eglAttributes) {
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }
    auto eglImage = EGL_CreateImageKHR(m_displayObj, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, eglAttributes->span().data());
    if (!eglImage) {
        LOG(XR, "invalid operation importing the image %d", EGL_GetError());
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }
    auto newName = ++m_nextExternalImageName;
    m_eglImages.add(newName, eglImage);
    return newName;
}

void GraphicsContextGLTextureMapperGBM::bindExternalImage(GCGLenum target, GCGLExternalImage image)
{
    if (!makeContextCurrent())
        return;
    EGLImage eglImage = EGL_NO_IMAGE_KHR;
    if (image) {
        eglImage = m_eglImages.get(image);
        if (!eglImage) {
            addError(GCGLErrorCode::InvalidOperation);
            return;
        }
    }
    if (target == RENDERBUFFER)
        GL_EGLImageTargetRenderbufferStorageOES(RENDERBUFFER, eglImage);
    else
        GL_EGLImageTargetTexture2DOES(target, eglImage);
}

bool GraphicsContextGLTextureMapperGBM::enableRequiredWebXRExtensions()
{
    if (!makeContextCurrent())
        return false;

    return enableExtensionsImpl({
        "GL_OES_EGL_image"_s,
        "GL_OES_EGL_image_external"_s
    });
}
#endif // ENABLE(WEBXR)

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
