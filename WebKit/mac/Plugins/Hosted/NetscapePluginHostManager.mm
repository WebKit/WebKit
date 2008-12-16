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

#import "NetscapePluginHostManager.h"

#import "NetscapePluginHostProxy.h"
#import "NetscapePluginInstanceProxy.h"
#import "WebKitSystemInterface.h"
#import "WebNetscapePluginPackage.h"
#import <mach/mach_port.h>
#import <servers/bootstrap.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

extern "C" {
#import "WebKitPluginAgent.h"
#import "WebKitPluginHost.h"
}

using namespace std;

namespace WebKit {

NetscapePluginHostManager& NetscapePluginHostManager::shared()
{
    DEFINE_STATIC_LOCAL(NetscapePluginHostManager, pluginHostManager, ());
    
    return pluginHostManager;
}

static const NSString *pluginHostAppName = @"WebKitPluginHost.app";

NetscapePluginHostManager::NetscapePluginHostManager()
    : m_pluginVendorPort(MACH_PORT_NULL)
{
}
 
NetscapePluginHostManager::~NetscapePluginHostManager()
{
}

NetscapePluginHostProxy* NetscapePluginHostManager::hostForPackage(WebNetscapePluginPackage *package)
{
    pair<PluginHostMap::iterator, bool> result = m_pluginHosts.add(package, 0);
    
    // The package was already in the map, just return it.
    if (!result.second)
        return result.first->second;
        
    mach_port_t clientPort;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort) != KERN_SUCCESS)
        return 0;
    
    mach_port_t pluginHostPort;
    if (!spawnPluginHost(package, clientPort, pluginHostPort)) {
        mach_port_destroy(mach_task_self(), clientPort);
        return 0;
    }
    
    NetscapePluginHostProxy* hostProxy = new NetscapePluginHostProxy(clientPort, pluginHostPort);
    
    CFRetain(package);
    result.first->second = hostProxy;
    
    return hostProxy;
}

bool NetscapePluginHostManager::spawnPluginHost(WebNetscapePluginPackage *package, mach_port_t clientPort, mach_port_t& pluginHostPort)
{
    if (m_pluginVendorPort == MACH_PORT_NULL) {
        if (!initializeVendorPort())
            return false;
    }

    mach_port_t renderServerPort = WKInitializeRenderServer();
    if (renderServerPort == MACH_PORT_NULL)
        return false;

    NSString *pluginHostAppPath = [[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] pathForAuxiliaryExecutable:pluginHostAppName];
    NSString *pluginHostAppExecutablePath = [[NSBundle bundleWithPath:pluginHostAppPath] executablePath];

    NSDictionary *launchProperties = [[NSDictionary alloc] initWithObjectsAndKeys:
                                      pluginHostAppExecutablePath, @"pluginHostPath",
                                      [NSNumber numberWithInt:[package pluginHostArchitecture]], @"cpuType",
                                      nil];

    NSData *data = [NSPropertyListSerialization dataFromPropertyList:launchProperties format:NSPropertyListBinaryFormat_v1_0 errorDescription:0];
    ASSERT(data);

    [launchProperties release];

    kern_return_t kr = _WKPASpawnPluginHost(m_pluginVendorPort, reinterpret_cast<uint8_t*>(const_cast<void*>([data bytes])), [data length], &pluginHostPort);

    if (kr != KERN_SUCCESS) {
        // FIXME: Check for invalid dest and try to re-spawn the plug-in agent.
        LOG_ERROR("Failed to spawn plug-in host, error %x", kr);
        return false;
    }
    
    NSDictionary *hostProperties = [[NSDictionary alloc] initWithObjectsAndKeys:
                                    [package path], @"bundlePath",
                                    nil];
    
    data = [NSPropertyListSerialization dataFromPropertyList:hostProperties format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
    ASSERT(data);

    [hostProperties release];
    
    kr = _WKPHCheckInWithPluginHost(pluginHostPort, (uint8_t*)[data bytes], [data length], clientPort, renderServerPort);
    
    if (kr != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), pluginHostPort);
        LOG_ERROR("Failed to check in with plug-in host, error %x", kr);

        return false;
    }

    return true;
}

bool NetscapePluginHostManager::initializeVendorPort()
{
    ASSERT(m_pluginVendorPort == MACH_PORT_NULL);

    // Get the plug-in agent port.
    mach_port_t pluginAgentPort;
    if (bootstrap_look_up(bootstrap_port, "com.apple.WebKit.PluginAgent", &pluginAgentPort) != KERN_SUCCESS) {
        LOG_ERROR("Failed to look up the plug-in agent port");
        return false;
    }
    
    NSData *appNameData = [[[NSProcessInfo processInfo] processName] dataUsingEncoding:NSUTF8StringEncoding];
    
    // Tell the plug-in agent that we exist.
    if (_WKPACheckInApplication(pluginAgentPort, (uint8_t*)[appNameData bytes], [appNameData length], &m_pluginVendorPort) != KERN_SUCCESS)
        return false;

    // FIXME: Should we add a notification for when the vendor port dies?
    
    return true;
}

void NetscapePluginHostManager::pluginHostDied(NetscapePluginHostProxy* pluginHost)
{
    PluginHostMap::iterator end = m_pluginHosts.end();

    // This has O(n) complexity but the number of active plug-in hosts is very small so it shouldn't matter.
    for (PluginHostMap::iterator it = m_pluginHosts.begin(); it != end; ++it) {
        if (it->second == pluginHost) {
            m_pluginHosts.remove(it);
            return;
        }
    }
}

PassRefPtr<NetscapePluginInstanceProxy> NetscapePluginHostManager::instantiatePlugin(WebNetscapePluginPackage *pluginPackage, WebHostedNetscapePluginView *pluginView, NSString *mimeType, NSArray *attributeKeys, NSArray *attributeValues, NSString *userAgent, NSURL *sourceURL)
{
    NetscapePluginHostProxy* hostProxy = hostForPackage(pluginPackage);
    if (!hostProxy)
        return 0;

    RetainPtr<NSMutableDictionary> properties(AdoptNS, [[NSMutableDictionary alloc] init]);
    
    if (mimeType)
        [properties.get() setObject:mimeType forKey:@"mimeType"];

    ASSERT_ARG(userAgent, userAgent);
    [properties.get() setObject:userAgent forKey:@"userAgent"];
    
    ASSERT_ARG(attributeKeys, attributeKeys);
    [properties.get() setObject:attributeKeys forKey:@"attributeKeys"];
    
    ASSERT_ARG(attributeValues, attributeValues);
    [properties.get() setObject:attributeValues forKey:@"attributeValues"];

    if (sourceURL)
        [properties.get() setObject:[sourceURL absoluteString] forKey:@"sourceURL"];
    
    NSData *data = [NSPropertyListSerialization dataFromPropertyList:properties.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
    ASSERT(data);
    
    RefPtr<NetscapePluginInstanceProxy> instance = NetscapePluginInstanceProxy::create(hostProxy, pluginView);
    kern_return_t kr = _WKPHInstantiatePlugin(hostProxy->port(), (uint8_t*)[data bytes], [data length], instance->pluginID());
    if (kr == MACH_SEND_INVALID_DEST) {
        // The plug-in host must have died, but we haven't received the death notification yet.
        pluginHostDied(hostProxy);

        // Try to spawn it again.
        hostProxy = hostForPackage(pluginPackage);
        
        kr = _WKPHInstantiatePlugin(hostProxy->port(), (uint8_t*)[data bytes], [data length], instance->pluginID());
    }

    auto_ptr<NetscapePluginInstanceProxy::InstantiatePluginReply> reply = instance->waitForReply<NetscapePluginInstanceProxy::InstantiatePluginReply>();
    if (!reply.get())
        return 0;
    
    instance->setRenderContextID(reply->m_renderContextID);
    instance->setUseSoftwareRenderer(reply->m_useSoftwareRenderer);

    return instance.release();
}
    
} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
