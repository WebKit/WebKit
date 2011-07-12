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
#include <QtTest/QtTest>
#include <qtouchwebpage.h>
#include <qtouchwebview.h>
#include "../testwindow.h"

Q_DECLARE_METATYPE(QTouchWebPage*);

class tst_QTouchWebView : public QObject {
    Q_OBJECT
public:
    tst_QTouchWebView();
private slots:
    void init();
    void cleanup();

    void accessPage();
    void navigationActionsStatusAtStartup();

private:
    inline QTouchWebView* webView() const;
    QScopedPointer<TestWindow> m_window;
};

tst_QTouchWebView::tst_QTouchWebView()
{
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

void tst_QTouchWebView::navigationActionsStatusAtStartup()
{
    QAction* backAction = webView()->page()->navigationAction(QtWebKit::Back);
    QVERIFY(backAction);
    QCOMPARE(backAction->isEnabled(), false);

    QAction* forwardAction = webView()->page()->navigationAction(QtWebKit::Forward);
    QVERIFY(forwardAction);
    QCOMPARE(forwardAction->isEnabled(), false);

    QAction* stopAction = webView()->page()->navigationAction(QtWebKit::Stop);
    QVERIFY(stopAction);
    QCOMPARE(stopAction->isEnabled(), false);

    QAction* reloadAction = webView()->page()->navigationAction(QtWebKit::Reload);
    QVERIFY(reloadAction);
    QCOMPARE(reloadAction->isEnabled(), false);
}


QTEST_MAIN(tst_QTouchWebView)

#include "tst_qtouchwebview.moc"

