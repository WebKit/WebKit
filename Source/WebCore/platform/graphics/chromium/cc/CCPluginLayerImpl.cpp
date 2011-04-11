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
#include "PluginLayerChromium.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CCPluginLayerImpl::CCPluginLayerImpl(LayerChromium* owner)
    : CCLayerImpl(owner)
    , m_textureId(0)
{
}

CCPluginLayerImpl::~CCPluginLayerImpl()
{
}

void CCPluginLayerImpl::draw(const IntRect&)
{
    ASSERT(layerRenderer());
    const CCPluginLayerImpl::Program* program = layerRenderer()->pluginLayerProgram();
    ASSERT(program && program->initialized());
    GraphicsContext3D* context = layerRenderer()->context();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId));

    // FIXME: setting the texture parameters every time is redundant. Move this code somewhere
    // where it will only happen once per texture.
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    layerRenderer()->useShader(program->program());
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));
    LayerChromium::drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                                    bounds().width(), bounds().height(), drawOpacity(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation());
}


void CCPluginLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "plugin layer texture id: " << m_textureId << "\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
