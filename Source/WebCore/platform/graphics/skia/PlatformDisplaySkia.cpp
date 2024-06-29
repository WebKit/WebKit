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
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/gl/GrGLInterface.h>
#include <skia/gpu/gl/GrGLTypes.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/threads/BinarySemaphore.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
IGNORE_CLANG_WARNINGS_END

#if USE(LIBEPOXY)
#include <skia/gpu/gl/epoxy/GrGLMakeEpoxyEGLInterface.h>
#else
#include <skia/gpu/gl/egl/GrGLMakeEGLInterface.h>
#endif

namespace WebCore {

#if !(PLATFORM(PLAYSTATION) && USE(COORDINATED_GRAPHICS))
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

static thread_local RefPtr<SkiaGLContext> s_skiaGLContext;

class SkiaGLContext : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<SkiaGLContext> {
public:
    static Ref<SkiaGLContext> create(PlatformDisplay& display)
    {
        return adoptRef(*new SkiaGLContext(display));
    }

    ~SkiaGLContext() = default;

    void invalidate()
    {
        if (&RunLoop::current() == m_runLoop) {
            invalidateOnCurrentThread();
            return;
        }

        BinarySemaphore semaphore;
        m_runLoop->dispatch([&semaphore, this] {
            invalidateOnCurrentThread();
            semaphore.signal();
        });
        semaphore.wait();
    }

    GLContext* skiaGLContext() const
    {
        Locker locker { m_lock };
        return m_skiaGLContext.get();
    }

    GrDirectContext* skiaGrContext() const
    {
        Locker locker { m_lock };
        return m_skiaGrContext.get();
    }

private:
    explicit SkiaGLContext(PlatformDisplay& display)
        : m_runLoop(&RunLoop::current())
    {
        auto glContext = GLContext::createOffscreen(display);
        if (!glContext || !glContext->makeContextCurrent())
            return;

        // FIXME: add GrContextOptions, shader cache, etc.
        if (auto grContext = GrDirectContexts::MakeGL(skiaGLInterface())) {
            m_skiaGLContext = WTFMove(glContext);
            m_skiaGrContext = WTFMove(grContext);
        }
    }

    void invalidateOnCurrentThread()
    {
        Locker locker { m_lock };
        m_skiaGrContext = nullptr;
        m_skiaGLContext = nullptr;
    }

    RunLoop* m_runLoop { nullptr };
    std::unique_ptr<GLContext> m_skiaGLContext WTF_GUARDED_BY_LOCK(m_lock);
    sk_sp<GrDirectContext> m_skiaGrContext WTF_GUARDED_BY_LOCK(m_lock);
    mutable Lock m_lock;
};
#endif

GLContext* PlatformDisplay::skiaGLContext()
{
#if !(PLATFORM(PLAYSTATION) && USE(COORDINATED_GRAPHICS))
    if (!s_skiaGLContext) {
        s_skiaGLContext = SkiaGLContext::create(*this);
        m_skiaGLContexts.add(*s_skiaGLContext);
    }
    return s_skiaGLContext->skiaGLContext();
#else
    // The PlayStation OpenGL implementation does not dispatch to the context bound to
    // the current thread so Skia cannot use OpenGL with coordinated graphics.
    return nullptr;
#endif
}

GrDirectContext* PlatformDisplay::skiaGrContext()
{
    RELEASE_ASSERT(s_skiaGLContext);
    return s_skiaGLContext->skiaGrContext();
}

void PlatformDisplay::invalidateSkiaGLContexts()
{
    auto contexts = WTFMove(m_skiaGLContexts);
    contexts.forEach([](auto& context) {
        context.invalidate();
    });
}

} // namespace WebCore

#endif // USE(SKIA)
