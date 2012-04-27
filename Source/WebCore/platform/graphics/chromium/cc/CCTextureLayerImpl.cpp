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

#include "cc/CCTextureLayerImpl.h"

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "cc/CCIOSurfaceDrawQuad.h"
#include "cc/CCProxy.h"
#include "cc/CCQuadCuller.h"
#include "cc/CCTextureDrawQuad.h"

namespace WebCore {

CCTextureLayerImpl::CCTextureLayerImpl(int id)
    : CCLayerImpl(id)
    , m_textureId(0)
    , m_premultipliedAlpha(true)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
    , m_ioSurfaceId(0)
    , m_ioSurfaceChanged(false)
    , m_ioSurfaceTextureId(0)
{
}

CCTextureLayerImpl::~CCTextureLayerImpl()
{
    // FIXME: it seems there is no layer renderer / GraphicsContext3D available here. Ideally we
    // would like to delete m_ioSurfaceTextureId.
    m_ioSurfaceTextureId = 0;
}

void CCTextureLayerImpl::willDraw(LayerRendererChromium* layerRenderer)
{
    CCLayerImpl::willDraw(layerRenderer);

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
                                                m_ioSurfaceSize.width(),
                                                m_ioSurfaceSize.height(),
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
}

void CCTextureLayerImpl::appendQuads(CCQuadCuller& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    IntRect quadRect(IntPoint(), bounds());
    if (m_textureId)
        quadList.append(CCTextureDrawQuad::create(sharedQuadState, quadRect, m_textureId, m_premultipliedAlpha, m_uvRect, m_flipped));
    else if (m_ioSurfaceTextureId)
        quadList.append(CCIOSurfaceDrawQuad::create(sharedQuadState, quadRect, m_flipped, m_ioSurfaceSize, m_ioSurfaceTextureId));
}

void CCTextureLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "texture layer texture id: " << m_textureId << " premultiplied: " << m_premultipliedAlpha << "\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

void CCTextureLayerImpl::didLoseContext()
{
    m_textureId = 0;
    if (m_ioSurfaceId) {
        // We don't have a valid texture ID in the new context; however,
        // the IOSurface is still valid.
        m_ioSurfaceTextureId = 0;
        m_ioSurfaceChanged = true;
    }
}

void CCTextureLayerImpl::setIOSurfaceProperties(const IntSize& size, unsigned ioSurfaceId)
{
    if (m_ioSurfaceId != ioSurfaceId)
        m_ioSurfaceChanged = true;

    m_ioSurfaceSize = size;
    m_ioSurfaceId = ioSurfaceId;
}
}

#endif // USE(ACCELERATED_COMPOSITING)
