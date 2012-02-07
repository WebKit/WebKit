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
#include "QtDialogRunner.h"
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
#include "qwebnavigationhistory_p.h"
#include "qwebnavigationhistory_p_p.h"
#include "qwebpreferences_p.h"
#include "qwebpreferences_p_p.h"
#include "qwebviewportinfo_p.h"

#include <JavaScriptCore/InitializeThreading.h>
#include <QDeclarativeEngine>
#include <QFileDialog>
#include <QtQuick/QQuickCanvas>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WKOpenPanelResultListener.h>
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
    , alertDialog(0)
    , confirmDialog(0)
    , promptDialog(0)
    , authenticationDialog(0)
    , certificateVerificationDialog(0)
    , itemSelector(0)
    , m_navigatorQtObjectEnabled(false)
    , m_renderToOffscreenBuffer(false)
{
    viewport->setFlags(QQuickItem::ItemClipsChildrenToShape);
    QObject::connect(viewport, SIGNAL(visibleChanged()), viewport, SLOT(_q_onVisibleChanged()));
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
    QObject::connect(q_ptr, SIGNAL(urlChanged(QUrl)), iconDatabase, SLOT(requestIconForPageURL(QUrl)));

    // Any page setting should preferrable be set before creating the page.
    webPageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    webPageProxy->pageGroup()->preferences()->setForceCompositingMode(true);

    pageClient.initialize(q_ptr, pageViewPrivate->eventHandler.data(), &undoController);
    webPageProxy->initializeWebPage();
}

void QQuickWebViewPrivate::enableMouseEvents()
{
    Q_Q(QQuickWebView);
    q->setAcceptedMouseButtons(Qt::MouseButtonMask);
    q->setAcceptHoverEvents(true);
}

void QQuickWebViewPrivate::disableMouseEvents()
{
    Q_Q(QQuickWebView);
    q->setAcceptedMouseButtons(Qt::NoButton);
    q->setAcceptHoverEvents(false);
}

void QQuickWebViewPrivate::loadDidSucceed()
{
    Q_Q(QQuickWebView);
    emit q->navigationStateChanged();
    emit q->loadSucceeded();
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

void QQuickWebViewPrivate::didChangeBackForwardList()
{
    navigationHistory->d->reset();
}

void QQuickWebViewPrivate::processDidCrash()
{
    emit q_ptr->navigationStateChanged();
    pageView->eventHandler()->resetGestureRecognizers();
    WebCore::KURL url(WebCore::ParsedURLString, webPageProxy->urlAtProcessExit());
    qWarning("WARNING: The web process experienced a crash on '%s'.", qPrintable(QUrl(url).toString(QUrl::RemoveUserInfo)));
}

void QQuickWebViewPrivate::didRelaunchProcess()
{
    emit q_ptr->navigationStateChanged();
    qWarning("WARNING: The web process has been successfully restarted.");
    pageView->d->setDrawingAreaSize(viewSize());
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

void QQuickWebViewPrivate::_q_viewportTrajectoryVectorChanged(const QPointF& trajectoryVector)
{
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;
    drawingArea->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QQuickWebViewPrivate::_q_onVisibleChanged()
{
    webPageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
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
    setViewInAttachedProperties(dialogRunner.dialog());

    disableMouseEvents();
    dialogRunner.exec();
    enableMouseEvents();
}

bool QQuickWebViewPrivate::runJavaScriptConfirm(const QString& message)
{
    if (!confirmDialog)
        return true;

    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner;
    if (!dialogRunner.initForConfirm(confirmDialog, q, message))
        return true;
    setViewInAttachedProperties(dialogRunner.dialog());

    disableMouseEvents();
    dialogRunner.exec();
    enableMouseEvents();

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
    setViewInAttachedProperties(dialogRunner.dialog());

    disableMouseEvents();
    dialogRunner.exec();
    enableMouseEvents();

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

    setViewInAttachedProperties(dialogRunner.dialog());

    disableMouseEvents();
    dialogRunner.exec();
    enableMouseEvents();

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

    setViewInAttachedProperties(dialogRunner.dialog());

    disableMouseEvents();
    dialogRunner.exec();
    enableMouseEvents();

    return dialogRunner.wasAccepted();
}

void QQuickWebViewPrivate::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, QtWebPageUIClient::FileChooserType type)
{
#ifndef QT_NO_FILEDIALOG
    Q_Q(QQuickWebView);
    openPanelResultListener = listenerRef;

    // Qt does not support multiple files suggestion, so we get just the first suggestion.
    QString selectedFileName;
    if (!selectedFileNames.isEmpty())
        selectedFileName = selectedFileNames.at(0);

    Q_ASSERT(!fileDialog);

    QWindow* window = q->canvas();
    if (!window)
        return;

    fileDialog = new QFileDialog(0, QString(), selectedFileName);
    fileDialog->window()->winId(); // Ensure that the dialog has a window
    Q_ASSERT(fileDialog->window()->windowHandle());
    fileDialog->window()->windowHandle()->setTransientParent(window);

    fileDialog->open(q, SLOT(_q_onOpenPanelFilesSelected()));

    q->connect(fileDialog, SIGNAL(finished(int)), SLOT(_q_onOpenPanelFinished(int)));
#endif
}

void QQuickWebViewPrivate::_q_onOpenPanelFilesSelected()
{
    const QStringList fileList = fileDialog->selectedFiles();
    Vector<RefPtr<APIObject> > wkFiles(fileList.size());

    for (unsigned i = 0; i < fileList.size(); ++i)
        wkFiles[i] = WebURL::create(QUrl::fromLocalFile(fileList.at(i)).toString());

    WKOpenPanelResultListenerChooseFiles(openPanelResultListener, toAPI(ImmutableArray::adopt(wkFiles).leakRef()));
}

void QQuickWebViewPrivate::_q_onOpenPanelFinished(int result)
{
    if (result == QDialog::Rejected)
        WKOpenPanelResultListenerCancel(openPanelResultListener);

    fileDialog->deleteLater();
    fileDialog = 0;
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
    emit q->iconChanged(m_iconURL);
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
    webPageProxy->drawingArea()->setVisibleContentsRectAndScale(IntRect(IntPoint(), viewportSize), 1);
}

QQuickWebViewFlickablePrivate::QQuickWebViewFlickablePrivate(QQuickWebView* viewport)
    : QQuickWebViewPrivate(viewport)
    , postTransitionState(adoptPtr(new PostTransitionState(this)))
    , isTransitioningToNewPage(false)
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

void QQuickWebViewFlickablePrivate::onComponentComplete()
{
    Q_Q(QQuickWebView);
    interactionEngine.reset(new QtViewportInteractionEngine(q, pageView.data()));
    pageView->eventHandler()->setViewportInteractionEngine(interactionEngine.data());

    QObject::connect(interactionEngine.data(), SIGNAL(contentSuspendRequested()), q, SLOT(_q_suspend()));
    QObject::connect(interactionEngine.data(), SIGNAL(contentResumeRequested()), q, SLOT(_q_resume()));
    QObject::connect(interactionEngine.data(), SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), q, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
    QObject::connect(interactionEngine.data(), SIGNAL(visibleContentRectAndScaleChanged()), q, SLOT(_q_updateVisibleContentRectAndScale()));

    _q_resume();

    if (loadSuccessDispatchIsPending) {
        QQuickWebViewPrivate::loadDidSucceed();
        loadSuccessDispatchIsPending = false;
    }

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();
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

    isTransitioningToNewPage = true;
}

void QQuickWebViewFlickablePrivate::didFinishFirstNonEmptyLayout()
{
    if (!pageIsSuspended) {
        isTransitioningToNewPage = false;
        postTransitionState->apply();
    }
}

void QQuickWebViewFlickablePrivate::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    viewportArguments = args;

    if (isTransitioningToNewPage)
        return;

    interactionEngine->applyConstraints(computeViewportConstraints());
}

void QQuickWebViewFlickablePrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSize viewportSize = q->boundingRect().size().toSize();

    if (viewportSize.isEmpty() || !interactionEngine)
        return;

    // Let the WebProcess know about the new viewport size, so that
    // it can resize the content accordingly.
    webPageProxy->setViewportSize(viewportSize);

    interactionEngine->applyConstraints(computeViewportConstraints());
    _q_updateVisibleContentRectAndScale();
}

void QQuickWebViewFlickablePrivate::_q_updateVisibleContentRectAndScale()
{
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;

    Q_Q(QQuickWebView);
    const QRectF visibleRectInCSSCoordinates = q->mapRectToWebContent(q->boundingRect()).intersected(pageView->boundingRect());
    float scale = pageView->contentsScale();

    QRect alignedVisibleContentRect = visibleRectInCSSCoordinates.toAlignedRect();
    drawingArea->setVisibleContentsRectAndScale(alignedVisibleContentRect, scale);

    // FIXME: Once we support suspend and resume, this should be delayed until the page is active if the page is suspended.
    webPageProxy->setFixedVisibleContentRect(alignedVisibleContentRect);
    q->experimental()->viewportInfo()->didUpdateCurrentScale();
}

void QQuickWebViewFlickablePrivate::_q_suspend()
{
    pageIsSuspended = true;
}

void QQuickWebViewFlickablePrivate::_q_resume()
{
    if (!interactionEngine)
        return;

    pageIsSuspended = false;

    if (isTransitioningToNewPage) {
        isTransitioningToNewPage = false;
        postTransitionState->apply();
    }

    _q_updateVisibleContentRectAndScale();
}

void QQuickWebViewFlickablePrivate::pageDidRequestScroll(const QPoint& pos)
{
    if (isTransitioningToNewPage) {
        postTransitionState->position = pos;
        return;
    }

    interactionEngine->pagePositionRequest(pos);
}

void QQuickWebViewFlickablePrivate::didChangeContentsSize(const QSize& newSize)
{
    Q_Q(QQuickWebView);
    // FIXME: We probably want to handle suspend here as well
    if (isTransitioningToNewPage) {
        postTransitionState->contentsSize = newSize;
        return;
    }

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

    // FIXME: Remove later; Hardcode some values for now to make sure the DPI adjustment is being tested.
    wkPrefs->setDeviceDPI(240);
    wkPrefs->setDeviceWidth(480);
    wkPrefs->setDeviceHeight(720);

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

void QQuickWebViewFlickablePrivate::PostTransitionState::apply()
{
    p->interactionEngine->reset();
    p->interactionEngine->applyConstraints(p->computeViewportConstraints());
    p->interactionEngine->pagePositionRequest(position);

    if (contentsSize.isValid()) {
        p->pageView->setContentsSize(contentsSize);
        p->q_ptr->experimental()->viewportInfo()->didUpdateContentsSize();
    }

    position = QPoint();
    contentsSize = QSize();
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
}

QQuickWebView::~QQuickWebView()
{
}

QQuickWebPage* QQuickWebView::page()
{
    Q_D(QQuickWebView);
    return d->pageView.data();
}

void QQuickWebView::load(const QUrl& url)
{
    if (url.isEmpty())
        return;

    Q_D(QQuickWebView);
    d->webPageProxy->loadURL(url.toString());
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

bool QQuickWebView::canReload() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebKit::WebFrameProxy> mainFrame = d->webPageProxy->mainFrame();
    if (mainFrame)
        return (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
    return d->webPageProxy->backForwardList()->currentItem();
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
    this->event(event);
}

void QQuickWebView::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QQuickWebView::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QQuickWebView::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QQuickWebView::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QQuickWebView::touchEvent(QTouchEvent* event)
{
    forceActiveFocus();
    this->event(event);
}

void QQuickWebView::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus();
    this->event(event);
}

void QQuickWebView::mouseMoveEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebView::mouseReleaseEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebView::mouseDoubleClickEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebView::wheelEvent(QWheelEvent* event)
{
    this->event(event);
}

void QQuickWebView::hoverEnterEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebView::hoverMoveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebView::hoverLeaveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebView::dragMoveEvent(QDragMoveEvent* event)
{
    this->event(event);
}

void QQuickWebView::dragEnterEvent(QDragEnterEvent* event)
{
    this->event(event);
}

void QQuickWebView::dragLeaveEvent(QDragLeaveEvent* event)
{
    this->event(event);
}

void QQuickWebView::dropEvent(QDropEvent* event)
{
    this->event(event);
}

bool QQuickWebView::event(QEvent* ev)
{
    Q_D(QQuickWebView);

    switch (ev->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::HoverLeave:
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
    case QEvent::DragEnter:
    case QEvent::DragLeave:
    case QEvent::DragMove:
    case QEvent::Drop:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
        if (d->pageView->eventHandler()->handleEvent(ev))
            return true;
    }

    if (ev->type() == QEvent::InputMethod)
        return false; // This is necessary to avoid an endless loop in connection with QQuickItem::event().

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

#include "moc_qquickwebview_p.cpp"
