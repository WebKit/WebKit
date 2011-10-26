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

#ifndef qdesktopwebview_h
#define qdesktopwebview_h

#include "qwebkitglobal.h"
#include <QUrl>
#include <QtDeclarative/qsgpainteditem.h>
#include <WebKit2/WKBase.h>

class QDesktopWebViewPrivate;
class QWebNavigationController;
class QWebPreferences;

QT_BEGIN_NAMESPACE
class QFocusEvent;
class QMouseEvent;
class QHoverEvent;
class QInputMethodEvent;
class QKeyEvent;
class QPainter;
class QRectF;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QTouchEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace WTR {
    class PlatformWebView;
}

class QWEBKIT_EXPORT QDesktopWebView : public QSGPaintedItem {
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

    QDesktopWebView(QSGItem* parent = 0);
    virtual ~QDesktopWebView();

    QUrl url() const;
    QString title() const;
    int loadProgress() const;

    QWebNavigationController* navigationController() const;
    QWebPreferences* preferences() const;

public Q_SLOTS:
     void load(const QUrl&);

Q_SIGNALS:
    void titleChanged(const QString& title);
    void statusBarMessageChanged(const QString& message);
    void loadStarted();
    void loadSucceeded();
    void loadFailed(QDesktopWebView::ErrorType errorType, int errorCode, const QUrl& url);
    void loadProgressChanged(int progress);
    void urlChanged(const QUrl& url);
    void linkHovered(const QUrl& url, const QString& title);

protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void mousePressEvent(QMouseEvent *);
    virtual void mouseMoveEvent(QMouseEvent *);
    virtual void mouseReleaseEvent(QMouseEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent *);
    virtual void wheelEvent(QWheelEvent*);
    virtual void touchEvent(QTouchEvent*);
    virtual void hoverEnterEvent(QHoverEvent*);
    virtual void hoverMoveEvent(QHoverEvent*);
    virtual void hoverLeaveEvent(QHoverEvent*);
    virtual void dragMoveEvent(QDragMoveEvent*);
    virtual void dragEnterEvent(QDragEnterEvent*);
    virtual void dragLeaveEvent(QDragLeaveEvent*);
    virtual void dropEvent(QDropEvent*);

    virtual void geometryChanged(const QRectF&, const QRectF&);
    void paint(QPainter*);
    virtual bool event(QEvent*);

private:
    QDesktopWebView(WKContextRef, WKPageGroupRef, QSGItem* parent = 0);
    WKPageRef pageRef() const;

    void init();

    friend class WTR::PlatformWebView;
    friend class QDesktopWebViewPrivate;
    QDesktopWebViewPrivate *d;
};

QML_DECLARE_TYPE(QDesktopWebView)
Q_DECLARE_METATYPE(QDesktopWebView::NavigationPolicy)

#endif /* qdesktopwebview_h */
