/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
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

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#include "NicosiaContentLayerTextureMapperImpl.h"

typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;

namespace WebCore {
class IntSize;
class GraphicsContextGLANGLE;
class GraphicsContextGLFallback;
#if USE(LIBGBM)
class GraphicsContextGLGBM;
#endif
class PlatformDisplay;
}

namespace Nicosia {

class GCGLANGLELayer final : public ContentLayerTextureMapperImpl::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GCGLANGLELayer(WebCore::GraphicsContextGLFallback&);
#if USE(LIBGBM)
    GCGLANGLELayer(WebCore::GraphicsContextGLGBM&);
#endif
    virtual ~GCGLANGLELayer();

    ContentLayer& contentLayer() const { return m_contentLayer; }
    void swapBuffersIfNeeded() final;

private:
    enum class ContextType {
        Fallback,
        Gbm,
    };
    ContextType m_contextType;

    WebCore::GraphicsContextGLANGLE& m_context;
    Ref<ContentLayer> m_contentLayer;
};

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER) && USE(LIBGBM)
