/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "Image.h"
#include "JSDOMBinding.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PluginContainerQt.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "PluginMainThreadScheduler.h"
#include "RenderLayer.h"
#include "ScriptController.h"
#include "Settings.h"
#include "npruntime_impl.h"
#include "runtime.h"
#include "runtime_root.h"
#include "QWebPageClient.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QPainter>
#include <QWidget>
#include <QX11Info>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <X11/X.h>
#ifndef QT_NO_XRENDER
#define Bool int
#define Status int
#include <X11/extensions/Xrender.h>
#endif

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

void PluginView::updatePluginWidget()
{
    if (!parent())
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (m_windowRect == oldWindowRect && m_clipRect == oldClipRect)
        return;

    if (m_drawable)
        XFreePixmap(QX11Info::display(), m_drawable);

    if (!m_isWindowed) {
        m_drawable = XCreatePixmap(QX11Info::display(), QX11Info::appRootWindow(), m_windowRect.width(), m_windowRect.height(), 
                                   ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->depth);
        QApplication::syncX(); // make sure that the server knows about the Drawable
    }

    // do not call setNPWindowIfNeeded immediately, will be called on paint()
    m_hasPendingGeometryChange = true;

    // in order to move/resize the plugin window at the same time as the
    // rest of frame during e.g. scrolling, we set the window geometry
    // in the paint() function, but as paint() isn't called when the
    // plugin window is outside the frame which can be caused by a
    // scroll, we need to move/resize immediately.
    if (!m_windowRect.intersects(frameView->frameRect()))
        setNPWindowIfNeeded();
}

void PluginView::setFocus()
{
    if (platformPluginWidget())
        platformPluginWidget()->setFocus(Qt::OtherFocusReason);
    else
        Widget::setFocus();
}

void PluginView::show()
{
    setSelfVisible(true);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(true);

    // do not call parent impl. here as it will set the platformWidget
    // (same as platformPluginWidget in the Qt port) to visible, even
    // when parent isn't visible.
}

void PluginView::hide()
{
    setSelfVisible(false);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(false);

    // do not call parent impl. here as it will set the platformWidget
    // (same as platformPluginWidget in the Qt port) to invisible, even
    // when parent isn't visible.
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    if (context->paintingDisabled())
        return;

    setNPWindowIfNeeded();

    if (m_isWindowed || !m_drawable)
        return;

    const bool syncX = m_pluginDisplay && m_pluginDisplay != QX11Info::display();

    QPainter* painter = context->platformContext();

    QPixmap qtDrawable = QPixmap::fromX11Pixmap(m_drawable, QPixmap::ExplicitlyShared);
    const int drawableDepth = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->depth;
    ASSERT(drawableDepth == qtDrawable.depth());

    if (m_isTransparent && drawableDepth != 32) {
        // Attempt content propagation for drawable with no alpha by copying over from the backing store
        QPoint offset;
        QPaintDevice* backingStoreDevice =  QPainter::redirected(painter->device(), &offset);
        offset = -offset; // negating the offset gives us the offset of the view within the backing store pixmap

        const bool hasValidBackingStore = backingStoreDevice && backingStoreDevice->devType() == QInternal::Pixmap;
        QPixmap* backingStorePixmap = static_cast<QPixmap*>(backingStoreDevice);

        // We cannot grab contents from the backing store when painting on QGraphicsView items
        // (because backing store contents are already transformed). What we really mean to do 
        // here is to check if we are painting on QWebView, but let's be a little permissive :)
        QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
        const bool backingStoreHasUntransformedContents = qobject_cast<QWidget*>(client->pluginParent());

        if (hasValidBackingStore && backingStorePixmap->depth() == drawableDepth 
            && backingStoreHasUntransformedContents) {
            GC gc = XDefaultGC(QX11Info::display(), QX11Info::appScreen());
            XCopyArea(QX11Info::display(), backingStorePixmap->handle(), m_drawable, gc,
                offset.x() + m_windowRect.x() + m_clipRect.x(), offset.y() + m_windowRect.y() + m_clipRect.y(),
                m_clipRect.width(), m_clipRect.height(), m_clipRect.x(), m_clipRect.y());
        } else { // no backing store, clean the pixmap because the plugin thinks its transparent
            QPainter painter(&qtDrawable);
            painter.fillRect(m_clipRect, Qt::white);
        }

        if (syncX)
            QApplication::syncX();
    }

    XEvent xevent;
    memset(&xevent, 0, sizeof(XEvent));
    XGraphicsExposeEvent& exposeEvent = xevent.xgraphicsexpose;
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = QX11Info::display();
    exposeEvent.drawable = m_drawable;
    exposeEvent.x = m_clipRect.x();
    exposeEvent.y = m_clipRect.y();
    exposeEvent.width = m_clipRect.x() + m_clipRect.width(); // flash bug? it thinks width is the right
    exposeEvent.height = m_clipRect.y() + m_clipRect.height(); // flash bug? it thinks height is the bottom

    dispatchNPEvent(xevent);

    if (syncX)
        XSync(m_pluginDisplay, False); // sync changes by plugin

    painter->drawPixmap(frameRect().x() + m_clipRect.x(), frameRect().y() + m_clipRect.y(), qtDrawable,
        m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height());
}

// TODO: Unify across ports.
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    if (!m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(false);

    setCallingPlugin(true);
    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);
    setCallingPlugin(false);

    return accepted;
}

void setSharedXEventFields(XEvent* xEvent, QWidget* hostWindow)
{
    xEvent->xany.serial = 0; // we are unaware of the last request processed by X Server
    xEvent->xany.send_event = false;
    xEvent->xany.display = hostWindow->x11Info().display();
    // NOTE: event->xany.window doesn't always respond to the .window property of other XEvent's
    // but does in the case of KeyPress, KeyRelease, ButtonPress, ButtonRelease, and MotionNotify
    // events; thus, this is right:
    xEvent->xany.window = hostWindow->window()->handle();
}

void PluginView::initXEvent(XEvent* xEvent)
{
    memset(xEvent, 0, sizeof(XEvent));

    QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
    QWidget* window = QWidget::find(client->winId());
    setSharedXEventFields(xEvent, window);
}

void setXKeyEventSpecificFields(XEvent* xEvent, KeyboardEvent* event)
{
    QKeyEvent* qKeyEvent = event->keyEvent()->qtEvent();

    xEvent->type = (event->type() == eventNames().keydownEvent) ? 2 : 3; // ints as Qt unsets KeyPress and KeyRelease
    xEvent->xkey.root = QX11Info::appRootWindow();
    xEvent->xkey.subwindow = 0; // we have no child window
    xEvent->xkey.time = event->timeStamp();
    xEvent->xkey.state = qKeyEvent->nativeModifiers();
    xEvent->xkey.keycode = qKeyEvent->nativeScanCode();
    xEvent->xkey.same_screen = true;

    // NOTE: As the XEvents sent to the plug-in are synthesized and there is not a native window
    // corresponding to the plug-in rectangle, some of the members of the XEvent structures are not
    // set to their normal Xserver values. e.g. Key events don't have a position.
    // source: https://developer.mozilla.org/en/NPEvent
    xEvent->xkey.x = 0;
    xEvent->xkey.y = 0;
    xEvent->xkey.x_root = 0;
    xEvent->xkey.y_root = 0;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_isWindowed)
        return;

    if (event->type() != eventNames().keydownEvent && event->type() != eventNames().keyupEvent)
        return;

    XEvent npEvent;
    initXEvent(&npEvent);
    setXKeyEventSpecificFields(&npEvent, event);

    if (!dispatchNPEvent(npEvent))
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

static void setXButtonEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos)
{
    XButtonEvent& xbutton = xEvent->xbutton;
    xbutton.type = event->type() == eventNames().mousedownEvent ? ButtonPress : ButtonRelease;
    xbutton.root = QX11Info::appRootWindow();
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

static void setXMotionEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos)
{
    XMotionEvent& xmotion = xEvent->xmotion;
    xmotion.type = MotionNotify;
    xmotion.root = QX11Info::appRootWindow();
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

static void setXCrossingEventSpecificFields(XEvent* xEvent, MouseEvent* event, const IntPoint& postZoomPos)
{
    XCrossingEvent& xcrossing = xEvent->xcrossing;
    xcrossing.type = event->type() == eventNames().mouseoverEvent ? EnterNotify : LeaveNotify;
    xcrossing.root = QX11Info::appRootWindow();
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
    if (m_isWindowed)
        return;

    if (event->type() == eventNames().mousedownEvent) {
        // Give focus to the plugin on click
        if (Page* page = m_parentFrame->page())
            page->focusController()->setActive(true);

        focusPluginElement();
    }

    XEvent npEvent;
    initXEvent(&npEvent);

    IntPoint postZoomPos = roundedIntPoint(m_element->renderer()->absoluteToLocal(event->absoluteLocation()));

    if (event->type() == eventNames().mousedownEvent || event->type() == eventNames().mouseupEvent)
        setXButtonEventSpecificFields(&npEvent, event, postZoomPos);
    else if (event->type() == eventNames().mousemoveEvent)
        setXMotionEventSpecificFields(&npEvent, event, postZoomPos);
    else if (event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mouseoverEvent)
        setXCrossingEventSpecificFields(&npEvent, event, postZoomPos);
    else
        return;

    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

void PluginView::handleFocusInEvent()
{
    XEvent npEvent;
    initXEvent(&npEvent);

    XFocusChangeEvent& event = npEvent.xfocus;
    event.type = 9; /* int as Qt unsets FocusIn */
    event.mode = NotifyNormal;
    event.detail = NotifyDetailNone;

    dispatchNPEvent(npEvent);
}

void PluginView::handleFocusOutEvent()
{
    XEvent npEvent;
    initXEvent(&npEvent);

    XFocusChangeEvent& event = npEvent.xfocus;
    event.type = 10; /* int as Qt unsets FocusOut */
    event.mode = NotifyNormal;
    event.detail = NotifyDetailNone;

    dispatchNPEvent(npEvent);
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
}

void PluginView::setNPWindowRect(const IntRect&)
{
    if (!m_isWindowed)
        setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    // On Unix, only call plugin if it's full-page or windowed
    if (m_mode != NP_FULL && m_mode != NP_EMBED)
        return;

    if (!m_hasPendingGeometryChange)
        return;
    m_hasPendingGeometryChange = false;

    if (m_isWindowed) {
        ASSERT(platformPluginWidget());
        platformPluginWidget()->setGeometry(m_windowRect);
        // if setMask is set with an empty QRegion, no clipping will
        // be performed, so in that case we hide the plugin view
        platformPluginWidget()->setVisible(!m_clipRect.isEmpty());
        platformPluginWidget()->setMask(QRegion(m_clipRect));

        m_npWindow.x = m_windowRect.x();
        m_npWindow.y = m_windowRect.y();

        m_npWindow.clipRect.left = m_clipRect.x();
        m_npWindow.clipRect.top = m_clipRect.y();
        m_npWindow.clipRect.right = m_clipRect.width();
        m_npWindow.clipRect.bottom = m_clipRect.height();
    } else {
        m_npWindow.x = 0;
        m_npWindow.y = 0;

        m_npWindow.clipRect.left = 0;
        m_npWindow.clipRect.top = 0;
        m_npWindow.clipRect.right = 0;
        m_npWindow.clipRect.bottom = 0;
    }

    // FLASH WORKAROUND: Only set initially. Multiple calls to
    // setNPWindow() cause the plugin to crash in windowed mode.
    if (!m_isWindowed || m_npWindow.width == -1 || m_npWindow.height == -1) {
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();
    }

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    if (isSelfVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(visible);
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    if (!fileExists(filename))
        return NPERR_FILE_NOT_FOUND;

    // FIXME - read the file data into buffer
    FILE* fileHandle = fopen((filename.utf8()).data(), "r");

    if (!fileHandle)
        return NPERR_FILE_NOT_FOUND;

    //buffer.resize();

    int bytesRead = fread(buffer.data(), 1, 0, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    LOG(Plugins, "PluginView::getValueStatic(%s)", prettyNameForNPNVariable(variable).data());

    switch (variable) {
    case NPNVToolkit:
        *static_cast<uint32*>(value) = 0;
        return NPERR_NO_ERROR;

    case NPNVSupportsXEmbedBool:
        *static_cast<NPBool*>(value) = true;
        return NPERR_NO_ERROR;

    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        return NPERR_NO_ERROR;

    case NPNVSupportsWindowless:
        *static_cast<NPBool*>(value) = true;
        return NPERR_NO_ERROR;

    default:
        return NPERR_GENERIC_ERROR;
    }
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    LOG(Plugins, "PluginView::getValue(%s)", prettyNameForNPNVariable(variable).data());

    switch (variable) {
    case NPNVxDisplay:
        if (platformPluginWidget())
            *(void **)value = platformPluginWidget()->x11Info().display();
        else {
            QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
            QWidget* window = QWidget::find(client->winId());
            *(void **)value = window->x11Info().display();
        }
        return NPERR_NO_ERROR;

    case NPNVxtAppContext:
        return NPERR_GENERIC_ERROR;

#if ENABLE(NETSCAPE_PLUGIN_API)
    case NPNVWindowNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* windowScriptObject = m_parentFrame->script()->windowScriptNPObject();

        // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (windowScriptObject)
            _NPN_RetainObject(windowScriptObject);

        void** v = (void**)value;
        *v = windowScriptObject;

        return NPERR_NO_ERROR;
    }

    case NPNVPluginElementNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* pluginScriptObject = 0;

        if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
            pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

        // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (pluginScriptObject)
            _NPN_RetainObject(pluginScriptObject);

        void** v = (void**)value;
        *v = pluginScriptObject;

        return NPERR_NO_ERROR;
    }
#endif

    case NPNVnetscapeWindow: {
        void* w = reinterpret_cast<void*>(value);
        *((XID *)w) = m_parentFrame->view()->hostWindow()->platformPageClient()->winId();
        return NPERR_NO_ERROR;
    }

    case NPNVToolkit:
        if (m_plugin->quirks().contains(PluginQuirkRequiresGtkToolKit)) {
            *((uint32 *)value) = 2;
            return NPERR_NO_ERROR;
        }
        // fall through
    default:
        return getValueStatic(variable, value);
    }
}

void PluginView::invalidateRect(const IntRect& rect)
{
    if (m_isWindowed) {
        platformWidget()->update(rect);
        return;
    }

    invalidateWindowlessPluginRect(rect);
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }
    IntRect r(rect->left, rect->top, rect->right + rect->left, rect->bottom + rect->top);
    invalidateWindowlessPluginRect(r);
}

void PluginView::invalidateRegion(NPRegion region)
{
    invalidate();
}

void PluginView::forceRedraw()
{
    invalidate();
}

static Display *getPluginDisplay()
{
    // The plugin toolkit might run using a different X connection. At the moment, we only
    // support gdk based plugins (like flash) that use a different X connection.
    // The code below has the same effect as this one:
    // Display *gdkDisplay = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    QLibrary library("libgdk-x11-2.0");
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
}

static void getVisualAndColormap(int depth, Visual **visual, Colormap *colormap)
{
    *visual = 0;
    *colormap = 0;

#ifndef QT_NO_XRENDER
    static const bool useXRender = qgetenv("QT_X11_NO_XRENDER").isNull(); // Should also check for XRender >= 0.5
#else
    static const bool useXRender = false;
#endif

    if (!useXRender && depth == 32)
        return;

    int nvi;
    XVisualInfo templ;
    templ.screen  = QX11Info::appScreen();
    templ.depth   = depth;
    templ.c_class = TrueColor;
    XVisualInfo* xvi = XGetVisualInfo(QX11Info::display(), VisualScreenMask | VisualDepthMask | VisualClassMask, &templ, &nvi);

    if (!xvi)
        return;

#ifndef QT_NO_XRENDER
    if (depth == 32) {
        for (int idx = 0; idx < nvi; ++idx) {
            XRenderPictFormat* format = XRenderFindVisualFormat(QX11Info::display(), xvi[idx].visual);
            if (format->type == PictTypeDirect && format->direct.alphaMask) {
                 *visual = xvi[idx].visual;
                 break;
            }
         }
    } else
#endif // QT_NO_XRENDER
        *visual = xvi[0].visual;

    XFree(xvi);

    if (*visual)
        *colormap = XCreateColormap(QX11Info::display(), QX11Info::appRootWindow(), *visual, AllocNone);
}

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (m_isWindowed) {
        if (m_needsXEmbed) {
            QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
            setPlatformWidget(new PluginContainerQt(this, QWidget::find(client->winId())));
            // sync our XEmbed container window creation before sending the xid to plugins.
            QApplication::syncX();
        } else {
            notImplemented();
            m_status = PluginStatusCanNotLoadPlugin;
            return false;
        }
    } else {
        setPlatformWidget(0);
        m_pluginDisplay = getPluginDisplay();
    }

    show();

    NPSetWindowCallbackStruct* wsi = new NPSetWindowCallbackStruct();
    wsi->type = 0;

    if (m_isWindowed) {
        const QX11Info* x11Info = &platformPluginWidget()->x11Info();

        wsi->display = x11Info->display();
        wsi->visual = (Visual*)x11Info->visual();
        wsi->depth = x11Info->depth();
        wsi->colormap = x11Info->colormap();

        m_npWindow.type = NPWindowTypeWindow;
        m_npWindow.window = (void*)platformPluginWidget()->winId();
        m_npWindow.width = -1;
        m_npWindow.height = -1;
    } else {
        const QX11Info* x11Info = &QApplication::desktop()->x11Info();

        if (x11Info->depth() == 32 || !m_plugin->quirks().contains(PluginQuirkRequiresDefaultScreenDepth)) {
            getVisualAndColormap(32, &m_visual, &m_colormap);
            wsi->depth = 32;
        }

        if (!m_visual) {
            getVisualAndColormap(x11Info->depth(), &m_visual, &m_colormap);
            wsi->depth = x11Info->depth();
        }

        wsi->display = x11Info->display();
        wsi->visual = m_visual;
        wsi->colormap = m_colormap;

        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0; // Not used?
        m_npWindow.x = 0;
        m_npWindow.y = 0;
        m_npWindow.width = -1;
        m_npWindow.height = -1;
    }

    m_npWindow.ws_info = wsi;

    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall))) {
        updatePluginWidget();
        setNPWindowIfNeeded();
    }

    return true;
}

void PluginView::platformDestroy()
{
    if (platformPluginWidget())
        delete platformPluginWidget();

    if (m_drawable)
        XFreePixmap(QX11Info::display(), m_drawable);

    if (m_colormap)
        XFreeColormap(QX11Info::display(), m_colormap);
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

} // namespace WebCore
