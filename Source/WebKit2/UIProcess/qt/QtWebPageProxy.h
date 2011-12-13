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
#include "WebPageProxy.h"
#include <wtf/RefPtr.h>
#include <QMenu>
#include <QSharedPointer>

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
    QtWebPageProxy(QQuickWebView*, QtPageClient*, WKContextRef = 0, WKPageGroupRef = 0);
    ~QtWebPageProxy();

    PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();

    void goBackTo(int index);
    void goForwardTo(int index);

    WKPageRef pageRef() const;

    void setDrawingAreaSize(const QSize&);

    QWebPreferences* preferences() const;

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

protected:
    QQuickWebView* m_qmlWebView;
    RefPtr<WebKit::WebPageProxy> m_webPageProxy;

private:
    RefPtr<QtWebContext> m_context;
    OwnPtr<QWebNavigationHistory> m_navigationHistory;

    mutable OwnPtr<QWebPreferences> m_preferences;

    bool m_navigatorQtObjectEnabled;

    QSharedPointer<QMenu> activeMenu;
    QtWebPageEventHandler* m_eventHandler;
};

#endif /* QtWebPageProxy_h */
