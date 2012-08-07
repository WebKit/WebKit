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
#include "QtViewportHandler.h"
#include "QtWebContext.h"
#include "QtWebError.h"
#include "QtWebIconDatabaseClient.h"
#include "QtWebPageEventHandler.h"
#include "QtWebPageLoadClient.h"
#include "QtWebPagePolicyClient.h"
#include "WebBackForwardList.h"
#include "WebInspectorProxy.h"
#include "WebInspectorServer.h"
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
#include "qwebiconimageprovider_p.h"
#include "qwebkittest_p.h"
#include "qwebloadrequest_p.h"
#include "qwebnavigationhistory_p.h"
#include "qwebnavigationhistory_p_p.h"
#include "qwebpreferences_p.h"
#include "qwebpreferences_p_p.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <QDateTime>
#include <QtCore/QFile>
#include <QtQml/QJSValue>
#include <QtQuick/QQuickView>
#include <WKOpenPanelResultListener.h>
#include <WKSerializedScriptValue.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>
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
    , m_time(0), m_sampleCount(0)
{
}

QVector2D QQuickWebViewPrivate::FlickableAxisLocker::touchVelocity(const QTouchEvent* event)
{
    static bool touchVelocityAvailable = event->device()->capabilities().testFlag(QTouchDevice::Velocity);
    const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();

    if (touchVelocityAvailable)
        return touchPoint.velocity();

    const QLineF movementLine(touchPoint.pos(), m_initialPosition);
    const ulong elapsed = event->timestamp() - m_time;

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
        m_initialPosition = touchPoint.pos();
        m_time = event->timestamp();
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
    , colorChooser(0)
    , m_useDefaultContentItemSize(true)
    , m_navigatorQtObjectEnabled(false)
    , m_renderToOffscreenBuffer(false)
    , m_allowAnyHTTPSCertificateForLocalHost(false)
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
    QObject::connect(iconDatabase, SIGNAL(iconChangedForPageURL(QString)), q_ptr, SLOT(_q_onIconChangedForPageURL(QString)));

    // Any page setting should preferrable be set before creating the page.
    webPageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    webPageProxy->pageGroup()->preferences()->setForceCompositingMode(true);
    webPageProxy->pageGroup()->preferences()->setFrameFlatteningEnabled(true);

    pageClient.initialize(q_ptr, pageViewPrivate->eventHandler.data(), &undoController);
    webPageProxy->initializeWebPage();
}

void QQuickWebViewPrivate::onComponentComplete()
{
    Q_Q(QQuickWebView);
    m_viewportHandler.reset(new QtViewportHandler(webPageProxy.get(), q, pageView.data()));
    pageView->eventHandler()->setViewportHandler(m_viewportHandler.data());
}

void QQuickWebViewPrivate::setTransparentBackground(bool enable)
{
    webPageProxy->setDrawsTransparentBackground(enable);
}

bool QQuickWebViewPrivate::transparentBackground() const
{
    return webPageProxy->drawsTransparentBackground();
}

/*!
    \qmlsignal WebView::loadingChanged(WebLoadRequest request)
*/

void QQuickWebViewPrivate::provisionalLoadDidStart(const WTF::String& url)
{
    Q_Q(QQuickWebView);

    q->emitUrlChangeIfNeeded();

    QWebLoadRequest loadRequest(QString(url), QQuickWebView::LoadStartedStatus);
    emit q->loadingChanged(&loadRequest);
}

void QQuickWebViewPrivate::didReceiveServerRedirectForProvisionalLoad(const WTF::String&)
{
    Q_Q(QQuickWebView);

    q->emitUrlChangeIfNeeded();
}

void QQuickWebViewPrivate::loadDidCommit()
{
    Q_Q(QQuickWebView);
    ASSERT(q->loading());

    emit q->navigationHistoryChanged();
    emit q->titleChanged();
}

void QQuickWebViewPrivate::didSameDocumentNavigation()
{
    Q_Q(QQuickWebView);

    q->emitUrlChangeIfNeeded();
    emit q->navigationHistoryChanged();
}

void QQuickWebViewPrivate::titleDidChange()
{
    Q_Q(QQuickWebView);

    emit q->titleChanged();
}

void QQuickWebViewPrivate::loadProgressDidChange(int loadProgress)
{
    Q_Q(QQuickWebView);

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

void QQuickWebViewPrivate::handleMouseEvent(QMouseEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        // If a MouseButtonDblClick was received then we got a MouseButtonPress before
        // handleMousePressEvent will take care of double clicks.
        pageView->eventHandler()->handleMousePressEvent(event);
        break;
    case QEvent::MouseMove:
        pageView->eventHandler()->handleMouseMoveEvent(event);
        break;
    case QEvent::MouseButtonRelease:
        pageView->eventHandler()->handleMouseReleaseEvent(event);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
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
    updateUserScripts();
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
    updateIcon();
}

void QQuickWebViewPrivate::_q_onIconChangedForPageURL(const QString& pageUrl)
{
    if (pageUrl != QString(m_currentUrl))
        return;

    updateIcon();
}

/* Called either when the url changes, or when the icon for the current page changes */
void QQuickWebViewPrivate::updateIcon()
{
    Q_Q(QQuickWebView);

    QQuickView* view = qobject_cast<QQuickView*>(q->window());
    if (!view)
        return;

    QWebIconImageProvider* provider = static_cast<QWebIconImageProvider*>(
                view->engine()->imageProvider(QWebIconImageProvider::identifier()));
    if (!provider)
        return;

    WTF::String iconUrl = provider->iconURLForPageURLInContext(m_currentUrl, context.get());

    if (iconUrl == m_iconUrl)
        return;

    m_iconUrl = iconUrl;
    emit q->iconChanged();
}

void QQuickWebViewPrivate::_q_onReceivedResponseFromDownload(QWebDownloadItem* downloadItem)
{
    // Now that our downloadItem has everything we need we can emit downloadRequested.
    if (!downloadItem)
        return;

    Q_Q(QQuickWebView);
    QQmlEngine::setObjectOwnership(downloadItem, QQmlEngine::JavaScriptOwnership);
    emit q->experimental()->downloadRequested(downloadItem);
}

void QQuickWebViewPrivate::runJavaScriptAlert(const QString& alertText)
{
    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForAlert(alertText))
        return;

    dialogRunner.run();
}

bool QQuickWebViewPrivate::runJavaScriptConfirm(const QString& message)
{
    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForConfirm(message))
        return true;

    dialogRunner.run();

    return dialogRunner.wasAccepted();
}

QString QQuickWebViewPrivate::runJavaScriptPrompt(const QString& message, const QString& defaultValue, bool& ok)
{
    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForPrompt(message, defaultValue)) {
        ok = true;
        return defaultValue;
    }

    dialogRunner.run();

    ok = dialogRunner.wasAccepted();
    return dialogRunner.result();
}

void QQuickWebViewPrivate::handleAuthenticationRequiredRequest(const QString& hostname, const QString& realm, const QString& prefilledUsername, QString& username, QString& password)
{
    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForAuthentication(hostname, realm, prefilledUsername))
        return;

    dialogRunner.run();

    username = dialogRunner.username();
    password = dialogRunner.password();
}

void QQuickWebViewPrivate::handleProxyAuthenticationRequiredRequest(const QString& hostname, uint16_t port, const QString& prefilledUsername, QString& username, QString& password)
{
    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForProxyAuthentication(hostname, port, prefilledUsername))
        return;

    dialogRunner.run();

    username = dialogRunner.username();
    password = dialogRunner.password();
}

bool QQuickWebViewPrivate::handleCertificateVerificationRequest(const QString& hostname)
{
    Q_Q(QQuickWebView);

    if (m_allowAnyHTTPSCertificateForLocalHost
        && (hostname == QStringLiteral("127.0.0.1") || hostname == QStringLiteral("localhost")))
        return true;

    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForCertificateVerification(hostname))
        return false;

    dialogRunner.run();

    return dialogRunner.wasAccepted();
}

void QQuickWebViewPrivate::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, QtWebPageUIClient::FileChooserType type)
{
    Q_Q(QQuickWebView);

    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForFilePicker(selectedFileNames, (type == QtWebPageUIClient::MultipleFilesSelection)))
        return;

    dialogRunner.run();

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
    Q_Q(QQuickWebView);
    QtDialogRunner dialogRunner(q);
    if (!dialogRunner.initForDatabaseQuotaDialog(databaseName, displayName, securityOrigin, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage))
        return 0;

    dialogRunner.run();

    return dialogRunner.wasAccepted() ? dialogRunner.databaseQuota() : 0;
}

/* The 'WebView' attached property allows items spawned by the webView to
   refer back to the originating webView through 'WebView.view', similar
   to how ListView.view and GridView.view is exposed to items. */
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

QQuickWebViewAttached* QQuickWebView::qmlAttachedProperties(QObject* object)
{
    return new QQuickWebViewAttached(object);
}



void QQuickWebViewPrivate::addAttachedPropertyTo(QObject* object)
{
    Q_Q(QQuickWebView);
    QQuickWebViewAttached* attached = static_cast<QQuickWebViewAttached*>(qmlAttachedPropertiesObject<QQuickWebView>(object));
    attached->setView(q);
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

static QString readUserScript(const QUrl& url)
{
    QString path;
    if (url.isLocalFile())
        path = url.toLocalFile();
    else if (url.scheme() == QLatin1String("qrc"))
        path = QStringLiteral(":") + url.path();
    else {
        qWarning("QQuickWebView: Couldn't open '%s' as user script because only file:/// and qrc:/// URLs are supported.", qPrintable(url.toString()));
        return QString();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("QQuickWebView: Couldn't open '%s' as user script due to error '%s'.", qPrintable(url.toString()), qPrintable(file.errorString()));
        return QString();
    }

    QString contents = QString::fromUtf8(file.readAll());
    if (contents.isEmpty())
        qWarning("QQuickWebView: Ignoring '%s' as user script because file is empty.", qPrintable(url.toString()));

    return contents;
}

void QQuickWebViewPrivate::updateUserScripts()
{
    Vector<String> scripts;
    scripts.reserveCapacity(userScripts.size());

    for (unsigned i = 0; i < userScripts.size(); ++i) {
        const QUrl& url = userScripts.at(i);
        if (!url.isValid()) {
            qWarning("QQuickWebView: Couldn't open '%s' as user script because URL is invalid.", qPrintable(url.toString()));
            continue;
        }

        QString contents = readUserScript(url);
        if (contents.isEmpty())
            continue;
        scripts.append(String(contents));
    }

    webPageProxy->setUserScripts(scripts);
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
}

void QQuickWebViewLegacyPrivate::initialize(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    Q_Q(QQuickWebView);
    QQuickWebViewPrivate::initialize(contextRef, pageGroupRef);

    q->setAcceptedMouseButtons(Qt::MouseButtonMask);
    q->setAcceptHoverEvents(true);

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();
}

void QQuickWebViewLegacyPrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSizeF viewportSize = q->boundingRect().size();
    if (viewportSize.isEmpty())
        return;
    pageView->setContentsSize(viewportSize);
    // The fixed layout is handled by the FrameView and the drawing area doesn't behave differently
    // whether its fixed or not. We still need to tell the drawing area which part of it
    // has to be rendered on tiles, and in desktop mode it's all of it.
    webPageProxy->drawingArea()->setSize(viewportSize.toSize(), IntSize());
    webPageProxy->drawingArea()->setVisibleContentsRect(FloatRect(FloatPoint(), viewportSize), 1, FloatPoint());
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
{
    viewport->setAcceptHoverEvents(false);
}

QQuickWebViewFlickablePrivate::~QQuickWebViewFlickablePrivate()
{
    m_viewportHandler->disconnect();
}

void QQuickWebViewFlickablePrivate::initialize(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    QQuickWebViewPrivate::initialize(contextRef, pageGroupRef);
    webPageProxy->setUseFixedLayout(true);
}

void QQuickWebViewFlickablePrivate::onComponentComplete()
{
    QQuickWebViewPrivate::onComponentComplete();

    // Trigger setting of correct visibility flags after everything was allocated and initialized.
    _q_onVisibleChanged();
}

void QQuickWebViewFlickablePrivate::didChangeViewportProperties(const WebCore::ViewportAttributes& newAttributes)
{
    if (m_viewportHandler)
        m_viewportHandler->viewportAttributesChanged(newAttributes);
}

void QQuickWebViewFlickablePrivate::updateViewportSize()
{
    // FIXME: Examine why there is not an viewportHandler here in the beginning.
    if (m_viewportHandler)
        m_viewportHandler->viewportItemSizeChanged();
}

void QQuickWebViewFlickablePrivate::pageDidRequestScroll(const QPoint& pos)
{
    m_viewportHandler->pageContentPositionRequested(pos);
}

void QQuickWebViewFlickablePrivate::didChangeContentsSize(const QSize& newSize)
{
    Q_Q(QQuickWebView);

    pageView->setContentsSize(newSize); // emits contentsSizeChanged()
    m_viewportHandler->pageContentsSizeChanged(newSize, q->boundingRect().size().toSize());
}

void QQuickWebViewFlickablePrivate::handleMouseEvent(QMouseEvent* event)
{
    if (!pageView->eventHandler())
        return;

    // FIXME: Update the axis locker for mouse events as well.
    pageView->eventHandler()->handleInputEvent(event);
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

QQuickWebViewExperimental::QQuickWebViewExperimental(QQuickWebView *webView)
    : QObject(webView)
    , q_ptr(webView)
    , d_ptr(webView->d_ptr.data())
    , schemeParent(new QObject(this))
    , m_test(new QWebKitTest(webView->d_ptr.data(), this))
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
    WebPreferences* webPreferences = d->webPageProxy->pageGroup()->preferences();

    if (width == webPreferences->layoutFallbackWidth())
        return;

    webPreferences->setLayoutFallbackWidth(width);
    emit preferredMinimumContentsWidthChanged();
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

QQmlComponent* QQuickWebViewExperimental::alertDialog() const
{
    Q_D(const QQuickWebView);
    return d->alertDialog;
}

void QQuickWebViewExperimental::setAlertDialog(QQmlComponent* alertDialog)
{
    Q_D(QQuickWebView);
    if (d->alertDialog == alertDialog)
        return;
    d->alertDialog = alertDialog;
    emit alertDialogChanged();
}

QQmlComponent* QQuickWebViewExperimental::confirmDialog() const
{
    Q_D(const QQuickWebView);
    return d->confirmDialog;
}

void QQuickWebViewExperimental::setConfirmDialog(QQmlComponent* confirmDialog)
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

QQmlComponent* QQuickWebViewExperimental::promptDialog() const
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

void QQuickWebViewExperimental::setPromptDialog(QQmlComponent* promptDialog)
{
    Q_D(QQuickWebView);
    if (d->promptDialog == promptDialog)
        return;
    d->promptDialog = promptDialog;
    emit promptDialogChanged();
}

QQmlComponent* QQuickWebViewExperimental::authenticationDialog() const
{
    Q_D(const QQuickWebView);
    return d->authenticationDialog;
}

void QQuickWebViewExperimental::setAuthenticationDialog(QQmlComponent* authenticationDialog)
{
    Q_D(QQuickWebView);
    if (d->authenticationDialog == authenticationDialog)
        return;
    d->authenticationDialog = authenticationDialog;
    emit authenticationDialogChanged();
}

QQmlComponent* QQuickWebViewExperimental::proxyAuthenticationDialog() const
{
    Q_D(const QQuickWebView);
    return d->proxyAuthenticationDialog;
}

void QQuickWebViewExperimental::setProxyAuthenticationDialog(QQmlComponent* proxyAuthenticationDialog)
{
    Q_D(QQuickWebView);
    if (d->proxyAuthenticationDialog == proxyAuthenticationDialog)
        return;
    d->proxyAuthenticationDialog = proxyAuthenticationDialog;
    emit proxyAuthenticationDialogChanged();
}
QQmlComponent* QQuickWebViewExperimental::certificateVerificationDialog() const
{
    Q_D(const QQuickWebView);
    return d->certificateVerificationDialog;
}

void QQuickWebViewExperimental::setCertificateVerificationDialog(QQmlComponent* certificateVerificationDialog)
{
    Q_D(QQuickWebView);
    if (d->certificateVerificationDialog == certificateVerificationDialog)
        return;
    d->certificateVerificationDialog = certificateVerificationDialog;
    emit certificateVerificationDialogChanged();
}

QQmlComponent* QQuickWebViewExperimental::itemSelector() const
{
    Q_D(const QQuickWebView);
    return d->itemSelector;
}

void QQuickWebViewExperimental::setItemSelector(QQmlComponent* itemSelector)
{
    Q_D(QQuickWebView);
    if (d->itemSelector == itemSelector)
        return;
    d->itemSelector = itemSelector;
    emit itemSelectorChanged();
}

QQmlComponent* QQuickWebViewExperimental::filePicker() const
{
    Q_D(const QQuickWebView);
    return d->filePicker;
}

void QQuickWebViewExperimental::setFilePicker(QQmlComponent* filePicker)
{
    Q_D(QQuickWebView);
    if (d->filePicker == filePicker)
        return;
    d->filePicker = filePicker;
    emit filePickerChanged();
}

QQmlComponent* QQuickWebViewExperimental::databaseQuotaDialog() const
{
    Q_D(const QQuickWebView);
    return d->databaseQuotaDialog;
}

void QQuickWebViewExperimental::setDatabaseQuotaDialog(QQmlComponent* databaseQuotaDialog)
{
    Q_D(QQuickWebView);
    if (d->databaseQuotaDialog == databaseQuotaDialog)
        return;
    d->databaseQuotaDialog = databaseQuotaDialog;
    emit databaseQuotaDialogChanged();
}

QQmlComponent* QQuickWebViewExperimental::colorChooser() const
{
    Q_D(const QQuickWebView);
    return d->colorChooser;
}

void QQuickWebViewExperimental::setColorChooser(QQmlComponent* colorChooser)
{
    Q_D(QQuickWebView);
    if (d->colorChooser == colorChooser)
        return;

    d->colorChooser = colorChooser;
    emit colorChooserChanged();
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
    return d->webPageProxy->deviceScaleFactor();
}

void QQuickWebViewExperimental::setDevicePixelRatio(double devicePixelRatio)
{
    Q_D(QQuickWebView);
    if (devicePixelRatio == this->devicePixelRatio())
        return;

    d->webPageProxy->setCustomDeviceScaleFactor(devicePixelRatio);
    emit devicePixelRatioChanged();
}

/*!
    \internal

    \qmlproperty int WebViewExperimental::deviceWidth
    \brief The device width used by the viewport calculations.

    The value used when calculation the viewport, eg. what is used for 'device-width' when
    used in the viewport meta tag. If unset (zero or negative width), the width of the
    actual viewport is used instead.
*/

int QQuickWebViewExperimental::deviceWidth() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->pageGroup()->preferences()->deviceWidth();
}

void QQuickWebViewExperimental::setDeviceWidth(int value)
{
    Q_D(QQuickWebView);
    d->webPageProxy->pageGroup()->preferences()->setDeviceWidth(qMax(0, value));
    emit deviceWidthChanged();
}

/*!
    \internal

    \qmlproperty int WebViewExperimental::deviceHeight
    \brief The device width used by the viewport calculations.

    The value used when calculation the viewport, eg. what is used for 'device-height' when
    used in the viewport meta tag. If unset (zero or negative height), the height of the
    actual viewport is used instead.
*/

int QQuickWebViewExperimental::deviceHeight() const
{
    Q_D(const QQuickWebView);
    return d->webPageProxy->pageGroup()->preferences()->deviceHeight();
}

void QQuickWebViewExperimental::setDeviceHeight(int value)
{
    Q_D(QQuickWebView);
    d->webPageProxy->pageGroup()->preferences()->setDeviceHeight(qMax(0, value));
    emit deviceHeightChanged();
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

QList<QUrl> QQuickWebViewExperimental::userScripts() const
{
    Q_D(const QQuickWebView);
    return d->userScripts;
}

void QQuickWebViewExperimental::setUserScripts(const QList<QUrl>& userScripts)
{
    Q_D(QQuickWebView);
    if (d->userScripts == userScripts)
        return;
    d->userScripts = userScripts;
    d->updateUserScripts();
    emit userScriptsChanged();
}

QUrl QQuickWebViewExperimental::remoteInspectorUrl() const
{
    return QUrl(WebInspectorServer::shared().inspectorUrlForPageID(d_ptr->webPageProxy->inspector()->remoteInspectionPageID()));
}

QQuickUrlSchemeDelegate* QQuickWebViewExperimental::schemeDelegates_At(QQmlListProperty<QQuickUrlSchemeDelegate>* property, int index)
{
    const QObjectList children = property->object->children();
    if (index < children.count())
        return static_cast<QQuickUrlSchemeDelegate*>(children.at(index));
    return 0;
}

void QQuickWebViewExperimental::schemeDelegates_Append(QQmlListProperty<QQuickUrlSchemeDelegate>* property, QQuickUrlSchemeDelegate *scheme)
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

int QQuickWebViewExperimental::schemeDelegates_Count(QQmlListProperty<QQuickUrlSchemeDelegate>* property)
{
    return property->object->children().count();
}

void QQuickWebViewExperimental::schemeDelegates_Clear(QQmlListProperty<QQuickUrlSchemeDelegate>* property)
{
    const QObjectList children = property->object->children();
    for (int index = 0; index < children.count(); index++) {
        QObject* child = children.at(index);
        child->setParent(0);
        delete child;
    }
}

QQmlListProperty<QQuickUrlSchemeDelegate> QQuickWebViewExperimental::schemeDelegates()
{
    return QQmlListProperty<QQuickUrlSchemeDelegate>(schemeParent, 0,
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

QWebKitTest* QQuickWebViewExperimental::test()
{
    return m_test;
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

    WebFrameProxy* mainFrame = d->webPageProxy->mainFrame();
    if (mainFrame && !mainFrame->unreachableURL().isEmpty() && mainFrame->url() != blankURL()) {
        // We are aware of the unreachable url on the UI process side, but since we haven't
        // loaded alternative/subsitute data for it (an error page eg.) WebCore doesn't know
        // about the unreachable url yet. If we just do a reload at this point WebCore will try to
        // reload the currently committed url instead of the unrachable url. To work around this
        // we override the reload here by doing a manual load.
        d->webPageProxy->loadURL(mainFrame->unreachableURL());
        // FIXME: We should make WebCore aware of the unreachable url regardless of substitute-loads
        return;
    }

    const bool reloadFromOrigin = true;
    d->webPageProxy->reload(reloadFromOrigin);
}

QUrl QQuickWebView::url() const
{
    Q_D(const QQuickWebView);

    // FIXME: Enable once we are sure this should not trigger
    // Q_ASSERT(d->m_currentUrl == d->webPageProxy->activeURL());

    return QUrl(d->m_currentUrl);
}

void QQuickWebView::setUrl(const QUrl& url)
{
    Q_D(QQuickWebView);

    if (url.isEmpty())
        return;

    d->webPageProxy->loadURL(url.toString());
    emitUrlChangeIfNeeded();
}

// Make sure we don't emit urlChanged unless it actually changed
void QQuickWebView::emitUrlChangeIfNeeded()
{
    Q_D(QQuickWebView);

    WTF::String activeUrl = d->webPageProxy->activeURL();
    if (activeUrl != d->m_currentUrl) {
        d->m_currentUrl = activeUrl;
        emit urlChanged();
    }
}

QUrl QQuickWebView::icon() const
{
    Q_D(const QQuickWebView);
    return QUrl(d->m_iconUrl);
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

/*!
    \internal
*/
void QQuickWebView::platformInitialize()
{
    JSC::initializeThreading();
    WTF::initializeMainThread();
}

bool QQuickWebView::childMouseEventFilter(QQuickItem* item, QEvent* event)
{
    if (!isVisible() || !isEnabled())
        return false;

    // This function is used by MultiPointTouchArea and PinchArea to filter
    // touch events, thus to hinder the canvas from sending synthesized
    // mouse events to the Flickable implementation we need to reimplement
    // childMouseEventFilter to ignore touch and mouse events.

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        // Force all mouse and touch events through the default propagation path.
        return false;
    default:
        ASSERT(event->type() == QEvent::UngrabMouse);
        break;
    }

    return QQuickFlickable::childMouseEventFilter(item, event);
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
    d->handleMouseEvent(event);
}

void QQuickWebView::mouseMoveEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    d->handleMouseEvent(event);
}

void QQuickWebView::mouseReleaseEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    d->handleMouseEvent(event);
}

void QQuickWebView::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_D(QQuickWebView);
    forceActiveFocus();
    d->handleMouseEvent(event);
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

    if (pos == contentPos())
        return;

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

    If an \a unreachableUrl is passed it is used as the url for the loaded
    content. This is typically used to display error pages for a failed
    load.

    \sa WebView::url
*/
void QQuickWebView::loadHtml(const QString& html, const QUrl& baseUrl, const QUrl& unreachableUrl)
{
    Q_D(QQuickWebView);
    if (unreachableUrl.isValid())
        d->webPageProxy->loadAlternateHTMLString(html, baseUrl.toString(), unreachableUrl.toString());
    else
        d->webPageProxy->loadHTMLString(html, baseUrl.toString());
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

bool QQuickWebView::allowAnyHTTPSCertificateForLocalHost() const
{
    Q_D(const QQuickWebView);
    return d->m_allowAnyHTTPSCertificateForLocalHost;
}

void QQuickWebView::setAllowAnyHTTPSCertificateForLocalHost(bool allow)
{
    Q_D(QQuickWebView);
    d->m_allowAnyHTTPSCertificateForLocalHost = allow;
}


#include "moc_qquickwebview_p.cpp"
