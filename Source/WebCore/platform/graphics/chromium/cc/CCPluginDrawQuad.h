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

#ifndef CCPluginDrawQuad_h
#define CCPluginDrawQuad_h

#include "cc/CCDrawQuad.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCLayerImpl;

class CCPluginDrawQuad : public CCDrawQuad {
    WTF_MAKE_NONCOPYABLE(CCPluginDrawQuad);
public:
    static PassOwnPtr<CCPluginDrawQuad> create(const CCSharedQuadState*, const IntRect& quadRect, const FloatRect& uvRect, unsigned textureId, bool flipped, int ioSurfaceWidth, int ioSurfaceHeight, unsigned m_ioSurfaceTextureId);

    FloatRect uvRect() const { return m_uvRect; }
    unsigned textureId() const { return m_textureId; }
    bool flipped() const { return m_flipped; }
    int ioSurfaceWidth() const { return m_ioSurfaceWidth; }
    int ioSurfaceHeight() const { return m_ioSurfaceHeight; }

    unsigned ioSurfaceTextureId() const { return m_ioSurfaceTextureId; }
    
private:
    CCPluginDrawQuad(const CCSharedQuadState*, const IntRect& quadRect, const FloatRect& uvRect, unsigned textureId, bool flipped, int ioSurfaceWidth, int ioSurfaceHeight, unsigned ioSurfaceTextureId);

    FloatRect m_uvRect;
    unsigned m_textureId;
    bool m_flipped;
    int m_ioSurfaceWidth;
    int m_ioSurfaceHeight;
    unsigned m_ioSurfaceTextureId;
};

}

#endif
