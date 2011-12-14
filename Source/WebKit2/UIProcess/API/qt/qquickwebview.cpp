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

#include "DrawingAreaProxyImpl.h"
#include "QtDialogRunner.h"
#include "QtWebPageEventHandler.h"
#include "QtWebPageProxy.h"
#include "UtilsQt.h"
#include "WebBackForwardList.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"

#include "qquickwebpage_p_p.h"
#include "qquickwebview_p_p.h"
#include "qwebnavigationhistory_p.h"
#include "qwebnavigationhistory_p_p.h"
#include "qwebpreferences_p_p.h"

#include <JavaScriptCore/InitializeThreading.h>
#include <QFileDialog>
#include <QtQuick/QQuickCanvas>
#include <WKOpenPanelResultListener.h>

QQuickWebViewPrivate::QQuickWebViewPrivate(QQuickWebView* viewport)
    : q_ptr(viewport)
    , alertDialog(0)
    , confirmDialog(0)
    , promptDialog(0)
    , itemSelector(0)
    , postTransitionState(adoptPtr(new PostTransitionState(this)))
    , isTransitioningToNewPage(false)
    , pageIsSuspended(false)
{
    viewport->setFlags(QQuickItem::ItemClipsChildrenToShape);
    QObject::connect(viewport, SIGNAL(visibleChanged()), viewport, SLOT(_q_onVisibleChanged()));
    pageView.reset(new QQuickWebPage(viewport));
}

QQuickWebViewPrivate::~QQuickWebViewPrivate()
{
    if (interactionEngine)
        interactionEngine->disconnect();
}

// Note: we delay this initialization to make sure that QQuickWebView has its d-ptr in-place.
void QQuickWebViewPrivate::initialize(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    QQuickWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    setPageProxy(new QtWebPageProxy(q_ptr, &pageClient, contextRef, pageGroupRef));
    pageViewPrivate->initialize(webPageProxy());

    pageLoadClient.reset(new QtWebPageLoadClient(pageProxy->pageRef(), q_ptr));
    pagePolicyClient.reset(new QtWebPagePolicyClient(pageProxy->pageRef(), q_ptr));
    pageUIClient.reset(new QtWebPageUIClient(pageProxy->pageRef(), q_ptr));
    navigationHistory = adoptPtr(QWebNavigationHistoryPrivate::createHistory(pageProxy->pageRef()));

    // Any page setting should preferrable be set before creating the page, so set them here:
    setUseTraditionalDesktopBehaviour(false);
    QWebPreferencesPrivate::get(pageProxy->preferences())->setAttribute(QWebPreferencesPrivate::AcceleratedCompositingEnabled, true);

    pageClient.initialize(q_ptr, pageProxy.data(), pageViewPrivate->eventHandler.data(), &undoController);

    // Creates a page with the page creation parameters.
    pageProxy->init();

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();
}

void QQuickWebViewPrivate::enableMouseEvents()
{
    Q_Q(QQuickWebView);
    q->setAcceptedMouseButtons(Qt::MouseButtonMask);
    q->setAcceptHoverEvents(true);
    pageView->setAcceptedMouseButtons(Qt::MouseButtonMask);
    pageView->setAcceptHoverEvents(true);
}

void QQuickWebViewPrivate::disableMouseEvents()
{
    Q_Q(QQuickWebView);
    q->setAcceptedMouseButtons(Qt::NoButton);
    q->setAcceptHoverEvents(false);
    pageView->setAcceptedMouseButtons(Qt::NoButton);
    pageView->setAcceptHoverEvents(false);
}

void QQuickWebViewPrivate::initializeDesktop(QQuickWebView* viewport)
{
    if (interactionEngine) {
        QObject::disconnect(interactionEngine.data(), SIGNAL(contentSuspendRequested()), viewport, SLOT(_q_suspend()));
        QObject::disconnect(interactionEngine.data(), SIGNAL(contentResumeRequested()), viewport, SLOT(_q_resume()));
        QObject::disconnect(interactionEngine.data(), SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), viewport, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
    }
    interactionEngine.reset(0);
    pageView->d->eventHandler->setViewportInteractionEngine(0);
    enableMouseEvents();
}

void QQuickWebViewPrivate::initializeTouch(QQuickWebView* viewport)
{
    interactionEngine.reset(new QtViewportInteractionEngine(viewport, pageView.data()));
    pageView->d->eventHandler->setViewportInteractionEngine(interactionEngine.data());
    disableMouseEvents();
    QObject::connect(interactionEngine.data(), SIGNAL(contentSuspendRequested()), viewport, SLOT(_q_suspend()));
    QObject::connect(interactionEngine.data(), SIGNAL(contentResumeRequested()), viewport, SLOT(_q_resume()));
    QObject::connect(interactionEngine.data(), SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), viewport, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
    updateViewportSize();
}

void QQuickWebViewPrivate::loadDidCommit()
{
    // Due to entering provisional load before committing, we
    // might actually be suspended here.

    if (useTraditionalDesktopBehaviour)
        return;

    isTransitioningToNewPage = true;
}

void QQuickWebViewPrivate::didFinishFirstNonEmptyLayout()
{
    if (useTraditionalDesktopBehaviour)
        return;

    if (!pageIsSuspended) {
        isTransitioningToNewPage = false;
        postTransitionState->apply();
    }
}

void QQuickWebViewPrivate::_q_suspend()
{
    pageIsSuspended = true;
}

void QQuickWebViewPrivate::_q_resume()
{
    pageIsSuspended = false;

    if (isTransitioningToNewPage) {
        isTransitioningToNewPage = false;
        postTransitionState->apply();
    }

    updateVisibleContentRectAndScale();
}

void QQuickWebViewPrivate::didChangeContentsSize(const QSize& newSize)
{
    if (useTraditionalDesktopBehaviour)
        return;

    // FIXME: We probably want to handle suspend here as well
    if (isTransitioningToNewPage) {
        postTransitionState->contentsSize = newSize;
        return;
    }

    pageView->setWidth(newSize.width());
    pageView->setHeight(newSize.height());
}

void QQuickWebViewPrivate::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    if (useTraditionalDesktopBehaviour)
        return;

    viewportArguments = args;

    if (isTransitioningToNewPage)
        return;

    interactionEngine->applyConstraints(computeViewportConstraints());
}

void QQuickWebViewPrivate::didChangeBackForwardList()
{
    navigationHistory->d->reset();
}

void QQuickWebViewPrivate::pageDidRequestScroll(const QPoint& pos)
{
    if (useTraditionalDesktopBehaviour)
        return;

    if (isTransitioningToNewPage) {
        postTransitionState->position = pos;
        return;
    }

    interactionEngine->pagePositionRequest(pos);
}

void QQuickWebViewPrivate::processDidCrash()
{
    emit q_ptr->navigationStateChanged();
    pageView->d->eventHandler->resetGestureRecognizers();
    WebCore::KURL url(WebCore::ParsedURLString, webPageProxy()->urlAtProcessExit());
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
    return DrawingAreaProxyImpl::create(webPageProxy());
}

void QQuickWebViewPrivate::updateVisibleContentRectAndScale()
{
    DrawingAreaProxy* drawingArea = webPageProxy()->drawingArea();
    if (!drawingArea)
        return;

    Q_Q(QQuickWebView);
    const QRectF visibleRectInPageViewCoordinates = q->mapRectToItem(pageView.data(), q->boundingRect()).intersected(pageView->boundingRect());
    float scale = pageView->scale();

    QRect alignedVisibleContentRect = visibleRectInPageViewCoordinates.toAlignedRect();
    drawingArea->setVisibleContentsRectAndScale(alignedVisibleContentRect, scale);

    // FIXME: Once we support suspend and resume, this should be delayed until the page is active if the page is suspended.
    webPageProxy()->setFixedVisibleContentRect(alignedVisibleContentRect);
}

void QQuickWebViewPrivate::_q_viewportTrajectoryVectorChanged(const QPointF& trajectoryVector)
{
    DrawingAreaProxy* drawingArea = webPageProxy()->drawingArea();
    if (!drawingArea)
        return;
    drawingArea->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QQuickWebViewPrivate::_q_onVisibleChanged()
{
    webPageProxy()->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

void QQuickWebViewPrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSize viewportSize = q->boundingRect().size().toSize();

    if (viewportSize.isEmpty())
        return;

    // Let the WebProcess know about the new viewport size, so that
    // it can resize the content accordingly.
    webPageProxy()->setViewportSize(viewportSize);

    interactionEngine->applyConstraints(computeViewportConstraints());
    updateVisibleContentRectAndScale();
}

QtViewportInteractionEngine::Constraints QQuickWebViewPrivate::computeViewportConstraints()
{
    Q_Q(QQuickWebView);

    QtViewportInteractionEngine::Constraints newConstraints;
    QSize availableSize = q->boundingRect().size().toSize();

    // Return default values for zero sized viewport.
    if (availableSize.isEmpty())
        return newConstraints;

    WebPreferences* wkPrefs = webPageProxy()->pageGroup()->preferences();

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

    return newConstraints;
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

void QQuickWebViewPrivate::setUseTraditionalDesktopBehaviour(bool enable)
{
    Q_Q(QQuickWebView);

    // Do not guard, testing for the same value, as we call this from the constructor.

    webPageProxy()->setUseFixedLayout(!enable);

    useTraditionalDesktopBehaviour = enable;
    if (useTraditionalDesktopBehaviour)
        initializeDesktop(q);
    else
        initializeTouch(q);
}

void QQuickWebViewPrivate::setViewInAttachedProperties(QObject* object)
{
    Q_Q(QQuickWebView);
    QQuickWebViewAttached* attached = static_cast<QQuickWebViewAttached*>(qmlAttachedPropertiesObject<QQuickWebView>(object));
    attached->setView(q);
}

// FIXME: Remove this once QtWebPageProxy is removed.
WebKit::WebPageProxy* QQuickWebViewPrivate::webPageProxy() const
{
    return toImpl(pageProxy->pageRef());
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

/*!
    \qmlsignal WebView::onNavigationRequested(request)

    This signal is emitted for every navigation request. The request object contains url, button and modifiers properties
    describing the navigation action, e.g. "a middle click with shift key pressed to 'http://qt-project.org'".

    The navigation will be accepted by default. To change that, one can set the action property to WebView.IgnoreRequest to reject
    the request or WebView.DownloadRequest to trigger a download instead of navigating to the url.

    The request object cannot be used after the signal handler function ends.
*/

void QQuickWebViewPrivate::setPageProxy(QtWebPageProxy* pageProxy)
{
    this->pageProxy.reset(pageProxy);
}

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
{
}

QQuickWebViewExperimental::~QQuickWebViewExperimental()
{
}

void QQuickWebViewExperimental::setUseTraditionalDesktopBehaviour(bool enable)
{
    Q_D(QQuickWebView);

    if (enable == d->useTraditionalDesktopBehaviour)
        return;

    d->setUseTraditionalDesktopBehaviour(enable);
}

void QQuickWebViewExperimental::postMessage(const QString& message)
{
    Q_D(QQuickWebView);
    d->pageProxy->postMessageToNavigatorQtObject(message);
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

void QQuickWebViewExperimental::setPromptDialog(QDeclarativeComponent* promptDialog)
{
    Q_D(QQuickWebView);
    if (d->promptDialog == promptDialog)
        return;
    d->promptDialog = promptDialog;
    emit promptDialogChanged();
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

bool QQuickWebViewExperimental::useTraditionalDesktopBehaviour() const
{
    Q_D(const QQuickWebView);
    return d->useTraditionalDesktopBehaviour;
}

void QQuickWebViewExperimental::goForwardTo(int index)
{
    d_ptr->navigationHistory->d->goForwardTo(index);
}

void QQuickWebViewExperimental::goBackTo(int index)
{
    d_ptr->navigationHistory->d->goBackTo(index);
}

QQuickWebView::QQuickWebView(QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(new QQuickWebViewPrivate(this))
    , m_experimental(new QQuickWebViewExperimental(this))
{
    Q_D(QQuickWebView);
    d->initialize();
    d->initializeTouch(this);
}

QQuickWebView::QQuickWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(new QQuickWebViewPrivate(this))
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
    Q_D(QQuickWebView);
    d->webPageProxy()->loadURL(url.toString());
}

void QQuickWebView::goBack()
{
    Q_D(QQuickWebView);
    d->webPageProxy()->goBack();
}

void QQuickWebView::goForward()
{
    Q_D(QQuickWebView);
    d->webPageProxy()->goForward();
}

void QQuickWebView::stop()
{
    Q_D(QQuickWebView);
    d->webPageProxy()->stopLoading();
}

void QQuickWebView::reload()
{
    Q_D(QQuickWebView);
    const bool reloadFromOrigin = true;
    d->webPageProxy()->reload(reloadFromOrigin);
}

QUrl QQuickWebView::url() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebFrameProxy> mainFrame = d->webPageProxy()->mainFrame();
    if (!mainFrame)
        return QUrl();
    return QUrl(QString(mainFrame->url()));
}

int QQuickWebView::loadProgress() const
{
    Q_D(const QQuickWebView);
    return d->pageLoadClient->loadProgress();
}

bool QQuickWebView::canGoBack() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy()->canGoBack();
}

bool QQuickWebView::canGoForward() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy()->canGoForward();
}

bool QQuickWebView::loading() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebKit::WebFrameProxy> mainFrame = d->webPageProxy()->mainFrame();
    return mainFrame && !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
}

bool QQuickWebView::canReload() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebKit::WebFrameProxy> mainFrame = d->webPageProxy()->mainFrame();
    if (mainFrame)
        return (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
    return d->webPageProxy()->backForwardList()->currentItem();
}

QString QQuickWebView::title() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy()->pageTitle();
}

QWebPreferences* QQuickWebView::preferences() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->preferences();
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
    if (newGeometry.size() != oldGeometry.size()) {
        if (d->useTraditionalDesktopBehaviour) {
            d->pageView->setWidth(newGeometry.width());
            d->pageView->setHeight(newGeometry.height());
        } else
            d->updateViewportSize();
    }
}

void QQuickWebView::focusInEvent(QFocusEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->event(event);
}

void QQuickWebView::focusOutEvent(QFocusEvent* event)
{
    Q_D(QQuickWebView);
    d->pageView->event(event);
}

WKPageRef QQuickWebView::pageRef() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->pageRef();
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
    d->webPageProxy()->loadHTMLString(html, baseUrl.toString());
}

#include "moc_qquickwebview_p.cpp"
