/*
 * Copyright (C) 2018, 2019, 2021, 2024 Igalia S.L
 * Copyright (C) 2018, 2019 Zodiac Inflight Innovations
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>
#include <memory>
#include <wpe/webkit.h>
#include <wtf/glib/GRefPtr.h>

class WPEQtProfile;
class WPEQtViewLoadRequest;

class Q_DECL_EXPORT WPEQtView : public QQuickItem {
    Q_OBJECT
    Q_DISABLE_COPY(WPEQtView)
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY loadingChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY loadingChanged)
    Q_PROPERTY(WPEQtProfile* profile READ profile WRITE setProfile NOTIFY profileChanged FINAL)
    Q_ENUMS(LoadStatus)

public:
    enum LoadStatus {
        LoadStartedStatus,
        LoadStoppedStatus,
        LoadSucceededStatus,
        LoadFailedStatus
    };

    WPEQtView(QQuickItem* parent = nullptr);
    ~WPEQtView();

    void triggerUpdateScene() { QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection); };
    void triggerDidUpdateScene() { QMetaObject::invokeMethod(this, "didUpdateScene", Qt::QueuedConnection); };

    QUrl url() const;
    void setUrl(const QUrl&);
    int loadProgress() const;
    QString title() const;
    bool canGoBack() const;
    bool isLoading() const;
    bool canGoForward() const;

    WebKitWebView* webView() const;

    WPEQtProfile* profile() const;
    void setProfile(WPEQtProfile*);

public Q_SLOTS:
    void goBack();
    void goForward();
    void reload();
    void stop();
    void loadHtml(const QString& html, const QUrl& baseUrl = QUrl());
    void runJavaScript(const QString& script, const QJSValue& callback = QJSValue());

Q_SIGNALS:
    void webViewCreated();
    void urlChanged();
    void titleChanged();
    void loadingChanged(WPEQtViewLoadRequest* loadRequest);
    void loadProgressChanged();
    void profileChanged();

protected:
    bool errorOccured() const { return m_errorOccured; };
    void setErrorOccured(bool errorOccured) { m_errorOccured = errorOccured; };

    void geometryChange(const QRectF&, const QRectF&) override;

    void hoverEnterEvent(QHoverEvent*) override;
    void hoverLeaveEvent(QHoverEvent*) override;
    void hoverMoveEvent(QHoverEvent*) override;

    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;

    void touchEvent(QTouchEvent*) override;

private Q_SLOTS:
    void configureWindow();
    void createWebView();
    void didUpdateScene();
    void updateHttpUserAgent();
    void updateHttpAcceptLanguage();

private:
    QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) final;

    static void notifyUrlChangedCallback(WPEQtView*);
    static void notifyTitleChangedCallback(WPEQtView*);
    static void notifyLoadProgressCallback(WPEQtView*);
    static void notifyLoadChangedCallback(WebKitWebView*, WebKitLoadEvent, WPEQtView*);
    static void notifyLoadFailedCallback(WebKitWebView*, WebKitLoadEvent, const gchar* failingURI, GError*, WPEQtView*);

    GRefPtr<WebKitWebView> m_webView;
    QUrl m_url;
    QString m_html;
    QUrl m_baseUrl;
    QSize m_size;
    bool m_errorOccured { false };
    WPEQtProfile* m_profile { nullptr };
};
