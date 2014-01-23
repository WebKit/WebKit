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
#include "WebLoaderClient.h"

#include "APIArray.h"
#include "ImmutableDictionary.h"
#include "PluginInformation.h"
#include "WKAPICast.h"
#include "WebBackForwardListItem.h"
#include "WebPageProxy.h"
#include <string.h>

using namespace WebCore;

namespace WebKit {

void WebLoaderClient::didStartProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didStartProvisionalLoadForFrame)
        return;

    m_client.didStartProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
        return;

    m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, API::Object* userData)
{
    if (!m_client.didFailProvisionalLoadWithErrorForFrame)
        return;

    m_client.didFailProvisionalLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didCommitLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didCommitLoadForFrame)
        return;

    m_client.didCommitLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didFinishDocumentLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didFinishDocumentLoadForFrame)
        return;

    m_client.didFinishDocumentLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didFinishLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didFinishLoadForFrame)
        return;

    m_client.didFinishLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didFailLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, API::Object* userData)
{
    if (!m_client.didFailLoadWithErrorForFrame)
        return;

    m_client.didFailLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didSameDocumentNavigationForFrame(WebPageProxy* page, WebFrameProxy* frame, SameDocumentNavigationType type, API::Object* userData)
{
    if (!m_client.didSameDocumentNavigationForFrame)
        return;

    m_client.didSameDocumentNavigationForFrame(toAPI(page), toAPI(frame), toAPI(type), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didReceiveTitleForFrame(WebPageProxy* page, const String& title, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didReceiveTitleForFrame)
        return;

    m_client.didReceiveTitleForFrame(toAPI(page), toAPI(title.impl()), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didFirstLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didFirstLayoutForFrame)
        return;

    m_client.didFirstLayoutForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didFirstVisuallyNonEmptyLayoutForFrame)
        return;

    m_client.didFirstVisuallyNonEmptyLayoutForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didLayout(WebPageProxy* page, LayoutMilestones milestones, API::Object* userData)
{
    if (!m_client.didLayout)
        return;

    m_client.didLayout(toAPI(page), toWKLayoutMilestones(milestones), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didRemoveFrameFromHierarchy(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didRemoveFrameFromHierarchy)
        return;

    m_client.didRemoveFrameFromHierarchy(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didDisplayInsecureContentForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didDisplayInsecureContentForFrame)
        return;

    m_client.didDisplayInsecureContentForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didRunInsecureContentForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didRunInsecureContentForFrame)
        return;

    m_client.didRunInsecureContentForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

void WebLoaderClient::didDetectXSSForFrame(WebPageProxy* page, WebFrameProxy* frame, API::Object* userData)
{
    if (!m_client.didDetectXSSForFrame)
        return;

    m_client.didDetectXSSForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_client.base.clientInfo);
}

bool WebLoaderClient::canAuthenticateAgainstProtectionSpaceInFrame(WebPageProxy* page, WebFrameProxy* frame, WebProtectionSpace* protectionSpace)
{
    if (!m_client.canAuthenticateAgainstProtectionSpaceInFrame)
        return false;

    return m_client.canAuthenticateAgainstProtectionSpaceInFrame(toAPI(page), toAPI(frame), toAPI(protectionSpace), m_client.base.clientInfo);
}

void WebLoaderClient::didReceiveAuthenticationChallengeInFrame(WebPageProxy* page, WebFrameProxy* frame, AuthenticationChallengeProxy* authenticationChallenge)
{
    if (!m_client.didReceiveAuthenticationChallengeInFrame)
        return;

    m_client.didReceiveAuthenticationChallengeInFrame(toAPI(page), toAPI(frame), toAPI(authenticationChallenge), m_client.base.clientInfo);
}

void WebLoaderClient::didStartProgress(WebPageProxy* page)
{
    if (!m_client.didStartProgress)
        return;

    m_client.didStartProgress(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::didChangeProgress(WebPageProxy* page)
{
    if (!m_client.didChangeProgress)
        return;

    m_client.didChangeProgress(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::didFinishProgress(WebPageProxy* page)
{
    if (!m_client.didFinishProgress)
        return;

    m_client.didFinishProgress(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::processDidBecomeUnresponsive(WebPageProxy* page)
{
    if (!m_client.processDidBecomeUnresponsive)
        return;

    m_client.processDidBecomeUnresponsive(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::interactionOccurredWhileProcessUnresponsive(WebPageProxy* page)
{
    if (!m_client.interactionOccurredWhileProcessUnresponsive)
        return;

    m_client.interactionOccurredWhileProcessUnresponsive(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::processDidBecomeResponsive(WebPageProxy* page)
{
    if (!m_client.processDidBecomeResponsive)
        return;

    m_client.processDidBecomeResponsive(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::processDidCrash(WebPageProxy* page)
{
    if (!m_client.processDidCrash)
        return;

    m_client.processDidCrash(toAPI(page), m_client.base.clientInfo);
}

void WebLoaderClient::didChangeBackForwardList(WebPageProxy* page, WebBackForwardListItem* addedItem, Vector<RefPtr<API::Object>>* removedItems)
{
    if (!m_client.didChangeBackForwardList)
        return;

    RefPtr<API::Array> removedItemsArray;
    if (removedItems && !removedItems->isEmpty())
        removedItemsArray = API::Array::create(std::move(*removedItems));

    m_client.didChangeBackForwardList(toAPI(page), toAPI(addedItem), toAPI(removedItemsArray.get()), m_client.base.clientInfo);
}

bool WebLoaderClient::shouldGoToBackForwardListItem(WebPageProxy* page, WebBackForwardListItem* item)
{
    // We should only even considering sending the shouldGoToBackForwardListItem() client callback
    // for version 0 clients. Later versioned clients should get willGoToBackForwardListItem() instead,
    // but due to XPC race conditions this one might have been called instead.
    if (m_client.base.version > 0 || !m_client.shouldGoToBackForwardListItem)
        return true;

    return m_client.shouldGoToBackForwardListItem(toAPI(page), toAPI(item), m_client.base.clientInfo);
}

void WebLoaderClient::willGoToBackForwardListItem(WebPageProxy* page, WebBackForwardListItem* item, API::Object* userData)
{
    if (m_client.willGoToBackForwardListItem)
        m_client.willGoToBackForwardListItem(toAPI(page), toAPI(item), toAPI(userData), m_client.base.clientInfo);
}

#if ENABLE(NETSCAPE_PLUGIN_API)

void WebLoaderClient::didFailToInitializePlugin(WebPageProxy* page, ImmutableDictionary* pluginInformation)
{
    if (m_client.didFailToInitializePlugin_deprecatedForUseWithV0)
        m_client.didFailToInitializePlugin_deprecatedForUseWithV0(
            toAPI(page),
            toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())),
            m_client.base.clientInfo);

    if (m_client.pluginDidFail_deprecatedForUseWithV1)
        m_client.pluginDidFail_deprecatedForUseWithV1(
            toAPI(page),
            kWKErrorCodeCannotLoadPlugIn,
            toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())),
            0,
            0,
            m_client.base.clientInfo);

    if (m_client.pluginDidFail)
        m_client.pluginDidFail(
            toAPI(page),
            kWKErrorCodeCannotLoadPlugIn,
            toAPI(pluginInformation),
            m_client.base.clientInfo);
}

void WebLoaderClient::didBlockInsecurePluginVersion(WebPageProxy* page, ImmutableDictionary* pluginInformation)
{
    if (m_client.pluginDidFail_deprecatedForUseWithV1)
        m_client.pluginDidFail_deprecatedForUseWithV1(
            toAPI(page),
            kWKErrorCodeInsecurePlugInVersion,
            toAPI(pluginInformation->get<API::String>(pluginInformationMIMETypeKey())),
            toAPI(pluginInformation->get<API::String>(pluginInformationBundleIdentifierKey())),
            toAPI(pluginInformation->get<API::String>(pluginInformationBundleVersionKey())),
            m_client.base.clientInfo);

    if (m_client.pluginDidFail)
        m_client.pluginDidFail(
            toAPI(page),
            kWKErrorCodeInsecurePlugInVersion,
            toAPI(pluginInformation),
            m_client.base.clientInfo);
}

PluginModuleLoadPolicy WebLoaderClient::pluginLoadPolicy(WebPageProxy* page, PluginModuleLoadPolicy currentPluginLoadPolicy, ImmutableDictionary* pluginInformation, String& unavailabilityDescription, String& useBlockedPluginTitle)
{
    WKStringRef unavailabilityDescriptionOut = 0;
    WKStringRef useBlockedPluginTitleOut = 0;
    PluginModuleLoadPolicy loadPolicy = currentPluginLoadPolicy;

    if (m_client.pluginLoadPolicy_deprecatedForUseWithV2)
        loadPolicy = toPluginModuleLoadPolicy(m_client.pluginLoadPolicy_deprecatedForUseWithV2(toAPI(page), toWKPluginLoadPolicy(currentPluginLoadPolicy), toAPI(pluginInformation), m_client.base.clientInfo));
    else if (m_client.pluginLoadPolicy_deprecatedForUseWithV3)
        loadPolicy = toPluginModuleLoadPolicy(m_client.pluginLoadPolicy_deprecatedForUseWithV3(toAPI(page), toWKPluginLoadPolicy(currentPluginLoadPolicy), toAPI(pluginInformation), &unavailabilityDescriptionOut, m_client.base.clientInfo));
    else if (m_client.pluginLoadPolicy)
        loadPolicy = toPluginModuleLoadPolicy(m_client.pluginLoadPolicy(toAPI(page), toWKPluginLoadPolicy(currentPluginLoadPolicy), toAPI(pluginInformation), &unavailabilityDescriptionOut, &useBlockedPluginTitleOut, m_client.base.clientInfo));

    if (unavailabilityDescriptionOut) {
        RefPtr<API::String> webUnavailabilityDescription = adoptRef(toImpl(unavailabilityDescriptionOut));
        unavailabilityDescription = webUnavailabilityDescription->string();
    }

    if (useBlockedPluginTitleOut) {
        RefPtr<API::String> webUseBlockedPluginTitle = adoptRef(toImpl(useBlockedPluginTitleOut));
        useBlockedPluginTitle = webUseBlockedPluginTitle->string();
    }
    
    return loadPolicy;
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
WebCore::WebGLLoadPolicy WebLoaderClient::webGLLoadPolicy(WebPageProxy* page, const String& url) const
{
    WebCore::WebGLLoadPolicy loadPolicy = WebGLAllow;

    if (m_client.webGLLoadPolicy)
        loadPolicy = toWebGLLoadPolicy(m_client.webGLLoadPolicy(toAPI(page), toAPI(url.impl()), m_client.base.clientInfo));

    return loadPolicy;
}
#endif // ENABLE(WEBGL)

} // namespace WebKit
