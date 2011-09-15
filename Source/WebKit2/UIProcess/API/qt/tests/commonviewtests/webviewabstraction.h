/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef webviewabstraction_h
#define webviewabstraction_h

#include <QHash>
#include <QUrl>
#include <QtTest/QtTest>
#include <qtouchwebpage.h>
#include <qtouchwebview.h>
#include <qdesktopwebview.h>
#include "../testwindow.h"

class WebViewAbstraction : public QObject
{
    Q_OBJECT
public:
    WebViewAbstraction();

    void show();
    void hide();

    void load(const QUrl&);
    bool url(QUrl&) const;
    int loadProgress() const;

    void triggerNavigationAction(QtWebKit::NavigationAction);

Q_SIGNALS:
    void loadStarted();
    void loadSucceeded();
    void loadFailed(const QJSValue&);
    void loadProgressChanged(int);

private Q_SLOTS:
    void touchViewLoadStarted();
    void desktopViewLoadStarted();
    void touchViewLoadSucceeded();
    void desktopViewLoadSucceeded();
    void touchViewLoadFailed(const QJSValue&);
    void desktopViewLoadFailed(const QJSValue&);
    void touchViewLoadProgressChanged(int);
    void desktopViewLoadProgressChanged(int);

private:
    QTouchWebView* touchWebView() const;
    QDesktopWebView* desktopWebView() const;

    TestWindow m_touchWebViewWindow;
    QHash<const char *, unsigned int> m_touchViewSignalsCounter;

    TestWindow m_desktopWebViewWindow;
    QHash<const char *, unsigned int> m_desktopViewSignalsCounter;
};

#endif /* webviewabstraction_h */
