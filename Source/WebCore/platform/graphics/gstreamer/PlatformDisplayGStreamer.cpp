/*
 * Copyright (C) 2020 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PlatformDisplay.h"

#if USE(GSTREAMER_GL)

#include "GLContext.h"
#include "GStreamerCommon.h"
#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif
#undef GST_USE_UNSTABLE_API
#include <wtf/glib/GUniquePtr.h>

GST_DEBUG_CATEGORY(webkit_display_debug);
#define GST_CAT_DEFAULT webkit_display_debug

namespace WebCore {

static void ensureDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_display_debug, "webkitdisplay", 0, "WebKit Display");
    });
}

GstGLDisplay* PlatformDisplay::gstGLDisplay() const
{
    ensureDebugCategoryInitialized();
    if (!m_gstGLDisplay)
        m_gstGLDisplay = adoptGRef(GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(eglDisplay())));
    GST_TRACE("Using GL display %" GST_PTR_FORMAT, m_gstGLDisplay.get());
    return m_gstGLDisplay.get();
}

GstGLContext* PlatformDisplay::gstGLContext() const
{
    ensureDebugCategoryInitialized();

    if (m_gstGLContext)
        return m_gstGLContext.get();

    auto* gstDisplay = gstGLDisplay();
    if (!gstDisplay) {
        GST_ERROR("No GL display");
        return nullptr;
    }

    auto* context = const_cast<PlatformDisplay*>(this)->sharingGLContext();
    if (!context) {
        GST_ERROR("No sharing GL context");
        return nullptr;
    }

    m_gstGLContext = adoptGRef(gst_gl_context_new_wrapped(gstDisplay, reinterpret_cast<guintptr>(context->platformContext()), GST_GL_PLATFORM_EGL, GST_GL_API_GLES2));
    {
        GLContext::ScopedGLContextCurrent scopedCurrent(*context);
        if (gst_gl_context_activate(m_gstGLContext.get(), TRUE)) {
            GUniqueOutPtr<GError> error;
            if (!gst_gl_context_fill_info(m_gstGLContext.get(), &error.outPtr()))
                GST_ERROR("Failed to fill in GStreamer context: %s", error->message);
            gst_gl_context_activate(m_gstGLContext.get(), FALSE);
        }
    }
    GST_DEBUG("Created GL context %" GST_PTR_FORMAT, m_gstGLContext.get());
    return m_gstGLContext.get();
}

void PlatformDisplay::clearGStreamerGLState()
{
    m_gstGLDisplay = nullptr;
    m_gstGLContext = nullptr;
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_GL)
