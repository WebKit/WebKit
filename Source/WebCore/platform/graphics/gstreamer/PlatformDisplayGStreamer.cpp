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
#if USE(EGL) && GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif
#undef GST_USE_UNSTABLE_API
#include <wtf/glib/GUniquePtr.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

GstGLDisplay* PlatformDisplay::gstGLDisplay() const
{
#if USE(EGL)
    if (!m_gstGLDisplay)
        m_gstGLDisplay = adoptGRef(GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(eglDisplay())));
#endif
    return m_gstGLDisplay.get();
}

GstGLContext* PlatformDisplay::gstGLContext() const
{
#if USE(EGL)
    if (m_gstGLContext)
        return m_gstGLContext.get();

    auto* gstDisplay = gstGLDisplay();
    if (!gstDisplay)
        return nullptr;

    auto* context = const_cast<PlatformDisplay*>(this)->sharingGLContext();
    if (!context)
        return nullptr;

    m_gstGLContext = adoptGRef(gst_gl_context_new_wrapped(gstDisplay, reinterpret_cast<guintptr>(context->platformContext()), GST_GL_PLATFORM_EGL, GST_GL_API_GLES2));
    {
        GLContext::ScopedGLContextCurrent scopedCurrent(*context);
        if (gst_gl_context_activate(m_gstGLContext.get(), TRUE)) {
            GUniqueOutPtr<GError> error;
            if (!gst_gl_context_fill_info(m_gstGLContext.get(), &error.outPtr()))
                GST_WARNING("Failed to fill in GStreamer context: %s", error->message);
            gst_gl_context_activate(m_gstGLContext.get(), FALSE);
        }
    }
#endif
    return m_gstGLContext.get();
}

void PlatformDisplay::clearGStreamerGLState()
{
    m_gstGLDisplay = nullptr;
    m_gstGLContext = nullptr;
}

} // namespace WebCore

#endif
