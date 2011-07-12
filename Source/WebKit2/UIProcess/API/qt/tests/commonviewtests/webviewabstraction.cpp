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

#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
#include "webviewabstraction.h"

WebViewAbstraction::WebViewAbstraction()
    : m_touchWebViewWindow(new QTouchWebView)
    , m_desktopWebViewWindow(new QDesktopWebView)
{
    QDesktopWidget* const desktopWidget = QApplication::desktop();
    const QRect mainScreenGeometry = desktopWidget->availableGeometry();

    QRect screenHalf(mainScreenGeometry.topLeft(), QSize(mainScreenGeometry.width() / 2, mainScreenGeometry.height()));
    m_touchWebViewWindow.setGeometry(screenHalf);
    m_touchWebViewWindow.setWindowTitle(QLatin1String("TouchWebView"));
    connect(touchWebView()->page(), SIGNAL(loadStarted()), this, SLOT(touchViewLoadStarted()));
    connect(touchWebView()->page(), SIGNAL(loadSucceeded()), this, SLOT(touchViewLoadSucceeded()));
    connect(touchWebView()->page(), SIGNAL(loadFailed(QWebError)), this, SLOT(touchViewLoadFailed(QWebError)));

    screenHalf.moveLeft(screenHalf.right());
    m_desktopWebViewWindow.setGeometry(screenHalf);
    m_desktopWebViewWindow.setWindowTitle(QLatin1String("DesktopWebView"));
    connect(desktopWebView(), SIGNAL(loadStarted()), this, SLOT(desktopViewLoadStarted()));
    connect(desktopWebView(), SIGNAL(loadSucceeded()), this, SLOT(desktopViewLoadSucceeded()));
    connect(desktopWebView(), SIGNAL(loadFailed(QWebError)), this, SLOT(desktopViewLoadFailed(QWebError)));
}

void WebViewAbstraction::show()
{
    m_touchWebViewWindow.show();
    m_desktopWebViewWindow.show();
    QTest::qWaitForWindowShown(&m_desktopWebViewWindow);
}

void WebViewAbstraction::hide()
{
    m_touchWebViewWindow.hide();
    m_desktopWebViewWindow.hide();
}

void WebViewAbstraction::load(const QUrl& url)
{
    touchWebView()->page()->load(url);
    desktopWebView()->load(url);
}

bool WebViewAbstraction::url(QUrl& url) const
{
    const QUrl touchViewUrl = touchWebView()->page()->url();
    const QUrl desktopViewUrl = desktopWebView()->url();

    if (touchViewUrl != desktopViewUrl) {
        qWarning() << "WebViewAbstraction::url(): the URLs are not equals.";
        qWarning() << "QTouchView's url = " << touchViewUrl;
        qWarning() << "QDesktopView's url = " << desktopViewUrl;
        return false;
    }

    url = touchViewUrl;
    return true;
}

void WebViewAbstraction::triggerNavigationAction(QtWebKit::NavigationAction which)
{
    QAction* touchAction = touchWebView()->page()->navigationAction(which);
    touchAction->trigger();
    QAction* desktopAction = desktopWebView()->navigationAction(which);
    desktopAction->trigger();
}

void WebViewAbstraction::touchViewLoadStarted()
{
    m_touchViewSignalsCounter[SIGNAL(loadStarted())]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadStarted())] == m_desktopViewSignalsCounter[SIGNAL(loadStarted())])
        emit loadStarted();
}

void WebViewAbstraction::desktopViewLoadStarted()
{
    m_desktopViewSignalsCounter[SIGNAL(loadStarted())]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadStarted())] == m_desktopViewSignalsCounter[SIGNAL(loadStarted())])
        emit loadStarted();
}

void WebViewAbstraction::touchViewLoadSucceeded()
{
    m_touchViewSignalsCounter[SIGNAL(loadSucceeded())]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadSucceeded())] == m_desktopViewSignalsCounter[SIGNAL(loadSucceeded())])
        emit loadSucceeded();
}

void WebViewAbstraction::desktopViewLoadSucceeded()
{
    m_desktopViewSignalsCounter[SIGNAL(loadSucceeded())]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadSucceeded())] == m_desktopViewSignalsCounter[SIGNAL(loadSucceeded())])
        emit loadSucceeded();
}

void WebViewAbstraction::touchViewLoadFailed(const QWebError& error)
{
    m_touchViewSignalsCounter[SIGNAL(loadFailed(QWebError))]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadFailed(QWebError))] == m_desktopViewSignalsCounter[SIGNAL(loadFailed(QWebError))])
        emit loadFailed(error);
}

void WebViewAbstraction::desktopViewLoadFailed(const QWebError& error)
{
    m_desktopViewSignalsCounter[SIGNAL(loadFailed(QWebError))]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadFailed(QWebError))] == m_desktopViewSignalsCounter[SIGNAL(loadFailed(QWebError))])
        emit loadFailed(error);
}

QTouchWebView* WebViewAbstraction::touchWebView() const
{
    return static_cast<QTouchWebView*>(m_touchWebViewWindow.webView.data());
}

QDesktopWebView* WebViewAbstraction::desktopWebView() const
{
    return static_cast<QDesktopWebView*>(m_desktopWebViewWindow.webView.data());
}

