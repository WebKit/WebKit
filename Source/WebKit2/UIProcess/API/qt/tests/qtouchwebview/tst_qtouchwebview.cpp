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

#include <QtTest/QtTest>
#include <qtouchwebpage.h>
#include <qtouchwebview.h>
#include <qwebnavigationcontroller.h>
#include "../testwindow.h"
#include "../util.h"

Q_DECLARE_METATYPE(QTouchWebPage*);

class tst_QTouchWebView : public QObject {
    Q_OBJECT
public:
    tst_QTouchWebView();
private slots:
    void init();
    void cleanup();

    void accessPage();
    void navigationStatusAtStartup();

private:
    inline QTouchWebView* webView() const;
    QScopedPointer<TestWindow> m_window;
};

tst_QTouchWebView::tst_QTouchWebView()
{
    addQtWebProcessToPath();
    qRegisterMetaType<QTouchWebPage*>("QTouchWebPage*");
}

void tst_QTouchWebView::init()
{
    m_window.reset(new TestWindow(new QTouchWebView));
}

void tst_QTouchWebView::cleanup()
{
    m_window.reset();
}

inline QTouchWebView* tst_QTouchWebView::webView() const
{
    return static_cast<QTouchWebView*>(m_window->webView.data());
}

void tst_QTouchWebView::accessPage()
{
    QTouchWebPage* const pageDirectAccess = webView()->page();

    QVariant pagePropertyValue = webView()->property("page");
    QTouchWebPage* const pagePropertyAccess = pagePropertyValue.value<QTouchWebPage*>();
    QCOMPARE(pagePropertyAccess, pageDirectAccess);
}

void tst_QTouchWebView::navigationStatusAtStartup()
{
    QCOMPARE(webView()->navigationController()->canGoBack(), false);

    QCOMPARE(webView()->navigationController()->canGoForward(), false);

    QCOMPARE(webView()->navigationController()->canStop(), false);

    QCOMPARE(webView()->navigationController()->canReload(), false);
}


QTEST_MAIN(tst_QTouchWebView)

#include "tst_qtouchwebview.moc"

