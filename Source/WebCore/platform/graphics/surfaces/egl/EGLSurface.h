/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EGLSurface_h
#define EGLSurface_h

#if USE(EGL) && USE(GRAPHICS_SURFACE)

#include "GLTransportSurface.h"

#include <wtf/Noncopyable.h>

namespace WebCore {

class EGLConfigSelector;

class EGLTransportSurface : public GLTransportSurface {
public:
    static std::unique_ptr<GLTransportSurface> createTransportSurface(const IntSize&, SurfaceAttributes);
    static std::unique_ptr<GLTransportSurfaceClient> createTransportSurfaceClient(const PlatformBufferHandle, const IntSize&, bool);
    virtual ~EGLTransportSurface();
    PlatformSurfaceConfig configuration() override;
    void destroy() override;
    GLPlatformSurface::SurfaceAttributes attributes() const override;
    bool isCurrentDrawable() const override;

protected:
    EGLTransportSurface(const IntSize&, SurfaceAttributes);
    std::unique_ptr<EGLConfigSelector> m_configSelector;
};

class EGLOffScreenSurface : public GLPlatformSurface {
public:
    static std::unique_ptr<GLPlatformSurface> createOffScreenSurface(SurfaceAttributes);
    virtual ~EGLOffScreenSurface();
    PlatformSurfaceConfig configuration() override;
    void destroy() override;
    GLPlatformSurface::SurfaceAttributes attributes() const override;
    bool isCurrentDrawable() const override;

protected:
    EGLOffScreenSurface(SurfaceAttributes);
    std::unique_ptr<EGLConfigSelector> m_configSelector;
};

}

#endif

#endif
