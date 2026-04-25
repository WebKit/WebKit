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

#include "MessageReceiver.h"
#include <WebCore/Color.h>
#include <WebCore/CoordinatedCompositionReason.h>
#include <WebCore/Damage.h>
#include <WebCore/IntSize.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/CheckedRef.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakRef.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM) || OS(ANDROID)
#include "RendererBufferFormat.h"
#include <atomic>
#endif

#if USE(GBM)
#include <WebCore/DMABufBuffer.h>
#include <WebCore/DRMDevice.h>
#include <WebCore/GBMDevice.h>
struct gbm_bo;
#endif

#if OS(ANDROID)
typedef struct AHardwareBuffer AHardwareBuffer;
#endif

#if USE(GBM) || OS(ANDROID)
typedef void *EGLImage;
#endif

#if USE(WPE_RENDERER)
struct wpe_renderer_backend_egl_target;
#endif

namespace WTF {
class RunLoop;
}

namespace WebCore {
class BitmapTexture;
class GLFence;
class ShareableBitmap;
class ShareableBitmapHandle;
}

namespace WebKit {
class AcceleratedSurface;
}

namespace WebKit {
class WebPage;

class AcceleratedSurface final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AcceleratedSurface, WTF::DestructionThread::MainRunLoop>, public CanMakeThreadSafeCheckedPtr<AcceleratedSurface>
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    , public IPC::MessageReceiver
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedSurface);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AcceleratedSurface);
public:
    enum class RenderingPurpose {
        Composited,
        NonComposited,
    };

    static Ref<AcceleratedSurface> create(WebPage&, Function<void()>&& frameCompleteHandler, RenderingPurpose, bool useSkia);
    ~AcceleratedSurface();

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }
#endif

    uint64_t window();
    uint64_t surfaceID() const { return m_id; }
    bool shouldPaintMirrored() const
    {
#if PLATFORM(WPE) || (PLATFORM(GTK) && USE(GTK4))
        return false;
#else
        return true;
#endif
    }

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    bool usesGL() const { return m_renderingPurpose == RenderingPurpose::Composited || m_hardwareAccelerationEnabled; }
#elif PLATFORM(WPE)
    constexpr bool usesGL() const { return true; }
#endif

    SkCanvas* canvas();

    void willDestroyGLContext();
    void willRenderFrame(const WebCore::IntSize&);
    void didRenderFrame();
    void sendFrame();
    void clear(const OptionSet<WebCore::CompositionReason>&);

#if ENABLE(DAMAGE_TRACKING)
    void setFrameDamage(WebCore::Damage&&);
    const std::optional<WebCore::Damage>& frameDamage() const LIFETIME_BOUND { return m_frameDamage; }
    const std::optional<WebCore::Damage>& renderTargetDamage();
#endif

    void didCreateCompositingRunLoop(WTF::RunLoop&);
    void willDestroyCompositingRunLoop();

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
    void preferredBufferFormatsDidChange();
#endif

    void visibilityDidChange(bool);
    void backgroundColorDidChange();

private:
    AcceleratedSurface(WebPage&, Function<void()>&& frameCompleteHandler, RenderingPurpose, bool useSkia);

    RenderingPurpose renderingPurpose() const { return m_renderingPurpose; }
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    bool hardwareAccelerationEnabled() const { return m_hardwareAccelerationEnabled; }
#endif
    bool useSkia() const { return m_useSkia; }
    bool isOpaque() const;

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void releaseBuffer(uint64_t, WTF::UnixFileDescriptor&&);
#endif
    void frameDone();
    void releaseUnusedBuffersTimerFired();

    class RenderTarget {
        WTF_MAKE_TZONE_ALLOCATED(RenderTarget);
    public:
        virtual ~RenderTarget();

        uint64_t id() const { return m_id; }

        virtual void willRenderFrame() { }
        virtual void didRenderFrame() { }
        virtual void sendFrame(Vector<WebCore::IntRect, 1>&&) { };

        virtual void sync(bool) { }
        virtual void setReleaseFenceFD(UnixFileDescriptor&&) { }

        SkSurface* skiaSurface() const { return m_skiaSurface.get(); }

#if ENABLE(DAMAGE_TRACKING)
        void setDamage(WebCore::Damage&& damage) { m_damage = WTF::move(damage); }
        const std::optional<WebCore::Damage>& damage() LIFETIME_BOUND { return m_damage; }
        void addDamage(const std::optional<WebCore::Damage>&);
#endif

    protected:
        explicit RenderTarget(AcceleratedSurface&);

        void createSkiaSurfaceForTexture(const WebCore::BitmapTexture&);

        uint64_t m_id { 0 };
        const CheckedRef<AcceleratedSurface> m_surface;
        sk_sp<SkSurface> m_skiaSurface;
#if ENABLE(DAMAGE_TRACKING)
        std::optional<WebCore::Damage> m_damage;
#endif
    };

#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    class RenderTargetShareableBuffer : public RenderTarget {
        WTF_MAKE_TZONE_ALLOCATED(RenderTargetShareableBuffer);
    public:
        virtual ~RenderTargetShareableBuffer();

    private:
        std::unique_ptr<WebCore::GLFence> createRenderingFence(bool) const;

    protected:
        RenderTargetShareableBuffer(AcceleratedSurface&, const WebCore::IntSize&);

        void willRenderFrame() override;
        void sendFrame(Vector<WebCore::IntRect, 1>&&) override;

        virtual bool supportsExplicitSync() const = 0;
        void sync(bool) override;
        void setReleaseFenceFD(UnixFileDescriptor&&) override;

        unsigned m_fbo { 0 };
        unsigned m_depthStencilBuffer { 0 };
        UnixFileDescriptor m_renderingFenceFD;
        UnixFileDescriptor m_releaseFenceFD;
        WebCore::IntSize m_initialSize;
    };

#if USE(GBM) || OS(ANDROID)
    struct BufferFormat {
        BufferFormat() = default;
        BufferFormat(const BufferFormat&) = delete;
        BufferFormat& operator=(const BufferFormat&) = delete;
        BufferFormat(BufferFormat&& other)
        {
            *this = WTF::move(other);
        }
        BufferFormat& operator=(BufferFormat&& other)
        {
            usage = std::exchange(other.usage, RendererBufferFormat::Usage::Rendering);
            fourcc = std::exchange(other.fourcc, 0);
#if USE(GBM)
            modifiers = WTF::move(other.modifiers);
            gbmDevice = WTF::move(other.gbmDevice);
#endif
            return *this;
        }

        bool operator==(const BufferFormat& other) const
        {
            return usage == other.usage
#if USE(GBM)
                && gbmDevice == other.gbmDevice
                && drmDevice == other.drmDevice
                && modifiers == other.modifiers
#endif
                && fourcc == other.fourcc;
        }

        RendererBufferFormat::Usage usage { RendererBufferFormat::Usage::Rendering };
        uint32_t fourcc { 0 };

#if USE(GBM)
        WebCore::DRMDevice drmDevice;
        Vector<uint64_t, 1> modifiers;
        RefPtr<WebCore::GBMDevice> gbmDevice;
#endif
    };

    class RenderTargetEGLImage final : public RenderTargetShareableBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(AcceleratedSurface&, const WebCore::IntSize&, const BufferFormat&);
#if OS(ANDROID)
        RenderTargetEGLImage(AcceleratedSurface&, const WebCore::IntSize&, EGLImage, RefPtr<AHardwareBuffer>&&);
#else
        RenderTargetEGLImage(AcceleratedSurface&, const WebCore::IntSize&, EGLImage, WebCore::DMABufBufferAttributes&&, RendererBufferFormat::Usage);
#endif
        ~RenderTargetEGLImage();

    private:
        bool supportsExplicitSync() const override { return true; }
        void initializeColorBuffer();

        unsigned m_colorBuffer { 0 };
        EGLImage m_image { nullptr };
        RefPtr<WebCore::BitmapTexture> m_texture;
    };
#endif // USE(GBM) || OS(ANDROID)

    class RenderTargetSHMImage final : public RenderTargetShareableBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(AcceleratedSurface&, const WebCore::IntSize&);
        RenderTargetSHMImage(AcceleratedSurface&, const WebCore::IntSize&, Ref<WebCore::ShareableBitmap>&&, WebCore::ShareableBitmapHandle&&);
        ~RenderTargetSHMImage();

    private:
        bool supportsExplicitSync() const override { return false; }
        void didRenderFrame() override;

        unsigned m_colorBuffer { 0 };
        const Ref<WebCore::ShareableBitmap> m_bitmap;
        RefPtr<WebCore::BitmapTexture> m_texture;
    };

    class RenderTargetTexture final : public RenderTargetShareableBuffer {
    public:
        static std::unique_ptr<RenderTarget> create(AcceleratedSurface&, const WebCore::IntSize&);
        RenderTargetTexture(AcceleratedSurface&, const WebCore::IntSize&, Ref<WebCore::BitmapTexture>&&, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~RenderTargetTexture();

    private:
        bool supportsExplicitSync() const override { return true; }

        Ref<WebCore::BitmapTexture> m_texture;
    };
#endif // PLATFORM(GTK) || ENABLE(WPE_PLATFORM)

#if USE(WPE_RENDERER)
    class RenderTargetWPEBackend final : public RenderTarget {
    public:
        static std::unique_ptr<RenderTarget> create(AcceleratedSurface&, const WebCore::IntSize&, UnixFileDescriptor&&);
        RenderTargetWPEBackend(AcceleratedSurface&, const WebCore::IntSize&, UnixFileDescriptor&&);
        ~RenderTargetWPEBackend();

        uint64_t window() const;
        void resize(const WebCore::IntSize&);

    private:
        void willRenderFrame() override;
        void didRenderFrame() override;

        struct wpe_renderer_backend_egl_target* m_backend { nullptr };
    };
#endif

    class SwapChain {
        WTF_MAKE_NONCOPYABLE(SwapChain);
    public:
        explicit SwapChain(AcceleratedSurface&);

        enum class Type {
            Invalid,
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
#if USE(GBM) || OS(ANDROID)
            EGLImage,
#endif
            SharedMemory,
            Texture,
#endif
#if USE(WPE_RENDERER)
            WPEBackend
#endif
        };

        Type type() const { return m_type; }
        bool resize(const WebCore::IntSize&);
        bool handleBufferFormatChangeIfNeeded();
        const WebCore::IntSize& size() const LIFETIME_BOUND { return m_size; }
        RenderTarget* nextTarget();
        void releaseTarget(uint64_t, UnixFileDescriptor&& releaseFence);
        void reset();
        void releaseUnusedBuffers();

#if ENABLE(DAMAGE_TRACKING)
        void addDamage(const std::optional<WebCore::Damage>&);
#endif

#if (PLATFORM(GTK) || ENABLE(WPE_PLATFORM)) && (USE(GBM) || OS(ANDROID))
        void setupBufferFormat();
#endif

#if USE(WPE_RENDERER)
        uint64_t window();
#endif

    private:
        // FIXME: Allow configuring the initial buffer count, e.g. for triple buffering.
        static constexpr unsigned s_initialBuffers = 2;
        static constexpr unsigned s_maximumBuffers = 4;

        std::unique_ptr<RenderTarget> createTarget() const;

        const CheckedRef<AcceleratedSurface> m_surface;
        Type m_type { Type::Invalid };
        WebCore::IntSize m_size;
        Vector<std::unique_ptr<RenderTarget>, s_maximumBuffers> m_freeTargets;
        Vector<std::unique_ptr<RenderTarget>, s_maximumBuffers> m_lockedTargets;
        bool m_initialTargetsCreated { false };
#if (PLATFORM(GTK) || ENABLE(WPE_PLATFORM)) && (USE(GBM) || OS(ANDROID))
        Lock m_bufferFormatLock;
        BufferFormat m_bufferFormat WTF_GUARDED_BY_LOCK(m_bufferFormatLock);
        bool m_bufferFormatChanged WTF_GUARDED_BY_LOCK(m_bufferFormatLock) { false };
#endif
#if USE(WPE_RENDERER)
        UnixFileDescriptor m_hostFD;
        WebCore::IntSize m_initialSize;
#endif
    };

    const WeakRef<WebPage> m_webPage;
    Function<void()> m_frameCompleteHandler;
    bool m_useSkia { false };
    uint64_t m_id { 0 };
    RenderingPurpose m_renderingPurpose { RenderingPurpose::Composited };
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    bool m_hardwareAccelerationEnabled { true };
#endif
    Lock m_backgroundColorLock;
    std::optional<WebCore::Color> m_backgroundColor WTF_GUARDED_BY_LOCK(m_backgroundColorLock);
    SwapChain m_swapChain;
    RenderTarget* m_target { nullptr };
    Vector<std::pair<RenderTarget*, Vector<WebCore::IntRect, 1>>, 1> m_pendingFrameNotifyTargets;
    bool m_isVisible { false };
    bool m_useExplicitSync { false };
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedBuffersTimer;
#if ENABLE(DAMAGE_TRACKING)
    std::optional<WebCore::Damage> m_frameDamage;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
