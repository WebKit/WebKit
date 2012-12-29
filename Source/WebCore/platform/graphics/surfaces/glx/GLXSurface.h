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

#ifndef GLXSurface_h
#define GLXSurface_h

#if USE(ACCELERATED_COMPOSITING) && USE(GLX)

#include "GLPlatformSurface.h"
#include "GLXConfigSelector.h"
#include "X11WindowResources.h"

namespace WebCore {

#if USE(GRAPHICS_SURFACE)
class GLXTransportSurface : public GLPlatformSurface {

public:
    GLXTransportSurface();
    virtual ~GLXTransportSurface();
    virtual PlatformSurfaceConfig configuration() OVERRIDE;
    virtual void swapBuffers() OVERRIDE;
    virtual void setGeometry(const IntRect&) OVERRIDE;
    virtual void destroy() OVERRIDE;

private:
    void initialize();
    OwnPtr<X11OffScreenWindow> m_nativeResource;
    OwnPtr<GLXConfigSelector> m_configSelector;
};
#endif

class GLXPBuffer : public GLPlatformSurface {

public:
    GLXPBuffer();
    virtual ~GLXPBuffer();
    virtual PlatformSurfaceConfig configuration() OVERRIDE;
    virtual void setGeometry(const IntRect&) OVERRIDE;
    virtual void destroy() OVERRIDE;

private:
    void initialize();
    void freeResources();
    OwnPtr<X11OffScreenWindow> m_nativeResource;
    OwnPtr<GLXConfigSelector> m_configSelector;
};

}

#endif

#endif
