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

#ifndef CCYUVVideoDrawQuad_h
#define CCYUVVideoDrawQuad_h

#include "CCDrawQuad.h"
#include "CCVideoLayerImpl.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCYUVVideoDrawQuad : public CCDrawQuad {
    WTF_MAKE_NONCOPYABLE(CCYUVVideoDrawQuad);
public:
    static PassOwnPtr<CCYUVVideoDrawQuad> create(const CCSharedQuadState*, const IntRect&, const CCVideoLayerImpl::FramePlane& yPlane, const CCVideoLayerImpl::FramePlane& uPlane, const CCVideoLayerImpl::FramePlane& vPlane);

    const CCVideoLayerImpl::FramePlane& yPlane() const { return m_yPlane; }
    const CCVideoLayerImpl::FramePlane& uPlane() const { return m_uPlane; }
    const CCVideoLayerImpl::FramePlane& vPlane() const { return m_vPlane; }

    static const CCYUVVideoDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCYUVVideoDrawQuad(const CCSharedQuadState*, const IntRect&, const CCVideoLayerImpl::FramePlane& yPlane, const CCVideoLayerImpl::FramePlane& uPlane, const CCVideoLayerImpl::FramePlane& vPlane);

    CCVideoLayerImpl::FramePlane m_yPlane;
    CCVideoLayerImpl::FramePlane m_uPlane;
    CCVideoLayerImpl::FramePlane m_vPlane;
};

}

#endif
