/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if USE(3D_GRAPHICS) || USE(ACCELERATED_COMPOSITING)
#include "GraphicsContext3DPrivate.h"

#include "GraphicsContext.h"
#include "HostWindow.h"
#include "NotImplemented.h"
#include <Ecore_Evas.h>
#include <Evas_GL.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D* context, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
    : m_context(context)
    , m_hostWindow(hostWindow)
    , m_evasGL(0)
    , m_evasGLContext(0)
    , m_evasGLSurface(0)
    , m_glContext(0)
    , m_glSurface(0)
    , m_api(0)
    , m_renderStyle(renderStyle)
{
    if (renderStyle == GraphicsContext3D::RenderToCurrentGLContext)
        return;

    if (m_hostWindow && m_hostWindow->platformPageClient()) {
        // FIXME: Implement this code path for WebKit1.
        // Get Evas object from platformPageClient and set EvasGL related members.
        return;
    }

    // For WebKit2, we need to create a dummy ecoreEvas object for the WebProcess in order to use EvasGL APIs.
#ifdef HAVE_ECORE_X
    ecore_evas_init();
    m_ecoreEvas = adoptPtr(ecore_evas_gl_x11_new(0, 0, 0, 0, 1, 1));
    if (!m_ecoreEvas)
        return;
#else
    return;
#endif

    Evas* evas = ecore_evas_get(m_ecoreEvas.get());
    if (!evas)
        return;

    // Create a new Evas_GL object for gl rendering on efl.
    m_evasGL = evas_gl_new(evas);
    if (!m_evasGL)
        return;

    // Get the API for rendering using OpenGL.
    // This returns a structure that contains all the OpenGL functions we can use to render in Evas
    m_api = evas_gl_api_get(m_evasGL);
    if (!m_api)
        return;

    // Create a context
    m_evasGLContext = evas_gl_context_create(m_evasGL, 0);
    if (!m_evasGLContext)
        return;

    // Create a surface
    if (!createSurface(0, renderStyle == GraphicsContext3D::RenderDirectlyToHostWindow))
        return;

    makeContextCurrent();
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
    if (!m_evasGL)
        return;

    if (m_evasGLSurface)
        evas_gl_surface_destroy(m_evasGL, m_evasGLSurface);

    if (m_evasGLContext)
        evas_gl_context_destroy(m_evasGL, m_evasGLContext);

    evas_gl_free(m_evasGL);
}


bool GraphicsContext3DPrivate::createSurface(PageClientEfl* pageClient, bool renderDirectlyToHostWindow)
{
    // If RenderStyle is RenderOffscreen, we will be rendering to a FBO,
    // so Evas_GL_Surface has a 1x1 dummy surface.
    int width = 1;
    int height = 1;

    // But, in case of RenderDirectlyToHostWindow, we have to render to a render target surface with the same size as our webView.
    if (renderDirectlyToHostWindow) {
        if (!pageClient)
            return false;
        // FIXME: Get geometry of webView and set size of target surface.
    }

    Evas_GL_Config config = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE, // FIXME: set EVAS_GL_STENCIL_BIT_8 after fixing Evas_GL bug.
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    // Create a new Evas_GL_Surface object
    m_evasGLSurface = evas_gl_surface_create(m_evasGL, &config, width, height);
    if (!m_evasGLSurface)
        return false;

#if USE(ACCELERATED_COMPOSITING)
    if (renderDirectlyToHostWindow) {
        Evas_Native_Surface nativeSurface;
        // Fill in the Native Surface information from the given Evas GL surface.
        evas_gl_native_surface_get(m_evasGL, m_evasGLSurface, &nativeSurface);

        // FIXME: Create and specially set up a evas_object which act as the render targer surface.
    }
#endif

    return true;
}

void GraphicsContext3DPrivate::setCurrentGLContext(void* context, void* surface)
{
    m_glContext = context;
    m_glSurface = surface;
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformGraphicsContext3D() const
{
    if (m_renderStyle == GraphicsContext3D::RenderToCurrentGLContext)
        return m_glContext;

    return m_evasGLContext;
}

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    return evas_gl_make_current(m_evasGL, m_evasGLSurface, m_evasGLContext);
}

#if USE(TEXTURE_MAPPER_GL)
void GraphicsContext3DPrivate::paintToTextureMapper(TextureMapper*, const FloatRect& /* target */, const TransformationMatrix&, float /* opacity */, BitmapTexture* /* mask */)
{
    notImplemented();
}
#endif
} // namespace WebCore

#endif // USE(3D_GRAPHICS) || USE(ACCELERATED_COMPOSITING)
