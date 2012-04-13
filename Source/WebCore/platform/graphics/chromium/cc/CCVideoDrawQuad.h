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

#ifndef CCVideoDrawQuad_h
#define CCVideoDrawQuad_h

#include "GraphicsTypes3D.h"
#include "cc/CCDrawQuad.h"
#include "cc/CCVideoLayerImpl.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {
class WebVideoFrame;
}

namespace WebCore {

class CCVideoDrawQuad : public CCDrawQuad {
    WTF_MAKE_NONCOPYABLE(CCVideoDrawQuad);
public:
    static PassOwnPtr<CCVideoDrawQuad> create(const CCSharedQuadState*, const IntRect&, CCVideoLayerImpl::Texture* textures, WebKit::WebVideoFrame*, GC3Denum format);

    CCVideoLayerImpl::Texture* textures() const { return m_textures; }
    WebKit::WebVideoFrame* frame() const { return m_frame; }
    GC3Denum format() const { return m_format; }
    const float* matrix() const { return m_matrix; }

    void setMatrix(const float* matrix) { m_matrix = matrix; }

private:
    CCVideoDrawQuad(const CCSharedQuadState*, const IntRect&, CCVideoLayerImpl::Texture* textures, WebKit::WebVideoFrame*, GC3Denum format);

    CCVideoLayerImpl::Texture* m_textures;
    WebKit::WebVideoFrame* m_frame;
    GC3Denum m_format;
    const float* m_matrix;
};

}

#endif
