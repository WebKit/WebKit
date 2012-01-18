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

#include "cc/CCCanvasLayerImpl.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "cc/CCCanvasDrawQuad.h"
#include "cc/CCProxy.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CCCanvasLayerImpl::CCCanvasLayerImpl(int id)
    : CCLayerImpl(id)
    , m_textureId(0)
    , m_hasAlpha(true)
    , m_premultipliedAlpha(true)
{
}

CCCanvasLayerImpl::~CCCanvasLayerImpl()
{
}

void CCCanvasLayerImpl::draw(LayerRendererChromium* layerRenderer)
{
    ASSERT(CCProxy::isImplThread());
    const CCCanvasLayerImpl::Program* program = layerRenderer->canvasLayerProgram();
    ASSERT(program && program->initialized());
    GraphicsContext3D* context = layerRenderer->context();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));

    if (!m_hasAlpha) {
        // Even though the WebGL layer's texture was likely allocated
        // as GL_RGB, disable blending anyway for better robustness.
        context->disable(GraphicsContext3D::BLEND);
    } else {
        GC3Denum sfactor = m_premultipliedAlpha ? GraphicsContext3D::ONE : GraphicsContext3D::SRC_ALPHA;
        GLC(context, context->blendFunc(sfactor, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));
    }
    GLC(context, context->useProgram(program->program()));
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));
    layerRenderer->drawTexturedQuad(drawTransform(), bounds().width(), bounds().height(), drawOpacity(), layerRenderer->sharedGeometryQuad(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation(),
                                    -1);
    if (!m_hasAlpha)
        context->enable(GraphicsContext3D::BLEND);
}

void CCCanvasLayerImpl::appendQuads(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState)
{
    IntRect quadRect(IntPoint(), bounds());
    quadList.append(CCCanvasDrawQuad::create(sharedQuadState, quadRect, this));
}

void CCCanvasLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "canvas layer texture id: " << m_textureId << " premultiplied: " << m_premultipliedAlpha << "\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
