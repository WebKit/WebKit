/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AcceleratedSurface.h"

#if USE(GBM)
#include <wtf/WeakPtr.h>

typedef void *EGLImage;
struct gbm_bo;

namespace WebKit {

class WebPage;

class AcceleratedSurfaceDMABuf final : public AcceleratedSurface, public CanMakeWeakPtr<AcceleratedSurfaceDMABuf> {
public:
    static std::unique_ptr<AcceleratedSurfaceDMABuf> create(WebPage&, Client&);
    ~AcceleratedSurfaceDMABuf();

    uint64_t window() const override { return 0; }
    uint64_t surfaceID() const override;
    void clientResize(const WebCore::IntSize&) override;
    bool shouldPaintMirrored() const override
    {
#if PLATFORM(GTK) && USE(GTK4)
        return false;
#else
        return true;
#endif
    }
    void didCreateGLContext() override;
    void willDestroyGLContext() override;
    void willRenderFrame() override;
    void didRenderFrame() override;

private:
    AcceleratedSurfaceDMABuf(WebPage&, Client&);

    unsigned m_texture { 0 };
    unsigned m_fbo { 0 };
    unsigned m_depthStencilBuffer { 0 };
    EGLImage m_backImage { nullptr };
    EGLImage m_frontImage { nullptr };
};

} // namespace WebKit

#endif // USE(GBM)
