/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef qwkpage_p_h
#define qwkpage_p_h

#include "DrawingAreaProxy.h"
#include "LayerTreeContext.h"
#include "PageClient.h"
#include "qwkpage.h"
#include "qgraphicswkview.h"
#include "WebPageProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <QBasicTimer>
#include <QGraphicsView>
#include <QKeyEvent>

class QGraphicsWKView;
class QWKPreferences;

using namespace WebKit;

class QWKPagePrivate : WebKit::PageClient {
public:
    QWKPagePrivate(QWKPage*, QWKContext*);
    ~QWKPagePrivate();

    static QWKPagePrivate* get(QWKPage* page) { return page->d; }

    void init(QGraphicsItem*, QGraphicsWKView::BackingStoreType);

    // PageClient
    virtual PassOwnPtr<WebKit::DrawingAreaProxy> createDrawingAreaProxy();
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
    virtual void didFindZoomableArea(const WebCore::IntRect&);
    virtual void setCursor(const WebCore::Cursor&);
    virtual void setViewportArguments(const WebCore::ViewportArguments&);
    virtual void toolTipChanged(const WTF::String&, const WTF::String&);
    virtual void registerEditCommand(PassRefPtr<WebKit::WebEditCommandProxy>, WebKit::WebPageProxy::UndoOrRedo);
    virtual void clearAllEditCommands();
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo);
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo);
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&);
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&);
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

    void paint(QPainter* painter, QRect);

    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent*);
    void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    void mousePressEvent(QGraphicsSceneMouseEvent*);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);
    void wheelEvent(QGraphicsSceneWheelEvent*);

    void updateAction(QWKPage::WebAction action);
    void updateNavigationActions();
    void updateEditorActions();

    void _q_webActionTriggered(bool checked);

    void touchEvent(QTouchEvent*);

    QWKPage* q;

    QGraphicsItem* view;
    QWKContext* context;
    QWKHistory* history;

    QAction* actions[QWKPage::WebActionCount];
    QWKPreferences* preferences;

    RefPtr<WebKit::WebPageProxy> page;

    WebCore::ViewportArguments viewportArguments;

    QWKPage::CreateNewPageFn createNewPageFn;

    QPoint tripleClick;
    QBasicTimer tripleClickTimer;
    QGraphicsWKView::BackingStoreType backingStoreType;

    bool isConnectedToEngine;
};

class QtViewportAttributesPrivate : public QSharedData {
public:
    QtViewportAttributesPrivate(QWKPage::ViewportAttributes* qq)
        : q(qq)
    { }

    QWKPage::ViewportAttributes* q;
};


#endif /* qkpage_p_h */
