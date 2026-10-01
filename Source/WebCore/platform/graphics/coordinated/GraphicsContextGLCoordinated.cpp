/*
 * Copyright (C) 2024, 2026 Igalia S.L.
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
#include "GraphicsContextGLCoordinated.h"

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS)
#include "ANGLEHeaders.h"
#include "GLFence.h"
#include "GraphicsLayerContentsDisplayDelegateCoordinated.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"

#if USE(TEXTURE_MAPPER)
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "TextureMapperFlags.h"
#else
#include "CoordinatedPlatformLayerBufferSkiaImage.h"
#endif

namespace WebCore {

RefPtr<GraphicsContextGLCoordinated> GraphicsContextGLCoordinated::create(GraphicsContextGLAttributes&& attributes)
{
    Ref context = adoptRef(*new GraphicsContextGLCoordinated(WTF::move(attributes)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLCoordinated::GraphicsContextGLCoordinated(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLEGL(WTF::move(attributes))
{
    m_layerContentsDisplayDelegate = GraphicsLayerContentsDisplayDelegateCoordinated::create();
}

GraphicsContextGLCoordinated::~GraphicsContextGLCoordinated()
{
    if (!makeContextCurrent())
        return;

    while (!m_obsoleteDrawingBuffers.isEmpty()) {
        auto buffer = m_obsoleteDrawingBuffers.takeLast();
        destroyDrawingBuffer(buffer);
    }

    for (auto& buffer : m_drawingBuffers)
        destroyDrawingBuffer(buffer);

    m_texture = 0;
}

bool GraphicsContextGLCoordinated::platformInitialize()
{
    // Destroy the texture created by GraphicsContextGLANGLE since we will
    // use our own textures from the swap chain.
    GL_DeleteTextures(1, &m_texture);
    m_texture = 0;

    return true;
}

GraphicsContextGLCoordinated::DrawingBuffer::DrawingBuffer(GCGLuint texture, Ref<CoordinatedWebGLTextureWrapper>&& textureWrapper)
    : m_texture(texture)
    , m_textureWrapper(WTF::move(textureWrapper))
{
}

GraphicsContextGLCoordinated::DrawingBuffer::~DrawingBuffer() = default;

GraphicsContextGLCoordinated::DrawingBuffer::DrawingBuffer(GraphicsContextGLCoordinated::DrawingBuffer&& other)
    : m_texture(std::exchange(other.m_texture, 0))
    , m_textureWrapper(WTF::move(other.m_textureWrapper))
{
}

GraphicsContextGLCoordinated::DrawingBuffer& GraphicsContextGLCoordinated::DrawingBuffer::operator=(GraphicsContextGLCoordinated::DrawingBuffer&& other)
{
    m_texture = std::exchange(other.m_texture, 0);
    m_textureWrapper = WTF::move(other.m_textureWrapper);
    return *this;
}

bool GraphicsContextGLCoordinated::DrawingBuffer::isInUse() const
{
    if (!m_textureWrapper)
        return false;

    return !m_textureWrapper->hasOneRef();
}

GCGLuint GraphicsContextGLCoordinated::DrawingBuffer::release()
{
    m_textureWrapper = nullptr;
    return std::exchange(m_texture, 0);
}

GraphicsContextGLCoordinated::DrawingBuffer GraphicsContextGLCoordinated::createDrawingBuffer() const
{
    ScopedRestoreTextureBinding restoreBinding(TEXTURE_BINDING_2D, TEXTURE_2D);
    ScopedBufferBinding scopedPixelUnpackBufferReset(GL_PIXEL_UNPACK_BUFFER, 0, m_isForWebGL2);
    GCGLuint texture;
    GL_GenTextures(1, &texture);
    GL_BindTexture(GL_TEXTURE_2D, texture);
    GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    auto textureID = setupCurrentTexture();

    const auto size = getInternalFramebufferSize();
    if (!size.isEmpty()) {
        GLuint colorFormat = contextAttributes().alpha ? GL_RGBA : GL_RGB;
        GL_TexImage2D(GL_TEXTURE_2D, 0, colorFormat, size.width(), size.height(), 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    }

    return { texture, CoordinatedWebGLTextureWrapper::create(textureID) };
}

void GraphicsContextGLCoordinated::destroyDrawingBuffer(DrawingBuffer& buffer) const
{
    if (!buffer)
        return;

    auto texture = buffer.release();
    GL_DeleteTextures(1, &texture);
}

void GraphicsContextGLCoordinated::freeDrawingBuffers()
{
    freeObsoleteDrawingBuffers();

    for (auto& buffer : m_drawingBuffers) {
        if (buffer.isInUse())
            m_obsoleteDrawingBuffers.append(WTF::move(buffer));
        else
            destroyDrawingBuffer(buffer);
    }

    m_texture = 0;
}

void GraphicsContextGLCoordinated::freeObsoleteDrawingBuffers()
{
    m_obsoleteDrawingBuffers.removeAllMatching([&](auto& buffer) {
        if (buffer.isInUse())
            return false;

        destroyDrawingBuffer(buffer);
        return true;
    });
}

bool GraphicsContextGLCoordinated::bindNextDrawingBuffer()
{
    freeObsoleteDrawingBuffers();

    m_currentDrawingBufferIndex++;
    auto& buffer = drawingBuffer();
    if (buffer && (buffer.isInUse() || m_failNextDrawingBufferAllocation))
        m_obsoleteDrawingBuffers.append(WTF::move(buffer));

    if (std::exchange(m_failNextDrawingBufferAllocation, false))
        return false;

    if (!buffer) {
        buffer = createDrawingBuffer();
        if (!buffer)
            return false;
    }

    m_texture = buffer.texture();
    return true;
}

bool GraphicsContextGLCoordinated::reshapeDrawingBuffer()
{
    freeDrawingBuffers();
    return bindNextDrawingBuffer();
}

void GraphicsContextGLCoordinated::prepareForDisplay()
{
    if (!makeContextCurrent())
        return;

    if (!drawingBuffer())
        return;

    prepareTexture();
    if (!bindNextDrawingBuffer()) {
        forceContextLost();
        return;
    }

    attachDrawingBufferTexture();

    auto& displayBuffer = this->displayBuffer();
    if (!displayBuffer)
        return;

    std::unique_ptr<CoordinatedPlatformLayerBuffer> buffer;
    auto fboSize = getInternalFramebufferSize();
    auto fence = GLFence::create(PlatformDisplay::sharedDisplay().glDisplay());
#if USE(TEXTURE_MAPPER)
    OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
    if (contextAttributes().alpha)
        flags.add(TextureMapperFlags::ShouldBlend);
    buffer = CoordinatedPlatformLayerBufferRGB::create(displayBuffer.textureWrapper()->id(), fboSize, flags, WTF::move(fence));
#else
    auto alphaMode = contextAttributes().alpha ? CoordinatedPlatformLayerBuffer::AlphaMode::Premultiplied : CoordinatedPlatformLayerBuffer::AlphaMode::Opaque;
    buffer = CoordinatedPlatformLayerBufferSkiaImage::create(Ref { *displayBuffer.textureWrapper() }, fboSize, alphaMode, WTF::move(fence), m_layerContentsDisplayDelegate->threadSafeGrContext());
#endif
    m_layerContentsDisplayDelegate->setDisplayBuffer(WTF::move(buffer));
}

RefPtr<PixelBuffer> GraphicsContextGLCoordinated::readCompositedResults()
{
    auto& displayBuffer = this->displayBuffer();
    if (!displayBuffer)
        return nullptr;
    if (!makeContextCurrent())
        return nullptr;
    if (getInternalFramebufferSize().isEmpty())
        return nullptr;
    // bindNextDrawingBuffer() leaves m_texture bound to the buffer that will be rendered into next,
    // so the presented frame is only available as the displayBuffer texture.
    ScopedScratchReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO, displayBuffer.texture());
    return readPixelsForPaintResults();
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS)
