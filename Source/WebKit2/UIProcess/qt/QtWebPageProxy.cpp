/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtWebPageProxy.h"

#include <qdeclarativeengine.h>
#include <QtQuick/qquickcanvas.h>
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"
#include "qquickwebpage_p.h"
#include "qwebdownloaditem_p.h"
#include "qwebdownloaditem_p_p.h"
#include "qwebpreferences_p.h"
#include "qwebpreferences_p_p.h"

#include "DownloadProxy.h"
#include "DrawingAreaProxyImpl.h"
#include "LayerTreeHostProxy.h"
#include "qwkhistory.h"
#include "qwkhistory_p.h"
#include "QtDownloadManager.h"
#include "QtPageClient.h"
#include "QtWebPageEventHandler.h"
#include "QtWebUndoCommand.h"
#include "WebBackForwardList.h"
#include "WebContext.h"
#include "WebContextMenuProxyQt.h"
#include "WebEditCommandProxy.h"
#include "WebPopupMenuProxyQt.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include <QDrag>
#include <QMimeData>
#include <QUndoStack>
#include <WebCore/DragData.h>
#include <WebKit2/WKFrame.h>
#include <WebKit2/WKPageGroup.h>
#include <WebKit2/WKRetainPtr.h>

using namespace WebKit;
using namespace WebCore;

QtWebPageProxy::QtWebPageProxy(QQuickWebPage* qmlWebPage, QQuickWebView* qmlWebView, QtPageClient *pageClient, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : m_qmlWebPage(qmlWebPage)
    , m_qmlWebView(qmlWebView)
    , m_context(contextRef ? QtWebContext::create(toImpl(contextRef)) : QtWebContext::defaultContext())
    , m_undoStack(adoptPtr(new QUndoStack(this)))
    , m_navigatorQtObjectEnabled(false)
{
    m_webPageProxy = m_context->createWebPage(pageClient, toImpl(pageGroupRef));
    m_history = QWKHistoryPrivate::createHistory(this, m_webPageProxy->backForwardList());
}

void QtWebPageProxy::init(QtWebPageEventHandler* eventHandler)
{
    m_eventHandler = eventHandler;
    m_webPageProxy->initializeWebPage();
}

QtWebPageProxy::~QtWebPageProxy()
{
    m_webPageProxy->close();
    delete m_history;
}

void QtWebPageProxy::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (activeMenu)
        activeMenu->hide();

    if (menu->isEmpty())
        return;

    QWindow* window = m_qmlWebView->canvas();
    if (!window)
        return;

    activeMenu = menu;

    activeMenu->window()->winId(); // Ensure that the menu has a window
    Q_ASSERT(activeMenu->window()->windowHandle());
    activeMenu->window()->windowHandle()->setTransientParent(window);

    QPoint menuPositionInScene = m_qmlWebView->mapToScene(menu->pos()).toPoint();
    menu->exec(window->mapToGlobal(menuPositionInScene));
    // The last function to get out of exec() clear the local copy.
    if (activeMenu == menu)
        activeMenu.clear();
}

void QtWebPageProxy::hideContextMenu()
{
    if (activeMenu)
        activeMenu->hide();
}

void QtWebPageProxy::setViewNeedsDisplay(const WebCore::IntRect&)
{
    m_qmlWebPage->update();
}

WebCore::IntSize QtWebPageProxy::viewSize()
{
    return WebCore::IntSize(m_qmlWebPage->width(), m_qmlWebPage->height());
}

bool QtWebPageProxy::isViewFocused()
{
    return m_qmlWebView->hasFocus();
}

bool QtWebPageProxy::isViewVisible()
{
    return m_qmlWebView->isVisible() && m_qmlWebPage->isVisible();
}

void QtWebPageProxy::pageDidRequestScroll(const IntPoint& pos)
{
    m_qmlWebView->d_func()->scrollPositionRequested(pos);
}

void QtWebPageProxy::didChangeContentsSize(const IntSize& newSize)
{
    m_qmlWebView->d_func()->didChangeContentsSize(newSize);
}

void QtWebPageProxy::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    m_qmlWebView->d_func()->didChangeViewportProperties(args);
}

void QtWebPageProxy::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo) {
        const QtWebUndoCommand* webUndoCommand = static_cast<const QtWebUndoCommand*>(m_undoStack->command(m_undoStack->index()));
        if (webUndoCommand && webUndoCommand->inUndoRedo())
            return;
        m_undoStack->push(new QtWebUndoCommand(command));
    }
}

void QtWebPageProxy::clearAllEditCommands()
{
    m_undoStack->clear();
}

bool QtWebPageProxy::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo)
        return m_undoStack->canUndo();
    return m_undoStack->canRedo();
}

void QtWebPageProxy::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo)
        m_undoStack->undo();
    else
        m_undoStack->redo();
}

void QtWebPageProxy::selectionChanged(bool, bool, bool, bool)
{
}

PassRefPtr<WebPopupMenuProxy> QtWebPageProxy::createPopupMenuProxy(WebPageProxy*)
{
    return WebPopupMenuProxyQt::create(m_webPageProxy.get(), m_qmlWebView);
}

WKPageRef QtWebPageProxy::pageRef() const
{
    return toAPI(m_webPageProxy.get());;
}

void QtWebPageProxy::didReceiveMessageFromNavigatorQtObject(const String& message)
{
    QVariantMap variantMap;
    variantMap.insert(QLatin1String("data"), QString(message));
    variantMap.insert(QLatin1String("origin"), url());
    emit receivedMessageFromNavigatorQtObject(variantMap);
}

bool QtWebPageProxy::canGoBack() const
{
    return m_webPageProxy->canGoBack();
}

void QtWebPageProxy::goBack()
{
    m_webPageProxy->goBack();
}

bool QtWebPageProxy::canGoForward() const
{
    return m_webPageProxy->canGoForward();
}

void QtWebPageProxy::goForward()
{
    m_webPageProxy->goForward();
}

bool QtWebPageProxy::loading() const
{
    RefPtr<WebKit::WebFrameProxy> mainFrame = m_webPageProxy->mainFrame();
    return mainFrame && !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
}

void QtWebPageProxy::stop()
{
    m_webPageProxy->stopLoading();
}

bool QtWebPageProxy::canReload() const
{
    RefPtr<WebKit::WebFrameProxy> mainFrame = m_webPageProxy->mainFrame();
    if (mainFrame)
        return (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
    return m_webPageProxy->backForwardList()->currentItem();
}

void QtWebPageProxy::reload()
{
    m_webPageProxy->reload(/* reloadFromOrigin */ true);
}

void QtWebPageProxy::updateNavigationState()
{
    emit m_qmlWebView->navigationStateChanged();
}

void QtWebPageProxy::didRelaunchProcess()
{
    updateNavigationState();
    qWarning("WARNING: The web process has been successfully restarted.");
    setDrawingAreaSize(viewSize());
}

void QtWebPageProxy::processDidCrash()
{
    updateNavigationState();

    ASSERT(m_eventHandler);
    m_eventHandler->resetGestureRecognizers();

    WebCore::KURL url(WebCore::ParsedURLString, m_webPageProxy->urlAtProcessExit());
    qWarning("WARNING: The web process experienced a crash on '%s'.", qPrintable(QUrl(url).toString(QUrl::RemoveUserInfo)));
}

QWebPreferences* QtWebPageProxy::preferences() const
{
    if (!m_preferences)
        m_preferences = adoptPtr(QWebPreferencesPrivate::createPreferences(const_cast<QtWebPageProxy*>(this)));
    return m_preferences.get();
}

void QtWebPageProxy::setCustomUserAgent(const QString& userAgent)
{
    WKRetainPtr<WKStringRef> wkUserAgent(WKStringCreateWithQString(userAgent));
    WKPageSetCustomUserAgent(pageRef(), wkUserAgent.get());
}

QString QtWebPageProxy::customUserAgent() const
{
    return WKStringCopyQString(WKPageCopyCustomUserAgent(pageRef()));
}

void QtWebPageProxy::setNavigatorQtObjectEnabled(bool enabled)
{
    ASSERT(enabled != m_navigatorQtObjectEnabled);
    // FIXME: Currently we have to keep this information in both processes and the setting is asynchronous.
    m_navigatorQtObjectEnabled = enabled;
    m_context->setNavigatorQtObjectEnabled(m_webPageProxy.get(), enabled);
}

void QtWebPageProxy::postMessageToNavigatorQtObject(const QString& message)
{
    m_context->postMessageToNavigatorQtObject(m_webPageProxy.get(), message);
}

void QtWebPageProxy::loadHTMLString(const QString& html, const QUrl& baseUrl)
{
    WKRetainPtr<WKURLRef> wkUrl(WKURLCreateWithQUrl(baseUrl));
    WKRetainPtr<WKStringRef> wkHtmlString(WKStringCreateWithQString(html));

    WKPageLoadHTMLString(pageRef(), wkHtmlString.get(), wkUrl.get());
}

void QtWebPageProxy::load(const QUrl& url)
{
    WKRetainPtr<WKURLRef> wkurl = adoptWK(WKURLCreateWithQUrl(url));
    WKPageLoadURL(pageRef(), wkurl.get());
}

QUrl QtWebPageProxy::url() const
{
    WKRetainPtr<WKFrameRef> frame = WKPageGetMainFrame(pageRef());
    if (!frame)
        return QUrl();
    return WKURLCopyQUrl(adoptWK(WKFrameCopyURL(frame.get())).get());
}

QString QtWebPageProxy::title() const
{
    return WKStringCopyQString(adoptWK(WKPageCopyTitle(toAPI(m_webPageProxy.get()))).get());
}

void QtWebPageProxy::setDrawingAreaSize(const QSize& size)
{
    if (!m_webPageProxy->drawingArea())
        return;

    m_webPageProxy->drawingArea()->setSize(IntSize(size), IntSize());
}

qreal QtWebPageProxy::textZoomFactor() const
{
    return WKPageGetTextZoomFactor(pageRef());
}

void QtWebPageProxy::setTextZoomFactor(qreal zoomFactor)
{
    WKPageSetTextZoomFactor(pageRef(), zoomFactor);
}

qreal QtWebPageProxy::pageZoomFactor() const
{
    return WKPageGetPageZoomFactor(pageRef());
}

void QtWebPageProxy::setPageZoomFactor(qreal zoomFactor)
{
    WKPageSetPageZoomFactor(pageRef(), zoomFactor);
}

void QtWebPageProxy::setPageAndTextZoomFactors(qreal pageZoomFactor, qreal textZoomFactor)
{
    WKPageSetPageAndTextZoomFactors(pageRef(), pageZoomFactor, textZoomFactor);
}

QWKHistory* QtWebPageProxy::history() const
{
    return m_history;
}

void QtWebPageProxy::startDrag(const WebCore::DragData& dragData, PassRefPtr<ShareableBitmap> dragImage)
{
    QImage dragQImage;
    if (dragImage)
        dragQImage = dragImage->createQImage();
    else if (dragData.platformData() && dragData.platformData()->hasImage())
        dragQImage = qvariant_cast<QImage>(dragData.platformData()->imageData());


    DragOperation dragOperationMask = dragData.draggingSourceOperationMask();
    QMimeData* mimeData = const_cast<QMimeData*>(dragData.platformData());
    Qt::DropActions supportedDropActions = QtWebPageEventHandler::dragOperationToDropActions(dragOperationMask);

    QPoint clientPosition;
    QPoint globalPosition;
    Qt::DropAction actualDropAction = Qt::IgnoreAction;

    if (QWindow* window = m_qmlWebView->canvas()) {
        QDrag* drag = new QDrag(window);
        drag->setPixmap(QPixmap::fromImage(dragQImage));
        drag->setMimeData(mimeData);
        actualDropAction = drag->exec(supportedDropActions);
        globalPosition = QCursor::pos();
        clientPosition = window->mapFromGlobal(globalPosition);
    }

    m_webPageProxy->dragEnded(clientPosition, globalPosition, QtWebPageEventHandler::dropActionToDragOperation(actualDropAction));
}

void QtWebPageProxy::handleDownloadRequest(DownloadProxy* download)
{
    // This function is responsible for hooking up a DownloadProxy to our API layer
    // by creating a QWebDownloadItem. It will then wait for the QWebDownloadItem to be
    // ready (filled with the ResourceResponse information) so we can pass it through to
    // our WebViews.
    QWebDownloadItem* downloadItem = new QWebDownloadItem();
    downloadItem->d->downloadProxy = download;

    connect(downloadItem->d, SIGNAL(receivedResponse(QWebDownloadItem*)), this, SLOT(didReceiveDownloadResponse(QWebDownloadItem*)));
    m_context->downloadManager()->addDownload(download, downloadItem);
}

void QtWebPageProxy::didReceiveDownloadResponse(QWebDownloadItem* downloadItem)
{
    // Now that our downloadItem has everything we need we can emit downloadRequested.
    if (!downloadItem)
        return;

    QDeclarativeEngine::setObjectOwnership(downloadItem, QDeclarativeEngine::JavaScriptOwnership);
    emit m_qmlWebView->experimental()->downloadRequested(downloadItem);
}

PassOwnPtr<DrawingAreaProxy> QtWebPageProxy::createDrawingAreaProxy()
{
    return DrawingAreaProxyImpl::create(m_webPageProxy.get());
}

void QtWebPageProxy::renderToCurrentGLContext(const TransformationMatrix& transform, float opacity)
{
    DrawingAreaProxy* drawingArea = m_webPageProxy->drawingArea();
    if (drawingArea)
        drawingArea->paintToCurrentGLContext(transform, opacity);
}

void QtWebPageProxy::purgeGLResources()
{
    DrawingAreaProxy* drawingArea = m_webPageProxy->drawingArea();
    if (drawingArea && drawingArea->layerTreeHostProxy())
        drawingArea->layerTreeHostProxy()->purgeGLResources();
}

void QtWebPageProxy::setVisibleContentRectAndScale(const QRectF& visibleContentRect, float scale)
{
    if (!m_webPageProxy->drawingArea())
        return;

    QRect alignedVisibleContentRect = visibleContentRect.toAlignedRect();
    m_webPageProxy->drawingArea()->setVisibleContentsRectAndScale(alignedVisibleContentRect, scale);

    // FIXME: Once we support suspend and resume, this should be delayed until the page is active if the page is suspended.
    m_webPageProxy->setFixedVisibleContentRect(alignedVisibleContentRect);
}

void QtWebPageProxy::setVisibleContentRectTrajectoryVector(const QPointF& trajectoryVector)
{
    if (!m_webPageProxy->drawingArea())
        return;

    m_webPageProxy->drawingArea()->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

#include "moc_QtWebPageProxy.cpp"
