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

#include "cc/CCTileDrawQuad.h"

namespace WebCore {

PassOwnPtr<CCTileDrawQuad> CCTileDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const IntRect& opaqueRect, Platform3DObject textureId, const IntPoint& textureOffset, const IntSize& textureSize, GC3Dint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA)
{
    return adoptPtr(new CCTileDrawQuad(sharedQuadState, quadRect, opaqueRect, textureId, textureOffset, textureSize, textureFilter, swizzleContents, leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA));
}

CCTileDrawQuad::CCTileDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const IntRect& opaqueRect, Platform3DObject textureId, const IntPoint& textureOffset, const IntSize& textureSize, GC3Dint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::TiledContent, quadRect)
    , m_textureId(textureId)
    , m_textureOffset(textureOffset)
    , m_textureSize(textureSize)
    , m_textureFilter(textureFilter)
    , m_swizzleContents(swizzleContents)
    , m_leftEdgeAA(leftEdgeAA)
    , m_topEdgeAA(topEdgeAA)
    , m_rightEdgeAA(rightEdgeAA)
    , m_bottomEdgeAA(bottomEdgeAA)
{
    if (isAntialiased())
        m_needsBlending = true;
    m_opaqueRect = opaqueRect;
}

}
