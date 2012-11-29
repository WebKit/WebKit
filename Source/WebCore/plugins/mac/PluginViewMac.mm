/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "BridgeJSC.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
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
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "PluginMainThreadScheduler.h"
#include "RenderObject.h"
#include "ScriptController.h"
#include "Settings.h"
#include "WheelEvent.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <wtf/RetainPtr.h>


using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::JSValue;

#if PLATFORM(QT)
#include <QWidget>
#include <QKeyEvent>
#include <QPainter>
#include <QDateTime>
#include <QPixmap>
#include "QWebPageClient.h"
QT_BEGIN_NAMESPACE
extern Q_GUI_EXPORT OSWindowRef qt_mac_window_for(const QWidget* w);
extern Q_GUI_EXPORT CGContextRef qt_mac_cg_context(const QPaintDevice *pdev); //qpaintdevice_mac.cpp
QT_END_NAMESPACE
#endif

#if PLATFORM(WX)
#include <wx/defs.h>
#include <wx/wx.h>
#endif

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

#ifndef NP_NO_CARBON
static int modifiersForEvent(UIEventWithKeyState *event);
#endif

static inline WindowRef nativeWindowFor(PlatformWidget widget)
{
#if PLATFORM(QT)
    if (widget)
#if QT_MAC_USE_COCOA
        return static_cast<WindowRef>([qt_mac_window_for(static_cast<QWidget*>(widget)) windowRef]);
#else
        return static_cast<WindowRef>(qt_mac_window_for(widget));
#endif
#elif PLATFORM(WX)
    if (widget)
        return (WindowRef)widget->MacGetTopLevelWindowRef();
#endif
    return 0;
}

static inline CGContextRef cgHandleFor(PlatformWidget widget)
{
#if PLATFORM(QT)
    if (widget)
        return (CGContextRef)static_cast<QWidget*>(widget)->macCGHandle();
#endif
#if PLATFORM(WX)
    if (widget)
        return (CGContextRef)widget->MacGetCGContextRef();
#endif
    return 0;
}

static inline IntPoint topLevelOffsetFor(PlatformWidget widget)
{
#if PLATFORM(QT)
    if (widget) {
        QWidget* topLevel = static_cast<QWidget*>(widget)->window();
        return static_cast<QWidget*>(widget)->mapTo(topLevel, QPoint(0, 0)) + topLevel->geometry().topLeft() - topLevel->pos();
    }
#endif
#if PLATFORM(WX)
    if (widget) {
        PlatformWidget toplevel = wxGetTopLevelParent(widget);
        return toplevel->ScreenToClient(widget->GetScreenPosition());
    }
#endif
    return IntPoint();
}

// --------- Cocoa specific utility functions ----------

static void initializeNPCocoaEvent(NPCocoaEvent* event)
{
    memset(event, 0, sizeof(NPCocoaEvent));
}

static int32_t getModifiers(UIEventWithKeyState *event)
{
    int32_t modifiers = 0;
    if (event->keyCode() == 57) modifiers |= NSAlphaShiftKeyMask;
    if (event->shiftKey())  modifiers |= NSShiftKeyMask;
    if (event->ctrlKey())   modifiers |= NSControlKeyMask;
    if (event->metaKey())   modifiers |= NSCommandKeyMask;
    if (event->altKey())    modifiers |= NSAlternateKeyMask;

    return modifiers;
}

// --------------- Lifetime management -----------------

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);

    if (m_drawingModel == NPDrawingModel(-1)) {
        // We default to QuickDraw, even though we don't support it,
        // since that's what Safari does, and some plugins expect this
        // behavior and never set the drawing model explicitly.
#ifndef NP_NO_QUICKDRAW
        m_drawingModel = NPDrawingModelQuickDraw;
#else
        // QuickDraw not available, so we have to default to CoreGraphics
        m_drawingModel = NPDrawingModelCoreGraphics;
#endif
    }

    if (m_eventModel == NPEventModel(-1)) {
        // If the plug-in did not specify an event model
        // we default to Carbon, when it is available.
#ifndef NP_NO_CARBON
        m_eventModel = NPEventModelCarbon;
#else
        m_eventModel = NPEventModelCocoa;
#endif
    }

    // Gracefully handle unsupported drawing or event models. We can do this
    // now since the drawing and event model can only be set during NPP_New.
#ifndef NP_NO_CARBON
    NPBool eventModelSupported;
    if (getValueStatic(NPNVariable(NPNVsupportsCarbonBool + m_eventModel), &eventModelSupported) != NPERR_NO_ERROR
            || !eventModelSupported) {
#else
    NPBool eventModelSupported;
    if (getValueStatic(NPNVariable(NPNVsupportsCocoaBool/* + m_eventModel*/), &eventModelSupported) != NPERR_NO_ERROR
            || !eventModelSupported) {
#endif
        m_status = PluginStatusCanNotLoadPlugin;
        LOG(Plugins, "Plug-in '%s' uses unsupported event model %s",
                m_plugin->name().utf8().data(), prettyNameForEventModel(m_eventModel));
        return false;
    }

#ifndef NP_NO_QUICKDRAW
    NPBool drawingModelSupported;
    if (getValueStatic(NPNVariable(NPNVsupportsQuickDrawBool + m_drawingModel), &drawingModelSupported) != NPERR_NO_ERROR
            || !drawingModelSupported) {
        m_status = PluginStatusCanNotLoadPlugin;
        LOG(Plugins, "Plug-in '%s' uses unsupported drawing model %s",
                m_plugin->name().utf8().data(), prettyNameForDrawingModel(m_drawingModel));
        return false;
    }
#endif

#ifdef NP_NO_QUICKDRAW
    NPBool drawingModelSupported;
    if (getValueStatic(NPNVariable(NPNVsupportsCoreGraphicsBool/* + m_drawingModel*/), &drawingModelSupported) != NPERR_NO_ERROR
            || !drawingModelSupported) {
        m_status = PluginStatusCanNotLoadPlugin;
        LOG(Plugins, "Plug-in '%s' uses unsupported drawing model %s",
                m_plugin->name().utf8().data(), prettyNameForDrawingModel(m_drawingModel));
        return false;
    }
#endif


#if PLATFORM(QT)
    // Set the platformPluginWidget only in the case of QWebView so that the context menu appears in the right place.
    // In all other cases, we use off-screen rendering
    if (QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient()) {
        if (QWidget* widget = qobject_cast<QWidget*>(client->pluginParent()))
            setPlatformPluginWidget(widget);
    }
#endif
#if PLATFORM(WX)
    if (wxWindow* widget = m_parentFrame->view()->hostWindow()->platformPageClient())
        setPlatformPluginWidget(widget);
#endif

    // Create a fake window relative to which all events will be sent when using offscreen rendering
#ifndef NP_NO_CARBON
    if (!platformPluginWidget()) {
        // Make the default size really big. It is unclear why this is required but with a smaller size, mouse move
        // events don't get processed. Resizing the fake window to flash's size doesn't help.
        ::Rect windowBounds = { 0, 0, 1000, 1000 };
        CreateNewWindow(kDocumentWindowClass, kWindowStandardDocumentAttributes, &windowBounds, &m_fakeWindow);
        // Flash requires the window to be hilited to process mouse move events.
        HiliteWindow(m_fakeWindow, true);
    }
#endif

    updatePluginWidget();

    if (!m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall))
        setNPWindowIfNeeded();

#ifndef NP_NO_CARBON
    // TODO: Implement null timer throttling depending on plugin activation
    m_nullEventTimer = adoptPtr(new Timer<PluginView>(this, &PluginView::nullEventTimerFired));
    m_nullEventTimer->startRepeating(0.02);

    m_lastMousePos.h = m_lastMousePos.v = 0;
#endif // NP_NO_CARBON

    return true;
}

void PluginView::platformDestroy()
{
    if (platformPluginWidget())
        setPlatformPluginWidget(0);
    else {
        CGContextRelease(m_contextRef);
#ifndef NP_NO_CARBON
        if (m_fakeWindow)
            DisposeWindow(m_fakeWindow);
#endif
    }
}

// Used before the plugin view has been initialized properly, and as a
// fallback for variables that do not require a view to resolve.
bool PluginView::platformGetValueStatic(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVToolkit:
        *static_cast<uint32_t*>(value) = 0;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

#ifndef NP_NO_CARBON
    case NPNVsupportsCarbonBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

#endif
    case NPNVsupportsCocoaBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    // CoreGraphics is the only drawing model we support
    case NPNVsupportsCoreGraphicsBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVsupportsAdvancedKeyHandling:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

#ifndef NP_NO_QUICKDRAW
    // QuickDraw is deprecated in 10.5 and not supported on 64-bit
    case NPNVsupportsQuickDrawBool:
#endif
    case NPNVsupportsOpenGLBool:
    case NPNVsupportsCoreAnimationBool:
        *static_cast<NPBool*>(value) = false;
        *result = NPERR_NO_ERROR;
        return true;

    default:
        return false;
    }
}

// Used only for variables that need a view to resolve
bool PluginView::platformGetValue(NPNVariable variable, void* value, NPError* error)
{
    // In WebKit2, this is set if the plugin queries it's availiablity and
    // no key down events have already been sent.
    if (variable == NPNVsupportsUpdatedCocoaTextInputBool) {
        if (m_keyDownSent && !m_updatedCocoaTextInputRequested) {
            *static_cast<NPBool*>(value) = false;
            *error = NPERR_NO_ERROR;
        }
        else {
            *static_cast<NPBool*>(value) = true;
            *error = NPERR_NO_ERROR;
            m_updatedCocoaTextInputRequested = true;
        }
        return true;
    }
    return false;
}

void PluginView::setParent(ScrollView* parent)
{
    LOG(Plugins, "PluginView::setParent(%p)", parent);

    Widget::setParent(parent);

    if (parent)
        init();
}

// -------------- Geometry and painting ----------------

void PluginView::show()
{
    LOG(Plugins, "PluginView::show()");

    setSelfVisible(true);

    Widget::show();
}

void PluginView::hide()
{
    LOG(Plugins, "PluginView::hide()");

    setSelfVisible(false);

    Widget::hide();
}

void PluginView::setFocus(bool focused)
{
    LOG(Plugins, "PluginView::setFocus(%d)", focused);
    if (!focused) {
        Widget::setFocus(focused);
        if (m_eventModel != NPEventModelCocoa)
            return;
    }

    if (platformPluginWidget())
#if PLATFORM(QT)
        static_cast<QWidget*>(platformPluginWidget())->setFocus(Qt::OtherFocusReason);
#else
        platformPluginWidget()->SetFocus();
#endif
    else
        Widget::setFocus(focused);


#ifndef NP_NO_CARBON
    // TODO: Also handle and pass on blur events (focus lost)
    EventRecord record;
    record.what = NPEventType_GetFocusEvent;
    record.message = 0;
    record.when = TickCount();
    record.where = globalMousePosForPlugin();
    record.modifiers = GetCurrentKeyModifiers();

    if (!dispatchNPEvent(record))
        LOG(Events, "PluginView::setFocus(%d): Focus event not accepted", focused);
#endif
    {
        NPCocoaEvent cocoaEvent;
        initializeNPCocoaEvent(&cocoaEvent);
        cocoaEvent.type = NPCocoaEventFocusChanged;
        NPBool focus = focused;
        cocoaEvent.data.focus.hasFocus = focus;

        if(!dispatchNPCocoaEvent(cocoaEvent)) {
            LOG(Events, "PluginView::setFocus(): Focus event %d not accepted", cocoaEvent.type);
        }
    }
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);
}

void PluginView::setNPWindowRect(const IntRect&)
{
    setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    CGContextRef newContextRef = 0;
    WindowRef newWindowRef = 0;
    if (platformPluginWidget()) {
        newContextRef = cgHandleFor(platformPluginWidget());
        newWindowRef = nativeWindowFor(platformPluginWidget());
        m_npWindow.type = NPWindowTypeWindow;
    } else {
        newContextRef = m_contextRef;
        newWindowRef = m_fakeWindow;
        m_npWindow.type = NPWindowTypeDrawable;
    }

    if (!newContextRef || !newWindowRef) {
        if (!m_usePixmap)
            return;
    }

    m_npWindow.window = (void*)&m_npCgContext;
#ifndef NP_NO_CARBON
    m_npCgContext.window = newWindowRef;
#endif
    m_npCgContext.context = newContextRef;

    m_npWindow.x = m_windowRect.x();
    m_npWindow.y = m_windowRect.y();
    m_npWindow.width = m_windowRect.width();
    m_npWindow.height = m_windowRect.height();

    // TODO: (also clip against scrollbars, etc.)
    m_npWindow.clipRect.left = max(0, m_windowRect.x());
    m_npWindow.clipRect.top = max(0, m_windowRect.y());
    m_npWindow.clipRect.right = m_windowRect.x() + m_windowRect.width();
    m_npWindow.clipRect.bottom = m_windowRect.y() + m_windowRect.height();

    LOG(Plugins, "PluginView::setNPWindowIfNeeded(): window=%p, context=%p,"
            " window.x:%d window.y:%d window.width:%d window.height:%d window.clipRect size:%dx%d",
            newWindowRef, newContextRef, m_npWindow.x, m_npWindow.y, m_npWindow.width, m_npWindow.height,
            m_npWindow.clipRect.right - m_npWindow.clipRect.left, m_npWindow.clipRect.bottom - m_npWindow.clipRect.top);

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonJSGlobalData());
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::updatePluginWidget()
{
    if (!parent())
       return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = frameView->contentsToWindow(frameRect());
    IntPoint offset = topLevelOffsetFor(platformPluginWidget());
    m_windowRect.move(offset.x(), offset.y());

    if (!platformPluginWidget()) {
        if (m_windowRect.size() != oldWindowRect.size()) {
            CGContextRelease(m_contextRef);
#if PLATFORM(QT)
            m_pixmap = QPixmap(m_windowRect.size());
            m_pixmap.fill(Qt::transparent);
            m_contextRef = m_pixmap.isNull() ? 0 : qt_mac_cg_context(&m_pixmap);
#endif
        }
    }

    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (platformPluginWidget() && (m_windowRect != oldWindowRect || m_clipRect != oldClipRect))
        setNPWindowIfNeeded();
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted || m_status != PluginStatusLoadedSuccessfully) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    if (context->paintingDisabled())
        return;
#if PLATFORM(QT)
    QPainter* p = context->platformContext();
    CGContextRef cgContext = qt_mac_cg_context(p->device());
#else
    CGContextRef cgContext = m_npCgContext.context;
#endif
    setNPWindowIfNeeded();
    if (!cgContext) {
        cgContext = m_contextRef;
        if (!cgContext)
            return;
        else {
            m_usePixmap = true;
            setNPWindowIfNeeded();
        }
    } else
        m_usePixmap = false;

    bool oldUsePixmap = m_usePixmap;
    if (m_isTransparent && !m_usePixmap) {
        if (m_pixmap.isNull())
            m_pixmap = QPixmap(frameRect().size());
        m_usePixmap = true;
        setNPWindowIfNeeded();
        cgContext = qt_mac_cg_context(&m_pixmap);
    }

    CGContextSaveGState(cgContext);
    if (platformPluginWidget()) {
        IntPoint offset = frameRect().location();
        CGContextTranslateCTM(cgContext, offset.x(), offset.y());
    }

    IntRect targetRect(frameRect());
    targetRect.intersects(rect);

    // clip the context so that plugin only updates the interested area.
    CGRect r;
    r.origin.x = targetRect.x() - frameRect().x();
    r.origin.y = targetRect.y() - frameRect().y();
    r.size.width = targetRect.width();
    r.size.height = targetRect.height();
    CGContextClipToRect(cgContext, r);

    if (!platformPluginWidget() || m_isTransparent) { // clean the pixmap in transparent mode
#if PLATFORM(QT)
        QPainter painter(&m_pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(QRectF(r.origin.x, r.origin.y, r.size.width, r.size.height), Qt::transparent);
#endif
    }

#ifndef NP_NO_CARBON
    if (m_eventModel != NPEventModelCocoa) {
        EventRecord event;
        event.what = updateEvt;
        event.message = (long unsigned int)m_npCgContext.window;
        event.when = TickCount();
        event.where.h = 0;
        event.where.v = 0;
        event.modifiers = GetCurrentKeyModifiers();

        if (!dispatchNPEvent(event))
            LOG(Events, "PluginView::paint(): Paint event not accepted");
    } else
#endif
    {
        NPCocoaEvent cocoaEvent;
        initializeNPCocoaEvent(&cocoaEvent);
        cocoaEvent.type = NPCocoaEventDrawRect;
        cocoaEvent.data.draw.x = m_usePixmap ? 0 : r.origin.x;
        cocoaEvent.data.draw.y = m_usePixmap ? 0 : r.origin.y;
        cocoaEvent.data.draw.width = m_usePixmap ? m_pixmap.width() : r.size.width;
        cocoaEvent.data.draw.height = m_usePixmap ? m_pixmap.height() : r.size.height;
        cocoaEvent.data.draw.context = cgContext;
        
        if(!dispatchNPCocoaEvent(cocoaEvent))
            LOG(Events, "PluginView::paint(): Paint event type %d not accepted", cocoaEvent.type);
    }
    
    if (!platformPluginWidget() || m_isTransparent) {
#if PLATFORM(QT)
        QPainter* painter = context->platformContext();
        painter->drawPixmap(targetRect.x(), targetRect.y(), m_pixmap,
                            targetRect.x() - frameRect().x(), targetRect.y() - frameRect().y(), targetRect.width(), targetRect.height());
#endif
    }
    CGContextRestoreGState(cgContext);
    if (oldUsePixmap != m_usePixmap) {
        m_usePixmap = oldUsePixmap;
        m_pixmap = QPixmap();
    }    
}

bool PluginView::popUpContextMenu(NPMenu *menu)
{
    NSEvent* currentEvent = [NSApp currentEvent];
    
    // NPN_PopUpContextMenu must be called from within the plug-in's NPP_HandleEvent.
    if (!currentEvent)
        return NPERR_GENERIC_ERROR;
    
    NSWindow* window = [currentEvent window];
    NSView* view = [window contentView];
    [NSMenu popUpContextMenu:(NSMenu*)menu withEvent:currentEvent forView:view];
    return true;
}
    
void PluginView::invalidateRect(const IntRect& rect)
{
    if (platformPluginWidget() && m_isTransparent)
#if PLATFORM(QT)
        static_cast<QWidget*>(platformPluginWidget())->update(convertToContainingWindow(rect));
#else
        platformPluginWidget()->RefreshRect(convertToContainingWindow(rect));
#endif
    invalidateWindowlessPluginRect(rect);
}

void PluginView::invalidateRect(NPRect* rect)
{
    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    invalidateRect(r);
}

void PluginView::invalidateRegion(NPRegion region)
{
    // TODO: optimize
    invalidate();
}

void PluginView::forceRedraw()
{
    notImplemented();
}


// ----------------- Event handling --------------------
void PluginView::handleWheelEvent(WheelEvent *event)
{
    if (!m_isStarted || m_eventModel != NPEventModelCocoa)
        return;
    
    NPCocoaEvent cocoaEvent;
    initializeNPCocoaEvent(&cocoaEvent);
    
    NSEvent *currentEvent = [NSApp currentEvent];
    
    cocoaEvent.type = NPCocoaEventScrollWheel;
    
    cocoaEvent.data.mouse.pluginX = event->layerX() - m_npWindow.x + m_windowRect.x() - m_element->offsetLeft();
    cocoaEvent.data.mouse.pluginY = event->layerY() - m_npWindow.y + m_windowRect.y() - m_element->offsetTop();
    cocoaEvent.data.mouse.deltaX = [currentEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [currentEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [currentEvent deltaZ];
    cocoaEvent.data.mouse.modifierFlags = getModifiers(event);
    
    if(!dispatchNPCocoaEvent(cocoaEvent)) {
        LOG(Events, "PluginView::handleMouseEvent(): Wheel event type %d at %d,%d not accepted", cocoaEvent.type
            cocoaEvent.data.mouse.x, cocoaEvent.data.mouse.y);
    }
    event->setDefaultHandled();
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    if (!m_isStarted)
        return;
    
#ifndef NP_NO_CARBON
    if (m_eventModel != NPEventModelCocoa) {
        EventRecord record;
        
        if (event->type() == eventNames().mousemoveEvent) {
            // Mouse movement is handled by null timer events
            m_lastMousePos = mousePosForPlugin(event);
            return;
        } else if (event->type() == eventNames().mouseoverEvent) {
            record.what = NPEventType_AdjustCursorEvent;
        } else if (event->type() == eventNames().mouseoutEvent) {
            record.what = NPEventType_AdjustCursorEvent;
        } else if (event->type() == eventNames().mousedownEvent) {
            record.what = mouseDown;
            // The plugin needs focus to receive keyboard events
            if (Page* page = m_parentFrame->page())
                page->focusController()->setFocusedFrame(m_parentFrame);
            m_parentFrame->document()->setFocusedNode(m_element);
        } else if (event->type() == eventNames().mouseupEvent) {
            record.what = mouseUp;
        } else {
            return;
        }
        record.where = mousePosForPlugin(event);
        record.modifiers = modifiersForEvent(event);
        
        if (!event->buttonDown())
            record.modifiers |= btnState;
        
        if (event->button() == 2)
            record.modifiers |= controlKey;
        
        if (!dispatchNPEvent(record)) {
            if (record.what == NPEventType_AdjustCursorEvent)
                return; // Signals that the plugin wants a normal cursor
            
            LOG(Events, "PluginView::handleMouseEvent(): Mouse event type %d at %d,%d not accepted",
                record.what, record.where.h, record.where.v);
        } else {
            event->setDefaultHandled();
        }
    } else
#endif
    {
        NPCocoaEventType eventType;
        int32_t buttonNumber = 0;
        int32_t clickCount = 0;
        NSEvent *currentEvent = [NSApp currentEvent];
        
        NSEventType type = [currentEvent type];
        
        switch (type) {
            case NSLeftMouseDown:
            case NSRightMouseDown:
            case NSOtherMouseDown:
                buttonNumber = [currentEvent buttonNumber];
                clickCount = [currentEvent clickCount];
                eventType = NPCocoaEventMouseDown;
                // The plugin needs focus to receive keyboard events
                if (Page* page = m_parentFrame->page())
                    page->focusController()->setFocusedFrame(m_parentFrame);
                m_parentFrame->document()->setFocusedNode(m_element);
                break;
                
            case NSLeftMouseUp:
            case NSRightMouseUp:
            case NSOtherMouseUp:
                buttonNumber = [currentEvent buttonNumber];
                clickCount = [currentEvent clickCount];
                eventType = NPCocoaEventMouseUp;
                break;
                
            case NSMouseMoved:
                eventType = NPCocoaEventMouseMoved;
                break;
                
            case NSLeftMouseDragged:
            case NSRightMouseDragged:
            case NSOtherMouseDragged:
                buttonNumber = [currentEvent buttonNumber];
                eventType = NPCocoaEventMouseDragged;
                break;
                
            case NSMouseEntered:
                eventType = NPCocoaEventMouseEntered;
                break;
                
            case NSMouseExited:
                eventType = NPCocoaEventMouseExited;
            default:
                return;
        }
        
        NPCocoaEvent cocoaEvent;
        initializeNPCocoaEvent(&cocoaEvent);
        
        cocoaEvent.type = eventType;
        if (!(NPCocoaEventMouseEntered == eventType || NPCocoaEventMouseExited == eventType)) {
            cocoaEvent.data.mouse.buttonNumber = buttonNumber;
            cocoaEvent.data.mouse.clickCount = clickCount;
        }
        
        cocoaEvent.data.mouse.pluginX = event->layerX() - m_npWindow.x + m_windowRect.x() - m_element->offsetLeft();
        cocoaEvent.data.mouse.pluginY = event->layerY() - m_npWindow.y + m_windowRect.y() - m_element->offsetTop();
        cocoaEvent.data.mouse.deltaX = [currentEvent deltaX];
        cocoaEvent.data.mouse.deltaY = [currentEvent deltaY];
        cocoaEvent.data.mouse.deltaZ = [currentEvent deltaZ];
        cocoaEvent.data.mouse.modifierFlags = getModifiers(event);
        
        int16_t response = dispatchNPCocoaEvent(cocoaEvent);
        if(response = kNPEventNotHandled) {
            LOG(Events, "PluginView::handleMouseEvent(): Mouse event type %d at %d,%d not accepted", cocoaEvent.type
                cocoaEvent.data.mouse.x, cocoaEvent.data.mouse.y);
        }
        
        // Safari policy is to return true for all mouse events, because some plugins
        // return false even if they have handled the event.
        event->setDefaultHandled();
    }
}
    
void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    if (!m_isStarted)
        return;
    LOG(Plugins, "PluginView::handleKeyboardEvent() ----------------- ");
    
    LOG(Plugins, "PV::hKE(): KE.keyCode: 0x%02X, KE.charCode: %d",
        event->keyCode(), event->charCode());
    
#ifndef NP_NO_CARBON
    if (m_eventModel != NPEventModelCocoa) {
        EventRecord record;
        if (event->type() == eventNames().keydownEvent) {
            // This event is the result of a PlatformKeyboardEvent::KeyDown which
            // was disambiguated into a PlatformKeyboardEvent::RawKeyDown. Since
            // we don't have access to the text here, we return, and wait for the
            // corresponding event based on PlatformKeyboardEvent::Char.
            return;
        } else if (event->type() == eventNames().keypressEvent) {
            // Which would be this one. This event was disambiguated from the same
            // PlatformKeyboardEvent::KeyDown, but to a PlatformKeyboardEvent::Char,
            // which retains the text from the original event. So, we can safely pass
            // on the event as a key-down event to the plugin.
            record.what = keyDown;
        } else if (event->type() == eventNames().keyupEvent) {
            // PlatformKeyboardEvent::KeyUp events always have the text, so nothing
            // fancy here.
            record.what = keyUp;
        } else {
            return;
        }
        
        const PlatformKeyboardEvent* platformEvent = event->keyEvent();
        int keyCode = platformEvent->nativeVirtualKeyCode();
        
        const String text = platformEvent->text();
        if (text.length() < 1) {
            event->setDefaultHandled();
            return;
        }
        
        WTF::RetainPtr<CFStringRef> cfText(WTF::AdoptCF, text.createCFString());
        
        LOG(Plugins, "PV::hKE(): PKE.text: %s, PKE.unmodifiedText: %s, PKE.keyIdentifier: %s",
            text.ascii().data(), platformEvent->unmodifiedText().ascii().data(),
            platformEvent->keyIdentifier().ascii().data());
        
        char charCodes[2] = { 0, 0 };
        if (!CFStringGetCString(cfText.get(), charCodes, 2, CFStringGetSystemEncoding())) {
            LOG_ERROR("Could not resolve character code using system encoding.");
            event->setDefaultHandled();
            return;
        }
        
        record.where = globalMousePosForPlugin();
        record.modifiers = modifiersForEvent(event);
        record.message = ((keyCode & 0xFF) << 8) | (charCodes[0] & 0xFF);
        record.when = TickCount();
        
        LOG(Plugins, "PV::hKE(): record.modifiers: %d", record.modifiers);
        
#if PLATFORM(QT)
        LOG(Plugins, "PV::hKE(): PKE.qtEvent()->nativeVirtualKey: 0x%02X, charCode: %d",
            keyCode, int(uchar(charCodes[0])));
#endif
        
        if (!dispatchNPEvent(record))
            LOG(Events, "PluginView::handleKeyboardEvent(): Keyboard event type %d not accepted", record.what);
        else
            event->setDefaultHandled();
    } else
#endif
    {
        NSEvent *currentEvent = [NSApp currentEvent];
        NPCocoaEventType eventType;
        NSEventType type = [currentEvent type];
        
        switch (type) {
            case NSKeyDown:
                eventType = NPCocoaEventKeyDown;
                m_keyDownSent = true;
                break;
            case NSKeyUp:
                if (m_disregardKeyUpCounter > 0) {
                    m_disregardKeyUpCounter--;
                    event->setDefaultHandled();
                    return;
                }
                eventType = NPCocoaEventKeyUp;
                break;
            case NSFlagsChanged:
                eventType = NPCocoaEventFlagsChanged;
                break;
            default:
                return;
        }
        
        NPCocoaEvent cocoaEvent;
        initializeNPCocoaEvent(&cocoaEvent);
        cocoaEvent.type = eventType;
        if (eventType != NPCocoaEventFlagsChanged) {
            NSString *characters = [currentEvent characters];
            NSString *charactersIgnoringModifiers = [currentEvent charactersIgnoringModifiers];
            cocoaEvent.data.key.characters = reinterpret_cast<NPNSString*>(characters);
            cocoaEvent.data.key.charactersIgnoringModifiers = reinterpret_cast<NPNSString*>(charactersIgnoringModifiers);
            cocoaEvent.data.key.isARepeat = [currentEvent isARepeat];
            cocoaEvent.data.key.keyCode = [currentEvent keyCode];
            cocoaEvent.data.key.modifierFlags = getModifiers(event);
        }
        
        int16_t response = dispatchNPCocoaEvent(cocoaEvent);
        if(response == kNPEventNotHandled) {
            LOG(Events, "PluginView::handleKeyboardEvent(): Keyboard event type %d not accepted", cocoaEvent.type);
        } else if (response == kNPEventStartIME) {
            // increment counter and resend as a text input
            m_disregardKeyUpCounter++;
            NPCocoaEvent textEvent;
            initializeNPCocoaEvent(&textEvent);
            textEvent.type = NPCocoaEventTextInput;
            textEvent.data.text.text = reinterpret_cast<NPNSString*>([currentEvent characters]);
            response = dispatchNPCocoaEvent(textEvent);
            if(response == kNPEventNotHandled)
                LOG(Events, "PluginView::handleKeyboardEvent(): Keyboard event type %d not accepted", cocoaEvent.type);
        }
        
        // All keyboard events need to be handled to prevent them falling
        // through to the page, unless they are Meta key events, in which
        // case they are, unless they are Cmd+a. From WebKit2, possibly
        // not the most elegant piece of key handling code.....
        if (event->metaKey()) {
            if (cocoaEvent.data.key.keyCode == 0)
                event->setDefaultHandled();
        } else {
            // else ignore, it's a Meta Key event for the browser.
            event->setDefaultHandled();
        }
    }
}
    
#ifndef NP_NO_CARBON
void PluginView::nullEventTimerFired(Timer<PluginView>*)
{
    EventRecord record;

    record.what = nullEvent;
    record.message = 0;
    record.when = TickCount();
    record.where = m_lastMousePos;
    record.modifiers = GetCurrentKeyModifiers();
    if (!Button())
        record.modifiers |= btnState;

    if (!dispatchNPEvent(record))
        LOG(Events, "PluginView::nullEventTimerFired(): Null event not accepted");
}
#endif

#ifndef NP_NO_CARBON
static int modifiersForEvent(UIEventWithKeyState* event)
{
    int modifiers = 0;

    if (event->ctrlKey())
        modifiers |= controlKey;

    if (event->altKey())
        modifiers |= optionKey;

    if (event->metaKey())
        modifiers |= cmdKey;

    if (event->shiftKey())
        modifiers |= shiftKey;

     return modifiers;
}
#endif

#ifndef NP_NO_CARBON
Point PluginView::globalMousePosForPlugin() const
{
    Point pos;
    GetGlobalMouse(&pos);

#if PLATFORM(WX)
    // make sure the titlebar/toolbar size is included
    WindowRef windowRef = nativeWindowFor(platformPluginWidget());
    ::Rect content, structure;

    GetWindowBounds(windowRef, kWindowStructureRgn, &structure);
    GetWindowBounds(windowRef, kWindowContentRgn, &content);

    int top = content.top  - structure.top;
    pos.v -= top;
#endif

    return pos;
}
#endif

#ifndef NP_NO_CARBON
Point PluginView::mousePosForPlugin(MouseEvent* event) const
{
    ASSERT(event);
    if (platformPluginWidget())
        return globalMousePosForPlugin();

    if (event->button() == 2) {
        // always pass the global position for right-click since Flash uses it to position the context menu
        return globalMousePosForPlugin();
    }

    Point pos;
    IntPoint postZoomPos = roundedIntPoint(m_element->renderer()->absoluteToLocal(event->absoluteLocation()));
    pos.h = postZoomPos.x() + m_windowRect.x();
    // The number 22 is the height of the title bar. As to why it figures in the calculation below
    // is left as an exercise to the reader :-)
    pos.v = postZoomPos.y() + m_windowRect.y() - 22;
    return pos;
}
#endif

#ifndef NP_NO_CARBON
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonJSGlobalData());
    setCallingPlugin(true);

    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
    return accepted;
}
#endif


int16_t PluginView::dispatchNPCocoaEvent(NPCocoaEvent& cocoaEvent)
{
    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonJSGlobalData());
    setCallingPlugin(true);

    int16_t response = m_plugin->pluginFuncs()->event(m_instance, &cocoaEvent);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    return response;
}

// ------------------- Miscellaneous  ------------------

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32_t len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    if (!fileExists(filename))
        return NPERR_FILE_NOT_FOUND;

    FILE* fileHandle = fopen((filename.utf8()).data(), "r");

    if (fileHandle == 0)
        return NPERR_FILE_NOT_FOUND;

    int bytesRead = fread(buffer.data(), 1, 0, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

} // namespace WebCore
