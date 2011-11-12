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

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
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
        offsetLocation = program->vertexShader().offsetLocation();
        scaleLocation = program->vertexShader().scaleLocation();
        alphaLocation = program->fragmentShader().alphaLocation();
    }
    int programId;
    int samplerLocation;
    int matrixLocation;
    int offsetLocation;
    int scaleLocation;
    int alphaLocation;
};

} // anonymous namespace

namespace WebCore {

CCPluginLayerImpl::CCPluginLayerImpl(int id)
    : CCLayerImpl(id)
    , m_textureId(0)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
{
}

CCPluginLayerImpl::~CCPluginLayerImpl()
{
}

void CCPluginLayerImpl::draw(LayerRendererChromium* layerRenderer)
{
    ASSERT(CCProxy::isImplThread());
    PluginProgramBinding binding;
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


void CCPluginLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "plugin layer texture id: " << m_textureId << "\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
