/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "WebNetscapePluginEventHandlerCocoa.h"

#import "WebBaseNetscapePluginViewInternal.h"

WebNetscapePluginEventHandlerCocoa::WebNetscapePluginEventHandlerCocoa(WebBaseNetscapePluginView* pluginView)
    : WebNetscapePluginEventHandler(pluginView)
{
}

void WebNetscapePluginEventHandlerCocoa::drawRect(const NSRect& rect)
{
    NPCocoaEvent event;
    
    event.type = NPCocoaEventDrawRect;
    event.draw.x = rect.origin.x;
    event.draw.y = rect.origin.y;
    event.draw.width = rect.size.width;
    event.draw.height = rect.size.height;
    
    sendEvent(&event);
}

void WebNetscapePluginEventHandlerCocoa::mouseDown(NSEvent *event)
{
    sendMouseEvent(event, NPCocoaEventMouseDown);
}

void WebNetscapePluginEventHandlerCocoa::mouseDragged(NSEvent *event)
{
    sendMouseEvent(event, NPCocoaEventMouseDragged);
}

void WebNetscapePluginEventHandlerCocoa::mouseEntered(NSEvent *event)
{
    sendMouseEvent(event, NPCocoaEventMouseEntered);
}

void WebNetscapePluginEventHandlerCocoa::mouseExited(NSEvent *event)
{
    sendMouseEvent(event, NPCocoaEventMouseExited);
}

void WebNetscapePluginEventHandlerCocoa::mouseMoved(NSEvent *event)
{
    sendMouseEvent(event, NPCocoaEventMouseMoved);
}

void WebNetscapePluginEventHandlerCocoa::mouseUp(NSEvent *event)
{
    sendMouseEvent(event, NPCocoaEventMouseUp);
}

bool WebNetscapePluginEventHandlerCocoa::scrollWheel(NSEvent* event)
{
    return sendMouseEvent(event, NPCocoaEventScrollWheel);
}

bool WebNetscapePluginEventHandlerCocoa::sendMouseEvent(NSEvent *nsEvent, NPCocoaEventType type)
{
    NPCocoaEvent event;
    
    NSPoint point = [m_pluginView convertPoint:[nsEvent locationInWindow] fromView:nil];
    
    event.type = type;
    event.mouse.modifierFlags = [nsEvent modifierFlags];
    event.mouse.buttonNumber = [nsEvent buttonNumber];
    event.mouse.pluginX = point.x;
    event.mouse.pluginY = point.y;
    event.mouse.deltaX = [nsEvent deltaX];
    event.mouse.deltaY = [nsEvent deltaY];
    event.mouse.deltaZ = [nsEvent deltaZ];
    
    return sendEvent(&event);
}

void WebNetscapePluginEventHandlerCocoa::keyDown(NSEvent *event)
{
    sendKeyEvent(event, NPCocoaEventKeyDown);
}

void WebNetscapePluginEventHandlerCocoa::keyUp(NSEvent *event)
{
    sendKeyEvent(event, NPCocoaEventKeyUp);
}

void WebNetscapePluginEventHandlerCocoa::flagsChanged(NSEvent *nsEvent)
{
    NPCocoaEvent event;
    
    NSPoint point = [m_pluginView convertPoint:[nsEvent locationInWindow] fromView:nil];
    
    event.type = NPCocoaEventFlagsChanged;
    event.key.modifierFlags = [nsEvent modifierFlags];
    event.key.pluginX = point.x;
    event.key.pluginY = point.y;
    event.key.keyCode = [nsEvent keyCode];
    event.key.isARepeat = false;
    event.key.characters = 0;
    event.key.charactersIgnoringModifiers = 0;
    
    sendEvent(&event);
}

void WebNetscapePluginEventHandlerCocoa::sendKeyEvent(NSEvent* nsEvent, NPCocoaEventType type)
{
    NPCocoaEvent event;
    
    NSPoint point = [m_pluginView convertPoint:[nsEvent locationInWindow] fromView:nil];
    
    event.type = type;
    event.key.modifierFlags = [nsEvent modifierFlags];
    event.key.pluginX = point.x;
    event.key.pluginY = point.y;
    event.key.keyCode = [nsEvent keyCode];
    event.key.isARepeat = [nsEvent isARepeat];
    event.key.characters = (NPNSString *)[nsEvent characters];
    event.key.charactersIgnoringModifiers = (NPNSString *)[nsEvent charactersIgnoringModifiers];
     
    sendEvent(&event);
}

void WebNetscapePluginEventHandlerCocoa::windowFocusChanged(bool hasFocus)
{
    NPCocoaEvent event;
    
    event.type = NPCocoaEventWindowFocusChanged;
    event.focus.hasFocus = hasFocus;
    
    sendEvent(&event);
}

void WebNetscapePluginEventHandlerCocoa::focusChanged(bool hasFocus)
{
    NPCocoaEvent event;
    
    event.type = NPCocoaEventFocusChanged;
    event.focus.hasFocus = hasFocus;
    
    sendEvent(&event);
}

void* WebNetscapePluginEventHandlerCocoa::platformWindow(NSWindow* window)
{
    return window;
}

bool WebNetscapePluginEventHandlerCocoa::sendEvent(NPCocoaEvent* event)
{
    switch (event->type) {
        case NPCocoaEventMouseDown:
        case NPCocoaEventMouseUp:
        case NPCocoaEventMouseDragged:
        case NPCocoaEventKeyDown:
        case NPCocoaEventKeyUp:
        case NPCocoaEventFlagsChanged:
        case NPCocoaEventScrollWheel:
            m_currentEventIsUserGesture = true;
            break;
        default:
            m_currentEventIsUserGesture = false;
    }
            
    bool result = [m_pluginView sendEvent:event isDrawRect:event->type == NPCocoaEventDrawRect];
    
    m_currentEventIsUserGesture = false;
    return result;
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)
