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

#include "../util.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <qgraphicswkview.h>
#include <qwkcontext.h>

class View;

class tst_QGraphicsWKView : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void loadEmptyUrl();
    void loadEmptyPage();

private:
    View* m_view;
};

class View : public QGraphicsView {
public:
    View();
    QGraphicsWKView* m_webView;

protected:
    void resizeEvent(QResizeEvent*);
};

View::View()
{
    QGraphicsScene* const scene = new QGraphicsScene(this);
    setScene(scene);

    QWKContext* context = new QWKContext(this);
    m_webView = new QGraphicsWKView(context);
    scene->addItem(m_webView);
}

void View::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    QRectF rect(QPoint(0, 0), event->size());
    m_webView->setGeometry(rect);
    scene()->setSceneRect(rect);
}

void tst_QGraphicsWKView::init()
{
    m_view = new View;
}

void tst_QGraphicsWKView::cleanup()
{
    delete m_view;
    m_view = 0;
}

void tst_QGraphicsWKView::loadEmptyPage()
{
    m_view->show();

    m_view->m_webView-> load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html")));
    QVERIFY(waitForSignal(m_view->m_webView, SIGNAL(loadFinished(bool))));
}

void tst_QGraphicsWKView::loadEmptyUrl()
{
    // That should not crash.
    m_view->show();
    m_view->m_webView->load(QUrl());
    QVERIFY(!waitForSignal(m_view->m_webView->page(), SIGNAL(engineConnectionChanged(bool)), 50));

    m_view->m_webView->load(QUrl(QLatin1String("")));
    QVERIFY(!waitForSignal(m_view->m_webView->page(), SIGNAL(engineConnectionChanged(bool)), 50));
}

QTEST_MAIN(tst_QGraphicsWKView)

#include "tst_qgraphicswkview.moc"

