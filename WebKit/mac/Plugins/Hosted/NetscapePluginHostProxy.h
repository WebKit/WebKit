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

#ifndef NetscapePluginHostProxy_h
#define NetscapePluginHostProxy_h

#include <dispatch/dispatch.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/PassRefPtr.h>

@class WebPlaceholderModalWindow;

namespace WebKit {
    
class NetscapePluginInstanceProxy;

class NetscapePluginHostProxy {
public:
    NetscapePluginHostProxy(mach_port_t clientPort, mach_port_t pluginHostPort, const ProcessSerialNumber& pluginHostPSN, bool shouldCacheMissingPropertiesAndMethods);
    
    mach_port_t port() const { return m_pluginHostPort; }
    mach_port_t clientPort() const { return m_clientPort; }

    void addPluginInstance(NetscapePluginInstanceProxy*);
    void removePluginInstance(NetscapePluginInstanceProxy*);

    NetscapePluginInstanceProxy* pluginInstance(uint32_t pluginID);

    bool isMenuBarVisible() const { return m_menuBarIsVisible; }
    void setMenuBarVisible(bool);

    bool isFullScreenWindowShowing() const { return m_fullScreenWindowIsShowing; }
    void setFullScreenWindowIsShowing(bool);

    void setModal(bool);

    void applicationDidBecomeActive();
    
    bool processRequests();
    bool isProcessingRequests() const { return m_processingRequests; }
    
    bool shouldCacheMissingPropertiesAndMethods() const { return m_shouldCacheMissingPropertiesAndMethods; }
    
private:
    ~NetscapePluginHostProxy();
    void pluginHostDied();

    void beginModal();
    void endModal();

    void didEnterFullScreen() const;
    void didExitFullScreen() const;

    static void deadNameNotificationCallback(CFMachPortRef, void *msg, CFIndex size, void *info);

    typedef HashMap<uint32_t, RefPtr<NetscapePluginInstanceProxy> > PluginInstanceMap;
    PluginInstanceMap m_instances;
    
    mach_port_t m_clientPort;
    mach_port_t m_portSet;
    
#ifdef USE_LIBDISPATCH
    dispatch_source_t m_clientPortSource;
#else
    RetainPtr<CFRunLoopSourceRef> m_clientPortSource;
#endif
    mach_port_t m_pluginHostPort;
    RetainPtr<CFMachPortRef> m_deadNameNotificationPort;
    
    RetainPtr<id> m_activationObserver;
    RetainPtr<WebPlaceholderModalWindow *> m_placeholderWindow;
    unsigned m_isModal;
    bool m_menuBarIsVisible;
    bool m_fullScreenWindowIsShowing;
    const ProcessSerialNumber m_pluginHostPSN;

    unsigned m_processingRequests;

    bool m_shouldCacheMissingPropertiesAndMethods;
};
    
} // namespace WebKit

#endif // NetscapePluginHostProxy_h
#endif // USE(PLUGIN_HOST_PROCESS)
