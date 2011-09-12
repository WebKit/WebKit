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

#include "qwkpreferences_p.h"

#include "ClientImpl.h"
#include "qwkhistory.h"
#include "qwkhistory_p.h"
#include "FindIndicator.h"
#include "LocalizedStrings.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "PolicyInterface.h"
#include "ViewInterface.h"
#include "WebBackForwardList.h"
#include "WebContext.h"
#include "WebContextMenuProxyQt.h"
#include "WebEditCommandProxy.h"
#include "WebEventFactoryQt.h"
#include "WebPopupMenuProxyQt.h"
#include "WebUndoCommandQt.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include <QAction>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QStyle>
#include <QTouchEvent>
#include <QUndoStack>
#include <QtDebug>
#include <WebCore/Cursor.h>
#include <WebCore/DragData.h>
#include <WebCore/FloatRect.h>
#include <WebCore/NotImplemented.h>
#include <WebKit2/WKFrame.h>
#include <WebKit2/WKPageGroup.h>
#include <WebKit2/WKRetainPtr.h>

using namespace WebKit;
using namespace WebCore;


RefPtr<WebContext> QtWebPageProxy::s_defaultContext;

unsigned QtWebPageProxy::s_defaultPageProxyCount = 0;

PassRefPtr<WebContext> QtWebPageProxy::defaultWKContext()
{
    if (!s_defaultContext)
        s_defaultContext = WebContext::create(String());
    return s_defaultContext;
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

QtWebPageProxy::QtWebPageProxy(ViewInterface* viewInterface, PolicyInterface* policyInterface, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : m_viewInterface(viewInterface)
    , m_policyInterface(policyInterface)
    , m_context(contextRef ? toImpl(contextRef) : defaultWKContext())
    , m_preferences(0)
    , m_undoStack(adoptPtr(new QUndoStack(this)))
    , m_loadProgress(0)
{
    ASSERT(viewInterface);
    memset(m_actions, 0, sizeof(m_actions));
    m_webPageProxy = m_context->createWebPage(this, toImpl(pageGroupRef));
    m_history = QWKHistoryPrivate::createHistory(this, m_webPageProxy->backForwardList());
    if (!contextRef)
        s_defaultPageProxyCount++;
}

void QtWebPageProxy::init()
{
    m_webPageProxy->initializeWebPage();

    WKPageLoaderClient loadClient;
    memset(&loadClient, 0, sizeof(WKPageLoaderClient));
    loadClient.version = kWKPageLoaderClientCurrentVersion;
    loadClient.clientInfo = this;
    loadClient.didStartProvisionalLoadForFrame = qt_wk_didStartProvisionalLoadForFrame;
    loadClient.didFailProvisionalLoadWithErrorForFrame = qt_wk_didFailProvisionalLoadWithErrorForFrame;
    loadClient.didCommitLoadForFrame = qt_wk_didCommitLoadForFrame;
    loadClient.didFinishLoadForFrame = qt_wk_didFinishLoadForFrame;
    loadClient.didFailLoadWithErrorForFrame = qt_wk_didFailLoadWithErrorForFrame;
    loadClient.didSameDocumentNavigationForFrame = qt_wk_didSameDocumentNavigationForFrame;
    loadClient.didReceiveTitleForFrame = qt_wk_didReceiveTitleForFrame;
    loadClient.didStartProgress = qt_wk_didStartProgress;
    loadClient.didChangeProgress = qt_wk_didChangeProgress;
    loadClient.didFinishProgress = qt_wk_didFinishProgress;
    WKPageSetPageLoaderClient(pageRef(), &loadClient);

    WKPageUIClient uiClient;
    memset(&uiClient, 0, sizeof(WKPageUIClient));
    uiClient.version = kWKPageUIClientCurrentVersion;
    uiClient.clientInfo = m_viewInterface;
    uiClient.setStatusText = qt_wk_setStatusText;
    WKPageSetPageUIClient(toAPI(m_webPageProxy.get()), &uiClient);

    if (m_policyInterface) {
        WKPagePolicyClient policyClient;
        memset(&policyClient, 0, sizeof(WKPagePolicyClient));
        policyClient.version = kWKPagePolicyClientCurrentVersion;
        policyClient.clientInfo = m_policyInterface;
        policyClient.decidePolicyForNavigationAction = qt_wk_decidePolicyForNavigationAction;
        WKPageSetPagePolicyClient(toAPI(m_webPageProxy.get()), &policyClient);
    }
}

QtWebPageProxy::~QtWebPageProxy()
{
    m_webPageProxy->close();
    // The context is the default one and we're deleting the last QtWebPageProxy.
    if (m_context == s_defaultContext) {
        ASSERT(s_defaultPageProxyCount > 0);
        s_defaultPageProxyCount--;
        if (!s_defaultPageProxyCount)
            s_defaultContext.clear();
    }
    delete m_history;
}

bool QtWebPageProxy::handleEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::KeyPress:
        return handleKeyPressEvent(reinterpret_cast<QKeyEvent*>(ev));
    case QEvent::KeyRelease:
        return handleKeyReleaseEvent(reinterpret_cast<QKeyEvent*>(ev));
    case QEvent::FocusIn:
        return handleFocusInEvent(reinterpret_cast<QFocusEvent*>(ev));
    case QEvent::FocusOut:
        return handleFocusOutEvent(reinterpret_cast<QFocusEvent*>(ev));
    }

    // FIXME: Move all common event handling here.
    return false;
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

void QtWebPageProxy::pageDidRequestScroll(const IntPoint& point)
{
    emit scrollRequested(point.x(), point.y());
}

void QtWebPageProxy::didChangeContentsSize(const IntSize& newSize)
{
    m_viewInterface->contentSizeChanged(QSize(newSize));
}

void QtWebPageProxy::toolTipChanged(const String&, const String& newTooltip)
{
    m_viewInterface->didChangeToolTip(QString(newTooltip));
}

void QtWebPageProxy::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    if (undoOrRedo == WebPageProxy::Undo) {
        const WebUndoCommandQt* webUndoCommand = static_cast<const WebUndoCommandQt*>(m_undoStack->command(m_undoStack->index()));
        if (webUndoCommand && webUndoCommand->inUndoRedo())
            return;
        m_undoStack->push(new WebUndoCommandQt(command));
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

void QtWebPageProxy::setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut)
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
    m_viewInterface->didFindZoomableArea(QPoint(target), QRect(area));
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

void QtWebPageProxy::loadDidFail(const QWebError& error)
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

void QtWebPageProxy::updateAction(QtWebPageProxy::WebAction action)
{
    QAction* a = m_actions[action];
    if (!a)
        return;

    RefPtr<WebKit::WebFrameProxy> mainFrame = m_webPageProxy->mainFrame();

    bool enabled = a->isEnabled();

    switch (action) {
    case QtWebPageProxy::Back:
        enabled = m_webPageProxy->canGoBack();
        break;
    case QtWebPageProxy::Forward:
        enabled = m_webPageProxy->canGoForward();
        break;
    case QtWebPageProxy::Stop:
        enabled = mainFrame && !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    case QtWebPageProxy::Reload:
        if (mainFrame)
            enabled = (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        else
            enabled = m_webPageProxy->backForwardList()->currentItem();
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    a->setEnabled(enabled);
}

void QtWebPageProxy::updateNavigationActions()
{
    updateAction(QtWebPageProxy::Back);
    updateAction(QtWebPageProxy::Forward);
    updateAction(QtWebPageProxy::Stop);
    updateAction(QtWebPageProxy::Reload);
}

void QtWebPageProxy::webActionTriggered(bool checked)
{
    QAction* a = qobject_cast<QAction*>(sender());
    if (!a)
        return;
    QtWebPageProxy::WebAction action = static_cast<QtWebPageProxy::WebAction>(a->data().toInt());
    triggerAction(action, checked);
}

void QtWebPageProxy::didRelaunchProcess()
{
    updateNavigationActions();
    m_viewInterface->didRelaunchProcess();
    setDrawingAreaSize(m_viewInterface->drawingAreaSize());
}

void QtWebPageProxy::processDidCrash()
{
    updateNavigationActions();
    m_viewInterface->processDidCrash();
}

QWKPreferences* QtWebPageProxy::preferences() const
{
    if (!m_preferences) {
        WKPageGroupRef pageGroupRef = WKPageGetPageGroup(pageRef());
        m_preferences = QWKPreferencesPrivate::createPreferences(pageGroupRef);
    }

    return m_preferences;
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

void QtWebPageProxy::triggerAction(WebAction webAction, bool)
{
    switch (webAction) {
    case Back:
        m_webPageProxy->goBack();
        return;
    case Forward:
        m_webPageProxy->goForward();
        return;
    case Stop:
        m_webPageProxy->stopLoading();
        return;
    case Reload:
        m_webPageProxy->reload(/* reloadFromOrigin */ true);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

QAction* QtWebPageProxy::navigationAction(QtWebKit::NavigationAction which) const
{
    switch (which) {
    case QtWebKit::Back:
        return action(QtWebPageProxy::Back);
    case QtWebKit::Forward:
        return action(QtWebPageProxy::Forward);
    case QtWebKit::Reload:
        return action(QtWebPageProxy::Reload);
    case QtWebKit::Stop:
        return action(QtWebPageProxy::Stop);
    }

    return 0;
}

QAction* QtWebPageProxy::action(WebAction action) const
{
    if (action == QtWebPageProxy::NoWebAction || action >= WebActionCount)
        return 0;

    if (m_actions[action])
        return m_actions[action];

    QString text;
    QIcon icon;
    QStyle* style = qobject_cast<QApplication*>(QCoreApplication::instance())->style();
    bool checkable = false;
    QtWebPageProxy* mutableSelf = const_cast<QtWebPageProxy*>(this);

    switch (action) {
    case Back:
        text = contextMenuItemTagGoBack();
        icon = style->standardIcon(QStyle::SP_ArrowBack);
        break;
    case Forward:
        text = contextMenuItemTagGoForward();
        icon = style->standardIcon(QStyle::SP_ArrowForward);
        break;
    case Stop:
        text = contextMenuItemTagStop();
        icon = style->standardIcon(QStyle::SP_BrowserStop);
        break;
    case Reload:
        text = contextMenuItemTagReload();
        icon = style->standardIcon(QStyle::SP_BrowserReload);
        break;
    case Undo: {
        QAction* undoAction = m_undoStack->createUndoAction(mutableSelf);
        m_actions[action] = undoAction;
        return undoAction;
    }
    case Redo: {
        QAction* redoAction = m_undoStack->createRedoAction(mutableSelf);
        m_actions[action] = redoAction;
        return redoAction;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (text.isEmpty())
        return 0;

    QAction* a = new QAction(mutableSelf);
    a->setText(text);
    a->setData(action);
    a->setCheckable(checkable);
    a->setIcon(icon);

    connect(a, SIGNAL(triggered(bool)), this, SLOT(webActionTriggered(bool)));

    m_actions[action] = a;
    mutableSelf->updateAction(action);
    return a;
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

void QtWebPageProxy::setViewportArguments(const WebCore::ViewportArguments& args)
{
    m_viewInterface->didReceiveViewportArguments(args);
}

void QtWebPageProxy::setPageIsVisible(bool isVisible)
{
    m_webPageProxy->drawingArea()->setPageIsVisible(isVisible);
}

#include "moc_QtWebPageProxy.cpp"
