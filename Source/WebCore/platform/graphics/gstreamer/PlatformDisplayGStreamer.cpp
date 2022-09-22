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

#include "GStreamerCommon.h"

#if USE(GLX)
#include "GLContextGLX.h"
#endif

#if USE(EGL)
#include "GLContextEGL.h"
#endif

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#endif

#if PLATFORM(WAYLAND)
#include "PlatformDisplayWayland.h"
#endif

#if USE(WPE_RENDERER)
#include "PlatformDisplayLibWPE.h"
#endif

#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#if USE(GLX) && GST_GL_HAVE_PLATFORM_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#if USE(EGL) && GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif
#undef GST_USE_UNSTABLE_API

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

using namespace WebCore;

static GstGLDisplay* createGstGLDisplay(const PlatformDisplay& sharedDisplay, GstGLPlatform glPlatform)
{
#if USE(EGL)
    if (glPlatform == GST_GL_PLATFORM_EGL)
        return GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(sharedDisplay.eglDisplay()));
#endif
#if USE(GLX)
    if (is<PlatformDisplayX11>(sharedDisplay) && glPlatform == GST_GL_PLATFORM_GLX)
        return GST_GL_DISPLAY(gst_gl_display_x11_new_with_display(downcast<PlatformDisplayX11>(sharedDisplay).native()));
#endif

    return nullptr;
}

bool PlatformDisplay::tryEnsureGstGLContext() const
{
    if (m_gstGLDisplay && m_gstGLContext)
        return true;

#if USE(OPENGL_ES)
    GstGLAPI glAPI = GST_GL_API_GLES2;
#elif USE(OPENGL)
    GstGLAPI glAPI = GST_GL_API_OPENGL;
#else
    return false;
#endif

    auto* sharedContext = const_cast<PlatformDisplay*>(this)->sharingGLContext();
    if (!sharedContext)
        return false;

    GCGLContext contextHandle = sharedContext->platformContext();
    if (!contextHandle)
        return false;

    GstGLPlatform glPlatform = sharedContext->isEGLContext() ? GST_GL_PLATFORM_EGL : GST_GL_PLATFORM_GLX;
    m_gstGLDisplay = adoptGRef(createGstGLDisplay(*this, glPlatform));
    if (!m_gstGLDisplay)
        return false;

    m_gstGLContext = adoptGRef(gst_gl_context_new_wrapped(m_gstGLDisplay.get(), reinterpret_cast<guintptr>(contextHandle), glPlatform, glAPI));

    // Activate and fill the GStreamer wrapped context with the Webkit's shared one.
    auto* previousActiveContext = GLContext::current();
    sharedContext->makeContextCurrent();
    if (gst_gl_context_activate(m_gstGLContext.get(), TRUE)) {
        GUniqueOutPtr<GError> error;
        if (!gst_gl_context_fill_info(m_gstGLContext.get(), &error.outPtr()))
            GST_WARNING("Failed to fill in GStreamer context: %s", error->message);
    } else
        GST_WARNING("Failed to activate GStreamer context %" GST_PTR_FORMAT, m_gstGLContext.get());
    if (previousActiveContext)
        previousActiveContext->makeContextCurrent();

    return true;
}

GstGLDisplay* PlatformDisplay::gstGLDisplay() const
{
    if (!tryEnsureGstGLContext())
        return nullptr;
    return m_gstGLDisplay.get();
}

GstGLContext* PlatformDisplay::gstGLContext() const
{
    if (!tryEnsureGstGLContext())
        return nullptr;
    return m_gstGLContext.get();
}
