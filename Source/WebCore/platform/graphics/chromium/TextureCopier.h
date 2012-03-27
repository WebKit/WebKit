/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextureCopier_h
#define TextureCopier_h

#include "GraphicsContext3D.h"
#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class TextureCopier {
public:
    // Copy the base level contents of |sourceTextureId| to |destTextureId|. Both texture objects
    // must be complete and have a base level of |size| dimensions. The color formats do not need
    // to match, but |destTextureId| must have a renderable format.
    virtual void copyTexture(GraphicsContext3D*, unsigned sourceTextureId, unsigned destTextureId, const IntSize&) = 0;

protected:
    virtual ~TextureCopier() { }
};

#if USE(ACCELERATED_COMPOSITING)

class AcceleratedTextureCopier : public TextureCopier {
    WTF_MAKE_NONCOPYABLE(AcceleratedTextureCopier);
public:
    static PassOwnPtr<AcceleratedTextureCopier> create(PassRefPtr<GraphicsContext3D> context)
    {
        return adoptPtr(new AcceleratedTextureCopier(context));
    }
    virtual ~AcceleratedTextureCopier();

    virtual void copyTexture(GraphicsContext3D*, unsigned sourceTextureId, unsigned destTextureId, const IntSize&);

protected:
    explicit AcceleratedTextureCopier(PassRefPtr<GraphicsContext3D>);

private:
    typedef ProgramBinding<VertexShaderPosTexIdentity, FragmentShaderRGBATex> BlitProgram;

    RefPtr<GraphicsContext3D> m_context;
    Platform3DObject m_fbo;
    Platform3DObject m_positionBuffer;
    OwnPtr<BlitProgram> m_blitProgram;
};

#endif // USE(ACCELERATED_COMPOSITING)

}

#endif
