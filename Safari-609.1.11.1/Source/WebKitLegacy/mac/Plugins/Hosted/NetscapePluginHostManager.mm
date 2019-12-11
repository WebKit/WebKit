/*
 * Copyright (C) 2008-2018 Apple Inc. All Rights Reserved.
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

#if USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)

#import "NetscapePluginHostManager.h"

#import "NetscapePluginHostProxy.h"
#import "NetscapePluginInstanceProxy.h"
#import "WebLocalizableStringsInternal.h"
#import "WebNetscapePluginPackage.h"
#import <mach/mach_port.h>
#import <pal/spi/cf/CFLocaleSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/ServersSPI.h>
#import <spawn.h>
#import <wtf/Assertions.h>
#import <wtf/MachSendRight.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

extern "C" {
#import "WebKitPluginAgent.h"
#import "WebKitPluginHost.h"
}

using namespace WebCore;

namespace WebKit {

NetscapePluginHostManager& NetscapePluginHostManager::singleton()
{
    static NeverDestroyed<NetscapePluginHostManager> pluginHostManager;
    return pluginHostManager;
}

static NSString * const pluginHostAppName = @"WebKitPluginHost.app";

NetscapePluginHostManager::NetscapePluginHostManager()
    : m_pluginVendorPort(MACH_PORT_NULL)
{
}
 
NetscapePluginHostManager::~NetscapePluginHostManager()
{
}

NetscapePluginHostProxy* NetscapePluginHostManager::hostForPlugin(const WTF::String& pluginPath, cpu_type_t pluginArchitecture, const String& bundleIdentifier)
{
    PluginHostMap::AddResult result = m_pluginHosts.add(pluginPath, nullptr);
    
    // The package was already in the map, just return it.
    if (!result.isNewEntry)
        return result.iterator->value;
        
    mach_port_t clientPort = MACH_PORT_NULL;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort) != KERN_SUCCESS) {
        m_pluginHosts.remove(result.iterator);
        return nullptr;
    }
    
    mach_port_t pluginHostPort = MACH_PORT_NULL;
    ProcessSerialNumber pluginHostPSN;
    if (!spawnPluginHost(pluginPath, pluginArchitecture, clientPort, pluginHostPort, pluginHostPSN)) {
        mach_port_mod_refs(mach_task_self(), clientPort, MACH_PORT_RIGHT_RECEIVE, -1);
        m_pluginHosts.remove(result.iterator);
        return nullptr;
    }
    
    // Since Flash NPObjects add methods dynamically, we don't want to cache when a property/method doesn't exist
    // on an object because it could be added later.
    bool shouldCacheMissingPropertiesAndMethods = bundleIdentifier != "com.macromedia.Flash Player.plugin";
    
    NetscapePluginHostProxy* hostProxy = new NetscapePluginHostProxy(clientPort, pluginHostPort, pluginHostPSN, shouldCacheMissingPropertiesAndMethods);
    
    result.iterator->value = hostProxy;
    
    return hostProxy;
}

static NSString *preferredBundleLocalizationName()
{
    // FIXME: Any use of this function to pass localizations to another
    // process is likely not completely right, since it only considers
    // one localization.
    NSArray *preferredLocalizations = [[NSBundle mainBundle] preferredLocalizations];
    if (!preferredLocalizations || ![preferredLocalizations count])
        return @"en_US";

    NSString *language = [preferredLocalizations objectAtIndex:0];

    // FIXME: <rdar://problem/18083880> Replace use of Script Manager
    // to canonicalize locales with a custom Web-specific table
    LangCode languageCode;
    RegionCode regionCode;

    Boolean success = CFLocaleGetLanguageRegionEncodingForLocaleIdentifier((__bridge CFStringRef)language, &languageCode, &regionCode, nullptr, nullptr);
    if (!success)
        return @"en_US";

    return CFBridgingRelease(CFLocaleCreateCanonicalLocaleIdentifierFromScriptManagerCodes(0, languageCode, regionCode));
}

bool NetscapePluginHostManager::spawnPluginHost(const String& pluginPath, cpu_type_t pluginArchitecture, mach_port_t clientPort, mach_port_t& pluginHostPort, ProcessSerialNumber& pluginHostPSN)
{
    if (m_pluginVendorPort == MACH_PORT_NULL) {
        if (!initializeVendorPort())
            return false;
    }

    if (!CARenderServerStart())
        return MACH_PORT_NULL;

    mach_port_t renderServerPort = CARenderServerGetPort();
    if (renderServerPort == MACH_PORT_NULL)
        return false;

    NSString *pluginHostAppPath = [[NSBundle bundleForClass:[WebNetscapePluginPackage class]] pathForAuxiliaryExecutable:pluginHostAppName];
    NSString *pluginHostAppExecutablePath = [[NSBundle bundleWithPath:pluginHostAppPath] executablePath];

    NSDictionary *launchProperties = [[NSDictionary alloc] initWithObjectsAndKeys:
                                      pluginHostAppExecutablePath, @"pluginHostPath",
                                      [NSNumber numberWithInt:pluginArchitecture], @"cpuType",
                                      preferredBundleLocalizationName(), @"localization",
                                      nil];

    NSData *data = [NSPropertyListSerialization dataWithPropertyList:launchProperties format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    ASSERT(data);

    [launchProperties release];

    kern_return_t kr = _WKPASpawnPluginHost(m_pluginVendorPort, reinterpret_cast<uint8_t*>(const_cast<void*>([data bytes])), [data length], &pluginHostPort);

    if (kr == MACH_SEND_INVALID_DEST) {
        // The plug-in vendor port has gone away for some reason. Try to reinitialize it.
        m_pluginVendorPort = MACH_PORT_NULL;
        if (!initializeVendorPort())
            return false;
        
        // And spawn the plug-in host again.
        kr = _WKPASpawnPluginHost(m_pluginVendorPort, reinterpret_cast<uint8_t*>(const_cast<void*>([data bytes])), [data length], &pluginHostPort);
    }

    if (kr != KERN_SUCCESS) {
        // FIXME: Check for invalid dest and try to re-spawn the plug-in agent.
        LOG_ERROR("Failed to spawn plug-in host, error %x", kr);
        return false;
    }
    
    NSString *visibleName = [NSString stringWithFormat:UI_STRING_INTERNAL("%@ (%@ Internet plug-in)",
                                                                 "visible name of the plug-in host process. The first argument is the plug-in name "
                                                                 "and the second argument is the application name."),
                             [[(NSString*)pluginPath lastPathComponent] stringByDeletingPathExtension], [[NSProcessInfo processInfo] processName]];
    
    NSDictionary *hostProperties = [[NSDictionary alloc] initWithObjectsAndKeys:
                                    visibleName, @"visibleName",
                                    (NSString *)pluginPath, @"bundlePath",
                                    nil];
    
    data = [NSPropertyListSerialization dataWithPropertyList:hostProperties format:NSPropertyListBinaryFormat_v1_0 options:0 error:NULL];
    ASSERT(data);

    [hostProperties release];

    ProcessSerialNumber psn;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    GetCurrentProcess(&psn);
    ALLOW_DEPRECATED_DECLARATIONS_END

    ASSERT(MACH_PORT_VALID(clientPort));
    kr = _WKPHCheckInWithPluginHost(pluginHostPort, static_cast<uint8_t*>(const_cast<void*>([data bytes])), [data length], clientPort, psn.highLongOfPSN, psn.lowLongOfPSN, renderServerPort,
                                    &pluginHostPSN.highLongOfPSN, &pluginHostPSN.lowLongOfPSN);
    
    if (kr != KERN_SUCCESS) {
        LOG_ERROR("Failed to check in with plug-in host, error %x", kr);
        deallocateSendRightSafely(pluginHostPort);

        return false;
    }

    return true;
}

bool NetscapePluginHostManager::initializeVendorPort()
{
    ASSERT(m_pluginVendorPort == MACH_PORT_NULL);

    // Get the plug-in agent port.
    mach_port_t pluginAgentPort = MACH_PORT_NULL;
    if (bootstrap_look_up(bootstrap_port, "com.apple.WebKit.PluginAgent", &pluginAgentPort) != KERN_SUCCESS) {
        LOG_ERROR("Failed to look up the plug-in agent port");
        return false;
    }
    
    NSData *appNameData = [[[NSProcessInfo processInfo] processName] dataUsingEncoding:NSUTF8StringEncoding];
    
    // Tell the plug-in agent that we exist.
    if (_WKPACheckInApplication(pluginAgentPort, static_cast<uint8_t*>(const_cast<void*>([appNameData bytes])), [appNameData length], &m_pluginVendorPort) != KERN_SUCCESS)
        return false;

    // FIXME: Should we add a notification for when the vendor port dies?
    
    return true;
}

void NetscapePluginHostManager::pluginHostDied(NetscapePluginHostProxy* pluginHost)
{
    PluginHostMap::iterator end = m_pluginHosts.end();

    // This has O(n) complexity but the number of active plug-in hosts is very small so it shouldn't matter.
    for (PluginHostMap::iterator it = m_pluginHosts.begin(); it != end; ++it) {
        if (it->value == pluginHost) {
            m_pluginHosts.remove(it);
            return;
        }
    }
}

RefPtr<NetscapePluginInstanceProxy> NetscapePluginHostManager::instantiatePlugin(const String& pluginPath, cpu_type_t pluginArchitecture, const String& bundleIdentifier, WebHostedNetscapePluginView *pluginView, NSString *mimeType, NSArray *attributeKeys, NSArray *attributeValues, NSString *userAgent, NSURL *sourceURL, bool fullFrame, bool isPrivateBrowsingEnabled, bool isAcceleratedCompositingEnabled, bool hostLayersInWindowServer)
{
    NetscapePluginHostProxy* hostProxy = hostForPlugin(pluginPath, pluginArchitecture, bundleIdentifier);
    if (!hostProxy)
        return nullptr;

    RetainPtr<NSMutableDictionary> properties = adoptNS([[NSMutableDictionary alloc] init]);
    
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
    
    [properties.get() setObject:[NSNumber numberWithBool:fullFrame] forKey:@"fullFrame"];
    [properties.get() setObject:[NSNumber numberWithBool:isPrivateBrowsingEnabled] forKey:@"privateBrowsingEnabled"];
    [properties.get() setObject:[NSNumber numberWithBool:isAcceleratedCompositingEnabled] forKey:@"acceleratedCompositingEnabled"];
    [properties.get() setObject:[NSNumber numberWithBool:hostLayersInWindowServer] forKey:@"hostLayersInWindowServer"];

    NSData *data = [NSPropertyListSerialization dataWithPropertyList:properties.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    ASSERT(data);
    
    auto instance = NetscapePluginInstanceProxy::create(hostProxy, pluginView, fullFrame);
    uint32_t requestID = instance->nextRequestID();
    kern_return_t kr = _WKPHInstantiatePlugin(hostProxy->port(), requestID, static_cast<uint8_t*>(const_cast<void*>([data bytes])), [data length], instance->pluginID());
    if (kr == MACH_SEND_INVALID_DEST) {
        // Invalidate the instance.
        instance->invalidate();
        
        // The plug-in host must have died, but we haven't received the death notification yet.
        pluginHostDied(hostProxy);

        // Try to spawn it again.
        hostProxy = hostForPlugin(pluginPath, pluginArchitecture, bundleIdentifier);
        
        // Create a new instance.
        instance = NetscapePluginInstanceProxy::create(hostProxy, pluginView, fullFrame);
        requestID = instance->nextRequestID();
        _WKPHInstantiatePlugin(hostProxy->port(), requestID, static_cast<uint8_t*>(const_cast<void*>([data bytes])), [data length], instance->pluginID());
    }

    auto reply = instance->waitForReply<NetscapePluginInstanceProxy::InstantiatePluginReply>(requestID);
    if (!reply || reply->m_resultCode != KERN_SUCCESS) {
        instance->cleanup();
        return nullptr;
    }
    
    instance->setRenderContextID(reply->m_renderContextID);
    instance->setRendererType(reply->m_rendererType);

    return WTFMove(instance);
}

void NetscapePluginHostManager::createPropertyListFile(const String& pluginPath, cpu_type_t pluginArchitecture, const String& bundleIdentifier)
{
    NetscapePluginHostProxy* hostProxy = hostForPlugin(pluginPath, pluginArchitecture, bundleIdentifier);
    if (!hostProxy)
        return;

    _WKPHCreatePluginMIMETypesPreferences(hostProxy->port());
}
    
void NetscapePluginHostManager::didCreateWindow()
{
    // See if any of our hosts are in full-screen mode.
    PluginHostMap::iterator end = m_pluginHosts.end();
    for (PluginHostMap::iterator it = m_pluginHosts.begin(); it != end; ++it) {
        NetscapePluginHostProxy* hostProxy = it->value;
        
        if (!hostProxy->isMenuBarVisible()) {
            // Make ourselves the front process.
            ProcessSerialNumber psn;
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            GetCurrentProcess(&psn);
            SetFrontProcess(&psn);
            ALLOW_DEPRECATED_DECLARATIONS_END
            return;
        }
    }
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)
