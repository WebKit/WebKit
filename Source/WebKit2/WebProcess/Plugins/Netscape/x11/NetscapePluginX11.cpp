/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if PLUGIN_ARCHITECTURE(X11)

#include "NetscapePlugin.h"

#include "WebEvent.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/NotImplemented.h>

#if PLATFORM(QT)
#include <QApplication>
#include <QDesktopWidget>
#include <QPixmap>
#include <QX11Info>
#elif PLATFORM(GTK)
#include <gdk/gdkx.h>
#include <WebCore/GtkVersioning.h>
#endif

using namespace WebCore;

namespace WebKit {

static Display *getPluginDisplay()
{
#if PLATFORM(QT)
    // At the moment, we only support gdk based plugins (like Flash) that use a different X connection.
    // The code below has the same effect as this one:
    // Display *gdkDisplay = gdk_x11_display_get_xdisplay(gdk_display_get_default());

    QLibrary library(QLatin1String("libgdk-x11-2.0"), 0);
    if (!library.load())
        return 0;

    typedef void *(*gdk_display_get_default_ptr)();
    gdk_display_get_default_ptr gdk_display_get_default = (gdk_display_get_default_ptr)library.resolve("gdk_display_get_default");
    if (!gdk_display_get_default)
        return 0;

    typedef void *(*gdk_x11_display_get_xdisplay_ptr)(void *);
    gdk_x11_display_get_xdisplay_ptr gdk_x11_display_get_xdisplay = (gdk_x11_display_get_xdisplay_ptr)library.resolve("gdk_x11_display_get_xdisplay");
    if (!gdk_x11_display_get_xdisplay)
        return 0;

    return (Display*)gdk_x11_display_get_xdisplay(gdk_display_get_default());
#elif PLATFORM(GTK)
    // Since we're a gdk/gtk app, we'll (probably?) have the same X connection as any gdk-based
    // plugins, so we can return that. We might want to add other implementations here later.
    return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
#else
    return 0;
#endif
}

static inline Display* x11Display()
{
#if PLATFORM(QT)
    return QX11Info::display();
#elif PLATFORM(GTK)
    return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
#else
    return 0;
#endif
}

static inline int displayDepth()
{
#if PLATFORM(QT)
    return QApplication::desktop()->x11Info().depth();
#elif PLATFORM(GTK)
    return gdk_visual_get_depth(gdk_screen_get_system_visual(gdk_screen_get_default()));
#else
    return 0;
#endif
}

static inline unsigned long rootWindowID()
{
#if PLATFORM(QT)
    return QX11Info::appRootWindow();
#elif PLATFORM(GTK)
    return GDK_ROOT_WINDOW();
#else
    return 0;
#endif
}

static inline int x11Screen()
{
#if PLATFORM(QT)
    return QX11Info::appScreen();
#elif PLATFORM(GTK)
    return gdk_screen_get_number(gdk_screen_get_default());
#else
    return 0;
#endif
}

bool NetscapePlugin::platformPostInitialize()
{
    if (m_isWindowed)
        return false;

    if (!(m_pluginDisplay = getPluginDisplay()))
        return false;

    NPSetWindowCallbackStruct* callbackStruct = new NPSetWindowCallbackStruct;
    callbackStruct->type = 0;
    Display* display = x11Display();
    int depth = displayDepth();
    callbackStruct->display = display;
    callbackStruct->depth = depth;

    XVisualInfo visualTemplate;
    visualTemplate.screen = x11Screen();
    visualTemplate.depth = depth;
    visualTemplate.c_class = TrueColor;
    int numMatching;
    XVisualInfo* visualInfo = XGetVisualInfo(display, VisualScreenMask | VisualDepthMask | VisualClassMask,
                                             &visualTemplate, &numMatching);
    ASSERT(visualInfo);
    Visual* visual = visualInfo[0].visual;
    ASSERT(visual);
    XFree(visualInfo);

    callbackStruct->visual = visual;
    callbackStruct->colormap = XCreateColormap(display, rootWindowID(), visual, AllocNone);

    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = 0;
    m_npWindow.ws_info = callbackStruct;

    callSetWindow();

    return true;
}

void NetscapePlugin::platformDestroy()
{
    delete static_cast<NPSetWindowCallbackStruct*>(m_npWindow.ws_info);

    if (m_drawable) {
        XFreePixmap(x11Display(), m_drawable);
        m_drawable = 0;
    }
}

bool NetscapePlugin::platformInvalidate(const IntRect&)
{
    notImplemented();
    return false;
}

void NetscapePlugin::platformGeometryDidChange()
{
    if (m_isWindowed) {
        notImplemented();
        return;
    }

    Display* display = x11Display();
    if (m_drawable)
        XFreePixmap(display, m_drawable);

    m_drawable = XCreatePixmap(display, rootWindowID(), m_frameRect.width(), m_frameRect.height(), displayDepth());

    XSync(display, false); // Make sure that the server knows about the Drawable.
}

void NetscapePlugin::platformPaint(GraphicsContext* context, const IntRect& dirtyRect, bool /*isSnapshot*/)
{
    if (m_isWindowed) {
        notImplemented();
        return;
    }

    if (!m_isStarted) {
        // FIXME: we should paint a missing plugin icon.
        return;
    }

    if (context->paintingDisabled())
        return;

    ASSERT(m_drawable);

#if PLATFORM(QT)
    QPainter* painter = context->platformContext();
    painter->translate(m_frameRect.x(), m_frameRect.y());
#else
    notImplemented();
    return;
#endif

    XEvent xevent;
    memset(&xevent, 0, sizeof(XEvent));
    XGraphicsExposeEvent& exposeEvent = xevent.xgraphicsexpose;
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = x11Display();
    exposeEvent.drawable = m_drawable;

    IntRect exposedRect(dirtyRect);
    exposedRect.intersect(m_frameRect);
    exposedRect.move(-m_frameRect.x(), -m_frameRect.y());
    exposeEvent.x = exposedRect.x();
    exposeEvent.y = exposedRect.y();

    // Note: in transparent mode Flash thinks width is the right and height is the bottom.
    // We should take it into account if we want to support transparency.
    exposeEvent.width = exposedRect.width();
    exposeEvent.height = exposedRect.height();

    NPP_HandleEvent(&xevent);

    if (m_pluginDisplay != x11Display())
        XSync(m_pluginDisplay, false);

#if PLATFORM(QT)
    QPixmap qtDrawable = QPixmap::fromX11Pixmap(m_drawable, QPixmap::ExplicitlyShared);
    ASSERT(qtDrawable.depth() == static_cast<NPSetWindowCallbackStruct*>(m_npWindow.ws_info)->depth);
    painter->drawPixmap(QPoint(exposedRect.x(), exposedRect.y()), qtDrawable, exposedRect);

    painter->translate(-m_frameRect.x(), -m_frameRect.y());
#endif
}

static inline void initializeXEvent(XEvent& event)
{
    memset(&event, 0, sizeof(XEvent));
    event.xany.serial = 0;
    event.xany.send_event = false;
    event.xany.display = x11Display();
    event.xany.window = 0;
}

static inline uint64_t xTimeStamp(double timestampInSeconds)
{
    return timestampInSeconds * 1000;
}

static inline unsigned xKeyModifiers(const WebEvent& event)
{
    unsigned xModifiers = 0;
    if (event.controlKey())
        xModifiers |= ControlMask;
    if (event.shiftKey())
        xModifiers |= ShiftMask;
    if (event.altKey())
        xModifiers |= Mod1Mask;
    if (event.metaKey())
        xModifiers |= Mod4Mask;

    return xModifiers;
}

template <typename XEventType>
static inline void setCommonMouseEventFields(XEventType& xEvent, const WebMouseEvent& webEvent, const WebCore::IntPoint& pluginLocation)
{
    xEvent.root = rootWindowID();
    xEvent.subwindow = 0;
    xEvent.time = xTimeStamp(webEvent.timestamp());
    xEvent.x = webEvent.position().x() - pluginLocation.x();
    xEvent.y = webEvent.position().y() - pluginLocation.y();
    xEvent.x_root = webEvent.globalPosition().x();
    xEvent.y_root = webEvent.globalPosition().y();
    xEvent.state = xKeyModifiers(webEvent);
    xEvent.same_screen = true;
}

static inline void setXMotionEventFields(XEvent& xEvent, const WebMouseEvent& webEvent, const WebCore::IntPoint& pluginLocation)
{
    XMotionEvent& xMotion = xEvent.xmotion;
    setCommonMouseEventFields(xMotion, webEvent, pluginLocation);
    xMotion.type = MotionNotify;
}

static inline void setXButtonEventFields(XEvent& xEvent, const WebMouseEvent& webEvent, const WebCore::IntPoint& pluginLocation)
{
    XButtonEvent& xButton = xEvent.xbutton;
    setCommonMouseEventFields(xButton, webEvent, pluginLocation);

    xButton.type = (webEvent.type() == WebEvent::MouseDown) ? ButtonPress : ButtonRelease;
    switch (webEvent.button()) {
    case WebMouseEvent::LeftButton:
        xButton.button = Button1;
        break;
    case WebMouseEvent::MiddleButton:
        xButton.button = Button2;
        break;
    case WebMouseEvent::RightButton:
        xButton.button = Button3;
        break;
    }
}

static inline void setXCrossingEventFields(XEvent& xEvent, const WebMouseEvent& webEvent, const WebCore::IntPoint& pluginLocation, int type)
{
    XCrossingEvent& xCrossing = xEvent.xcrossing;
    setCommonMouseEventFields(xCrossing, webEvent, pluginLocation);

    xCrossing.type = type;
    xCrossing.mode = NotifyNormal;
    xCrossing.detail = NotifyDetailNone;
    xCrossing.focus = false;
}

bool NetscapePlugin::platformHandleMouseEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    XEvent xEvent;
    initializeXEvent(xEvent);

    switch (event.type()) {
    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
        setXButtonEventFields(xEvent, event, m_frameRect.location());
        break;
    case WebEvent::MouseMove:
        setXMotionEventFields(xEvent, event, m_frameRect.location());
        break;
    }

    return NPP_HandleEvent(&xEvent);
}

// We undefine these constants in npruntime_internal.h to avoid collision
// with WebKit and platform headers. Values are defined in X.h.
const int kKeyPressType = 2;
const int kKeyReleaseType = 3;
const int kFocusInType = 9;
const int kFocusOutType = 10;

bool NetscapePlugin::platformHandleWheelEvent(const WebWheelEvent&)
{
    notImplemented();
    return false;
}

void NetscapePlugin::platformSetFocus(bool)
{
    notImplemented();
}

bool NetscapePlugin::platformHandleMouseEnterEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXCrossingEventFields(xEvent, event, m_frameRect.location(), EnterNotify);

    return NPP_HandleEvent(&xEvent);
}

bool NetscapePlugin::platformHandleMouseLeaveEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXCrossingEventFields(xEvent, event, m_frameRect.location(), LeaveNotify);

    return NPP_HandleEvent(&xEvent);
}

static inline void setXKeyEventFields(XEvent& xEvent, const WebKeyboardEvent& webEvent)
{
    xEvent.xany.type = (webEvent.type() == WebEvent::KeyDown) ? kKeyPressType : kKeyReleaseType;
    XKeyEvent& xKey = xEvent.xkey;
    xKey.root = rootWindowID();
    xKey.subwindow = 0;
    xKey.time = xTimeStamp(webEvent.timestamp());
    xKey.state = xKeyModifiers(webEvent);
    xKey.keycode = webEvent.nativeVirtualKeyCode();

    xKey.same_screen = true;

    // Key events propagated to the plugin does not need to have position.
    // source: https://developer.mozilla.org/en/NPEvent
    xKey.x = 0;
    xKey.y = 0;
    xKey.x_root = 0;
    xKey.y_root = 0;
}

bool NetscapePlugin::platformHandleKeyboardEvent(const WebKeyboardEvent& event)
{
    // We don't generate other types of keyboard events via WebEventFactory.
    ASSERT(event.type() == WebEvent::KeyDown || event.type() == WebEvent::KeyUp);

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXKeyEventFields(xEvent, event);

    return NPP_HandleEvent(&xEvent);
}

} // namespace WebKit

#endif // PLUGIN_ARCHITECTURE(X11)
