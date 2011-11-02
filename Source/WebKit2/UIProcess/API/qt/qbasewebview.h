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

#ifndef qbasewebview_h
#define qbasewebview_h

#include "qwebkitglobal.h"
#include <QtDeclarative/qquickpainteditem.h>

class QBaseWebViewPrivate;
class QWebNavigationController;
class QWebPreferences;

QT_BEGIN_NAMESPACE
class QPainter;
class QUrl;
QT_END_NAMESPACE

class QWEBKIT_EXPORT QBaseWebView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(QWebNavigationController* navigation READ navigationController CONSTANT FINAL)
    Q_PROPERTY(QWebPreferences* preferences READ preferences CONSTANT FINAL)
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
        HttpError
    };
    virtual ~QBaseWebView();

    // FIXME: We inherit from QQuickPaintedItem until QDesktopWebView works on top of a plain QQuickItem.
    void paint(QPainter*) { }

    QUrl url() const;
    QString title() const;
    int loadProgress() const;

    QWebNavigationController* navigationController() const;
    QWebPreferences* preferences() const;

public Q_SLOTS:
     void load(const QUrl&);
     void postMessage(const QString&);

Q_SIGNALS:
    void titleChanged(const QString& title);
    void statusBarMessageChanged(const QString& message);
    void loadStarted();
    void loadSucceeded();
    void loadFailed(QBaseWebView::ErrorType errorType, int errorCode, const QUrl& url);
    void loadProgressChanged(int progress);
    void urlChanged(const QUrl& url);
    void messageReceived(const QVariantMap& message);

protected:
    QBaseWebView(QBaseWebViewPrivate &dd, QQuickItem *parent = 0);
    // Hides QObject::d_ptr allowing us to use the convenience macros.
    QScopedPointer<QBaseWebViewPrivate> d_ptr;
private:
    Q_DECLARE_PRIVATE(QBaseWebView)
    friend class QtWebPageProxy;
};

#endif /* qbasewebview_h */
