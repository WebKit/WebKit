/*
 * Copyright (C) 2012 Igalia, S.L.
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
#include "WindowGLContext.h"

#include <GL/glx.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

PassOwnPtr<WindowGLContext> WindowGLContext::createContextWithGdkWindow(GdkWindow* window)
{
    OwnPtr<WindowGLContext> context = adoptPtr(new WindowGLContext(window));
    if (!context->m_context)
        return nullptr;
    return context.release();
}

WindowGLContext::WindowGLContext(GdkWindow* window)
    : m_window(window)
    , m_context(0)
    , m_display(0)
    , m_needToCloseDisplay(0)
{
    GdkDisplay* gdkDisplay = gdk_window_get_display(m_window);
    if (gdkDisplay)
        m_display = GDK_DISPLAY_XDISPLAY(gdkDisplay);
    else {
        m_display = XOpenDisplay(0);
        m_needToCloseDisplay = true;
    }

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(m_display, GDK_WINDOW_XID(m_window), &attributes))
        return;

    XVisualInfo visualInfo;
    visualInfo.visualid = XVisualIDFromVisual(attributes.visual);

    int numReturned = 0;
    XVisualInfo* visualInfoList = XGetVisualInfo(m_display, VisualIDMask, &visualInfo, &numReturned);
    m_context = glXCreateContext(m_display, visualInfoList, 0, True);
    XFree(visualInfoList);
}

WindowGLContext::~WindowGLContext()
{
    if (!m_context)
        return;
    glXMakeCurrent(m_display, None, None);
    glXDestroyContext(m_display, m_context);

    if (m_needToCloseDisplay)
        XCloseDisplay(m_display);
}

void WindowGLContext::startDrawing()
{
    glXMakeCurrent(m_display, GDK_WINDOW_XID(m_window), m_context);
}

void WindowGLContext::finishDrawing()
{
    glXSwapBuffers(m_display, GDK_WINDOW_XID(m_window));
}

} // namespace WebCore
