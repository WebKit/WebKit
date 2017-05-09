/*
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(WPE)

#include "PlatformDisplay.h"

// FIXME: For now default to the GBM EGL platform, but this should really be
// somehow deducible from the build configuration.
#define __GBM__ 1
#include <EGL/eglplatform.h>

struct wpe_renderer_backend_egl;
struct wpe_renderer_backend_egl_target;
struct wpe_renderer_backend_egl_offscreen_target;

namespace WebCore {

class GLContext;
class IntSize;

class PlatformDisplayWPE final : public PlatformDisplay {
public:
    PlatformDisplayWPE();
    virtual ~PlatformDisplayWPE();

    void initialize(int);

    class EGLTarget {
    public:
        class Client {
        public:
            virtual void frameComplete() = 0;
        };

        EGLTarget(const PlatformDisplayWPE&, Client&, int);
        ~EGLTarget();

        void initialize(const IntSize&);
        EGLNativeWindowType nativeWindow() const;

        void resize(const IntSize&);

        void frameWillRender();
        void frameRendered();

    private:
        const PlatformDisplayWPE& m_display;
        Client& m_client;
        struct wpe_renderer_backend_egl_target* m_backend;
    };

    class EGLOffscreenTarget {
    public:
        EGLOffscreenTarget(const PlatformDisplayWPE&);
        ~EGLOffscreenTarget();

        EGLNativeWindowType nativeWindow() const;

    private:
        struct wpe_renderer_backend_egl_offscreen_target* m_target;
    };

    std::unique_ptr<EGLTarget> createEGLTarget(EGLTarget::Client&, int);
    std::unique_ptr<EGLOffscreenTarget> createEGLOffscreenTarget();

private:
    Type type() const override { return PlatformDisplay::Type::WPE; }

    struct wpe_renderer_backend_egl* m_backend;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(PlatformDisplayWPE, WPE)

#endif // PLATFORM(WPE)
