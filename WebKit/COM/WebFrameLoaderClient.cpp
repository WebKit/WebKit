/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFrameLoaderClient.h"

// not sure why this is necessary, but the compiler was giving very strange
// errors without this first
#include "IWebURLResponse.h"

#pragma warning(push, 0)
#include "DocumentLoader.h"
#include "Element.h"
#include "FormState.h"
#include "HTMLFormElement.h"
#include "IWebView.h"
#include "PlatformString.h"
#include "WebFrame.h"
#include "WebView.h"
#pragma warning(pop)

using namespace WebCore;

#define notImplemented() {}

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame* webFrame)
    : m_webFrame(webFrame)
{

}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}

void WebFrameLoaderClient::frameLoaderDestroyed()
{
    notImplemented();
}

bool WebFrameLoaderClient::hasWebView() const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::hasFrameView() const
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::privateBrowsingEnabled() const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::makeDocumentView()
{
    notImplemented();
}

void WebFrameLoaderClient::makeRepresentation(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setDocumentViewFromPageCache(WebCore::PageCache*)
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayout()
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent1()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent2()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent4()
{
    notImplemented();
}

void WebFrameLoaderClient::loadedFromPageCache()
{
    notImplemented();
}


void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long, WebCore::DocumentLoader*, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long, WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long, const WebCore::AuthenticationChallenge&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long, const WebCore::AuthenticationChallenge&)        
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceError&)
{
    notImplemented();
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(
    const WebCore::KURL&, double, double)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const WebCore::String&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const WebCore::ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFailLoad(const WebCore::ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    notImplemented();
}

Frame* WebFrameLoaderClient::dispatchCreatePage()
{
    notImplemented();
    return 0;
}

void WebFrameLoaderClient::dispatchShow()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction, const WebCore::String&, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const WebCore::ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillSubmitForm(WebCore::FramePolicyFunction,
                                                  PassRefPtr<WebCore::FormState>)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::revertToProvisionalState(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::clearUnarchivingState(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::progressStarted()
{
    notImplemented();
}

void WebFrameLoaderClient::progressCompleted()
{
    notImplemented();
}

void WebFrameLoaderClient::incrementProgress(unsigned long, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void WebFrameLoaderClient::incrementProgress(unsigned long, const char*, int)
{
    notImplemented();
}

void WebFrameLoaderClient::completeProgress(unsigned long)
{
    notImplemented();
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrameLoaderClient::startDownload(const WebCore::ResourceRequest&)
{
    notImplemented();
}

void WebFrameLoaderClient::willChangeTitle(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeTitle(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::committedLoad(WebCore::DocumentLoader*, const char*, int)
{
    notImplemented();
}

void WebFrameLoaderClient::finishedLoading(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::finalSetupForReplace(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::updateGlobalHistoryForStandardLoad(const WebCore::KURL&)
{
    notImplemented();
}

void WebFrameLoaderClient::updateGlobalHistoryForReload(const WebCore::KURL&)
{
    notImplemented();
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(WebCore::HistoryItem*) const
{
    notImplemented();
    return false;
}

ResourceError WebFrameLoaderClient::cancelledError(const WebCore::ResourceRequest&)
{
    notImplemented();
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const WebCore::ResourceRequest&)
{
    notImplemented();
}

ResourceError WebFrameLoaderClient::interruptForPolicyChangeError(const WebCore::ResourceRequest&)
{
    notImplemented();
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const WebCore::ResourceResponse&)
{
    notImplemented();
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const WebCore::ResourceResponse&)
{
    notImplemented();
}

bool WebFrameLoaderClient::shouldFallBack(const WebCore::ResourceError&)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::setDefersLoading(bool)
{
    notImplemented();
}

bool WebFrameLoaderClient::willUseArchive(ResourceLoader*, const WebCore::ResourceRequest&, const WebCore::KURL&) const
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::isArchiveLoadPending(WebCore::ResourceLoader*) const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::cancelPendingArchiveLoad(WebCore::ResourceLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::clearArchivedResources()
{
    notImplemented();
}

bool WebFrameLoaderClient::canHandleRequest(
    const WebCore::ResourceRequest&) const
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::canShowMIMEType(const WebCore::String&) const
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::representationExistsForURLScheme(
    const WebCore::String&) const
{
    notImplemented();
    return false;
}

WebCore::String WebFrameLoaderClient::generatedMIMETypeForURLScheme(
    const WebCore::String&) const
{
    notImplemented();
    return WebCore::String();
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    notImplemented();
}

void WebFrameLoaderClient::saveScrollPositionAndViewStateToItem(WebCore::HistoryItem*)
{
    notImplemented();
}

void WebFrameLoaderClient::restoreScrollPositionAndViewState()
{
    notImplemented();
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    notImplemented();
}

void WebFrameLoaderClient::didFinishLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    notImplemented();
}

PassRefPtr<WebCore::DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const WebCore::ResourceRequest&)
{
    notImplemented();
}

void WebFrameLoaderClient::setTitle(const WebCore::String&,
                                    const WebCore::KURL&)
{
    notImplemented();
}

WebCore::String WebFrameLoaderClient::userAgent()
{
    return "Mozilla/5.0 (PC; U; Intel; Windows; en) AppleWebKit/420+ (KHTML, like Gecko)";
}

void WebFrameLoaderClient::saveDocumentViewToPageCache(WebCore::PageCache*)
{
    notImplemented();
}

bool WebFrameLoaderClient::canCachePage() const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}
