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

bool WebFrameLoaderClient::hasBackForwardList() const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::resetBackForwardList()
{
    notImplemented();
}

bool WebFrameLoaderClient::provisionalItemIsTarget() const
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::loadProvisionalItemFromPageCache()
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::invalidateCurrentItemPageCache()
{
    notImplemented();
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

void WebFrameLoaderClient::forceLayout()
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebFrameLoaderClient::updateHistoryForCommit()
{
    notImplemented();
}

void WebFrameLoaderClient::updateHistoryForBackForwardNavigation()
{
    notImplemented();
}

void WebFrameLoaderClient::updateHistoryForReload()
{
    notImplemented();
}

void WebFrameLoaderClient::updateHistoryForStandardLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::updateHistoryForInternalLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::updateHistoryAfterClientRedirect()
{
    notImplemented();
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

WebCore::LoadErrorResetToken* WebFrameLoaderClient::tokenForLoadErrorReset()
{
    notImplemented();
    return 0;
}

void WebFrameLoaderClient::resetAfterLoadError(WebCore::LoadErrorResetToken*)
{
    notImplemented();
}

void WebFrameLoaderClient::doNotResetAfterLoadError(WebCore::LoadErrorResetToken*)
{
    notImplemented();
}

void WebFrameLoaderClient::willCloseDocument()
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

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchShow()
{
    notImplemented();
}

void WebFrameLoaderClient::cancelPolicyCheck()
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

void WebFrameLoaderClient::clearLoadingFromPageCache(WebCore::DocumentLoader*)
{
    notImplemented();
}

bool WebFrameLoaderClient::isLoadingFromPageCache(WebCore::DocumentLoader*)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::revertToProvisionalState(WebCore::DocumentLoader*)
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

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
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

void WebFrameLoaderClient::finishedLoading(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::finalSetupForReplace(WebCore::DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setDefersLoading(bool)
{
    notImplemented();
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

void WebFrameLoaderClient::restoreScrollPositionAndViewState()
{
    notImplemented();
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    notImplemented();
}

bool WebFrameLoaderClient::shouldTreatURLAsSameAsCurrent(
    const WebCore::KURL&) const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::addHistoryItemForFragmentScroll()
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

void WebFrameLoaderClient::setTitle(const WebCore::String&,
                                    const WebCore::KURL&)
{
    notImplemented();
}

WebCore::String WebFrameLoaderClient::userAgent()
{
    return "Mozilla/5.0 (PC; U; Intel; Windows; en) AppleWebKit/420+ (KHTML, like Gecko)";
}
