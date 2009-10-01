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

#include <QtTest/QtTest>

#include <qgraphicswebview.h>

class tst_QGraphicsWebView : public QObject
{
    Q_OBJECT

private slots:
    void qgraphicswebview();
};

void tst_QGraphicsWebView::qgraphicswebview()
{
    QGraphicsWebView item;
    item.url();
    item.title();
    item.icon();
    item.zoomFactor();
    item.isInteractive();
    item.progress();
    item.toHtml();
    item.history();
    item.settings();
    item.status();
    item.page();
    item.setPage(0);
    item.page();
    item.setUrl(QUrl());
    item.setZoomFactor(0);
    item.setInteractive(true);
    item.load(QUrl());
    item.setHtml(QString());
    item.setContent(QByteArray());
}

QTEST_MAIN(tst_QGraphicsWebView)

#include "tst_qgraphicswebview.moc"
