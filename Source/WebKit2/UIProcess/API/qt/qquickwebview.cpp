/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "qquickwebview_p.h"

#include "DownloadProxy.h"
#include "DrawingAreaProxyImpl.h"
#include "QtDownloadManager.h"
#include "QtWebContext.h"
#include "QtWebIconDatabaseClient.h"
#include "QtWebPageEventHandler.h"
#include "UtilsQt.h"
#include "WebBackForwardList.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"

#include "qquicknetworkreply_p.h"
#include "qquicknetworkrequest_p.h"
#include "qquickwebpage_p_p.h"
#include "qquickwebview_p_p.h"
#include "qwebdownloaditem_p_p.h"
#include "qwebloadrequest_p.h"
#include "qwebnavigationhistory_p.h"
#include "qwebnavigationhistory_p_p.h"
#include "qwebpreferences_p.h"
#include "qwebpreferences_p_p.h"
#include "qwebviewportinfo_p.h"

#include <private/qquickflickable_p.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <QDeclarativeEngine>
#include <QtQuick/QQuickCanvas>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WKOpenPanelResultListener.h>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

static bool s_flickableViewportEnabled = true;

static QQuickWebViewPrivate* createPrivateObject(QQuickWebView* publicObject)
{
    if (s_flickableViewportEnabled)
        return new QQuickWebViewFlickablePrivate(publicObject);
    return new QQuickWebViewLegacyPrivate(publicObject);
}

QQuickWebViewPrivate::QQuickWebViewPrivate(QQuickWebView* viewport)
    : q_ptr(viewport)
    , flickProvider(0)
    , alertDialog(0)
    , confirmDialog(0)
    , promptDialog(0)
    , authenticationDialog(0)
    , certificateVerificationDialog(0)
    , itemSelector(0)
    , proxyAuthenticationDialog(0)
    , filePicker(0)
    , databaseQuotaDialog(0)
    , userDidOverrideContentWidth(false)
    , userDidOverrideContentHeight(false)
    , m_navigatorQtObjectEnabled(false)
    , m_renderToOffscreenBuffer(false)
    , m_loadStartedSignalSent(false)
    , m_dialogActive(false)
{
    viewport->setFlags(QQuickItem::ItemClipsChildrenToShape);
    QObject::connect(viewport, SIGNAL(visibleChanged()), viewport, SLOT(_q_onVisibleChanged()));
    QObject::connect(viewport, SIGNAL(urlChanged()), viewport, SLOT(_q_onUrlChanged()));
    pageView.reset(new QQuickWebPage(viewport));
}

QQuickWebViewPrivate::~QQuickWebViewPrivate()
{
    webPageProxy->close();
}

// Note: we delay this initialization to make sure that QQuickWebView has its d-ptr in-place.
void QQuickWebViewPrivate::initialize(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    RefPtr<WebPageGroup> pageGroup;
    if (pageGroupRef)
        pageGroup = toImpl(pageGroupRef);
    else
        pageGroup = WebPageGroup::create();

    context = contextRef ? QtWebContext::create(toImpl(contextRef)) : QtWebContext::defaultContext();
    webPageProxy = context->createWebPage(&pageClient, pageGroup.get());

    QQuickWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    pageViewPrivate->initialize(webPageProxy.get());

    pageLoadClient.reset(new QtWebPageLoadClient(toAPI(webPageProxy.get()), q_ptr));
    pagePolicyClient.reset(new QtWebPagePolicyClient(toAPI(webPageProxy.get()), q_ptr));
    pageUIClient.reset(new QtWebPageUIClient(toAPI(webPageProxy.get()), q_ptr));
    navigationHistory = adoptPtr(QWebNavigationHistoryPrivate::createHistory(toAPI(webPageProxy.get())));

    QtWebIconDatabaseClient* iconDatabase = context->iconDatabase();
    QObject::connect(iconDatabase, SIGNAL(iconChangedForPageURL(QUrl, QUrl)), q_ptr, SLOT(_q_onIconChangedForPageURL(QUrl, QUrl)));

    // Any page setting should preferrable be set before creating the page.
    webPageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    webPageProxy->pageGroup()->preferences()->setForceCompositingMode(true);
    webPageProxy->pageGroup()->preferences()->setFrameFlatteningEnabled(true);

    pageClient.initialize(q_ptr, pageViewPrivate->eventHandler.data(), &undoController);
    webPageProxy->initializeWebPage();
}

void QQuickWebViewPrivate::setTransparentBackground(bool enable)
{
    webPageProxy->setDrawsTransparentBackground(enable);
}

bool QQuickWebViewPrivate::transparentBackground() const
{
    return webPageProxy->drawsTransparentBackground();
}

QPointF QQuickWebViewPrivate::pageItemPos()
{
    ASSERT(pageView);
    return pageView->pos();
}

void QQuickWebViewPrivate::loadDidSucceed()
{
    Q_Q(QQuickWebView);
    ASSERT(!q->loading());
    QWebLoadRequest loadRequest(q->url(), QQuickWebView::LoadSucceededStatus);
    emit q->loadingChanged(&loadRequest);
}

void QQuickWebViewPrivate::onComponentComplete()
{
    if (m_deferedUrlToLoad.isEmpty())
        return;

    q_ptr->setUrl(m_deferedUrlToLoad);
}

void QQuickWebViewPrivate::setNeedsDisplay()
{
    Q_Q(QQuickWebView);
    if (renderToOffscreenBuffer()) {
        // TODO: we can maintain a real image here and use it for pixel tests. Right now this is used only for running the rendering code-path while running tests.
        QImage dummyImage(1, 1, QImage::Format_ARGB32);
        QPainter painter(&dummyImage);
        q->page()->d->paint(&painter);
        return;
    }

    q->page()->update();
}

void QQuickWebViewPrivate::_q_onIconChangedForPageURL(const QUrl& pageURL, const QUrl& iconURL)
{
    Q_Q(QQuickWebView);
    if (q->url() != pageURL)
        return;

    setIcon(iconURL);
}

void QQuickWebViewPrivate::didChangeLoadingState(QWebLoadRequest* loadRequest)
{
    Q_Q(QQuickWebView);
    ASSERT(q->loading() == (loadRequest->status() == QQuickWebView::LoadStartedStatus));
    emit q->loadingChanged(loadRequest);
    m_loadStartedSignalSent = loadRequest->status() == QQuickWebView::LoadStartedStatus;
}

void QQuickWebViewPrivate::didChangeBackForwardList()
{
    navigationHistory->d->reset();
}

void QQuickWebViewPrivate::processDidCrash()
{
    pageView->eventHandler()->resetGestureRecognizers();
    QUrl url(KURL(WebCore::ParsedURLString, webPageProxy->urlAtProcessExit()));
    if (m_loadStartedSignalSent) {
        QWebLoadRequest loadRequest(url, QQuickWebView::LoadFailedStatus, QLatin1String("The web process crashed."), QQuickWebView::InternalErrorDomain, 0);
        didChangeLoadingState(&loadRequest);
    }
    qWarning("WARNING: The web process experienced a crash on '%s'.", qPrintable(url.toString(QUrl::RemoveUserInfo)));
}

void QQuickWebViewPrivate::didRelaunchProcess()
{
    qWarning("WARNING: The web process has been successfully restarted.");
    webPageProxy->drawingArea()->setSize(viewSize(), IntSize());
}

PassOwnPtr<DrawingAreaProxy> QQuickWebViewPrivate::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(webPageProxy.get());
}

void QQuickWebViewPrivate::handleDownloadRequest(DownloadProxy* download)
{
    Q_Q(QQuickWebView);
    // This function is responsible for hooking up a DownloadProxy to our API layer
    // by creating a QWebDownloadItem. It will then wait for the QWebDownloadItem to be
    // ready (filled with the ResourceResponse information) so we can pass it through to
    // our WebViews.
    QWebDownloadItem* downloadItem = new QWebDownloadItem();
    downloadItem->d->downloadProxy = download;

    q->connect(downloadItem->d, SIGNAL(receivedResponse(QWebDownloadItem*)), q, SLOT(_q_onReceivedResponseFromDownload(QWebDownloadItem*)));
    context->downloadManager()->addDownload(download, downloadItem);
}

void QQuickWebViewPrivate::_q_onVisibleChanged()
{
    webPageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

void QQuickWebViewPrivate::_q_onUrlChanged()
{
    Q_Q(QQuickWebView);
    context->iconDatabase()->requestIconForPageURL(q->url());
}

void QQuickWebViewPrivate::_q_onReceivedResponseFromDownload(QWebDownloadItem* downloadItem)
{
    // Now that our downloadItem has everything we need we can emit downloadRequested.
    if (!downloadItem)
        return;

    Q_Q(QQuickWebView);
    QDeclarativeEngine::setObjectOwnership(downloadItem, QDeclarativeEngine::JavaScriptOwnership);
    emit q->experimental()->downloadRequested(downloadItem);
}

void QQuickWebViewPrivate::runJavaScriptAlert(const QString& alertText)
{
    if (!alertDialog)
        return;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForAlert(alertDialog, q, alertText))
        return;

    execDialogRunner(dialogRunner);
}

bool QQuickWebViewPrivate::runJavaScriptConfirm(const QString& message)
{
    if (!confirmDialog)
        return true;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForConfirm(confirmDialog, q, message))
        return true;

    execDialogRunner(dialogRunner);

    return dialogRunner.wasAccepted();
}

QString QQuickWebViewPrivate::runJavaScriptPrompt(const QString& message, const QString& defaultValue, bool& ok)
{
    if (!promptDialog) {
        ok = true;
        return defaultValue;
    }

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForPrompt(promptDialog, q, message, defaultValue)) {
        ok = true;
        return defaultValue;
    }

    execDialogRunner(dialogRunner);

    ok = dialogRunner.wasAccepted();
    return dialogRunner.result();
}

void QQuickWebViewPrivate::handleAuthenticationRequiredRequest(const QString& hostname, const QString& realm, const QString& prefilledUsername, QString& username, QString& password)
{
    if (!authenticationDialog)
        return;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForAuthentication(authenticationDialog, q, hostname, realm, prefilledUsername))
        return;

    execDialogRunner(dialogRunner);

    username = dialogRunner.username();
    password = dialogRunner.password();
}

void QQuickWebViewPrivate::handleProxyAuthenticationRequiredRequest(const QString& hostname, uint16_t port, const QString& prefilledUsername, QString& username, QString& password)
{
    if (!proxyAuthenticationDialog)
        return;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForProxyAuthentication(proxyAuthenticationDialog, q, hostname, port, prefilledUsername))
        return;

    execDialogRunner(dialogRunner);

    username = dialogRunner.username();
    password = dialogRunner.password();
}

bool QQuickWebViewPrivate::handleCertificateVerificationRequest(const QString& hostname)
{
    if (!certificateVerificationDialog)
        return false;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForCertificateVerification(certificateVerificationDialog, q, hostname))
        return false;

    execDialogRunner(dialogRunner);

    return dialogRunner.wasAccepted();
}

void QQuickWebViewPrivate::execDialogRunner(QtDialogRunner& dialogRunner)
{
    setViewInAttachedProperties(dialogRunner.dialog());

    disableMouseEvents();
    m_dialogActive = true;

    dialogRunner.exec();
    m_dialogActive = false;
    enableMouseEvents();
}

void QQuickWebViewPrivate::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, QtWebPageUIClient::FileChooserType type)
{
    Q_Q(QQuickWebView);

    if (!filePicker)
        return;

    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForFilePicker(filePicker, q, selectedFileNames, (type == QtWebPageUIClient::MultipleFilesSelection)))
        return;

    execDialogRunner(dialogRunner);

    if (dialogRunner.wasAccepted()) {
        QStringList selectedPaths = dialogRunner.filePaths();

        Vector<RefPtr<APIObject> > wkFiles(selectedPaths.size());
        for (unsigned i = 0; i < selectedPaths.size(); ++i)
            wkFiles[i] = WebURL::create(QUrl::fromLocalFile(selectedPaths.at(i)).toString());            

        WKOpenPanelResultListenerChooseFiles(listenerRef, toAPI(ImmutableArray::adopt(wkFiles).leakRef()));
    } else
        WKOpenPanelResultListenerCancel(listenerRef);

}

quint64 QQuickWebViewPrivate::exceededDatabaseQuota(const QString& databaseName, const QString& displayName, WKSecurityOriginRef securityOrigin, quint64 currentQuota, quint64 currentOriginUsage, quint64 currentDatabaseUsage, quint64 expectedUsage)
{
    if (!databaseQuotaDialog)
        return 0;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForDatabaseQuotaDialog(databaseQuotaDialog, q, databaseName, displayName, securityOrigin, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage))
        return 0;

    execDialogRunner(dialogRunner);

    return dialogRunner.wasAccepted() ? dialogRunner.databaseQuota() : 0;
}

void QQuickWebViewPrivate::setViewInAttachedProperties(QObject* object)
{
    Q_Q(QQuickWebView);
    QQuickWebViewAttached* attached = static_cast<QQuickWebViewAttached*>(qmlAttachedPropertiesObject<QQuickWebView>(object));
    attached->setView(q);
}

void QQuickWebViewPrivate::setIcon(const QUrl& iconURL)
{
    Q_Q(QQuickWebView);
    if (m_iconURL == iconURL)
        return;

    String oldPageURL = QUrl::fromPercentEncoding(m_iconURL.encodedFragment());
    String newPageURL = webPageProxy->mainFrame()->url();

    if (oldPageURL != newPageURL) {
        QtWebIconDatabaseClient* iconDatabase = context->iconDatabase();
        if (!oldPageURL.isEmpty())
            iconDatabase->releaseIconForPageURL(oldPageURL);

        if (!newPageURL.isEmpty())
            iconDatabase->retainIconForPageURL(newPageURL);
    }

    m_iconURL = iconURL;
    emit q->iconChanged();
}

bool QQuickWebViewPrivate::navigatorQtObjectEnabled() const
{
    return m_navigatorQtObjectEnabled;
}

void QQuickWebViewPrivate::setNavigatorQtObjectEnabled(bool enabled)
{
    ASSERT(enabled != m_navigatorQtObjectEnabled);
    // FIXME: Currently we have to keep this information in both processes and the setting is asynchronous.
    m_navigatorQtObjectEnabled = enabled;
    context->setNavigatorQtObjectEnabled(webPageProxy.get(), enabled);
}

QRect QQuickWebViewPrivate::visibleContentsRect() const
{
    Q_Q(const QQuickWebView);
    const QRectF visibleRect(q->boundingRect().intersected(pageView->boundingRect()));

    return q->mapRectToWebContent(visibleRect).toAlignedRect();
}

WebCore::IntSize QQuickWebViewPrivate::viewSize() const
{
    return WebCore::IntSize(pageView->width(), pageView->height());
}

void QQuickWebViewPrivate::didReceiveMessageFromNavigatorQtObject(const String& message)
{
    QVariantMap variantMap;
    variantMap.insert(QLatin1String("data"), QString(message));
    variantMap.insert(QLatin1String("origin"), q_ptr->url());
    emit q_ptr->experimental()->messageReceived(variantMap);
}

QQuickWebViewLegacyPrivate::QQuickWebViewLegacyPrivate(QQuickWebView* viewport)
    : QQuickWebViewPrivate(viewport)
{
}

void QQuickWebViewLegacyPrivate::initialize(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    QQuickWebViewPrivate::initialize(contextRef, pageGroupRef);
    enableMouseEvents();

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();
}

void QQuickWebViewLegacyPrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSize viewportSize = q->boundingRect().size().toSize();
    pageView->setContentsSize(viewportSize);
    // The fixed layout is handled by the FrameView and the drawing area doesn't behave differently
    // whether its fixed or not. We still need to tell the drawing area which part of it
    // has to be rendered on tiles, and in desktop mode it's all of it.
    webPageProxy->drawingArea()->setSize(viewportSize, IntSize());
    webPageProxy->drawingArea()->setVisibleContentsRect(IntRect(IntPoint(), viewportSize), 1, FloatPoint());
}

void QQuickWebViewLegacyPrivate::enableMouseEvents()
{
    Q_Q(QQuickWebView);
    q->setAcceptedMouseButtons(Qt::MouseButtonMask);
    q->setAcceptHoverEvents(true);
}

void QQuickWebViewLegacyPrivate::disableMouseEvents()
{
    Q_Q(QQuickWebView);
    q->setAcceptedMouseButtons(Qt::NoButton);
    q->setAcceptHoverEvents(false);
}

QQuickWebViewFlickablePrivate::QQuickWebViewFlickablePrivate(QQuickWebView* viewport)
    : QQuickWebViewPrivate(viewport)
    , pageIsSuspended(true)
    , loadSuccessDispatchIsPending(false)
{
}

QQuickWebViewFlickablePrivate::~QQuickWebViewFlickablePrivate()
{
    interactionEngine->disconnect();
}

void QQuickWebViewFlickablePrivate::initialize(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    QQuickWebViewPrivate::initialize(contextRef, pageGroupRef);
    webPageProxy->setUseFixedLayout(true);
}

QPointF QQuickWebViewFlickablePrivate::pageItemPos()
{
    // Flickable moves its contentItem so we need to take that position into account,
    // as well as the potential displacement of the page on the contentItem because
    // of additional QML items.
    qreal xPos = flickProvider->contentItem()->x() + pageView->x();
    qreal yPos = flickProvider->contentItem()->y() + pageView->y();
    return QPointF(xPos, yPos);
}

void QQuickWebViewFlickablePrivate::updateContentsSize(const QSizeF& size)
{
    ASSERT(flickProvider);

    // Make sure that the contentItem is sized to the page
    // if the user did not add other flickable items in QML.
    // If the user adds items in QML he has to make sure to
    // also bind the contentWidth and contentHeight accordingly.
    // This is in accordance with normal QML Flickable behaviour.
    if (!userDidOverrideContentWidth)
        flickProvider->setContentWidth(size.width());
    if (!userDidOverrideContentHeight)
        flickProvider->setContentHeight(size.height());
}

void QQuickWebViewFlickablePrivate::onComponentComplete()
{
    Q_Q(QQuickWebView);

    ASSERT(!flickProvider);
    flickProvider = new QtFlickProvider(q, pageView.data());

    // Propagate flickable signals.
    QQuickWebViewExperimental* experimental = q->experimental();
    QObject::connect(flickProvider, SIGNAL(contentWidthChanged()), experimental, SIGNAL(contentWidthChanged()));
    QObject::connect(flickProvider, SIGNAL(contentHeightChanged()), experimental, SIGNAL(contentHeightChanged()));
    QObject::connect(flickProvider, SIGNAL(contentXChanged()), experimental, SIGNAL(contentXChanged()));
    QObject::connect(flickProvider, SIGNAL(contentYChanged()), experimental, SIGNAL(contentYChanged()));

    interactionEngine.reset(new QtViewportInteractionEngine(q, pageView.data(), flickProvider));
    pageView->eventHandler()->setViewportInteractionEngine(interactionEngine.data());

    QObject::connect(interactionEngine.data(), SIGNAL(contentSuspendRequested()), q, SLOT(_q_suspend()));
    QObject::connect(interactionEngine.data(), SIGNAL(contentResumeRequested()), q, SLOT(_q_resume()));
    QObject::connect(interactionEngine.data(), SIGNAL(contentViewportChanged(QPointF)), q, SLOT(_q_contentViewportChanged(QPointF)));

    _q_resume();

    if (loadSuccessDispatchIsPending) {
        QQuickWebViewPrivate::loadDidSucceed();
        loadSuccessDispatchIsPending = false;
    }

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();

    QQuickWebViewPrivate::onComponentComplete();

    emit experimental->flickableChanged();
}

void QQuickWebViewFlickablePrivate::loadDidSucceed()
{
    if (interactionEngine)
        QQuickWebViewPrivate::loadDidSucceed();
    else
        loadSuccessDispatchIsPending = true;

}

void QQuickWebViewFlickablePrivate::loadDidCommit()
{
    // Due to entering provisional load before committing, we
    // might actually be suspended here.
}

void QQuickWebViewFlickablePrivate::didFinishFirstNonEmptyLayout()
{
}

void QQuickWebViewFlickablePrivate::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    viewportArguments = args;

    // FIXME: If suspended we should do this on resume.
    interactionEngine->applyConstraints(computeViewportConstraints());
}

void QQuickWebViewFlickablePrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSize viewportSize = q->boundingRect().size().toSize();

    if (viewportSize.isEmpty() || !interactionEngine)
        return;

    flickProvider->setViewportSize(viewportSize);

    // Let the WebProcess know about the new viewport size, so that
    // it can resize the content accordingly.
    webPageProxy->setViewportSize(viewportSize);

    interactionEngine->applyConstraints(computeViewportConstraints());
    _q_contentViewportChanged(QPointF());
}

void QQuickWebViewFlickablePrivate::_q_contentViewportChanged(const QPointF& trajectoryVector)
{
    Q_Q(QQuickWebView);
    // This is only for our QML ViewportInfo debugging API.
    q->experimental()->viewportInfo()->didUpdateCurrentScale();

    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;

    const QRect visibleRect(visibleContentsRect());
    float scale = pageView->contentsScale();

    drawingArea->setVisibleContentsRect(visibleRect, scale, trajectoryVector);
}

void QQuickWebViewFlickablePrivate::_q_suspend()
{
    pageIsSuspended = true;
    webPageProxy->suspendActiveDOMObjectsAndAnimations();
}

void QQuickWebViewFlickablePrivate::_q_resume()
{
    if (!interactionEngine)
        return;

    pageIsSuspended = false;
    webPageProxy->resumeActiveDOMObjectsAndAnimations();

    _q_contentViewportChanged(QPointF());
}

void QQuickWebViewFlickablePrivate::pageDidRequestScroll(const QPoint& pos)
{
    interactionEngine->pagePositionRequest(pos);
}

void QQuickWebViewFlickablePrivate::didChangeContentsSize(const QSize& newSize)
{
    Q_Q(QQuickWebView);
    pageView->setContentsSize(newSize);
    q->experimental()->viewportInfo()->didUpdateContentsSize();
}

QtViewportInteractionEngine::Constraints QQuickWebViewFlickablePrivate::computeViewportConstraints()
{
    Q_Q(QQuickWebView);

    QtViewportInteractionEngine::Constraints newConstraints;
    QSize availableSize = q->boundingRect().size().toSize();

    // Return default values for zero sized viewport.
    if (availableSize.isEmpty())
        return newConstraints;

    WebPreferences* wkPrefs = webPageProxy->pageGroup()->preferences();

    // FIXME: Remove later; Hardcode a value for now to make sure the DPI adjustment is being tested.
    wkPrefs->setDeviceDPI(160);

    wkPrefs->setDeviceWidth(availableSize.width());
    wkPrefs->setDeviceHeight(availableSize.height());

    int minimumLayoutFallbackWidth = qMax<int>(wkPrefs->layoutFallbackWidth(), availableSize.width());

    WebCore::ViewportAttributes attr = WebCore::computeViewportAttributes(viewportArguments, minimumLayoutFallbackWidth, wkPrefs->deviceWidth(), wkPrefs->deviceHeight(), wkPrefs->deviceDPI(), availableSize);
    WebCore::restrictMinimumScaleFactorToViewportSize(attr, availableSize);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attr);

    newConstraints.initialScale = attr.initialScale;
    newConstraints.minimumScale = attr.minimumScale;
    newConstraints.maximumScale = attr.maximumScale;
    newConstraints.devicePixelRatio = attr.devicePixelRatio;
    newConstraints.isUserScalable = !!attr.userScalable;
    newConstraints.layoutSize = attr.layoutSize;

    q->experimental()->viewportInfo()->didUpdateViewportConstraints();

    return newConstraints;
}

/*!
    \qmlsignal WebView::onNavigationRequested(request)

    This signal is emitted for every navigation request. The request object contains url, button and modifiers properties
    describing the navigation action, e.g. "a middle click with shift key pressed to 'http://qt-project.org'".

    The navigation will be accepted by default. To change that, one can set the action property to WebView.IgnoreRequest to reject
    the request or WebView.DownloadRequest to trigger a download instead of navigating to the url.

    The request object cannot be used after the signal handler function ends.
*/

QQuickWebViewAttached::QQuickWebViewAttached(QObject* object)
    : QObject(object)
    , m_view(0)
{

}

void QQuickWebViewAttached::setView(QQuickWebView* view)
{
    if (m_view == view)
        return;
    m_view = view;
    emit viewChanged();
}

QQuickWebViewExperimental::QQuickWebViewExperimental(QQuickWebView *webView)
    : QObject(webView)
    , q_ptr(webView)
    , d_ptr(webView->d_ptr.data())
    , schemeParent(new QObject(this))
    , m_viewportInfo(new QWebViewportInfo(webView->d_ptr.data(), this))
{
}

QQuickWebViewExperimental::~QQuickWebViewExperimental()
{
}

void QQuickWebViewExperimental::setRenderToOffscreenBuffer(bool enable)
{
    Q_D(QQuickWebView);
    d->setRenderToOffscreenBuffer(enable);
}

bool QQuickWebViewExperimental::renderToOffscreenBuffer() const
{
    Q_D(const QQuickWebView);
    return d->renderToOffscreenBuffer();
}

bool QQuickWebViewExperimental::transparentBackground() const
{
    Q_D(const QQuickWebView);
    return d->transparentBackground();
}
void QQuickWebViewExperimental::setTransparentBackground(bool enable)
{
    Q_D(QQuickWebView);
    d->setTransparentBackground(enable);
}

void QQuickWebViewExperimental::setFlickableViewportEnabled(bool enable)
{
    s_flickableViewportEnabled = enable;
}

bool QQuickWebViewExperimental::flickableViewportEnabled()
{
    return s_flickableViewportEnabled;
}

void QQuickWebViewExperimental::postMessage(const QString& message)
{
    Q_D(QQuickWebView);
    d->context->postMessageToNavigatorQtObject(d->webPageProxy.get(), message);
}

QDeclarativeComponent* QQuickWebViewExperimental::alertDialog() const
{
    Q_D(const QQuickWebView);
    return d->alertDialog;
}

void QQuickWebViewExperimental::setAlertDialog(QDeclarativeComponent* alertDialog)
{
    Q_D(QQuickWebView);
    if (d->alertDialog == alertDialog)
        return;
    d->alertDialog = alertDialog;
    emit alertDialogChanged();
}

QDeclarativeComponent* QQuickWebViewExperimental::confirmDialog() const
{
    Q_D(const QQuickWebView);
    return d->confirmDialog;
}

void QQuickWebViewExperimental::setConfirmDialog(QDeclarativeComponent* confirmDialog)
{
    Q_D(QQuickWebView);
    if (d->confirmDialog == confirmDialog)
        return;
    d->confirmDialog = confirmDialog;
    emit confirmDialogChanged();
}

QWebNavigationHistory* QQuickWebViewExperimental::navigationHistory() const
{
    return d_ptr->navigationHistory.get();
}

QDeclarativeComponent* QQuickWebViewExperimental::promptDialog() const
{
    Q_D(const QQuickWebView);
    return d->promptDialog;
}

QWebPreferences* QQuickWebViewExperimental::preferences() const
{
    QQuickWebViewPrivate* const d = d_ptr;
    if (!d->preferences)
        d->preferences = adoptPtr(QWebPreferencesPrivate::createPreferences(d));
    return d->preferences.get();
}

void QQuickWebViewExperimental::setPromptDialog(QDeclarativeComponent* promptDialog)
{
    Q_D(QQuickWebView);
    if (d->promptDialog == promptDialog)
        return;
    d->promptDialog = promptDialog;
    emit promptDialogChanged();
}

QDeclarativeComponent* QQuickWebViewExperimental::authenticationDialog() const
{
    Q_D(const QQuickWebView);
    return d->authenticationDialog;
}

void QQuickWebViewExperimental::setAuthenticationDialog(QDeclarativeComponent* authenticationDialog)
{
    Q_D(QQuickWebView);
    if (d->authenticationDialog == authenticationDialog)
        return;
    d->authenticationDialog = authenticationDialog;
    emit authenticationDialogChanged();
}

QDeclarativeComponent* QQuickWebViewExperimental::proxyAuthenticationDialog() const
{
    Q_D(const QQuickWebView);
    return d->proxyAuthenticationDialog;
}

void QQuickWebViewExperimental::setProxyAuthenticationDialog(QDeclarativeComponent* proxyAuthenticationDialog)
{
    Q_D(QQuickWebView);
    if (d->proxyAuthenticationDialog == proxyAuthenticationDialog)
        return;
    d->proxyAuthenticationDialog = proxyAuthenticationDialog;
    emit proxyAuthenticationDialogChanged();
}
QDeclarativeComponent* QQuickWebViewExperimental::certificateVerificationDialog() const
{
    Q_D(const QQuickWebView);
    return d->certificateVerificationDialog;
}

void QQuickWebViewExperimental::setCertificateVerificationDialog(QDeclarativeComponent* certificateVerificationDialog)
{
    Q_D(QQuickWebView);
    if (d->certificateVerificationDialog == certificateVerificationDialog)
        return;
    d->certificateVerificationDialog = certificateVerificationDialog;
    emit certificateVerificationDialogChanged();
}

QDeclarativeComponent* QQuickWebViewExperimental::itemSelector() const
{
    Q_D(const QQuickWebView);
    return d->itemSelector;
}

void QQuickWebViewExperimental::setItemSelector(QDeclarativeComponent* itemSelector)
{
    Q_D(QQuickWebView);
    if (d->itemSelector == itemSelector)
        return;
    d->itemSelector = itemSelector;
    emit itemSelectorChanged();
}

QDeclarativeComponent* QQuickWebViewExperimental::filePicker() const
{
    Q_D(const QQuickWebView);
    return d->filePicker;
}

void QQuickWebViewExperimental::setFilePicker(QDeclarativeComponent* filePicker)
{
    Q_D(QQuickWebView);
    if (d->filePicker == filePicker)
        return;
    d->filePicker = filePicker;
    emit filePickerChanged();
}

QDeclarativeComponent* QQuickWebViewExperimental::databaseQuotaDialog() const
{
    Q_D(const QQuickWebView);
    return d->databaseQuotaDialog;
}

void QQuickWebViewExperimental::setDatabaseQuotaDialog(QDeclarativeComponent* databaseQuotaDialog)
{
    Q_D(QQuickWebView);
    if (d->databaseQuotaDialog == databaseQuotaDialog)
        return;
    d->databaseQuotaDialog = databaseQuotaDialog;
    emit databaseQuotaDialogChanged();
}

QString QQuickWebViewExperimental::userAgent() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->userAgent();
}

void QQuickWebViewExperimental::setUserAgent(const QString& userAgent)
{
    Q_D(QQuickWebView);
    if (userAgent == QString(d->webPageProxy->userAgent()))
        return;

    d->webPageProxy->setUserAgent(userAgent);
    emit userAgentChanged();
}

QQuickUrlSchemeDelegate* QQuickWebViewExperimental::schemeDelegates_At(QDeclarativeListProperty<QQuickUrlSchemeDelegate>* property, int index)
{
    const QObjectList children = property->object->children();
    if (index < children.count())
        return static_cast<QQuickUrlSchemeDelegate*>(children.at(index));
    return 0;
}

void QQuickWebViewExperimental::schemeDelegates_Append(QDeclarativeListProperty<QQuickUrlSchemeDelegate>* property, QQuickUrlSchemeDelegate *scheme)
{
    QObject* schemeParent = property->object;
    scheme->setParent(schemeParent);
    QQuickWebViewExperimental* webViewExperimental = qobject_cast<QQuickWebViewExperimental*>(property->object->parent());
    if (!webViewExperimental)
        return;
    scheme->reply()->setWebViewExperimental(webViewExperimental);
    QQuickWebViewPrivate* d = webViewExperimental->d_func();
    d->webPageProxy->registerApplicationScheme(scheme->scheme());
}

int QQuickWebViewExperimental::schemeDelegates_Count(QDeclarativeListProperty<QQuickUrlSchemeDelegate>* property)
{
    return property->object->children().count();
}

void QQuickWebViewExperimental::schemeDelegates_Clear(QDeclarativeListProperty<QQuickUrlSchemeDelegate>* property)
{
    const QObjectList children = property->object->children();
    for (int index = 0; index < children.count(); index++) {
        QObject* child = children.at(index);
        child->setParent(0);
        delete child;
    }
}

QDeclarativeListProperty<QQuickUrlSchemeDelegate> QQuickWebViewExperimental::schemeDelegates()
{
    return QDeclarativeListProperty<QQuickUrlSchemeDelegate>(schemeParent, 0,
            QQuickWebViewExperimental::schemeDelegates_Append,
            QQuickWebViewExperimental::schemeDelegates_Count,
            QQuickWebViewExperimental::schemeDelegates_At,
            QQuickWebViewExperimental::schemeDelegates_Clear);
}

void QQuickWebViewExperimental::invokeApplicationSchemeHandler(PassRefPtr<QtRefCountedNetworkRequestData> request)
{
    RefPtr<QtRefCountedNetworkRequestData> req = request;
    const QObjectList children = schemeParent->children();
    for (int index = 0; index < children.count(); index++) {
        QQuickUrlSchemeDelegate* delegate = qobject_cast<QQuickUrlSchemeDelegate*>(children.at(index));
        if (!delegate)
            continue;
        if (!delegate->scheme().compare(QString(req->data().m_scheme), Qt::CaseInsensitive)) {
            delegate->request()->setNetworkRequestData(req);
            delegate->reply()->setNetworkRequestData(req);
            emit delegate->receivedRequest();
            return;
        }
    }
}

void QQuickWebViewExperimental::sendApplicationSchemeReply(QQuickNetworkReply* reply)
{
    d_ptr->webPageProxy->sendApplicationSchemeReply(reply);
}

void QQuickWebViewExperimental::goForwardTo(int index)
{
    d_ptr->navigationHistory->d->goForwardTo(index);
}

void QQuickWebViewExperimental::goBackTo(int index)
{
    d_ptr->navigationHistory->d->goBackTo(index);
}

QWebViewportInfo* QQuickWebViewExperimental::viewportInfo()
{
    return m_viewportInfo;
}

QQuickWebPage* QQuickWebViewExperimental::page()
{
    return q_ptr->page();
}

QDeclarativeListProperty<QObject> QQuickWebViewExperimental::flickableData()
{
    Q_D(const QQuickWebView);
    ASSERT(d->flickProvider);
    return d->flickProvider->flickableData();
}

QQuickFlickable* QQuickWebViewExperimental::flickable()
{
    Q_D(QQuickWebView);
    if (!d->flickProvider)
        return 0;

    QQuickFlickable* flickableItem = qobject_cast<QQuickFlickable*>(contentItem()->parentItem());

    ASSERT(flickableItem);

    return flickableItem;
}

QQuickItem* QQuickWebViewExperimental::contentItem()
{
    Q_D(QQuickWebView);
    ASSERT(d->flickProvider);
    return d->flickProvider->contentItem();
}

qreal QQuickWebViewExperimental::contentWidth() const
{
    Q_D(const QQuickWebView);
    ASSERT(d->flickProvider);
    return d->flickProvider->contentWidth();
}

void QQuickWebViewExperimental::setContentWidth(qreal width)
{
    Q_D(QQuickWebView);
    ASSERT(d->flickProvider);
    d->userDidOverrideContentWidth = true;
    d->flickProvider->setContentWidth(width);
}

qreal QQuickWebViewExperimental::contentHeight() const
{
    Q_D(const QQuickWebView);
    ASSERT(d->flickProvider);
    return d->flickProvider->contentHeight();
}

void QQuickWebViewExperimental::setContentHeight(qreal height)
{
    Q_D(QQuickWebView);
    ASSERT(d->flickProvider);
    d->userDidOverrideContentHeight = true;
    d->flickProvider->setContentHeight(height);
}

qreal QQuickWebViewExperimental::contentX() const
{
    Q_D(const QQuickWebView);
    ASSERT(d->flickProvider);
    return d->flickProvider->contentX();
}

void QQuickWebViewExperimental::setContentX(qreal x)
{
    Q_D(QQuickWebView);
    ASSERT(d->flickProvider);
    d->flickProvider->setContentX(x);
}

qreal QQuickWebViewExperimental::contentY() const
{
    Q_D(const QQuickWebView);
    ASSERT(d->flickProvider);
    return d->flickProvider->contentY();
}

void QQuickWebViewExperimental::setContentY(qreal y)
{
    Q_D(QQuickWebView);
    ASSERT(d->flickProvider);
    d->flickProvider->setContentY(y);
}

QQuickWebView::QQuickWebView(QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(createPrivateObject(this))
    , m_experimental(new QQuickWebViewExperimental(this))
{
    Q_D(QQuickWebView);
    d->initialize();
}

QQuickWebView::QQuickWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(createPrivateObject(this))
    , m_experimental(new QQuickWebViewExperimental(this))
{
    Q_D(QQuickWebView);
    d->initialize(contextRef, pageGroupRef);
    setClip(true);
}

QQuickWebView::~QQuickWebView()
{
}

QQuickWebPage* QQuickWebView::page()
{
    Q_D(QQuickWebView);
    return d->pageView.data();
}

void QQuickWebView::goBack()
{
    Q_D(QQuickWebView);
    d->webPageProxy->goBack();
}

void QQuickWebView::goForward()
{
    Q_D(QQuickWebView);
    d->webPageProxy->goForward();
}

void QQuickWebView::stop()
{
    Q_D(QQuickWebView);
    d->webPageProxy->stopLoading();
}

void QQuickWebView::reload()
{
    Q_D(QQuickWebView);
    const bool reloadFromOrigin = true;
    d->webPageProxy->reload(reloadFromOrigin);
}

QUrl QQuickWebView::url() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebFrameProxy> mainFrame = d->webPageProxy->mainFrame();
    if (!mainFrame)
        return QUrl();
    return QUrl(QString(mainFrame->url()));
}

void QQuickWebView::setUrl(const QUrl& url)
{
    Q_D(QQuickWebView);

    if (url.isEmpty())
        return;

    if (!isComponentComplete()) {
        d->m_deferedUrlToLoad = url;
        return;
    }

    d->webPageProxy->loadURL(url.toString());
}

QUrl QQuickWebView::icon() const
{
    Q_D(const QQuickWebView);
    return d->m_iconURL;
}

int QQuickWebView::loadProgress() const
{
    Q_D(const QQuickWebView);
    return d->pageLoadClient->loadProgress();
}

bool QQuickWebView::canGoBack() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->canGoBack();
}

bool QQuickWebView::canGoForward() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->canGoForward();
}

bool QQuickWebView::loading() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebKit::WebFrameProxy> mainFrame = d->webPageProxy->mainFrame();
    return mainFrame && !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
}

QPointF QQuickWebView::mapToWebContent(const QPointF& pointInViewCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformFromItem().map(pointInViewCoordinates);
}

QRectF QQuickWebView::mapRectToWebContent(const QRectF& rectInViewCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformFromItem().mapRect(rectInViewCoordinates);
}

QPointF QQuickWebView::mapFromWebContent(const QPointF& pointInCSSCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformToItem().map(pointInCSSCoordinates);
}

QRectF QQuickWebView::mapRectFromWebContent(const QRectF& rectInCSSCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformToItem().mapRect(rectInCSSCoordinates);
}

QString QQuickWebView::title() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->pageTitle();
}

QVariant QQuickWebView::inputMethodQuery(Qt::InputMethodQuery property) const
{
    Q_D(const QQuickWebView);
    const EditorState& state = d->webPageProxy->editorState();

    switch(property) {
    case Qt::ImCursorRectangle:
        return QRectF(state.cursorRect);
    case Qt::ImFont:
        return QVariant();
    case Qt::ImCursorPosition:
        return QVariant(static_cast<int>(state.cursorPosition));
    case Qt::ImAnchorPosition:
        return QVariant(static_cast<int>(state.anchorPosition));
    case Qt::ImSurroundingText:
        return QString(state.surroundingText);
    case Qt::ImCurrentSelection:
        return QString(state.selectedText);
    case Qt::ImMaximumTextLength:
        return QVariant(); // No limit.
    case Qt::ImHints:
        return int(Qt::InputMethodHints(state.inputMethodHints));
    default:
        // Rely on the base implementation for ImEnabled, ImHints and ImPreferredLanguage.
        return QQuickItem::inputMethodQuery(property);
    }
}

QQuickWebViewExperimental* QQuickWebView::experimental() const
{
    return m_experimental;
}

QQuickWebViewAttached* QQuickWebView::qmlAttachedProperties(QObject* object)
{
    return new QQuickWebViewAttached(object);
}

void QQuickWebView::platformInitialize()
{
    JSC::initializeThreading();
    WTF::initializeMainThread();
}

void QQuickWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_D(QQuickWebView);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->updateViewportSize();
}

void QQuickWebView::componentComplete()
{
    Q_D(QQuickWebView);
    QQuickItem::componentComplete();

    d->onComponentComplete();
    d->updateViewportSize();
}

void QQuickWebView::keyPressEvent(QKeyEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleKeyPressEvent(event);
}

void QQuickWebView::keyReleaseEvent(QKeyEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleKeyReleaseEvent(event);
}

void QQuickWebView::inputMethodEvent(QInputMethodEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleInputMethodEvent(event);
}

void QQuickWebView::focusInEvent(QFocusEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleFocusInEvent(event);
}

void QQuickWebView::focusOutEvent(QFocusEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleFocusOutEvent(event);
}

void QQuickWebView::touchEvent(QTouchEvent* event)
{
    Q_D(QQuickWebView);
    if (d->m_dialogActive) {
        event->ignore();
        return;
    }

    forceActiveFocus();
    d->pageView->eventHandler()->handleTouchEvent(event);
}

void QQuickWebView::mousePressEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    forceActiveFocus();
    d->pageView->eventHandler()->handleMousePressEvent(event);
}

void QQuickWebView::mouseMoveEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleMouseMoveEvent(event);
}

void QQuickWebView::mouseReleaseEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleMouseReleaseEvent(event);
}

void QQuickWebView::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    // If a MouseButtonDblClick was received then we got a MouseButtonPress before
    // handleMousePressEvent will take care of double clicks.
    d->pageView->eventHandler()->handleMousePressEvent(event);
}

void QQuickWebView::wheelEvent(QWheelEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleWheelEvent(event);
}

void QQuickWebView::hoverEnterEvent(QHoverEvent* event)
{
    Q_D(QQuickWebView);
    // Map HoverEnter to Move, for WebKit the distinction doesn't matter.
    d->pageView->eventHandler()->handleHoverMoveEvent(event);
}

void QQuickWebView::hoverMoveEvent(QHoverEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleHoverMoveEvent(event);
}

void QQuickWebView::hoverLeaveEvent(QHoverEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleHoverLeaveEvent(event);
}

void QQuickWebView::dragMoveEvent(QDragMoveEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleDragMoveEvent(event);
}

void QQuickWebView::dragEnterEvent(QDragEnterEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleDragEnterEvent(event);
}

void QQuickWebView::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleDragLeaveEvent(event);
}

void QQuickWebView::dropEvent(QDropEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->eventHandler()->handleDropEvent(event);
}

bool QQuickWebView::event(QEvent* ev)
{
    // Re-implemented for possible future use without breaking binary compatibility.
    return QQuickItem::event(ev);
}

WKPageRef QQuickWebView::pageRef() const
{
    Q_D(const QQuickWebView);
    return toAPI(d->webPageProxy.get());
}

/*!
    Loads the specified \a html as the content of the web view.

    External objects such as stylesheets or images referenced in the HTML
    document are located relative to \a baseUrl.

    \sa load()
*/
void QQuickWebView::loadHtml(const QString& html, const QUrl& baseUrl)
{
    Q_D(QQuickWebView);
    d->webPageProxy->loadHTMLString(html, baseUrl.toString());
}

QPointF QQuickWebView::pageItemPos()
{
    Q_D(QQuickWebView);
    return d->pageItemPos();
}

void QQuickWebView::updateContentsSize(const QSizeF& size)
{
    Q_D(QQuickWebView);
    d->updateContentsSize(size);
}

#include "moc_qquickwebview_p.cpp"
