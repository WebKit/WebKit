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

#import "ImmutableDictionary.h"
#import "PluginInfoStore.h"
#import "StringUtilities.h"
#import "WKAPICast.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import "WebContext.h"
#import "WebNumber.h"
#import "WebString.h"
#import <WebKitSystemInterface.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

bool WKContextIsPlugInUpdateAvailable(WKContextRef contextRef, WKStringRef plugInBundleIdentifierRef)
{
    return WKIsPluginUpdateAvailable((NSString *)adoptCF(WKStringCopyCFString(kCFAllocatorDefault, plugInBundleIdentifierRef)).get());
}


WKStringRef WKPlugInInfoPathKey()
{
    static WebString* key = WebString::createFromUTF8String("WKPlugInInfoPath").leakRef();
    return toAPI(key);
}

WKStringRef WKPlugInInfoBundleIdentifierKey()
{
    static WebString* key = WebString::createFromUTF8String("WKPlugInInfoBundleIdentifier").leakRef();
    return toAPI(key);
}

WKStringRef WKPlugInInfoVersionKey()
{
    static WebString* key = WebString::createFromUTF8String("WKPlugInInfoVersion").leakRef();
    return toAPI(key);
}

WKStringRef WKPlugInInfoLoadPolicyKey()
{
    static WebString* key = WebString::createFromUTF8String("WKPlugInInfoLoadPolicy").leakRef();
    return toAPI(key);
}

WKStringRef WKPlugInInfoUpdatePastLastBlockedVersionIsKnownAvailableKey()
{
    static WebString* key = WebString::createFromUTF8String("WKPlugInInfoUpdatePastLastBlockedVersionIsKnownAvailable").leakRef();
    return toAPI(key);
}

WKDictionaryRef WKContextCopyPlugInInfoForBundleIdentifier(WKContextRef contextRef, WKStringRef plugInBundleIdentifierRef)
{
    PluginModuleInfo info = toImpl(contextRef)->pluginInfoStore().findPluginWithBundleIdentifier(toWTFString(plugInBundleIdentifierRef));
    if (info.path.isNull())
        return 0;

    ImmutableDictionary::MapType map;
    map.set(toWTFString(WKPlugInInfoPathKey()), WebString::create(info.path));
    map.set(toWTFString(WKPlugInInfoBundleIdentifierKey()), WebString::create(info.bundleIdentifier));
    map.set(toWTFString(WKPlugInInfoVersionKey()), WebString::create(info.versionString));
    map.set(toWTFString(WKPlugInInfoLoadPolicyKey()), WebUInt64::create(toWKPluginLoadPolicy(PluginInfoStore::policyForPlugin(info))));
    map.set(toWTFString(WKPlugInInfoUpdatePastLastBlockedVersionIsKnownAvailableKey()), WebBoolean::create(WKIsPluginUpdateAvailable(nsStringFromWebCoreString(info.bundleIdentifier))));

    return toAPI(ImmutableDictionary::adopt(map).leakRef());
}
