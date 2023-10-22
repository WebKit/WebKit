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
}

namespace WTF {
class UnixFileDescriptor;
}

namespace WebKit {

class ShareableBitmap;
class ShareableBitmapHandle;
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
    void didCreateBufferSHM(uint64_t id, ShareableBitmapHandle&&);
    void didDestroyBuffer(uint64_t id);
    void frame(uint64_t id);
    void frameDone();
    void ensureGLContext();
    void ensureRenderer();
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
            EglImage,
#if USE(GBM)
            Gbm,
#endif
            SharedMemory
        };

        virtual Type type() const = 0;

        uint64_t id() const { return m_id; }
        const WebCore::IntSize size() const { return m_size; }
    protected:
        Buffer(uint64_t id, const WebCore::IntSize&);

        uint64_t m_id { 0 };
        WebCore::IntSize m_size;
    };

    class BufferEGLImage final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, const WebCore::IntSize&, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier);
        ~BufferEGLImage();

        Buffer::Type type() const override { return Buffer::Type::EglImage; }

        EGLImage image() const { return m_image; }

    private:
        BufferEGLImage(uint64_t id, const WebCore::IntSize&, Vector<WTF::UnixFileDescriptor>&&, EGLImage);

        Vector<WTF::UnixFileDescriptor> m_fds;
        EGLImage m_image { nullptr };
    };

#if USE(GBM)
    class BufferGBM final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, const WebCore::IntSize&, uint32_t format, WTF::UnixFileDescriptor&&, uint32_t stride);
        ~BufferGBM();

        Buffer::Type type() const override { return Buffer::Type::Gbm; }

        RefPtr<cairo_surface_t> surface() const;

    private:
        BufferGBM(uint64_t id, const WebCore::IntSize&, WTF::UnixFileDescriptor&&, struct gbm_bo*);

        WTF::UnixFileDescriptor m_fd;
        struct gbm_bo* m_buffer { nullptr };
    };
#endif

    class BufferSHM final : public Buffer {
    public:
        static RefPtr<Buffer> create(uint64_t id, RefPtr<ShareableBitmap>&&);
        ~BufferSHM() = default;

        Buffer::Type type() const override { return Buffer::Type::SharedMemory; }

        RefPtr<cairo_surface_t> surface() const;

    private:
        BufferSHM(uint64_t id, RefPtr<ShareableBitmap>&&);

        RefPtr<ShareableBitmap> m_bitmap;
    };

    class Renderer {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~Renderer() = default;

        enum class Type { Gl, Cairo };

        virtual Type type() const = 0;
        virtual bool setBuffer(Buffer&, float deviceScaleFactor);
#if USE(GTK4)
        virtual void snapshot(GtkSnapshot*) const = 0;
#else
        virtual void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const = 0;
#endif

        Buffer* buffer() const { return m_buffer.get(); }

    protected:
        Renderer() = default;

        WebCore::IntSize m_size;
        float m_deviceScaleFactor { 0 };
        RefPtr<Buffer> m_buffer;
    };

    class RendererGL : public Renderer {
    public:
        RendererGL();
        ~RendererGL();

    private:
        Renderer::Type type() const override { return Renderer::Type::Gl; }
        bool setBuffer(Buffer&, float deviceScaleFactor) override;
#if USE(GTK4)
        void snapshot(GtkSnapshot*) const override;
#else
        void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const override;
#endif

        unsigned m_textureID { 0 };
#if USE(GTK4)
        GRefPtr<GdkTexture> m_texture[2];
        uint16_t m_textureIndex : 1;
#endif
    };

    class RendererCairo : public Renderer {
    public:
        RendererCairo() = default;
        ~RendererCairo() = default;

    private:
        Renderer::Type type() const override { return Renderer::Type::Cairo; }
        bool setBuffer(Buffer&, float deviceScaleFactor) override;
#if USE(GTK4)
        void snapshot(GtkSnapshot*) const override;
#else
        void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const override;
#endif

        RefPtr<cairo_surface_t> m_surface;
    };

    GRefPtr<GdkGLContext> m_gdkGLContext;
    bool m_glContextInitialized { false };
    uint64_t m_surfaceID { 0 };
    std::unique_ptr<Renderer> m_renderer;
    RefPtr<Buffer> m_pendingBuffer;
    RefPtr<Buffer> m_committedBuffer;
    HashMap<uint64_t, RefPtr<Buffer>> m_buffers;
};

} // namespace WebKit

#endif // USE(EGL)
