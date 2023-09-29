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

#if USE(EGL)

#include "AcceleratedSurface.h"

#include "MessageReceiver.h"
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
typedef void *EGLImage;
struct gbm_bo;
#endif

namespace WebKit {

class ShareableBitmap;
class ShareableBitmapHandle;
class WebPage;

class AcceleratedSurfaceDMABuf final : public AcceleratedSurface, public IPC::MessageReceiver {
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

    void didCreateCompositingRunLoop(WTF::RunLoop&) override;
    void willDestroyCompositingRunLoop() override;

private:
    AcceleratedSurfaceDMABuf(WebPage&, Client&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void frameDone();

    class RenderTarget {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~RenderTarget();

        virtual void willRenderFrame() const;
        virtual void didRenderFrame() { };
        virtual void didDisplayFrame() { };

    protected:
        explicit RenderTarget(const WebCore::IntSize&);

        unsigned m_depthStencilBuffer { 0 };
    };

    class RenderTargetColorBuffer : public RenderTarget {
    protected:
        explicit RenderTargetColorBuffer(const WebCore::IntSize&);
        virtual ~RenderTargetColorBuffer();

        void willRenderFrame() const final;
        void didRenderFrame() override;
        void didDisplayFrame() override;

        unsigned m_backColorBuffer { 0 };
        unsigned m_frontColorBuffer { 0 };
        unsigned m_displayColorBuffer { 0 };
    };

#if USE(GBM)
    class RenderTargetEGLImage final : public RenderTargetColorBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(uint64_t, const WebCore::IntSize&);
        RenderTargetEGLImage(uint64_t, const WebCore::IntSize&, EGLImage, WTF::UnixFileDescriptor&&, EGLImage, WTF::UnixFileDescriptor&&, EGLImage, WTF::UnixFileDescriptor&&, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier);
        ~RenderTargetEGLImage();

    private:
        void didRenderFrame() override;
        void didDisplayFrame() override;

        EGLImage m_backImage { nullptr };
        EGLImage m_frontImage { nullptr };
        EGLImage m_displayImage { nullptr };
    };
#endif

    class RenderTargetSHMImage final : public RenderTargetColorBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(uint64_t, const WebCore::IntSize&);
        RenderTargetSHMImage(uint64_t, const WebCore::IntSize&, Ref<ShareableBitmap>&&, ShareableBitmapHandle&&, Ref<ShareableBitmap>&&, ShareableBitmapHandle&&, Ref<ShareableBitmap>&&, ShareableBitmapHandle&&);
        ~RenderTargetSHMImage() = default;

    private:
        void didRenderFrame() override;
        void didDisplayFrame() override;

        Ref<ShareableBitmap> m_backBitmap;
        Ref<ShareableBitmap> m_frontBitmap;
        Ref<ShareableBitmap> m_displayBitmap;
    };

    class RenderTargetTexture final : public RenderTarget {
    public:
        static std::unique_ptr<RenderTarget> create(uint64_t, const WebCore::IntSize&);
        RenderTargetTexture(uint64_t, const WebCore::IntSize&, WTF::UnixFileDescriptor&&, unsigned, WTF::UnixFileDescriptor&&, unsigned, WTF::UnixFileDescriptor&&, unsigned, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier);
        ~RenderTargetTexture();

    private:
        void willRenderFrame() const override;
        void didRenderFrame() override;
        void didDisplayFrame() override;

        unsigned m_backTexture { 0 };
        unsigned m_frontTexture { 0 };
        unsigned m_displayTexture { 0 };
    };

    uint64_t m_id { 0 };
    unsigned m_fbo { 0 };
    std::unique_ptr<RenderTarget> m_target;
};

} // namespace WebKit

#endif // USE(EGL)
