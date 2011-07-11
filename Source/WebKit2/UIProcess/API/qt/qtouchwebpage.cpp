/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "qtouchwebpage.h"
#include "qtouchwebpage_p.h"
#include "qtouchwebpageproxy.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QUrl>
#include <QtDebug>

QTouchWebPage::QTouchWebPage(QGraphicsItem* parent)
    : QGraphicsWidget(parent)
    , d(new QTouchWebPagePrivate(this))
{
    setFocusPolicy(Qt::TabFocus);
    setAcceptTouchEvents(true);

    connect(this, SIGNAL(scaleChanged()), this, SLOT(onScaleChanged()));
}

QTouchWebPage::~QTouchWebPage()
{
    delete d;
}

void QTouchWebPage::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    d->page->paint(painter, option->exposedRect.toAlignedRect());
}

void QTouchWebPage::load(const QUrl& url)
{
    d->page->load(url);
}

QUrl QTouchWebPage::url() const
{
    return d->page->url();
}

QString QTouchWebPage::title() const
{
    return d->page->title();
}

class FriendlyWidget : public QWidget {
public:
    bool focusNextPrevChild(bool next);
};

void QTouchWebPage::focusNextPrevChildCallback(bool next)
{
    if (!hasFocus())
        return;

    // Find the view which has the focus.
    QList<QGraphicsView*> views = scene()->views();
    const int viewCount = views.count();
    QGraphicsView* focusedView = 0;
    for (int i = 0; i < viewCount; ++i) {
        if (views[i]->hasFocus()) {
            focusedView = views[i];
            break;
        }
    }

    if (focusedView) {
        QWidget* window = focusedView->window();
        FriendlyWidget* friendlyWindow = static_cast<FriendlyWidget*>(window);
        friendlyWindow->focusNextPrevChild(next);
    }
}

/*! \reimp
*/
bool QTouchWebPage::focusNextPrevChild(bool next)
{
#if 0
    // FIXME: Key event is handled by the QtWebPageProxy subclass.
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Tab, Qt::KeyboardModifiers(next ? Qt::NoModifier : Qt::ShiftModifier));
    d->page.keyPressEvent(&ev);
#endif
    return true;
}

/*! \reimp
*/
bool QTouchWebPage::event(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::Show:
        d->page->setPageIsVisible(true);
        break;
    case QEvent::Hide:
        d->page->setPageIsVisible(false);
        break;
    default:
        break;
    }

    if (d->page->handleEvent(ev))
        return true;
    return QGraphicsWidget::event(ev);
}

void QTouchWebPage::timerEvent(QTimerEvent* ev)
{
    if (ev->timerId() == d->m_scaleCommitTimer.timerId()) {
        d->commitScaleChange();
        d->m_scaleCommitTimer.stop();
    }
}

void QTouchWebPage::resizeEvent(QGraphicsSceneResizeEvent* ev)
{
    d->page->setDrawingAreaSize(ev->newSize().toSize());
    QGraphicsWidget::resizeEvent(ev);
}

QAction* QTouchWebPage::navigationAction(QtWebKit::NavigationAction which)
{
    return d->page->navigationAction(which);
}

QTouchWebPagePrivate::QTouchWebPagePrivate(QTouchWebPage* view)
    : q(view)
    , page(0)
    , m_isChangingScale(false)
{
}

void QTouchWebPagePrivate::prepareScaleChange()
{
    ASSERT(!m_isChangingScale);
    m_isChangingScale = true;
    m_scaleCommitTimer.stop();
}

void QTouchWebPagePrivate::commitScaleChange()
{
    ASSERT(m_isChangingScale);
    m_isChangingScale = false;
    // FIXME: add webpage support when the view has its proper WKPage subclass.
}

void QTouchWebPagePrivate::onScaleChanged()
{
    if (!m_isChangingScale)
        m_scaleCommitTimer.start(0.1, q);
}

void QTouchWebPagePrivate::setPage(QTouchWebPageProxy* page)
{
    ASSERT(!this->page);
    ASSERT(page);
    this->page = page;
    QObject::connect(page, SIGNAL(focusNextPrevChild(bool)), q, SLOT(focusNextPrevChildCallback(bool)));
}

void QTouchWebPagePrivate::setViewportRect(const QRectF& viewportRect)
{
    const QRectF visibleArea = q->boundingRect().intersected(viewportRect);
    page->setVisibleArea(visibleArea);
}

#include "moc_qtouchwebpage.cpp"
