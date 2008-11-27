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
        return result.first->second.get();
        
    mach_port_t pluginHostPort;
    if (!spawnPluginHost(package, pluginHostPort))
        return false;
    
    RefPtr<NetscapePluginHostProxy> hostProxy = NetscapePluginHostProxy::create(pluginHostPort);
    
    CFRetain(package);
    result.first->second = hostProxy.release();
    
    return result.first->second.get();
}

bool NetscapePluginHostManager::spawnPluginHost(WebNetscapePluginPackage *package, mach_port_t& pluginHostPort)
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

    kern_return_t kr = _WKPASpawnPluginHost(m_pluginVendorPort, (uint8_t*)[data bytes], [data length], &pluginHostPort);

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
    
    kr = _WKPHCheckInWithPluginHost(pluginHostPort, (uint8_t*)[data bytes], [data length], renderServerPort);
    
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

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
