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

#ifndef CCTileDrawQuad_h
#define CCTileDrawQuad_h

#include "GraphicsTypes3D.h"
#include "cc/CCDrawQuad.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCTileDrawQuad : public CCDrawQuad {
    WTF_MAKE_NONCOPYABLE(CCTileDrawQuad);
public:
    static PassOwnPtr<CCTileDrawQuad> create(const CCSharedQuadState*, const IntRect& quadRect, const IntRect& opaqueRect, Platform3DObject textureId, const IntPoint& textureOffset, const IntSize& textureSize, GC3Dint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA);

    Platform3DObject textureId() const { return m_textureId; }
    IntPoint textureOffset() const { return m_textureOffset; }
    IntSize textureSize() const { return m_textureSize; }
    GC3Dint textureFilter() const { return m_textureFilter; }
    bool swizzleContents() const { return m_swizzleContents; }

    bool leftEdgeAA() const { return m_leftEdgeAA; }
    bool topEdgeAA() const { return m_topEdgeAA; }
    bool rightEdgeAA() const { return m_rightEdgeAA; }
    bool bottomEdgeAA() const { return m_bottomEdgeAA; }

    bool isAntialiased() const { return leftEdgeAA() || topEdgeAA() || rightEdgeAA() || bottomEdgeAA(); }

private:
    CCTileDrawQuad(const CCSharedQuadState*, const IntRect& quadRect, const IntRect& opaqueRect, Platform3DObject textureId, const IntPoint& textureOffset, const IntSize& textureSize, GC3Dint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA);

    Platform3DObject m_textureId;
    IntPoint m_textureOffset;
    IntSize m_textureSize;
    GC3Dint m_textureFilter;
    bool m_swizzleContents;
    bool m_leftEdgeAA;
    bool m_topEdgeAA;
    bool m_rightEdgeAA;
    bool m_bottomEdgeAA;
};

}

#endif
