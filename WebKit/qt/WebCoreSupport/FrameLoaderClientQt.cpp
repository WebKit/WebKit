/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Coypright (C) 2008 Holger Hans Peter Freyther
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
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "FormState.h"
#include "FrameLoaderClientQt.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "DocumentLoader.h"
#include "MIMETypeRegistry.h"
#include "ResourceResponse.h"
#include "Page.h"
#include "PluginData.h"
#include "PluginDatabase.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "ResourceRequest.h"
#include "HistoryItem.h"
#include "HTMLAppletElement.h"
#include "HTMLFormElement.h"
#include "HTMLPlugInElement.h"
#include "NotImplemented.h"
#include "QNetworkReplyHandler.h"
#include "ResourceHandleInternal.h"
#include "ResourceHandle.h"
#include "Settings.h"
#include "ScriptString.h"
#include "QWebPageClient.h"

#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebhistoryinterface.h"
#include "qwebpluginfactory.h"

#include <qfileinfo.h>

#include <QCoreApplication>
#include <QDebug>
#if QT_VERSION >= 0x040400
#include <QGraphicsScene>
#include <QGraphicsWidget>
#include <QNetworkRequest>
#include <QNetworkReply>
#else
#include "qwebnetworkinterface_p.h"
#endif
#include "qwebhistory_p.h"

static bool dumpFrameLoaderCallbacks = false;
static bool dumpResourceLoadCallbacks = false;

static QMap<unsigned long, QString> dumpAssignedUrls;

void QWEBKIT_EXPORT qt_dump_frame_loader(bool b)
{
    dumpFrameLoaderCallbacks = b;
}

void QWEBKIT_EXPORT qt_dump_resource_load_callbacks(bool b)
{
    dumpResourceLoadCallbacks = b;
}

// Compare with WebKitTools/DumpRenderTree/mac/FrameLoadDelegate.mm
static QString drtDescriptionSuitableForTestResult(WebCore::Frame* _frame)
{
    QWebFrame* frame = QWebFramePrivate::kit(_frame);
    QString name = frame->frameName();

    bool isMainFrame = frame == frame->page()->mainFrame();
    if (isMainFrame) {
        if (!name.isEmpty())
            return QString::fromLatin1("main frame \"%1\"").arg(name);
        return QLatin1String("main frame");
    } else {
        if (!name.isEmpty())
            return QString::fromLatin1("frame \"%1\"").arg(name);
        return QLatin1String("frame (anonymous)");
    }
}

static QString drtDescriptionSuitableForTestResult(const WebCore::KURL& _url)
{
    QUrl url = _url;
    return url.toString();
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceError& error)
{
    QString failingURL = error.failingURL();
    return QString::fromLatin1("<NSError domain NSURLErrorDomain, code %1, failing URL \"%2\">").arg(error.errorCode()).arg(failingURL);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceRequest& request)
{
    QString url = request.url().string();
    return QString::fromLatin1("<NSURLRequest %1>").arg(url);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceResponse& response)
{
    QString text = response.httpStatusText();
    if (text.isEmpty())
        return QLatin1String("(null)");

    return text;
}


namespace WebCore
{

FrameLoaderClientQt::FrameLoaderClientQt()
    : m_frame(0)
    , m_webFrame(0)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_firstData(false)
    , m_loadError (ResourceError())
{
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

    connect(this, SIGNAL(loadStarted()),
            m_webFrame->page(), SIGNAL(loadStarted()));
    connect(this, SIGNAL(loadStarted()),
            m_webFrame, SIGNAL(loadStarted()));
    connect(this, SIGNAL(loadProgress(int)),
            m_webFrame->page(), SIGNAL(loadProgress(int)));
    connect(this, SIGNAL(loadFinished(bool)),
            m_webFrame->page(), SIGNAL(loadFinished(bool)));
    connect(this, SIGNAL(loadFinished(bool)),
            m_webFrame, SIGNAL(loadFinished(bool)));
    connect(this, SIGNAL(titleChanged(const QString&)),
            m_webFrame, SIGNAL(titleChanged(const QString&)));
}

QWebFrame* FrameLoaderClientQt::webFrame() const
{
    return m_webFrame;
}

void FrameLoaderClientQt::callPolicyFunction(FramePolicyFunction function, PolicyAction action)
{
    (m_frame->loader()->*function)(action);
}

bool FrameLoaderClientQt::hasWebView() const
{
    //notImplemented();
    return true;
}

void FrameLoaderClientQt::savePlatformDataToCachedFrame(CachedFrame*) 
{
    notImplemented();
}

void FrameLoaderClientQt::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

void FrameLoaderClientQt::transitionToCommittedForNewPage()
{
    ASSERT(m_frame);
    ASSERT(m_webFrame);

    QBrush brush = m_webFrame->page()->palette().brush(QPalette::Base);
    QColor backgroundColor = brush.style() == Qt::SolidPattern ? brush.color() : QColor();

    QWebPage* page = m_webFrame->page();
    const QSize fixedLayoutSize = page->fixedContentsSize();

    m_frame->createView(m_webFrame->page()->viewportSize(),
                        backgroundColor, !backgroundColor.alpha(),
                        fixedLayoutSize.isValid() ? IntSize(fixedLayoutSize) : IntSize(),
                        fixedLayoutSize.isValid(),
                        (ScrollbarMode)m_webFrame->scrollBarPolicy(Qt::Horizontal),
                        (ScrollbarMode)m_webFrame->scrollBarPolicy(Qt::Vertical));
}


void FrameLoaderClientQt::makeRepresentation(DocumentLoader*)
{
    // don't need this for now I think.
}


void FrameLoaderClientQt::forceLayout()
{
    FrameView* view = m_frame->view();
    if (view)
        view->forceLayout(true);
}


void FrameLoaderClientQt::forceLayoutForNonHTML()
{
}


void FrameLoaderClientQt::setCopiesOnScroll()
{
    // apparently mac specific
}


void FrameLoaderClientQt::detachedFromParent2()
{
}


void FrameLoaderClientQt::detachedFromParent3()
{
}

void FrameLoaderClientQt::dispatchDidHandleOnloadEvents()
{
    // don't need this one
    if (dumpFrameLoaderCallbacks)
        printf("%s - didHandleOnloadEventsForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

}


void FrameLoaderClientQt::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didReceiveServerRedirectForProvisionalLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    notImplemented();
}


void FrameLoaderClientQt::dispatchDidCancelClientRedirect()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didCancelClientRedirectForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    notImplemented();
}


void FrameLoaderClientQt::dispatchWillPerformClientRedirect(const KURL& url, double, double)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - willPerformClientRedirectToURL: %s \n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)), qPrintable(drtDescriptionSuitableForTestResult(url)));

    notImplemented();
}


void FrameLoaderClientQt::dispatchDidChangeLocationWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didChangeLocationWithinPageForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (!m_webFrame)
        return;

    emit m_webFrame->urlChanged(m_webFrame->url());
    m_webFrame->page()->d->updateNavigationActions();
}


void FrameLoaderClientQt::dispatchWillClose()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - willCloseFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));
}


void FrameLoaderClientQt::dispatchDidStartProvisionalLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didStartProvisionalLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_webFrame)
        emit m_webFrame->provisionalLoad();
}


void FrameLoaderClientQt::dispatchDidReceiveTitle(const String& title)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didReceiveTitle: %s\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)), qPrintable(QString(title)));

    if (!m_webFrame)
        return;

    emit titleChanged(title);
}


void FrameLoaderClientQt::dispatchDidCommitLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didCommitLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_frame->tree()->parent() || !m_webFrame)
        return;

    emit m_webFrame->urlChanged(m_webFrame->url());
    m_webFrame->page()->d->updateNavigationActions();

    // We should assume first the frame has no title. If it has, then the above dispatchDidReceiveTitle()
    // will be called very soon with the correct title.
    // This properly resets the title when we navigate to a URI without a title.
    emit titleChanged(String());
}


void FrameLoaderClientQt::dispatchDidFinishDocumentLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFinishDocumentLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (QWebPagePrivate::drtRun) {
        int unloadEventCount = m_frame->domWindow()->pendingUnloadEventListeners();
        if (unloadEventCount)
            printf("%s - has %u onunload handler(s)\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)), unloadEventCount);
    }

    if (m_frame->tree()->parent() || !m_webFrame)
        return;

    m_webFrame->page()->d->updateNavigationActions();
}


void FrameLoaderClientQt::dispatchDidFinishLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFinishLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    m_loadError = ResourceError(); // clears the previous error

    if (!m_webFrame)
        return;
    m_webFrame->page()->d->updateNavigationActions();
}


void FrameLoaderClientQt::dispatchDidFirstLayout()
{
    if (m_webFrame)
        emit m_webFrame->initialLayoutCompleted();
}

void FrameLoaderClientQt::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notImplemented();
}

void FrameLoaderClientQt::dispatchShow()
{
    notImplemented();
}


void FrameLoaderClientQt::cancelPolicyCheck()
{
//    qDebug() << "FrameLoaderClientQt::cancelPolicyCheck";
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
}


void FrameLoaderClientQt::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientQt::postProgressStartedNotification()
{
    if (m_webFrame && m_frame->page()) {
        emit loadStarted();
        postProgressEstimateChangedNotification();
    }
    if (m_frame->tree()->parent() || !m_webFrame)
        return;
    m_webFrame->page()->d->updateNavigationActions();
}

void FrameLoaderClientQt::postProgressEstimateChangedNotification()
{
    if (m_webFrame && m_frame->page())
        emit loadProgress(qRound(m_frame->page()->progress()->estimatedProgress() * 100));
}

void FrameLoaderClientQt::postProgressFinishedNotification()
{
    // send a mousemove event to
    // (1) update the cursor to change according to whatever is underneath the mouse cursor right now
    // (2) display the tool tip if the mouse hovers a node which has a tool tip
    if (m_frame && m_frame->eventHandler() && m_webFrame->page()) {
        QWidget* view = m_webFrame->page()->view();
        if (view && view->hasFocus()) {
            QPoint localPos = view->mapFromGlobal(QCursor::pos());
            if (view->rect().contains(localPos)) {
                QMouseEvent event(QEvent::MouseMove, localPos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                m_frame->eventHandler()->mouseMoved(PlatformMouseEvent(&event, 0));
            }
        }
    }

    if (m_webFrame && m_frame->page())
        emit loadFinished(m_loadError.isNull());
}

void FrameLoaderClientQt::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}


void FrameLoaderClientQt::willChangeTitle(DocumentLoader*)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}


void FrameLoaderClientQt::didChangeTitle(DocumentLoader *)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}


void FrameLoaderClientQt::finishedLoading(DocumentLoader* loader)
{
    if (!m_pluginView) {
        if(m_firstData) {
            FrameLoader *fl = loader->frameLoader();
            fl->setEncoding(m_response.textEncodingName(), false);
            m_firstData = false; 
        }
    }
    else {
        m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}


bool FrameLoaderClientQt::canShowMIMEType(const String& MIMEType) const
{
    if (MIMETypeRegistry::isSupportedImageMIMEType(MIMEType))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(MIMEType))
        return true;

    if (m_frame && m_frame->settings()  && m_frame->settings()->arePluginsEnabled()
        && PluginDatabase::installedPlugins()->isMIMETypeRegistered(MIMEType))
        return true;

    return false;
}

bool FrameLoaderClientQt::representationExistsForURLScheme(const String&) const
{
    return false;
}


String FrameLoaderClientQt::generatedMIMETypeForURLScheme(const String&) const
{
    notImplemented();
    return String();
}


void FrameLoaderClientQt::frameLoadCompleted()
{
    // Note: Can be called multiple times.
}


void FrameLoaderClientQt::restoreViewState()
{
    if (!m_webFrame)
        return;
    emit m_webFrame->page()->restoreFrameStateRequested(m_webFrame);
}


void FrameLoaderClientQt::provisionalLoadStarted()
{
    // don't need to do anything here
}


void FrameLoaderClientQt::didFinishLoad()
{
//     notImplemented();
}


void FrameLoaderClientQt::prepareForDataSourceReplacement()
{
}

void FrameLoaderClientQt::setTitle(const String&, const KURL&)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}


String FrameLoaderClientQt::userAgent(const KURL& url)
{
    if (m_webFrame) {
        return m_webFrame->page()->userAgentForUrl(url);
    }
    return String();
}

void FrameLoaderClientQt::dispatchDidReceiveIcon()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didReceiveIconForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_webFrame) {
        emit m_webFrame->iconChanged();
    }
}

void FrameLoaderClientQt::frameLoaderDestroyed()
{
    delete m_webFrame;
    m_frame = 0;
    m_webFrame = 0;

    delete this;
}

bool FrameLoaderClientQt::canHandleRequest(const WebCore::ResourceRequest&) const
{
    return true;
}

void FrameLoaderClientQt::windowObjectCleared()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didClearWindowObjectForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_webFrame)
        emit m_webFrame->javaScriptWindowObjectCleared();
}

void FrameLoaderClientQt::documentElementAvailable()
{
    return;
}

void FrameLoaderClientQt::didPerformFirstNavigation() const
{
    if (m_frame->tree()->parent() || !m_webFrame)
        return;
    m_webFrame->page()->d->updateNavigationActions();
}

void FrameLoaderClientQt::registerForIconNotification(bool)
{
    notImplemented();
}

void FrameLoaderClientQt::updateGlobalHistory()
{
    QWebHistoryInterface *history = QWebHistoryInterface::defaultInterface();
    if (history)
        history->addHistoryEntry(m_frame->loader()->documentLoader()->urlForHistory().prettyURL());
}

void FrameLoaderClientQt::updateGlobalHistoryRedirectLinks()
{
}

bool FrameLoaderClientQt::shouldGoToHistoryItem(WebCore::HistoryItem *) const
{
    return true;
}

void FrameLoaderClientQt::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientQt::didRunInsecureContent(WebCore::SecurityOrigin*)
{
    notImplemented();
}

void FrameLoaderClientQt::saveViewStateToItem(WebCore::HistoryItem* item)
{
    QWebHistoryItem historyItem(new QWebHistoryItemPrivate(item));
    emit m_webFrame->page()->saveFrameStateRequested(m_webFrame, &historyItem);
}

bool FrameLoaderClientQt::canCachePage() const
{
    return true;
}

void FrameLoaderClientQt::setMainDocumentError(WebCore::DocumentLoader* loader, const WebCore::ResourceError& error)
{
    if (!m_pluginView) {
        if (m_firstData) {
            loader->frameLoader()->setEncoding(m_response.textEncodingName(), false);
            m_firstData = false;
        }
    } else {
        m_pluginView->didFail(error);
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

void FrameLoaderClientQt::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView) {
        if (!m_frame)
            return;
        FrameLoader *fl = loader->frameLoader();
        if (m_firstData) {
            fl->setEncoding(m_response.textEncodingName(), false);
            m_firstData = false;
        }
        fl->addData(data, length);
    }
    
    // We re-check here as the plugin can have been created
    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            // didReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
            // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
            // to null
            if (!m_pluginView)
                return;
            m_hasSentResponseToPlugin = true;
        }
        m_pluginView->didReceiveData(data, length);
    }
}

WebCore::ResourceError FrameLoaderClientQt::cancelledError(const WebCore::ResourceRequest& request)
{
    ResourceError error = ResourceError("QtNetwork", QNetworkReply::OperationCanceledError, request.url().prettyURL(),
            QCoreApplication::translate("QWebFrame", "Request cancelled", 0, QCoreApplication::UnicodeUTF8));
    error.setIsCancellation(true);
    return error;
}

// copied from WebKit/Misc/WebKitErrors[Private].h
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort = 103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
};

WebCore::ResourceError FrameLoaderClientQt::blockedError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKit", WebKitErrorCannotUseRestrictedPort, request.url().prettyURL(),
            QCoreApplication::translate("QWebFrame", "Request blocked", 0, QCoreApplication::UnicodeUTF8));
}


WebCore::ResourceError FrameLoaderClientQt::cannotShowURLError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKit", WebKitErrorCannotShowURL, request.url().string(),
            QCoreApplication::translate("QWebFrame", "Cannot show URL", 0, QCoreApplication::UnicodeUTF8));
}

WebCore::ResourceError FrameLoaderClientQt::interruptForPolicyChangeError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKit", WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().string(),
            QCoreApplication::translate("QWebFrame", "Frame load interrupted by policy change", 0, QCoreApplication::UnicodeUTF8));
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowMIMETypeError(const WebCore::ResourceResponse& response)
{
    return ResourceError("WebKit", WebKitErrorCannotShowMIMEType, response.url().string(),
            QCoreApplication::translate("QWebFrame", "Cannot show mimetype", 0, QCoreApplication::UnicodeUTF8));
}

WebCore::ResourceError FrameLoaderClientQt::fileDoesNotExistError(const WebCore::ResourceResponse& response)
{
    return ResourceError("QtNetwork", QNetworkReply::ContentNotFoundError, response.url().string(),
            QCoreApplication::translate("QWebFrame", "File does not exist", 0, QCoreApplication::UnicodeUTF8));
}

WebCore::ResourceError FrameLoaderClientQt::pluginWillHandleLoadError(const WebCore::ResourceResponse&)
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
    RefPtr<DocumentLoader> loader = DocumentLoader::create(request, substituteData);
    if (substituteData.isValid())
        loader->setDeferMainResourceDataLoad(false);
    return loader.release();
}

void FrameLoaderClientQt::download(WebCore::ResourceHandle* handle, const WebCore::ResourceRequest&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
#if QT_VERSION >= 0x040400
    if (!m_webFrame)
        return;

    QNetworkReplyHandler* handler = handle->getInternal()->m_job;
    QNetworkReply* reply = handler->release();
    if (reply) {
        QWebPage *page = m_webFrame->page();
        if (page->forwardUnsupportedContent())
            emit m_webFrame->page()->unsupportedContent(reply);
        else
            reply->abort();
    }
#endif
}

void FrameLoaderClientQt::assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest& request)
{
    if (dumpResourceLoadCallbacks)
        dumpAssignedUrls[identifier] = drtDescriptionSuitableForTestResult(request.url());
}

void FrameLoaderClientQt::dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest& newRequest, const WebCore::ResourceResponse& redirectResponse)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - willSendRequest %s redirectResponse %s\n",
               qPrintable(dumpAssignedUrls[identifier]),
               qPrintable(drtDescriptionSuitableForTestResult(newRequest)),
               qPrintable(drtDescriptionSuitableForTestResult(redirectResponse)));

    // seems like the Mac code doesn't do anything here by default neither
    //qDebug() << "FrameLoaderClientQt::dispatchWillSendRequest" << request.isNull() << request.url().string`();
}

bool
FrameLoaderClientQt::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
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
    //qDebug() << "    got response from" << response.url().string();
}

void FrameLoaderClientQt::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int)
{
}

void FrameLoaderClientQt::dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long)
{
}

void FrameLoaderClientQt::dispatchDidFailLoading(WebCore::DocumentLoader* loader, unsigned long identifier, const WebCore::ResourceError& error)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - didFailLoadingWithError: %s\n", qPrintable(dumpAssignedUrls[identifier]), qPrintable(drtDescriptionSuitableForTestResult(error)));

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

void FrameLoaderClientQt::dispatchDidLoadResourceByXMLHttpRequest(unsigned long, const WebCore::ScriptString&)
{
    notImplemented();
}

void FrameLoaderClientQt::callErrorPageExtension(const WebCore::ResourceError& error)
{
    QWebPage* page = m_webFrame->page();
    if (page->supportsExtension(QWebPage::ErrorPageExtension)) {
        QWebPage::ErrorPageExtensionOption option;

        if (error.domain() == "QtNetwork")
            option.domain = QWebPage::QtNetwork;
        else if (error.domain() == "HTTP")
            option.domain = QWebPage::Http;
        else if (error.domain() == "WebKit")
            option.domain = QWebPage::WebKit;
        else
            return;

        option.url = QUrl(error.failingURL());
        option.error = error.errorCode();
        option.errorString = error.localizedDescription();

        QWebPage::ErrorPageExtensionReturn output;
        if (!page->extension(QWebPage::ErrorPageExtension, &option, &output))
            return;

        KURL baseUrl(output.baseUrl);
        KURL failingUrl(option.url);

        WebCore::ResourceRequest request(baseUrl);
        WTF::RefPtr<WebCore::SharedBuffer> buffer = WebCore::SharedBuffer::create(output.content.constData(), output.content.length());
        WebCore::SubstituteData substituteData(buffer, output.contentType, output.encoding, failingUrl);
        m_frame->loader()->load(request, substituteData, false);
    }
}

void FrameLoaderClientQt::dispatchDidFailProvisionalLoad(const WebCore::ResourceError& error)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFailProvisionalLoadWithError\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    m_loadError = error;
    if (!error.isNull() && !error.isCancellation())
        callErrorPageExtension(error);
}

void FrameLoaderClientQt::dispatchDidFailLoad(const WebCore::ResourceError& error)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFailLoadWithError\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    m_loadError = error;
    if (!error.isNull() && !error.isCancellation())
        callErrorPageExtension(error);
}

WebCore::Frame* FrameLoaderClientQt::dispatchCreatePage()
{
    if (!m_webFrame)
        return 0;
    QWebPage *newPage = m_webFrame->page()->createWindow(QWebPage::WebBrowserWindow);
    if (!newPage)
        return 0;
    return newPage->mainFrame()->d->frame;
}

void FrameLoaderClientQt::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WebCore::String& MIMEType, const WebCore::ResourceRequest&)
{
    // we need to call directly here
    if (canShowMIMEType(MIMEType))
        callPolicyFunction(function, PolicyUse);
    else
        callPolicyFunction(function, PolicyDownload);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState>, const WebCore::String&)
{
    Q_ASSERT(m_webFrame);
#if QT_VERSION < 0x040400
    QWebNetworkRequest r(request);
#else
    QNetworkRequest r(request.toNetworkRequest());
#endif
    QWebPage* page = m_webFrame->page();

    if (!page->d->acceptNavigationRequest(0, r, QWebPage::NavigationType(action.type()))) {
        if (action.type() == NavigationTypeFormSubmitted || action.type() == NavigationTypeFormResubmitted)
            m_frame->loader()->resetMultipleFormSubmissionProtection();

        if (action.type() == NavigationTypeLinkClicked && r.url().hasFragment()) {
            ResourceRequest emptyRequest;
            m_frame->loader()->activeDocumentLoader()->setLastCheckedRequest(emptyRequest);
        }

        callPolicyFunction(function, PolicyIgnore);
        return;
    }
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState>)
{
    Q_ASSERT(m_webFrame);
#if QT_VERSION < 0x040400
    QWebNetworkRequest r(request);
#else
    QNetworkRequest r(request.toNetworkRequest());
#endif
    QWebPage*page = m_webFrame->page();

    if (!page->d->acceptNavigationRequest(m_webFrame, r, QWebPage::NavigationType(action.type()))) {
        if (action.type() == NavigationTypeFormSubmitted || action.type() == NavigationTypeFormResubmitted)
            m_frame->loader()->resetMultipleFormSubmissionProtection();

        if (action.type() == NavigationTypeLinkClicked && r.url().hasFragment()) {
            ResourceRequest emptyRequest;
            m_frame->loader()->activeDocumentLoader()->setLastCheckedRequest(emptyRequest);
        }

        callPolicyFunction(function, PolicyIgnore);
        return;
    }
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientQt::dispatchUnableToImplementPolicy(const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientQt::startDownload(const WebCore::ResourceRequest& request)
{
#if QT_VERSION >= 0x040400
    if (!m_webFrame)
        return;

    emit m_webFrame->page()->downloadRequested(request.toNetworkRequest());
#endif
}

PassRefPtr<Frame> FrameLoaderClientQt::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    if (!m_webFrame)
        return 0;

    QWebFrameData frameData(m_frame->page(), m_frame, ownerElement, name);
    frameData.url = url;
    frameData.referrer = referrer;
    frameData.allowsScrolling = allowsScrolling;
    frameData.marginWidth = marginWidth;
    frameData.marginHeight = marginHeight;

    QPointer<QWebFrame> webFrame = new QWebFrame(m_webFrame, &frameData);
    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!webFrame->d->frame->page()) {
        frameData.frame.release();
        ASSERT(webFrame.isNull());
        return 0;
    }

    emit m_webFrame->page()->frameCreated(webFrame);

    // ### set override encoding if we have one

    frameData.frame->loader()->loadURLIntoChildFrame(frameData.url, frameData.referrer, frameData.frame.get());

    // The frame's onload handler may have removed it from the document.
    if (!frameData.frame->tree()->parent())
        return 0;

    return frameData.frame.release();
}

ObjectContentType FrameLoaderClientQt::objectContentType(const KURL& url, const String& _mimeType)
{
//    qDebug()<<" ++++++++++++++++ url is "<<url.prettyURL()<<", mime = "<<_mimeType;
    if (_mimeType == "application/x-qt-plugin" || _mimeType == "application/x-qt-styled-widget")
        return ObjectContentOtherPlugin;

    if (url.isEmpty() && !_mimeType.length())
        return ObjectContentNone;

    String mimeType = _mimeType;
    if (!mimeType.length()) {
        QFileInfo fi(url.path());
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(fi.suffix());
    }

    if (!mimeType.length())
        return ObjectContentFrame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentImage;

    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return ObjectContentNetscapePlugin;

    if (m_frame->page() && m_frame->page()->pluginData() && m_frame->page()->pluginData()->supportsMimeType(mimeType))
        return ObjectContentOtherPlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentFrame;

    if (url.protocol() == "about")
        return ObjectContentFrame;

    return ObjectContentNone;
}

static const CSSPropertyID qstyleSheetProperties[] = {
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontWeight
};

const unsigned numqStyleSheetProperties = sizeof(qstyleSheetProperties) / sizeof(qstyleSheetProperties[0]);

class QtPluginWidget: public Widget
{
public:
    QtPluginWidget(QWidget* w = 0): Widget(w) {}
    ~QtPluginWidget()
    {
        if (platformWidget())
            platformWidget()->deleteLater();
    }
    virtual void invalidateRect(const IntRect& r)
    { 
        if (platformWidget())
            platformWidget()->update(r);
    }
    virtual void frameRectsChanged()
    {
        if (!platformWidget())
            return;

        IntRect windowRect = convertToContainingWindow(IntRect(0, 0, frameRect().width(), frameRect().height()));
        platformWidget()->setGeometry(windowRect);

        ScrollView* parentScrollView = parent();
        if (!parentScrollView)
            return;

        ASSERT(parentScrollView->isFrameView());
        IntRect clipRect(static_cast<FrameView*>(parentScrollView)->windowClipRect());
        clipRect.move(-windowRect.x(), -windowRect.y());
        clipRect.intersect(platformWidget()->rect());

        QRegion clipRegion = QRegion(clipRect);
        platformWidget()->setMask(clipRegion);

        // if setMask is set with an empty QRegion, no clipping will
        // be performed, so in that case we hide the platformWidget
        platformWidget()->setVisible(!clipRegion.isEmpty());
    }
};

#if QT_VERSION >= 0x040600
class QtPluginGraphicsWidget: public Widget
{
public:
    static RefPtr<QtPluginGraphicsWidget> create(QGraphicsWidget* w = 0)
    {
        return adoptRef(new QtPluginGraphicsWidget(w));
    }

    ~QtPluginGraphicsWidget()
    {
        if (graphicsWidget)
            graphicsWidget->deleteLater();
    }
    virtual void invalidateRect(const IntRect& r)
    {
        QGraphicsScene* scene = graphicsWidget ? graphicsWidget->scene() : 0;
        if (scene)
            scene->update(QRect(r));
    }
    virtual void frameRectsChanged()
    {
        if (!graphicsWidget)
            return;

        IntRect windowRect = convertToContainingWindow(IntRect(0, 0, frameRect().width(), frameRect().height()));
        graphicsWidget->setGeometry(QRect(windowRect));

        // FIXME: clipping of graphics widgets
    }
    virtual void show()
    {
        if (graphicsWidget)
            graphicsWidget->show();
    }
    virtual void hide()
    {
        if (graphicsWidget)
            graphicsWidget->hide();
    }
private:
    QtPluginGraphicsWidget(QGraphicsWidget* w = 0): Widget(0), graphicsWidget(w) {}

    QGraphicsWidget* graphicsWidget;
};
#endif

PassRefPtr<Widget> FrameLoaderClientQt::createPlugin(const IntSize& pluginSize, HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames,
                                          const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
//     qDebug()<<"------ Creating plugin in FrameLoaderClientQt::createPlugin for "<<url.prettyURL() << mimeType;
//     qDebug()<<"------\t url = "<<url.prettyURL();

    if (!m_webFrame)
        return 0;

    QStringList params;
    QStringList values;
    QString classid(element->getAttribute("classid"));

    for (unsigned i = 0; i < paramNames.size(); ++i) {
        params.append(paramNames[i]);
        if (paramNames[i] == "classid")
            classid = paramValues[i];
    }
    for (unsigned i = 0; i < paramValues.size(); ++i)
        values.append(paramValues[i]);

    QString urlStr(url.string());
    QUrl qurl = urlStr;

    QObject* object = 0;

    if (mimeType == "application/x-qt-plugin" || mimeType == "application/x-qt-styled-widget") {
        object = m_webFrame->page()->createPlugin(classid, qurl, params, values);
#ifndef QT_NO_STYLE_STYLESHEET
        QWidget* widget = qobject_cast<QWidget*>(object);
        if (widget && mimeType == "application/x-qt-styled-widget") {

            QString styleSheet = element->getAttribute("style");
            if (!styleSheet.isEmpty())
                styleSheet += QLatin1Char(';');

            for (unsigned i = 0; i < numqStyleSheetProperties; ++i) {
                CSSPropertyID property = qstyleSheetProperties[i];

                styleSheet += QString::fromLatin1(::getPropertyName(property));
                styleSheet += QLatin1Char(':');
                styleSheet += computedStyle(element)->getPropertyValue(property);
                styleSheet += QLatin1Char(';');
            }

            widget->setStyleSheet(styleSheet);
        }
#endif // QT_NO_STYLE_STYLESHEET
    }

#if QT_VERSION >= 0x040400
        if (!object) {
            QWebPluginFactory* factory = m_webFrame->page()->pluginFactory();
            if (factory)
                object = factory->create(mimeType, qurl, params, values);
        }
#endif

        if (object) {
            QWidget* widget = qobject_cast<QWidget*>(object);
            if (widget) {
                QWidget* parentWidget = qobject_cast<QWidget*>(m_webFrame->page()->d->client->pluginParent());
                if (parentWidget)
                    widget->setParent(parentWidget);
                RefPtr<QtPluginWidget> w = adoptRef(new QtPluginWidget());
                w->setPlatformWidget(widget);
                // Make sure it's invisible until properly placed into the layout
                w->setFrameRect(IntRect(0, 0, 0, 0));
                return w;
            }
#if QT_VERSION >= 0x040600
            QGraphicsWidget* graphicsWidget = qobject_cast<QGraphicsWidget*>(object);
            if (graphicsWidget) {
                graphicsWidget->hide();
                graphicsWidget->setParentItem(qobject_cast<QGraphicsObject*>(m_webFrame->page()->d->client->pluginParent()));
                RefPtr<QtPluginGraphicsWidget> w = QtPluginGraphicsWidget::create(graphicsWidget);
                // Make sure it's invisible until properly placed into the layout
                w->setFrameRect(IntRect(0, 0, 0, 0));
                return w;
            }
#endif
            // FIXME: make things work for widgetless plugins as well
            delete object;
    } else { // NPAPI Plugins
        RefPtr<PluginView> pluginView = PluginView::create(m_frame, pluginSize, element, url,
            paramNames, paramValues, mimeType, loadManually);
        return pluginView;
    }

    return 0;
}

void FrameLoaderClientQt::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

PassRefPtr<Widget> FrameLoaderClientQt::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL&,
                                                    const Vector<String>&, const Vector<String>&)
{
    notImplemented();
    return 0;
}

String FrameLoaderClientQt::overrideMediaType() const
{
    return String();
}

QString FrameLoaderClientQt::chooseFile(const QString& oldFile)
{
    return webFrame()->page()->chooseFile(webFrame(), oldFile);
}

}

#include "moc_FrameLoaderClientQt.cpp"
