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

#include "CCRenderPassDrawQuad.h"

namespace WebCore {

PassOwnPtr<CCRenderPassDrawQuad> CCRenderPassDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, int renderPassId, bool isReplica, const CCResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY)
{
    return adoptPtr(new CCRenderPassDrawQuad(sharedQuadState, quadRect, renderPassId, isReplica, maskResourceId, contentsChangedSinceLastFrame, maskTexCoordScaleX, maskTexCoordScaleY, maskTexCoordOffsetX, maskTexCoordOffsetY));
}

CCRenderPassDrawQuad::CCRenderPassDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, int renderPassId, bool isReplica, CCResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::RenderPass, quadRect)
    , m_renderPassId(renderPassId)
    , m_isReplica(isReplica)
    , m_maskResourceId(maskResourceId)
    , m_contentsChangedSinceLastFrame(contentsChangedSinceLastFrame)
    , m_maskTexCoordScaleX(maskTexCoordScaleX)
    , m_maskTexCoordScaleY(maskTexCoordScaleY)
    , m_maskTexCoordOffsetX(maskTexCoordOffsetX)
    , m_maskTexCoordOffsetY(maskTexCoordOffsetY)
{
    ASSERT(m_renderPassId > 0);
}

const CCRenderPassDrawQuad* CCRenderPassDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::RenderPass);
    return static_cast<const CCRenderPassDrawQuad*>(quad);
}

}
