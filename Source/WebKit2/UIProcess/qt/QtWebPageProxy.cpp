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
#include "qwebpreferences_p.h"

#include "ClientImpl.h"
#include "qwkhistory.h"
#include "qwkhistory_p.h"
#include "FindIndicator.h"
#include "LocalizedStrings.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "QtPolicyInterface.h"
#include "QtViewInterface.h"
#include "QtWebUndoCommand.h"
#include "WebBackForwardList.h"
#include "WebContext.h"
#include "WebContextMenuProxyQt.h"
#include "WebEditCommandProxy.h"
#include "WebEventFactoryQt.h"
#include "WebPopupMenuProxyQt.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QJSEngine>
#include <QMimeData>
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

QtWebPageProxy::QtWebPageProxy(QtViewInterface* viewInterface, QtPolicyInterface* policyInterface, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : m_viewInterface(viewInterface)
    , m_policyInterface(policyInterface)
    , m_context(contextRef ? toImpl(contextRef) : defaultWKContext())
    , m_preferences(0)
    , m_undoStack(adoptPtr(new QUndoStack(this)))
    , m_loadProgress(0)
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
    m_viewInterface->processDidCrash();
}

QWebPreferences* QtWebPageProxy::preferences() const
{
    if (!m_preferences) {
        WKPageGroupRef pageGroupRef = WKPageGetPageGroup(pageRef());
        m_preferences = QWebPreferencesPrivate::createPreferences(pageGroupRef);
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

void QtWebPageProxy::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    m_viewInterface->didChangeViewportProperties(args);
}

#include "moc_QtWebPageProxy.cpp"
