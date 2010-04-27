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
    , m_resizesToContents(false)
{
    setScene(new QGraphicsScene(this));
    scene()->addItem(m_item);

    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QStateMachine* machine = new QStateMachine(this);
    QState* s0 = new QState(machine);
    s0->assignProperty(this, "yRotation", 0);

    QState* s1 = new QState(machine);
    s1->assignProperty(this, "yRotation", 90);

    QAbstractTransition* t1 = s0->addTransition(this, SIGNAL(yFlipRequest()), s1);
    QPropertyAnimation* yRotationAnim = new QPropertyAnimation(this, "yRotation", this);
    yRotationAnim->setDuration(1000);
    t1->addAnimation(yRotationAnim);

    QState* s2 = new QState(machine);
    s2->assignProperty(this, "yRotation", -90);
    s1->addTransition(s1, SIGNAL(propertiesAssigned()), s2);

    QAbstractTransition* t2 = s2->addTransition(s0);
    t2->addAnimation(yRotationAnim);

    machine->setInitialState(s0);
    machine->start();
#endif

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(1000);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(updateFrameRate()));
}

void WebViewGraphicsBased::setResizesToContents(bool b)
{
    m_resizesToContents = b;
    m_item->setResizesToContents(m_resizesToContents);
    if (m_resizesToContents) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void WebViewGraphicsBased::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (m_resizesToContents)
        return;
    QRectF rect(QPoint(0, 0), event->size());
    m_item->setGeometry(rect);
    scene()->setSceneRect(rect);
}

void WebViewGraphicsBased::setFrameRateMeasurementEnabled(bool enabled)
{
    m_measureFps = enabled;
    if (m_measureFps) {
        m_lastConsultTime = m_startTime = QTime::currentTime();
        m_fpsTimer.start();
        m_updateTimer->start();
    } else {
        m_fpsTimer.stop();
        m_updateTimer->stop();
    }
}

void WebViewGraphicsBased::updateFrameRate()
{
    const QTime now = QTime::currentTime();
    int interval = m_lastConsultTime.msecsTo(now);
    int frames = m_fpsTimer.numFrames(interval);
    int current = interval ? frames * 1000 / interval : 0;

    emit currentFPSUpdated(current);

    m_lastConsultTime = now;
}

void WebViewGraphicsBased::animatedFlip()
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QSizeF center = m_item->boundingRect().size() / 2;
    QPointF centerPoint = QPointF(center.width(), center.height());
    m_item->setTransformOriginPoint(centerPoint);

    QPropertyAnimation* animation = new QPropertyAnimation(m_item, "rotation", this);
    animation->setDuration(1000);

    int rotation = int(m_item->rotation());

    animation->setStartValue(rotation);
    animation->setEndValue(rotation + 180 - (rotation % 180));

    animation->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}

void WebViewGraphicsBased::animatedYFlip()
{
    emit yFlipRequest();
}

void WebViewGraphicsBased::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
    if (!m_measureFps)
        return;
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

