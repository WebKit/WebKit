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
#include "APIWebsitePolicies.h"
#include "BrowsingContextGroup.h"
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
    return toAPILeakingRef(API::PageConfiguration::create());
}

WKContextRef WKPageConfigurationGetContext(WKPageConfigurationRef configuration)
{
    return toAPI(protect(toProtectedImpl(configuration)->processPool()).get());
}

void WKPageConfigurationSetContext(WKPageConfigurationRef configuration, WKContextRef context)
{
    toProtectedImpl(configuration)->setProcessPool(toProtectedImpl(context));
}

WKPageGroupRef WKPageConfigurationGetPageGroup(WKPageConfigurationRef)
{
    return nullptr;
}

void WKPageConfigurationSetPageGroup(WKPageConfigurationRef, WKPageGroupRef)
{
}

WKUserContentControllerRef WKPageConfigurationGetUserContentController(WKPageConfigurationRef configuration)
{
    return toAPI(protect(toProtectedImpl(configuration)->userContentController()).get());
}

void WKPageConfigurationSetUserContentController(WKPageConfigurationRef configuration, WKUserContentControllerRef userContentController)
{
    toProtectedImpl(configuration)->setUserContentController(toProtectedImpl(userContentController));
}

WKPreferencesRef WKPageConfigurationGetPreferences(WKPageConfigurationRef configuration)
{
    return toAPI(protect(toProtectedImpl(configuration)->preferences()).get());
}

void WKPageConfigurationSetPreferences(WKPageConfigurationRef configuration, WKPreferencesRef preferences)
{
    toProtectedImpl(configuration)->setPreferences(toProtectedImpl(preferences));
}

WKPageRef WKPageConfigurationGetRelatedPage(WKPageConfigurationRef configuration)
{
    return toAPI(protect(toProtectedImpl(configuration)->relatedPage()).get());
}

void WKPageConfigurationSetRelatedPage(WKPageConfigurationRef configuration, WKPageRef relatedPage)
{
    toProtectedImpl(configuration)->setRelatedPage(toProtectedImpl(relatedPage));
}

WKWebsiteDataStoreRef WKPageConfigurationGetWebsiteDataStore(WKPageConfigurationRef configuration)
{
    return toAPI(protect(toProtectedImpl(configuration)->websiteDataStore()).get());
}

void WKPageConfigurationSetWebsiteDataStore(WKPageConfigurationRef configuration, WKWebsiteDataStoreRef websiteDataStore)
{
    toProtectedImpl(configuration)->setWebsiteDataStore(toProtectedImpl(websiteDataStore));
}

void WKPageConfigurationSetInitialCapitalizationEnabled(WKPageConfigurationRef configuration, bool enabled)
{
    toProtectedImpl(configuration)->setInitialCapitalizationEnabled(enabled);
}

void WKPageConfigurationSetBackgroundCPULimit(WKPageConfigurationRef configuration, double cpuLimit)
{
    toProtectedImpl(configuration)->setCPULimit(cpuLimit);
}

void WKPageConfigurationSetAllowTestOnlyIPC(WKPageConfigurationRef configuration, bool allowTestOnlyIPC)
{
    toProtectedImpl(configuration)->setAllowTestOnlyIPC(allowTestOnlyIPC);
}

void WKPageConfigurationSetShouldSendConsoleLogsToUIProcessForTesting(WKPageConfigurationRef configuration, bool should)
{
    toProtectedImpl(configuration)->setShouldSendConsoleLogsToUIProcessForTesting(should);
}

void WKPageConfigurationSetPortsForUpgradingInsecureSchemeForTesting(WKPageConfigurationRef configuration, uint16_t upgradeFromInsecurePort, uint16_t upgradeToSecurePort)
{
    toProtectedImpl(configuration)->setPortsForUpgradingInsecureSchemeForTesting(upgradeFromInsecurePort, upgradeToSecurePort);
}

WKWebsitePoliciesRef WKPageConfigurationGetDefaultWebsitePolicies(WKPageConfigurationRef configuration)
{
    return toAPI(protect(toProtectedImpl(configuration)->defaultWebsitePolicies()).get());
}

void WKPageConfigurationSetDefaultWebsitePolicies(WKPageConfigurationRef configuration, WKWebsitePoliciesRef policies)
{
    toProtectedImpl(configuration)->setDefaultWebsitePolicies(toProtectedImpl(policies));
}
