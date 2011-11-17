/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef qquickwebview_p_h
#define qquickwebview_p_h

#include "qwebkitglobal.h"
#include <QtDeclarative/qquickitem.h>

class QQuickWebPage;
class QQuickWebViewPrivate;
class QQuickWebViewExperimental;
class QWebDownloadItem;
class QWebPreferences;

namespace WebKit {
class QtViewInterface;
}

namespace WTR {
class PlatformWebView;
}

typedef const struct OpaqueWKContext* WKContextRef;
typedef const struct OpaqueWKPageGroup* WKPageGroupRef;
typedef const struct OpaqueWKPage* WKPageRef;

QT_BEGIN_NAMESPACE
class QPainter;
class QUrl;
QT_END_NAMESPACE

class QWEBKIT_EXPORT QQuickWebView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canStop READ canStop NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canReload READ canReload NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(QWebPreferences* preferences READ preferences CONSTANT FINAL)
    Q_PROPERTY(QQuickWebPage* page READ page CONSTANT FINAL)
    Q_ENUMS(NavigationPolicy)
    Q_ENUMS(ErrorType)
public:
    enum NavigationPolicy {
        UsePolicy,
        DownloadPolicy,
        IgnorePolicy
    };

    enum ErrorType {
        EngineError,
        NetworkError,
        HttpError,
        DownloadError
    };
    QQuickWebView(QQuickItem* parent = 0);
    virtual ~QQuickWebView();

    QUrl url() const;
    QString title() const;
    int loadProgress() const;

    bool canGoBack() const;
    bool canGoForward() const;
    bool canStop() const;
    bool canReload() const;

    QWebPreferences* preferences() const;
    QQuickWebPage* page();

    QQuickWebViewExperimental* experimental() const;

public Q_SLOTS:
    void load(const QUrl&);
    void postMessage(const QString&);

    void goBack();
    void goForward();
    void stop();
    void reload();

Q_SIGNALS:
    void titleChanged(const QString& title);
    void statusBarMessageChanged(const QString& message);
    void loadStarted();
    void loadSucceeded();
    void loadFailed(QQuickWebView::ErrorType errorType, int errorCode, const QUrl& url);
    void loadProgressChanged(int progress);
    void urlChanged(const QUrl& url);
    void messageReceived(const QVariantMap& message);
    void downloadRequested(QWebDownloadItem* downloadItem);
    void linkHovered(const QUrl& url, const QString& title);
    void viewModeChanged();
    void navigationStateChanged();

protected:
    virtual void geometryChanged(const QRectF&, const QRectF&);
    virtual void touchEvent(QTouchEvent* event);

private:
    Q_DECLARE_PRIVATE(QQuickWebView)

    QQuickWebView(WKContextRef, WKPageGroupRef, QQuickItem* parent = 0);
    WKPageRef pageRef() const;

    Q_PRIVATE_SLOT(d_func(), void _q_viewportUpdated());
    Q_PRIVATE_SLOT(d_func(), void _q_viewportTrajectoryVectorChanged(const QPointF&));
    Q_PRIVATE_SLOT(d_func(), void _q_onOpenPanelFilesSelected());
    Q_PRIVATE_SLOT(d_func(), void _q_onOpenPanelFinished(int result));
    Q_PRIVATE_SLOT(d_func(), void _q_onVisibleChanged());
    // Hides QObject::d_ptr allowing us to use the convenience macros.
    QScopedPointer<QQuickWebViewPrivate> d_ptr;
    QQuickWebViewExperimental* m_experimental;

    friend class QtWebPageProxy;
    friend class WebKit::QtViewInterface;
    friend class WTR::PlatformWebView;
    friend class QQuickWebViewExperimental;
};

QML_DECLARE_TYPE(QQuickWebView)

class QWEBKIT_EXPORT QQuickWebViewExperimental : public QObject {
    Q_OBJECT
public:
    QQuickWebViewExperimental(QQuickWebView* webView);
    virtual ~QQuickWebViewExperimental();

public Q_SLOTS:
    void setUseTraditionalDesktopBehaviour(bool enable);

private:
    QQuickWebView* q_ptr;
    QQuickWebViewPrivate* d_ptr;

    Q_DECLARE_PRIVATE(QQuickWebView)
    Q_DECLARE_PUBLIC(QQuickWebView)
};

#endif // qquickwebview_p_h
