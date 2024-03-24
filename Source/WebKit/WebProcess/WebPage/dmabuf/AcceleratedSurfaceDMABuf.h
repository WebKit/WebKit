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

#if (PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))) && USE(EGL)

#include "AcceleratedSurface.h"

#include "MessageReceiver.h"
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
#include "DMABufRendererBufferFormat.h"
#include <atomic>
#include <wtf/Lock.h>
typedef void *EGLImage;
struct gbm_bo;
#endif

namespace WebCore {
class ShareableBitmap;
class ShareableBitmapHandle;
}

namespace WebKit {

class WebPage;

class AcceleratedSurfaceDMABuf final : public AcceleratedSurface, public IPC::MessageReceiver {
public:
    static std::unique_ptr<AcceleratedSurfaceDMABuf> create(WebPage&, Client&);
    ~AcceleratedSurfaceDMABuf();

private:
    uint64_t window() const override { return 0; }
    uint64_t surfaceID() const override;
    void clientResize(const WebCore::IntSize&) override;
    bool shouldPaintMirrored() const override
    {
#if PLATFORM(WPE) || (PLATFORM(GTK) && USE(GTK4))
        return false;
#else
        return true;
#endif
    }
    void didCreateGLContext() override;
    void willDestroyGLContext() override;
    void willRenderFrame() override;
    void didRenderFrame() override;

    void didCreateCompositingRunLoop(WTF::RunLoop&) override;
    void willDestroyCompositingRunLoop() override;

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange() override;
#endif

    void visibilityDidChange(bool) override;

    AcceleratedSurfaceDMABuf(WebPage&, Client&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void releaseBuffer(uint64_t);
    void frameDone();
    void releaseUnusedBuffersTimerFired();

    class RenderTarget {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~RenderTarget();

        uint64_t id() const { return m_id; }

        virtual void willRenderFrame() const;
        virtual void didRenderFrame() { };

    protected:
        RenderTarget(uint64_t, const WebCore::IntSize&);

        uint64_t m_id { 0 };
        uint64_t m_surfaceID { 0 };
        unsigned m_depthStencilBuffer { 0 };
    };

    class RenderTargetColorBuffer : public RenderTarget {
    protected:
        RenderTargetColorBuffer(uint64_t, const WebCore::IntSize&);
        virtual ~RenderTargetColorBuffer();

        void willRenderFrame() const final;

        unsigned m_colorBuffer { 0 };
    };

#if USE(GBM)
    class RenderTargetEGLImage final : public RenderTargetColorBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(uint64_t, const WebCore::IntSize&, const DMABufRendererBufferFormat&);
        RenderTargetEGLImage(uint64_t, const WebCore::IntSize&, EGLImage, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~RenderTargetEGLImage();

    private:
        EGLImage m_image { nullptr };
    };
#endif

    class RenderTargetSHMImage final : public RenderTargetColorBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(uint64_t, const WebCore::IntSize&);
        RenderTargetSHMImage(uint64_t, const WebCore::IntSize&, Ref<WebCore::ShareableBitmap>&&, WebCore::ShareableBitmapHandle&&);
        ~RenderTargetSHMImage() = default;

    private:
        void didRenderFrame() override;

        Ref<WebCore::ShareableBitmap> m_bitmap;
    };

    class RenderTargetTexture final : public RenderTarget {
    public:
        static std::unique_ptr<RenderTarget> create(uint64_t, const WebCore::IntSize&);
        RenderTargetTexture(uint64_t, const WebCore::IntSize&, unsigned texture, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~RenderTargetTexture();

    private:
        void willRenderFrame() const override;

        unsigned m_texture { 0 };
    };

    class SwapChain {
        WTF_MAKE_NONCOPYABLE(SwapChain);
    public:
        explicit SwapChain(uint64_t);
        ~SwapChain() = default;

        enum class Type {
            Invalid,
#if USE(GBM)
            EGLImage,
#endif
            SharedMemory,
            Texture
        };

        Type type() const { return m_type; }
        void resize(const WebCore::IntSize&);
        RenderTarget* nextTarget();
        void releaseTarget(uint64_t);
        void reset();
        void releaseUnusedBuffers();

        unsigned size() const { return m_freeTargets.size() + m_lockedTargets.size(); }

#if USE(GBM)
        void setupBufferFormat(const Vector<DMABufRendererBufferFormat>&);
#endif

    private:
        static constexpr unsigned s_maximumBuffers = 3;

        std::unique_ptr<RenderTarget> createTarget() const;

        uint64_t m_surfaceID { 0 };
        Type m_type { Type::Invalid };
        WebCore::IntSize m_size;
        Vector<std::unique_ptr<RenderTarget>, s_maximumBuffers> m_freeTargets;
        Vector<std::unique_ptr<RenderTarget>, s_maximumBuffers> m_lockedTargets;
#if USE(GBM)
        Lock m_dmabufFormatLock;
        DMABufRendererBufferFormat m_dmabufFormat WTF_GUARDED_BY_LOCK(m_dmabufFormatLock);
        bool m_dmabufFormatChanged WTF_GUARDED_BY_LOCK(m_dmabufFormatLock) { false };
#endif
    };

    uint64_t m_id { 0 };
    unsigned m_fbo { 0 };
    SwapChain m_swapChain;
    RenderTarget* m_target { nullptr };
    bool m_isVisible { false };
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedBuffersTimer;
};

} // namespace WebKit

#endif // (PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))) && USE(EGL)
