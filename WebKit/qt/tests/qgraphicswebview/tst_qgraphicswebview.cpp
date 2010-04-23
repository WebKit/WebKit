/*
    Copyright (C) 2009 Jakub Wieczorek <faw217@gmail.com>

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

#include "../util.h"
#include <QtTest/QtTest>
#include <QGraphicsView>
#include <qgraphicswebview.h>
#include <qwebpage.h>
#include <qwebframe.h>

class tst_QGraphicsWebView : public QObject
{
    Q_OBJECT

private slots:
    void qgraphicswebview();
    void crashOnViewlessWebPages();
    void microFocusCoordinates();
};

void tst_QGraphicsWebView::qgraphicswebview()
{
    QGraphicsWebView item;
    item.url();
    item.title();
    item.icon();
    item.zoomFactor();
    item.history();
    item.settings();
    item.page();
    item.setPage(0);
    item.page();
    item.setUrl(QUrl());
    item.setZoomFactor(0);
    item.load(QUrl());
    item.setHtml(QString());
    item.setContent(QByteArray());
    item.isModified();
}

class WebPage : public QWebPage
{
    Q_OBJECT

public:
    WebPage(QObject* parent = 0): QWebPage(parent)
    {
    }

    QGraphicsWebView* webView;

private slots:
    // Force a webview deletion during the load.
    // It should not cause WebPage to crash due to
    // it accessing invalid pageClient pointer.
    void aborting()
    {
        delete webView;
    }
};

void tst_QGraphicsWebView::crashOnViewlessWebPages()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);

    QGraphicsWebView* webView = new QGraphicsWebView;
    WebPage* page = new WebPage;
    webView->setPage(page);
    page->webView = webView;
    connect(page->mainFrame(), SIGNAL(initialLayoutCompleted()), page, SLOT(aborting()));

    scene.addItem(webView);

    view.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view.resize(600, 480);
    webView->resize(view.geometry().size());
    QCoreApplication::processEvents();
    view.show();

    page->mainFrame()->setHtml(QString("data:text/html,"
                                            "<frameset cols=\"25%,75%\">"
                                                "<frame src=\"data:text/html,foo \">"
                                                "<frame src=\"data:text/html,bar\">"
                                            "</frameset>"));

    QVERIFY(waitForSignal(page, SIGNAL(loadFinished(bool))));
}

void tst_QGraphicsWebView::microFocusCoordinates()
{
    QWebPage* page = new QWebPage;
    QGraphicsWebView* webView = new QGraphicsWebView;
    webView->setPage( page );
    QGraphicsView* view = new QGraphicsView;
    QGraphicsScene* scene = new QGraphicsScene(view);
    view->setScene(scene);
    scene->addItem(webView);
    view->setGeometry(QRect(0,0,500,500));

    page->mainFrame()->setHtml("<html><body>" \
        "<input type='text' id='input1' style='font--family: serif' value='' maxlength='20'/><br>" \
        "<canvas id='canvas1' width='500' height='500'/>" \
        "<input type='password'/><br>" \
        "<canvas id='canvas2' width='500' height='500'/>" \
        "</body></html>");

    page->mainFrame()->setFocus();

    QVariant initialMicroFocus = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(initialMicroFocus.isValid());

    page->mainFrame()->scroll(0,300);

    QVariant currentMicroFocus = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(currentMicroFocus.isValid());

    QCOMPARE(initialMicroFocus.toRect().translated(QPoint(0,-300)), currentMicroFocus.toRect());

    delete view;
}


QTEST_MAIN(tst_QGraphicsWebView)

#include "tst_qgraphicswebview.moc"
