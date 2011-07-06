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

#ifndef testwindow_h
#define testwindow_h

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QResizeEvent>
#include <QScopedPointer>

// TestWindow: Utility class to ignore QGraphicsView details.
class TestWindow : public QGraphicsView {
public:
    inline TestWindow(QGraphicsWidget* webView);
    QScopedPointer<QGraphicsWidget >webView;

protected:
    inline void resizeEvent(QResizeEvent*);
};

inline TestWindow::TestWindow(QGraphicsWidget* webView)
    : webView(webView)
{
    Q_ASSERT(webView);
    setFrameStyle(QFrame::NoFrame | QFrame::Plain);

    QGraphicsScene* const scene = new QGraphicsScene(this);
    setScene(scene);
    scene->addItem(webView);
}

inline void TestWindow::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    QRectF rect(QPoint(0, 0), event->size());
    webView->setGeometry(rect);
    scene()->setSceneRect(rect);
}

#endif /* testwindow_h */
