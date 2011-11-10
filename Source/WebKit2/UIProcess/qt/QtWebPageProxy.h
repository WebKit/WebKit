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

#ifndef QtWebPageProxy_h
#define QtWebPageProxy_h

#include "DrawingAreaProxy.h"
#include "LayerTreeContext.h"
#include "PageClient.h"
#include "QtDownloadManager.h"
#include "QtPanGestureRecognizer.h"
#include "QtPinchGestureRecognizer.h"
#include "QtPolicyInterface.h"
#include "QtTapGestureRecognizer.h"
#include "QtViewInterface.h"
#include "ShareableBitmap.h"
#include "ViewportArguments.h"
#include "WebContext.h"
#include "WebPageProxy.h"
#include <wtf/RefPtr.h>
#include <QBasicTimer>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMenu>
#include <QSharedPointer>

QT_BEGIN_NAMESPACE
class QUndoStack;
QT_END_NAMESPACE

class QtWebError;
class QWebPreferences;
class QWKHistory;

using namespace WebKit;

WebCore::DragOperation dropActionToDragOperation(Qt::DropActions actions);

// FIXME: needs focus in/out, window activation, support through viewStateDidChange().
class QtWebPageProxy : public QObject, WebKit::PageClient {
    Q_OBJECT

public:
    enum WebAction {
        NoWebAction = - 1,

        Back,
        Forward,
        Stop,
        Reload,

        Undo,
        Redo,

        WebActionCount
    };

    QtWebPageProxy(WebKit::QtViewInterface*, WebKit::QtViewportInteractionEngine* = 0, WebKit::QtPolicyInterface* = 0, WKContextRef = 0, WKPageGroupRef = 0);
    ~QtWebPageProxy();

    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();

    virtual bool handleEvent(QEvent*);

    // PageClient
    virtual void setViewNeedsDisplay(const WebCore::IntRect&);
    virtual void displayView();
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);

    virtual WebCore::IntSize viewSize();
    virtual bool isViewWindowActive();
    virtual bool isViewFocused();
    virtual bool isViewVisible();
    virtual bool isViewInWindow();

#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode();
#endif // USE(ACCELERATED_COMPOSITING)
    virtual void pageDidRequestScroll(const WebCore::IntPoint&);
    virtual void processDidCrash();
    virtual void pageClosed() { }
    virtual void didRelaunchProcess();
    virtual void didChangeContentsSize(const WebCore::IntSize&);

    virtual void startDrag(const WebCore::DragData&, PassRefPtr<ShareableBitmap> dragImage);
    virtual void didChangeViewportProperties(const WebCore::ViewportArguments&);
    virtual void setCursor(const WebCore::Cursor&);
    virtual void setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves);
    virtual void toolTipChanged(const WTF::String&, const WTF::String&);
    virtual void registerEditCommand(PassRefPtr<WebKit::WebEditCommandProxy>, WebKit::WebPageProxy::UndoOrRedo);
    virtual void clearAllEditCommands();
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo);
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo);
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&);
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&);
    virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&);
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&);

    virtual void doneWithKeyEvent(const WebKit::NativeWebKeyboardEvent&, bool wasEventHandled);
    virtual void selectionChanged(bool, bool, bool, bool);
    virtual PassRefPtr<WebKit::WebPopupMenuProxy> createPopupMenuProxy(WebKit::WebPageProxy*);
    virtual PassRefPtr<WebKit::WebContextMenuProxy> createContextMenuProxy(WebKit::WebPageProxy*);

    virtual void setFindIndicator(PassRefPtr<WebKit::FindIndicator>, bool fadeOut);

    virtual void didCommitLoadForMainFrame(bool useCustomRepresentation);
    virtual void didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&);
    virtual double customRepresentationZoomFactor() { return 1; }
    virtual void setCustomRepresentationZoomFactor(double) { }
    virtual void didChangeScrollbarsForMainFrame() const { }

    virtual void flashBackingStoreUpdates(const Vector<WebCore::IntRect>& updateRects);
    virtual void findStringInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount) { }
    virtual void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount) { }

    virtual void didFindZoomableArea(const WebCore::IntPoint&, const WebCore::IntRect&);
    virtual void didReceiveMessageFromNavigatorQtObject(const String&);

    void didChangeUrl(const QUrl&);
    void didChangeTitle(const QString&);

    void loadDidBegin();
    void loadDidCommit();
    void loadDidSucceed();
    void loadDidFail(const QtWebError&);
    void didChangeLoadProgress(int);
    int loadProgress() const { return m_loadProgress; }

    void paint(QPainter*, const QRect&);

    bool canGoBack() const;
    void goBack();
    bool canGoForward() const;
    void goForward();
    bool canStop() const;
    void stop();
    bool canReload() const;
    void reload();

    void updateEditorActions();

    WKPageRef pageRef() const;

    void load(const QUrl& url);
    QUrl url() const;

    void setDrawingAreaSize(const QSize&);

    QWebPreferences* preferences() const;

    QString title() const;

    void setCustomUserAgent(const QString&);
    QString customUserAgent() const;

    void setNavigatorQtObjectEnabled(bool);
    bool navigatorQtObjectEnabled() const { return m_navigatorQtObjectEnabled; }

    void setViewportInteractionEngine(QtViewportInteractionEngine* engine) { m_interactionEngine = engine; m_panGestureRecognizer.setViewportInteractionEngine(engine); m_pinchGestureRecognizer.setViewportInteractionEngine(engine);}

    void postMessageToNavigatorQtObject(const QString&);

    qreal textZoomFactor() const;
    qreal pageZoomFactor() const;
    void setTextZoomFactor(qreal zoomFactor);
    void setPageZoomFactor(qreal zoomFactor);
    void setPageAndTextZoomFactors(qreal pageZoomFactor, qreal textZoomFactor);

    void setVisibleContentRectAndScale(const QRectF&, float);
    void setVisibleContentRectTrajectoryVector(const QPointF&);
    void findZoomableAreaForPoint(const QPoint&);
    void renderToCurrentGLContext(const WebCore::TransformationMatrix&, float);

    QWKHistory* history() const;
    QtViewInterface* viewInterface() const { return m_viewInterface; }

    void handleDownloadRequest(DownloadProxy*);
    void init();

public Q_SLOTS:
    void navigationStateChanged();
    void didReceiveDownloadResponse(QWebDownloadItem*);

public:
    Q_SIGNAL void zoomableAreaFound(const QRect&);
    Q_SIGNAL void updateNavigationState();
    Q_SIGNAL void receivedMessageFromNavigatorQtObject(const QVariantMap&);

protected:
    virtual void paintContent(QPainter* painter, const QRect& area);
    RefPtr<WebKit::WebPageProxy> m_webPageProxy;
    WebKit::QtViewInterface* const m_viewInterface;
    QtViewportInteractionEngine* m_interactionEngine;
    QtPanGestureRecognizer m_panGestureRecognizer;
    QtPinchGestureRecognizer m_pinchGestureRecognizer;
    QtTapGestureRecognizer m_tapGestureRecognizer;
    WebKit::QtPolicyInterface* const m_policyInterface;

private:
    bool handleKeyPressEvent(QKeyEvent*);
    bool handleKeyReleaseEvent(QKeyEvent*);
    bool handleFocusInEvent(QFocusEvent*);
    bool handleFocusOutEvent(QFocusEvent*);
    bool handleMouseMoveEvent(QMouseEvent*);
    bool handleMousePressEvent(QMouseEvent*);
    bool handleMouseReleaseEvent(QMouseEvent*);
    bool handleMouseDoubleClickEvent(QMouseEvent*);
    bool handleWheelEvent(QWheelEvent*);
    bool handleHoverLeaveEvent(QHoverEvent*);
    bool handleHoverMoveEvent(QHoverEvent*);
    bool handleDragEnterEvent(QDragEnterEvent*);
    bool handleDragLeaveEvent(QDragLeaveEvent*);
    bool handleDragMoveEvent(QDragMoveEvent*);
    bool handleDropEvent(QDropEvent*);

    virtual void timerEvent(QTimerEvent*);

#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled);
#endif

    void touchEvent(QTouchEvent*);

    static PassRefPtr<WebContext> defaultWKContext();
    static RefPtr<WebContext> s_defaultContext;
    static RefPtr<QtDownloadManager> s_downloadManager;
    static unsigned s_defaultPageProxyCount;

    RefPtr<WebContext> m_context;
    QWKHistory* m_history;

    mutable OwnPtr<QWebPreferences> m_preferences;

    OwnPtr<QUndoStack> m_undoStack;
    int m_loadProgress;

    bool m_navigatorQtObjectEnabled;
    QPoint m_tripleClick;
    QBasicTimer m_tripleClickTimer;
};

#endif /* QtWebPageProxy_h */
