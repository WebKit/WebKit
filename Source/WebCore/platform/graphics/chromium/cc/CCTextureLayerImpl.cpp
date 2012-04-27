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
{
}

CCTextureLayerImpl::~CCTextureLayerImpl()
{
}

void CCTextureLayerImpl::appendQuads(CCQuadCuller& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    IntRect quadRect(IntPoint(), bounds());
    quadList.append(CCTextureDrawQuad::create(sharedQuadState, quadRect, m_textureId, m_premultipliedAlpha, m_uvRect, m_flipped));
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
}

}

#endif // USE(ACCELERATED_COMPOSITING)
