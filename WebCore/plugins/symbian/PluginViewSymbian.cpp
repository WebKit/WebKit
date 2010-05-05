/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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
#include "PluginView.h"

#include "Bridge.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HostWindow.h"
#include "Image.h"
#include "JSDOMBinding.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PluginContainerSymbian.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
#include "QWebPageClient.h"
#include "RenderLayer.h"
#include "ScriptController.h"
#include "Settings.h"
#include "npfunctions.h"
#include "npinterface.h"
#include "npruntime_impl.h"
#include "qgraphicswebview.h"
#include "runtime_root.h"
#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QPixmap>
#include <QRegion>
#include <QVector>
#include <QWidget>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using namespace std;

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

    // in order to move/resize the plugin window at the same time as the rest of frame
    // during e.g. scrolling, we set the mask and geometry in the paint() function, but
    // as paint() isn't called when the plugin window is outside the frame which can
    // be caused by a scroll, we need to move/resize immediately.
    if (!m_windowRect.intersects(frameView->frameRect()))
        setNPWindowIfNeeded();
}

void PluginView::setFocus(bool focused)
{
    if (platformPluginWidget()) {
        if (focused)
            platformPluginWidget()->setFocus(Qt::OtherFocusReason);
    } else {
        Widget::setFocus(focused);
    }
}

void PluginView::show()
{
    setSelfVisible(true);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(true);
}

void PluginView::hide()
{
    setSelfVisible(false);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(false);
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }
    
    if (context->paintingDisabled())
        return;
    m_npWindow.ws_info = (void*)(context->platformContext());
    setNPWindowIfNeeded();

    if (m_isWindowed && platformPluginWidget())
        static_cast<PluginContainerSymbian*>(platformPluginWidget())->adjustGeometry();

    if (m_isWindowed)
        return;

    context->save();
    IntRect clipRect(rect);
    clipRect.intersect(frameRect());
    context->clip(clipRect);
    context->translate(frameRect().location().x(), frameRect().location().y());

    QPaintEvent ev(rect);
    QEvent& npEvent = ev;
    dispatchNPEvent(npEvent);

    context->restore();
}

// TODO: Unify across ports.
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    if (!m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);

    setCallingPlugin(true);
    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    return accepted;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_isWindowed)
        return;

    QEvent& npEvent = *(event->keyEvent()->qtEvent());
    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
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

    QEvent::Type type;
    if (event->type() == eventNames().mousedownEvent)
        type = QEvent::MouseButtonPress;
    else if (event->type() == eventNames().mousemoveEvent)
        type = QEvent::MouseMove;
    else if (event->type() == eventNames().mouseupEvent)
        type = QEvent::MouseButtonRelease;
    else
        return;

    QPoint position(event->offsetX(), event->offsetY());
    Qt::MouseButton button;
    switch (event->which()) {
    case 1:
        button = Qt::LeftButton;
        break;
    case 2:
        button = Qt::MidButton;
        break;
    case 3:
        button = Qt::RightButton;
        break;
    default:
        button = Qt::NoButton;
    }
    Qt::KeyboardModifiers modifiers = 0;
    if (event->ctrlKey())
        modifiers |= Qt::ControlModifier;
    if (event->altKey())
        modifiers |= Qt::AltModifier;
    if (event->shiftKey())
        modifiers |= Qt::ShiftModifier;
    if (event->metaKey())
        modifiers |= Qt::MetaModifier;
    QMouseEvent mouseEvent(type, position, button, button, modifiers); 
    QEvent& npEvent = mouseEvent;
    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
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
        // always call this method before painting.
        m_npWindow.x = 0;
        m_npWindow.y = 0;
    
        m_npWindow.clipRect.left = 0;
        m_npWindow.clipRect.top = 0;
        m_npWindow.clipRect.right = m_windowRect.width();
        m_npWindow.clipRect.bottom = m_windowRect.height();
        m_npWindow.window = 0;
    }

    m_npWindow.width = m_windowRect.width();
    m_npWindow.height = m_windowRect.height();
    
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

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32_t len, const char* buf)
{
    notImplemented();
    return NPERR_NO_ERROR;
}

bool PluginView::platformGetValueStatic(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
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

bool PluginView::platformGetValue(NPNVariable, void*, NPError*)
{
    return false;
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
    if (m_isWindowed)
        return;
    if (!rect) {
        invalidate();
        return;
    }
    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    m_invalidRects.append(r);
    if (!m_invalidateTimer.isActive())
        m_invalidateTimer.startOneShot(0.001);
}

void PluginView::invalidateRegion(NPRegion region)
{
    if (m_isWindowed)
        return;
    
    if (!region) 
        return;
    
    QVector<QRect> rects = region->rects();
    for (int i = 0; i < rects.size(); ++i) {
        const QRect& qRect = rects.at(i);
        m_invalidRects.append(qRect);
        if (!m_invalidateTimer.isActive())
            m_invalidateTimer.startOneShot(0.001);
    }
}

void PluginView::forceRedraw()
{
    if (m_isWindowed)
        return;
    invalidate();
}

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);
    
    show();

    if (m_isWindowed) {
        QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
        QGraphicsProxyWidget* proxy = 0;
        if (QGraphicsWebView *webView = qobject_cast<QGraphicsWebView*>(client->pluginParent()))
            proxy = new QGraphicsProxyWidget(webView);

        PluginContainerSymbian* container = new PluginContainerSymbian(this, proxy ? 0 : client->ownerWidget(), proxy);
        setPlatformWidget(container);
        if (proxy)
            proxy->setWidget(container);
        
        m_npWindow.type = NPWindowTypeWindow;
        m_npWindow.window = (void*)platformPluginWidget();

    } else {
        setPlatformWidget(0);
        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0; // Not used?
    }    
    setNPWindowIfNeeded();
    
    return true;
}

void PluginView::platformDestroy()
{
    if (platformPluginWidget()) {
        PluginContainerSymbian* container = static_cast<PluginContainerSymbian*>(platformPluginWidget());
        delete container->proxy();
        delete container;
    }
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

} // namespace WebCore
