/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKContextPrivateMac.h"

#import "APIArray.h"
#import "APINumber.h"
#import "APIString.h"
#import "ImmutableDictionary.h"
#import "PluginInfoStore.h"
#import "PluginInformation.h"
#import "PluginSandboxProfile.h"
#import "StringUtilities.h"
#import "WKAPICast.h"
#import "WKPluginInformation.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import "WebContext.h"
#import <WebKitSystemInterface.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

bool WKContextIsPlugInUpdateAvailable(WKContextRef contextRef, WKStringRef plugInBundleIdentifierRef)
{
#if PLATFORM(IOS)
    return false;
#else
    return WKIsPluginUpdateAvailable((NSString *)adoptCF(WKStringCopyCFString(kCFAllocatorDefault, plugInBundleIdentifierRef)).get());
#endif
}

WKDictionaryRef WKContextCopyPlugInInfoForBundleIdentifier(WKContextRef contextRef, WKStringRef plugInBundleIdentifierRef)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginModuleInfo plugin = toImpl(contextRef)->pluginInfoStore().findPluginWithBundleIdentifier(toWTFString(plugInBundleIdentifierRef));
    if (plugin.path.isNull())
        return 0;

    RefPtr<ImmutableDictionary> dictionary = createPluginInformationDictionary(plugin);
    return toAPI(dictionary.release().leakRef());
#else
    return 0;
#endif
}

void WKContextGetInfoForInstalledPlugIns(WKContextRef contextRef, WKContextGetInfoForInstalledPlugInsBlock block)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins = toImpl(contextRef)->pluginInfoStore().plugins();

    Vector<RefPtr<API::Object>> pluginInfoDictionaries;
    pluginInfoDictionaries.reserveInitialCapacity(plugins.size());

    for (const auto& plugin: plugins)
        pluginInfoDictionaries.uncheckedAppend(createPluginInformationDictionary(plugin));

    RefPtr<API::Array> array = API::Array::create(std::move(pluginInfoDictionaries));

    toImpl(contextRef)->ref();
    dispatch_async(dispatch_get_main_queue(), ^() {
        block(toAPI(array.get()), 0);
    
        toImpl(contextRef)->deref();
    });
#endif
}

void WKContextResetHSTSHosts(WKContextRef context)
{
    return toImpl(context)->resetHSTSHosts();
}



void WKContextRegisterSchemeForCustomProtocol(WKContextRef context, WKStringRef scheme)
{
    WebContext::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(toWTFString(scheme));
}

void WKContextUnregisterSchemeForCustomProtocol(WKContextRef context, WKStringRef scheme)
{
    WebContext::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(toWTFString(scheme));
}

/* DEPRECATED -  Please use constants from WKPluginInformation instead. */

WKStringRef WKPlugInInfoPathKey()
{
    return WKPluginInformationPathKey();
}

WKStringRef WKPlugInInfoBundleIdentifierKey()
{
    return WKPluginInformationBundleIdentifierKey();
}

WKStringRef WKPlugInInfoVersionKey()
{
    return WKPluginInformationBundleVersionKey();
}

WKStringRef WKPlugInInfoLoadPolicyKey()
{
    return WKPluginInformationDefaultLoadPolicyKey();
}

WKStringRef WKPlugInInfoUpdatePastLastBlockedVersionIsKnownAvailableKey()
{
    return WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey();
}

WKStringRef WKPlugInInfoIsSandboxedKey()
{
    return WKPluginInformationHasSandboxProfileKey();
}

bool WKContextShouldBlockWebGL()
{
    return WKShouldBlockWebGL();
}

bool WKContextShouldSuggestBlockWebGL()
{
    return WKShouldSuggestBlockingWebGL();
}
