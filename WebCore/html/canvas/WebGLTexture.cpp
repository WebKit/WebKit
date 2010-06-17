/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGLTexture.h"
#include "WebGLRenderingContext.h"

namespace WebCore {
    
PassRefPtr<WebGLTexture> WebGLTexture::create(WebGLRenderingContext* ctx)
{
    return adoptRef(new WebGLTexture(ctx));
}

WebGLTexture::WebGLTexture(WebGLRenderingContext* ctx)
    : CanvasObject(ctx)
    , cubeMapRWrapModeInitialized(false)
    , m_target(0)
    , m_minFilter(GraphicsContext3D::NEAREST_MIPMAP_LINEAR)
    , m_magFilter(GraphicsContext3D::LINEAR)
    , m_wrapS(GraphicsContext3D::REPEAT)
    , m_wrapT(GraphicsContext3D::REPEAT)
    , m_internalFormat(0)
    , m_isNPOT(false)
    , m_needToUseBlackTexture(false)
{
    setObject(context()->graphicsContext3D()->createTexture());
    for (int ii = 0; ii < 6; ++ii) {
        m_width[ii] = 0;
        m_height[ii] = 0;
    }
}

void WebGLTexture::setTarget(unsigned long target)
{
    // Target is finalized the first time bindTexture() is called.
    if (m_target)
        return;
    switch (target) {
    case GraphicsContext3D::TEXTURE_2D:
    case GraphicsContext3D::TEXTURE_CUBE_MAP:
        m_target = target;
        break;
    }
}

void WebGLTexture::setParameteri(unsigned long pname, int param)
{
    switch (pname) {
    case GraphicsContext3D::TEXTURE_MIN_FILTER:
        switch (param) {
        case GraphicsContext3D::NEAREST:
        case GraphicsContext3D::LINEAR:
        case GraphicsContext3D::NEAREST_MIPMAP_NEAREST:
        case GraphicsContext3D::LINEAR_MIPMAP_NEAREST:
        case GraphicsContext3D::NEAREST_MIPMAP_LINEAR:
        case GraphicsContext3D::LINEAR_MIPMAP_LINEAR:
            m_minFilter = param;
            break;
        }
        break;
    case GraphicsContext3D::TEXTURE_MAG_FILTER:
        switch (param) {
        case GraphicsContext3D::NEAREST:
        case GraphicsContext3D::LINEAR:
            m_magFilter = param;
            break;
        }
        break;
    case GraphicsContext3D::TEXTURE_WRAP_S:
        switch (param) {
        case GraphicsContext3D::CLAMP_TO_EDGE:
        case GraphicsContext3D::MIRRORED_REPEAT:
        case GraphicsContext3D::REPEAT:
            m_wrapS = param;
            break;
        }
        break;
    case GraphicsContext3D::TEXTURE_WRAP_T:
        switch (param) {
        case GraphicsContext3D::CLAMP_TO_EDGE:
        case GraphicsContext3D::MIRRORED_REPEAT:
        case GraphicsContext3D::REPEAT:
            m_wrapT = param;
            break;
        }
        break;
    default:
        return;
    }
    updateNPOTStates();
}

void WebGLTexture::setParameterf(unsigned long pname, float param)
{
    int iparam = static_cast<int>(param);
    setParameteri(pname, iparam);
}

void WebGLTexture::setSize(unsigned long target, unsigned width, unsigned height)
{
    if (!width || !height)
        return;
    int iTarget = -1;
    if (m_target == GraphicsContext3D::TEXTURE_2D) {
        if (target == GraphicsContext3D::TEXTURE_2D)
            iTarget = 0;
    } else if (m_target == GraphicsContext3D::TEXTURE_CUBE_MAP && width == height) {
        switch (target) {
        case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_X:
            iTarget = 0;
            break;
        case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_X:
            iTarget = 1;
            break;
        case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Y:
            iTarget = 2;
            break;
        case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Y:
            iTarget = 3;
            break;
        case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Z:
            iTarget = 4;
            break;
        case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Z:
            iTarget = 5;
            break;
        }
    }
    if (iTarget < 0)
        return;
    m_width[iTarget] = width;
    m_height[iTarget] = height;
    updateNPOTStates();
}

bool WebGLTexture::isNPOT(unsigned width, unsigned height)
{
    if (!width || !height)
        return false;
    if ((width & (width - 1)) || (height & (height - 1)))
        return true;
    return false;
}

void WebGLTexture::_deleteObject(Platform3DObject object)
{
    context()->graphicsContext3D()->deleteTexture(object);
}

void WebGLTexture::updateNPOTStates()
{
    int numTargets = 0;
    if (m_target == GraphicsContext3D::TEXTURE_2D)
        numTargets = 1;
    else if (m_target == GraphicsContext3D::TEXTURE_CUBE_MAP)
        numTargets = 6;
    m_isNPOT = false;
    unsigned w0 = m_width[0], h0 = m_height[0];
    for (int ii = 0; ii < numTargets; ++ii) {
        if (ii && (!m_width[ii] || !m_height[ii] || m_width[ii] != w0 || m_height[ii] != h0)) {
            // We only set NPOT for complete cube map textures.
            m_isNPOT = false;
            break;
        }
        if (isNPOT(m_width[ii], m_height[ii]))
            m_isNPOT = true;
    }
    m_needToUseBlackTexture = false;
    if (m_isNPOT && ((m_minFilter != GraphicsContext3D::NEAREST && m_minFilter != GraphicsContext3D::LINEAR)
                     || m_wrapS != GraphicsContext3D::CLAMP_TO_EDGE || m_wrapT != GraphicsContext3D::CLAMP_TO_EDGE))
        m_needToUseBlackTexture = true;
}

}

#endif // ENABLE(3D_CANVAS)
