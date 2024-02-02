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

#if USE(EGL)

#include "AcceleratedBackingStore.h"
#include "DMABufRendererBufferFormat.h"
#include "MessageReceiver.h"
#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <gtk/gtk.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

typedef void *EGLImage;

#if USE(GBM)
struct gbm_bo;
#endif

namespace WebCore {
class IntRect;
class ShareableBitmap;
class ShareableBitmapHandle;
}

namespace WTF {
class UnixFileDescriptor;
}

namespace WebKit {

class WebPageProxy;
enum class DMABufRendererBufferMode : uint8_t;

class AcceleratedBackingStoreDMABuf final : public AcceleratedBackingStore, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(AcceleratedBackingStoreDMABuf); WTF_MAKE_FAST_ALLOCATED;
public:
    static OptionSet<DMABufRendererBufferMode> rendererBufferMode();
    static bool checkRequirements();
#if USE(GBM)
    static Vector<DMABufRendererBufferFormat> preferredBufferFormats();
#endif
    static std::unique_ptr<AcceleratedBackingStoreDMABuf> create(WebPageProxy&);
    ~AcceleratedBackingStoreDMABuf();
private:
    AcceleratedBackingStoreDMABuf(WebPageProxy&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didCreateBuffer(uint64_t id, const WebCore::IntSize&, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
    void didCreateBufferSHM(uint64_t id, WebCore::ShareableBitmapHandle&&);
    void didDestroyBuffer(uint64_t id);
    void frame(uint64_t id);
    void frameDone();
    void ensureGLContext();
    bool prepareForRendering();

#if USE(GTK4)
    void snapshot(GtkSnapshot*) override;
#else
    bool paint(cairo_t*, const WebCore::IntRect&) override;
#endif
    void unrealize() override;
    void update(const LayerTreeContext&) override;

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
        virtual void didUpdateContents() = 0;
#if USE(GTK4)
        virtual GdkTexture* texture() const { return nullptr; }
#else
        virtual unsigned textureID() const { return 0; }
#endif
        virtual cairo_surface_t* surface() const { return nullptr; }

        uint64_t id() const { return m_id; }
        const WebCore::IntSize size() const { return m_size; }
        float deviceScaleFactor() const { return m_deviceScaleFactor; }
        float unscaledWidth() const { return static_cast<float>(m_size.width() / m_deviceScaleFactor); }
        float unscaledHeight() const { return static_cast<float>(m_size.height() / m_deviceScaleFactor); }

    protected:
        Buffer(uint64_t id, const WebCore::IntSize&, float deviceScaleFactor);

        uint64_t m_id { 0 };
        WebCore::IntSize m_size;
        float m_deviceScaleFactor { 1 };
    };

#if GTK_CHECK_VERSION(4, 13, 4)
    class BufferDMABuf final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, GdkDisplay*, const WebCore::IntSize&, float deviceScaleFactor, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~BufferDMABuf() = default;

    private:
        BufferDMABuf(uint64_t id, const WebCore::IntSize&, float deviceScaleFactor, Vector<WTF::UnixFileDescriptor>&&, GRefPtr<GdkDmabufTextureBuilder>&&);

        Buffer::Type type() const override { return Buffer::Type::DmaBuf; }
        void didUpdateContents() override;
        GdkTexture* texture() const override { return m_texture.get(); }

        Vector<WTF::UnixFileDescriptor> m_fds;
        GRefPtr<GdkDmabufTextureBuilder> m_builder;
        GRefPtr<GdkTexture> m_texture;
    };
#endif

    class BufferEGLImage final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, const WebCore::IntSize&, float deviceScaleFactor, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~BufferEGLImage();

    private:
        BufferEGLImage(uint64_t id, const WebCore::IntSize&, float deviceScaleFactor, Vector<WTF::UnixFileDescriptor>&&, EGLImage);

        Buffer::Type type() const override { return Buffer::Type::EglImage; }
        void didUpdateContents() override;
#if USE(GTK4)
        GdkTexture* texture() const override { return m_texture.get(); }
#else
        unsigned textureID() const override { return m_textureID; }
#endif

        Vector<WTF::UnixFileDescriptor> m_fds;
        EGLImage m_image { nullptr };
#if USE(GTK4)
        GRefPtr<GdkTexture> m_texture;
#else
        unsigned m_textureID { 0 };
#endif
    };

#if USE(GBM)
    class BufferGBM final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, const WebCore::IntSize&, float deviceScaleFactor, uint32_t format, WTF::UnixFileDescriptor&&, uint32_t stride);
        ~BufferGBM();

    private:
        BufferGBM(uint64_t id, const WebCore::IntSize&, float deviceScaleFactor, WTF::UnixFileDescriptor&&, struct gbm_bo*);

        Buffer::Type type() const override { return Buffer::Type::Gbm; }
        void didUpdateContents() override;
        cairo_surface_t* surface() const override { return m_surface.get(); }

        WTF::UnixFileDescriptor m_fd;
        struct gbm_bo* m_buffer { nullptr };
        RefPtr<cairo_surface_t> m_surface;
    };
#endif

    class BufferSHM final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, RefPtr<WebCore::ShareableBitmap>&&, float deviceScaleFactor);
        ~BufferSHM() = default;

    private:
        BufferSHM(uint64_t id, RefPtr<WebCore::ShareableBitmap>&&, float deviceScaleFactor);

        Buffer::Type type() const override { return Buffer::Type::SharedMemory; }
        void didUpdateContents() override;
        cairo_surface_t* surface() const override { return m_surface.get(); }

        RefPtr<WebCore::ShareableBitmap> m_bitmap;
        RefPtr<cairo_surface_t> m_surface;
    };

    class Renderer {
    public:
        Renderer() = default;
        ~Renderer() = default;

        void setBuffer(Buffer* buffer) { m_buffer = buffer; }
        Buffer* buffer() const { return m_buffer.get(); }

#if USE(GTK4)
        void snapshot(GtkSnapshot*) const;
#else
        void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const;
#endif

    private:
        RefPtr<Buffer> m_buffer;
    };

    GRefPtr<GdkGLContext> m_gdkGLContext;
    bool m_glContextInitialized { false };
    uint64_t m_surfaceID { 0 };
    Renderer m_renderer;
    RefPtr<Buffer> m_pendingBuffer;
    RefPtr<Buffer> m_committedBuffer;
    HashMap<uint64_t, RefPtr<Buffer>> m_buffers;
};

} // namespace WebKit

#endif // USE(EGL)
