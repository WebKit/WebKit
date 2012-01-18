/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCPluginLayerImpl.h"

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "cc/CCPluginDrawQuad.h"
#include "cc/CCProxy.h"
#include <wtf/text/WTFString.h>

namespace {

struct PluginProgramBinding {
    template<class Program> void set(Program* program)
    {
        ASSERT(program && program->initialized());
        programId = program->program();
        samplerLocation = program->fragmentShader().samplerLocation();
        matrixLocation = program->vertexShader().matrixLocation();
        alphaLocation = program->fragmentShader().alphaLocation();
    }
    int programId;
    int samplerLocation;
    int matrixLocation;
    int alphaLocation;
};

struct TexStretchPluginProgramBinding : PluginProgramBinding {
    template<class Program> void set(Program* program)
    {
        PluginProgramBinding::set(program);
        offsetLocation = program->vertexShader().offsetLocation();
        scaleLocation = program->vertexShader().scaleLocation();
    }
    int offsetLocation;
    int scaleLocation;
};

struct TexTransformPluginProgramBinding : PluginProgramBinding {
    template<class Program> void set(Program* program)
    {
        PluginProgramBinding::set(program);
        texTransformLocation = program->vertexShader().texTransformLocation();
    }
    int texTransformLocation;
};

} // anonymous namespace

namespace WebCore {

CCPluginLayerImpl::CCPluginLayerImpl(int id)
    : CCLayerImpl(id)
    , m_textureId(0)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
    , m_ioSurfaceId(0)
    , m_ioSurfaceWidth(0)
    , m_ioSurfaceHeight(0)
    , m_ioSurfaceChanged(false)
    , m_ioSurfaceTextureId(0)
{
}

CCPluginLayerImpl::~CCPluginLayerImpl()
{
    cleanupResources();
}

void CCPluginLayerImpl::draw(LayerRendererChromium* layerRenderer)
{
    ASSERT(CCProxy::isImplThread());

    if (m_ioSurfaceChanged) {
        GraphicsContext3D* context = layerRenderer->context();
        Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(context->getExtensions());
        ASSERT(extensions->supports("GL_CHROMIUM_iosurface"));
        ASSERT(extensions->supports("GL_ARB_texture_rectangle"));

        if (!m_ioSurfaceTextureId)
            m_ioSurfaceTextureId = context->createTexture();

        GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
        GLC(context, context->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, m_ioSurfaceTextureId));
        GLC(context, context->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        GLC(context, context->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
        GLC(context, context->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
        GLC(context, context->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
        extensions->texImageIOSurface2DCHROMIUM(Extensions3D::TEXTURE_RECTANGLE_ARB,
                                                m_ioSurfaceWidth,
                                                m_ioSurfaceHeight,
                                                m_ioSurfaceId,
                                                0);
        // Do not check for error conditions. texImageIOSurface2DCHROMIUM is supposed to hold on to
        // the last good IOSurface if the new one is already closed. This is only a possibility
        // during live resizing of plugins. However, it seems that this is not sufficient to
        // completely guard against garbage being drawn. If this is found to be a significant issue,
        // it may be necessary to explicitly tell the embedder when to free the surfaces it has
        // allocated.
        m_ioSurfaceChanged = false;
    }

    if (m_ioSurfaceTextureId) {
        TexTransformPluginProgramBinding binding;
        if (m_flipped)
            binding.set(layerRenderer->pluginLayerTexRectProgramFlip());
        else
            binding.set(layerRenderer->pluginLayerTexRectProgram());

        GraphicsContext3D* context = layerRenderer->context();
        GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
        GLC(context, context->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, m_ioSurfaceTextureId));

        GLC(context, context->useProgram(binding.programId));
        GLC(context, context->uniform1i(binding.samplerLocation, 0));
        // Note: this code path ignores m_uvRect.
        GLC(context, context->uniform4f(binding.texTransformLocation, 0, 0, m_ioSurfaceWidth, m_ioSurfaceHeight));
        layerRenderer->drawTexturedQuad(drawTransform(), bounds().width(), bounds().height(), drawOpacity(), layerRenderer->sharedGeometryQuad(),
                                        binding.matrixLocation,
                                        binding.alphaLocation,
                                        -1);
        GLC(context, context->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, 0));
    } else {
        TexStretchPluginProgramBinding binding;
        if (m_flipped)
            binding.set(layerRenderer->pluginLayerProgramFlip());
        else
            binding.set(layerRenderer->pluginLayerProgram());

        GraphicsContext3D* context = layerRenderer->context();
        GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
        GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId));

        // FIXME: setting the texture parameters every time is redundant. Move this code somewhere
        // where it will only happen once per texture.
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
        GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

        GLC(context, context->useProgram(binding.programId));
        GLC(context, context->uniform1i(binding.samplerLocation, 0));
        GLC(context, context->uniform2f(binding.offsetLocation, m_uvRect.x(), m_uvRect.y()));
        GLC(context, context->uniform2f(binding.scaleLocation, m_uvRect.width(), m_uvRect.height()));
        layerRenderer->drawTexturedQuad(drawTransform(), bounds().width(), bounds().height(), drawOpacity(), layerRenderer->sharedGeometryQuad(),
                                        binding.matrixLocation,
                                        binding.alphaLocation,
                                        -1);
    }
}

void CCPluginLayerImpl::appendQuads(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState)
{
    IntRect quadRect(IntPoint(), bounds());
    quadList.append(CCPluginDrawQuad::create(sharedQuadState, quadRect, this));
}

void CCPluginLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "plugin layer texture id: " << m_textureId << "\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

void CCPluginLayerImpl::setIOSurfaceProperties(int width, int height, uint32_t ioSurfaceId)
{
    if (m_ioSurfaceId != ioSurfaceId)
        m_ioSurfaceChanged = true;

    m_ioSurfaceWidth = width;
    m_ioSurfaceHeight = height;
    m_ioSurfaceId = ioSurfaceId;
}

void CCPluginLayerImpl::cleanupResources()
{
    // FIXME: it seems there is no layer renderer / GraphicsContext3D available here. Ideally we
    // would like to delete m_ioSurfaceTextureId.
    m_ioSurfaceTextureId = 0;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
