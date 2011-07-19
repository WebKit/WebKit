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
#include "qwebkittypes.h"
#include <WebKit2/WKBase.h>

#include <QGraphicsWidget>
#include <QUrl>

class QDesktopWebViewPrivate;
class QWebError;

namespace WTR {
    class WebView;
}

class QWEBKIT_EXPORT QDesktopWebView : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)

    Q_ENUMS(NavigationAction)

public:
    QDesktopWebView();
    virtual ~QDesktopWebView();

    QUrl url() const;
    QString title() const;

    Q_INVOKABLE QAction* navigationAction(QtWebKit::NavigationAction which) const;

public Q_SLOTS:
     void load(const QUrl&);

Q_SIGNALS:
    void titleChanged(const QString&);
    void statusBarMessageChanged(const QString&);
    void loadStarted();
    void loadSucceeded();
    void loadFailed(const QWebError&);
    void loadProgress(int progress);
    void urlChanged(const QUrl&);

protected:
    void resizeEvent(QGraphicsSceneResizeEvent*);
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);

    virtual bool event(QEvent*);

private:
    QDesktopWebView(WKContextRef, WKPageGroupRef);
    WKPageRef pageRef() const;

    void init();

    friend class WTR::WebView;
    friend class QDesktopWebViewPrivate;
    QDesktopWebViewPrivate *d;
};
#endif /* qdesktopwebview_h */
