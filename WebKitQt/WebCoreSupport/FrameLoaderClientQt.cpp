/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
#include "FrameTree.h"
#include "FrameView.h"
#include "DocumentLoader.h"
#include "ResourceResponse.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceRequest.h"

#include "qwebpage.h"
#include "qwebframe.h"
#include "qwebframe_p.h"

#include "qdebug.h"

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore
{

FrameLoaderClientQt::FrameLoaderClientQt()
    : m_frame(0)
    , m_webFrame(0)
    , m_firstData(false)
    , m_policyFunction(0)
{
    connect(this, SIGNAL(sigCallPolicyFunction(int)), this, SLOT(slotCallPolicyFunction(int)), Qt::QueuedConnection);
}


FrameLoaderClientQt::~FrameLoaderClientQt()
{
}

void FrameLoaderClientQt::setFrame(QWebFrame *webFrame, FrameQt *frame)
{
    m_webFrame = webFrame;
    m_frame = frame;
    if (!m_webFrame || !m_webFrame->page()) {
        qWarning("FrameLoaderClientQt::setFrame frame without Page!");
        return;
    }

    connect(this, SIGNAL(loadStarted(QWebFrame*)),
            m_webFrame->page(), SIGNAL(loadStarted(QWebFrame *)));
    connect(this, SIGNAL(loadProgressChanged(double)),
            m_webFrame->page(), SIGNAL(loadProgressChanged(double)));
    connect(this, SIGNAL(loadFinished(QWebFrame*)),
            m_webFrame->page(), SIGNAL(loadFinished(QWebFrame *)));
}

void FrameLoaderClientQt::detachFrameLoader()
{
    disconnect(this, SIGNAL(loadStarted(QWebFrame*)));
    disconnect(this, SIGNAL(loadProgressChanged(double)));
    disconnect(this, SIGNAL(loadFinished(QWebFrame*)));
    m_webFrame = 0;
    m_frame = 0;
}

void FrameLoaderClientQt::callPolicyFunction(FramePolicyFunction function, PolicyAction action)
{
    qDebug() << "FrameLoaderClientQt::callPolicyFunction";
    ASSERT(!m_policyFunction);

    m_policyFunction = function;
    emit sigCallPolicyFunction(action);
}

void FrameLoaderClientQt::slotCallPolicyFunction(int action)
{
    qDebug() << "FrameLoaderClientQt::slotCallPolicyFunction";
    if (!m_frame || !m_policyFunction)
        return;
    (m_frame->loader()->*m_policyFunction)(WebCore::PolicyAction(action));
    m_policyFunction = 0;
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

void FrameLoaderClientQt::detachedFromParent2()
{
    notImplemented();
}


void FrameLoaderClientQt::detachedFromParent3()
{
    if (!m_webFrame)
        return;
    if (m_webFrame->d->frameView)
        m_webFrame->d->frameView->setScrollArea(0);
    m_webFrame->d->frameView = 0;
}


void FrameLoaderClientQt::detachedFromParent4()
{
    delete m_webFrame;
    m_webFrame = 0;
    m_frame = 0;
}


void FrameLoaderClientQt::loadedFromPageCache()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidHandleOnloadEvents()
{
    if (m_webFrame)
        emit m_webFrame->loadDone();
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


void FrameLoaderClientQt::dispatchDidFinishDocumentLoad()
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
    qDebug() << "FrameLoaderClientQt::cancelPolicyCheck";
    m_policyFunction = 0;
}


void FrameLoaderClientQt::dispatchWillSubmitForm(FramePolicyFunction function,
                                                 PassRefPtr<FormState>)
{
    notImplemented();
    // FIXME: This is surely too simple
    callPolicyFunction(function, PolicyUse);
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
    if (m_webFrame && m_frame->page())
        emit loadStarted(m_webFrame);
}

void FrameLoaderClientQt::postProgressEstimateChangedNotification()
{
    if (m_webFrame && m_frame->page())
        emit loadProgressChanged(m_frame->page()->progress()->estimatedProgress());
}

void FrameLoaderClientQt::postProgressFinishedNotification()
{
    if (m_webFrame && m_frame->page())
        emit loadFinished(m_webFrame);
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
    m_webFrame = 0;
    m_frame = 0;
    delete this;
}

bool FrameLoaderClientQt::canHandleRequest(const WebCore::ResourceRequest&) const
{
    return true;
}

void FrameLoaderClientQt::windowObjectCleared() const
{
    if (m_webFrame)
        emit m_webFrame->cleared();
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
    // we need to call directly here
    m_policyFunction = function;
    slotCallPolicyFunction(PolicyUse);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&)
{
    notImplemented();
    callPolicyFunction(function, PolicyIgnore);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&)
{
    notImplemented();
    callPolicyFunction(function, PolicyUse);
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

Frame* FrameLoaderClientQt::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    qDebug() << ">>>>>>>>>>> createFrame";
    QWebFrameData frameData;
    frameData.url = url;
    frameData.name = name;
    frameData.ownerElement = ownerElement;
    frameData.referrer = referrer;
    frameData.allowsScrolling = allowsScrolling;
    frameData.marginWidth = marginWidth;
    frameData.marginHeight = marginHeight;
        

    QWebFrame* webFrame = m_webFrame->page()->createFrame(m_webFrame, &frameData);

    RefPtr<Frame> childFrame = webFrame->d->frame;

    // FIXME: All of the below should probably be moved over into WebCore
    childFrame->tree()->setName(name);
    m_frame->tree()->appendChild(childFrame);
    // ### set override encoding if we have one

    FrameLoadType loadType = m_frame->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeInternal;

    childFrame->loader()->load(frameData.url, frameData.referrer, childLoadType,
                             String(), 0, 0, WTF::HashMap<String, String>());
    
    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;
    
    return childFrame.get();
}

ObjectContentType FrameLoaderClientQt::objectContentType(const KURL& url, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

Widget* FrameLoaderClientQt::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    notImplemented();
    return 0;
}

void FrameLoaderClientQt::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
    return;
}

Widget* FrameLoaderClientQt::createJavaAppletWidget(const IntSize&, Element*, const KURL& baseURL,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

String FrameLoaderClientQt::overrideMediaType() const
{
    notImplemented();
    return String();
}

}

#include "FrameLoaderClientQt.moc"
