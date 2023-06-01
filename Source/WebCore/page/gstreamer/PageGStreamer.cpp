/*
 *  Copyright (C) 2023 Igalia, S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Page.h"

#if ENABLE(VIDEO) && USE(COORDINATED_GRAPHICS) && USE(GSTREAMER_GL)
#include "GLContext.h"
#include "PlatformDisplay.h"
#include <epoxy/egl.h>
#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#undef GST_USE_UNSTABLE_API
#include <wtf/glib/GUniquePtr.h>
#include <wtf/threads/BinarySemaphore.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

#if USE(EGL)
static GstGLAPI queryGstGLAPI()
{
    switch (eglQueryAPI()) {
    case EGL_OPENGL_API:
        return GST_GL_API_OPENGL;
    case EGL_OPENGL_ES_API:
        return GST_GL_API_GLES2;
    default:
        break;
    }

    return GST_GL_API_NONE;
}
#endif

GstGLContext* Page::gstGLContext()
{
#if USE(EGL)
    if (m_gstGLContext)
        return m_gstGLContext.get();

    if (!m_compositingRunLoop)
        return nullptr;

    auto* gstGLDisplay = PlatformDisplay::sharedDisplayForCompositing().gstGLDisplay();
    if (!gstGLDisplay)
        return nullptr;

    GRefPtr<GstGLContext> gstGLContext;
    BinarySemaphore semaphore;
    m_compositingRunLoop->dispatch([&semaphore, gstGLDisplay, &gstGLContext]() {
        if (auto* context = GLContext::current()) {
            GstGLAPI glAPI = queryGstGLAPI();
            if (glAPI != GST_GL_API_NONE) {
                gstGLContext = adoptGRef(gst_gl_context_new_wrapped(gstGLDisplay, reinterpret_cast<guintptr>(context->platformContext()), GST_GL_PLATFORM_EGL, glAPI));
                if (gst_gl_context_activate(gstGLContext.get(), TRUE)) {
                    GUniqueOutPtr<GError> error;
                    if (!gst_gl_context_fill_info(gstGLContext.get(), &error.outPtr()))
                        GST_WARNING("Failed to fill in GStreamer context: %s", error->message);
                } else
                    GST_WARNING("Failed to activate GStreamer context %" GST_PTR_FORMAT, gstGLContext.get());
            }
        }
        semaphore.signal();
    });
    semaphore.wait();

    if (!gstGLContext)
        return nullptr;

    m_gstGLContext = WTFMove(gstGLContext);
    return m_gstGLContext.get();
#else
    return nullptr;
#endif
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(COORDINATED_GRAPHICS) && USE(GSTREAMER_GL)
