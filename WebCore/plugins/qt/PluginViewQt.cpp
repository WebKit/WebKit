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
#include <QWidget>
#include <QX11Info>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

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

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    notImplemented();
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    notImplemented();
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

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(false);
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

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;

    JSC::JSLock::DropAllLocks dropAllLocks(false);

    PluginMainThreadScheduler::scheduler().unregisterPlugin(m_instance);

    // Clear the window
    m_npWindow.window = 0;
    if (m_plugin->pluginFuncs()->setwindow && !m_plugin->quirks().contains(PluginQuirkDontSetNullWindowHandleOnDestroy)) {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    delete (NPSetWindowCallbackStruct *)m_npWindow.ws_info;
    m_npWindow.ws_info = 0;

    // Destroy the plugin
    {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->destroy(m_instance, 0);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    m_instance->pdata = 0;
}

static const char* MozillaUserAgent = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1) Gecko/20061010 Firefox/2.0";

const char* PluginView::userAgent()
{
    if (m_plugin->quirks().contains(PluginQuirkWantsMozillaUserAgent))
        return MozillaUserAgent;

    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();

    return m_userAgent.data();
}

const char* PluginView::userAgentStatic()
{
    //FIXME - Just say we are Mozilla
    return MozillaUserAgent;
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    if (!fileExists(filename))
        return NPERR_FILE_NOT_FOUND;

    //FIXME - read the file data into buffer
    FILE* fileHandle = fopen((filename.utf8()).data(), "r");

    if (fileHandle == 0)
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
    switch (variable) {
    case NPNVxDisplay:
        if (platformPluginWidget())
            *(void **)value = platformPluginWidget()->x11Info().display();
        else
            *(void **)value = m_parentFrame->view()->hostWindow()->platformWindow()->x11Info().display();
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
        *((XID *)w) = m_parentFrame->view()->hostWindow()->platformWindow()->winId();
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

PluginView::~PluginView()
{
    stop();

    deleteAllValues(m_requests);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    m_parentFrame->script()->cleanupScriptObjectsForPlugin(this);

    if (m_plugin && !(m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin)))
        m_plugin->unload();

    delete platformPluginWidget();
}

void PluginView::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

    m_hasPendingGeometryChange = false;

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }

    if (!m_plugin->load()) {
        m_plugin = 0;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (!start()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (m_needsXEmbed) {
        setPlatformWidget(new PluginContainerQt(this, m_parentFrame->view()->hostWindow()->platformWindow()));
    } else {
        notImplemented();
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }
    show ();

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

    m_status = PluginStatusLoadedSuccessfully;
}

} // namespace WebCore
