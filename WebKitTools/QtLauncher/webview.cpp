/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "webview.h"

#include <QtGui>
#include <QGraphicsScene>

WebViewGraphicsBased::WebViewGraphicsBased(QWidget* parent)
    : QGraphicsView(parent)
    , m_item(new GraphicsWebView)
    , m_numPaintsTotal(0)
    , m_numPaintsSinceLastMeasure(0)
    , m_measureFps(false)
{
    setScene(new QGraphicsScene(this));
    scene()->addItem(m_item);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

}

void WebViewGraphicsBased::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    QRectF rect(QPoint(0, 0), event->size());
    m_item->setGeometry(rect);
}

void WebViewGraphicsBased::enableFrameRateMeasurement()
{
    m_measureFps = true;
    m_lastConsultTime = m_startTime = QTime::currentTime();
    QTimer* updateTimer = new QTimer(this);
    updateTimer->setInterval(1000);
    connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateFrameRate()));
    updateTimer->start();
}

void WebViewGraphicsBased::updateFrameRate()
{
    QTime now = QTime::currentTime();

    int interval = m_lastConsultTime.msecsTo(now);
    int total = m_startTime.msecsTo(now);

    int average = total ? m_numPaintsTotal * 1000 / total : 0;
    int current = interval ? m_numPaintsSinceLastMeasure * 1000 / interval : 0;

    qDebug("[FPS] average: %d, current: %d", average, current);

    m_lastConsultTime = now;
    m_numPaintsSinceLastMeasure = 0;
}

void WebViewGraphicsBased::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
    if (!m_measureFps)
        return;
    m_numPaintsSinceLastMeasure++;
    m_numPaintsTotal++;
}

static QMenu* createContextMenu(QWebPage* page, QPoint position)
{
    QMenu* menu = page->createStandardContextMenu();

    QWebHitTestResult r = page->mainFrame()->hitTestContent(position);

    if (!r.linkUrl().isEmpty()) {
        WebPage* webPage = qobject_cast<WebPage*>(page);
        QAction* newTabAction = menu->addAction("Open in Default &Browser", webPage, SLOT(openUrlInDefaultBrowser()));
        newTabAction->setData(r.linkUrl());
        menu->insertAction(menu->actions().at(2), newTabAction);
    }
    return menu;
}

void GraphicsWebView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    setProperty("mouseButtons", QVariant::fromValue(int(event->buttons())));
    setProperty("keyboardModifiers", QVariant::fromValue(int(event->modifiers())));

    QGraphicsWebView::mousePressEvent(event);
}

void WebViewTraditional::mousePressEvent(QMouseEvent* event)
{
    setProperty("mouseButtons", QVariant::fromValue(int(event->buttons())));
    setProperty("keyboardModifiers", QVariant::fromValue(int(event->modifiers())));

    QWebView::mousePressEvent(event);
}

void GraphicsWebView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu* menu = createContextMenu(page(), event->pos().toPoint());
    menu->exec(mapToScene(event->pos()).toPoint());
    delete menu;
}

void WebViewTraditional::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createContextMenu(page(), event->pos());
    menu->exec(mapToGlobal(event->pos()));
    delete menu;
}

