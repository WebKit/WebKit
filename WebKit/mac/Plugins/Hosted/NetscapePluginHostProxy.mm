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

#import "NetscapePluginHostProxy.h"

#import <mach/mach.h>
#import "NetscapePluginHostManager.h"
#import "NetscapePluginInstanceProxy.h"
#import <wtf/StdLibExtras.h>

extern "C" {
#import "WebKitPluginHost.h"
#import "WebKitPluginClientServer.h"
}

using namespace std;

namespace WebKit {

typedef HashMap<mach_port_t, NetscapePluginHostProxy*> PluginProxyMap;
static PluginProxyMap& pluginProxyMap()
{
    DEFINE_STATIC_LOCAL(PluginProxyMap, pluginProxyMap, ());
    
    return pluginProxyMap;
}

NetscapePluginHostProxy::NetscapePluginHostProxy(mach_port_t clientPort, mach_port_t pluginHostPort)
    : m_clientPort(clientPort)
    , m_pluginHostPort(pluginHostPort)
{
    pluginProxyMap().add(m_clientPort, this);
    
    // FIXME: We should use libdispatch for this.
    CFMachPortContext context = { 0, this, 0, 0, 0 };
    m_deadNameNotificationPort.adoptCF(CFMachPortCreate(0, deadNameNotificationCallback, &context, 0));

    mach_port_t previous;
    mach_port_request_notification(mach_task_self(), pluginHostPort, MACH_NOTIFY_DEAD_NAME, 0, 
                                   CFMachPortGetPort(m_deadNameNotificationPort.get()), MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    ASSERT(previous == MACH_PORT_NULL);
    
    RetainPtr<CFRunLoopSourceRef> deathPortSource(AdoptCF, CFMachPortCreateRunLoopSource(0, m_deadNameNotificationPort.get(), 0));
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), deathPortSource.get(), kCFRunLoopDefaultMode);
    
    m_clientPortSource = dispatch_source_mig_create(m_clientPort, WKPCWebKitPluginClient_subsystem.maxsize, 0, 
                                                    dispatch_get_main_queue(), WebKitPluginClient_server);
}

NetscapePluginHostProxy::~NetscapePluginHostProxy()
{
    pluginProxyMap().remove(m_clientPort);
    
    ASSERT(m_clientPortSource);
    dispatch_source_release(m_clientPortSource);
}

void NetscapePluginHostProxy::pluginHostDied()
{
    PluginInstanceMap instances;    
    m_instances.swap(instances);
  
    PluginInstanceMap::const_iterator end = instances.end();
    for (PluginInstanceMap::const_iterator it = instances.begin(); it != end; ++it)
        it->second->pluginHostDied();
    
    NetscapePluginHostManager::shared().pluginHostDied(this);
    
    delete this;
}
    
void NetscapePluginHostProxy::addPluginInstance(NetscapePluginInstanceProxy* instance)
{
    ASSERT(!m_instances.contains(instance->pluginID()));
    
    m_instances.set(instance->pluginID(), instance);
}
    
void NetscapePluginHostProxy::removePluginInstance(NetscapePluginInstanceProxy* instance)
{
    ASSERT(m_instances.get(instance->pluginID()) == instance);

    m_instances.remove(instance->pluginID());
}

NetscapePluginInstanceProxy* NetscapePluginHostProxy::pluginInstance(uint32_t pluginID)
{
    return m_instances.get(pluginID).get();
}

void NetscapePluginHostProxy::deadNameNotificationCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    ASSERT(msg && static_cast<mach_msg_header_t*>(msg)->msgh_id == MACH_NOTIFY_DEAD_NAME);
    
    static_cast<NetscapePluginHostProxy*>(info)->pluginHostDied();
}

} // namespace WebKit

using namespace WebKit;

// MiG callbacks
kern_return_t WKPCStatusText(mach_port_t clientPort, uint32_t pluginID, data_t text, mach_msg_type_number_t textCnt)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;
    
    instanceProxy->status(text);
    return KERN_SUCCESS;
}

kern_return_t WKPCLoadURL(mach_port_t clientPort, uint32_t pluginID, data_t url, mach_msg_type_number_t urlLength, data_t target, mach_msg_type_number_t targetLength, 
                          boolean_t post, data_t postData, mach_msg_type_number_t postDataLength, boolean_t postDataIsFile, boolean_t currentEventIsUserGesture)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;

    uint32_t streamID;
    NPError result = instanceProxy->loadURL(url, target, post, postData, postDataLength, postDataIsFile, streamID, currentEventIsUserGesture);
    
    return _WKPHLoadURLReply(hostProxy->port(), pluginID, result, streamID);
}

#endif // USE(PLUGIN_HOST_PROCESS)
