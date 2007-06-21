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
#include "MimeTypeRegistry.h"
#include "ResourceResponse.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceRequest.h"
#include "HistoryItem.h"
#include "HTMLFormElement.h"
#include "NotImplemented.h"

#include "qwebpage.h"
#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebobjectplugin_p.h"
#include "qwebnetworkinterface_p.h"

#include <qfileinfo.h>

#include "qdebug.h"

#define methodDebug() qDebug("FrameLoaderClientQt: %s loader=%p", __FUNCTION__, (m_frame ? m_frame->loader() : 0))

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

void FrameLoaderClientQt::setFrame(QWebFrame* webFrame, Frame* frame)
{
    m_webFrame = webFrame;
    m_frame = frame;
    if (!m_webFrame || !m_webFrame->page()) {
        qWarning("FrameLoaderClientQt::setFrame frame without Page!");
        return;
    }

    connect(this, SIGNAL(loadStarted(QWebFrame*)),
            m_webFrame->page(), SIGNAL(loadStarted(QWebFrame *)));
    connect(this, SIGNAL(loadProgressChanged(int)),
            m_webFrame->page(), SIGNAL(loadProgressChanged(int)));
    connect(this, SIGNAL(loadFinished(QWebFrame*)),
            m_webFrame->page(), SIGNAL(loadFinished(QWebFrame *)));
    connect(this, SIGNAL(titleChanged(const QString&)),
            m_webFrame, SIGNAL(titleChanged(const QString&)));
}

QWebFrame* FrameLoaderClientQt::webFrame() const
{
    return m_webFrame;
}

void FrameLoaderClientQt::callPolicyFunction(FramePolicyFunction function, PolicyAction action)
{
    ASSERT(!m_policyFunction);
    ASSERT(function);

    m_policyFunction = function;
    emit sigCallPolicyFunction(action);
}

void FrameLoaderClientQt::slotCallPolicyFunction(int action)
{
    if (!m_frame || !m_policyFunction)
        return;
    FramePolicyFunction function = m_policyFunction;
    m_policyFunction = 0;
    (m_frame->loader()->*function)(WebCore::PolicyAction(action));
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
    m_frame->view()->setNeedsLayout();
    m_frame->view()->layout();
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
}


void FrameLoaderClientQt::detachedFromParent4()
{
    if (!m_webFrame)
        return;
    m_webFrame->d->frame = 0;
    m_webFrame->d->frameView = 0;
    m_webFrame = 0;
    m_frame = 0;
}


void FrameLoaderClientQt::loadedFromCachedPage()
{
    notImplemented();
}


void FrameLoaderClientQt::dispatchDidHandleOnloadEvents()
{
    // don't need this one
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
    setTitle(title);
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
    if (m_webFrame)
        emit m_webFrame->loadDone(true);    
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
    Q_ASSERT(!m_policyFunction);
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
        emit loadProgressChanged(qRound(m_frame->page()->progress()->estimatedProgress() * 100));
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
    setTitle(l->title());
}


void FrameLoaderClientQt::finishedLoading(DocumentLoader* loader)
{
    if (m_firstData) {
        FrameLoader *fl = loader->frameLoader();
        fl->setEncoding(m_response.textEncodingName(), false);
        m_firstData = false;
    }
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
    if (MimeTypeRegistry::isSupportedImageMIMEType(MIMEType))
        return true;

    if (MimeTypeRegistry::isSupportedNonImageMIMEType(MIMEType))
        return true;

    return false;
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
    // Note: Can be called multiple times.
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to clear these out.
    m_frame->loader()->setPreviousHistoryItem(0);
}


void FrameLoaderClientQt::restoreViewState()
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
//     notImplemented();
}


void FrameLoaderClientQt::prepareForDataSourceReplacement()
{
    m_frame->loader()->detachChildren();
}


void FrameLoaderClientQt::setTitle(const String& title)
{
    QString t = title;
    emit titleChanged(t);
}


void FrameLoaderClientQt::setTitle(const String& title, const KURL& url)
{
    Q_UNUSED(url)
    setTitle(title);
}


String FrameLoaderClientQt::userAgent(const KURL&)
{
    return "Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3 Qt";
}

void FrameLoaderClientQt::dispatchDidReceiveIcon()
{
    if (m_webFrame) {
        emit m_webFrame->page()->iconLoaded();
    }
}

void FrameLoaderClientQt::frameLoaderDestroyed()
{
    Q_ASSERT(m_webFrame == 0);
    Q_ASSERT(m_frame == 0);
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

void FrameLoaderClientQt::setDocumentViewFromCachedPage(CachedPage*)
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

bool FrameLoaderClientQt::shouldGoToHistoryItem(WebCore::HistoryItem *item) const
{
    if (item) {
        return true;
    }
    return false;
}

void FrameLoaderClientQt::saveViewStateToItem(WebCore::HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientQt::saveDocumentViewToCachedPage(CachedPage*)
{
    notImplemented();
}

bool FrameLoaderClientQt::canCachePage() const
{
    // don't do any caching for now
    return false;
}

void FrameLoaderClientQt::setMainDocumentError(WebCore::DocumentLoader* loader, const WebCore::ResourceError& error)
{
    if (m_firstData) {
        loader->frameLoader()->setEncoding(m_response.textEncodingName(), false);
        m_firstData = false;
    }
}

void FrameLoaderClientQt::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    if (!m_frame)
        return;
    FrameLoader *fl = loader->frameLoader();
    if (m_firstData) {
        fl->setEncoding(m_response.textEncodingName(), false);
        m_firstData = false;
    }
    fl->addData(data, length);
}

WebCore::ResourceError FrameLoaderClientQt::cancelledError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientQt::blockedError(const WebCore::ResourceRequest&)
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

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClientQt::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<DocumentLoader> loader = new DocumentLoader(request, substituteData);
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
    //qDebug() << "FrameLoaderClientQt::dispatchWillSendRequest" << request.isNull() << request.url().url();
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
    //qDebug() << "    got response from" << response.url().url();
}

void FrameLoaderClientQt::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int)
{
}

void FrameLoaderClientQt::dispatchDidFinishLoading(WebCore::DocumentLoader* loader, unsigned long)
{
}

void FrameLoaderClientQt::dispatchDidFailLoading(WebCore::DocumentLoader* loader, unsigned long, const WebCore::ResourceError&)
{
    if (m_firstData) {
        FrameLoader *fl = loader->frameLoader();
        fl->setEncoding(m_response.textEncodingName(), false);
        m_firstData = false;
    }
}

bool FrameLoaderClientQt::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void FrameLoaderClientQt::dispatchDidFailProvisionalLoad(const WebCore::ResourceError&)
{
    if (m_webFrame)
        emit m_webFrame->loadDone(false);    
}

void FrameLoaderClientQt::dispatchDidFailLoad(const WebCore::ResourceError&)
{
    if (m_webFrame)
        emit m_webFrame->loadDone(false);    
}

WebCore::Frame* FrameLoaderClientQt::dispatchCreatePage()
{
    notImplemented();
    return 0;
}

void FrameLoaderClientQt::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WebCore::String&, const WebCore::ResourceRequest&)
{
    // we need to call directly here
    Q_ASSERT(!m_policyFunction);
    m_policyFunction = function;
    slotCallPolicyFunction(PolicyUse);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&)
{
    notImplemented();
    Q_ASSERT(!m_policyFunction);
    callPolicyFunction(function, PolicyIgnore);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest& request)
{
    Q_ASSERT(!m_policyFunction);
    m_policyFunction = function;
    if (m_webFrame) {
        QWebNetworkRequest r(request);
        QWebPage *page = m_webFrame->page();

        if (page->d->navigationRequested(m_webFrame, r) == QWebPage::IgnoreNavigationRequest) {
            slotCallPolicyFunction(PolicyIgnore);
            return;
        }
    }
    slotCallPolicyFunction(PolicyUse);
    return;
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
                             String(), 0, 0);

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.get();
}

ObjectContentType FrameLoaderClientQt::objectContentType(const KURL& url, const String& _mimeType)
{
    //qDebug()<<" ++++++++++++++++ url is "<<url.prettyURL()<<", mime = "<<mimeType;
    if (!url.isValid())
        return ObjectContentNone;

    String mimeType = _mimeType;
    if (!mimeType.length()) {
        QFileInfo fi(url.path());
        mimeType = MimeTypeRegistry::getMIMETypeForExtension(fi.suffix());
    }

    if (!mimeType.length())
        return ObjectContentFrame;

    if (MimeTypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentImage;

    if (QWebFactoryLoader::self()->supportsMimeType(mimeType))
        return ObjectContentPlugin;

    if (MimeTypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentFrame;
    
    return ObjectContentNone;
}

Widget* FrameLoaderClientQt::createPlugin(Element* element, const KURL& url, const Vector<String>& paramNames,
                                          const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    //qDebug()<<"------ Creating plugin in FrameLoaderClientQt::createPlugin for "<<mimeType;
    //qDebug()<<"------\t url = "<<url.prettyURL();
    QStringList params;
    QStringList values;
    for (int i = 0; i < paramNames.size(); ++i)
        params.append(paramNames[i]);
    for (int i = 0; i < paramValues.size(); ++i) 
        values.append(paramValues[i]);

    QUrl qurl = QString(url.url());
    
    QObject *object = QWebFactoryLoader::self()->create(m_webFrame, qurl, mimeType, params, values);
    if (object) {
        QWidget *widget = qobject_cast<QWidget *>(object);
        if (widget) {
            Widget* w= new Widget();
            w->setQWidget(widget);
            return w;
        }
        // FIXME: make things work for widgetless plugins as well
        delete object;
    }

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

QString FrameLoaderClientQt::chooseFile(const QString& oldFile)
{
    return webFrame()->page()->chooseFile(webFrame(), oldFile);
}

}
