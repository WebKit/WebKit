/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2009, 2010 Kakai, Inc. <brian@kakai.com>
 * Copyright (C) 2010 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginView.h"

#include "BridgeJSC.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GRefPtrGtk.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HostWindow.h"
#include "Image.h"
#include "JSDOMBinding.h"
#include "JSDOMWindowBase.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
#include "RenderElement.h"
#include "Settings.h"
#include "SpatialNavigation.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <runtime/JSCJSValue.h>
#include <runtime/JSLock.h>

#ifdef GTK_API_VERSION_2
#include <gdkconfig.h>
#endif
#include <gtk/gtk.h>

#define String XtStringType
#include "RefPtrCairo.h"
#include "gtk2xtbin.h"
#define Bool int // this became undefined in npruntime_internal.h
#define Status int // ditto
#include <X11/extensions/Xrender.h>
#include <gdk/gdkx.h>

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

Window PluginView::getRootWindow(Frame* parentFrame)
{
    GtkWidget* parentWidget = parentFrame->view()->hostWindow()->platformPageClient();
    GdkScreen* gscreen = gtk_widget_get_screen(parentWidget);
    return GDK_WINDOW_XWINDOW(gdk_screen_get_root_window(gscreen));
}

void PluginView::setFocus(bool focused)
{
    ASSERT(platformPluginWidget() == platformWidget());
    if (focused && platformWidget())
        gtk_widget_grab_focus(platformWidget());
    Widget::setFocus(focused);
}

void PluginView::show()
{
    ASSERT(platformPluginWidget() == platformWidget());
    Widget::show();
}

void PluginView::hide()
{
    ASSERT(platformPluginWidget() == platformWidget());
    Widget::hide();
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonVM());

    if (!m_isStarted || m_status != PluginStatusLoadedSuccessfully)
        return;

    if (event->type() != eventNames().keydownEvent && event->type() != eventNames().keyupEvent)
        return;

    NPEvent xEvent;
    initXEvent(&xEvent);
    GdkEventKey* gdkEvent = event->keyEvent()->gdkEventKey();

    xEvent.type = (event->type() == eventNames().keydownEvent) ? 2 : 3; // KeyPress/Release get unset somewhere
    xEvent.xkey.root = getRootWindow(m_parentFrame.get());
    xEvent.xkey.subwindow = 0; // we have no child window
    xEvent.xkey.time = event->timeStamp();
    xEvent.xkey.state = gdkEvent->state; // GdkModifierType mirrors xlib state masks
    xEvent.xkey.keycode = gdkEvent->hardware_keycode;
    xEvent.xkey.same_screen = true;

    // NOTE: As the XEvents sent to the plug-in are synthesized and there is not a native window
    // corresponding to the plug-in rectangle, some of the members of the XEvent structures are not
    // set to their normal Xserver values. e.g. Key events don't have a position.
    // source: https://developer.mozilla.org/en/NPEvent
    xEvent.xkey.x = 0;
    xEvent.xkey.y = 0;
    xEvent.xkey.x_root = 0;
    xEvent.xkey.y_root = 0;

    if (dispatchNPEvent(xEvent))
        event->setDefaultHandled();
}

static unsigned int inputEventState(MouseEvent* event)
{
    unsigned int state = 0;
    if (event->ctrlKey())
        state |= ControlMask;
    if (event->shiftKey())
        state |= ShiftMask;
    if (event->altKey())
        state |= Mod1Mask;
    if (event->metaKey())
        state |= Mod4Mask;
    return state;
}

static void setXButtonEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos, Frame* parentFrame)
{
    XButtonEvent& xbutton = xEvent->xbutton;
    xbutton.type = event->type() == eventNames().mousedownEvent ? ButtonPress : ButtonRelease;
    xbutton.root = PluginView::getRootWindow(parentFrame);
    xbutton.subwindow = 0;
    xbutton.time = event->timeStamp();
    xbutton.x = postZoomPos.x();
    xbutton.y = postZoomPos.y();
    xbutton.x_root = event->screenX();
    xbutton.y_root = event->screenY();
    xbutton.state = inputEventState(event);
    switch (event->button()) {
    case MiddleButton:
        xbutton.button = Button2;
        break;
    case RightButton:
        xbutton.button = Button3;
        break;
    case LeftButton:
    default:
        xbutton.button = Button1;
        break;
    }
    xbutton.same_screen = true;
}

static void setXMotionEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos, Frame* parentFrame)
{
    XMotionEvent& xmotion = xEvent->xmotion;
    xmotion.type = MotionNotify;
    xmotion.root = PluginView::getRootWindow(parentFrame);
    xmotion.subwindow = 0;
    xmotion.time = event->timeStamp();
    xmotion.x = postZoomPos.x();
    xmotion.y = postZoomPos.y();
    xmotion.x_root = event->screenX();
    xmotion.y_root = event->screenY();
    xmotion.state = inputEventState(event);
    xmotion.is_hint = NotifyNormal;
    xmotion.same_screen = true;
}

static void setXCrossingEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos, Frame* parentFrame)
{
    XCrossingEvent& xcrossing = xEvent->xcrossing;
    xcrossing.type = event->type() == eventNames().mouseoverEvent ? EnterNotify : LeaveNotify;
    xcrossing.root = PluginView::getRootWindow(parentFrame);
    xcrossing.subwindow = 0;
    xcrossing.time = event->timeStamp();
    xcrossing.x = postZoomPos.y();
    xcrossing.y = postZoomPos.x();
    xcrossing.x_root = event->screenX();
    xcrossing.y_root = event->screenY();
    xcrossing.state = inputEventState(event);
    xcrossing.mode = NotifyNormal;
    xcrossing.detail = NotifyDetailNone;
    xcrossing.same_screen = true;
    xcrossing.focus = false;
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonVM());

    if (!m_isStarted || m_status != PluginStatusLoadedSuccessfully)
        return;

    if (event->button() == RightButton && m_plugin->quirks().contains(PluginQuirkIgnoreRightClickInWindowlessMode))
        return;

    if (event->type() == eventNames().mousedownEvent) {
        if (Page* page = m_parentFrame->page())
            page->focusController().setActive(true);
        focusPluginElement();
    }

    NPEvent xEvent;
    initXEvent(&xEvent);

    IntPoint postZoomPos = roundedIntPoint(m_element->renderer()->absoluteToLocal(event->absoluteLocation()));

    if (event->type() == eventNames().mousedownEvent || event->type() == eventNames().mouseupEvent)
        setXButtonEventSpecificFields(&xEvent, event, postZoomPos, m_parentFrame.get());
    else if (event->type() == eventNames().mousemoveEvent)
        setXMotionEventSpecificFields(&xEvent, event, postZoomPos, m_parentFrame.get());
    else if (event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mouseoverEvent) {
        setXCrossingEventSpecificFields(&xEvent, event, postZoomPos, m_parentFrame.get());

        // This is a work-around for plugins which change the cursor. When that happens we
        // get out of sync with GDK somehow. Resetting the cursor here seems to fix the issue.
        if (event->type() == eventNames().mouseoutEvent)
            gdk_window_set_cursor(gtk_widget_get_window(m_parentFrame->view()->hostWindow()->platformPageClient()), 0);
    }
    else
        return;

    if (dispatchNPEvent(xEvent))
        event->setDefaultHandled();
}

void PluginView::updateWidgetAllocationAndClip()
{
    // If the window has not been embedded yet (the plug added), we delay setting its allocation until 
    // that point. This fixes issues with some Java plugin instances not rendering immediately.
    if (!m_plugAdded || m_delayedAllocation.isEmpty())
        return;

    GtkWidget* widget = platformPluginWidget();
    if (gtk_widget_get_realized(widget)) {
        GdkRectangle clipRect = m_clipRect;
#ifdef GTK_API_VERSION_2
        GdkRegion* clipRegion = gdk_region_rectangle(&clipRect);
        gdk_window_shape_combine_region(gtk_widget_get_window(widget), clipRegion, 0, 0);
        gdk_region_destroy(clipRegion);
#else
        cairo_region_t* clipRegion = cairo_region_create_rectangle(&clipRect);
        gdk_window_shape_combine_region(gtk_widget_get_window(widget), clipRegion, 0, 0);
        cairo_region_destroy(clipRegion);
#endif
    }

    // The goal is to try to avoid calling gtk_widget_size_allocate in the WebView's
    // size-allocate method. It blocks the main loop and if the widget is offscreen
    // or hasn't moved it isn't required.

    // Don't do anything if the allocation has not changed.
    GtkAllocation currentAllocation;
    gtk_widget_get_allocation(widget, &currentAllocation);
    if (currentAllocation == m_delayedAllocation)
        return;

    // Don't do anything if both the old and the new allocations are outside the frame.
    IntRect currentAllocationRect(currentAllocation);
    currentAllocationRect.intersect(frameRect());
    if (currentAllocationRect.isEmpty() && m_clipRect.isEmpty())
        return;

    g_object_set_data(G_OBJECT(widget), "delayed-allocation", &m_delayedAllocation);
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    if (isSelfVisible() && platformPluginWidget()) {
        if (visible)
            gtk_widget_show(platformPluginWidget());
        else
            gtk_widget_hide(platformPluginWidget());
    }
}

NPError PluginView::handlePostReadFile(Vector<char>& outputBuffer, uint32_t filenameLength, const char* filenameBuffer)
{
    // There doesn't seem to be any documentation about what encoding the filename
    // is in, but most ports seem to assume UTF-8 here and the test plugin is definitely
    // sending the path in UTF-8 encoding.
    CString filename(filenameBuffer, filenameLength);

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_commandline_arg(filename.data()));
    if (g_file_query_file_type(file.get(), G_FILE_QUERY_INFO_NONE, 0) != G_FILE_TYPE_REGULAR)
        return NPERR_FILE_NOT_FOUND;

    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_info(file.get(),
                                                              G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                                              G_FILE_QUERY_INFO_NONE,
                                                              0, 0));
    if (!fileInfo)
        return NPERR_FILE_NOT_FOUND;

    GRefPtr<GFileInputStream> inputStream = adoptGRef(g_file_read(file.get(), 0, 0));
    if (!inputStream)
        return NPERR_FILE_NOT_FOUND;

    outputBuffer.resize(g_file_info_get_size(fileInfo.get()));
    gsize bytesRead = 0;
    if (!g_input_stream_read_all(G_INPUT_STREAM(inputStream.get()),
                                 outputBuffer.data(), outputBuffer.size(), &bytesRead, 0, 0))
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

bool PluginView::platformGetValueStatic(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVToolkit:
        *static_cast<uint32_t*>(value) = 2;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsXEmbedBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsWindowless:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    default:
        return false;
    }
}

bool PluginView::platformGetValue(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVxDisplay:
        if (m_needsXEmbed)
            *(void **)value = (void *)GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
        else
            *(void **)value = (void *)GTK_XTBIN(platformPluginWidget())->xtclient.xtdisplay;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVxtAppContext:
        if (!m_needsXEmbed) {
            *(void **)value = XtDisplayToApplicationContext (GTK_XTBIN(platformPluginWidget())->xtclient.xtdisplay);

            *result = NPERR_NO_ERROR;
        } else
            *result = NPERR_GENERIC_ERROR;
        return true;

        case NPNVnetscapeWindow: {
            GdkWindow* gdkWindow = gtk_widget_get_window(m_parentFrame->view()->hostWindow()->platformPageClient());
            GdkWindow* toplevelWindow = gdk_window_get_toplevel(gdkWindow);
            if (!toplevelWindow) {
                *result = NPERR_GENERIC_ERROR;
                return true;
            }
            *static_cast<Window*>(value) = GDK_WINDOW_XWINDOW(toplevelWindow);
            *result = NPERR_NO_ERROR;
            return true;
        }

    default:
        return false;
    }
}

void PluginView::invalidateRect(const IntRect& rect)
{
    if (m_isWindowed) {
        gtk_widget_queue_draw_area(GTK_WIDGET(platformPluginWidget()), rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }

    invalidateWindowlessPluginRect(rect);
}

void PluginView::forceRedraw()
{
    if (m_isWindowed)
        gtk_widget_queue_draw(platformPluginWidget());
    else
        gtk_widget_queue_draw(m_parentFrame->view()->hostWindow()->platformPageClient());
}

Display* PluginView::getPluginDisplay(Frame* parentFrame)
{
    if (parentFrame) {
        GtkWidget* widget = parentFrame->view()->hostWindow()->platformPageClient();
        return GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(widget));
    }

    // The plugin toolkit might have a different X connection open.  Since we're
    // a gdk/gtk app, we'll (probably?) have the same X connection as any gdk-based
    // plugins, so we can return that.  We might want to add other implementations here
    // later.

    return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
}

static void getVisualAndColormap(int depth, Visual** visual, Colormap* colormap)
{
    *visual = 0;
    *colormap = 0;

    Display* display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    int rmaj, rmin;
    if (depth == 32 && (!XRenderQueryVersion(display, &rmaj, &rmin) || (rmaj == 0 && rmin < 5)))
        return;

    XVisualInfo templ;
    templ.screen  = gdk_screen_get_number(gdk_screen_get_default());
    templ.depth   = depth;
    templ.c_class = TrueColor;
    int nVisuals;
    XVisualInfo* visualInfo = XGetVisualInfo(display, VisualScreenMask | VisualDepthMask | VisualClassMask, &templ, &nVisuals);

    if (!nVisuals)
        return;

    if (depth == 32) {
        for (int idx = 0; idx < nVisuals; ++idx) {
            XRenderPictFormat* format = XRenderFindVisualFormat(display, visualInfo[idx].visual);
            if (format->type == PictTypeDirect && format->direct.alphaMask) {
                 *visual = visualInfo[idx].visual;
                 break;
            }
         }
    } else
        *visual = visualInfo[0].visual;

    XFree(visualInfo);

    if (*visual)
        *colormap = XCreateColormap(display, GDK_ROOT_WINDOW(), *visual, AllocNone);
}

gboolean PluginView::plugRemovedCallback(GtkSocket*, PluginView* view)
{
    view->m_plugAdded = false;
    return TRUE;
}

void PluginView::plugAddedCallback(GtkSocket* socket, PluginView* view)
{
    ASSERT_UNUSED(socket, socket);
    ASSERT(view);
    view->m_plugAdded = true;
    view->updateWidgetAllocationAndClip();
}

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonVM());
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (m_isWindowed) {
        GtkWidget* pageClient = m_parentFrame->view()->hostWindow()->platformPageClient();
        if (m_needsXEmbed) {
            // If our parent is not anchored the startup process will
            // fail miserably for XEmbed plugins a bit later on when
            // we try to get the ID of our window (since realize will
            // fail), so let's just abort here.
            if (!gtk_widget_get_parent(pageClient))
                return false;

            m_plugAdded = false;
            setPlatformWidget(gtk_socket_new());
            gtk_container_add(GTK_CONTAINER(pageClient), platformPluginWidget());
            g_signal_connect(platformPluginWidget(), "plug-added", G_CALLBACK(PluginView::plugAddedCallback), this);
            g_signal_connect(platformPluginWidget(), "plug-removed", G_CALLBACK(PluginView::plugRemovedCallback), this);
        } else
            setPlatformWidget(gtk_xtbin_new(pageClient, 0));
    } else {
        setPlatformWidget(0);
        m_pluginDisplay = getPluginDisplay(nullptr);
    }

    show();

        NPSetWindowCallbackStruct* ws = new NPSetWindowCallbackStruct();
        ws->type = 0;

    if (m_isWindowed) {
        m_npWindow.type = NPWindowTypeWindow;
        if (m_needsXEmbed) {
            GtkWidget* widget = platformPluginWidget();
            gtk_widget_realize(widget);
            m_npWindow.window = reinterpret_cast<void*>(gtk_socket_get_id(GTK_SOCKET(platformPluginWidget())));
            GdkWindow* window = gtk_widget_get_window(widget);
            ws->display = GDK_WINDOW_XDISPLAY(window);
            ws->visual = GDK_VISUAL_XVISUAL(gdk_window_get_visual(window));
            ws->depth = gdk_visual_get_depth(gdk_window_get_visual(window));
            ws->colormap = XCreateColormap(ws->display, GDK_ROOT_WINDOW(), ws->visual, AllocNone);
        } else {
            m_npWindow.window = reinterpret_cast<void*>((GTK_XTBIN(platformPluginWidget())->xtwindow));
            ws->display = GTK_XTBIN(platformPluginWidget())->xtdisplay;
            ws->visual = GTK_XTBIN(platformPluginWidget())->xtclient.xtvisual;
            ws->depth = GTK_XTBIN(platformPluginWidget())->xtclient.xtdepth;
            ws->colormap = GTK_XTBIN(platformPluginWidget())->xtclient.xtcolormap;
        }
        XFlush (ws->display);
    } else {
        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0; // Not used?

        GdkScreen* gscreen = gdk_screen_get_default();
        GdkVisual* gvisual = gdk_screen_get_system_visual(gscreen);

        if (gdk_visual_get_depth(gvisual) == 32 || !m_plugin->quirks().contains(PluginQuirkRequiresDefaultScreenDepth)) {
            getVisualAndColormap(32, &m_visual, &m_colormap);
            ws->depth = 32;
        }

        if (!m_visual) {
            getVisualAndColormap(gdk_visual_get_depth(gvisual), &m_visual, &m_colormap);
            ws->depth = gdk_visual_get_depth(gvisual);
        }

        ws->display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
        ws->visual = m_visual;
        ws->colormap = m_colormap;

        m_npWindow.x = 0;
        m_npWindow.y = 0;
        m_npWindow.width = -1;
        m_npWindow.height = -1;
    }

    m_npWindow.ws_info = ws;

    // TODO remove in favor of null events, like mac port?
    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall)))
        updatePluginWidget(); // was: setNPWindowIfNeeded(), but this doesn't produce 0x0 rects at first go

    return true;
}

void PluginView::platformDestroy()
{
    if (m_drawable) {
        XFreePixmap(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), m_drawable);
        m_drawable = 0;
    }

    GtkWidget* widget = platformWidget();
    if (widget) {
        GtkWidget* parent = gtk_widget_get_parent(widget);
        ASSERT(parent);
        gtk_container_remove(GTK_CONTAINER(parent), widget);
    }
}

} // namespace WebCore
