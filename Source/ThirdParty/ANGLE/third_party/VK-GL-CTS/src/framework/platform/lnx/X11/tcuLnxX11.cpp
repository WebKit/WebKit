/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief X11 utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxX11.hpp"
#include "gluRenderConfig.hpp"
#include "deMemory.h"

#include <X11/Xutil.h>

namespace tcu
{
namespace lnx
{
namespace x11
{

DisplayBase::DisplayBase(EventState &platform) : m_eventState(platform)
{
}

DisplayBase::~DisplayBase(void)
{
}

WindowBase::WindowBase() : m_visible(false)
{
}

WindowBase::~WindowBase(void)
{
}

XlibDisplay::DisplayState XlibDisplay::s_displayState = XlibDisplay::DISPLAY_STATE_UNKNOWN;

bool XlibDisplay::hasDisplay(const char *name)
{
    if (s_displayState == DISPLAY_STATE_UNKNOWN)
    {
        XInitThreads();
        Display *display = XOpenDisplay((char *)name);
        if (display)
        {
            s_displayState = DISPLAY_STATE_AVAILABLE;
            XCloseDisplay(display);
        }
        else
            s_displayState = DISPLAY_STATE_UNAVAILABLE;
    }
    return s_displayState == DISPLAY_STATE_AVAILABLE ? true : false;
}

XlibDisplay::XlibDisplay(EventState &eventState, const char *name) : DisplayBase(eventState)
{
    // From man:XinitThreads(3):
    //
    //     The XInitThreads function initializes Xlib support for concurrent
    //     threads.  This function must be the first Xlib function
    //     a multi-threaded program calls, and it must complete before any other
    //     Xlib call is made.
    DE_CHECK_RUNTIME_ERR(XInitThreads() != 0);
    m_display = XOpenDisplay((char *)name); // Won't modify argument string.
    if (!m_display)
        throw ResourceError("Failed to open display", name, __FILE__, __LINE__);

    m_deleteAtom = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
}

XlibDisplay::~XlibDisplay(void)
{
    XCloseDisplay(m_display);
}

void XlibDisplay::processEvent(XEvent &event)
{
    switch (event.type)
    {
    case ClientMessage:
        if ((unsigned)event.xclient.data.l[0] == m_deleteAtom)
            m_eventState.setQuitFlag(true);
        break;
    // note: ConfigureNotify for window is handled in setDimensions()
    default:
        break;
    }
}

void XlibDisplay::processEvents(void)
{
    XEvent event;

    while (XPending(m_display))
    {
        XNextEvent(m_display, &event);
        processEvent(event);
    }
}

bool XlibDisplay::getVisualInfo(VisualID visualID, XVisualInfo &dst)
{
    XVisualInfo query;
    query.visualid        = visualID;
    int numVisuals        = 0;
    XVisualInfo *response = XGetVisualInfo(m_display, VisualIDMask, &query, &numVisuals);
    bool succ             = false;

    if (response != nullptr)
    {
        if (numVisuals > 0) // should be 1, but you never know...
        {
            dst  = response[0];
            succ = true;
        }
        XFree(response);
    }

    return succ;
}

::Visual *XlibDisplay::getVisual(VisualID visualID)
{
    XVisualInfo info;

    if (getVisualInfo(visualID, info))
        return info.visual;

    return nullptr;
}

XlibWindow::XlibWindow(XlibDisplay &display, int width, int height, ::Visual *visual)
    : WindowBase()
    , m_display(display)
    , m_colormap(None)
    , m_window(None)
{
    XSetWindowAttributes swa;
    ::Display *const dpy = m_display.getXDisplay();
    ::Window root        = DefaultRootWindow(dpy);
    unsigned long mask   = CWBorderPixel | CWEventMask;

    const bool overrideRedirect = true;

    int depth = CopyFromParent;

    if (overrideRedirect)
    {
        mask |= CWOverrideRedirect;
        swa.override_redirect = true;
    }

    if (visual == nullptr)
        visual = CopyFromParent;
    else
    {
        XVisualInfo info = XVisualInfo();
        bool succ        = display.getVisualInfo(XVisualIDFromVisual(visual), info);

        TCU_CHECK_INTERNAL(succ);

        root         = RootWindow(dpy, info.screen);
        m_colormap   = XCreateColormap(dpy, root, visual, AllocNone);
        swa.colormap = m_colormap;
        mask |= CWColormap;

        depth = info.depth;
    }

    swa.border_pixel = 0;
    swa.event_mask   = ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask;

    if (width == glu::RenderConfig::DONT_CARE)
        width = DEFAULT_WINDOW_WIDTH;
    if (height == glu::RenderConfig::DONT_CARE)
        height = DEFAULT_WINDOW_HEIGHT;

    m_window = XCreateWindow(dpy, root, 0, 0, width, height, 0, depth, InputOutput, visual, mask, &swa);
    TCU_CHECK(m_window);

    /* Prevent the window from stealing input, since our windows are
     * non-interactive.
     */
    XWMHints *hints = XAllocWMHints();
    hints->flags |= InputHint;
    hints->input = False;
    XSetWMHints(dpy, m_window, hints);
    XFree(hints);

    Atom deleteAtom = m_display.getDeleteAtom();
    XSetWMProtocols(dpy, m_window, &deleteAtom, 1);
    XSync(dpy, false);
}

void XlibWindow::setVisibility(bool visible)
{
    ::Display *dpy = m_display.getXDisplay();
    int eventType  = None;
    XEvent event;

    if (visible == m_visible)
        return;

    if (visible)
    {
        XMapWindow(dpy, m_window);
        eventType = MapNotify;
    }
    else
    {
        XUnmapWindow(dpy, m_window);
        eventType = UnmapNotify;
    }

    // We are only interested about exposure/structure notify events, not user input
    XSelectInput(dpy, m_window, ExposureMask | StructureNotifyMask);

    do
    {
        XWindowEvent(dpy, m_window, ExposureMask | StructureNotifyMask, &event);
    } while (event.type != eventType);

    m_visible = visible;
}

void XlibWindow::getDimensions(int *width, int *height) const
{
    int x, y;
    ::Window root;
    unsigned width_, height_, borderWidth, depth;

    XGetGeometry(m_display.getXDisplay(), m_window, &root, &x, &y, &width_, &height_, &borderWidth, &depth);
    if (width != nullptr)
        *width = static_cast<int>(width_);
    if (height != nullptr)
        *height = static_cast<int>(height_);
}

void XlibWindow::setDimensions(int width, int height)
{
    ::Display *dpy = m_display.getXDisplay();
    int width_, height_ = 0;

    XResizeWindow(dpy, m_window, width, height);
    getDimensions(&width_, &height_);
    DE_ASSERT(width_ == width && height_ == height);
}

void XlibWindow::processEvents(void)
{
    // A bit of a hack, since we don't really handle all the events.
    m_display.processEvents();
}

XlibWindow::~XlibWindow(void)
{
    XDestroyWindow(m_display.getXDisplay(), m_window);
    if (m_colormap != None)
        XFreeColormap(m_display.getXDisplay(), m_colormap);
}

} // namespace x11
} // namespace lnx
} // namespace tcu
