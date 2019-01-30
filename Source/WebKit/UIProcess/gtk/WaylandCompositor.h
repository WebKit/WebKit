/*
 * Copyright (C) 2016 Igalia S.L.
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

#if PLATFORM(WAYLAND) && USE(EGL)

#include "WebPageProxy.h"
#include <WebCore/RefPtrCairo.h>
#include <WebCore/WlUniquePtr.h>
#include <wayland-server.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef void *EGLImageKHR;

namespace WebKit {

class WebPageProxy;

class WaylandCompositor {
    WTF_MAKE_NONCOPYABLE(WaylandCompositor);
    friend NeverDestroyed<WaylandCompositor>;
public:
    static WaylandCompositor& singleton();

    class Buffer : public CanMakeWeakPtr<Buffer> {
        WTF_MAKE_NONCOPYABLE(Buffer); WTF_MAKE_FAST_ALLOCATED;
    public:
        static Buffer* getOrCreate(struct wl_resource*);
        ~Buffer();

        void use();
        void unuse();

        EGLImageKHR createImage() const;
        WebCore::IntSize size() const;

    private:
        Buffer(struct wl_resource*);
        static void destroyListenerCallback(struct wl_listener*, void*);

        struct wl_resource* m_resource { nullptr };
        struct wl_listener m_destroyListener;
        uint32_t m_busyCount { 0 };
    };

    class Surface : public CanMakeWeakPtr<Surface> {
        WTF_MAKE_NONCOPYABLE(Surface); WTF_MAKE_FAST_ALLOCATED;
    public:
        Surface();
        ~Surface();

        void attachBuffer(struct wl_resource*);
        void requestFrame(struct wl_resource*);
        void commit();

        void setWebPage(WebPageProxy*);
        bool prepareTextureForPainting(unsigned&, WebCore::IntSize&);

    private:
        void flushFrameCallbacks();
        void flushPendingFrameCallbacks();
        void makePendingBufferCurrent();

        WeakPtr<Buffer> m_buffer;
        WeakPtr<Buffer> m_pendingBuffer;
        unsigned m_texture { 0 };
        EGLImageKHR m_image;
        WebCore::IntSize m_imageSize;
        Vector<wl_resource*> m_pendingFrameCallbackList;
        Vector<wl_resource*> m_frameCallbackList;
        WebPageProxy* m_webPage { nullptr };
        unsigned m_tickCallbackID { 0 };
    };

    bool isRunning() const { return !!m_display; }
    String displayName() const { return m_displayName; }

    void bindSurfaceToWebPage(Surface*, uint64_t pageID);
    void registerWebPage(WebPageProxy&);
    void unregisterWebPage(WebPageProxy&);

    bool getTexture(WebPageProxy&, unsigned&, WebCore::IntSize&);

private:
    WaylandCompositor();

    bool initializeEGL();

    String m_displayName;

    struct DisplayDeleter {
        void operator() (struct wl_display* display) { wl_display_destroy(display); }
    };
    std::unique_ptr<struct wl_display, DisplayDeleter> m_display;

    WebCore::WlUniquePtr<struct wl_global> m_compositorGlobal;
    WebCore::WlUniquePtr<struct wl_global> m_webkitgtkGlobal;
    GRefPtr<GSource> m_eventSource;
    HashMap<WebPageProxy*, WeakPtr<Surface>> m_pageMap;
};

} // namespace WebKit

#endif // PLATFORM(WAYLAND) && USE(EGL)
