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
#import "APIDictionary.h"
#import "APINumber.h"
#import "APIString.h"
#import "StringUtilities.h"
#import "WKAPICast.h"
#import "WKPluginInformation.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import "WebProcessPool.h"
#import <WebCore/PluginBlocklist.h>
#import <WebCore/WebGLBlocklist.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

bool WKContextIsPlugInUpdateAvailable(WKContextRef, WKStringRef)
{
    return false;
}

void WKContextSetPluginLoadClientPolicy(WKContextRef, WKPluginLoadClientPolicy, WKStringRef, WKStringRef, WKStringRef)
{
}

void WKContextClearPluginClientPolicies(WKContextRef)
{
}

WKDictionaryRef WKContextCopyPlugInInfoForBundleIdentifier(WKContextRef contextRef, WKStringRef plugInBundleIdentifierRef)
{
    return 0;
}

void WKContextGetInfoForInstalledPlugIns(WKContextRef contextRef, WKContextGetInfoForInstalledPlugInsBlock block)
{
}

void WKContextResetHSTSHosts(WKContextRef)
{
}

void WKContextResetHSTSHostsAddedAfterDate(WKContextRef, double)
{
}

void WKContextRegisterSchemeForCustomProtocol(WKContextRef context, WKStringRef scheme)
{
    WebKit::WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(WebKit::toWTFString(scheme));
}

void WKContextUnregisterSchemeForCustomProtocol(WKContextRef context, WKStringRef scheme)
{
    WebKit::WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(WebKit::toWTFString(scheme));
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
#if PLATFORM(MAC)
    return WebCore::WebGLBlocklist::shouldBlockWebGL();
#else
    return false;
#endif
}

bool WKContextShouldSuggestBlockWebGL()
{
#if PLATFORM(MAC)
    return WebCore::WebGLBlocklist::shouldSuggestBlockingWebGL();
#else
    return false;
#endif
}

bool WKContextHandlesSafeBrowsing()
{
    return true;
}
