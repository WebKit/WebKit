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

#include "config.h"

#include "LayerTreeContext.h"
#include "PageClient.h"
#include "qwebkittypes.h"
#include "ShareableBitmap.h"
#include "ViewportArguments.h"
#include "ViewInterface.h"
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

class QWKContext;
class QWKHistory;
class QWKPreferences;

using namespace WebKit;

QWKContext *defaultWKContext();

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

    QtWebPageProxy(WebKit::ViewInterface* viewInterface, QWKContext*, WKPageGroupRef = 0);
    ~QtWebPageProxy();

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
    virtual float userSpaceScaleFactor() const { return 1; }

    void didChangeUrl(const QUrl&);
    void didChangeTitle(const QString&);

    void loadDidBegin();
    void loadDidSucceed();
    void loadDidFail(const QWebError&);
    void didChangeLoadProgress(int);

    void paint(QPainter* painter, QRect);

    void updateAction(QtWebPageProxy::WebAction action);
    void updateNavigationActions();
    void updateEditorActions();

    WKPageRef pageRef() const;

    void load(const QUrl& url);
    QUrl url() const;

    void setDrawingAreaSize(const QSize&);

    QWKPreferences* preferences() const;

    QString title() const;

    QAction* navigationAction(QtWebKit::NavigationAction) const;

    QAction* action(WebAction action) const;
    void triggerAction(WebAction action, bool checked = false);

    void setCustomUserAgent(const QString&);
    QString customUserAgent() const;

    qreal textZoomFactor() const;
    qreal pageZoomFactor() const;
    void setTextZoomFactor(qreal zoomFactor);
    void setPageZoomFactor(qreal zoomFactor);
    void setPageAndTextZoomFactors(qreal pageZoomFactor, qreal textZoomFactor);

    QWKHistory* history() const;

    void setPageIsVisible(bool);

public Q_SLOTS:
    void webActionTriggered(bool checked);

public:
    Q_SIGNAL void scrollRequested(int dx, int dy);
    Q_SIGNAL void zoomableAreaFound(const QRect&);

protected:
    void init();

    virtual void paintContent(QPainter* painter, const QRect& area) = 0;
    RefPtr<WebKit::WebPageProxy> m_webPageProxy;
    WebKit::ViewInterface* const m_viewInterface;

private:
    bool handleKeyPressEvent(QKeyEvent*);
    bool handleKeyReleaseEvent(QKeyEvent*);
    bool handleFocusInEvent(QFocusEvent*);
    bool handleFocusOutEvent(QFocusEvent*);

    QWKContext* m_context;
    QWKHistory* m_history;

    mutable QAction* m_actions[QtWebPageProxy::WebActionCount];
    mutable QWKPreferences* m_preferences;

    OwnPtr<QUndoStack> m_undoStack;
};

#endif /* QtWebPageProxy_h */
