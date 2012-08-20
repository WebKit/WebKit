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
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {
class IntSize;

class TextureCopier {
public:
    struct Parameters {
        unsigned sourceTexture;
        unsigned destTexture;
        IntSize size;
    };
    // Copy the base level contents of |sourceTexture| to |destTexture|. Both texture objects
    // must be complete and have a base level of |size| dimensions. The color formats do not need
    // to match, but |destTexture| must have a renderable format.
    virtual void copyTexture(Parameters) = 0;
    virtual void flush() = 0;

    virtual ~TextureCopier() { }
};

#if USE(ACCELERATED_COMPOSITING)

class AcceleratedTextureCopier : public TextureCopier {
    WTF_MAKE_NONCOPYABLE(AcceleratedTextureCopier);
public:
    static PassOwnPtr<AcceleratedTextureCopier> create(WebKit::WebGraphicsContext3D* context, bool usingBindUniforms)
    {
        return adoptPtr(new AcceleratedTextureCopier(context, usingBindUniforms));
    }
    virtual ~AcceleratedTextureCopier();

    virtual void copyTexture(Parameters) OVERRIDE;
    virtual void flush() OVERRIDE;

protected:
    AcceleratedTextureCopier(WebKit::WebGraphicsContext3D*, bool usingBindUniforms);

private:
    typedef ProgramBinding<VertexShaderPosTexIdentity, FragmentShaderRGBATex> BlitProgram;

    WebKit::WebGraphicsContext3D* m_context;
    Platform3DObject m_fbo;
    Platform3DObject m_positionBuffer;
    OwnPtr<BlitProgram> m_blitProgram;
    bool m_usingBindUniforms;
};

#endif // USE(ACCELERATED_COMPOSITING)

}

#endif
