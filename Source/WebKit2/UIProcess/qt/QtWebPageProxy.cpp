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
#include "qwkcontext.h"
#include "qwkcontext_p.h"
#include "qwkhistory.h"
#include "qwkhistory_p.h"
#include "ViewInterface.h"
#include "FindIndicator.h"
#include "LocalizedStrings.h"
#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
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

QWKContext* defaultWKContext()
{
    static QWKContext* defaultContext = new QWKContext();
    return defaultContext;
}

static WebCore::ContextMenuAction contextMenuActionForWebAction(QtWebPageProxy::WebAction action)
{
    switch (action) {
    case QtWebPageProxy::OpenLink:
        return WebCore::ContextMenuItemTagOpenLink;
    case QtWebPageProxy::OpenLinkInNewWindow:
        return WebCore::ContextMenuItemTagOpenLinkInNewWindow;
    case QtWebPageProxy::CopyLinkToClipboard:
        return WebCore::ContextMenuItemTagCopyLinkToClipboard;
    case QtWebPageProxy::OpenImageInNewWindow:
        return WebCore::ContextMenuItemTagOpenImageInNewWindow;
    case QtWebPageProxy::Cut:
        return WebCore::ContextMenuItemTagCut;
    case QtWebPageProxy::Copy:
        return WebCore::ContextMenuItemTagCopy;
    case QtWebPageProxy::Paste:
        return WebCore::ContextMenuItemTagPaste;
    case QtWebPageProxy::SelectAll:
        return WebCore::ContextMenuItemTagSelectAll;
    default:
        ASSERT(false);
        break;
    }
    return WebCore::ContextMenuItemTagNoAction;
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

QtWebPageProxy::QtWebPageProxy(ViewInterface* viewInterface, QWKContext* c, WKPageGroupRef pageGroupRef)
    : m_viewInterface(viewInterface)
    , m_context(c)
    , m_preferences(0)
    , m_createNewPageFn(0)
    , m_undoStack(adoptPtr(new QUndoStack(this)))
{
    ASSERT(viewInterface);
    memset(m_actions, 0, sizeof(m_actions));
    m_webPageProxy = m_context->d->context->createWebPage(this, toImpl(pageGroupRef));
    m_history = QWKHistoryPrivate::createHistory(this, m_webPageProxy->backForwardList());
}

void QtWebPageProxy::init()
{
    m_webPageProxy->initializeWebPage();
    WKPageLoaderClient loadClient = {
        0,      /* version */
        this,   /* clientInfo */
        qt_wk_didStartProvisionalLoadForFrame,
        qt_wk_didReceiveServerRedirectForProvisionalLoadForFrame,
        qt_wk_didFailProvisionalLoadWithErrorForFrame,
        qt_wk_didCommitLoadForFrame,
        qt_wk_didFinishDocumentLoadForFrame,
        qt_wk_didFinishLoadForFrame,
        qt_wk_didFailLoadWithErrorForFrame,
        qt_wk_didSameDocumentNavigationForFrame,
        qt_wk_didReceiveTitleForFrame,
        qt_wk_didFirstLayoutForFrame,
        qt_wk_didFirstVisuallyNonEmptyLayoutForFrame,
        qt_wk_didRemoveFrameFromHierarchy,
        0, /* didDisplayInsecureContentForFrame */
        0, /* didRunInsecureContentForFrame */
        0, /* canAuthenticateAgainstProtectionSpaceInFrame */
        0, /* didReceiveAuthenticationChallengeInFrame */
        qt_wk_didStartProgress,
        qt_wk_didChangeProgress,
        qt_wk_didFinishProgress,
        qt_wk_didBecomeUnresponsive,
        qt_wk_didBecomeResponsive,
        0,  /* processDidCrash */
        0,  /* didChangeBackForwardList */
        0,  /* shouldGoToBackForwardListItem */
        0   /* didFailToInitializePlugin */
    };
    WKPageSetPageLoaderClient(pageRef(), &loadClient);

    WKPageUIClient uiClient = {
        0,      /* version */
        this,   /* clientInfo */
        qt_wk_createNewPage,
        qt_wk_showPage,
        qt_wk_close,
        qt_wk_takeFocus,
        0,  /* focus */
        0,  /* unfocus */
        qt_wk_runJavaScriptAlert,
        0,  /* runJavaScriptConfirm */
        0,  /* runJavaScriptPrompt */
        qt_wk_setStatusText,
        0,  /* mouseDidMoveOverElement */
        0,  /* missingPluginButtonClicked */
        0,  /* didNotHandleKeyEvent */
        0,  /* didNotHandleWheelEvent */
        0,  /* toolbarsAreVisible */
        0,  /* setToolbarsAreVisible */
        0,  /* menuBarIsVisible */
        0,  /* setMenuBarIsVisible */
        0,  /* statusBarIsVisible */
        0,  /* setStatusBarIsVisible */
        0,  /* isResizable */
        0,  /* setIsResizable */
        0,  /* getWindowFrame */
        0,  /* setWindowFrame */
        0,  /* runBeforeUnloadConfirmPanel */
        0,  /* didDraw */
        0,  /* pageDidScroll */
        0,  /* exceededDatabaseQuota */
        0,  /* runOpenPanel */
        0,  /* decidePolicyForGeolocationPermissionRequest */
        0,  /* headerHeight */
        0,  /* footerHeight */
        0,  /* drawHeader */
        0,  /* drawFooter */
        0,  /* printFrame */
        0,  /* runModal */
        0,  /* didCompleteRubberBandForMainFrame */
        0,  /* saveDataToFileInDownloadsFolder */
        0,  /* shouldInterruptJavaScript */
    };
    WKPageSetPageUIClient(toAPI(m_webPageProxy.get()), &uiClient);
}

QtWebPageProxy::~QtWebPageProxy()
{
    m_webPageProxy->close();
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
    return WebContextMenuProxyQt::create(this);
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

void QtWebPageProxy::didChangeUrl(const QUrl& url)
{
    m_viewInterface->didChangeUrl(url);
}

void QtWebPageProxy::didChangeTitle(const QString& newTitle)
{
    m_viewInterface->didChangeTitle(newTitle);
}

void QtWebPageProxy::didChangeStatusText(const QString& text)
{
    m_viewInterface->didChangeStatusText(text);
}

void QtWebPageProxy::showContextMenu(QSharedPointer<QMenu> menu)
{
    m_viewInterface->showContextMenu(menu);
}

void QtWebPageProxy::hideContextMenu()
{
    m_viewInterface->hideContextMenu();
}

void QtWebPageProxy::loadDidBegin()
{
    m_viewInterface->loadDidBegin();
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
    m_viewInterface->didChangeLoadProgress(newLoadProgress);
}

void QtWebPageProxy::paint(QPainter* painter, QRect area)
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
    if (!mainFrame)
        return;

    bool enabled = a->isEnabled();
    bool checked = a->isChecked();

    switch (action) {
    case QtWebPageProxy::Back:
        enabled = m_webPageProxy->canGoBack();
        break;
    case QtWebPageProxy::Forward:
        enabled = m_webPageProxy->canGoForward();
        break;
    case QtWebPageProxy::Stop:
        enabled = !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    case QtWebPageProxy::Reload:
        enabled = (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    default:
        break;
    }

    a->setEnabled(enabled);

    if (a->isCheckable())
        a->setChecked(checked);
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
    m_viewInterface->didRelaunchProcess();
    setDrawingAreaSize(m_viewInterface->drawingAreaSize());
}

void QtWebPageProxy::processDidCrash()
{
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

void QtWebPageProxy::setCreateNewPageFunction(CreateNewPageFn function)
{
    m_createNewPageFn = function;
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
        break;
    }

    QAction* qtAction = action(webAction);
    WebKit::WebContextMenuItemData menuItemData(ActionType, contextMenuActionForWebAction(webAction), qtAction->text(), qtAction->isEnabled(), qtAction->isChecked());
    m_webPageProxy->contextMenuItemSelected(menuItemData);
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
    case OpenLink:
        text = contextMenuItemTagOpenLink();
        break;
    case OpenLinkInNewWindow:
        text = contextMenuItemTagOpenLinkInNewWindow();
        break;
    case CopyLinkToClipboard:
        text = contextMenuItemTagCopyLinkToClipboard();
        break;
    case OpenImageInNewWindow:
        text = contextMenuItemTagOpenImageInNewWindow();
        break;
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
    case Cut:
        text = contextMenuItemTagCut();
        break;
    case Copy:
        text = contextMenuItemTagCopy();
        break;
    case Paste:
        text = contextMenuItemTagPaste();
        break;
    case SelectAll:
        text = contextMenuItemTagSelectAll();
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
        return 0;
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

void QtWebPageProxy::setPageIsVisible(bool isVisible)
{
    m_webPageProxy->drawingArea()->setPageIsVisible(isVisible);
}

#include "moc_QtWebPageProxy.cpp"
