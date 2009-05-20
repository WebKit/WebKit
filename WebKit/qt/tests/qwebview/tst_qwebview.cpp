/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#include <QtTest/QtTest>

#include <qpainter.h>
#include <qwebview.h>

class tst_QWebView : public QObject
{
    Q_OBJECT

public:
    tst_QWebView();
    virtual ~tst_QWebView();

public slots:
    void init();
    void cleanup();

private slots:
    void renderHints();
};

tst_QWebView::tst_QWebView()
{
}

tst_QWebView::~tst_QWebView()
{
}

void tst_QWebView::init()
{
}

void tst_QWebView::cleanup()
{
}

void tst_QWebView::renderHints()
{
    QWebView webView;

    // default is only text antialiasing
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::Antialiasing, true);
    QVERIFY(webView.renderHints() & QPainter::Antialiasing);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::Antialiasing, false);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::SmoothPixmapTransform, false);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));
}


QTEST_MAIN(tst_QWebView)
#include "tst_qwebview.moc"
