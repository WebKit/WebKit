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

#include "cc/CCDrawQuad.h"

#include "cc/CCCheckerboardDrawQuad.h"
#include "cc/CCDebugBorderDrawQuad.h"
#include "cc/CCIOSurfaceDrawQuad.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCRenderSurfaceDrawQuad.h"
#include "cc/CCSolidColorDrawQuad.h"
#include "cc/CCTextureDrawQuad.h"
#include "cc/CCTileDrawQuad.h"
#include "cc/CCVideoDrawQuad.h"

namespace WebCore {

CCDrawQuad::CCDrawQuad(const CCSharedQuadState* sharedQuadState, Material material, const IntRect& quadRect)
    : m_sharedQuadState(sharedQuadState)
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
    if (m_sharedQuadState->isOpaque() && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void CCDrawQuad::setQuadVisibleRect(const IntRect& quadVisibleRect)
{
    m_quadVisibleRect = quadVisibleRect;
    m_quadVisibleRect.intersect(m_quadRect);
}

const CCCheckerboardDrawQuad* CCDrawQuad::toCheckerboardDrawQuad() const
{
    ASSERT(m_material == Checkerboard);
    return static_cast<const CCCheckerboardDrawQuad*>(this);
}

const CCDebugBorderDrawQuad* CCDrawQuad::toDebugBorderDrawQuad() const
{
    ASSERT(m_material == DebugBorder);
    return static_cast<const CCDebugBorderDrawQuad*>(this);
}

const CCIOSurfaceDrawQuad* CCDrawQuad::toIOSurfaceDrawQuad() const
{
    ASSERT(m_material == IOSurfaceContent);
    return static_cast<const CCIOSurfaceDrawQuad*>(this);
}

const CCRenderSurfaceDrawQuad* CCDrawQuad::toRenderSurfaceDrawQuad() const
{
    ASSERT(m_material == RenderSurface);
    return static_cast<const CCRenderSurfaceDrawQuad*>(this);
}

const CCSolidColorDrawQuad* CCDrawQuad::toSolidColorDrawQuad() const
{
    ASSERT(m_material == SolidColor);
    return static_cast<const CCSolidColorDrawQuad*>(this);
}

const CCTextureDrawQuad* CCDrawQuad::toTextureDrawQuad() const
{
    ASSERT(m_material == TextureContent);
    return static_cast<const CCTextureDrawQuad*>(this);
}
const CCTileDrawQuad* CCDrawQuad::toTileDrawQuad() const
{
    ASSERT(m_material == TiledContent);
    return static_cast<const CCTileDrawQuad*>(this);
}

const CCVideoDrawQuad* CCDrawQuad::toVideoDrawQuad() const
{
    ASSERT(m_material == VideoContent);
    return static_cast<const CCVideoDrawQuad*>(this);
}


}
