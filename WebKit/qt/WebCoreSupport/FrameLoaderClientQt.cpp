/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Coypright (C) 2008 Holger Hans Peter Freyther
 * Coypright (C) 2009, 2010 Girish Ramakrishnan <girish@forwardbias.in>
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
#include "FrameNetworkingContextQt.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "DocumentLoader.h"
#include "HitTestResult.h"
#if USE(JSC)
#include "JSDOMWindowBase.h"
#elif USE(V8)
#include "V8DOMWindow.h"
#endif
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
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
#include "HTTPParsers.h"
#include "NotImplemented.h"
#include "QNetworkReplyHandler.h"
#include "ResourceHandleInternal.h"
#include "ResourceHandle.h"
#include "ScriptController.h"
#include "ScriptString.h"
#include "Settings.h"
#include "QWebPageClient.h"
#include "ViewportArguments.h"

#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebhistoryinterface.h"
#include "qwebpluginfactory.h"

#include <qfileinfo.h>

#include <QCoreApplication>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsWidget>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStringList>
#include "qwebhistory_p.h"

static QMap<unsigned long, QString> dumpAssignedUrls;

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
    if (_url.isEmpty() || !_url.isLocalFile())
        return _url.string();
    // Remove the leading path from file urls
    return QString(_url.string()).replace(WebCore::FrameLoaderClientQt::dumpResourceLoadCallbacksPath, "").mid(1);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceError& error)
{
    QString failingURL = error.failingURL();
    return QString::fromLatin1("<NSError domain NSURLErrorDomain, code %1, failing URL \"%2\">").arg(error.errorCode()).arg(failingURL);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceRequest& request)
{
    QString url = drtDescriptionSuitableForTestResult(request.url());
    QString httpMethod = request.httpMethod();
    QString mainDocumentUrl = drtDescriptionSuitableForTestResult(request.firstPartyForCookies());
    return QString::fromLatin1("<NSURLRequest URL %1, main document URL %2, http method %3>").arg(url).arg(mainDocumentUrl).arg(httpMethod);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceResponse& response)
{
    QString url = drtDescriptionSuitableForTestResult(response.url());
    int httpStatusCode = response.httpStatusCode();
    return QString::fromLatin1("<NSURLResponse %1, http status code %2>").arg(url).arg(httpStatusCode);
}

static QString drtDescriptionSuitableForTestResult(const RefPtr<WebCore::Node> node, int exception)
{
    QString result;
    if (exception) {
        result.append("ERROR");
        return result;
    }
    if (!node) {
        result.append("NULL");
        return result;
    }
    result.append(node->nodeName());
    RefPtr<WebCore::Node> parent = node->parentNode();
    if (parent) {
        result.append(" > ");
        result.append(drtDescriptionSuitableForTestResult(parent, 0));
    }
    return result;
}

namespace WebCore
{

bool FrameLoaderClientQt::dumpFrameLoaderCallbacks = false;
bool FrameLoaderClientQt::dumpResourceLoadCallbacks = false;
bool FrameLoaderClientQt::sendRequestReturnsNullOnRedirect = false;
bool FrameLoaderClientQt::sendRequestReturnsNull = false;
bool FrameLoaderClientQt::dumpResourceResponseMIMETypes = false;
bool FrameLoaderClientQt::deferMainResourceDataLoad = true;

QStringList FrameLoaderClientQt::sendRequestClearHeaders;
QString FrameLoaderClientQt::dumpResourceLoadCallbacksPath;
bool FrameLoaderClientQt::policyDelegateEnabled = false;
bool FrameLoaderClientQt::policyDelegatePermissive = false;

// Taken from DumpRenderTree/chromium/WebViewHost.cpp
static const char* navigationTypeToString(NavigationType type)
{
    switch (type) {
    case NavigationTypeLinkClicked:
        return "link clicked";
    case NavigationTypeFormSubmitted:
        return "form submitted";
    case NavigationTypeBackForward:
        return "back/forward";
    case NavigationTypeReload:
        return "reload";
    case NavigationTypeFormResubmitted:
        return "form resubmitted";
    case NavigationTypeOther:
        return "other";
    }
    return "illegal value";
}

FrameLoaderClientQt::FrameLoaderClientQt()
    : m_frame(0)
    , m_webFrame(0)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_hasRepresentation(false)
    , m_loadError(ResourceError())
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
    connect(this, SIGNAL(titleChanged(QString)),
            m_webFrame, SIGNAL(titleChanged(QString)));
}

QWebFrame* FrameLoaderClientQt::webFrame() const
{
    return m_webFrame;
}

void FrameLoaderClientQt::callPolicyFunction(FramePolicyFunction function, PolicyAction action)
{
    (m_frame->loader()->policyChecker()->*function)(action);
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
    const QSize preferredLayoutSize = page->preferredContentsSize();

    ScrollbarMode hScrollbar = (ScrollbarMode) m_webFrame->scrollBarPolicy(Qt::Horizontal);
    ScrollbarMode vScrollbar = (ScrollbarMode) m_webFrame->scrollBarPolicy(Qt::Vertical);
    bool hLock = hScrollbar != ScrollbarAuto;
    bool vLock = vScrollbar != ScrollbarAuto;

    m_frame->createView(m_webFrame->page()->viewportSize(),
                        backgroundColor, !backgroundColor.alpha(),
                        preferredLayoutSize.isValid() ? IntSize(preferredLayoutSize) : IntSize(),
                        preferredLayoutSize.isValid(),
                        hScrollbar, hLock,
                        vScrollbar, vLock);
}


void FrameLoaderClientQt::makeRepresentation(DocumentLoader*)
{
    m_hasRepresentation = true;
}


void FrameLoaderClientQt::forceLayout()
{
    FrameView* view = m_frame->view();
    if (view)
        view->layout(true);
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

#if USE(V8)
void FrameLoaderClientQt::didCreateScriptContextForFrame()
{
}
void FrameLoaderClientQt::didDestroyScriptContextForFrame()
{
}
void FrameLoaderClientQt::didCreateIsolatedScriptContext()
{
}
#endif

void FrameLoaderClientQt::dispatchDidPushStateWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - dispatchDidPushStateWithinPage\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));
        
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidReplaceStateWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - dispatchDidReplaceStateWithinPage\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));
        
    notImplemented();
}

void FrameLoaderClientQt::dispatchDidPopStateWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - dispatchDidPopStateWithinPage\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));
        
    notImplemented();
}

void FrameLoaderClientQt::dispatchWillClose()
{
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


void FrameLoaderClientQt::dispatchDidChangeIcons()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didChangeIcons\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (!m_webFrame)
        return;

    // FIXME: To be notified of changing icon URLS add notification
    // emit iconsChanged();
}


void FrameLoaderClientQt::dispatchDidCommitLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didCommitLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_frame->tree()->parent() || !m_webFrame)
        return;

    // Clear the viewport arguments.
    m_webFrame->d->viewportArguments = WebCore::ViewportArguments();

    emit m_webFrame->urlChanged(m_webFrame->url());
    m_webFrame->page()->d->updateNavigationActions();

    // We should assume first the frame has no title. If it has, then the above dispatchDidReceiveTitle()
    // will be called very soon with the correct title.
    // This properly resets the title when we navigate to a URI without a title.
    emit titleChanged(String());

    bool isMainFrame = (m_frame == m_frame->page()->mainFrame());
    if (!isMainFrame)
        return;

    emit m_webFrame->page()->viewportChangeRequested();
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

    // Clears the previous error.
    m_loadError = ResourceError();

    if (!m_webFrame)
        return;
    m_webFrame->page()->d->updateNavigationActions();
}


void FrameLoaderClientQt::dispatchDidFirstLayout()
{
}

void FrameLoaderClientQt::dispatchDidFirstVisuallyNonEmptyLayout()
{
    if (m_webFrame)
        emit m_webFrame->initialLayoutCompleted();
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
    m_hasRepresentation = true;
}


void FrameLoaderClientQt::postProgressStartedNotification()
{
    if (m_webFrame && m_frame->page()) {
        // A new load starts, so lets clear the previous error.
        m_loadError = ResourceError();
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


void FrameLoaderClientQt::didChangeTitle(DocumentLoader*)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}


void FrameLoaderClientQt::finishedLoading(DocumentLoader* loader)
{
    if (!m_pluginView) {
        // This is necessary to create an empty document. See bug 634004.
        // However, we only want to do this if makeRepresentation has been called, to
        // match the behavior on the Mac.
        if (m_hasRepresentation)
            loader->frameLoader()->writer()->setEncoding("", false);
        return;
    }
    if (m_pluginView->isPluginView())
        m_pluginView->didFinishLoading();
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}


bool FrameLoaderClientQt::canShowMIMEType(const String& MIMEType) const
{
    String type = MIMEType;
    type.makeLower();
    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return true;

    if (m_frame && m_frame->settings()  && m_frame->settings()->arePluginsEnabled()
        && PluginDatabase::installedPlugins()->isMIMETypeRegistered(type))
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

void FrameLoaderClientQt::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (m_webFrame) {
        emit m_webFrame->javaScriptWindowObjectCleared();
    }
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

void FrameLoaderClientQt::dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const
{
}

void FrameLoaderClientQt::dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const
{
}

void FrameLoaderClientQt::dispatchDidChangeBackForwardIndex() const
{
}

void FrameLoaderClientQt::didDisplayInsecureContent()
{
    if (dumpFrameLoaderCallbacks)
        printf("didDisplayInsecureContent\n");

    notImplemented();
}

void FrameLoaderClientQt::didRunInsecureContent(WebCore::SecurityOrigin*)
{
    if (dumpFrameLoaderCallbacks)
        printf("didRunInsecureContent\n");

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
    if (!m_pluginView)
        return;
    if (m_pluginView->isPluginView())
        m_pluginView->didFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

// FIXME: This function should be moved into WebCore.
void FrameLoaderClientQt::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView)
        loader->commitData(data, length);
    
    // We re-check here as the plugin can have been created
    if (m_pluginView && m_pluginView->isPluginView()) {
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
    return ResourceError("WebKitErrorDomain", WebKitErrorCannotUseRestrictedPort, request.url().prettyURL(),
            QCoreApplication::translate("QWebFrame", "Request blocked", 0, QCoreApplication::UnicodeUTF8));
}


WebCore::ResourceError FrameLoaderClientQt::cannotShowURLError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorCannotShowURL, request.url().string(),
            QCoreApplication::translate("QWebFrame", "Cannot show URL", 0, QCoreApplication::UnicodeUTF8));
}

WebCore::ResourceError FrameLoaderClientQt::interruptForPolicyChangeError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().string(),
            QCoreApplication::translate("QWebFrame", "Frame load interrupted by policy change", 0, QCoreApplication::UnicodeUTF8));
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowMIMETypeError(const WebCore::ResourceResponse& response)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorCannotShowMIMEType, response.url().string(),
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
    if (!deferMainResourceDataLoad || substituteData.isValid()) {
        loader->setDeferMainResourceDataLoad(false);
        // Use the default timeout interval for JS as the HTML tokenizer delay. This ensures
        // that long-running JavaScript will still allow setHtml() to be synchronous, while
        // still giving a reasonable timeout to prevent deadlock.
#if USE(JSC)
        double delay = JSDOMWindowBase::commonJSGlobalData()->timeoutChecker.timeoutInterval() / 1000.0f;
#elif USE(V8)
        // FIXME: Hard coded for now.
        double delay = 10000 / 1000.0f;
#endif
        m_frame->page()->setCustomHTMLTokenizerTimeDelay(delay);
    } else
        m_frame->page()->setCustomHTMLTokenizerTimeDelay(-1);
    return loader.release();
}

void FrameLoaderClientQt::download(WebCore::ResourceHandle* handle, const WebCore::ResourceRequest&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    if (!m_webFrame)
        return;

    QNetworkReplyHandler* handler = handle->getInternal()->m_job;
    QNetworkReply* reply = handler->release();
    if (reply) {
        QWebPage *page = m_webFrame->page();
        if (page->forwardUnsupportedContent())
            emit page->unsupportedContent(reply);
        else
            reply->abort();
    }
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
               (redirectResponse.isNull()) ? "(null)" : qPrintable(drtDescriptionSuitableForTestResult(redirectResponse)));

    if (sendRequestReturnsNull)
        newRequest.setURL(QUrl());

    if (sendRequestReturnsNullOnRedirect && !redirectResponse.isNull()) {
        printf("Returning null for this redirect\n");
        newRequest.setURL(QUrl());
    }

    for (int i = 0; i < sendRequestClearHeaders.size(); ++i)
          newRequest.setHTTPHeaderField(sendRequestClearHeaders.at(i).toLocal8Bit().constData(), QString());

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

void FrameLoaderClientQt::dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse& response)
{

    m_response = response;
    if (dumpResourceLoadCallbacks)
        printf("%s - didReceiveResponse %s\n",
               qPrintable(dumpAssignedUrls[identifier]),
               qPrintable(drtDescriptionSuitableForTestResult(response)));

    if (dumpResourceResponseMIMETypes) {
        printf("%s has MIME type %s\n",
               qPrintable(QFileInfo(drtDescriptionSuitableForTestResult(response.url())).fileName()),
               qPrintable(QString(response.mimeType())));
    }
}

void FrameLoaderClientQt::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int)
{
}

void FrameLoaderClientQt::dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - didFinishLoading\n",
               (dumpAssignedUrls.contains(identifier) ? qPrintable(dumpAssignedUrls[identifier]) : "<unknown>"));
}

void FrameLoaderClientQt::dispatchDidFailLoading(WebCore::DocumentLoader* loader, unsigned long identifier, const WebCore::ResourceError& error)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - didFailLoadingWithError: %s\n",
               (dumpAssignedUrls.contains(identifier) ? qPrintable(dumpAssignedUrls[identifier]) : "<unknown>"),
               qPrintable(drtDescriptionSuitableForTestResult(error)));
}

bool FrameLoaderClientQt::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
    return false;
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
        option.frame = m_webFrame;
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

void FrameLoaderClientQt::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WTF::String& MIMEType, const WebCore::ResourceRequest&)
{
    // we need to call directly here
    const ResourceResponse& response = m_frame->loader()->activeDocumentLoader()->response();
    if (WebCore::contentDispositionType(response.httpHeaderField("Content-Disposition")) == WebCore::ContentDispositionAttachment)
        callPolicyFunction(function, PolicyDownload);
    else if (canShowMIMEType(MIMEType))
        callPolicyFunction(function, PolicyUse);
    else
        callPolicyFunction(function, PolicyDownload);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState>, const WTF::String&)
{
    Q_ASSERT(m_webFrame);
    QNetworkRequest r(request.toNetworkRequest(m_webFrame));
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
    QNetworkRequest r(request.toNetworkRequest(m_webFrame));
    QWebPage*page = m_webFrame->page();
    PolicyAction result;

    // Currently, this is only enabled by DRT
    if (policyDelegateEnabled) {
        RefPtr<Node> node;
        for (const Event* event = action.event(); event; event = event->underlyingEvent()) {
            if (event->isMouseEvent()) {
                const MouseEvent* mouseEvent =  static_cast<const MouseEvent*>(event);
                node = QWebFramePrivate::core(m_webFrame)->eventHandler()->hitTestResultAtPoint(
                    mouseEvent->absoluteLocation(), false).innerNonSharedNode();
                break;
            }
        }

        printf("Policy delegate: attempt to load %s with navigation type '%s'%s\n",
               qPrintable(drtDescriptionSuitableForTestResult(request.url())), navigationTypeToString(action.type()),
               (node) ? qPrintable(QString(" originating from " + drtDescriptionSuitableForTestResult(node, 0))) : "");

        if (policyDelegatePermissive)
            result = PolicyUse;
        else
            result = PolicyIgnore;

        callPolicyFunction(function, result);
        return;
    }

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
    if (!m_webFrame)
        return;

    emit m_webFrame->page()->downloadRequested(request.toNetworkRequest(m_webFrame));
}

PassRefPtr<Frame> FrameLoaderClientQt::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    if (!m_webFrame)
        return 0;

    QWebFrameData frameData(m_frame->page(), m_frame, ownerElement, name);

    if (url.isEmpty())
        frameData.url = blankURL();
    else
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

    m_frame->loader()->loadURLIntoChildFrame(frameData.url, frameData.referrer, frameData.frame.get());

    // The frame's onload handler may have removed it from the document.
    if (!frameData.frame->tree()->parent())
        return 0;

    return frameData.frame.release();
}

void FrameLoaderClientQt::didTransferChildFrameToNewDocument()
{
    ASSERT(m_frame->ownerElement());

    if (!m_webFrame)
        return;

    Frame* parentFrame = m_webFrame->d->frame->tree()->parent();
    ASSERT(parentFrame);

    if (QWebFrame* parent = QWebFramePrivate::kit(parentFrame)) {
        m_webFrame->d->setPage(parent->page());

        if (m_webFrame->parent() != qobject_cast<QObject*>(parent))
            m_webFrame->setParent(parent);
    }
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

        handleVisibility();

        platformWidget()->update();
    }

    virtual void show()
    {
        Widget::show();
        handleVisibility();
    }

private:
    void handleVisibility()
    {
        if (!isVisible())
            return;

        // if setMask is set with an empty QRegion, no clipping will
        // be performed, so in that case we hide the platformWidget
        QRegion mask = platformWidget()->mask();
        platformWidget()->setVisible(!mask.isEmpty());
    }
};

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
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

        if (!object) {
            QWebPluginFactory* factory = m_webFrame->page()->pluginFactory();
            if (factory)
                object = factory->create(mimeType, qurl, params, values);
        }

        if (object) {
            QWidget* widget = qobject_cast<QWidget*>(object);
            if (widget) {
                QWidget* parentWidget = 0;
                if (m_webFrame->page()->d->client)
                    parentWidget = qobject_cast<QWidget*>(m_webFrame->page()->d->client->pluginParent());
                if (parentWidget) // don't reparent to nothing (i.e. keep whatever parent QWebPage::createPlugin() chose.
                    widget->setParent(parentWidget);
                widget->hide();
                RefPtr<QtPluginWidget> w = adoptRef(new QtPluginWidget());
                w->setPlatformWidget(widget);
                // Make sure it's invisible until properly placed into the layout
                w->setFrameRect(IntRect(0, 0, 0, 0));
                return w;
            }
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
            QGraphicsWidget* graphicsWidget = qobject_cast<QGraphicsWidget*>(object);
            if (graphicsWidget) {
                QGraphicsObject* parentWidget = 0;
                if (m_webFrame->page()->d->client)
                    parentWidget = qobject_cast<QGraphicsObject*>(m_webFrame->page()->d->client->pluginParent());
                graphicsWidget->hide();
                if (parentWidget) // don't reparent to nothing (i.e. keep whatever parent QWebPage::createPlugin() chose.
                    graphicsWidget->setParentItem(parentWidget);
                RefPtr<QtPluginGraphicsWidget> w = QtPluginGraphicsWidget::create(graphicsWidget);
                // Make sure it's invisible until properly placed into the layout
                w->setFrameRect(IntRect(0, 0, 0, 0));
                return w;
            }
#endif
            // FIXME: make things work for widgetless plugins as well
            delete object;
    }
#if ENABLE(NETSCAPE_PLUGIN_API)
    else { // NPAPI Plugins
        Vector<String> params = paramNames;
        Vector<String> values = paramValues;
        if (mimeType == "application/x-shockwave-flash") {
            QWebPageClient* client = m_webFrame->page()->d->client;
            const bool isQWebView = client && qobject_cast<QWidget*>(client->pluginParent());
#if defined(MOZ_PLATFORM_MAEMO) && (MOZ_PLATFORM_MAEMO == 5)
            size_t wmodeIndex = params.find("wmode");
            if (wmodeIndex == -1) {
                // Disable XEmbed mode and force it to opaque mode
                params.append("wmode");
                values.append("opaque");
            } else if (!isQWebView) {
                // Disable transparency if client is not a QWebView
                values[wmodeIndex] = "opaque";
            }
#else
            if (!isQWebView) {
                // inject wmode=opaque when there is no client or the client is not a QWebView
                size_t wmodeIndex = params.find("wmode");
                if (wmodeIndex == -1) {
                    params.append("wmode");
                    values.append("opaque");
                } else
                    values[wmodeIndex] = "opaque";
            }
#endif
        }

        RefPtr<PluginView> pluginView = PluginView::create(m_frame, pluginSize, element, url,
            params, values, mimeType, loadManually);
        return pluginView;
    }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

    return 0;
}

void FrameLoaderClientQt::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

PassRefPtr<Widget> FrameLoaderClientQt::createJavaAppletWidget(const IntSize& pluginSize, HTMLAppletElement* element, const KURL& url,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    return createPlugin(pluginSize, element, url, paramNames, paramValues, "application/x-java-applet", true);
}

String FrameLoaderClientQt::overrideMediaType() const
{
    return String();
}

QString FrameLoaderClientQt::chooseFile(const QString& oldFile)
{
    return webFrame()->page()->chooseFile(webFrame(), oldFile);
}

PassRefPtr<FrameNetworkingContext> FrameLoaderClientQt::createNetworkingContext()
{
    return FrameNetworkingContextQt::create(m_frame, m_webFrame, m_webFrame->page()->networkAccessManager());
}

}

#include "moc_FrameLoaderClientQt.cpp"
