/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL) && USE(NICOSIA) && USE(TEXTURE_MAPPER)

#include "NicosiaContentLayerTextureMapperImpl.h"
#include <memory>

#if USE(ANGLE)
#include "GraphicsContextGLANGLE.h"
#else
#include "GraphicsContextGLOpenGL.h"
#endif

namespace WebCore {
class GLContext;
}

namespace Nicosia {

class GCGLLayer : public ContentLayerTextureMapperImpl::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
#if USE(ANGLE)
    using GraphicsContextGLType = WebCore::GraphicsContextGLANGLE;
#else
    using GraphicsContextGLType = WebCore::GraphicsContextGLOpenGL;
#endif
    explicit GCGLLayer(GraphicsContextGLType&);

    virtual ~GCGLLayer();

    ContentLayer& contentLayer() const { return m_contentLayer; }
    virtual bool makeContextCurrent();
    virtual PlatformGraphicsContextGL platformContext() const;

    void swapBuffersIfNeeded() override;

private:
    GraphicsContextGLType& m_context;
    std::unique_ptr<WebCore::GLContext> m_glContext;

    Ref<ContentLayer> m_contentLayer;
};

} // namespace Nicosia

#endif // ENABLE(WEBGL) && USE(NICOSIA) && USE(TEXTURE_MAPPER)
