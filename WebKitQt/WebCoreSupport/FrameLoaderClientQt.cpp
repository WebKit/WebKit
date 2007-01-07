/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "FrameLoaderClientQt.h"
#include "DocumentLoader.h"

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

namespace WebCore
{

FrameLoaderClientQt::FrameLoaderClientQt()
{
    notImplemented();
}


FrameLoaderClientQt::~FrameLoaderClientQt()
{
    notImplemented();
}


void FrameLoaderClientQt::detachFrameLoader()
{
    notImplemented();
}

void FrameLoaderClientQt::ref()
{
    Shared<FrameLoaderClientQt>::ref();
}

void FrameLoaderClientQt::deref()
{
    Shared<FrameLoaderClientQt>::deref();
}

bool FrameLoaderClientQt::hasWebView() const
{
    //notImplemented();
    return true;
}


bool FrameLoaderClientQt::hasFrameView() const
{
    //notImplemented();
    return true;
}


bool FrameLoaderClientQt::hasBackForwardList() const
{
    notImplemented();
    return false;
}


void FrameLoaderClientQt::resetBackForwardList()
{
    notImplemented();
}


bool FrameLoaderClientQt::provisionalItemIsTarget() const
{
    notImplemented();
    return false;
}


bool FrameLoaderClientQt::loadProvisionalItemFromPageCache()
{
    notImplemented();
    return false;
}


void FrameLoaderClientQt::invalidateCurrentItemPageCache()
{
    notImplemented();
}


bool FrameLoaderClientQt::privateBrowsingEnabled() const
{
    notImplemented();
    return false;
}


void FrameLoaderClientQt::makeDocumentView()
{
    notImplemented();
}


void FrameLoaderClientQt::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::forceLayout()
{
    notImplemented();
}


void FrameLoaderClientQt::forceLayoutForNonHTML()
{
    notImplemented();
}


void FrameLoaderClientQt::updateHistoryForCommit()
{
    notImplemented();
}


void FrameLoaderClientQt::updateHistoryForBackForwardNavigation()
{
    notImplemented();
}


void FrameLoaderClientQt::updateHistoryForReload()
{
    notImplemented();
}


void FrameLoaderClientQt::updateHistoryForStandardLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::updateHistoryForInternalLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::updateHistoryAfterClientRedirect()
{
    notImplemented();
}


void FrameLoaderClientQt::setCopiesOnScroll()
{
    notImplemented();
}


LoadErrorResetToken* FrameLoaderClientQt::tokenForLoadErrorReset()
{
    notImplemented();
    return 0;
}


void FrameLoaderClientQt::resetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}


void FrameLoaderClientQt::doNotResetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}


void FrameLoaderClientQt::willCloseDocument()
{
    notImplemented();
}


void FrameLoaderClientQt::detachedFromParent1()
{
    notImplemented();
}


void FrameLoaderClientQt::detachedFromParent2()
{
    notImplemented();
}


void FrameLoaderClientQt::detachedFromParent3()
{
    notImplemented();
}


void FrameLoaderClientQt::detachedFromParent4()
{
    notImplemented();
}


void FrameLoaderClientQt::loadedFromPageCache()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidHandleOnloadEvents()
{
    //notImplemented();
}


void FrameLoaderClientQt::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidCancelClientRedirect()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchWillPerformClientRedirect(const KURL&,
                                                            double interval,
                                                            double fireDate)
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchWillClose()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidStartProvisionalLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidReceiveTitle(const String& title)
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidCommitLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidFinishLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidFirstLayout()
{
    //notImplemented();
}


void FrameLoaderClientQt::dispatchShow()
{
    notImplemented();
}


void FrameLoaderClientQt::cancelPolicyCheck()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchWillSubmitForm(FramePolicyFunction,
                                                 PassRefPtr<FormState>)
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::clearLoadingFromPageCache(DocumentLoader*)
{
    notImplemented();
}


bool FrameLoaderClientQt::isLoadingFromPageCache(DocumentLoader*)
{
    notImplemented();
    return false;
}


void FrameLoaderClientQt::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::clearUnarchivingState(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::progressStarted()
{
    notImplemented();
}


void FrameLoaderClientQt::progressCompleted()
{
    notImplemented();
}


void FrameLoaderClientQt::setMainFrameDocumentReady(bool)
{
    notImplemented();
}


void FrameLoaderClientQt::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::didChangeTitle(DocumentLoader *l)
{
    setTitle(l->title(), l->URL());
}


void FrameLoaderClientQt::finishedLoading(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::finalSetupForReplace(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::setDefersLoading(bool)
{
    notImplemented();
}


bool FrameLoaderClientQt::isArchiveLoadPending(ResourceLoader*) const
{
    notImplemented();
    return false;
}


void FrameLoaderClientQt::cancelPendingArchiveLoad(ResourceLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::clearArchivedResources()
{
    notImplemented();
}


bool FrameLoaderClientQt::canShowMIMEType(const String& MIMEType) const
{
    notImplemented();
    return false;
}


bool FrameLoaderClientQt::representationExistsForURLScheme(
    const String& URLScheme) const
{
    notImplemented();
    return false;
}


String FrameLoaderClientQt::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return String();
}


void FrameLoaderClientQt::frameLoadCompleted()
{
    notImplemented();
}


void FrameLoaderClientQt::restoreScrollPositionAndViewState()
{
    notImplemented();
}


void FrameLoaderClientQt::provisionalLoadStarted()
{
    notImplemented();
}


bool FrameLoaderClientQt::shouldTreatURLAsSameAsCurrent(const KURL&) const
{
    notImplemented();
    return false;
}


void FrameLoaderClientQt::addHistoryItemForFragmentScroll()
{
    notImplemented();
}


void FrameLoaderClientQt::didFinishLoad()
{
    notImplemented();
}


void FrameLoaderClientQt::prepareForDataSourceReplacement()
{
    notImplemented();
}


void FrameLoaderClientQt::setTitle(const String& title, const KURL&)
{
    //notImplemented();
}


String FrameLoaderClientQt::userAgent()
{
    return "Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3 Qt";
}

void FrameLoaderClientQt::dispatchDidReceiveIcon()
{
    notImplemented();
}

void FrameLoaderClientQt::frameLoaderDestroyed()
{
    notImplemented();
}

bool FrameLoaderClientQt::canHandleRequest(const WebCore::ResourceRequest&) const
{
    notImplemented();
}

void FrameLoaderClientQt::partClearedInBegin()
{
    notImplemented();
}

void FrameLoaderClientQt::setDocumentViewFromPageCache(WebCore::PageCache*)
{
    notImplemented();
}

void FrameLoaderClientQt::updateGlobalHistoryForStandardLoad(const WebCore::KURL&)
{
    notImplemented();
}

void FrameLoaderClientQt::updateGlobalHistoryForReload(const WebCore::KURL&)
{
    notImplemented();
}

bool FrameLoaderClientQt::shouldGoToHistoryItem(WebCore::HistoryItem*) const
{
    notImplemented();
}

void FrameLoaderClientQt::saveScrollPositionAndViewStateToItem(WebCore::HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientQt::saveDocumentViewToPageCache(WebCore::PageCache*)
{
    notImplemented();
}

bool FrameLoaderClientQt::canCachePage() const
{
    notImplemented();
}

void FrameLoaderClientQt::setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientQt::committedLoad(WebCore::DocumentLoader*, const char*, int)
{
    notImplemented();
}

WebCore::ResourceError FrameLoaderClientQt::cancelledError(const WebCore::ResourceRequest&)
{
    notImplemented();
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowURLError(const WebCore::ResourceRequest&)
{
    notImplemented();
}

WebCore::ResourceError FrameLoaderClientQt::interruptForPolicyChangeError(const WebCore::ResourceRequest&)
{
    notImplemented();
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowMIMETypeError(const WebCore::ResourceResponse&)
{
    notImplemented();
}

WebCore::ResourceError FrameLoaderClientQt::fileDoesNotExistError(const WebCore::ResourceResponse&)
{
    notImplemented();
}

bool FrameLoaderClientQt::shouldFallBack(const WebCore::ResourceError&)
{
    notImplemented();
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClientQt::createDocumentLoader(const WebCore::ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientQt::download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchWillSendRequest(WebCore::DocumentLoader*, void*, WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidReceiveResponse(WebCore::DocumentLoader*, void*, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, void*, int)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidFinishLoading(WebCore::DocumentLoader*, void*)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidFailLoading(WebCore::DocumentLoader*, void*, const WebCore::ResourceError&)
{
    notImplemented();
}

bool FrameLoaderClientQt::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidFailProvisionalLoad(const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidFailLoad(const WebCore::ResourceError&)
{
    notImplemented();
}

WebCore::Frame* FrameLoaderClientQt::dispatchCreatePage()
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDecidePolicyForMIMEType(void (WebCore::FrameLoader::*)(WebCore::PolicyAction), const WebCore::String&, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(void (WebCore::FrameLoader::*)(WebCore::PolicyAction), const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDecidePolicyForNavigationAction(void (WebCore::FrameLoader::*)(WebCore::PolicyAction), const WebCore::NavigationAction&, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchUnableToImplementPolicy(const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientQt::incrementProgress(void*, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientQt::incrementProgress(void*, const char*, int)
{
    notImplemented();
}

void FrameLoaderClientQt::completeProgress(void*)
{
    notImplemented();
}

void FrameLoaderClientQt::startDownload(const WebCore::ResourceRequest&)
{
    notImplemented();
}

bool FrameLoaderClientQt::willUseArchive(WebCore::ResourceLoader*, const WebCore::ResourceRequest&, const WebCore::KURL&) const
{
    notImplemented();
}


}


