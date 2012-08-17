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
#include "CCDrawQuad.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "CCSolidColorDrawQuad.h"
#include "CCStreamVideoDrawQuad.h"
#include "CCTextureDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "CCYUVVideoDrawQuad.h"
#include "IntRect.h"

namespace WebCore {

CCDrawQuad::CCDrawQuad(const CCSharedQuadState* sharedQuadState, Material material, const IntRect& quadRect)
    : m_sharedQuadState(sharedQuadState)
    , m_sharedQuadStateId(sharedQuadState->id)
    , m_material(material)
    , m_quadRect(quadRect)
    , m_quadVisibleRect(quadRect)
    , m_quadOpaque(true)
    , m_needsBlending(false)
{
    ASSERT(m_sharedQuadState);
    ASSERT(m_material != Invalid);
}

IntRect CCDrawQuad::opaqueRect() const
{
    if (opacity() != 1)
        return IntRect();
    if (m_sharedQuadState->opaque && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void CCDrawQuad::setQuadVisibleRect(const IntRect& quadVisibleRect)
{
    IntRect intersection = quadVisibleRect;
    intersection.intersect(m_quadRect);
    m_quadVisibleRect = intersection;
}

unsigned CCDrawQuad::size() const
{
    switch (material()) {
    case Checkerboard:
        return sizeof(CCCheckerboardDrawQuad);
    case DebugBorder:
        return sizeof(CCDebugBorderDrawQuad);
    case IOSurfaceContent:
        return sizeof(CCIOSurfaceDrawQuad);
    case TextureContent:
        return sizeof(CCTextureDrawQuad);
    case SolidColor:
        return sizeof(CCSolidColorDrawQuad);
    case TiledContent:
        return sizeof(CCTileDrawQuad);
    case StreamVideoContent:
        return sizeof(CCStreamVideoDrawQuad);
    case RenderPass:
        return sizeof(CCRenderPassDrawQuad);
    case YUVVideoContent:
        return sizeof(CCYUVVideoDrawQuad);
    case Invalid:
        break;
    }

    CRASH();
    return sizeof(CCDrawQuad);
}

void CCDrawQuad::setSharedQuadState(const CCSharedQuadState* sharedQuadState)
{
    m_sharedQuadState = sharedQuadState;
    m_sharedQuadStateId = sharedQuadState->id;
}

}
