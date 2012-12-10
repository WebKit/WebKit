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

#ifndef GLPlatformSurface_h
#define GLPlatformSurface_h

#if USE(ACCELERATED_COMPOSITING)

#include "GLDefs.h"
#include "IntRect.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

// Encapsulates a surface that can be rendered to with GL, hiding platform
// specific management.
namespace WebCore {

class GLPlatformSurface {
    WTF_MAKE_NONCOPYABLE(GLPlatformSurface);

public:
    // Creates a GL surface used for offscreen rendering.
    static PassOwnPtr<GLPlatformSurface> createOffscreenSurface();

    // Creates a GL surface used for offscreen rendering. The results can be transported
    // to the UI process for display.
    static PassOwnPtr<GLPlatformSurface> createTransportSurface();

    virtual ~GLPlatformSurface();

    const IntRect& geometry() const;

    // Creates FBO used by the surface. Buffers can be bound to this FBO.
    void initialize(GLuint* frameBufferId);

    // Get the underlying platform specific surface handle.
    PlatformSurface handle() const;

    PlatformDisplay sharedDisplay() const;

    virtual void swapBuffers();

    // Convenience Function to update surface contents.
    // Function does the following(in order):
    // a) Blits back buffer contents to front buffer.
    // b) Calls Swap Buffers.
    // c) Sets current FBO as bindFboId.
    virtual void updateContents(const GLuint bindFboId);

    virtual void setGeometry(const IntRect& newRect);

    virtual PlatformSurfaceConfig configuration();

    virtual void destroy();

protected:
    GLPlatformSurface();
    bool m_restoreNeeded;
    IntRect m_rect;
    GLuint m_fboId;
    PlatformDisplay m_sharedDisplay;
    PlatformSurface m_drawable;
};

}

#endif

#endif
