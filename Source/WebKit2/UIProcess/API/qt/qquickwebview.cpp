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
#include "qquickwebview.h"

#include "QtViewInterface.h"
#include "QtWebPageProxy.h"
#include "UtilsQt.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"

#include "qquickwebpage_p.h"
#include "qquickwebview_p.h"
#include "qwebnavigationcontroller.h"
#include "qwebpreferences_p.h"

#include <QtDeclarative/QQuickCanvas>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <WKOpenPanelResultListener.h>

QQuickWebViewPrivate::QQuickWebViewPrivate()
    : q_ptr(0)
    , navigationController(0)
    , useTraditionalDesktopBehaviour(false)
{
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

void QQuickWebViewPrivate::initialize(QQuickWebView* viewport, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    q_ptr = viewport;
    viewport->setFlags(QQuickItem::ItemClipsChildrenToShape);

    QObject::connect(viewport, SIGNAL(visibleChanged()), viewport, SLOT(_q_onVisibleChanged()));
    pageView.reset(new QQuickWebPage(viewport));
    viewInterface.reset(new WebKit::QtViewInterface(viewport, pageView.data()));

    QQuickWebPagePrivate* const pageViewPrivate = pageView.data()->d;
    setPageProxy(new QtWebPageProxy(viewInterface.data(), 0, this, contextRef, pageGroupRef));
    pageViewPrivate->setPageProxy(pageProxy.data());

    QWebPreferencesPrivate::get(pageProxy->preferences())->setAttribute(QWebPreferencesPrivate::AcceleratedCompositingEnabled, true);
    pageProxy->init();
}

void QQuickWebViewPrivate::initializeDesktop(QQuickWebView* viewport)
{
    if (interactionEngine) {
        QObject::disconnect(interactionEngine.data(), SIGNAL(viewportUpdateRequested()), viewport, SLOT(_q_viewportUpdated()));
        QObject::disconnect(interactionEngine.data(), SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), viewport, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
    }
    interactionEngine.reset(0);
    pageProxy->setViewportInteractionEngine(0);
    enableMouseEvents();
}

void QQuickWebViewPrivate::initializeTouch(QQuickWebView* viewport)
{
    interactionEngine.reset(new QtViewportInteractionEngine(viewport, pageView.data()));
    pageProxy->setViewportInteractionEngine(interactionEngine.data());
    disableMouseEvents();
    QObject::connect(interactionEngine.data(), SIGNAL(viewportUpdateRequested()), viewport, SLOT(_q_viewportUpdated()));
    QObject::connect(interactionEngine.data(), SIGNAL(viewportTrajectoryVectorChanged(const QPointF&)), viewport, SLOT(_q_viewportTrajectoryVectorChanged(const QPointF&)));
    updateViewportSize();
}

void QQuickWebViewPrivate::loadDidCommit()
{
    if (!useTraditionalDesktopBehaviour)
        interactionEngine->reset();
}

void QQuickWebViewPrivate::contentSizeChanged(const QSize& newSize)
{
    if (useTraditionalDesktopBehaviour)
        return;

    pageView->setWidth(newSize.width());
    pageView->setHeight(newSize.height());
}

void QQuickWebViewPrivate::scrollPositionRequested(const QPoint& pos)
{
    if (!useTraditionalDesktopBehaviour)
        interactionEngine->pagePositionRequest(pos);
}

void QQuickWebViewPrivate::_q_viewportUpdated()
{
    Q_Q(QQuickWebView);
    const QRectF visibleRectInPageViewCoordinates = q->mapRectToItem(pageView.data(), q->boundingRect()).intersected(pageView->boundingRect());
    float scale = pageView->scale();
    pageProxy->setVisibleContentRectAndScale(visibleRectInPageViewCoordinates, scale);
}

void QQuickWebViewPrivate::_q_viewportTrajectoryVectorChanged(const QPointF& trajectoryVector)
{
    pageProxy->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QQuickWebViewPrivate::_q_onVisibleChanged()
{
    WebPageProxy* wkPage = toImpl(pageProxy->pageRef());

    wkPage->viewStateDidChange(WebPageProxy::ViewIsVisible);
}


void QQuickWebViewPrivate::updateViewportSize()
{
    Q_Q(QQuickWebView);
    QSize viewportSize = q->boundingRect().size().toSize();

    if (viewportSize.isEmpty())
        return;

    WebPageProxy* wkPage = toImpl(pageProxy->pageRef());
    // Let the WebProcess know about the new viewport size, so that
    // it can resize the content accordingly.
    wkPage->setViewportSize(viewportSize);
    updateViewportConstraints();
    _q_viewportUpdated();
}

void QQuickWebViewPrivate::updateViewportConstraints()
{
    Q_Q(QQuickWebView);
    QSize availableSize = q->boundingRect().size().toSize();

    if (availableSize.isEmpty())
        return;

    WebPageProxy* wkPage = toImpl(pageProxy->pageRef());
    WebPreferences* wkPrefs = wkPage->pageGroup()->preferences();

    // FIXME: Remove later; Hardcode some values for now to make sure the DPI adjustment is being tested.
    wkPrefs->setDeviceDPI(240);
    wkPrefs->setDeviceWidth(480);
    wkPrefs->setDeviceHeight(720);

    int minimumLayoutFallbackWidth = qMax<int>(wkPrefs->layoutFallbackWidth(), availableSize.width());

    WebCore::ViewportAttributes attr = WebCore::computeViewportAttributes(viewportArguments, minimumLayoutFallbackWidth, wkPrefs->deviceWidth(), wkPrefs->deviceHeight(), wkPrefs->deviceDPI(), availableSize);
    WebCore::restrictMinimumScaleFactorToViewportSize(attr, availableSize);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attr);

    QtViewportInteractionEngine::Constraints newConstraints;
    newConstraints.initialScale = attr.initialScale;
    newConstraints.minimumScale = attr.minimumScale;
    newConstraints.maximumScale = attr.maximumScale;
    newConstraints.devicePixelRatio = attr.devicePixelRatio;
    newConstraints.isUserScalable = !!attr.userScalable;
    interactionEngine->setConstraints(newConstraints);

    // Overwrite minimum scale value with fit-to-view value, unless the the content author
    // explicitly says no. NB: We can only do this when we know we have a valid size, ie.
    // after initial layout has completed.
}

void QQuickWebViewPrivate::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    if (useTraditionalDesktopBehaviour)
        return;
    viewportArguments = args;
    updateViewportConstraints();
}


void QQuickWebViewPrivate::runJavaScriptAlert(const QString& alertText)
{
#ifndef QT_NO_MESSAGEBOX
    Q_Q(QQuickWebView);
    const QString title = QObject::tr("JavaScript Alert - %1").arg(q->url().host());
    disableMouseEvents();
    QMessageBox::information(0, title, escapeHtml(alertText), QMessageBox::Ok);
    enableMouseEvents();
#else
    Q_UNUSED(alertText);
#endif
}

bool QQuickWebViewPrivate::runJavaScriptConfirm(const QString& message)
{
    bool result = true;
#ifndef QT_NO_MESSAGEBOX
    Q_Q(QQuickWebView);
    const QString title = QObject::tr("JavaScript Confirm - %1").arg(q->url().host());
    disableMouseEvents();
    result = QMessageBox::Yes == QMessageBox::information(0, title, escapeHtml(message), QMessageBox::Yes, QMessageBox::No);
    enableMouseEvents();
#else
    Q_UNUSED(message);
#endif
    return result;
}

QString QQuickWebViewPrivate::runJavaScriptPrompt(const QString& message, const QString& defaultValue, bool& ok)
{
#ifndef QT_NO_INPUTDIALOG
    Q_Q(QQuickWebView);
    const QString title = QObject::tr("JavaScript Prompt - %1").arg(q->url().host());
    disableMouseEvents();
    QString result = QInputDialog::getText(0, title, escapeHtml(message), QLineEdit::Normal, defaultValue, &ok);
    enableMouseEvents();
    return result;
#else
    Q_UNUSED(message);
    return defaultValue;
#endif
}

void QQuickWebViewPrivate::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, QtViewInterface::FileChooserType type)
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
    if (enable == useTraditionalDesktopBehaviour)
        return;

    useTraditionalDesktopBehaviour = enable;
    if (useTraditionalDesktopBehaviour)
        initializeDesktop(q);
    else
        initializeTouch(q);
}

static QtPolicyInterface::PolicyAction toPolicyAction(QQuickWebView::NavigationPolicy policy)
{
    switch (policy) {
    case QQuickWebView::UsePolicy:
        return QtPolicyInterface::Use;
    case QQuickWebView::DownloadPolicy:
        return QtPolicyInterface::Download;
    case QQuickWebView::IgnorePolicy:
        return QtPolicyInterface::Ignore;
    }
    ASSERT_NOT_REACHED();
    return QtPolicyInterface::Ignore;
}

static bool hasMetaMethod(QObject* object, const char* methodName)
{
    int methodIndex = object->metaObject()->indexOfMethod(QMetaObject::normalizedSignature(methodName));
    return methodIndex >= 0 && methodIndex < object->metaObject()->methodCount();
}

/*!
    \qmlmethod NavigationPolicy DesktopWebView::navigationPolicyForUrl(url, button, modifiers)

    This method should be implemented by the user of DesktopWebView element.

    It will be called to decide the policy for a navigation: whether the WebView should ignore the navigation,
    continue it or start a download. The return value must be one of the policies in the NavigationPolicy enumeration.
*/
QtPolicyInterface::PolicyAction QQuickWebViewPrivate::navigationPolicyForURL(const QUrl& url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    Q_Q(QQuickWebView);
    // We need to check this first because invokeMethod() warns if the method doesn't exist for the object.
    if (!hasMetaMethod(q, "navigationPolicyForUrl(QVariant,QVariant,QVariant)"))
        return QtPolicyInterface::Use;

    QVariant ret;
    if (QMetaObject::invokeMethod(q, "navigationPolicyForUrl", Q_RETURN_ARG(QVariant, ret), Q_ARG(QVariant, url), Q_ARG(QVariant, button), Q_ARG(QVariant, QVariant(modifiers))))
        return toPolicyAction(static_cast<QQuickWebView::NavigationPolicy>(ret.toInt()));
    return QtPolicyInterface::Use;
}

void QQuickWebViewPrivate::setPageProxy(QtWebPageProxy* pageProxy)
{
    Q_Q(QQuickWebView);
    this->pageProxy.reset(pageProxy);
    QObject::connect(pageProxy, SIGNAL(receivedMessageFromNavigatorQtObject(QVariantMap)), q, SIGNAL(messageReceived(QVariantMap)));
}

QQuickWebView::QQuickWebView(QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(new QQuickWebViewPrivate)
{
    Q_D(QQuickWebView);
    d->initialize(this);
    d->initializeTouch(this);
}

QQuickWebView::QQuickWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, QQuickItem* parent)
    : QQuickItem(parent)
    , d_ptr(new QQuickWebViewPrivate)
{
    Q_D(QQuickWebView);
    d->initialize(this, contextRef, pageGroupRef);
    // Used by WebKitTestRunner.
    d->setUseTraditionalDesktopBehaviour(true);
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
    d->pageProxy->load(url);
}

void QQuickWebView::postMessage(const QString& message)
{
    Q_D(QQuickWebView);
    d->pageProxy->postMessageToNavigatorQtObject(message);
}

QUrl QQuickWebView::url() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->url();
}

int QQuickWebView::loadProgress() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->loadProgress();
}

QString QQuickWebView::title() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->title();
}

QWebNavigationController* QQuickWebView::navigationController() const
{
    Q_D(const QQuickWebView);
    if (!d->navigationController)
        const_cast<QQuickWebViewPrivate*>(d)->navigationController = new QWebNavigationController(d->pageProxy.data());
    return d->navigationController;
}

QWebPreferences* QQuickWebView::preferences() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->preferences();
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

void QQuickWebView::touchEvent(QTouchEvent* event)
{
    forceActiveFocus();
    QQuickItem::touchEvent(event);
}

WKPageRef QQuickWebView::pageRef() const
{
    Q_D(const QQuickWebView);
    return d->pageProxy->pageRef();
}

#include "moc_qquickwebview.cpp"
