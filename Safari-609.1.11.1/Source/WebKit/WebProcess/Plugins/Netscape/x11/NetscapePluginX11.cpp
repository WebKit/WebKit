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
#include "NetscapePluginX11.h"

#if PLATFORM(X11) && ENABLE(NETSCAPE_PLUGIN_API)

#include "NetscapePlugin.h"
#include "PluginController.h"
#include "WebEvent.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformDisplayX11.h>
#include <WebCore/XUniquePtr.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#endif

#if USE(CAIRO)
#include <WebCore/PlatformContextCairo.h>
#include <WebCore/RefPtrCairo.h>
#include <cairo/cairo-xlib.h>
#endif

namespace WebKit {
using namespace WebCore;

static inline Display* x11HostDisplay()
{
    return downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native();
}

static Display* getPluginDisplay()
{
#if PLATFORM(GTK)
    // Since we're a gdk/gtk app, we'll (probably?) have the same X connection as any gdk-based
    // plugins, so we can return that. We might want to add other implementations here later.
    return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
#else
    return nullptr;
#endif
}

static inline int x11Screen()
{
#if PLATFORM(GTK)
    return gdk_screen_get_number(gdk_screen_get_default());
#else
    return 0;
#endif
}

static inline int displayDepth()
{
#if PLATFORM(GTK)
    return gdk_visual_get_depth(gdk_screen_get_system_visual(gdk_screen_get_default()));
#else
    return 0;
#endif
}

static inline unsigned long rootWindowID()
{
#if PLATFORM(GTK)
    return GDK_ROOT_WINDOW();
#else
    return 0;
#endif
}

#if PLATFORM(GTK)
static gboolean socketPlugRemovedCallback(GtkSocket*)
{
    // Default action is to destroy the GtkSocket, so we just return TRUE here
    // to be able to reuse the socket. For some obscure reason, newer versions
    // of flash plugin remove the plug from the socket, probably because the plug
    // created by the plugin is re-parented.
    return TRUE;
}
#endif

std::unique_ptr<NetscapePluginX11> NetscapePluginX11::create(NetscapePlugin& plugin)
{
#if PLATFORM(GTK)
    uint64_t windowID = 0;
#endif
    if (plugin.isWindowed()) {
#if PLATFORM(GTK)
        // NPPVplugiNeedsXEmbed is a boolean value, but at least the
        // Flash player plugin is using an 'int' instead.
        int needsXEmbed = 0;
        plugin.NPP_GetValue(NPPVpluginNeedsXEmbed, &needsXEmbed);
        if (needsXEmbed) {
            windowID = plugin.controller()->createPluginContainer();
            if (!windowID)
                return nullptr;
        } else {
            notImplemented();
            return nullptr;
        }
#else
        notImplemented();
        return nullptr;
#endif
    }

    Display* display = getPluginDisplay();
    if (!display)
        return nullptr;

#if PLATFORM(GTK)
    if (plugin.isWindowed())
        return makeUnique<NetscapePluginX11>(plugin, display, windowID);
#endif

    return makeUnique<NetscapePluginX11>(plugin, display);
}

NetscapePluginX11::NetscapePluginX11(NetscapePlugin& plugin, Display* display)
    : m_plugin(plugin)
    , m_pluginDisplay(display)
{
    Display* hostDisplay = x11HostDisplay();
    int depth = displayDepth();
    m_setWindowCallbackStruct.display = hostDisplay;
    m_setWindowCallbackStruct.depth = depth;

    XVisualInfo visualTemplate;
    visualTemplate.screen = x11Screen();
    visualTemplate.depth = depth;
    visualTemplate.c_class = TrueColor;
    int numMatching;
    XUniquePtr<XVisualInfo> visualInfo(XGetVisualInfo(hostDisplay, VisualScreenMask | VisualDepthMask | VisualClassMask, &visualTemplate, &numMatching));
    ASSERT(visualInfo);
    Visual* visual = visualInfo.get()[0].visual;
    ASSERT(visual);

    m_setWindowCallbackStruct.type = NP_SETWINDOW;
    m_setWindowCallbackStruct.visual = visual;
    m_setWindowCallbackStruct.colormap = XCreateColormap(hostDisplay, rootWindowID(), visual, AllocNone);
}

#if PLATFORM(GTK)
NetscapePluginX11::NetscapePluginX11(NetscapePlugin& plugin, Display* display, uint64_t windowID)
    : m_plugin(plugin)
    , m_pluginDisplay(display)
{
    // It seems flash needs the socket to be in the same process,
    // I guess it uses gdk_window_lookup(), so we create a new socket here
    // containing a plug with the UI process socket embedded.
    m_platformPluginWidget = gtk_plug_new(static_cast<Window>(windowID));

    // Hide the GtkPlug on delete-event since we assume the widget is valid while the plugin is active.
    // platformDestroy() will be called anyway right after the delete-event.
    g_signal_connect(m_platformPluginWidget, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

    GtkWidget* socket = gtk_socket_new();
    // Do not show the plug widget until the socket is connected.
    g_signal_connect_swapped(socket, "plug-added", G_CALLBACK(gtk_widget_show), m_platformPluginWidget);
    g_signal_connect(socket, "plug-removed", G_CALLBACK(socketPlugRemovedCallback), nullptr);
    gtk_container_add(GTK_CONTAINER(m_platformPluginWidget), socket);
    gtk_widget_show(socket);

    Display* hostDisplay = x11HostDisplay();
    m_npWindowID = gtk_socket_get_id(GTK_SOCKET(socket));
    GdkWindow* window = gtk_widget_get_window(socket);
    m_setWindowCallbackStruct.type = NP_SETWINDOW;
    m_setWindowCallbackStruct.display = GDK_WINDOW_XDISPLAY(window);
    m_setWindowCallbackStruct.visual = GDK_VISUAL_XVISUAL(gdk_window_get_visual(window));
    m_setWindowCallbackStruct.depth = gdk_visual_get_depth(gdk_window_get_visual(window));
    m_setWindowCallbackStruct.colormap = XCreateColormap(hostDisplay, GDK_ROOT_WINDOW(), m_setWindowCallbackStruct.visual, AllocNone);

    XFlush(hostDisplay);
}
#endif

NetscapePluginX11::~NetscapePluginX11()
{
    XFreeColormap(x11HostDisplay(), m_setWindowCallbackStruct.colormap);

    m_drawable.reset();

#if PLATFORM(GTK)
    if (m_platformPluginWidget)
        gtk_widget_destroy(m_platformPluginWidget);
#endif
}

NPWindowType NetscapePluginX11::windowType() const
{
    return m_plugin.isWindowed() ? NPWindowTypeWindow : NPWindowTypeDrawable;
}

void* NetscapePluginX11::window() const
{
#if PLATFORM(GTK)
    return m_plugin.isWindowed() ? GINT_TO_POINTER(m_npWindowID) : nullptr;
#else
    return nullptr;
#endif
}

void NetscapePluginX11::geometryDidChange()
{
    if (m_plugin.isWindowed()) {
        uint64_t windowID = 0;
#if PLATFORM(GTK)
        if (!gtk_plug_get_embedded(GTK_PLUG(m_platformPluginWidget)))
            return;
        windowID = static_cast<uint64_t>(GDK_WINDOW_XID(gtk_plug_get_socket_window(GTK_PLUG(m_platformPluginWidget))));
#endif
        m_plugin.controller()->windowedPluginGeometryDidChange(m_plugin.frameRectInWindowCoordinates(), m_plugin.clipRect(), windowID);
        return;
    }

    m_drawable.reset();
    if (m_plugin.size().isEmpty())
        return;

    m_drawable = XCreatePixmap(x11HostDisplay(), rootWindowID(), m_plugin.size().width(), m_plugin.size().height(), displayDepth());
    XSync(x11HostDisplay(), false); // Make sure that the server knows about the Drawable.
}

void NetscapePluginX11::visibilityDidChange()
{
    ASSERT(m_plugin.isWindowed());
    uint64_t windowID = 0;
#if PLATFORM(GTK)
    if (!gtk_plug_get_embedded(GTK_PLUG(m_platformPluginWidget)))
        return;
    windowID = static_cast<uint64_t>(GDK_WINDOW_XID(gtk_plug_get_socket_window(GTK_PLUG(m_platformPluginWidget))));
#endif
    m_plugin.controller()->windowedPluginVisibilityDidChange(m_plugin.isVisible(), windowID);
    m_plugin.controller()->windowedPluginGeometryDidChange(m_plugin.frameRectInWindowCoordinates(), m_plugin.clipRect(), windowID);
}

void NetscapePluginX11::paint(GraphicsContext& context, const IntRect& dirtyRect)
{
    ASSERT(!m_plugin.isWindowed());

    if (context.paintingDisabled() || !m_drawable)
        return;

    XEvent xevent;
    memset(&xevent, 0, sizeof(XEvent));
    XGraphicsExposeEvent& exposeEvent = xevent.xgraphicsexpose;
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = x11HostDisplay();
    exposeEvent.drawable = m_drawable.get();

    IntRect exposedRect(dirtyRect);
    exposeEvent.x = exposedRect.x();
    exposeEvent.y = exposedRect.y();

    // Note: in transparent mode Flash thinks width is the right and height is the bottom.
    // We should take it into account if we want to support transparency.
    exposeEvent.width = exposedRect.width();
    exposeEvent.height = exposedRect.height();

    m_plugin.NPP_HandleEvent(&xevent);

    if (m_pluginDisplay != x11HostDisplay())
        XSync(m_pluginDisplay, false);

#if PLATFORM(GTK)
    RefPtr<cairo_surface_t> drawableSurface = adoptRef(cairo_xlib_surface_create(m_pluginDisplay, m_drawable.get(),
        m_setWindowCallbackStruct.visual, m_plugin.size().width(), m_plugin.size().height()));
    cairo_t* cr = context.platformContext()->cr();
    cairo_save(cr);

    cairo_set_source_surface(cr, drawableSurface.get(), 0, 0);

    cairo_rectangle(cr, exposedRect.x(), exposedRect.y(), exposedRect.width(), exposedRect.height());
    cairo_clip(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_restore(cr);
#else
    notImplemented();
#endif
}

static inline void initializeXEvent(XEvent& event)
{
    memset(&event, 0, sizeof(XEvent));
    event.xany.serial = 0;
    event.xany.send_event = false;
    event.xany.display = x11HostDisplay();
    event.xany.window = 0;
}

static inline uint64_t xTimeStamp(WallTime timestamp)
{
    return timestamp.secondsSinceEpoch().milliseconds();
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

template <typename XEventType, typename WebEventType>
static inline void setCommonMouseEventFields(XEventType& xEvent, const WebEventType& webEvent, const WebCore::IntPoint& pluginLocation)
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
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

static inline void setXButtonEventFieldsByWebWheelEvent(XEvent& xEvent, const WebWheelEvent& webEvent, const WebCore::IntPoint& pluginLocation)
{
    XButtonEvent& xButton = xEvent.xbutton;
    setCommonMouseEventFields(xButton, webEvent, pluginLocation);

    xButton.type = ButtonPress;
    FloatSize ticks = webEvent.wheelTicks();
    if (ticks.height()) {
        if (ticks.height() > 0)
            xButton.button = 4; // up
        else
            xButton.button = 5; // down
    } else {
        if (ticks.width() > 0)
            xButton.button = 6; // left
        else
            xButton.button = 7; // right
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

bool NetscapePluginX11::handleMouseEvent(const WebMouseEvent& event)
{
    ASSERT(!m_plugin.isWindowed());

    XEvent xEvent;
    initializeXEvent(xEvent);

    switch (event.type()) {
    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
        setXButtonEventFields(xEvent, event, m_plugin.convertToRootView(IntPoint()));
        break;
    case WebEvent::MouseMove:
        setXMotionEventFields(xEvent, event, m_plugin.convertToRootView(IntPoint()));
        break;
    case WebEvent::MouseForceChanged:
    case WebEvent::MouseForceDown:
    case WebEvent::MouseForceUp:
    case WebEvent::NoType:
    case WebEvent::Wheel:
    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char:
#if ENABLE(TOUCH_EVENTS)
    case WebEvent::TouchStart:
    case WebEvent::TouchMove:
    case WebEvent::TouchEnd:
    case WebEvent::TouchCancel:
#endif
        return false;
    }

    return !m_plugin.NPP_HandleEvent(&xEvent);
}

// We undefine these constants in npruntime_internal.h to avoid collision
// with WebKit and platform headers. Values are defined in X.h.
const int kKeyPressType = 2;
const int kKeyReleaseType = 3;
const int kFocusInType = 9;
const int kFocusOutType = 10;

bool NetscapePluginX11::handleWheelEvent(const WebWheelEvent& event)
{
    ASSERT(!m_plugin.isWindowed());

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXButtonEventFieldsByWebWheelEvent(xEvent, event, m_plugin.convertToRootView(IntPoint()));

    return !m_plugin.NPP_HandleEvent(&xEvent);
}

void NetscapePluginX11::setFocus(bool focusIn)
{
    ASSERT(!m_plugin.isWindowed());

    XEvent xEvent;
    initializeXEvent(xEvent);
    XFocusChangeEvent& focusEvent = xEvent.xfocus;
    focusEvent.type = focusIn ? kFocusInType : kFocusOutType;
    focusEvent.mode = NotifyNormal;
    focusEvent.detail = NotifyDetailNone;

    m_plugin.NPP_HandleEvent(&xEvent);
}

bool NetscapePluginX11::handleMouseEnterEvent(const WebMouseEvent& event)
{
    ASSERT(!m_plugin.isWindowed());

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXCrossingEventFields(xEvent, event, m_plugin.convertToRootView(IntPoint()), EnterNotify);

    return !m_plugin.NPP_HandleEvent(&xEvent);
}

bool NetscapePluginX11::handleMouseLeaveEvent(const WebMouseEvent& event)
{
    ASSERT(!m_plugin.isWindowed());

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXCrossingEventFields(xEvent, event, m_plugin.convertToRootView(IntPoint()), LeaveNotify);

    return !m_plugin.NPP_HandleEvent(&xEvent);
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

bool NetscapePluginX11::handleKeyboardEvent(const WebKeyboardEvent& event)
{
    ASSERT(!m_plugin.isWindowed());
    // We don't generate other types of keyboard events via WebEventFactory.
    ASSERT(event.type() == WebEvent::KeyDown || event.type() == WebEvent::KeyUp);

    XEvent xEvent;
    initializeXEvent(xEvent);
    setXKeyEventFields(xEvent, event);

    return !m_plugin.NPP_HandleEvent(&xEvent);
}

} // namespace WebKit

#endif // PLATFORM(X11) && ENABLE(NETSCAPE_PLUGIN_API)
