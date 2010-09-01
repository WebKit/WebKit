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

#include "WKBundle.h"
#include "WKBundlePrivate.h"

#include "InjectedBundle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"

using namespace WebKit;

WKTypeID WKBundleGetTypeID()
{
    return toRef(InjectedBundle::APIType);
}

void WKBundleSetClient(WKBundleRef bundleRef, WKBundleClient * wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(bundleRef)->initializeClient(wkClient);
}

void WKBundlePostMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toWK(bundleRef)->postMessage(toWK(messageNameRef)->string(), toWK(messageBodyRef));
}

void WKBundleSetShouldTrackVisitedLinks(WKBundleRef bundleRef, bool shouldTrackVisitedLinks)
{
    toWK(bundleRef)->setShouldTrackVisitedLinks(shouldTrackVisitedLinks);
}

void WKBundleRemoveAllVisitedLinks(WKBundleRef bundleRef)
{
    toWK(bundleRef)->removeAllVisitedLinks();
}

void WKBundleActivateMacFontAscentHack(WKBundleRef bundleRef)
{
    toWK(bundleRef)->activateMacFontAscentHack();
}

void WKBundleGarbageCollectJavaScriptObjects(WKBundleRef bundleRef)
{
    toWK(bundleRef)->garbageCollectJavaScriptObjects();
}

void WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(WKBundleRef bundleRef, bool waitUntilDone)
{
    toWK(bundleRef)->garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(waitUntilDone);
}

size_t WKBundleGetJavaScriptObjectsCount(WKBundleRef bundleRef)
{
    return toWK(bundleRef)->javaScriptObjectsCount();
}

void WKBundleAddUserScript(WKBundleRef bundleRef, WKBundleScriptWorldRef scriptWorldRef, WKStringRef sourceRef, WKURLRef urlRef, WKArrayRef whitelistRef, WKArrayRef blacklistRef, WKUserScriptInjectionTime injectionTimeRef, WKUserContentInjectedFrames injectedFramesRef)
{
    toWK(bundleRef)->addUserScript(toWK(scriptWorldRef), toWTFString(sourceRef), toWTFString(urlRef), toWK(whitelistRef), toWK(blacklistRef), toUserScriptInjectionTime(injectionTimeRef), toUserContentInjectedFrames(injectedFramesRef));
}

void WKBundleAddUserStyleSheet(WKBundleRef bundleRef, WKBundleScriptWorldRef scriptWorldRef, WKStringRef sourceRef, WKURLRef urlRef, WKArrayRef whitelistRef, WKArrayRef blacklistRef, WKUserContentInjectedFrames injectedFramesRef)
{
    toWK(bundleRef)->addUserStyleSheet(toWK(scriptWorldRef), toWTFString(sourceRef), toWTFString(urlRef), toWK(whitelistRef), toWK(blacklistRef), toUserContentInjectedFrames(injectedFramesRef));
}

void WKBundleRemoveUserScript(WKBundleRef bundleRef, WKBundleScriptWorldRef scriptWorldRef, WKURLRef urlRef)
{
    toWK(bundleRef)->removeUserScript(toWK(scriptWorldRef), toWTFString(urlRef));
}

void WKBundleRemoveUserStyleSheet(WKBundleRef bundleRef, WKBundleScriptWorldRef scriptWorldRef, WKURLRef urlRef)
{
    toWK(bundleRef)->removeUserStyleSheet(toWK(scriptWorldRef), toWTFString(urlRef));
}

void WKBundleRemoveUserScripts(WKBundleRef bundleRef, WKBundleScriptWorldRef scriptWorldRef)
{
    toWK(bundleRef)->removeUserScripts(toWK(scriptWorldRef));
}

void WKBundleRemoveUserStyleSheets(WKBundleRef bundleRef, WKBundleScriptWorldRef scriptWorldRef)
{
    toWK(bundleRef)->removeUserStyleSheets(toWK(scriptWorldRef));
}

void WKBundleRemoveAllUserContent(WKBundleRef bundleRef)
{
    toWK(bundleRef)->removeAllUserContent();
}

void WKBundleOverrideXSSAuditorEnabledForTestRunner(WKBundleRef bundleRef, bool enabled)
{
    toWK(bundleRef)->overrideXSSAuditorEnabledForTestRunner(enabled);
}
