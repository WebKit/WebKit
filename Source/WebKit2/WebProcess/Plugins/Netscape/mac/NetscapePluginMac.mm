/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetscapePlugin.h"

#import "PluginController.h"
#import "WebEvent.h"
#import <WebCore/GraphicsContext.h>
#import <Carbon/Carbon.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

#ifndef NP_NO_CARBON
static const double nullEventIntervalActive = 0.02;
static const double nullEventIntervalNotActive = 0.25;

static unsigned buttonStateFromLastMouseEvent;

#endif

NPError NetscapePlugin::setDrawingModel(NPDrawingModel drawingModel)
{
    // The drawing model can only be set from NPP_New.
    if (!m_inNPPNew)
        return NPERR_GENERIC_ERROR;

    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
#endif
        case NPDrawingModelCoreGraphics:
        case NPDrawingModelCoreAnimation:
            m_drawingModel = drawingModel;
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}
    
NPError NetscapePlugin::setEventModel(NPEventModel eventModel)
{
    // The event model can only be set from NPP_New.
    if (!m_inNPPNew)
        return NPERR_GENERIC_ERROR;

    switch (eventModel) {
#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
#endif
        case NPEventModelCocoa:
            m_eventModel = eventModel;
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }
    
    return NPERR_NO_ERROR;
}

static double flipScreenYCoordinate(double y)
{
    return [(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame].size.height - y;
}

NPBool NetscapePlugin::convertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double& destX, double& destY, NPCoordinateSpace destSpace)
{
    if (sourceSpace == destSpace) {
        destX = sourceX;
        destY = sourceY;
        return true;
    }

    double sourceXInScreenSpace;
    double sourceYInScreenSpace;

    FloatPoint sourceInScreenSpace;
    switch (sourceSpace) {
    case NPCoordinateSpacePlugin:
        sourceXInScreenSpace = sourceX + m_windowFrameInScreenCoordinates.x() + m_viewFrameInWindowCoordinates.x() + m_npWindow.x;
        sourceYInScreenSpace = m_windowFrameInScreenCoordinates.y() + m_viewFrameInWindowCoordinates.y() + m_viewFrameInWindowCoordinates.height() - (sourceY + m_npWindow.y);
        break;
    case NPCoordinateSpaceWindow:
        sourceXInScreenSpace = sourceX + m_windowFrameInScreenCoordinates.x();
        sourceYInScreenSpace = sourceY + m_windowFrameInScreenCoordinates.y();
        break;
    case NPCoordinateSpaceFlippedWindow:
        sourceXInScreenSpace = sourceX + m_windowFrameInScreenCoordinates.x();
        sourceYInScreenSpace = m_windowFrameInScreenCoordinates.y() + m_windowFrameInScreenCoordinates.height() - sourceY;
        break;
    case NPCoordinateSpaceScreen:
        sourceXInScreenSpace = sourceX;
        sourceYInScreenSpace = sourceY;
        break;
    case NPCoordinateSpaceFlippedScreen:
        sourceXInScreenSpace = sourceX;
        sourceYInScreenSpace = flipScreenYCoordinate(sourceY);
        break;
    default:
        return false;
    }

    // Now convert back.
    switch (destSpace) {
    case NPCoordinateSpacePlugin:
        destX = sourceXInScreenSpace - (m_windowFrameInScreenCoordinates.x() + m_viewFrameInWindowCoordinates.x() + m_npWindow.x);
        destY = m_windowFrameInScreenCoordinates.y() + m_viewFrameInWindowCoordinates.y() + m_viewFrameInWindowCoordinates.height() - (sourceYInScreenSpace + m_npWindow.y);
        break;
    case NPCoordinateSpaceWindow:
        destX = sourceXInScreenSpace - m_windowFrameInScreenCoordinates.x();
        destY = sourceYInScreenSpace - m_windowFrameInScreenCoordinates.y();
        break;
    case NPCoordinateSpaceFlippedWindow:
        destX = sourceXInScreenSpace - m_windowFrameInScreenCoordinates.x();
        destY = sourceYInScreenSpace - m_windowFrameInScreenCoordinates.y();
        destY = m_windowFrameInScreenCoordinates.height() - destY;
        break;
    case NPCoordinateSpaceScreen:
        destX = sourceXInScreenSpace;
        destY = sourceYInScreenSpace;
        break;
    case NPCoordinateSpaceFlippedScreen:
        destX = sourceXInScreenSpace;
        destY = flipScreenYCoordinate(sourceYInScreenSpace);
        break;
    default:
        return false;
    }

    return true;
}


NPError NetscapePlugin::popUpContextMenu(NPMenu* npMenu)
{
    if (!m_currentMouseEvent)
        return NPERR_GENERIC_ERROR;

    double screenX, screenY;
    if (!convertPoint(m_currentMouseEvent->data.mouse.pluginX, m_currentMouseEvent->data.mouse.pluginY, NPCoordinateSpacePlugin, screenX, screenY, NPCoordinateSpaceScreen))
        ASSERT_NOT_REACHED();

    WKPopupContextMenu(reinterpret_cast<NSMenu *>(npMenu), NSMakePoint(screenX, screenY));
    return NPERR_NO_ERROR;
}

mach_port_t NetscapePlugin::compositingRenderServerPort()
{
    return m_pluginController->compositingRenderServerPort();
}

#ifndef NP_NO_CARBON
typedef HashMap<WindowRef, NetscapePlugin*> WindowMap;

static WindowMap& windowMap()
{
    DEFINE_STATIC_LOCAL(WindowMap, windowMap, ());

    return windowMap;
}
#endif

bool NetscapePlugin::platformPostInitialize()
{
    if (m_drawingModel == static_cast<NPDrawingModel>(-1)) {
#ifndef NP_NO_QUICKDRAW
        // Default to QuickDraw if the plugin did not specify a drawing model.
        m_drawingModel = NPDrawingModelQuickDraw;
#else
        // QuickDraw is not available, so we can't default to it. Instead, default to CoreGraphics.
        m_drawingModel = NPDrawingModelCoreGraphics;
#endif
    }
    
    if (m_eventModel == static_cast<NPEventModel>(-1)) {
        // If the plug-in did not specify a drawing model we default to Carbon when it is available.
#ifndef NP_NO_CARBON
        m_eventModel = NPEventModelCarbon;
#else
        m_eventModel = NPEventModelCocoa;
#endif // NP_NO_CARBON
    }

#if !defined(NP_NO_CARBON) && !defined(NP_NO_QUICKDRAW)
    // The CA drawing model does not work with the Carbon event model.
    if (m_drawingModel == NPDrawingModelCoreAnimation && m_eventModel == NPEventModelCarbon)
        return false;
    
    // The Cocoa event model does not work with the QuickDraw drawing model.
    if (m_eventModel == NPEventModelCocoa && m_drawingModel == NPDrawingModelQuickDraw)
        return false;
#endif
    
#ifndef NP_NO_QUICKDRAW
    // Right now we don't support the QuickDraw drawing model at all
    if (m_drawingModel == NPDrawingModelQuickDraw &&
        !m_pluginModule->pluginQuirks().contains(PluginQuirks::AllowHalfBakedQuickDrawSupport))
        return false;
#endif

    if (m_drawingModel == NPDrawingModelCoreAnimation) {
        void* value = 0;
        // Get the Core Animation layer.
        if (NPP_GetValue(NPPVpluginCoreAnimationLayer, &value) == NPERR_NO_ERROR && value) {
            ASSERT(!m_pluginLayer);
            m_pluginLayer = reinterpret_cast<CALayer *>(value);
        }
    }

#ifndef NP_NO_CARBON
    if (m_eventModel == NPEventModelCarbon) {
        // Initialize the fake Carbon window.
        ::Rect bounds = { 0, 0, 0, 0 };
        CreateNewWindow(kDocumentWindowClass, kWindowNoTitleBarAttribute, &bounds, reinterpret_cast<WindowRef*>(&m_npCGContext.window));
        ASSERT(m_npCGContext.window);

        // FIXME: Disable the backing store.

        m_npWindow.window = &m_npCGContext;

        ASSERT(!windowMap().contains(windowRef()));
        windowMap().set(windowRef(), this);

        // Start the null event timer.
        // FIXME: Throttle null events when the plug-in isn't visible on screen.
        m_nullEventTimer.startRepeating(nullEventIntervalActive);
    }
#endif

    return true;
}

void NetscapePlugin::platformDestroy()
{
#ifndef NP_NO_CARBON
    if (m_eventModel == NPEventModelCarbon) {
        if (WindowRef window = windowRef()) {
            // Destroy the fake Carbon window.
            DisposeWindow(window);

            ASSERT(windowMap().contains(window));
            windowMap().remove(window);
        }

        // Stop the null event timer.
        m_nullEventTimer.stop();
    }
#endif
}

bool NetscapePlugin::platformInvalidate(const IntRect&)
{
    // NPN_InvalidateRect is just a no-op in the Core Animation drawing model.
    if (m_drawingModel == NPDrawingModelCoreAnimation)
        return true;

    return false;
}

void NetscapePlugin::platformGeometryDidChange()
{
}

static inline NPCocoaEvent initializeEvent(NPCocoaEventType type)
{
    NPCocoaEvent event;
    
    event.type = type;
    event.version = 0;
    
    return event;
}

#ifndef NP_NO_CARBON
NetscapePlugin* NetscapePlugin::netscapePluginFromWindow(WindowRef windowRef)
{
    return windowMap().get(windowRef);
}

WindowRef NetscapePlugin::windowRef() const
{
    ASSERT(m_eventModel == NPEventModelCarbon);

    return reinterpret_cast<WindowRef>(m_npCGContext.window);
}

unsigned NetscapePlugin::buttonState()
{
    return buttonStateFromLastMouseEvent;
}

static inline EventRecord initializeEventRecord(EventKind eventKind)
{
    EventRecord eventRecord;

    eventRecord.what = eventKind;
    eventRecord.message = 0;
    eventRecord.when = TickCount();
    eventRecord.where = Point();
    eventRecord.modifiers = 0;

    return eventRecord;
}

static bool anyMouseButtonIsDown(const WebEvent& event)
{
    if (event.type() == WebEvent::MouseDown)
        return true;

    if (event.type() == WebEvent::MouseMove && static_cast<const WebMouseEvent&>(event).button() != WebMouseEvent::NoButton)
        return true;

    return false;
}

static bool rightMouseButtonIsDown(const WebEvent& event)
{
    if (event.type() == WebEvent::MouseDown && static_cast<const WebMouseEvent&>(event).button() == WebMouseEvent::RightButton)
        return true;
    
    if (event.type() == WebEvent::MouseMove && static_cast<const WebMouseEvent&>(event).button() == WebMouseEvent::RightButton)
        return true;
    
    return false;
}

static EventModifiers modifiersForEvent(const WebEvent& event)
{
    EventModifiers modifiers = 0;

    // We only want to set the btnState if a mouse button is _not_ down.
    if (!anyMouseButtonIsDown(event))
        modifiers |= btnState;

    if (event.metaKey())
        modifiers |= cmdKey;

    if (event.shiftKey())
        modifiers |= shiftKey;

    if (event.altKey())
        modifiers |= optionKey;

    // Set controlKey if the control key is down or the right mouse button is down.
    if (event.controlKey() || rightMouseButtonIsDown(event))
        modifiers |= controlKey;

    return modifiers;
}

#endif

void NetscapePlugin::platformPaint(GraphicsContext* context, const IntRect& dirtyRect, bool isSnapshot)
{
    CGContextRef platformContext = context->platformContext();

    // Translate the context so that the origin is at the top left corner of the plug-in view.
    context->translate(m_frameRect.x(), m_frameRect.y());

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            // Don't send draw events when we're using the Core Animation drawing model.
            if (!isSnapshot && m_drawingModel == NPDrawingModelCoreAnimation)
                return;

            NPCocoaEvent event = initializeEvent(NPCocoaEventDrawRect);

            event.data.draw.context = platformContext;
            event.data.draw.x = dirtyRect.x() - m_frameRect.x();
            event.data.draw.y = dirtyRect.y() - m_frameRect.y();
            event.data.draw.width = dirtyRect.width();
            event.data.draw.height = dirtyRect.height();
            
            NPP_HandleEvent(&event);
            break;
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            if (platformContext != m_npCGContext.context) {
                m_npCGContext.context = platformContext;
                callSetWindow();
            }

            EventRecord event = initializeEventRecord(updateEvt);
            event.message = reinterpret_cast<unsigned long>(windowRef());
            
            NPP_HandleEvent(&event);
            break;            
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }
}

static uint32_t modifierFlags(const WebEvent& event)
{
    uint32_t modifiers = 0;

    if (event.shiftKey())
        modifiers |= NSShiftKeyMask;
    if (event.controlKey())
        modifiers |= NSControlKeyMask;
    if (event.altKey())
        modifiers |= NSAlternateKeyMask;
    if (event.metaKey())
        modifiers |= NSCommandKeyMask;
    
    return modifiers;
}
    
static int32_t buttonNumber(WebMouseEvent::Button button)
{
    switch (button) {
    case WebMouseEvent::NoButton:
    case WebMouseEvent::LeftButton:
        return 0;
    case WebMouseEvent::RightButton:
        return 1;
    case WebMouseEvent::MiddleButton:
        return 2;
    }

    ASSERT_NOT_REACHED();
    return -1;
}

static void fillInCocoaEventFromMouseEvent(NPCocoaEvent& event, const WebMouseEvent& mouseEvent, const WebCore::IntPoint& pluginLocation)
{
    event.data.mouse.modifierFlags = modifierFlags(mouseEvent);
    event.data.mouse.pluginX = mouseEvent.position().x() - pluginLocation.x();
    event.data.mouse.pluginY = mouseEvent.position().y() - pluginLocation.y();
    event.data.mouse.buttonNumber = buttonNumber(mouseEvent.button());
    event.data.mouse.clickCount = mouseEvent.clickCount();
    event.data.mouse.deltaX = mouseEvent.deltaX();
    event.data.mouse.deltaY = mouseEvent.deltaY();
    event.data.mouse.deltaZ = mouseEvent.deltaZ();
}
    
static NPCocoaEvent initializeMouseEvent(const WebMouseEvent& mouseEvent, const WebCore::IntPoint& pluginLocation)
{
    NPCocoaEventType eventType;

    switch (mouseEvent.type()) {
    case WebEvent::MouseDown:
        eventType = NPCocoaEventMouseDown;
        break;
    case WebEvent::MouseUp:
        eventType = NPCocoaEventMouseUp;
        break;
    case WebEvent::MouseMove:
        if (mouseEvent.button() == WebMouseEvent::NoButton)
            eventType = NPCocoaEventMouseMoved;
        else
            eventType = NPCocoaEventMouseDragged;
        break;
    default:
        ASSERT_NOT_REACHED();
        return NPCocoaEvent();
    }

    NPCocoaEvent event = initializeEvent(eventType);
    fillInCocoaEventFromMouseEvent(event, mouseEvent, pluginLocation);
    return event;
}

bool NetscapePlugin::platformHandleMouseEvent(const WebMouseEvent& mouseEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeMouseEvent(mouseEvent, m_frameRect.location());

            NPCocoaEvent* previousMouseEvent = m_currentMouseEvent;
            m_currentMouseEvent = &event;

            // Protect against NPP_HandleEvent causing the plug-in to be destroyed, since we
            // access m_currentMouseEvent afterwards.
            RefPtr<NetscapePlugin> protect(this);

            NPP_HandleEvent(&event);

            m_currentMouseEvent = previousMouseEvent;

            // Some plug-ins return false even if the mouse event has been handled.
            // This leads to bugs such as <rdar://problem/9167611>. Work around this
            // by always returning true.
            return true;
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventKind eventKind = nullEvent;

            switch (mouseEvent.type()) {
            case WebEvent::MouseDown:
                eventKind = mouseDown;
                buttonStateFromLastMouseEvent |= (1 << buttonNumber(mouseEvent.button()));
                break;
            case WebEvent::MouseUp:
                eventKind = mouseUp;
                buttonStateFromLastMouseEvent &= ~(1 << buttonNumber(mouseEvent.button()));
                break;
            case WebEvent::MouseMove:
                eventKind = nullEvent;
                break;
            default:
                ASSERT_NOT_REACHED();
            }

            EventRecord event = initializeEventRecord(eventKind);
            event.modifiers = modifiersForEvent(mouseEvent);
            event.where.h = mouseEvent.globalPosition().x();
            event.where.v = mouseEvent.globalPosition().y();

            NPP_HandleEvent(&event);

            // Some plug-ins return false even if the mouse event has been handled.
            // This leads to bugs such as <rdar://problem/9167611>. Work around this
            // by always returning true.
            return true;
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

bool NetscapePlugin::platformHandleWheelEvent(const WebWheelEvent& wheelEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventScrollWheel);
            
            event.data.mouse.modifierFlags = modifierFlags(wheelEvent);
            event.data.mouse.pluginX = wheelEvent.position().x() - m_frameRect.x();
            event.data.mouse.pluginY = wheelEvent.position().y() - m_frameRect.y();
            event.data.mouse.buttonNumber = 0;
            event.data.mouse.clickCount = 0;
            event.data.mouse.deltaX = wheelEvent.delta().width();
            event.data.mouse.deltaY = wheelEvent.delta().height();
            event.data.mouse.deltaZ = 0;
            return NPP_HandleEvent(&event);
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
            // Carbon doesn't have wheel events.
            break;
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

bool NetscapePlugin::platformHandleMouseEnterEvent(const WebMouseEvent& mouseEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventMouseEntered);
            
            fillInCocoaEventFromMouseEvent(event, mouseEvent, m_frameRect.location());
            return NPP_HandleEvent(&event);
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventRecord eventRecord = initializeEventRecord(NPEventType_AdjustCursorEvent);
            eventRecord.modifiers = modifiersForEvent(mouseEvent);
            
            return NPP_HandleEvent(&eventRecord);
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

bool NetscapePlugin::platformHandleMouseLeaveEvent(const WebMouseEvent& mouseEvent)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventMouseExited);
            
            fillInCocoaEventFromMouseEvent(event, mouseEvent, m_frameRect.location());
            return NPP_HandleEvent(&event);
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventRecord eventRecord = initializeEventRecord(NPEventType_AdjustCursorEvent);
            eventRecord.modifiers = modifiersForEvent(mouseEvent);
            
            return NPP_HandleEvent(&eventRecord);
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }

    return false;
}

static unsigned modifierFlags(const WebKeyboardEvent& keyboardEvent)
{
    unsigned modifierFlags = 0;

    if (keyboardEvent.capsLockKey())
        modifierFlags |= NSAlphaShiftKeyMask;
    if (keyboardEvent.shiftKey())
        modifierFlags |= NSShiftKeyMask;
    if (keyboardEvent.controlKey())
        modifierFlags |= NSControlKeyMask;
    if (keyboardEvent.altKey())
        modifierFlags |= NSAlternateKeyMask;
    if (keyboardEvent.metaKey())
        modifierFlags |= NSCommandKeyMask;

    return modifierFlags;
}

static bool isFlagsChangedEvent(const WebKeyboardEvent& keyboardEvent)
{
    switch (keyboardEvent.nativeVirtualKeyCode()) {
    case 54: // Right Command
    case 55: // Left Command

    case 57: // Capslock

    case 56: // Left Shift
    case 60: // Right Shift

    case 58: // Left Alt
    case 61: // Right Alt
            
    case 59: // Left Ctrl
    case 62: // Right Ctrl
        return true;
    }

    return false;
}

static NPCocoaEvent initializeKeyboardEvent(const WebKeyboardEvent& keyboardEvent)
{
    NPCocoaEventType eventType;

    if (isFlagsChangedEvent(keyboardEvent))
        eventType = NPCocoaEventFlagsChanged;
    else {
        switch (keyboardEvent.type()) {
            case WebEvent::KeyDown:
                eventType = NPCocoaEventKeyDown;
                break;
            case WebEvent::KeyUp:
                eventType = NPCocoaEventKeyUp;
                break;
            default:
                ASSERT_NOT_REACHED();
                return NPCocoaEvent();
        }
    }

    NPCocoaEvent event = initializeEvent(eventType);
    event.data.key.modifierFlags = modifierFlags(keyboardEvent);
    event.data.key.characters = reinterpret_cast<NPNSString*>(static_cast<NSString*>(keyboardEvent.text()));
    event.data.key.charactersIgnoringModifiers = reinterpret_cast<NPNSString*>(static_cast<NSString*>(keyboardEvent.unmodifiedText()));
    event.data.key.isARepeat = keyboardEvent.isAutoRepeat();
    event.data.key.keyCode = keyboardEvent.nativeVirtualKeyCode();

    return event;
}

bool NetscapePlugin::platformHandleKeyboardEvent(const WebKeyboardEvent& keyboardEvent)
{
    bool handled = false;

    switch (m_eventModel) {
    case NPEventModelCocoa: {
        NPCocoaEvent event = initializeKeyboardEvent(keyboardEvent);
        handled = NPP_HandleEvent(&event);
        break;
    }

#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
        EventKind eventKind = nullEvent;

        switch (keyboardEvent.type()) {
        case WebEvent::KeyDown:
            eventKind = keyboardEvent.isAutoRepeat() ? autoKey : keyDown;
            break;
        case WebEvent::KeyUp:
            eventKind = keyUp;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        EventRecord event = initializeEventRecord(eventKind);
        event.modifiers = modifiersForEvent(keyboardEvent);
        event.message = keyboardEvent.nativeVirtualKeyCode() << 8 | keyboardEvent.macCharCode();
        handled = NPP_HandleEvent(&event);
        break;
    }
#endif

    default:
        ASSERT_NOT_REACHED();
    }

    // Most plug-ins simply return true for all keyboard events, even those that aren't handled.
    // This leads to bugs such as <rdar://problem/8740926>. We work around this by returning false
    // if the keyboard event has the command modifier pressed.
    if (keyboardEvent.metaKey())
        return false;

    return handled;
}

void NetscapePlugin::platformSetFocus(bool hasFocus)
{
    m_pluginHasFocus = hasFocus;
    m_pluginController->setComplexTextInputEnabled(m_pluginHasFocus && m_windowHasFocus);

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventFocusChanged);
            
            event.data.focus.hasFocus = hasFocus;
            NPP_HandleEvent(&event);
            break;
        }

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            EventRecord event = initializeEventRecord(hasFocus ? NPEventType_GetFocusEvent : NPEventType_LoseFocusEvent);

            NPP_HandleEvent(&event);
            break;
        }
#endif
            
        default:
            ASSERT_NOT_REACHED();
    }
}

void NetscapePlugin::windowFocusChanged(bool hasFocus)
{
    m_windowHasFocus = hasFocus;
    m_pluginController->setComplexTextInputEnabled(m_pluginHasFocus && m_windowHasFocus);

    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventWindowFocusChanged);
            
            event.data.focus.hasFocus = hasFocus;
            NPP_HandleEvent(&event);
            break;
        }
        
#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            HiliteWindow(windowRef(), hasFocus);
            if (hasFocus)
                SetUserFocusWindow(windowRef());

            EventRecord event = initializeEventRecord(activateEvt);
            event.message = reinterpret_cast<unsigned long>(windowRef());
            if (hasFocus)
                event.modifiers |= activeFlag;
            
            NPP_HandleEvent(&event);
            break;
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }
}

#ifndef NP_NO_CARBON
static Rect computeFakeWindowBoundsRect(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates)
{
    // Carbon global coordinates has the origin set at the top left corner of the main viewing screen, so we want to flip the y coordinate.
    CGFloat maxY = NSMaxY([(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame]);

    int flippedWindowFrameYCoordinate = maxY - windowFrameInScreenCoordinates.maxY();
    int flippedViewFrameYCoordinate = windowFrameInScreenCoordinates.height() - viewFrameInWindowCoordinates.maxY();

    Rect bounds;
    
    bounds.top = flippedWindowFrameYCoordinate + flippedViewFrameYCoordinate;
    bounds.left = windowFrameInScreenCoordinates.x();
    bounds.right = bounds.left + viewFrameInWindowCoordinates.width();
    bounds.bottom = bounds.top + viewFrameInWindowCoordinates.height();
    
    return bounds;
}
#endif

void NetscapePlugin::windowAndViewFramesChanged(const IntRect& windowFrameInScreenCoordinates, const IntRect& viewFrameInWindowCoordinates)
{
    m_windowFrameInScreenCoordinates = windowFrameInScreenCoordinates;
    m_viewFrameInWindowCoordinates = viewFrameInWindowCoordinates;

    switch (m_eventModel) {
        case NPEventModelCocoa:
            // Nothing to do.
            break;

#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
            Rect bounds = computeFakeWindowBoundsRect(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates);

            ::SetWindowBounds(windowRef(), kWindowStructureRgn, &bounds);
            break;
        }
#endif

        default:
            ASSERT_NOT_REACHED();
    }
}
    
void NetscapePlugin::windowVisibilityChanged(bool)
{
    // FIXME: Implement.
}

uint64_t NetscapePlugin::pluginComplexTextInputIdentifier() const
{
    // Just return a dummy value; this is only called for in-process plug-ins, which we don't support on Mac.
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
}


#ifndef NP_NO_CARBON
static bool convertStringToKeyCodes(const String& string, ScriptCode scriptCode, Vector<UInt8>& keyCodes)
{
    // Create the mapping.
    UnicodeMapping mapping;

    if (GetTextEncodingFromScriptInfo(scriptCode, kTextLanguageDontCare, kTextRegionDontCare, &mapping.otherEncoding) != noErr)
        return false;

    mapping.unicodeEncoding = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kTextEncodingDefaultFormat);
    mapping.mappingVersion = kUnicodeUseLatestMapping;
    
    // Create the converter
    UnicodeToTextInfo textInfo;
    
    if (CreateUnicodeToTextInfo(&mapping, &textInfo) != noErr)
        return false;
    
    ByteCount inputLength = string.length() * sizeof(UniChar);
    ByteCount inputRead;
    ByteCount outputLength;
    ByteCount maxOutputLength = string.length() * sizeof(UniChar);

    Vector<UInt8> outputData(maxOutputLength);
    OSStatus status = ConvertFromUnicodeToText(textInfo, inputLength, string.characters(), kNilOptions, 0, 0, 0, 0, maxOutputLength, &inputRead, &outputLength, outputData.data());
    
    DisposeUnicodeToTextInfo(&textInfo);
    
    if (status != noErr)
        return false;

    outputData.swap(keyCodes);
    return true;
}
#endif

void NetscapePlugin::sendComplexTextInput(const String& textInput)
{
    switch (m_eventModel) {
    case NPEventModelCocoa: {
        NPCocoaEvent event = initializeEvent(NPCocoaEventTextInput);
        event.data.text.text = reinterpret_cast<NPNSString*>(static_cast<NSString*>(textInput));
        NPP_HandleEvent(&event);
        break;
    }
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
        ScriptCode scriptCode = WKGetScriptCodeFromCurrentKeyboardInputSource();
        Vector<UInt8> keyCodes;

        if (!convertStringToKeyCodes(textInput, scriptCode, keyCodes))
            return;

        // Set the script code as the keyboard script. Normally Carbon does this whenever the input source changes.
        // However, this is only done for the process that has the keyboard focus. We cheat and do it here instead.
        SetScriptManagerVariable(smKeyScript, scriptCode);
        
        EventRecord event = initializeEventRecord(keyDown);
        event.modifiers = 0;

        for (size_t i = 0; i < keyCodes.size(); i++) {
            event.message = keyCodes[i];
            NPP_HandleEvent(&event);
        }
        break;
    }
#endif
    default:
        ASSERT_NOT_REACHED();
    }
}

PlatformLayer* NetscapePlugin::pluginLayer()
{
    return static_cast<PlatformLayer*>(m_pluginLayer.get());
}

#ifndef NP_NO_CARBON
void NetscapePlugin::nullEventTimerFired()
{
    EventRecord event = initializeEventRecord(nullEvent);

    event.message = 0;
    CGPoint mousePosition;
    HIGetMousePosition(kHICoordSpaceScreenPixel, 0, &mousePosition);
    event.where.h = mousePosition.x;
    event.where.v = mousePosition.y;

    event.modifiers = GetCurrentKeyModifiers();
    if (!Button())
        event.modifiers |= btnState;

    NPP_HandleEvent(&event);
}
#endif

} // namespace WebKit
