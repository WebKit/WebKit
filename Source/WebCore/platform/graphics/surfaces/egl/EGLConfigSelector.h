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

#ifndef EGLConfigSelector_h
#define EGLConfigSelector_h

#if USE(EGL)

#include <opengl/GLDefs.h>
#include <opengl/GLPlatformSurface.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

typedef Display NativeSharedDisplay;

class SharedEGLDisplay : public WTF::RefCountedBase {
    WTF_MAKE_NONCOPYABLE(SharedEGLDisplay);

public:
    static PassRefPtr<SharedEGLDisplay> create(NativeSharedDisplay* display)
    {
        if (!m_staticSharedEGLDisplay)
            m_staticSharedEGLDisplay = new SharedEGLDisplay(display);
        else
            m_staticSharedEGLDisplay->ref();

        return adoptRef(m_staticSharedEGLDisplay);
    }

    void deref();
    EGLDisplay sharedEGLDisplay() const;

protected:
    SharedEGLDisplay(NativeSharedDisplay*);
    void cleanup();
    virtual ~SharedEGLDisplay();

    static SharedEGLDisplay* m_staticSharedEGLDisplay;
    EGLDisplay m_eglDisplay;
};

class EGLConfigSelector {
    WTF_MAKE_NONCOPYABLE(EGLConfigSelector);

public:
    EGLConfigSelector(GLPlatformSurface::SurfaceAttributes, NativeSharedDisplay* = 0);
    virtual ~EGLConfigSelector();
    PlatformDisplay display() const;
    virtual EGLConfig pBufferContextConfig();
    virtual EGLConfig surfaceContextConfig();
    EGLint nativeVisualId(const EGLConfig&) const;
    GLPlatformSurface::SurfaceAttributes attributes() const;
    void reset();

private:
    EGLConfig createConfig(const int attributes[]);

protected:
    EGLConfig m_pbufferFBConfig;
    EGLConfig m_surfaceContextFBConfig;
    RefPtr<SharedEGLDisplay> m_sharedDisplay;
    unsigned m_attributes : 3;
};

}

#endif

#endif
