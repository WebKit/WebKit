/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WKPageConfigurationRef.h"

#include "APIPageConfiguration.h"
#include "WKAPICast.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebUserContentControllerProxy.h"

using namespace WebKit;

WKTypeID WKPageConfigurationGetTypeID()
{
    return toAPI(API::PageConfiguration::APIType);
}

WKPageConfigurationRef WKPageConfigurationCreate()
{
    return toAPI(&API::PageConfiguration::create().leakRef());
}

WKContextRef WKPageConfigurationGetContext(WKPageConfigurationRef configuration)
{
    return toAPI(toImpl(configuration)->processPool());
}

void WKPageConfigurationSetContext(WKPageConfigurationRef configuration, WKContextRef context)
{
    toImpl(configuration)->setProcessPool(toImpl(context));
}

WKPageGroupRef WKPageConfigurationGetPageGroup(WKPageConfigurationRef configuration)
{
    return toAPI(toImpl(configuration)->pageGroup());
}

void WKPageConfigurationSetPageGroup(WKPageConfigurationRef configuration, WKPageGroupRef pageGroup)
{
    toImpl(configuration)->setPageGroup(toImpl(pageGroup));
}

WKUserContentControllerRef WKPageConfigurationGetUserContentController(WKPageConfigurationRef configuration)
{
    return toAPI(toImpl(configuration)->userContentController());
}

void WKPageConfigurationSetUserContentController(WKPageConfigurationRef configuration, WKUserContentControllerRef userContentController)
{
    toImpl(configuration)->setUserContentController(toImpl(userContentController));
}

WKPreferencesRef WKPageConfigurationGetPreferences(WKPageConfigurationRef configuration)
{
    return toAPI(toImpl(configuration)->preferences());
}

void WKPageConfigurationSetPreferences(WKPageConfigurationRef configuration, WKPreferencesRef preferences)
{
    toImpl(configuration)->setPreferences(toImpl(preferences));
}

WKPageRef WKPageConfigurationGetRelatedPage(WKPageConfigurationRef configuration)
{
    return toAPI(toImpl(configuration)->relatedPage());
}

void WKPageConfigurationSetRelatedPage(WKPageConfigurationRef configuration, WKPageRef relatedPage)
{
    toImpl(configuration)->setRelatedPage(toImpl(relatedPage));
}

WKWebsiteDataStoreRef WKPageConfigurationGetWebsiteDataStore(WKPageConfigurationRef configuration)
{
    return toAPI(toImpl(configuration)->websiteDataStore());
}

void WKPageConfigurationSetWebsiteDataStore(WKPageConfigurationRef configuration, WKWebsiteDataStoreRef websiteDataStore)
{
    toImpl(configuration)->setWebsiteDataStore(toImpl(websiteDataStore));
}

void WKPageConfigurationSetInitialCapitalizationEnabled(WKPageConfigurationRef configuration, bool enabled)
{
    toImpl(configuration)->setInitialCapitalizationEnabled(enabled);
}

void WKPageConfigurationSetBackgroundCPULimit(WKPageConfigurationRef configuration, double cpuLimit)
{
    toImpl(configuration)->setCPULimit(cpuLimit);
}
