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

#ifndef WKContextPrivateMac_h
#define WKContextPrivateMac_h

#include <WebKit/WKBase.h>
#include <WebKit/WKDeprecated.h>
#include <WebKit/WKPluginLoadPolicy.h>

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT bool WKContextIsPlugInUpdateAvailable(WKContextRef context, WKStringRef plugInBundleIdentifier);

WK_EXPORT void WKContextSetPluginLoadClientPolicy(WKContextRef context, WKPluginLoadClientPolicy policy, WKStringRef host, WKStringRef bundleIdentifier, WKStringRef versionString);
WK_EXPORT void WKContextClearPluginClientPolicies(WKContextRef context);

WK_EXPORT WKDictionaryRef WKContextCopyPlugInInfoForBundleIdentifier(WKContextRef context, WKStringRef plugInBundleIdentifier);

typedef void (^WKContextGetInfoForInstalledPlugInsBlock)(WKArrayRef, WKErrorRef);
WK_EXPORT void WKContextGetInfoForInstalledPlugIns(WKContextRef context, WKContextGetInfoForInstalledPlugInsBlock block);

WK_EXPORT void WKContextResetHSTSHosts(WKContextRef context) WK_C_API_DEPRECATED;
WK_EXPORT void WKContextResetHSTSHostsAddedAfterDate(WKContextRef context, double startDateIntervalSince1970) WK_C_API_DEPRECATED;

WK_EXPORT void WKContextRegisterSchemeForCustomProtocol(WKContextRef context, WKStringRef scheme);
WK_EXPORT void WKContextUnregisterSchemeForCustomProtocol(WKContextRef context, WKStringRef scheme);

/* DEPRECATED -  Please use constants from WKPluginInformation instead. */

/* Value type: WKStringRef */
WK_EXPORT WKStringRef WKPlugInInfoPathKey();

/* Value type: WKStringRef */
WK_EXPORT WKStringRef WKPlugInInfoBundleIdentifierKey();

/* Value type: WKStringRef */
WK_EXPORT WKStringRef WKPlugInInfoVersionKey();

/* Value type: WKUInt64Ref */
WK_EXPORT WKStringRef WKPlugInInfoLoadPolicyKey();

/* Value type: WKBooleanRef */
WK_EXPORT WKStringRef WKPlugInInfoUpdatePastLastBlockedVersionIsKnownAvailableKey();

/* Value type: WKBooleanRef */
WK_EXPORT WKStringRef WKPlugInInfoIsSandboxedKey();

WK_EXPORT bool WKContextShouldBlockWebGL();
WK_EXPORT bool WKContextShouldSuggestBlockWebGL();

WK_EXPORT bool WKContextHandlesSafeBrowsing();

#ifdef __cplusplus
}
#endif

#endif /* WKContextPrivateMac_h */
