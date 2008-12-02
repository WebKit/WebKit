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

#if USE(PLUGIN_HOST_PROCESS)

#import "NetscapePluginInstanceProxy.h"

#import "NetscapePluginHostProxy.h"
#import "WebHostedNetscapePluginView.h"
#import "WebKitPluginHost.h"

namespace WebKit {

NetscapePluginInstanceProxy::NetscapePluginInstanceProxy(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView, uint32_t pluginID, uint32_t renderContextID, boolean_t useSoftwareRenderer)
    : m_pluginHostProxy(pluginHostProxy)
    , m_pluginView(pluginView)
    , m_pluginID(pluginID)
    , m_renderContextID(renderContextID)
    , m_useSoftwareRenderer(useSoftwareRenderer)
{
    ASSERT(m_pluginView);
    
    pluginHostProxy->addPluginInstance(this);
}

NetscapePluginInstanceProxy::~NetscapePluginInstanceProxy()
{
    ASSERT(!m_pluginHostProxy);
}

void NetscapePluginInstanceProxy::resize(NSRect size, NSRect clipRect)
{
    _WKPHResizePluginInstance(m_pluginHostProxy->port(), m_pluginID, size.origin.x, size.origin.y, size.size.width, size.size.height);
}

void NetscapePluginInstanceProxy::destroy()
{
    _WKPHDestroyPluginInstance(m_pluginHostProxy->port(), m_pluginID);
    
    m_pluginHostProxy->removePluginInstance(this);
    m_pluginHostProxy = 0;
}

void NetscapePluginInstanceProxy::pluginHostDied()
{
    m_pluginHostProxy = 0;
    
    [m_pluginView pluginHostDied];
    m_pluginView = nil;
}

void NetscapePluginInstanceProxy::focusChanged(bool hasFocus)
{
    _WKPHPluginInstanceFocusChanged(m_pluginHostProxy->port(), m_pluginID, hasFocus);
}

void NetscapePluginInstanceProxy::windowFocusChanged(bool hasFocus)
{
    _WKPHPluginInstanceWindowFocusChanged(m_pluginHostProxy->port(), m_pluginID, hasFocus);
}

void NetscapePluginInstanceProxy::windowFrameChanged(NSRect frame)
{
    _WKPHPluginInstanceWindowFrameChanged(m_pluginHostProxy->port(), m_pluginID, frame.origin.x, frame.origin.y, frame.size.width, frame.size.height,
                                          // FIXME: Is it always correct to pass the rect of the first screen here?
                                          NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]));
}
    
void NetscapePluginInstanceProxy::startTimers(bool throttleTimers)
{
    _WKPHPluginInstanceStartTimers(m_pluginHostProxy->port(), m_pluginID, throttleTimers);
}
    
void NetscapePluginInstanceProxy::mouseEvent(NSView *pluginView, NSEvent *event, NPCocoaEventType type)
{
    NSPoint screenPoint = [[event window] convertBaseToScreen:[event locationInWindow]];
    NSPoint pluginPoint = [pluginView convertPoint:[event locationInWindow] fromView:nil];
    
    int clickCount;
    if (type == NPCocoaEventMouseEntered || type == NPCocoaEventMouseExited)
        clickCount = 0;
    else
        clickCount = [event clickCount];
    

    _WKPHPluginInstanceMouseEvent(m_pluginHostProxy->port(), m_pluginID,
                                  [event timestamp],
                                  type, [event modifierFlags],
                                  pluginPoint.x, pluginPoint.y,
                                  screenPoint.x, screenPoint.y,
                                  // FIXME: Is it always correct to pass the rect of the first screen here?
                                  NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]),
                                  [event buttonNumber], clickCount, 
                                  [event deltaX], [event deltaY], [event deltaZ]);
}
    

void NetscapePluginInstanceProxy::stopTimers()
{
    _WKPHPluginInstanceStopTimers(m_pluginHostProxy->port(), m_pluginID);
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
