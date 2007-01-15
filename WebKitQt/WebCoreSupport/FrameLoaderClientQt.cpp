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
#include "ResourceResponse.h"
#include "qdebug.h"

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)


namespace WebCore
{

FrameLoaderClientQt::FrameLoaderClientQt()
    : m_frame(0)
{
}


FrameLoaderClientQt::~FrameLoaderClientQt()
{
}

void FrameLoaderClientQt::setFrame(FrameQt *frame)
{
    m_frame = frame;
}

void FrameLoaderClientQt::detachFrameLoader()
{
    m_frame = 0;
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
    qDebug() << "FrameLoaderClientQt::makeDocumentView" << m_frame->document();
    
//     if (!m_frame->document()) 
//         m_frame->loader()->createEmptyDocument();
}


void FrameLoaderClientQt::makeRepresentation(DocumentLoader*)
{
    // don't need this for now I think.
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
    // apparently mac specific 
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
    // we're not interested in this neither I think
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
    // don't need to do anything here as long as we don't start doing asyncronous policy checks
}


void FrameLoaderClientQt::dispatchWillSubmitForm(FramePolicyFunction function,
                                                 PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    if (!m_frame)
        return;
    (m_frame->loader()->*function)(PolicyUse);
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


void FrameLoaderClientQt::postProgressStartedNotification()
{
    // no progress notification for now
}

void FrameLoaderClientQt::postProgressEstimateChangedNotification()
{
    // no progress notification for now    
}

void FrameLoaderClientQt::postProgressFinishedNotification()
{
    // no progress notification for now
}

void FrameLoaderClientQt::setMainFrameDocumentReady(bool b)
{
    // this is only interesting once we provide an external API for the DOM
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
    // don't think we need to do anything here currently
}


bool FrameLoaderClientQt::canShowMIMEType(const String& MIMEType) const
{
    // FIXME: This is not good enough in the general case
    qDebug() << "FrameLoaderClientQt::canShowMIMEType" << MIMEType;
    return true;
}


bool FrameLoaderClientQt::representationExistsForURLScheme(const String& URLScheme) const
{
    notImplemented();
    qDebug() << "    scheme is" << URLScheme;
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
    // don't need to do anything here
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
    m_frame->loader()->detachChildren();
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
    m_frame = 0;
    delete this;
}

bool FrameLoaderClientQt::canHandleRequest(const WebCore::ResourceRequest&) const
{
    return true;
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
    return false;
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
    // don't do any caching for now
    return false;
}

void FrameLoaderClientQt::setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientQt::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    qDebug() << "FrameLoaderClientQt::committedLoad" << length;
    if (!m_frame)
        return;
    FrameLoader *fl = loader->frameLoader();
    fl->setEncoding(m_response.textEncodingName(), false);
    fl->addData(data, length);
}

WebCore::ResourceError FrameLoaderClientQt::cancelledError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowURLError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientQt::interruptForPolicyChangeError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowMIMETypeError(const WebCore::ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientQt::fileDoesNotExistError(const WebCore::ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

bool FrameLoaderClientQt::shouldFallBack(const WebCore::ResourceError&)
{
    notImplemented();
    return false;
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClientQt::createDocumentLoader(const WebCore::ResourceRequest& request)
{
    RefPtr<DocumentLoader> loader = new DocumentLoader(request);
    return loader.release();
}

void FrameLoaderClientQt::download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientQt::assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&)
{
    notImplemented();   
}

void FrameLoaderClientQt::dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long, WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{
    // seems like the Mac code doesn't do anything here by default neither
    qDebug() << "FrameLoaderClientQt::dispatchWillSendRequest" << request.isNull() << request.url().url();
}

void FrameLoaderClientQt::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceResponse& response)
{

    m_response = response;
    m_firstData = true;
    qDebug() << "    got response from" << response.url().url();
}

void FrameLoaderClientQt::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long)
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceError&)
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

void FrameLoaderClientQt::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WebCore::String&, const WebCore::ResourceRequest&)
{
    if (!m_frame)
        return;
    // FIXME: This is maybe too simple
    (m_frame->loader()->*function)(PolicyUse);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&)
{
    if (!m_frame)
        return;
    // FIXME: This is maybe too simple
    (m_frame->loader()->*function)(PolicyIgnore);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&)
{
    if (!m_frame)
        return;
    // FIXME: This is maybe too simple
    (m_frame->loader()->*function)(PolicyUse);
}

void FrameLoaderClientQt::dispatchUnableToImplementPolicy(const WebCore::ResourceError&)
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
    return false;
}

}


