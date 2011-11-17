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

#include "QtWebError.h"
#include "qwebdownloaditem.h"
#include "qwebdownloaditem_p.h"
#include "qwebpreferences.h"
#include "qwebpreferences_p.h"

#include "ClientImpl.h"
#include "DownloadProxy.h"
#include "DrawingAreaProxyImpl.h"
#include "qwkhistory.h"
#include "qwkhistory_p.h"
#include "FindIndicator.h"
#include "LocalizedStrings.h"
#include "MutableArray.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "NotImplemented.h"
#include "QtPolicyInterface.h"
#include "QtViewInterface.h"
#include "QtViewportInteractionEngine.h"
#include "QtWebUndoCommand.h"
#include "WebBackForwardList.h"
#include "WebContext.h"
#include "WebContextMenuProxyQt.h"
#include "WebEditCommandProxy.h"
#include "WebEventFactoryQt.h"
#include "WebPopupMenuProxyQt.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include <QGuiApplication>
#include <QGraphicsSceneMouseEvent>
#include <QJSEngine>
#include <QMimeData>
#include <QStyleHints>
#include <QTouchEvent>
#include <QUndoStack>
#include <QtDebug>
#include <WebCore/Cursor.h>
#include <WebCore/DragData.h>
#include <WebCore/FloatRect.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Region.h>
#include <WebKit2/WKFrame.h>
#include <WebKit2/WKPageGroup.h>
#include <WebKit2/WKRetainPtr.h>

using namespace WebKit;
using namespace WebCore;

RefPtr<WebContext> QtWebPageProxy::s_defaultContext;
RefPtr<QtDownloadManager> QtWebPageProxy::s_downloadManager;

unsigned QtWebPageProxy::s_defaultPageProxyCount = 0;

PassRefPtr<WebContext> QtWebPageProxy::defaultWKContext()
{
    if (!s_defaultContext) {
        s_defaultContext = WebContext::create(String());
        setupContextInjectedBundleClient(toAPI(s_defaultContext.get()));
        s_downloadManager = QtDownloadManager::create(s_defaultContext.get());
    }
    return s_defaultContext;
}

static inline Qt::DropAction dragOperationToDropAction(unsigned dragOperation)
{
    Qt::DropAction result = Qt::IgnoreAction;
    if (dragOperation & DragOperationCopy)
        result = Qt::CopyAction;
    else if (dragOperation & DragOperationMove)
        result = Qt::MoveAction;
    else if (dragOperation & DragOperationGeneric)
        result = Qt::MoveAction;
    else if (dragOperation & DragOperationLink)
        result = Qt::LinkAction;
    return result;
}

static inline Qt::DropActions dragOperationToDropActions(unsigned dragOperations)
{
    Qt::DropActions result = Qt::IgnoreAction;
    if (dragOperations & DragOperationCopy)
        result |= Qt::CopyAction;
    if (dragOperations & DragOperationMove)
        result |= Qt::MoveAction;
    if (dragOperations & DragOperationGeneric)
        result |= Qt::MoveAction;
    if (dragOperations & DragOperationLink)
        result |= Qt::LinkAction;
    return result;
}

WebCore::DragOperation dropActionToDragOperation(Qt::DropActions actions)
{
    unsigned result = 0;
    if (actions & Qt::CopyAction)
        result |= DragOperationCopy;
    if (actions & Qt::MoveAction)
        result |= (DragOperationMove | DragOperationGeneric);
    if (actions & Qt::LinkAction)
        result |= DragOperationLink;
    if (result == (DragOperationCopy | DragOperationMove | DragOperationGeneric | DragOperationLink))
        result = DragOperationEvery;
    return (DragOperation)result;
}

QtWebPageProxy::QtWebPageProxy(QtViewInterface* viewInterface, QtViewportInteractionEngine* viewportInteractionEngine, QtPolicyInterface* policyInterface, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : m_viewInterface(viewInterface)
    , m_interactionEngine(viewportInteractionEngine)
    , m_panGestureRecognizer(viewportInteractionEngine)
    , m_pinchGestureRecognizer(viewportInteractionEngine)
    , m_tapGestureRecognizer(viewportInteractionEngine, this)
    , m_policyInterface(policyInterface)
    , m_context(contextRef ? toImpl(contextRef) : defaultWKContext())
    , m_undoStack(adoptPtr(new QUndoStack(this)))
    , m_loadProgress(0)
    , m_navigatorQtObjectEnabled(false)
{
    ASSERT(viewInterface);
    m_webPageProxy = m_context->createWebPage(this, toImpl(pageGroupRef));
    m_history = QWKHistoryPrivate::createHistory(this, m_webPageProxy->backForwardList());
    if (!contextRef)
        s_defaultPageProxyCount++;
}

void QtWebPageProxy::init()
{
    m_webPageProxy->initializeWebPage();

    setupPageLoaderClient(this, m_webPageProxy.get());
    setupPageUiClient(this, m_webPageProxy.get());

    if (m_policyInterface)
        setupPagePolicyClient(m_policyInterface, m_webPageProxy.get());
}

QtWebPageProxy::~QtWebPageProxy()
{
    m_webPageProxy->close();
    // The context is the default one and we're deleting the last QtWebPageProxy.
    if (m_context == s_defaultContext) {
        ASSERT(s_defaultPageProxyCount > 0);
        s_defaultPageProxyCount--;
        if (!s_defaultPageProxyCount) {
            s_defaultContext.clear();
            s_downloadManager.clear();
        }
    }
    delete m_history;
}

bool QtWebPageProxy::handleEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseMove:
        return handleMouseMoveEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonPress:
        return handleMousePressEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonRelease:
        return handleMouseReleaseEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::MouseButtonDblClick:
        return handleMouseDoubleClickEvent(reinterpret_cast<QMouseEvent*>(ev));
    case QEvent::Wheel:
        return handleWheelEvent(reinterpret_cast<QWheelEvent*>(ev));
    case QEvent::HoverLeave:
        return handleHoverLeaveEvent(reinterpret_cast<QHoverEvent*>(ev));
    case QEvent::HoverEnter: // Fall-through, for WebKit the distinction doesn't matter.
    case QEvent::HoverMove:
        return handleHoverMoveEvent(reinterpret_cast<QHoverEvent*>(ev));
    case QEvent::DragEnter:
        return handleDragEnterEvent(reinterpret_cast<QDragEnterEvent*>(ev));
    case QEvent::DragLeave:
        return handleDragLeaveEvent(reinterpret_cast<QDragLeaveEvent*>(ev));
    case QEvent::DragMove:
        return handleDragMoveEvent(reinterpret_cast<QDragMoveEvent*>(ev));
    case QEvent::Drop:
        return handleDropEvent(reinterpret_cast<QDropEvent*>(ev));
    case QEvent::KeyPress:
        return handleKeyPressEvent(reinterpret_cast<QKeyEvent*>(ev));
    case QEvent::KeyRelease:
        return handleKeyReleaseEvent(reinterpret_cast<QKeyEvent*>(ev));
    case QEvent::FocusIn:
        return handleFocusInEvent(reinterpret_cast<QFocusEvent*>(ev));
    case QEvent::FocusOut:
        return handleFocusOutEvent(reinterpret_cast<QFocusEvent*>(ev));
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
        touchEvent(static_cast<QTouchEvent*>(ev));
        return true;
    }

    // FIXME: Move all common event handling here.
    return false;
}

bool QtWebPageProxy::handleMouseMoveEvent(QMouseEvent* ev)
{
    // For some reason mouse press results in mouse hover (which is
    // converted to mouse move for WebKit). We ignore these hover
    // events by comparing lastPos with newPos.
    // NOTE: lastPos from the event always comes empty, so we work
    // around that here.
    static QPointF lastPos = QPointF();
    if (lastPos == ev->pos())
        return ev->isAccepted();
    lastPos = ev->pos();

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/0));

    return ev->isAccepted();
}

bool QtWebPageProxy::handleMousePressEvent(QMouseEvent* ev)
{
    if (m_tripleClickTimer.isActive() && (ev->pos() - m_tripleClick).manhattanLength() < qApp->styleHints()->startDragDistance()) {
        m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/3));
        return ev->isAccepted();
    }

    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/1));
    return ev->isAccepted();
}

bool QtWebPageProxy::handleMouseReleaseEvent(QMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/0));
    return ev->isAccepted();
}

bool QtWebPageProxy::handleMouseDoubleClickEvent(QMouseEvent* ev)
{
    m_webPageProxy->handleMouseEvent(NativeWebMouseEvent(ev, /*eventClickCount=*/2));

    m_tripleClickTimer.start(qApp->styleHints()->mouseDoubleClickInterval(), this);
    m_tripleClick = ev->localPos().toPoint();
    return ev->isAccepted();
}

bool QtWebPageProxy::handleWheelEvent(QWheelEvent* ev)
{
    m_webPageProxy->handleWheelEvent(NativeWebWheelEvent(ev));
    return ev->isAccepted();
}

bool QtWebPageProxy::handleHoverLeaveEvent(QHoverEvent* ev)
{
    // To get the correct behavior of mouseout, we need to turn the Leave event of our webview into a mouse move
    // to a very far region.
    QHoverEvent fakeEvent(QEvent::HoverMove, QPoint(INT_MIN, INT_MIN), ev->oldPos());
    return handleHoverMoveEvent(&fakeEvent);
}

bool QtWebPageProxy::handleHoverMoveEvent(QHoverEvent* ev)
{
    QMouseEvent me(QEvent::MouseMove, ev->pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    me.setAccepted(ev->isAccepted());

    return handleMouseMoveEvent(&me);
}

bool QtWebPageProxy::handleDragEnterEvent(QDragEnterEvent* ev)
{
    m_webPageProxy->resetDragOperation();
    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragEntered(&dragData);
    ev->acceptProposedAction();
    return true;
}

bool QtWebPageProxy::handleDragLeaveEvent(QDragLeaveEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    m_webPageProxy->dragExited(&dragData);
    m_webPageProxy->resetDragOperation();

    ev->setAccepted(accepted);
    return accepted;
}

bool QtWebPageProxy::handleDragMoveEvent(QDragMoveEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    m_webPageProxy->dragUpdated(&dragData);
    ev->setDropAction(dragOperationToDropAction(m_webPageProxy->dragSession().operation));
    if (m_webPageProxy->dragSession().operation != DragOperationNone)
        ev->accept();

    ev->setAccepted(accepted);
    return accepted;
}

bool QtWebPageProxy::handleDropEvent(QDropEvent* ev)
{
    bool accepted = ev->isAccepted();

    // FIXME: Should not use QCursor::pos()
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), dropActionToDragOperation(ev->possibleActions()));
    SandboxExtension::Handle handle;
    m_webPageProxy->performDrag(&dragData, String(), handle);
    ev->setDropAction(dragOperationToDropAction(m_webPageProxy->dragSession().operation));
    ev->accept();

    ev->setAccepted(accepted);
    return accepted;
}

void QtWebPageProxy::handleSingleTapEvent(const QTouchEvent::TouchPoint& point)
{
    WebGestureEvent gesture(WebEvent::GestureSingleTap, point.pos().toPoint(), point.screenPos().toPoint(), WebEvent::Modifiers(0), 0);
    m_webPageProxy->handleGestureEvent(gesture);
}

void QtWebPageProxy::handleDoubleTapEvent(const QTouchEvent::TouchPoint& point)
{
    m_webPageProxy->findZoomableAreaForPoint(point.pos().toPoint());
}

void QtWebPageProxy::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == m_tripleClickTimer.timerId())
        m_tripleClickTimer.stop();
    else
        QObject::timerEvent(ev);
}

bool QtWebPageProxy::handleKeyPressEvent(QKeyEvent* ev)
{
    m_webPageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(ev));
    return true;
}

bool QtWebPageProxy::handleKeyReleaseEvent(QKeyEvent* ev)
{
    m_webPageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(ev));
    return true;
}

bool QtWebPageProxy::handleFocusInEvent(QFocusEvent*)
{
    m_webPageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

bool QtWebPageProxy::handleFocusOutEvent(QFocusEvent*)
{
    m_webPageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

void QtWebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    m_viewInterface->didChangeCursor(*cursor.platformCursor());
}

void QtWebPageProxy::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    notImplemented();
}

void QtWebPageProxy::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    m_viewInterface->setViewNeedsDisplay(QRect(rect));
}

void QtWebPageProxy::displayView()
{
    // FIXME: Implement.
}

void QtWebPageProxy::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    // FIXME: Implement.
}

WebCore::IntSize QtWebPageProxy::viewSize()
{
    return WebCore::IntSize(m_viewInterface->drawingAreaSize());
}

bool QtWebPageProxy::isViewWindowActive()
{
    return m_viewInterface->isActive();
}

bool QtWebPageProxy::isViewFocused()
{
    return m_viewInterface->hasFocus();
}

bool QtWebPageProxy::isViewVisible()
{
    return m_viewInterface->isVisible();
}

bool QtWebPageProxy::isViewInWindow()
{
    // FIXME: Implement.
    return true;
}

void QtWebPageProxy::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    // FIXME: Implement.
}

void QtWebPageProxy::exitAcceleratedCompositingMode()
{
    // FIXME: Implement.
}

void QtWebPageProxy::pageDidRequestScroll(const IntPoint& pos)
{
    m_viewInterface->scrollPositionRequested(pos);
}

void QtWebPageProxy::didFinishFirstNonEmptyLayout()
{
    m_viewInterface->didFinishFirstNonEmptyLayout();
}

void QtWebPageProxy::didChangeContentsSize(const IntSize& newSize)
{
    m_viewInterface->didChangeContentsSize(QSize(newSize));
}

void QtWebPageProxy::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    m_viewInterface->didChangeViewportProperties(args);
}

void QtWebPageProxy::toolTipChanged(const String&, const String& newTooltip)
{
    m_viewInterface->didChangeToolTip(QString(newTooltip));
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

FloatRect QtWebPageProxy::convertToDeviceSpace(const FloatRect& rect)
{
    return rect;
}

IntPoint QtWebPageProxy::screenToWindow(const IntPoint& point)
{
    return point;
}

IntRect QtWebPageProxy::windowToScreen(const IntRect& rect)
{
    return rect;
}

FloatRect QtWebPageProxy::convertToUserSpace(const FloatRect& rect)
{
    return rect;
}

void QtWebPageProxy::selectionChanged(bool, bool, bool, bool)
{
}

void QtWebPageProxy::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool)
{
}

PassRefPtr<WebPopupMenuProxy> QtWebPageProxy::createPopupMenuProxy(WebPageProxy*)
{
    return WebPopupMenuProxyQt::create();
}

PassRefPtr<WebContextMenuProxy> QtWebPageProxy::createContextMenuProxy(WebPageProxy*)
{
    return WebContextMenuProxyQt::create(m_webPageProxy.get(), m_viewInterface);
}

void QtWebPageProxy::setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut, bool animate)
{
}

void QtWebPageProxy::didCommitLoadForMainFrame(bool useCustomRepresentation)
{
}

void QtWebPageProxy::didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&)
{
}

void QtWebPageProxy::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}

WKPageRef QtWebPageProxy::pageRef() const
{
    return toAPI(m_webPageProxy.get());;
}

void QtWebPageProxy::didFindZoomableArea(const IntPoint& target, const IntRect& area)
{
    // FIXME: As the find method might not respond immediately during load etc,
    // we should ignore all but the latest request.
    m_interactionEngine->zoomToAreaGestureEnded(QPointF(target), QRectF(area));
}

void QtWebPageProxy::didReceiveMessageFromNavigatorQtObject(const String& message)
{
    QVariantMap variantMap;
    variantMap.insert(QLatin1String("data"), QString(message));
    variantMap.insert(QLatin1String("origin"), url());
    emit receivedMessageFromNavigatorQtObject(variantMap);
}

void QtWebPageProxy::didChangeUrl(const QUrl& url)
{
    m_viewInterface->didChangeUrl(url);
}

void QtWebPageProxy::didChangeTitle(const QString& newTitle)
{
    m_viewInterface->didChangeTitle(newTitle);
}

void QtWebPageProxy::loadDidBegin()
{
    m_viewInterface->loadDidBegin();
}

void QtWebPageProxy::loadDidCommit()
{
    m_viewInterface->loadDidCommit();
}

void QtWebPageProxy::loadDidSucceed()
{
    m_viewInterface->loadDidSucceed();
}

void QtWebPageProxy::loadDidFail(const QtWebError& error)
{
    m_viewInterface->loadDidFail(error);
}

void QtWebPageProxy::didChangeLoadProgress(int newLoadProgress)
{
    m_loadProgress = newLoadProgress;
    m_viewInterface->didChangeLoadProgress(newLoadProgress);
}

void QtWebPageProxy::paint(QPainter* painter, const QRect& area)
{
    if (m_webPageProxy->isValid())
        paintContent(painter, area);
    else
        painter->fillRect(area, Qt::white);
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

bool QtWebPageProxy::canStop() const
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

void QtWebPageProxy::navigationStateChanged()
{
    emit updateNavigationState();
}

void QtWebPageProxy::didRelaunchProcess()
{
    updateNavigationState();
    m_viewInterface->didRelaunchProcess();
    setDrawingAreaSize(m_viewInterface->drawingAreaSize());
}

void QtWebPageProxy::processDidCrash()
{
    updateNavigationState();
    m_panGestureRecognizer.reset();
    m_pinchGestureRecognizer.reset();
    m_tapGestureRecognizer.reset();
    m_viewInterface->processDidCrash();
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
    static String messageName("SetNavigatorQtObjectEnabled");

    ASSERT(enabled != m_navigatorQtObjectEnabled);
    // FIXME: Currently we have to keep this information in both processes and the setting is asynchronous.
    m_navigatorQtObjectEnabled = enabled;
    RefPtr<MutableArray> body = MutableArray::create();
    body->append(m_webPageProxy.get());
    RefPtr<WebBoolean> webEnabled = WebBoolean::create(enabled);
    body->append(webEnabled.get());
    m_context->postMessageToInjectedBundle(messageName, body.get());
}

void QtWebPageProxy::postMessageToNavigatorQtObject(const QString& message)
{
    static String messageName("MessageToNavigatorQtObject");

    RefPtr<MutableArray> body = MutableArray::create();
    body->append(m_webPageProxy.get());
    RefPtr<WebString> contents = WebString::create(String(message));
    body->append(contents.get());
    m_context->postMessageToInjectedBundle(messageName, body.get());
}

void QtWebPageProxy::load(const QUrl& url)
{
    WKRetainPtr<WKURLRef> wkurl(WKURLCreateWithQUrl(url));
    WKPageLoadURL(pageRef(), wkurl.get());
}

QUrl QtWebPageProxy::url() const
{
    WKRetainPtr<WKFrameRef> frame = WKPageGetMainFrame(pageRef());
    if (!frame)
        return QUrl();
    return WKURLCopyQUrl(WKFrameCopyURL(frame.get()));
}

QString QtWebPageProxy::title() const
{
    return WKStringCopyQString(WKPageCopyTitle(toAPI(m_webPageProxy.get())));
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
    Qt::DropActions supportedDropActions = dragOperationToDropActions(dragOperationMask);

    QPoint clientPosition;
    QPoint globalPosition;
    Qt::DropAction actualDropAction;

    m_viewInterface->startDrag(supportedDropActions, dragQImage, mimeData,
                               &clientPosition, &globalPosition, &actualDropAction);

    m_webPageProxy->dragEnded(clientPosition, globalPosition, dropActionToDragOperation(actualDropAction));
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
    s_downloadManager->addDownload(download, downloadItem);
}

void QtWebPageProxy::didReceiveDownloadResponse(QWebDownloadItem* download)
{
    // Now that our downloadItem has everything we need we can emit downloadRequested.
    m_viewInterface->downloadRequested(download);
}

void QtWebPageProxy::paintContent(QPainter* painter, const QRect& area)
{
    // FIXME: Do something with the unpainted region?
    WebCore::Region unpaintedRegion;
    static_cast<DrawingAreaProxyImpl*>(m_webPageProxy->drawingArea())->paint(painter, area, unpaintedRegion);
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

#if ENABLE(TOUCH_EVENTS)
void QtWebPageProxy::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    if (!m_interactionEngine)
        return;

    if (wasEventHandled || event.type() == WebEvent::TouchCancel) {
        m_panGestureRecognizer.reset();
        m_pinchGestureRecognizer.reset();
        m_tapGestureRecognizer.reset();
        return;
    }

    const QTouchEvent* ev = event.nativeEvent();

    switch (ev->type()) {
    case QEvent::TouchBegin:
        ASSERT(!m_interactionEngine->panGestureActive());
        ASSERT(!m_interactionEngine->pinchGestureActive());

        // The interaction engine might still be animating kinetic scrolling or a scale animation
        // such as double-tap to zoom or the bounce back effect. A touch stops the kinetic scrolling
        // where as it does not stop the scale animation.
        if (m_interactionEngine->scrollAnimationActive())
            m_interactionEngine->interruptScrollAnimation();
        break;
    case QEvent::TouchUpdate:
        // The scale animation can only be interrupted by a pinch gesture, which will then take over.
        if (m_interactionEngine->scaleAnimationActive() && m_pinchGestureRecognizer.isRecognized())
            m_interactionEngine->interruptScaleAnimation();
        break;
    default:
        break;
    }

    // If the scale animation is active we don't pass the event to the recognizers. In the future
    // we would want to queue the event here and repost then when the animation ends.
    if (m_interactionEngine->scaleAnimationActive())
        return;

    // Convert the event timestamp from second to millisecond.
    qint64 eventTimestampMillis = static_cast<qint64>(event.timestamp() * 1000);
    m_panGestureRecognizer.recognize(ev, eventTimestampMillis);
    m_pinchGestureRecognizer.recognize(ev);

    if (m_panGestureRecognizer.isRecognized() || m_pinchGestureRecognizer.isRecognized())
        m_tapGestureRecognizer.reset();
    else {
        const QTouchEvent* ev = event.nativeEvent();
        m_tapGestureRecognizer.recognize(ev, eventTimestampMillis);
    }
}
#endif

void QtWebPageProxy::setVisibleContentRectAndScale(const QRectF& visibleContentRect, float scale)
{
    QRect alignedVisibleContentRect = visibleContentRect.toAlignedRect();
    m_webPageProxy->drawingArea()->setVisibleContentsRectAndScale(alignedVisibleContentRect, scale);

    // FIXME: Once we support suspend and resume, this should be delayed until the page is active if the page is suspended.
    m_webPageProxy->setFixedVisibleContentRect(alignedVisibleContentRect);
}

void QtWebPageProxy::setVisibleContentRectTrajectoryVector(const QPointF& trajectoryVector)
{
    m_webPageProxy->drawingArea()->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void QtWebPageProxy::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    m_webPageProxy->handleTouchEvent(NativeWebTouchEvent(event));
    event->accept();
#else
    ASSERT_NOT_REACHED();
    ev->ignore();
#endif
}

void QtWebPageProxy::findZoomableAreaForPoint(const QPoint& point)
{
    m_webPageProxy->findZoomableAreaForPoint(point);
}

#include "moc_QtWebPageProxy.cpp"
