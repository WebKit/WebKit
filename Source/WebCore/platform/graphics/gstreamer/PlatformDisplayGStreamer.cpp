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

#include "GLContext.h"
#include "GStreamerCommon.h"
#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#if USE(EGL) && GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif
#undef GST_USE_UNSTABLE_API

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
    if (!m_gstGLContext) {
        if (auto* gstDisplay = gstGLDisplay()) {
            if (auto* context = const_cast<PlatformDisplay*>(this)->sharingGLContext()) {
#if USE(OPENGL_ES)
                GstGLAPI glAPI = GST_GL_API_GLES2;
#elif USE(OPENGL)
                GstGLAPI glAPI = GST_GL_API_OPENGL;
#endif
                m_gstGLContext = adoptGRef(gst_gl_context_new_wrapped(gstDisplay, reinterpret_cast<guintptr>(context->platformContext()), GST_GL_PLATFORM_EGL, glAPI));
            }
        }
    }
#endif
    return m_gstGLContext.get();
}

} // namespace WebCore
