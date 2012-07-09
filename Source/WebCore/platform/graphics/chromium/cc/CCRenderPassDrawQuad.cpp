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

#include "cc/CCRenderPassDrawQuad.h"

#include "cc/CCRenderPass.h"

namespace WebCore {

PassOwnPtr<CCRenderPassDrawQuad> CCRenderPassDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const CCRenderPass* renderPass, bool isReplica, const WebKit::WebFilterOperations& filters, const WebKit::WebFilterOperations& backgroundFilters, unsigned maskTextureId, const IntRect& contentsChangedSinceLastFrame)
{
    return adoptPtr(new CCRenderPassDrawQuad(sharedQuadState, quadRect, renderPass, isReplica, filters, backgroundFilters, maskTextureId, contentsChangedSinceLastFrame));
}

CCRenderPassDrawQuad::CCRenderPassDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const CCRenderPass* renderPass, bool isReplica, const WebKit::WebFilterOperations& filters, const WebKit::WebFilterOperations& backgroundFilters, unsigned maskTextureId, const IntRect& contentsChangedSinceLastFrame)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::RenderPass, quadRect)
    , m_renderPass(renderPass)
    , m_renderPassId(renderPass->id())
    , m_isReplica(isReplica)
    , m_filters(filters)
    , m_backgroundFilters(backgroundFilters)
    , m_maskTextureId(maskTextureId)
    , m_contentsChangedSinceLastFrame(contentsChangedSinceLastFrame)
{
    ASSERT(m_renderPass);
}

}
