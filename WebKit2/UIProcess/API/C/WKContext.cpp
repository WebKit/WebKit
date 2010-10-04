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

#include "WKContext.h"
#include "WKContextPrivate.h"

#include "WKAPICast.h"
#include "WebContext.h"
#include "WebPreferences.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

WKTypeID WKContextGetTypeID()
{
    return toRef(WebContext::APIType);
}

WKContextRef WKContextCreate()
{
    RefPtr<WebContext> context = WebContext::create(String());
    return toRef(context.release().releaseRef());
}

WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef pathRef)
{
    RefPtr<WebContext> context = WebContext::create(toWK(pathRef)->string());
    return toRef(context.release().releaseRef());
}

WKContextRef WKContextGetSharedProcessContext()
{
    return toRef(WebContext::sharedProcessContext());
}

WKContextRef WKContextGetSharedThreadContext()
{
    return toRef(WebContext::sharedThreadContext());
}

void WKContextSetPreferences(WKContextRef contextRef, WKPreferencesRef preferencesRef)
{
    toWK(contextRef)->setPreferences(toWK(preferencesRef));
}

WKPreferencesRef WKContextGetPreferences(WKContextRef contextRef)
{
    return toRef(toWK(contextRef)->preferences());
}

void WKContextSetInjectedBundleClient(WKContextRef contextRef, const WKContextInjectedBundleClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(contextRef)->initializeInjectedBundleClient(wkClient);
}

void WKContextSetHistoryClient(WKContextRef contextRef, const WKContextHistoryClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(contextRef)->initializeHistoryClient(wkClient);
}

void WKContextPostMessageToInjectedBundle(WKContextRef contextRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toWK(contextRef)->postMessageToInjectedBundle(toWK(messageNameRef)->string(), toWK(messageBodyRef));
}

void WKContextGetStatistics(WKContextRef contextRef, WKContextStatistics* statistics)
{
    toWK(contextRef)->getStatistics(statistics);
}

void WKContextAddVisitedLink(WKContextRef contextRef, WKStringRef visitedURL)
{
    toWK(contextRef)->addVisitedLink(toWK(visitedURL)->string());
}

void WKContextSetCacheModel(WKContextRef contextRef, WKCacheModel cacheModel)
{
    toWK(contextRef)->setCacheModel(toCacheModel(cacheModel));
}

WKCacheModel WKContextGetCacheModel(WKContextRef contextRef)
{
    return toRef(toWK(contextRef)->cacheModel());
}

void _WKContextSetAdditionalPluginsDirectory(WKContextRef contextRef, WKStringRef pluginsDirectory)
{
    toWK(contextRef)->setAdditionalPluginsDirectory(toWK(pluginsDirectory)->string());
}

void _WKContextRegisterURLSchemeAsEmptyDocument(WKContextRef contextRef, WKStringRef urlScheme)
{
    toWK(contextRef)->registerURLSchemeAsEmptyDocument(toWK(urlScheme)->string());
}
