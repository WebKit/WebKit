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

#if USE(COORDINATED_GRAPHICS)

#include <WebCore/ColorComponents.h>
#include <WebCore/ColorModels.h>
#include <WebCore/CoordinatedCompositionReason.h>
#include <WebCore/Damage.h>
#include <WebCore/IntSize.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakRef.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(WPE_RENDERER)
struct wpe_renderer_backend_egl_target;
#endif

namespace WTF {
class RunLoop;
}

namespace WebCore {
class GLFence;
class GraphicsContext;
class ShareableBitmap;
class ShareableBitmapHandle;
}

namespace WebKit {
class AcceleratedSurface;
}

namespace WebKit {
class WebPage;

class AcceleratedSurface final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AcceleratedSurface, WTF::DestructionThread::MainRunLoop> {
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedSurface);
public:
    static Ref<AcceleratedSurface> create(WebPage&, Function<void()>&& frameCompleteHandler);
    ~AcceleratedSurface();

    using ColorComponents = WebCore::ColorComponents<float, 4>;

    uint64_t window() const;
    uint64_t surfaceID() const;
    bool shouldPaintMirrored() const { return true; }

    void willDestroyGLContext();
    void willRenderFrame(const WebCore::IntSize&);
    void didRenderFrame();
    void clear(const OptionSet<WebCore::CompositionReason>&);

    void didCreateCompositingRunLoop(WTF::RunLoop&);
    void willDestroyCompositingRunLoop();

    void visibilityDidChange(bool);
    void backgroundColorDidChange();

private:
    AcceleratedSurface(WebPage&, Function<void()>&& frameCompleteHandler);

    void frameDone();
    void releaseUnusedBuffersTimerFired();

    class RenderTarget {
        WTF_MAKE_TZONE_ALLOCATED(RenderTarget);
    public:
        virtual ~RenderTarget();

        uint64_t id() const { return m_id; }

        virtual void willRenderFrame() { }
        virtual void didRenderFrame(Vector<WebCore::IntRect, 1>&&) { }

        virtual void sync(bool) { }
        virtual void setReleaseFenceFD(UnixFileDescriptor&&) { }

    protected:
        RenderTarget();

        uint64_t m_id { 0 };
    };

#if USE(WPE_RENDERER)
    class RenderTargetWPEBackend final : public RenderTarget {
    public:
        static std::unique_ptr<RenderTarget> create(const WebCore::IntSize&, UnixFileDescriptor&&, const AcceleratedSurface&);
        RenderTargetWPEBackend(const WebCore::IntSize&, UnixFileDescriptor&&, const AcceleratedSurface&);
        ~RenderTargetWPEBackend();

        uint64_t window() const;
        void resize(const WebCore::IntSize&);

    private:
        void willRenderFrame() override;
        void didRenderFrame(Vector<WebCore::IntRect, 1>&&) override;

        struct wpe_renderer_backend_egl_target* m_backend { nullptr };
    };
#endif

    class SwapChain {
        WTF_MAKE_NONCOPYABLE(SwapChain);
    public:
        SwapChain();

        enum class Type {
            Invalid,
#if USE(WPE_RENDERER)
            WPEBackend
#endif
        };

        Type type() const { return m_type; }
        bool resize(const WebCore::IntSize&);
        const WebCore::IntSize& size() const { return m_size; }
        RenderTarget* nextTarget();
        void releaseTarget(uint64_t, UnixFileDescriptor&& releaseFence);
        void reset();
        void releaseUnusedBuffers();

#if USE(WPE_RENDERER)
        void initialize(WebPage&);
        uint64_t initializeTarget(const AcceleratedSurface&);
#endif

    private:
        // FIXME: Allow configuring the initial buffer count, e.g. for triple buffering.
        static constexpr unsigned s_initialBuffers = 2;
        static constexpr unsigned s_maximumBuffers = 4;

        std::unique_ptr<RenderTarget> createTarget() const;

        Type m_type { Type::Invalid };
        WebCore::IntSize m_size;
        Vector<std::unique_ptr<RenderTarget>, s_maximumBuffers> m_freeTargets;
        Vector<std::unique_ptr<RenderTarget>, s_maximumBuffers> m_lockedTargets;
#if USE(WPE_RENDERER)
        UnixFileDescriptor m_hostFD;
        WebCore::IntSize m_initialSize;
#endif
    };

    static constexpr ColorComponents white { 1.f, 1.f, 1.f, WebCore::AlphaTraits<float>::opaque };

    WeakRef<WebPage> m_webPage;
    Function<void()> m_frameCompleteHandler;
    uint64_t m_id { 0 };
    WebCore::IntSize m_size;
    SwapChain m_swapChain;
    RenderTarget* m_target { nullptr };
    bool m_isVisible { false };
    bool m_useExplicitSync { false };
    std::atomic<ColorComponents> m_backgroundColor { white };
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedBuffersTimer;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
