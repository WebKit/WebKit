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

#include "config.h"
#include "WaylandCompositor.h"

#if PLATFORM(WAYLAND) && USE(EGL)

#include "WebKit2WaylandServerProtocol.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <WebCore/GLContext.h>
#include <WebCore/PlatformDisplayWayland.h>
#include <WebCore/Region.h>
#include <WebCore/UUID.h>
#include <wayland-server-protocol.h>

#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <WebCore/OpenGLShims.h>
#endif

using namespace WebCore;

namespace WebKit {

#if !defined(PFNEGLBINDWAYLANDDISPLAYWL)
typedef EGLBoolean (*PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay, struct wl_display*);
#endif

#if !defined(PFNEGLUNBINDWAYLANDDISPLAYWL)
typedef EGLBoolean (*PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay, struct wl_display*);
#endif

#if !defined(PFNEGLQUERYWAYLANDBUFFERWL)
typedef EGLBoolean (*PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay, struct wl_resource*, EGLint attribute, EGLint* value);
#endif

#if !defined(PFNEGLCREATEIMAGEKHRPROC)
typedef EGLImageKHR (*PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay, EGLContext, EGLenum target, EGLClientBuffer, const EGLint* attribList);
#endif

#if !defined(PFNEGLDESTROYIMAGEKHRPROC)
typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay, EGLImageKHR);
#endif

#if !defined(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES);
#endif

static PFNEGLBINDWAYLANDDISPLAYWL eglBindWaylandDisplay;
static PFNEGLUNBINDWAYLANDDISPLAYWL eglUnbindWaylandDisplay;
static PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBuffer;
static PFNEGLCREATEIMAGEKHRPROC eglCreateImage;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImage;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glImageTargetTexture2D;

WaylandCompositor& WaylandCompositor::singleton()
{
    static NeverDestroyed<WaylandCompositor> waylandCompositor;
    return waylandCompositor;
}

WaylandCompositor::Buffer* WaylandCompositor::Buffer::getOrCreate(struct wl_resource* resource)
{
    if (struct wl_listener* listener = wl_resource_get_destroy_listener(resource, destroyListenerCallback)) {
        WaylandCompositor::Buffer* buffer;
        return wl_container_of(listener, buffer, m_destroyListener);
    }

    return new WaylandCompositor::Buffer(resource);
}

WaylandCompositor::Buffer::Buffer(struct wl_resource* resource)
    : m_resource(resource)
    , m_weakPtrFactory(this)
{
    wl_list_init(&m_destroyListener.link);
    m_destroyListener.notify = destroyListenerCallback;
    wl_resource_add_destroy_listener(m_resource, &m_destroyListener);
}

WaylandCompositor::Buffer::~Buffer()
{
    wl_list_remove(&m_destroyListener.link);
}

void WaylandCompositor::Buffer::destroyListenerCallback(struct wl_listener* listener, void*)
{
    WaylandCompositor::Buffer* buffer;
    buffer = wl_container_of(listener, buffer, m_destroyListener);
    delete buffer;
}

void WaylandCompositor::Buffer::use()
{
    m_busyCount++;
}

void WaylandCompositor::Buffer::unuse()
{
    m_busyCount--;
    if (!m_busyCount)
        wl_resource_queue_event(m_resource, WL_BUFFER_RELEASE);
}

EGLImageKHR WaylandCompositor::Buffer::createImage() const
{
    return static_cast<EGLImageKHR*>(eglCreateImage(PlatformDisplay::sharedDisplay().eglDisplay(), EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, m_resource, nullptr));
}

IntSize WaylandCompositor::Buffer::size() const
{
    EGLDisplay eglDisplay = PlatformDisplay::sharedDisplay().eglDisplay();
    int width, height;
    eglQueryWaylandBuffer(eglDisplay, m_resource, EGL_WIDTH, &width);
    eglQueryWaylandBuffer(eglDisplay, m_resource, EGL_HEIGHT, &height);

    return { width, height };
}

WaylandCompositor::Surface::Surface()
    : m_image(EGL_NO_IMAGE_KHR)
{
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

WaylandCompositor::Surface::~Surface()
{
    // Destroy pending frame callbacks.
    auto list = WTFMove(m_frameCallbackList);
    for (auto* resource : list)
        wl_resource_destroy(resource);

    if (m_buffer)
        m_buffer->unuse();

    if (m_image != EGL_NO_IMAGE_KHR)
        eglDestroyImage(PlatformDisplay::sharedDisplay().eglDisplay(), m_image);

    glDeleteTextures(1, &m_texture);
}

void WaylandCompositor::Surface::makePendingBufferCurrent()
{
    if (m_pendingBuffer == m_buffer)
        return;

    if (m_buffer)
        m_buffer->unuse();

    if (m_pendingBuffer)
        m_pendingBuffer->use();

    m_buffer = m_pendingBuffer;
}

void WaylandCompositor::Surface::attachBuffer(struct wl_resource* buffer)
{
    if (m_pendingBuffer)
        m_pendingBuffer = nullptr;

    if (buffer) {
        auto* compositorBuffer = WaylandCompositor::Buffer::getOrCreate(buffer);
        m_pendingBuffer = compositorBuffer->createWeakPtr();
    }
}

void WaylandCompositor::Surface::requestFrame(struct wl_resource* resource)
{
    wl_resource_set_implementation(resource, nullptr, this, [](struct wl_resource* resource) {
        auto* surface = static_cast<WaylandCompositor::Surface*>(wl_resource_get_user_data(resource));
        if (size_t item = surface->m_frameCallbackList.find(resource) != notFound)
            surface->m_frameCallbackList.remove(item);
    });
    m_frameCallbackList.append(resource);
}

bool WaylandCompositor::Surface::prepareTextureForPainting(unsigned& texture, IntSize& textureSize)
{
    if (m_image == EGL_NO_IMAGE_KHR)
        return false;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glImageTargetTexture2D(GL_TEXTURE_2D, m_image);

    texture = m_texture;
    textureSize = m_buffer->size();
    return true;
}

void WaylandCompositor::Surface::commit()
{
    EGLDisplay eglDisplay = PlatformDisplay::sharedDisplay().eglDisplay();
    if (m_image != EGL_NO_IMAGE_KHR)
        eglDestroyImage(eglDisplay, m_image);
    m_image = m_pendingBuffer->createImage();
    if (m_image == EGL_NO_IMAGE_KHR)
        return;

    makePendingBufferCurrent();
    if (m_webPage)
        m_webPage->setViewNeedsDisplay(IntRect(IntPoint::zero(), m_webPage->viewSize()));

    // From a Wayland point-of-view frame callbacks should be fired where the
    // compositor knows it has *used* the committed contents, so firing them here
    // can be surprising but we don't need them as a throttling mechanism because
    // rendering synchronization is handled elsewhere by WebKit.
    auto list = WTFMove(m_frameCallbackList);
    for (auto* resource : list) {
        wl_callback_send_done(resource, 0);
        wl_resource_destroy(resource);
    }
}

static const struct wl_surface_interface surfaceInterface = {
    // destroyCallback
    [](struct wl_client*, struct wl_resource* resource)
    {
        wl_resource_destroy(resource);
    },
    // attachCallback
    [](struct wl_client* client, struct wl_resource* resource, struct wl_resource* buffer, int32_t sx, int32_t sy)
    {
        auto* surface = static_cast<WaylandCompositor::Surface*>(wl_resource_get_user_data(resource));
        if (!surface)
            return;

        EGLint format;
        if (!eglQueryWaylandBuffer(PlatformDisplay::sharedDisplay().eglDisplay(), buffer, EGL_TEXTURE_FORMAT, &format)
            || (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA))
            return;

        surface->attachBuffer(buffer);
    },
    // damageCallback
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
    // frameCallback
    [](struct wl_client* client, struct wl_resource* resource, uint32_t id)
    {
        auto* surface = static_cast<WaylandCompositor::Surface*>(wl_resource_get_user_data(resource));
        if (!surface)
            return;

        if (struct wl_resource* callbackResource = wl_resource_create(client, &wl_callback_interface, 1, id))
            surface->requestFrame(callbackResource);
        else
            wl_client_post_no_memory(client);
    },
    // setOpaqueRegionCallback
    [](struct wl_client*, struct wl_resource*, struct wl_resource*) { },
    // setInputRegionCallback
    [](struct wl_client*, struct wl_resource*, struct wl_resource*) { },
    // commitCallback
    [](struct wl_client* client, struct wl_resource* resource)
    {
        auto* surface = static_cast<WaylandCompositor::Surface*>(wl_resource_get_user_data(resource));
        if (!surface)
            return;
        surface->commit();
    },
    // setBufferTransformCallback
    [](struct wl_client*, struct wl_resource*, int32_t) { },
    // setBufferScaleCallback
    [](struct wl_client*, struct wl_resource*, int32_t) { },
#if WAYLAND_VERSION_MAJOR > 1 || (WAYLAND_VERSION_MAJOR == 1 && WAYLAND_VERSION_MINOR >= 10)
    // damageBufferCallback
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
#endif
};

static const struct wl_compositor_interface compositorInterface = {
    // createSurfaceCallback
    [](struct wl_client* client, struct wl_resource* resource, uint32_t id)
    {
        if (struct wl_resource* surfaceResource = wl_resource_create(client, &wl_surface_interface, 1, id)) {
            wl_resource_set_implementation(surfaceResource, &surfaceInterface, new WaylandCompositor::Surface(),
                [](struct wl_resource* resource) {
                    auto* surface = static_cast<WaylandCompositor::Surface*>(wl_resource_get_user_data(resource));
                    delete surface;
                });
        } else
            wl_client_post_no_memory(client);
    },
    // createRegionCallback
    [](struct wl_client*, struct wl_resource*, uint32_t) { }
};

static const struct wl_webkitgtk_interface webkitgtkInterface = {
    // bindSurfaceToPageCallback
    [](struct wl_client*, struct wl_resource* resource, struct wl_resource* surfaceResource, uint32_t pageID)
    {
        auto* surface = static_cast<WaylandCompositor::Surface*>(wl_resource_get_user_data(surfaceResource));
        if (!surface)
            return;

        auto* compositor = static_cast<WaylandCompositor*>(wl_resource_get_user_data(resource));
        compositor->bindSurfaceToWebPage(surface, pageID);
    }
};

bool WaylandCompositor::initializeEGL()
{
    if (PlatformDisplay::sharedDisplay().eglCheckVersion(1, 5)) {
        eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImage"));
        eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImage"));
    } else {
        const char* extensions = eglQueryString(PlatformDisplay::sharedDisplay().eglDisplay(), EGL_EXTENSIONS);
        if (GLContext::isExtensionSupported(extensions, "EGL_KHR_image_base")) {
            eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
            eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
        }
    }
    if (!eglCreateImage || !eglDestroyImage) {
        WTFLogAlways("WaylandCompositor requires eglCreateImage and eglDestroyImage.");
        return false;
    }

    glImageTargetTexture2D = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!glImageTargetTexture2D) {
        WTFLogAlways("WaylandCompositor requires glEGLImageTargetTexture2D.");
        return false;
    }

    eglQueryWaylandBuffer = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
    if (!eglQueryWaylandBuffer) {
        WTFLogAlways("WaylandCompositor requires eglQueryWaylandBuffer.");
        return false;
    }

    eglBindWaylandDisplay = reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
    eglUnbindWaylandDisplay = reinterpret_cast<PFNEGLUNBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglUnbindWaylandDisplayWL"));
    if (!eglBindWaylandDisplay || !eglUnbindWaylandDisplay) {
        WTFLogAlways("WaylandCompositor requires eglBindWaylandDisplayWL and eglUnbindWaylandDisplayWL.");
        return false;
    }

    m_eglContext = GLContext::createOffscreenContext();
    if (!m_eglContext)
        return false;

    if (!m_eglContext->makeContextCurrent())
        return false;

    return true;
}

typedef struct {
    GSource source;
    gpointer fdTag;
    struct wl_display* display;
} WaylandLoopSource;

static const unsigned waylandLoopSourceCondition = G_IO_IN | G_IO_HUP | G_IO_ERR;

static GSourceFuncs waylandLoopSourceFunctions = {
    // prepare
    [](GSource *source, int *timeout) -> gboolean
    {
        *timeout = -1;
        auto* wlLoopSource = reinterpret_cast<WaylandLoopSource*>(source);
        wl_display_flush_clients(wlLoopSource->display);
        return FALSE;
    },
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        auto* wlLoopSource = reinterpret_cast<WaylandLoopSource*>(source);
        unsigned events = g_source_query_unix_fd(source, wlLoopSource->fdTag) & waylandLoopSourceCondition;
        if (events & G_IO_HUP || events & G_IO_ERR) {
            WTFLogAlways("Wayland Display Event Source: lost connection to nested Wayland compositor");
            return G_SOURCE_REMOVE;
        }

        if (events & G_IO_IN)
            wl_event_loop_dispatch(wl_display_get_event_loop(wlLoopSource->display), 0);
        return G_SOURCE_CONTINUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

static GRefPtr<GSource> createWaylandLoopSource(struct wl_display* display)
{
    GRefPtr<GSource> source = adoptGRef(g_source_new(&waylandLoopSourceFunctions, sizeof(WaylandLoopSource)));
    g_source_set_name(source.get(), "Nested Wayland compositor display event source");
    g_source_set_priority(source.get(), G_PRIORITY_DEFAULT + 1);

    auto* wlLoopSource = reinterpret_cast<WaylandLoopSource*>(source.get());
    wlLoopSource->display = display;
    wlLoopSource->fdTag = g_source_add_unix_fd(source.get(), wl_event_loop_get_fd(wl_display_get_event_loop(display)), static_cast<GIOCondition>(waylandLoopSourceCondition));
    g_source_attach(source.get(), nullptr);

    return source;
}

WaylandCompositor::WaylandCompositor()
{
    WlUniquePtr<struct wl_display> display(wl_display_create());
    if (!display) {
        WTFLogAlways("Nested Wayland compositor could not create display object");
        return;
    }

    String displayName = "webkitgtk-wayland-compositor-" + createCanonicalUUIDString();
    if (wl_display_add_socket(display.get(), displayName.utf8().data()) == -1) {
        WTFLogAlways("Nested Wayland compositor could not create display socket");
        return;
    }

    WlUniquePtr<struct wl_global> compositorGlobal(wl_global_create(display.get(), &wl_compositor_interface, wl_compositor_interface.version, this,
        [](struct wl_client* client, void* data, uint32_t version, uint32_t id) {
            if (struct wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, std::min(static_cast<int>(version), 3), id))
                wl_resource_set_implementation(resource, &compositorInterface, static_cast<WaylandCompositor*>(data), nullptr);
            else
                wl_client_post_no_memory(client);
        }));
    if (!compositorGlobal) {
        WTFLogAlways("Nested Wayland compositor could not register compositor global");
        return;
    }

    WlUniquePtr<struct wl_global> webkitgtkGlobal(wl_global_create(display.get(), &wl_webkitgtk_interface, 1, this,
        [](struct wl_client* client, void* data, uint32_t version, uint32_t id) {
            if (struct wl_resource* resource = wl_resource_create(client, &wl_webkitgtk_interface, 1, id))
                wl_resource_set_implementation(resource, &webkitgtkInterface, static_cast<WaylandCompositor*>(data), nullptr);
            else
                wl_client_post_no_memory(client);
        }));
    if (!webkitgtkGlobal) {
        WTFLogAlways("Nested Wayland compositor could not register webkitgtk global");
        return;
    }

    if (!initializeEGL()) {
        WTFLogAlways("Nested Wayland compositor could not initialize EGL");
        return;
    }

    if (!eglBindWaylandDisplay(PlatformDisplay::sharedDisplay().eglDisplay(), display.get())) {
        WTFLogAlways("Nested Wayland compositor could not bind nested display");
        return;
    }

    m_displayName = WTFMove(displayName);
    m_display = WTFMove(display);
    m_compositorGlobal = WTFMove(compositorGlobal);
    m_webkitgtkGlobal = WTFMove(webkitgtkGlobal);
    m_eventSource = createWaylandLoopSource(m_display.get());
}

bool WaylandCompositor::getTexture(WebPageProxy& webPage, unsigned& texture, IntSize& textureSize)
{
    if (auto* surface = m_pageMap.get(&webPage))
        return surface->prepareTextureForPainting(texture, textureSize);
    return false;
}

void WaylandCompositor::bindSurfaceToWebPage(WaylandCompositor::Surface* surface, uint64_t pageID)
{
    WebPageProxy* webPage = nullptr;
    for (auto* page : m_pageMap.keys()) {
        if (page->pageID() == pageID) {
            webPage = page;
            break;
        }
    }
    if (!webPage)
        return;

    surface->setWebPage(webPage);
    m_pageMap.set(webPage, surface);
}

void WaylandCompositor::registerWebPage(WebPageProxy& webPage)
{
    m_pageMap.add(&webPage, nullptr);
}

void WaylandCompositor::unregisterWebPage(WebPageProxy& webPage)
{
    if (auto* surface = m_pageMap.take(&webPage))
        surface->setWebPage(nullptr);
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND) && USE(EGL)
