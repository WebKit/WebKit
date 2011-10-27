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
#include <qwebnavigationcontroller.h>
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
    connect(touchWebView()->page(), SIGNAL(loadFailed(QTouchWebPage::ErrorType, int, const QUrl&)), this, SLOT(touchViewLoadFailed(QTouchWebPage::ErrorType, int, const QUrl&)));
    connect(touchWebView()->page(), SIGNAL(loadProgressChanged(int)), this, SLOT(touchViewLoadProgressChanged(int)));

    screenHalf.moveLeft(screenHalf.right());
    m_desktopWebViewWindow.setGeometry(screenHalf);
    m_desktopWebViewWindow.setWindowTitle(QLatin1String("DesktopWebView"));
    connect(desktopWebView(), SIGNAL(loadStarted()), this, SLOT(desktopViewLoadStarted()));
    connect(desktopWebView(), SIGNAL(loadSucceeded()), this, SLOT(desktopViewLoadSucceeded()));
    connect(desktopWebView(), SIGNAL(loadFailed(QDesktopWebView::ErrorType, int, const QUrl&)), this, SLOT(desktopViewLoadFailed(QDesktopWebView::ErrorType, int, const QUrl&)));
    connect(desktopWebView(), SIGNAL(loadProgressChanged(int)), this, SLOT(desktopViewLoadProgressChanged(int)));
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

int WebViewAbstraction::loadProgress() const
{
    int touchViewProgress = touchWebView()->page()->loadProgress();
    int desktopViewProgress = desktopWebView()->loadProgress();

    if (touchViewProgress != desktopViewProgress) {
        qWarning() << "WebViewAbstraction::loadProgress(): the load progress are different.";
        qWarning() << "QTouchView's load progress = " << touchViewProgress;
        qWarning() << "QDesktopView's load progress = " << desktopViewProgress;
        return -1;
    }

    return touchViewProgress;
}

void WebViewAbstraction::goBack()
{
    touchWebView()->page()->navigationController()->goBack();
    desktopWebView()->navigationController()->goBack();
}

void WebViewAbstraction::goForward()
{
    touchWebView()->page()->navigationController()->goForward();
    desktopWebView()->navigationController()->goForward();
}

void WebViewAbstraction::stop()
{
    touchWebView()->page()->navigationController()->stop();
    desktopWebView()->navigationController()->stop();
}

void WebViewAbstraction::reload()
{
    touchWebView()->page()->navigationController()->reload();
    desktopWebView()->navigationController()->reload();
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

void WebViewAbstraction::touchViewLoadFailed(QTouchWebPage::ErrorType errorType, int errorCode, const QUrl& url)
{
    m_touchViewSignalsCounter[SIGNAL(loadFailed(QTouchWebPage::ErrorType, int, const QUrl&))]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadFailed(QTouchWebPage::ErrorType, int, const QUrl&))] == m_desktopViewSignalsCounter[SIGNAL(loadFailed(QTouchWebPage::ErrorType, int, QUrl))])
        emit loadFailed(static_cast<QDesktopWebView::ErrorType>(errorType), errorCode, url);
}

void WebViewAbstraction::desktopViewLoadFailed(QDesktopWebView::ErrorType errorType, int errorCode, const QUrl& url)
{
    m_desktopViewSignalsCounter[SIGNAL(loadFailed(QDesktopWebView::ErrorType, int, const QUrl&))]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadFailed(QDesktopWebView::ErrorType, int, const QUrl&))] == m_desktopViewSignalsCounter[SIGNAL(loadFailed(QDesktopWebView::ErrorType, int, QUrl))])
        emit loadFailed(errorType, errorCode, url);
}

void WebViewAbstraction::touchViewLoadProgressChanged(int progress)
{
    m_touchViewSignalsCounter[SIGNAL(loadProgressChanged(int))]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadProgressChanged(int))] == m_desktopViewSignalsCounter[SIGNAL(loadProgressChanged(int))])
        emit loadProgressChanged(progress);
}

void WebViewAbstraction::desktopViewLoadProgressChanged(int progress)
{
    m_desktopViewSignalsCounter[SIGNAL(loadProgressChanged(int))]++;
    if (m_touchViewSignalsCounter[SIGNAL(loadProgressChanged(int))] == m_desktopViewSignalsCounter[SIGNAL(loadProgressChanged(int))])
        emit loadProgressChanged(progress);
}

QTouchWebView* WebViewAbstraction::touchWebView() const
{
    return static_cast<QTouchWebView*>(m_touchWebViewWindow.webView.data());
}

QDesktopWebView* WebViewAbstraction::desktopWebView() const
{
    return static_cast<QDesktopWebView*>(m_desktopWebViewWindow.webView.data());
}

