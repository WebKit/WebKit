/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "InjectedBundlePageLoaderClient.h"

#include "APIArray.h"
#include "APIData.h"
#include "APIError.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "InjectedBundleDOMWindowExtension.h"
#include "InjectedBundleScriptWorld.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WKSharedAPICast.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

InjectedBundlePageLoaderClient::InjectedBundlePageLoaderClient(const WKBundlePageLoaderClientBase* client)
{
    initialize(client);
#if !PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED > 101400
    // Deprecated callbacks.
    ASSERT(!m_client.shouldGoToBackForwardListItem);
#endif
}

void InjectedBundlePageLoaderClient::willLoadURLRequest(WebPage& page, const ResourceRequest& request, API::Object* userData)
{
    if (!m_client.willLoadURLRequest)
        return;

    m_client.willLoadURLRequest(toAPI(&page), toAPI(request), toAPI(userData), m_client.base.clientInfo);
}

static void releaseSharedBuffer(unsigned char*, const void* data)
{
    // Balanced by ref() in InjectedBundlePageLoaderClient::willLoadDataRequest().
    static_cast<SharedBuffer*>(const_cast<void*>(data))->deref();
}

void InjectedBundlePageLoaderClient::willLoadDataRequest(WebPage& page, const ResourceRequest& request, SharedBuffer* sharedBuffer, const String& MIMEType, const String& encodingName, const URL& unreachableURL, API::Object* userData)
{
    if (!m_client.willLoadDataRequest)
        return;

    RefPtr<API::Data> data;
    if (sharedBuffer) {
        sharedBuffer->ref();
        data = API::Data::createWithoutCopying((const unsigned char*)sharedBuffer->data(), sharedBuffer->size(), releaseSharedBuffer, sharedBuffer);
    }

    m_client.willLoadDataRequest(toAPI(&page), toAPI(request), toAPI(data.get()), toAPI(MIMEType.impl()), toAPI(encodingName.impl()), toURLRef(unreachableURL.string().impl()), toAPI(userData), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::didStartProvisionalLoadForFrame(WebPage& page, WebFrame& frame, CompletionHandler<void(RefPtr<API::Object>&&)>&& completionHandler)
{
    if (!m_client.didStartProvisionalLoadForFrame)
        return completionHandler(nullptr);

    WKTypeRef userDataToPass = nullptr;
    m_client.didStartProvisionalLoadForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    completionHandler(adoptRef(toImpl(userDataToPass)));
}

void InjectedBundlePageLoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didReceiveServerRedirectForProvisionalLoadForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didReceiveServerRedirectForProvisionalLoadForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPage& page, WebFrame& frame, const ResourceError& error, RefPtr<API::Object>& userData)
{
    if (!m_client.didFailProvisionalLoadWithErrorForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didFailProvisionalLoadWithErrorForFrame(toAPI(&page), toAPI(&frame), toAPI(error), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didCommitLoadForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didCommitLoadForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didCommitLoadForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFinishDocumentLoadForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didFinishDocumentLoadForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didFinishDocumentLoadForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFinishLoadForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didFinishLoadForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didFinishLoadForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFinishProgress(WebPage& page)
{
    if (!m_client.didFinishProgress)
        return;

    m_client.didFinishProgress(toAPI(&page), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::didFailLoadWithErrorForFrame(WebPage& page, WebFrame& frame, const ResourceError& error, RefPtr<API::Object>& userData)
{
    if (!m_client.didFailLoadWithErrorForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didFailLoadWithErrorForFrame(toAPI(&page), toAPI(&frame), toAPI(error), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didSameDocumentNavigationForFrame(WebPage& page, WebFrame& frame, SameDocumentNavigationType type, RefPtr<API::Object>& userData)
{
    if (!m_client.didSameDocumentNavigationForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didSameDocumentNavigationForFrame(toAPI(&page), toAPI(&frame), toAPI(type), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didReceiveTitleForFrame(WebPage& page, const String& title, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didReceiveTitleForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didReceiveTitleForFrame(toAPI(&page), toAPI(title.impl()), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didRemoveFrameFromHierarchy(WebPage& page , WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didRemoveFrameFromHierarchy)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didRemoveFrameFromHierarchy(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didDisplayInsecureContentForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didDisplayInsecureContentForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didDisplayInsecureContentForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didRunInsecureContentForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didRunInsecureContentForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didRunInsecureContentForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didDetectXSSForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didDetectXSSForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didDetectXSSForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFirstLayoutForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didFirstLayoutForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didFirstLayoutForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didFirstVisuallyNonEmptyLayoutForFrame(WebPage& page, WebFrame& frame, RefPtr<API::Object>& userData)
{
    if (!m_client.didFirstVisuallyNonEmptyLayoutForFrame)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didFirstVisuallyNonEmptyLayoutForFrame(toAPI(&page), toAPI(&frame), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didLayoutForFrame(WebPage& page, WebFrame& frame)
{
    if (!m_client.didLayoutForFrame)
        return;

    m_client.didLayoutForFrame(toAPI(&page), toAPI(&frame), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::didReachLayoutMilestone(WebPage& page, OptionSet<WebCore::LayoutMilestone> milestones, RefPtr<API::Object>& userData)
{
    if (!m_client.didLayout)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didLayout(toAPI(&page), toWKLayoutMilestones(milestones), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageLoaderClient::didClearWindowObjectForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    if (!m_client.didClearWindowObjectForFrame)
        return;

    m_client.didClearWindowObjectForFrame(toAPI(&page), toAPI(&frame), toAPI(InjectedBundleScriptWorld::getOrCreate(world).ptr()), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::didCancelClientRedirectForFrame(WebPage& page, WebFrame& frame)
{
    if (!m_client.didCancelClientRedirectForFrame)
        return;

    m_client.didCancelClientRedirectForFrame(toAPI(&page), toAPI(&frame), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::willPerformClientRedirectForFrame(WebPage& page, WebFrame& frame, const String& url, double delay, WallTime date)
{
    if (!m_client.willPerformClientRedirectForFrame)
        return;

    m_client.willPerformClientRedirectForFrame(toAPI(&page), toAPI(&frame), toURLRef(url.impl()), delay, date.secondsSinceEpoch().seconds(), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::didHandleOnloadEventsForFrame(WebPage& page, WebFrame& frame)
{
    if (!m_client.didHandleOnloadEventsForFrame)
        return;

    m_client.didHandleOnloadEventsForFrame(toAPI(&page), toAPI(&frame), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::globalObjectIsAvailableForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    if (!m_client.globalObjectIsAvailableForFrame)
        return;

    RefPtr<InjectedBundleScriptWorld> injectedWorld = InjectedBundleScriptWorld::getOrCreate(world);
    m_client.globalObjectIsAvailableForFrame(toAPI(&page), toAPI(&frame), toAPI(injectedWorld.get()), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::willInjectUserScriptForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    if (!m_client.willInjectUserScriptForFrame)
        return;

    RefPtr<InjectedBundleScriptWorld> injectedWorld = InjectedBundleScriptWorld::getOrCreate(world);
    m_client.willInjectUserScriptForFrame(toAPI(&page), toAPI(&frame), toAPI(injectedWorld.get()), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::willDisconnectDOMWindowExtensionFromGlobalObject(WebPage& page, DOMWindowExtension* coreExtension)
{
    if (!m_client.willDisconnectDOMWindowExtensionFromGlobalObject)
        return;

    RefPtr<InjectedBundleDOMWindowExtension> extension = InjectedBundleDOMWindowExtension::get(coreExtension);
    ASSERT(extension);
    m_client.willDisconnectDOMWindowExtensionFromGlobalObject(toAPI(&page), toAPI(extension.get()), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::didReconnectDOMWindowExtensionToGlobalObject(WebPage& page, DOMWindowExtension* coreExtension)
{
    if (!m_client.didReconnectDOMWindowExtensionToGlobalObject)
        return;

    RefPtr<InjectedBundleDOMWindowExtension> extension = InjectedBundleDOMWindowExtension::get(coreExtension);
    ASSERT(extension);
    m_client.didReconnectDOMWindowExtensionToGlobalObject(toAPI(&page), toAPI(extension.get()), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::willDestroyGlobalObjectForDOMWindowExtension(WebPage& page, DOMWindowExtension* coreExtension)
{
    if (!m_client.willDestroyGlobalObjectForDOMWindowExtension)
        return;

    RefPtr<InjectedBundleDOMWindowExtension> extension = InjectedBundleDOMWindowExtension::get(coreExtension);
    ASSERT(extension);
    m_client.willDestroyGlobalObjectForDOMWindowExtension(toAPI(&page), toAPI(extension.get()), m_client.base.clientInfo);
}

bool InjectedBundlePageLoaderClient::shouldForceUniversalAccessFromLocalURL(WebPage& page, const String& url)
{
    if (!m_client.shouldForceUniversalAccessFromLocalURL)
        return false;

    return m_client.shouldForceUniversalAccessFromLocalURL(toAPI(&page), toAPI(url.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageLoaderClient::featuresUsedInPage(WebPage& page, const Vector<String>& features)
{
    if (!m_client.featuresUsedInPage)
        return;

    return m_client.featuresUsedInPage(toAPI(&page), toAPI(API::Array::createStringArray(features).ptr()), m_client.base.clientInfo);
}

String InjectedBundlePageLoaderClient::userAgentForURL(WebFrame& frame, const URL& url) const
{
    if (!m_client.userAgentForURL)
        return String();
    WKStringRef userAgent = m_client.userAgentForURL(toAPI(&frame), toAPI(API::URL::create(url).ptr()), m_client.base.clientInfo);
    if (!userAgent)
        return String();
    return toImpl(userAgent)->string();
}

OptionSet<WebCore::LayoutMilestone> InjectedBundlePageLoaderClient::layoutMilestones() const
{
    OptionSet<WebCore::LayoutMilestone> milestones;
    if (m_client.didFirstLayoutForFrame)
        milestones.add(WebCore::DidFirstLayout);
    if (m_client.didFirstVisuallyNonEmptyLayoutForFrame)
        milestones.add(WebCore::DidFirstVisuallyNonEmptyLayout);
    return milestones;
}

} // namespace WebKit
