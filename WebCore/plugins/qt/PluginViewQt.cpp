/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
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
#include <QKeyEvent>
#include <QWidget>
#include <QX11Info>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <X11/X.h>

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
    if (!parent() || !m_isWindowed || !platformPluginWidget())
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

    if (m_isWindowed && platformPluginWidget())
        setNPWindowIfNeeded();
}

// TODO: Unify across ports.
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    if (!m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(false);

    setCallingPlugin(true);
    bool accepted = m_plugin->pluginFuncs();
    setCallingPlugin(false);

    return accepted;
}

void setSharedXEventFields(XEvent& xEvent, QWidget* hostWindow)
{
    xEvent.xany.serial = 0; // we are unaware of the last request processed by X Server
    xEvent.xany.send_event = false;
    xEvent.xany.display = hostWindow->x11Info().display();
    // NOTE: event.xany.window doesn't always respond to the .window property of other XEvent's
    // but does in the case of KeyPress, KeyRelease, ButtonPress, ButtonRelease, and MotionNotify
    // events; thus, this is right:
    xEvent.xany.window = hostWindow->window()->handle();
}

void setXKeyEventSpecificFields(XEvent& xEvent, KeyboardEvent* event)
{
    QKeyEvent* qKeyEvent = event->keyEvent()->qtEvent();

    xEvent.xkey.root = QX11Info::appRootWindow();
    xEvent.xkey.subwindow = 0; // we have no child window
    xEvent.xkey.time = event->timeStamp();
    xEvent.xkey.state = qKeyEvent->nativeModifiers();
    xEvent.xkey.keycode = qKeyEvent->nativeScanCode();
    xEvent.xkey.same_screen = true;

    // NOTE: As the XEvents sent to the plug-in are synthesized and there is not a native window
    // corresponding to the plug-in rectangle, some of the members of the XEvent structures are not
    // set to their normal Xserver values. e.g. Key events don't have a position.
    // source: https://developer.mozilla.org/en/NPEvent
    xEvent.xkey.x = 0;
    xEvent.xkey.y = 0;
    xEvent.xkey.x_root = 0;
    xEvent.xkey.y_root = 0;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_isWindowed)
        return;

    if (event->type() != "keydown" && event->type() != "keyup")
        return;

    XEvent npEvent; // On UNIX NPEvent is a typedef for XEvent.

    npEvent.type = (event->type() == "keydown") ? 2 : 3; // ints as Qt unsets KeyPress and KeyRelease
    QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
    QWidget* window = QWidget::find(client->winId());
    setSharedXEventFields(npEvent, window);
    setXKeyEventSpecificFields(npEvent, event);

    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    if (m_isWindowed)
        return;
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
}

void PluginView::setNPWindowRect(const IntRect&)
{
    // Ignored as we don't want to move immediately.
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

    ASSERT(platformPluginWidget());
    platformPluginWidget()->setGeometry(m_windowRect);
    // if setMask is set with an empty QRegion, no clipping will
    // be performed, so in that case we hide the plugin view
    platformPluginWidget()->setVisible(!m_clipRect.isEmpty());
    platformPluginWidget()->setMask(QRegion(m_clipRect));

    // FLASH WORKAROUND: Only set initially. Multiple calls to
    // setNPWindow() cause the plugin to crash.
    if (m_npWindow.width == -1 || m_npWindow.height == -1) {
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();
    }

    m_npWindow.x = m_windowRect.x();
    m_npWindow.y = m_windowRect.y();

    m_npWindow.clipRect.left = m_clipRect.x();
    m_npWindow.clipRect.top = m_clipRect.y();
    m_npWindow.clipRect.right = m_clipRect.width();
    m_npWindow.clipRect.bottom = m_clipRect.height();

    QApplication::syncX();

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
    if (platformWidget()) {
        platformWidget()->update(rect);
        return;
    }

    invalidateWindowlessPluginRect(rect);
}

void PluginView::invalidateRect(NPRect* rect)
{
    notImplemented();
}

void PluginView::invalidateRegion(NPRegion region)
{
    notImplemented();
}

void PluginView::forceRedraw()
{
    notImplemented();
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

    if (m_needsXEmbed) {
        QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
        setPlatformWidget(new PluginContainerQt(this, QWidget::find(client->winId())));
    } else {
        notImplemented();
        return false;
    }

    show();

    NPSetWindowCallbackStruct *wsi = new NPSetWindowCallbackStruct();

    wsi->type = 0;

    wsi->display = platformPluginWidget()->x11Info().display();
    wsi->visual = (Visual*)platformPluginWidget()->x11Info().visual();
    wsi->depth = platformPluginWidget()->x11Info().depth();
    wsi->colormap = platformPluginWidget()->x11Info().colormap();
    m_npWindow.ws_info = wsi;

    m_npWindow.type = NPWindowTypeWindow;
    m_npWindow.window = (void*)platformPluginWidget()->winId();
    m_npWindow.width = -1;
    m_npWindow.height = -1;

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
}

} // namespace WebCore
