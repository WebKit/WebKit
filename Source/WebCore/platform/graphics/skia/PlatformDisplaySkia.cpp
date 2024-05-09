/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "PlatformDisplay.h"

#if USE(SKIA)
#include "GLContext.h"
#include <skia/gpu/GrBackendSurface.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
IGNORE_CLANG_WARNINGS_END

#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/gl/GrGLInterface.h>
#include <skia/gpu/gl/GrGLTypes.h>
#include <wtf/NeverDestroyed.h>

#if USE(LIBEPOXY)
#include <skia/gpu/gl/epoxy/GrGLMakeEpoxyEGLInterface.h>
#else
#include <skia/gpu/gl/egl/GrGLMakeEGLInterface.h>
#endif

namespace WebCore {

static sk_sp<const GrGLInterface> skiaGLInterface()
{
    static NeverDestroyed<sk_sp<const GrGLInterface>> interface {
#if USE(LIBEPOXY)
        GrGLInterfaces::MakeEpoxyEGL()
#else
        GrGLInterfaces::MakeEGL()
#endif
    };

    return interface.get();
}

GLContext* PlatformDisplay::skiaGLContext()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [this] {
        // The PlayStation OpenGL implementation does not dispatch to the context bound to
        //  the current thread so Skia cannot use OpenGL with coordinated graphics
#if !(PLATFORM(PLAYSTATION) && USE(COORDINATED_GRAPHICS))
        const char* enableCPURendering = getenv("WEBKIT_SKIA_ENABLE_CPU_RENDERING");
        if (enableCPURendering && strcmp(enableCPURendering, "0"))
            return;

        auto skiaGLContext = GLContext::createOffscreen(*this);
        if (!skiaGLContext || !skiaGLContext->makeContextCurrent())
            return;

        // FIXME: add GrContextOptions, shader cache, etc.
        if (auto skiaGrContext = GrDirectContexts::MakeGL(skiaGLInterface())) {
            m_skiaGLContext = WTFMove(skiaGLContext);
            m_skiaGrContext = WTFMove(skiaGrContext);
        }
#endif
    });
    return m_skiaGLContext.get();
}

} // namespace WebCore

#endif // USE(SKIA)
