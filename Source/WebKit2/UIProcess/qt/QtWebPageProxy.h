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
#include "QtWebContext.h"
#include "ViewportArguments.h"
#include "WebPageProxy.h"
#include <wtf/RefPtr.h>
#include <QMenu>
#include <QSharedPointer>

QT_BEGIN_NAMESPACE
class QUndoStack;
QT_END_NAMESPACE

class QtPageClient;
class QQuickWebPage;
class QQuickWebView;
class QtWebPageEventHandler;
class QWebDownloadItem;
class QWebNavigationHistory;
class QWebPreferences;

namespace WebKit {
class QtWebContext;
}

using namespace WebKit;

// FIXME: needs focus in/out, window activation, support through viewStateDidChange().
class QtWebPageProxy : public QObject {
    Q_OBJECT

public:
    QtWebPageProxy(QQuickWebPage*, QQuickWebView*, QtPageClient*, WKContextRef = 0, WKPageGroupRef = 0);
    ~QtWebPageProxy();

    PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();

    void setViewNeedsDisplay(const WebCore::IntRect&);
    WebCore::IntSize viewSize();
    bool isViewFocused();
    bool isViewVisible();

    void pageDidRequestScroll(const WebCore::IntPoint&);
    void processDidCrash();
    void didRelaunchProcess();

    void didChangeContentsSize(const WebCore::IntSize&);
    void didChangeViewportProperties(const WebCore::ViewportArguments&);

    void registerEditCommand(PassRefPtr<WebKit::WebEditCommandProxy>, WebKit::WebPageProxy::UndoOrRedo);
    void clearAllEditCommands();
    bool canUndoRedo(WebPageProxy::UndoOrRedo);
    void executeUndoRedo(WebPageProxy::UndoOrRedo);

    PassRefPtr<WebKit::WebPopupMenuProxy> createPopupMenuProxy(WebKit::WebPageProxy*);

    void didReceiveMessageFromNavigatorQtObject(const String&);

    bool canGoBack() const;
    void goBack();
    void goBackTo(int index);
    bool canGoForward() const;
    void goForward();
    void goForwardTo(int index);
    bool loading() const;
    void stop();
    bool canReload() const;
    void reload();

    void updateNavigationState();

    WKPageRef pageRef() const;

    void load(const QUrl& url);
    void loadHTMLString(const QString& html, const QUrl& baseUrl);
    QUrl url() const;

    void setDrawingAreaSize(const QSize&);

    QWebPreferences* preferences() const;

    QString title() const;

    void setCustomUserAgent(const QString&);
    QString customUserAgent() const;

    void setNavigatorQtObjectEnabled(bool);
    bool navigatorQtObjectEnabled() const { return m_navigatorQtObjectEnabled; }

    void postMessageToNavigatorQtObject(const QString&);

    qreal textZoomFactor() const;
    qreal pageZoomFactor() const;
    void setTextZoomFactor(qreal zoomFactor);
    void setPageZoomFactor(qreal zoomFactor);
    void setPageAndTextZoomFactors(qreal pageZoomFactor, qreal textZoomFactor);

    void setVisibleContentRectAndScale(const QRectF&, float);
    void setVisibleContentRectTrajectoryVector(const QPointF&);
    void renderToCurrentGLContext(const WebCore::TransformationMatrix&, float);
    void purgeGLResources();

    QWebNavigationHistory* navigationHistory() const;

    void contextMenuItemSelected(const WebContextMenuItemData& data)
    {
        m_webPageProxy->contextMenuItemSelected(data);
    }

    void handleDownloadRequest(DownloadProxy*);
    void init(QtWebPageEventHandler*);

    void showContextMenu(QSharedPointer<QMenu>);
    void hideContextMenu();

    QtWebPageEventHandler* eventHandler() { return m_eventHandler; }

public Q_SLOTS:
    void didReceiveDownloadResponse(QWebDownloadItem* downloadItem);

public:
    Q_SIGNAL void zoomableAreaFound(const QRect&);
    Q_SIGNAL void receivedMessageFromNavigatorQtObject(const QVariantMap&);

protected:
    QQuickWebPage* m_qmlWebPage;
    QQuickWebView* m_qmlWebView;
    RefPtr<WebKit::WebPageProxy> m_webPageProxy;

private:
    RefPtr<QtWebContext> m_context;
    OwnPtr<QWebNavigationHistory> m_navigationHistory;

    mutable OwnPtr<QWebPreferences> m_preferences;

    OwnPtr<QUndoStack> m_undoStack;

    bool m_navigatorQtObjectEnabled;

    QSharedPointer<QMenu> activeMenu;
    QtWebPageEventHandler* m_eventHandler;
};

#endif /* QtWebPageProxy_h */
