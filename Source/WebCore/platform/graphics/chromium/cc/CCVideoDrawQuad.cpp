/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "cc/CCVideoDrawQuad.h"

namespace WebCore {

PassOwnPtr<CCVideoDrawQuad> CCVideoDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, CCVideoLayerImpl::FramePlane planes[WebKit::WebVideoFrame::maxPlanes], unsigned frameProviderTextureId, GC3Denum format, const WebKit::WebTransformationMatrix& matrix)
{
    return adoptPtr(new CCVideoDrawQuad(sharedQuadState, quadRect, planes, frameProviderTextureId, format, matrix));
}

CCVideoDrawQuad::CCVideoDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, CCVideoLayerImpl::FramePlane planes[WebKit::WebVideoFrame::maxPlanes], unsigned frameProviderTextureId, GC3Denum format, const WebKit::WebTransformationMatrix& matrix)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::VideoContent, quadRect)
    , m_frameProviderTextureId(frameProviderTextureId)
    , m_format(format)
    , m_matrix(matrix)
{
    for (size_t i = 0; i < WebKit::WebVideoFrame::maxPlanes; ++i)
      m_planes[i] = planes[i];

}

}
