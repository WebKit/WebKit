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

#ifndef CCDrawQuad_h
#define CCDrawQuad_h

#include "CCSharedQuadState.h"

namespace WebCore {

// WARNING! All CCXYZDrawQuad classes must remain PODs (plain old data).
// They are intended to be "serializable" by copying their raw bytes, so they
// must not contain any non-bit-copyable member variables!
//
// Furthermore, the class members need to be packed so they are aligned
// properly and don't have paddings/gaps, otherwise memory check tools
// like Valgrind will complain about uninitialized memory usage when
// transferring these classes over the wire.
#pragma pack(push, 4)

// CCDrawQuad is a bag of data used for drawing a quad. Because different
// materials need different bits of per-quad data to render, classes that derive
// from CCDrawQuad store additional data in their derived instance. The Material
// enum is used to "safely" downcast to the derived class.
class CCDrawQuad {
public:
    enum Material {
        Invalid,
        Checkerboard,
        DebugBorder,
        IOSurfaceContent,
        RenderPass,
        TextureContent,
        SolidColor,
        TiledContent,
        YUVVideoContent,
        StreamVideoContent,
    };

    IntRect quadRect() const { return m_quadRect; }
    const WebKit::WebTransformationMatrix& quadTransform() const { return m_sharedQuadState->quadTransform; }
    IntRect visibleContentRect() const { return m_sharedQuadState->visibleContentRect; }
    IntRect clippedRectInTarget() const { return m_sharedQuadState->clippedRectInTarget; }
    float opacity() const { return m_sharedQuadState->opacity; }
    // For the purposes of blending, what part of the contents of this quad are opaque?
    IntRect opaqueRect() const;
    bool needsBlending() const { return m_needsBlending || !opaqueRect().contains(m_quadVisibleRect); }

    // Allows changing the rect that gets drawn to make it smaller. Parameter passed
    // in will be clipped to quadRect().
    void setQuadVisibleRect(const IntRect&);
    IntRect quadVisibleRect() const { return m_quadVisibleRect; }
    bool isDebugQuad() const { return m_material == DebugBorder; }

    Material material() const { return m_material; }

    // Returns transfer size of this object based on the derived class (by
    // looking at the material type).
    unsigned size() const;

    const CCSharedQuadState* sharedQuadState() const { return m_sharedQuadState; }
    int sharedQuadStateId() const { return m_sharedQuadStateId; }
    void setSharedQuadState(const CCSharedQuadState*);

protected:
    CCDrawQuad(const CCSharedQuadState*, Material, const IntRect&);

    // Stores state common to a large bundle of quads; kept separate for memory
    // efficiency. There is special treatment to reconstruct these pointers
    // during serialization.
    const CCSharedQuadState* m_sharedQuadState;
    int m_sharedQuadStateId;

    Material m_material;
    IntRect m_quadRect;
    IntRect m_quadVisibleRect;

    // By default, the shared quad state determines whether or not this quad is
    // opaque or needs blending. Derived classes can override with these
    // variables.
    bool m_quadOpaque;
    bool m_needsBlending;

    // Be default, this rect is empty. It is used when the shared quad state and above
    // variables determine that the quad is not fully opaque but may be partially opaque.
    IntRect m_opaqueRect;
};

#pragma pack(pop)

}

#endif
