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

#include "AcceleratedBackingStore.h"

#if USE(GBM)
#include "MessageReceiver.h"
#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <gtk/gtk.h>
#include <wtf/CompletionHandler.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

typedef void *EGLImage;
struct gbm_bo;

namespace WebCore {
class IntRect;
}

namespace WTF {
class UnixFileDescriptor;
}

namespace WebKit {

class WebPageProxy;

class AcceleratedBackingStoreDMABuf final : public AcceleratedBackingStore, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(AcceleratedBackingStoreDMABuf); WTF_MAKE_FAST_ALLOCATED;
public:
    static bool checkRequirements();
    static std::unique_ptr<AcceleratedBackingStoreDMABuf> create(WebPageProxy&);
    ~AcceleratedBackingStoreDMABuf();
private:
    AcceleratedBackingStoreDMABuf(WebPageProxy&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void configure(WTF::UnixFileDescriptor&&, WTF::UnixFileDescriptor&&, int fourcc, int32_t offset, int32_t stride, WebCore::IntSize&&, uint64_t modifier);
    void frame(CompletionHandler<void()>&&);
    void ensureGLContext();

#if USE(GTK4)
    void snapshot(GtkSnapshot*) override;
#else
    bool paint(cairo_t*, const WebCore::IntRect&) override;
#endif
    void realize() override;
    void unrealize() override;
    bool makeContextCurrent() override;
    void update(const LayerTreeContext&) override;

    class RenderSource {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~RenderSource() = default;
        virtual bool swap() = 0;
#if USE(GTK4)
        virtual void snapshot(GtkSnapshot*) const = 0;
#else
        virtual void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const = 0;
#endif

        const WebCore::IntSize size() const { return m_size; }

    protected:
        RenderSource(const WebCore::IntSize&, float deviceScaleFactor);

        WebCore::IntSize m_size;
        float m_deviceScaleFactor { 1 };
    };

    class Texture final : public RenderSource {
    public:
        Texture(GdkGLContext*, const WTF::UnixFileDescriptor&, const WTF::UnixFileDescriptor&, int fourcc, int32_t offset, int32_t stride, const WebCore::IntSize&, uint64_t modifier, float deviceScaleFactor);
        ~Texture();

        unsigned texture() const { return m_textureID; }

    private:
        bool swap() override;
#if USE(GTK4)
        void snapshot(GtkSnapshot*) const override;
#else
        void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const override;
#endif

        GRefPtr<GdkGLContext> m_context;
        unsigned m_textureID { 0 };
        EGLImage m_backImage { nullptr };
        EGLImage m_frontImage { nullptr };
#if USE(GTK4)
        GRefPtr<GdkTexture> m_texture[2];
        uint16_t m_textureIndex : 1;
#endif
    };

    class Surface final : public RenderSource {
    public:
        Surface(const WTF::UnixFileDescriptor&, const WTF::UnixFileDescriptor&, int fourcc, int32_t offset, int32_t stride, const WebCore::IntSize&, float deviceScaleFactor);
        ~Surface();

        cairo_surface_t* surface() const { return m_surface.get(); }

    private:
        bool swap() override;
#if USE(GTK4)
        void snapshot(GtkSnapshot*) const override;
#else
        void paint(GtkWidget*, cairo_t*, const WebCore::IntRect&) const override;
#endif

        RefPtr<cairo_surface_t> map(struct gbm_bo*) const;

        struct gbm_bo* m_backBuffer { nullptr };
        struct gbm_bo* m_frontBuffer { nullptr };
        RefPtr<cairo_surface_t> m_surface;
        RefPtr<cairo_surface_t> m_backSurface;
    };

    std::unique_ptr<RenderSource> createSource();

    GRefPtr<GdkGLContext> m_gdkGLContext;
    bool m_glContextInitialized { false };
    struct {
        uint64_t id { 0 };
        WTF::UnixFileDescriptor backFD;
        WTF::UnixFileDescriptor frontFD;
        int fourcc { 0 };
        int32_t offset { 0 };
        int32_t stride { 0 };
        WebCore::IntSize size;
        uint64_t modifier { 0 };
    } m_surface;
    std::unique_ptr<RenderSource> m_pendingSource;
    std::unique_ptr<RenderSource> m_committedSource;
    CompletionHandler<void()> m_frameCompletionHandler;
};

} // namespace WebKit

#endif // USE(GBM)
