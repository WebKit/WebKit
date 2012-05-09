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
#include "QtViewportInteractionEngine.h"
#include "QtWebContext.h"
#include "QtWebError.h"
#include "QtWebIconDatabaseClient.h"
#include "QtWebPageEventHandler.h"
#include "QtWebPageLoadClient.h"
#include "QtWebPagePolicyClient.h"
#include "UtilsQt.h"
#include "WebBackForwardList.h"
#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif
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

#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <QDateTime>
#include <QtQml/QJSValue>
#include <WKOpenPanelResultListener.h>
#include <WKSerializedScriptValue.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;
using namespace WebKit;

static bool s_flickableViewportEnabled = true;
static const int kAxisLockSampleCount = 5;
static const qreal kAxisLockVelocityThreshold = 300;
static const qreal kAxisLockVelocityDirectionThreshold = 50;

struct JSCallbackClosure {
    QPointer<QObject> receiver;
    QByteArray method;
    QJSValue value;
};

static inline QString toQString(JSStringRef string)
{
    return QString(reinterpret_cast<const QChar*>(JSStringGetCharactersPtr(string)), JSStringGetLength(string));
}

static inline QJSValue toQJSValue(JSStringRef string)
{
    return QJSValue(toQString(string));
}

static QJSValue buildQJSValue(QJSEngine* engine, JSGlobalContextRef context, JSValueRef value, int depth)
{
    QJSValue var;
    JSValueRef exception = 0;

    if (depth > 10)
        return var;

    switch (JSValueGetType(context, value)) {
    case kJSTypeBoolean:
        var = QJSValue(JSValueToBoolean(context, value));
        break;
    case kJSTypeNumber:
        {
            double number = JSValueToNumber(context, value, &exception);
            if (!exception)
                var = QJSValue(number);
        }
        break;
    case kJSTypeString:
        {
            JSRetainPtr<JSStringRef> string = JSValueToStringCopy(context, value, &exception);
            if (!exception)
                var = toQJSValue(string.get());
        }
        break;
    case kJSTypeObject:
        {
            JSObjectRef obj = JSValueToObject(context, value, &exception);

            JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, obj);
            size_t length = JSPropertyNameArrayGetCount(names);

            var = engine->newObject();

            for (size_t i = 0; i < length; ++i) {
                JSRetainPtr<JSStringRef> name = JSPropertyNameArrayGetNameAtIndex(names, i);
                JSValueRef property = JSObjectGetProperty(context, obj, name.get(), &exception);

                if (!exception) {
                    QJSValue value = buildQJSValue(engine, context, property, depth + 1);
                    var.setProperty(toQString(name.get()), value);
                }
            }
        }
        break;
    }
    return var;
}

static void javaScriptCallback(WKSerializedScriptValueRef valueRef, WKErrorRef, void* data)
{
    JSCallbackClosure* closure = reinterpret_cast<JSCallbackClosure*>(data);

    if (closure->method.size())
        QMetaObject::invokeMethod(closure->receiver, closure->method);
    else {
        QJSValue function = closure->value;

        // If a callable function is supplied, we build a JavaScript value accessible
        // in the QML engine, and calls the function with that.
        if (function.isCallable()) {
            QJSValue var;
            if (valueRef) {
                // FIXME: Slow but OK for now.
                JSGlobalContextRef context = JSGlobalContextCreate(0);

                JSValueRef exception = 0;
                JSValueRef value = WKSerializedScriptValueDeserialize(valueRef, context, &exception);
                var = buildQJSValue(function.engine(), context, value, /* depth */ 0);

                JSGlobalContextRelease(context);
            }

            QList<QJSValue> args;
            args.append(var);
            function.call(args);
        }
    }

    delete closure;
}

static QQuickWebViewPrivate* createPrivateObject(QQuickWebView* publicObject)
{
    if (s_flickableViewportEnabled)
        return new QQuickWebViewFlickablePrivate(publicObject);
    return new QQuickWebViewLegacyPrivate(publicObject);
}

QQuickWebViewPrivate::FlickableAxisLocker::FlickableAxisLocker()
    : m_allowedDirection(QQuickFlickable::AutoFlickDirection)
    , m_sampleCount(0)
{
}

QVector2D QQuickWebViewPrivate::FlickableAxisLocker::touchVelocity(const QTouchEvent* event)
{
    static bool touchVelocityAvailable = event->device()->capabilities().testFlag(QTouchDevice::Velocity);
    const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();

    if (touchVelocityAvailable)
        return touchPoint.velocity();

    const QLineF movementLine(touchPoint.screenPos(), m_initialScreenPosition);
    const qint64 elapsed = m_time.elapsed();

    if (!elapsed)
        return QVector2D(0, 0);

    // Calculate an approximate velocity vector in the unit of pixel / second.
    return QVector2D(1000 * movementLine.dx() / elapsed, 1000 * movementLine.dy() / elapsed);
}

void QQuickWebViewPrivate::FlickableAxisLocker::update(const QTouchEvent* event)
{
    ASSERT(event->touchPoints().size() == 1);
    const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();

    ++m_sampleCount;

    if (m_sampleCount == 1) {
        m_initialScreenPosition = touchPoint.screenPos();
        m_time.restart();
        return;
    }

    if (m_sampleCount > kAxisLockSampleCount
            || m_allowedDirection == QQuickFlickable::HorizontalFlick
            || m_allowedDirection == QQuickFlickable::VerticalFlick)
        return;

    QVector2D velocity = touchVelocity(event);

    qreal directionIndicator = qAbs(velocity.x()) - qAbs(velocity.y());

    if (velocity.length() > kAxisLockVelocityThreshold && qAbs(directionIndicator) > kAxisLockVelocityDirectionThreshold)
        m_allowedDirection = (directionIndicator > 0) ? QQuickFlickable::HorizontalFlick : QQuickFlickable::VerticalFlick;
}

void QQuickWebViewPrivate::FlickableAxisLocker::setReferencePosition(const QPointF& position)
{
    m_lockReferencePosition = position;
}

void QQuickWebViewPrivate::FlickableAxisLocker::reset()
{
    m_allowedDirection = QQuickFlickable::AutoFlickDirection;
    m_sampleCount = 0;
}

QPointF QQuickWebViewPrivate::FlickableAxisLocker::adjust(const QPointF& position)
{
    if (m_allowedDirection == QQuickFlickable::HorizontalFlick)
        return QPointF(position.x(), m_lockReferencePosition.y());

    if (m_allowedDirection == QQuickFlickable::VerticalFlick)
        return QPointF(m_lockReferencePosition.x(), position.y());

    return position;
}

QQuickWebViewPrivate::QQuickWebViewPrivate(QQuickWebView* viewport)
    : q_ptr(viewport)
    , alertDialog(0)
    , confirmDialog(0)
    , promptDialog(0)
    , authenticationDialog(0)
    , certificateVerificationDialog(0)
    , itemSelector(0)
    , proxyAuthenticationDialog(0)
    , filePicker(0)
    , databaseQuotaDialog(0)
    , m_useDefaultContentItemSize(true)
    , m_navigatorQtObjectEnabled(false)
    , m_renderToOffscreenBuffer(false)
    , m_dialogActive(false)
    , m_loadProgress(0)
{
    viewport->setClip(true);
    viewport->setPixelAligned(true);
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
#if ENABLE(FULLSCREEN_API)
    webPageProxy->fullScreenManager()->setWebView(q_ptr);
#endif

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

/*!
    \qmlsignal WebView::loadingChanged(WebLoadRequest request)
*/

void QQuickWebViewPrivate::provisionalLoadDidStart(const QUrl& url)
{
    Q_Q(QQuickWebView);

    QWebLoadRequest loadRequest(url, QQuickWebView::LoadStartedStatus);
    emit q->loadingChanged(&loadRequest);
}

void QQuickWebViewPrivate::loadDidCommit()
{
    Q_Q(QQuickWebView);
    ASSERT(q->loading());

    emit q->navigationHistoryChanged();
    emit q->urlChanged();
    emit q->titleChanged();
}

void QQuickWebViewPrivate::didSameDocumentNavigation()
{
    Q_Q(QQuickWebView);

    emit q->navigationHistoryChanged();
    emit q->urlChanged();
}

void QQuickWebViewPrivate::titleDidChange()
{
    Q_Q(QQuickWebView);

    emit q->titleChanged();
}

void QQuickWebViewPrivate::loadProgressDidChange(int loadProgress)
{
    Q_Q(QQuickWebView);

    if (!loadProgress)
        setIcon(QUrl());

    m_loadProgress = loadProgress;

    emit q->loadProgressChanged();
}

void QQuickWebViewPrivate::backForwardListDidChange()
{
    navigationHistory->d->reset();
}

void QQuickWebViewPrivate::loadDidSucceed()
{
    Q_Q(QQuickWebView);
    ASSERT(!q->loading());

    QWebLoadRequest loadRequest(q->url(), QQuickWebView::LoadSucceededStatus);
    emit q->loadingChanged(&loadRequest);
}

void QQuickWebViewPrivate::loadDidFail(const QtWebError& error)
{
    Q_Q(QQuickWebView);
    ASSERT(!q->loading());

    QWebLoadRequest loadRequest(error.url(), QQuickWebView::LoadFailedStatus, error.description(), static_cast<QQuickWebView::ErrorDomain>(error.type()), error.errorCode());
    emit q->loadingChanged(&loadRequest);
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

void QQuickWebViewPrivate::processDidCrash()
{
    Q_Q(QQuickWebView);

    QUrl url(KURL(WebCore::ParsedURLString, webPageProxy->urlAtProcessExit()));
    qWarning("WARNING: The web process experienced a crash on '%s'.", qPrintable(url.toString(QUrl::RemoveUserInfo)));

    pageView->eventHandler()->resetGestureRecognizers();

    // Check if loading was ongoing, when process crashed.
    if (m_loadProgress > 0 && m_loadProgress < 100) {
        QWebLoadRequest loadRequest(url, QQuickWebView::LoadFailedStatus, QLatin1String("The web process crashed."), QQuickWebView::InternalErrorDomain, 0);

        loadProgressDidChange(100);
        emit q->loadingChanged(&loadRequest);
    }
}

void QQuickWebViewPrivate::didRelaunchProcess()
{
    qWarning("WARNING: The web process has been successfully restarted.");

    webPageProxy->drawingArea()->setSize(viewSize(), IntSize());
    updateViewportSize();
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

QPointF QQuickWebViewPrivate::contentPos() const
{
    Q_Q(const QQuickWebView);
    return QPointF(q->contentX(), q->contentY());
}

void QQuickWebViewPrivate::setContentPos(const QPointF& pos)
{
    Q_Q(QQuickWebView);
    q->setContentX(pos.x());
    q->setContentY(pos.y());
}

QRect QQuickWebViewPrivate::visibleContentsRect() const
{
    Q_Q(const QQuickWebView);
    const QRectF visibleRect(q->boundingRect().intersected(pageView->boundingRect()));

    // We avoid using toAlignedRect() because it produces inconsistent width and height.
    QRectF mappedRect(q->mapRectToWebContent(visibleRect));
    return QRect(floor(mappedRect.x()), floor(mappedRect.y()), floor(mappedRect.width()), floor(mappedRect.height()));
}

WebCore::IntSize QQuickWebViewPrivate::viewSize() const
{
    return WebCore::IntSize(pageView->width(), pageView->height());
}

/*!
    \internal

    \qmlsignal WebViewExperimental::onMessageReceived(var message)

    \brief Emitted when JavaScript code executing on the web page calls navigator.qt.postMessage().

    \sa postMessage
*/
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
    // Default values for the Legacy view.
    attributes.devicePixelRatio = 1;
    attributes.initialScale = 1;
    attributes.minimumScale = 1;
    attributes.maximumScale = 1;
    attributes.userScalable = 0;
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

qreal QQuickWebViewLegacyPrivate::zoomFactor() const
{
    return webPageProxy->pageZoomFactor();
}

void QQuickWebViewLegacyPrivate::setZoomFactor(qreal factor)
{
    webPageProxy->setPageZoomFactor(factor);
}

QQuickWebViewFlickablePrivate::QQuickWebViewFlickablePrivate(QQuickWebView* viewport)
    : QQuickWebViewPrivate(viewport)
    , pageIsSuspended(true)
{
    // Disable mouse events on the flickable web view so we do not
    // select text during pan gestures on platforms which send both
    // touch and mouse events simultaneously.
    // FIXME: Temporary workaround code which should be removed when
    // bug http://codereview.qt-project.org/21896 is fixed.
    viewport->setAcceptedMouseButtons(Qt::NoButton);
    viewport->setAcceptHoverEvents(false);
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
    Q_Q(QQuickWebView);
    // Flickable moves its contentItem so we need to take that position into account,
    // as well as the potential displacement of the page on the contentItem because
    // of additional QML items.
    qreal xPos = q->contentItem()->x() + pageView->x();
    qreal yPos = q->contentItem()->y() + pageView->y();
    return QPointF(xPos, yPos);
}

void QQuickWebViewFlickablePrivate::updateContentsSize(const QSizeF& size)
{
    Q_Q(QQuickWebView);

    // Make sure that the contentItem is sized to the page
    // if the user did not add other flickable items in QML.
    // If the user adds items in QML he has to make sure to
    // disable the default content item size property on the WebView
    // and bind the contentWidth and contentHeight accordingly.
    // This is in accordance with normal QML Flickable behaviour.
    if (!m_useDefaultContentItemSize)
        return;

    q->setContentWidth(size.width());
    q->setContentHeight(size.height());
}

void QQuickWebViewFlickablePrivate::onComponentComplete()
{
    Q_Q(QQuickWebView);

    interactionEngine.reset(new QtViewportInteractionEngine(q, pageView.data()));
    pageView->eventHandler()->setViewportInteractionEngine(interactionEngine.data());

    QObject::connect(interactionEngine.data(), SIGNAL(contentSuspendRequested()), q, SLOT(_q_suspend()));
    QObject::connect(interactionEngine.data(), SIGNAL(contentResumeRequested()), q, SLOT(_q_resume()));
    QObject::connect(interactionEngine.data(), SIGNAL(contentViewportChanged(QPointF)), q, SLOT(_q_contentViewportChanged(QPointF)));

    _q_resume();

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();
}

void QQuickWebViewFlickablePrivate::didChangeViewportProperties(const WebCore::ViewportAttributes& newAttributes)
{
    Q_Q(QQuickWebView);

    QSize viewportSize = q->boundingRect().size().toSize();

    // FIXME: Revise these when implementing fit-to-width.
    WebCore::ViewportAttributes attr = newAttributes;
    WebCore::restrictMinimumScaleFactorToViewportSize(attr, viewportSize);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attr);

    // FIXME: Resetting here can reset more than needed. For instance it will end deferrers.
    // This needs to be revised at some point.
    interactionEngine->reset();

    interactionEngine->setContentToDevicePixelRatio(attr.devicePixelRatio);

    interactionEngine->setAllowsUserScaling(!!attr.userScalable);
    interactionEngine->setCSSScaleBounds(attr.minimumScale, attr.maximumScale);

    if (!interactionEngine->hadUserInteraction() && !pageIsSuspended)
        interactionEngine->setCSSScale(attr.initialScale);

    this->attributes = attr;
    q->experimental()->viewportInfo()->didUpdateViewportConstraints();

    // If the web app successively changes the viewport on purpose
    // it wants to be in control and we should disable animations.
    interactionEngine->ensureContentWithinViewportBoundary(/*immediate*/ true);
}

void QQuickWebViewFlickablePrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSize viewportSize = q->boundingRect().size().toSize();

    if (viewportSize.isEmpty() || !interactionEngine)
        return;

    WebPreferences* wkPrefs = webPageProxy->pageGroup()->preferences();
    wkPrefs->setDeviceWidth(viewportSize.width());
    wkPrefs->setDeviceHeight(viewportSize.height());

    // Let the WebProcess know about the new viewport size, so that
    // it can resize the content accordingly.
    webPageProxy->setViewportSize(viewportSize);

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

    QRectF accurateVisibleRect(q->boundingRect());
    accurateVisibleRect.translate(contentPos());
    drawingArea->setVisibleContentsRect(visibleRect, scale, trajectoryVector, FloatPoint(accurateVisibleRect.x(), accurateVisibleRect.y()));

    // Ensure that updatePaintNode is always called before painting.
    pageView->update();
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

/*!
    \qmlsignal WebView::onNavigationRequested(WebNavigationRequest request)

    This signal is emitted for every navigation request. The request object contains url,
    button and modifiers properties describing the navigation action, e.g. "a middle click
    with shift key pressed to 'http://qt-project.org'".

    The navigation will be accepted by default. To change that, one can set the action
    property to WebView.IgnoreRequest to reject the request or WebView.DownloadRequest to
    trigger a download instead of navigating to the url.

    The request object cannot be used after the signal handler function ends.

    \sa WebNavigationRequest
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

bool QQuickWebViewExperimental::useDefaultContentItemSize() const
{
    Q_D(const QQuickWebView);
    return d->m_useDefaultContentItemSize;
}

void QQuickWebViewExperimental::setUseDefaultContentItemSize(bool enable)
{
    Q_D(QQuickWebView);
    d->m_useDefaultContentItemSize = enable;
}

/*!
    \internal

    \qmlproperty int WebViewExperimental::preferredMinimumContentsWidth
    \brief Minimum contents width when not overriden by the page itself.

    Unless the page defines how contents should be laid out, using e.g.
    the viewport meta tag, it is laid out given the width of the viewport
    (in CSS units).

    This setting can be used to enforce a minimum width when the page
    does not define a width itself. This is useful for laying out pages
    designed for big screens, commonly knows as desktop pages, on small
    devices.

    The default value is 0, but the value of 980 is recommented for small
    screens as it provides a good trade off between legitable pages and
    non-broken content.
 */
int QQuickWebViewExperimental::preferredMinimumContentsWidth() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->pageGroup()->preferences()->layoutFallbackWidth();
}

void QQuickWebViewExperimental::setPreferredMinimumContentsWidth(int width)
{
    Q_D(QQuickWebView);
    d->webPageProxy->pageGroup()->preferences()->setLayoutFallbackWidth(width);
}

void QQuickWebViewExperimental::setFlickableViewportEnabled(bool enable)
{
    s_flickableViewportEnabled = enable;
}

bool QQuickWebViewExperimental::flickableViewportEnabled()
{
    return s_flickableViewportEnabled;
}

/*!
    \internal

    \qmlmethod void WebViewExperimental::postMessage(string message)

    \brief Post a message to an onmessage function registered with the navigator.qt object
           by JavaScript code executing on the page.

    \sa onMessageReceived
*/

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

/*!
    \internal

    \qmlproperty real WebViewExperimental::devicePixelRatio
    \brief The ratio between the CSS units and device pixels when the content is unscaled.

    When designing touch-friendly contents, knowing the approximated target size on a device
    is important for contents providers in order to get the intented layout and element
    sizes.

    As most first generation touch devices had a PPI of approximately 160, this became a
    de-facto value, when used in conjunction with the viewport meta tag.

    Devices with a higher PPI learning towards 240 or 320, applies a pre-scaling on all
    content, of either 1.5 or 2.0, not affecting the CSS scale or pinch zooming.

    This value can be set using this property and it is exposed to CSS media queries using
    the -webkit-device-pixel-ratio query.

    For instance, if you want to load an image without having it upscaled on a web view
    using a device pixel ratio of 2.0 it can be done by loading an image of say 100x100
    pixels but showing it at half the size.

    FIXME: Move documentation example out in separate files

    @media (-webkit-min-device-pixel-ratio: 1.5) {
        .icon {
            width: 50px;
            height: 50px;
            url: "/images/icon@2x.png"; // This is actually a 100x100 image
        }
    }

    If the above is used on a device with device pixel ratio of 1.5, it will be scaled
    down but still provide a better looking image.
 */

double QQuickWebViewExperimental::devicePixelRatio() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->pageGroup()->preferences()->devicePixelRatio();
}

void QQuickWebViewExperimental::setDevicePixelRatio(double devicePixelRatio)
{
    Q_D(QQuickWebView);
    if (devicePixelRatio == this->devicePixelRatio())
        return;

    d->webPageProxy->pageGroup()->preferences()->setDevicePixelRatio(devicePixelRatio);
    emit devicePixelRatioChanged();
}

/*!
    \internal

    \qmlmethod void WebViewExperimental::evaluateJavaScript(string script [, function(result)])

    \brief Evaluates the specified JavaScript and, if supplied, calls a function with the result.
*/

void QQuickWebViewExperimental::evaluateJavaScript(const QString& script, const QJSValue& value)
{
    JSCallbackClosure* closure = new JSCallbackClosure;

    closure->receiver = this;
    closure->value = value;

    d_ptr->webPageProxy.get()->runJavaScriptInMainFrame(script, ScriptValueCallback::create(closure, javaScriptCallback));
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

/*!
    \qmlclass WebView QWebView
    \inqmlmodule QtWebKit 3.0
*/

/*!
   \qmlmethod WebView(Item parent)
   \brief Constructs a WebView with a parent.
*/

QQuickWebView::QQuickWebView(QQuickItem* parent)
    : QQuickFlickable(parent)
    , d_ptr(createPrivateObject(this))
    , m_experimental(new QQuickWebViewExperimental(this))
{
    Q_D(QQuickWebView);
    d->initialize();
}

QQuickWebView::QQuickWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, QQuickItem* parent)
    : QQuickFlickable(parent)
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

    d->webPageProxy->loadURL(url.toString());
}

QUrl QQuickWebView::icon() const
{
    Q_D(const QQuickWebView);
    return d->m_iconURL;
}

/*!
    \qmlproperty int WebView::loadProgress
    \brief The progress of loading the current web page.

    The range is from 0 to 100.
*/

int QQuickWebView::loadProgress() const
{
    Q_D(const QQuickWebView);
    return d->loadProgress();
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

/*!
    \qmlproperty bool WebView::loading
    \brief True if the web view is currently loading a web page, false otherwise.
*/

bool QQuickWebView::loading() const
{
    Q_D(const QQuickWebView);
    RefPtr<WebKit::WebFrameProxy> mainFrame = d->webPageProxy->mainFrame();
    return mainFrame && !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
}

/*!
    \internal
 */

QPointF QQuickWebView::mapToWebContent(const QPointF& pointInViewCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformFromItem().map(pointInViewCoordinates);
}

/*!
    \internal
 */

QRectF QQuickWebView::mapRectToWebContent(const QRectF& rectInViewCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformFromItem().mapRect(rectInViewCoordinates);
}

/*!
    \internal
 */

QPointF QQuickWebView::mapFromWebContent(const QPointF& pointInCSSCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformToItem().map(pointInCSSCoordinates);
}

/*!
    \internal
 */
QRectF QQuickWebView::mapRectFromWebContent(const QRectF& rectInCSSCoordinates) const
{
    Q_D(const QQuickWebView);
    return d->pageView->transformToItem().mapRect(rectInCSSCoordinates);
}

/*!
    \qmlproperty string WebView::title
    \brief The title of the loaded page.
*/

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
        return QQuickFlickable::inputMethodQuery(property);
    }
}

/*!
    \preliminary

    The experimental module consisting on experimental API which will break
    from version to version.
*/
QQuickWebViewExperimental* QQuickWebView::experimental() const
{
    return m_experimental;
}

QQuickWebViewAttached* QQuickWebView::qmlAttachedProperties(QObject* object)
{
    return new QQuickWebViewAttached(object);
}

/*!
    \internal
*/
void QQuickWebView::platformInitialize()
{
    JSC::initializeThreading();
    WTF::initializeMainThread();
}

void QQuickWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_D(QQuickWebView);
    QQuickFlickable::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->updateViewportSize();
}

void QQuickWebView::componentComplete()
{
    Q_D(QQuickWebView);
    QQuickFlickable::componentComplete();

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

    bool lockingDisabled = flickableDirection() != AutoFlickDirection
                           || event->touchPoints().size() != 1
                           || width() >= contentWidth()
                           || height() >= contentHeight();

    if (!lockingDisabled)
        d->axisLocker.update(event);
    else
        d->axisLocker.reset();

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
    return QQuickFlickable::event(ev);
}

WKPageRef QQuickWebView::pageRef() const
{
    Q_D(const QQuickWebView);
    return toAPI(d->webPageProxy.get());
}

QPointF QQuickWebView::contentPos() const
{
    Q_D(const QQuickWebView);
    return d->contentPos();
}

void QQuickWebView::setContentPos(const QPointF& pos)
{
    Q_D(QQuickWebView);
    d->setContentPos(pos);
}

void QQuickWebView::handleFlickableMousePress(const QPointF& position, qint64 eventTimestampMillis)
{
    Q_D(QQuickWebView);
    d->axisLocker.setReferencePosition(position);
    QMouseEvent mouseEvent(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    mouseEvent.setTimestamp(eventTimestampMillis);
    QQuickFlickable::mousePressEvent(&mouseEvent);
}

void QQuickWebView::handleFlickableMouseMove(const QPointF& position, qint64 eventTimestampMillis)
{
    Q_D(QQuickWebView);
    QMouseEvent mouseEvent(QEvent::MouseMove, d->axisLocker.adjust(position), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    mouseEvent.setTimestamp(eventTimestampMillis);
    QQuickFlickable::mouseMoveEvent(&mouseEvent);
}

void QQuickWebView::handleFlickableMouseRelease(const QPointF& position, qint64 eventTimestampMillis)
{
    Q_D(QQuickWebView);
    QMouseEvent mouseEvent(QEvent::MouseButtonRelease, d->axisLocker.adjust(position), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    d->axisLocker.reset();
    mouseEvent.setTimestamp(eventTimestampMillis);
    QQuickFlickable::mouseReleaseEvent(&mouseEvent);
}

/*!
    \qmlmethod void WebView::loadHtml(string html, url baseUrl, url unreachableUrl)
    \brief Loads the specified \a html as the content of the web view.

    External objects such as stylesheets or images referenced in the HTML
    document are located relative to \a baseUrl.

    \sa WebView::url
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

qreal QQuickWebView::zoomFactor() const
{
    Q_D(const QQuickWebView);
    return d->zoomFactor();
}

void QQuickWebView::setZoomFactor(qreal factor)
{

    Q_D(QQuickWebView);
    d->setZoomFactor(factor);
}

void QQuickWebView::runJavaScriptInMainFrame(const QString &script, QObject *receiver, const char *method)
{
    Q_D(QQuickWebView);

    JSCallbackClosure* closure = new JSCallbackClosure;
    closure->receiver = receiver;
    closure->method = method;

    d->webPageProxy.get()->runJavaScriptInMainFrame(script, ScriptValueCallback::create(closure, javaScriptCallback));
}

#include "moc_qquickwebview_p.cpp"
