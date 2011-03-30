/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKBundle.h"
#include "WKBundlePrivate.h"

#include "InjectedBundle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"

using namespace WebKit;

WKTypeID WKBundleGetTypeID()
{
    return toAPI(InjectedBundle::APIType);
}

void WKBundleSetClient(WKBundleRef bundleRef, WKBundleClient * wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(bundleRef)->initializeClient(wkClient);
}

void WKBundlePostMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toImpl(bundleRef)->postMessage(toImpl(messageNameRef)->string(), toImpl(messageBodyRef));
}

void WKBundlePostSynchronousMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef, WKTypeRef* returnDataRef)
{
    RefPtr<APIObject> returnData;
    toImpl(bundleRef)->postSynchronousMessage(toImpl(messageNameRef)->string(), toImpl(messageBodyRef), returnData);
    if (returnDataRef)
        *returnDataRef = toAPI(returnData.release().leakRef());
}

void WKBundleSetShouldTrackVisitedLinks(WKBundleRef bundleRef, bool shouldTrackVisitedLinks)
{
    toImpl(bundleRef)->setShouldTrackVisitedLinks(shouldTrackVisitedLinks);
}

void WKBundleRemoveAllVisitedLinks(WKBundleRef bundleRef)
{
    toImpl(bundleRef)->removeAllVisitedLinks();
}

void WKBundleActivateMacFontAscentHack(WKBundleRef bundleRef)
{
    toImpl(bundleRef)->activateMacFontAscentHack();
}

void WKBundleGarbageCollectJavaScriptObjects(WKBundleRef bundleRef)
{
    toImpl(bundleRef)->garbageCollectJavaScriptObjects();
}

void WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(WKBundleRef bundleRef, bool waitUntilDone)
{
    toImpl(bundleRef)->garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(waitUntilDone);
}

size_t WKBundleGetJavaScriptObjectsCount(WKBundleRef bundleRef)
{
    return toImpl(bundleRef)->javaScriptObjectsCount();
}

void WKBundleAddUserScript(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKStringRef sourceRef, WKURLRef urlRef, WKArrayRef whitelistRef, WKArrayRef blacklistRef, WKUserScriptInjectionTime injectionTimeRef, WKUserContentInjectedFrames injectedFramesRef)
{
    toImpl(bundleRef)->addUserScript(toImpl(pageGroupRef), toImpl(scriptWorldRef), toWTFString(sourceRef), toWTFString(urlRef), toImpl(whitelistRef), toImpl(blacklistRef), toUserScriptInjectionTime(injectionTimeRef), toUserContentInjectedFrames(injectedFramesRef));
}

void WKBundleAddUserStyleSheet(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKStringRef sourceRef, WKURLRef urlRef, WKArrayRef whitelistRef, WKArrayRef blacklistRef, WKUserContentInjectedFrames injectedFramesRef)
{
    toImpl(bundleRef)->addUserStyleSheet(toImpl(pageGroupRef), toImpl(scriptWorldRef), toWTFString(sourceRef), toWTFString(urlRef), toImpl(whitelistRef), toImpl(blacklistRef), toUserContentInjectedFrames(injectedFramesRef));
}

void WKBundleRemoveUserScript(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKURLRef urlRef)
{
    toImpl(bundleRef)->removeUserScript(toImpl(pageGroupRef), toImpl(scriptWorldRef), toWTFString(urlRef));
}

void WKBundleRemoveUserStyleSheet(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKURLRef urlRef)
{
    toImpl(bundleRef)->removeUserStyleSheet(toImpl(pageGroupRef), toImpl(scriptWorldRef), toWTFString(urlRef));
}

void WKBundleRemoveUserScripts(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef)
{
    toImpl(bundleRef)->removeUserScripts(toImpl(pageGroupRef), toImpl(scriptWorldRef));
}

void WKBundleRemoveUserStyleSheets(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef)
{
    toImpl(bundleRef)->removeUserStyleSheets(toImpl(pageGroupRef), toImpl(scriptWorldRef));
}

void WKBundleRemoveAllUserContent(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef)
{
    toImpl(bundleRef)->removeAllUserContent(toImpl(pageGroupRef));
}

void WKBundleOverrideXSSAuditorEnabledForTestRunner(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    toImpl(bundleRef)->overrideXSSAuditorEnabledForTestRunner(toImpl(pageGroupRef), enabled);
}

void WKBundleOverrideAllowUniversalAccessFromFileURLsForTestRunner(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    toImpl(bundleRef)->overrideAllowUniversalAccessFromFileURLsForTestRunner(toImpl(pageGroupRef), enabled);
}

void WKBundleReportException(JSContextRef context, JSValueRef exception)
{
    InjectedBundle::reportException(context, exception);
}
