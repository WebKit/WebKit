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
#include <public/WebCompositorQuad.h>

#include "IntRect.h"
#include <public/WebCompositorCheckerboardQuad.h>
#include <public/WebCompositorDebugBorderQuad.h>
#include <public/WebCompositorIOSurfaceQuad.h>
#include <public/WebCompositorSolidColorQuad.h>
#include <public/WebCompositorStreamVideoQuad.h>
#include <public/WebCompositorTextureQuad.h>
#include <public/WebCompositorTileQuad.h>

using namespace WebCore;

namespace WebKit {

WebCompositorQuad::WebCompositorQuad(const WebCompositorSharedQuadState* sharedQuadState, Material material, const IntRect& quadRect)
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

IntRect WebCompositorQuad::opaqueRect() const
{
    if (opacity() != 1)
        return IntRect();
    if (m_sharedQuadState->opaque && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void WebCompositorQuad::setQuadVisibleRect(const IntRect& quadVisibleRect)
{
    IntRect intersection = quadVisibleRect;
    intersection.intersect(m_quadRect);
    m_quadVisibleRect = intersection;
}

unsigned WebCompositorQuad::size() const
{
    switch (material()) {
    case Checkerboard:
        return sizeof(WebCompositorCheckerboardQuad);
    case DebugBorder:
        return sizeof(WebCompositorDebugBorderQuad);
    case IOSurfaceContent:
        return sizeof(WebCompositorIOSurfaceQuad);
    case TextureContent:
        return sizeof(WebCompositorTextureQuad);
    case SolidColor:
        return sizeof(WebCompositorSolidColorQuad);
    case TiledContent:
        return sizeof(WebCompositorTileQuad);
    case StreamVideoContent:
        return sizeof(WebCompositorStreamVideoQuad);
    case Invalid:
    case RenderPass:
    case YUVVideoContent:
        break;
    }

    CRASH();
    return sizeof(WebCompositorQuad);
}

void WebCompositorQuad::setSharedQuadState(const WebCompositorSharedQuadState* sharedQuadState)
{
    m_sharedQuadState = sharedQuadState;
    m_sharedQuadStateId = sharedQuadState->id;
}

}
