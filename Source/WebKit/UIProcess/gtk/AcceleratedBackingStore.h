/*
 * Copyright (C) 2016, 2023 Igalia S.L.
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

#include "FenceMonitor.h"
#include "MessageReceiver.h"
#include "RendererBufferDescription.h"
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <gtk/gtk.h>
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

typedef void *EGLImage;

#if USE(GBM)
struct gbm_bo;
#endif

namespace WebCore {
class NativeImage;
class ShareableBitmap;
class ShareableBitmapHandle;
}

namespace WebKit {
class LayerTreeContext;
class WebPageProxy;
class WebProcessProxy;
enum class RendererBufferTransportMode : uint8_t;

class AcceleratedBackingStore final : public IPC::MessageReceiver, public RefCounted<AcceleratedBackingStore> {
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedBackingStore);
public:
    using Rects = Vector<WebCore::IntRect, 1>;

    static OptionSet<RendererBufferTransportMode> rendererBufferTransportMode();
    static bool canUseHardwareAcceleration();
#if USE(GBM)
    static Vector<RendererBufferFormat> preferredBufferFormats();
#endif
    static Ref<AcceleratedBackingStore> create(WebPageProxy&);
    ~AcceleratedBackingStore();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void update(const LayerTreeContext&);
#if USE(GTK4)
    bool snapshot(GtkSnapshot*);
#else
    bool paint(cairo_t*, const WebCore::IntRect&);
#endif
    void realize();
    void unrealize();
    RendererBufferDescription bufferDescription() const;
    RefPtr<WebCore::NativeImage> bufferAsNativeImageForTesting() const;

private:
    explicit AcceleratedBackingStore(WebPageProxy&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didCreateDMABufBuffer(uint64_t id, const WebCore::IntSize&, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, RendererBufferFormat::Usage);
    void didCreateSHMBuffer(uint64_t id, WebCore::ShareableBitmapHandle&&);
    void didDestroyBuffer(uint64_t id);
    void frame(uint64_t id, Rects&&, WTF::UnixFileDescriptor&&);
    void frameDone();

    void ensureGLContext();
    bool swapBuffersIfNeeded();

    class Buffer : public RefCounted<Buffer> {
    public:
        virtual ~Buffer() = default;

        enum class Type {
#if GTK_CHECK_VERSION(4, 13, 4)
            DmaBuf,
#endif
            EglImage,
#if USE(GBM)
            Gbm,
#endif
            SharedMemory
        };

        virtual Type type() const = 0;
        virtual void didUpdateContents(Buffer*, const Rects&) = 0;
#if USE(GTK4)
        virtual GdkTexture* texture() const { return nullptr; }
#else
        virtual unsigned textureID() const { return 0; }
#endif
        virtual cairo_surface_t* surface() const { return nullptr; }

        virtual RendererBufferDescription description() const = 0;
        virtual RefPtr<WebCore::NativeImage> asNativeImageForTesting() const = 0;
        virtual void release() = 0;

        uint64_t id() const { return m_id; }
        float deviceScaleFactor() const;
        void setSurfaceID(uint64_t surfaceID) { m_surfaceID = surfaceID; }
#if USE(GTK4)
        void snapshot(GtkSnapshot*) const;
#else
        void paint(cairo_t*, const WebCore::IntRect&) const;
#endif
        void didRelease() const;

    protected:
        Buffer(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage);

        WeakPtr<WebPageProxy> m_webPage;
        uint64_t m_id { 0 };
        uint64_t m_surfaceID { 0 };
        WebCore::IntSize m_size;
        RendererBufferFormat::Usage m_usage { RendererBufferFormat::Usage::Rendering };
    };

#if GTK_CHECK_VERSION(4, 13, 4)
    class BufferDMABuf final : public Buffer {
    public:
        static RefPtr<Buffer> create(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);

    private:
        BufferDMABuf(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage, Vector<WTF::UnixFileDescriptor>&&, GRefPtr<GdkDmabufTextureBuilder>&&);

        Buffer::Type type() const override { return Buffer::Type::DmaBuf; }
        void didUpdateContents(Buffer*, const Rects&) override;
        GdkTexture* texture() const override { return m_texture.get(); }
        RendererBufferDescription description() const override;
        RefPtr<WebCore::NativeImage> asNativeImageForTesting() const override;
        void release() override;

        Vector<WTF::UnixFileDescriptor> m_fds;
        GRefPtr<GdkDmabufTextureBuilder> m_builder;
        GRefPtr<GdkTexture> m_texture;
    };
#endif

    class BufferEGLImage final : public Buffer {
    public:
        static RefPtr<Buffer> create(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~BufferEGLImage();

    private:
        BufferEGLImage(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, uint64_t modifier, EGLImage);

        Buffer::Type type() const override { return Buffer::Type::EglImage; }
        void didUpdateContents(Buffer*, const Rects&) override;
#if USE(GTK4)
        GdkTexture* texture() const override { return m_texture.get(); }
#else
        unsigned textureID() const override { return m_textureID; }
#endif
        RendererBufferDescription description() const override;
        RefPtr<WebCore::NativeImage> asNativeImageForTesting() const override;
        void release() override;

        Vector<WTF::UnixFileDescriptor> m_fds;
        EGLImage m_image { nullptr };
#if USE(GTK4)
        GRefPtr<GdkTexture> m_texture;
#else
        GRefPtr<GdkGLContext> m_gdkGLContext;
        unsigned m_textureID { 0 };
#endif
        uint32_t m_fourcc { 0 };
        uint64_t m_modifier { 0 };
    };

#if USE(GBM)
    class BufferGBM final : public Buffer {
    public:
        static RefPtr<Buffer> create(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage, uint32_t format, WTF::UnixFileDescriptor&&, uint32_t stride);
        ~BufferGBM();

    private:
        BufferGBM(WebPageProxy&, uint64_t id, uint64_t surfaceID, const WebCore::IntSize&, RendererBufferFormat::Usage, WTF::UnixFileDescriptor&&, struct gbm_bo*);

        Buffer::Type type() const override { return Buffer::Type::Gbm; }
        void didUpdateContents(Buffer*, const Rects&) override;
        cairo_surface_t* surface() const override { return m_surface.get(); }
        RendererBufferDescription description() const override;
        RefPtr<WebCore::NativeImage> asNativeImageForTesting() const override;
        void release() override;

        WTF::UnixFileDescriptor m_fd;
        struct gbm_bo* m_buffer { nullptr };
        RefPtr<cairo_surface_t> m_surface;
    };
#endif

    class BufferSHM final : public Buffer {
    public:
        static RefPtr<Buffer> create(WebPageProxy&, uint64_t id, uint64_t surfaceID, RefPtr<WebCore::ShareableBitmap>&&);

    private:
        BufferSHM(WebPageProxy&, uint64_t id, uint64_t surfaceID, RefPtr<WebCore::ShareableBitmap>&&);

        Buffer::Type type() const override { return Buffer::Type::SharedMemory; }
        void didUpdateContents(Buffer*, const Rects&) override;
        cairo_surface_t* surface() const override { return m_surface.get(); }
        RendererBufferDescription description() const override;
        RefPtr<WebCore::NativeImage> asNativeImageForTesting() const override;
        void release() override;

        RefPtr<WebCore::ShareableBitmap> m_bitmap;
        RefPtr<cairo_surface_t> m_surface;
    };

    WeakPtr<WebPageProxy> m_webPage;
    FenceMonitor m_fenceMonitor;
    GRefPtr<GdkGLContext> m_gdkGLContext;
    bool m_glContextInitialized { false };
    uint64_t m_surfaceID { 0 };
    WeakPtr<WebProcessProxy> m_legacyMainFrameProcess;
    RefPtr<Buffer> m_pendingBuffer;
    RefPtr<Buffer> m_committedBuffer;
    Rects m_pendingDamageRects;
    HashMap<uint64_t, RefPtr<Buffer>> m_buffers;
};

} // namespace WebKit
