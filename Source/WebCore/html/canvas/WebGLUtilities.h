/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL)
#include "WebGL2RenderingContext.h"

namespace WebCore {

class ScopedInspectorShaderProgramHighlight {
    WTF_MAKE_NONCOPYABLE(ScopedInspectorShaderProgramHighlight);
public:
    ScopedInspectorShaderProgramHighlight(WebGLRenderingContextBase& context)
       : m_context(shouldApply(context) ? &context : nullptr) // NOLINT
    {
        if (LIKELY(!m_context))
            return;
        showHighlight();
    }

    ~ScopedInspectorShaderProgramHighlight()
    {
        if (LIKELY(!m_context))
            return;
        hideHighlight();
    }
private:
    static bool shouldApply(WebGLRenderingContextBase&);
    void showHighlight();
    void hideHighlight();

    struct {
        GCGLfloat color[4];
        GCGLenum equationRGB;
        GCGLenum equationAlpha;
        GCGLenum srcRGB;
        GCGLenum dstRGB;
        GCGLenum srcAlpha;
        GCGLenum dstAlpha;
        GCGLboolean enabled;
    } m_savedBlend;

    WebGLRenderingContextBase* const m_context;
};

// Set UNPACK_ALIGNMENT to 1, all other parameters to 0.
class ScopedTightUnpackParameters {
    WTF_MAKE_NONCOPYABLE(ScopedTightUnpackParameters);
public:
    explicit ScopedTightUnpackParameters(WebGLRenderingContextBase& context, bool enabled = true)
        : m_context(enabled ? &context : nullptr)
    {
        if (!m_context)
            return;
        set(m_context->unpackPixelStoreParameters(), tightUnpack);
    }

    ~ScopedTightUnpackParameters()
    {
        if (!m_context)
            return;
        set(tightUnpack, m_context->unpackPixelStoreParameters());
    }
private:
    using PixelStoreParameters =  WebGLRenderingContextBase::PixelStoreParameters;
    static constexpr PixelStoreParameters tightUnpack { 1, 0, 0, 0, 0 };
    void set(const PixelStoreParameters& oldValues, const PixelStoreParameters& newValues)
    {
        RefPtr context = m_context->graphicsContextGL();
        if (oldValues.alignment != newValues.alignment)
            context->pixelStorei(GraphicsContextGL::UNPACK_ALIGNMENT, newValues.alignment);
        if (oldValues.rowLength != newValues.rowLength)
            context->pixelStorei(GraphicsContextGL::UNPACK_ROW_LENGTH, newValues.rowLength);
        if (oldValues.imageHeight != newValues.imageHeight)
            context->pixelStorei(GraphicsContextGL::UNPACK_IMAGE_HEIGHT, newValues.imageHeight);
        if (oldValues.skipPixels != newValues.skipPixels)
            context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_PIXELS, newValues.skipPixels);
        if (oldValues.skipRows != newValues.skipRows)
            context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_ROWS, newValues.skipRows);
        if (oldValues.skipImages != newValues.skipImages)
            context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_IMAGES, newValues.skipImages);
    }
    WebGLRenderingContextBase* const m_context;
};

class ScopedDisableRasterizerDiscard {
    WTF_MAKE_NONCOPYABLE(ScopedDisableRasterizerDiscard);
public:
    explicit ScopedDisableRasterizerDiscard(WebGLRenderingContextBase& context)
        : m_context(context.m_rasterizerDiscardEnabled ? &context : nullptr)
    {
        if (!m_context)
            return;
        m_context->protectedGraphicsContextGL()->disable(GraphicsContextGL::RASTERIZER_DISCARD);
    }

    ~ScopedDisableRasterizerDiscard()
    {
        if (!m_context)
            return;
        m_context->protectedGraphicsContextGL()->enable(GraphicsContextGL::RASTERIZER_DISCARD);
    }

private:
    WebGLRenderingContextBase* const m_context;
};

class ScopedEnableBackbuffer {
    WTF_MAKE_NONCOPYABLE(ScopedEnableBackbuffer);
public:
    explicit ScopedEnableBackbuffer(WebGLRenderingContextBase& context)
        : m_context(context.m_backDrawBuffer == GraphicsContextGL::NONE ? &context : nullptr)
    {
        if (!m_context)
            return;
        GCGLenum value[1] { GraphicsContextGL::COLOR_ATTACHMENT0 };
        if (m_context->isWebGL2())
            m_context->protectedGraphicsContextGL()->drawBuffers(value);
        else
            m_context->protectedGraphicsContextGL()->drawBuffersEXT(value);
    }

    ~ScopedEnableBackbuffer()
    {
        if (!m_context)
            return;
        GCGLenum value[1] { GraphicsContextGL::NONE };
        if (m_context->isWebGL2())
            m_context->protectedGraphicsContextGL()->drawBuffers(value);
        else
            m_context->protectedGraphicsContextGL()->drawBuffersEXT(value);
    }

private:
    WebGLRenderingContextBase* const m_context;
};

class ScopedDisableScissorTest {
    WTF_MAKE_NONCOPYABLE(ScopedDisableScissorTest);
public:
    explicit ScopedDisableScissorTest(WebGLRenderingContextBase& context)
        : m_context(context.m_scissorEnabled ? &context : nullptr)
    {
        if (!m_context)
            return;
        m_context->protectedGraphicsContextGL()->disable(GraphicsContextGL::SCISSOR_TEST);
    }

    ~ScopedDisableScissorTest()
    {
        if (!m_context)
            return;
        m_context->protectedGraphicsContextGL()->enable(GraphicsContextGL::SCISSOR_TEST);
    }

private:
    WebGLRenderingContextBase* const m_context;
};

class ScopedWebGLRestoreFramebuffer {
    WTF_MAKE_NONCOPYABLE(ScopedWebGLRestoreFramebuffer);
public:
    explicit ScopedWebGLRestoreFramebuffer(WebGLRenderingContextBase& context)
        : m_context(context)
    {
    }

    ~ScopedWebGLRestoreFramebuffer()
    {
        RefPtr gl = m_context.graphicsContextGL();
        if (m_context.isWebGL2()) {
            auto& context2 = downcast<WebGL2RenderingContext>(m_context);
            gl->bindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, objectOrZero(context2.m_readFramebufferBinding));
            gl->bindFramebuffer(GraphicsContextGL::DRAW_FRAMEBUFFER, objectOrZero(context2.m_framebufferBinding));
        } else
            gl->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, objectOrZero(m_context.m_framebufferBinding));
    }

private:
    WebGLRenderingContextBase& m_context;
};

class ScopedWebGLRestoreRenderbuffer {
    WTF_MAKE_NONCOPYABLE(ScopedWebGLRestoreRenderbuffer);
public:
    explicit ScopedWebGLRestoreRenderbuffer(WebGLRenderingContextBase& context)
        : m_context(context)
    {
    }

    ~ScopedWebGLRestoreRenderbuffer()
    {
        RefPtr gl = m_context.graphicsContextGL();
        gl->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, objectOrZero(m_context.m_renderbufferBinding));
    }
    WebGLRenderingContextBase& m_context;
};

class ScopedWebGLRestoreTexture {
    WTF_MAKE_NONCOPYABLE(ScopedWebGLRestoreTexture);
public:
    explicit ScopedWebGLRestoreTexture(WebGLRenderingContextBase& context, GCGLenum textureTarget)
        : m_context(context)
        , m_target(textureTarget)
    {
    }

    ~ScopedWebGLRestoreTexture()
    {
        auto& textureUnit = m_context.m_textureUnits[m_context.m_activeTextureUnit];
        PlatformGLObject texture = 0;
        switch (m_target) {
        case GraphicsContextGL::TEXTURE_2D:
            texture = objectOrZero(textureUnit.texture2DBinding);
            break;
        case GraphicsContextGL::TEXTURE_CUBE_MAP:
            texture = objectOrZero(textureUnit.textureCubeMapBinding);
            break;
        case GraphicsContextGL::TEXTURE_3D:
            texture = objectOrZero(textureUnit.texture3DBinding);
            break;
        case GraphicsContextGL::TEXTURE_2D_ARRAY:
            texture = objectOrZero(textureUnit.texture2DArrayBinding);
            break;
        default:
            // Not part of WebGL, does not need to be restored.
            return;
        }
        RefPtr gl = m_context.graphicsContextGL();
        gl->bindTexture(m_target, texture);
    }

private:
    WebGLRenderingContextBase& m_context;
    const GCGLenum m_target;
};

class ScopedClearColorAndMask {
    WTF_MAKE_NONCOPYABLE(ScopedClearColorAndMask);
public:
    explicit ScopedClearColorAndMask(WebGLRenderingContextBase& context, GCGLclampf clearRed, GCGLclampf clearGreen, GCGLclampf clearBlue, GCGLclampf clearAlpha, GCGLboolean maskRed, GCGLboolean maskGreen, GCGLboolean maskBlue, GCGLboolean maskAlpha)
        : m_context(context)
    {
        RefPtr gl = m_context.protectedGraphicsContextGL();
        gl->clearColor(clearRed, clearGreen, clearBlue, clearAlpha);
        if (m_context.m_oesDrawBuffersIndexed)
            gl->colorMaskiOES(0, maskRed, maskGreen, maskBlue, maskAlpha);
        else
            gl->colorMask(maskRed, maskGreen, maskBlue, maskAlpha);
    }

    ~ScopedClearColorAndMask()
    {
        auto clearRed   = m_context.m_clearColor[0];
        auto clearGreen = m_context.m_clearColor[1];
        auto clearBlue  = m_context.m_clearColor[2];
        auto clearAlpha = m_context.m_clearColor[3];

        auto maskRed   = m_context.m_colorMask[0];
        auto maskGreen = m_context.m_colorMask[1];
        auto maskBlue  = m_context.m_colorMask[2];
        auto maskAlpha = m_context.m_colorMask[3];

        RefPtr gl = m_context.protectedGraphicsContextGL();
        gl->clearColor(clearRed, clearGreen, clearBlue, clearAlpha);
        if (m_context.m_oesDrawBuffersIndexed)
            gl->colorMaskiOES(0, maskRed, maskGreen, maskBlue, maskAlpha);
        else
            gl->colorMask(maskRed, maskGreen, maskBlue, maskAlpha);
    }

private:
    WebGLRenderingContextBase& m_context;
};

class ScopedClearDepthAndMask {
    WTF_MAKE_NONCOPYABLE(ScopedClearDepthAndMask);
public:
    explicit ScopedClearDepthAndMask(WebGLRenderingContextBase& context, GCGLclampf clear, bool mask, bool enabled)
        : m_context(enabled ? &context : nullptr) // NOLINT
    {
        if (!m_context)
            return;

        RefPtr gl = m_context->protectedGraphicsContextGL();
        gl->clearDepth(clear);
        gl->depthMask(mask);
    }

    ~ScopedClearDepthAndMask()
    {
        if (!m_context)
            return;

        RefPtr gl = m_context->protectedGraphicsContextGL();
        gl->clearDepth(m_context->m_clearDepth);
        gl->depthMask(m_context->m_depthMask);
    }

private:
    WebGLRenderingContextBase* const m_context;
};

class ScopedClearStencilAndMask {
    WTF_MAKE_NONCOPYABLE(ScopedClearStencilAndMask);
public:
    explicit ScopedClearStencilAndMask(WebGLRenderingContextBase& context, GCGLint clear, GCGLenum face, GCGLuint mask, bool enabled)
        : m_context(enabled ? &context : nullptr) // NOLINT
    {
        if (!m_context)
            return;

        RefPtr gl = m_context->protectedGraphicsContextGL();
        gl->clearStencil(clear);
        gl->stencilMaskSeparate(face, mask);
    }

    ~ScopedClearStencilAndMask()
    {
        if (!m_context)
            return;

        RefPtr gl = m_context->protectedGraphicsContextGL();
        gl->clearStencil(m_context->m_clearStencil);
        gl->stencilMaskSeparate(GraphicsContextGL::FRONT, m_context->m_stencilMask);
    }

private:
    WebGLRenderingContextBase* const m_context;
};

}

#endif
