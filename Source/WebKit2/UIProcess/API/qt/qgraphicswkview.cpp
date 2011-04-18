/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "qgraphicswkview.h"

#include "ChunkedUpdateDrawingAreaProxy.h"
#include "IntSize.h"
#include "RunLoop.h"
#include "TiledDrawingAreaProxy.h"
#include "UpdateChunk.h"
#include "WKAPICast.h"
#include "qwkpage.h"
#include "qwkpage_p.h"
#include <QApplication>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QUrl>
#include <QtDebug>
#include <WebKit2/WKRetainPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;
using namespace WebCore;

struct QGraphicsWKViewPrivate {
    QGraphicsWKViewPrivate(QGraphicsWKView* view);
    WKPageRef pageRef() const { return page->pageRef(); }

    void onToolTipChanged(const QString&);
    void onScaleChanged();
    void commitScale();

    QGraphicsWKView* q;
    QWKPage* page;
    QSharedPointer<QMenu> activeMenu;
    RunLoop::Timer<QGraphicsWKViewPrivate> m_scaleCommitTimer;
    bool m_isChangingScale;
};

QGraphicsWKView::QGraphicsWKView(QWKContext* context, BackingStoreType backingStoreType, QGraphicsItem* parent)
    : QGraphicsWidget(parent)
    , d(new QGraphicsWKViewPrivate(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptHoverEvents(true);


#if ENABLE(TILED_BACKING_STORE)
    if (backingStoreType == Tiled)
        connect(this, SIGNAL(scaleChanged()), this, SLOT(onScaleChanged()));
#endif

    d->page = new QWKPage(context);
    d->page->d->init(this, backingStoreType);
    connect(d->page, SIGNAL(titleChanged(QString)), this, SIGNAL(titleChanged(QString)));
    connect(d->page, SIGNAL(loadStarted()), this, SIGNAL(loadStarted()));
    connect(d->page, SIGNAL(loadFinished(bool)), this, SIGNAL(loadFinished(bool)));
    connect(d->page, SIGNAL(loadProgress(int)), this, SIGNAL(loadProgress(int)));
    connect(d->page, SIGNAL(initialLayoutCompleted()), this, SIGNAL(initialLayoutCompleted()));
    connect(d->page, SIGNAL(urlChanged(const QUrl&)), this, SIGNAL(urlChanged(const QUrl&)));
    connect(d->page, SIGNAL(cursorChanged(const QCursor&)), this, SLOT(updateCursor(const QCursor&)));
    connect(d->page, SIGNAL(focusNextPrevChild(bool)), this, SLOT(focusNextPrevChildCallback(bool)));
    connect(d->page, SIGNAL(showContextMenu(QSharedPointer<QMenu>)), this, SLOT(showContextMenu(QSharedPointer<QMenu>)));
    connect(d->page, SIGNAL(toolTipChanged(QString)), this, SLOT(onToolTipChanged(QString)));
}

QGraphicsWKView::~QGraphicsWKView()
{
    delete d->page;
    delete d;
}

QWKPage* QGraphicsWKView::page() const
{
    return d->page;
}

void QGraphicsWKView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    page()->d->paint(painter, option->exposedRect.toAlignedRect());
}

void QGraphicsWKView::setGeometry(const QRectF& rect)
{
    QSizeF oldSize = geometry().size();
    QGraphicsWidget::setGeometry(rect);
    if (geometry().size() == oldSize)
        return;

    // NOTE: call geometry() as setGeometry ensures that
    // the geometry is within legal bounds (minimumSize, maximumSize)
    page()->setViewportSize(geometry().size().toSize());
}

void QGraphicsWKView::load(const QUrl& url)
{
    page()->load(url);
}

void QGraphicsWKView::setUrl(const QUrl& url)
{
    page()->setUrl(url);
}

QUrl QGraphicsWKView::url() const
{
    return page()->url();
}

QString QGraphicsWKView::title() const
{
    return page()->title();
}

void QGraphicsWKView::triggerPageAction(QWKPage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

void QGraphicsWKView::back()
{
    page()->triggerAction(QWKPage::Back);
}

void QGraphicsWKView::forward()
{
    page()->triggerAction(QWKPage::Forward);
}

void QGraphicsWKView::reload()
{
    page()->triggerAction(QWKPage::Reload);
}

void QGraphicsWKView::stop()
{
    page()->triggerAction(QWKPage::Stop);
}

void QGraphicsWKView::updateCursor(const QCursor& cursor)
{
    setCursor(cursor);
}

class FriendlyWidget : public QWidget
{
public:
    bool focusNextPrevChild(bool next);
};

void QGraphicsWKView::focusNextPrevChildCallback(bool next)
{
    if (hasFocus()) {
        // find the view which has the focus:
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
}

/*! \reimp
*/
bool QGraphicsWKView::focusNextPrevChild(bool next)
{
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Tab, Qt::KeyboardModifiers(next ? Qt::NoModifier : Qt::ShiftModifier));
    page()->d->keyPressEvent(&ev);
    return true;
}

/*! \reimp
*/
QVariant QGraphicsWKView::itemChange(GraphicsItemChange change, const QVariant& value)
{
    // Here so that it can be reimplemented without breaking ABI.
    return QGraphicsWidget::itemChange(change, value);
}

/*! \reimp
*/
bool QGraphicsWKView::event(QEvent* event)
{
    QEvent::Type eventType = event->type();
    switch (eventType) {
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
        touchEvent(static_cast<QTouchEvent*>(event));
        return true;
    case QEvent::Show:
        page()->d->page->drawingArea()->setPageIsVisible(true);
        break;
    case QEvent::Hide:
        page()->d->page->drawingArea()->setPageIsVisible(false);
        break;
    default:
        break;
    }

    // Here so that it can be reimplemented without breaking ABI.
    return QGraphicsWidget::event(event);
}

/*! \reimp
*/
QSizeF QGraphicsWKView::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    if (which == Qt::PreferredSize)
        return QSizeF(800, 600);
    return QGraphicsWidget::sizeHint(which, constraint);
}

/*! \reimp
*/
QVariant QGraphicsWKView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    // implement
    return QVariant();
}

/*! \reimp
*/
void QGraphicsWKView::keyPressEvent(QKeyEvent* ev)
{
    page()->d->keyPressEvent(ev);
}

/*! \reimp
*/
void QGraphicsWKView::keyReleaseEvent(QKeyEvent* ev)
{
    page()->d->keyReleaseEvent(ev);
}

void QGraphicsWKView::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
    QGraphicsSceneMouseEvent me(QEvent::GraphicsSceneMouseMove);
    me.setPos(ev->pos());
    me.setScreenPos(ev->screenPos());

    page()->d->mouseMoveEvent(&me);

    if (!ev->isAccepted())
        QGraphicsItem::hoverMoveEvent(ev);
}

void QGraphicsWKView::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    page()->d->mouseMoveEvent(ev);
    if (!ev->isAccepted())
        QGraphicsItem::mouseMoveEvent(ev);
}

void QGraphicsWKView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    page()->d->mousePressEvent(ev);
    if (!ev->isAccepted())
        QGraphicsItem::mousePressEvent(ev);
}

void QGraphicsWKView::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    page()->d->mouseReleaseEvent(ev);
    if (!ev->isAccepted())
        QGraphicsItem::mouseReleaseEvent(ev);
}

void QGraphicsWKView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
    page()->d->mouseDoubleClickEvent(ev);
    if (!ev->isAccepted())
        QGraphicsItem::mouseReleaseEvent(ev);
}

void QGraphicsWKView::wheelEvent(QGraphicsSceneWheelEvent* ev)
{
    page()->d->wheelEvent(ev);
    if (!ev->isAccepted())
        QGraphicsItem::wheelEvent(ev);
}

void QGraphicsWKView::touchEvent(QTouchEvent* ev)
{
    page()->d->touchEvent(ev);
}

void QGraphicsWKView::focusInEvent(QFocusEvent*)
{
    page()->d->page->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
}

void QGraphicsWKView::focusOutEvent(QFocusEvent*)
{
    page()->d->page->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
}


/*!
    This slot is called when the engine require a context sensitive menu to be displayed.

    The \a menu passed as a parameter is the menu to be displayed. It is populated with the
    actions possible for its current position. The menu is empty if there is no action for the position.
*/
void QGraphicsWKView::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (d->activeMenu)
        d->activeMenu->hide();

    if (menu->isEmpty())
        return;

    d->activeMenu = menu;

    QWidget* view = 0;
    if (QGraphicsScene* myScene = scene()) {
        const QList<QGraphicsView*> views = myScene->views();
        for (unsigned i = 0; i < views.size(); ++i) {
            if (views.at(i) == QApplication::focusWidget()) {
                view = views.at(i);
                break;
            }
        }
        if (!view)
            view = views.value(0, 0);
    }
    if (view)
        menu->setParent(view, menu->windowFlags());
    menu->exec(view->mapToGlobal(menu->pos()));
    if (d->activeMenu == menu)
        d->activeMenu.clear();
}

void QGraphicsWKView::takeSnapshot(const QSize& size, const QRect& contentsRect)
{
#if ENABLE(TILED_BACKING_STORE)
    DrawingAreaProxy* drawingArea = page()->d->page->drawingArea();
    if (drawingArea->type() != DrawingAreaTypeTiled)
        return;
    TiledDrawingAreaProxy* tiledDrawingArea = static_cast<TiledDrawingAreaProxy*>(drawingArea);
    tiledDrawingArea->takeSnapshot(size, contentsRect);
#endif
}

QGraphicsWKViewPrivate::QGraphicsWKViewPrivate(QGraphicsWKView* view)
    : q(view)
    , activeMenu(0)
    , m_scaleCommitTimer(RunLoop::current(), this, &QGraphicsWKViewPrivate::commitScale)
    , m_isChangingScale(false)
{
}

QRectF QGraphicsWKView::visibleRect() const
{
    if (!scene())
        return QRectF();

    QList<QGraphicsView*> views = scene()->views();
    if (views.isEmpty())
        return QRectF();

    QGraphicsView* graphicsView = views.at(0);
    int xOffset = graphicsView->horizontalScrollBar()->value();
    int yOffset = graphicsView->verticalScrollBar()->value();
    return mapRectFromScene(QRectF(QPointF(xOffset, yOffset), graphicsView->viewport()->size()));
}

void QGraphicsWKView::prepareScaleChange()
{
#if ENABLE(TILED_BACKING_STORE)
    ASSERT(!d->m_isChangingScale);
    d->m_isChangingScale = true;
    d->m_scaleCommitTimer.stop();
#endif
}

void QGraphicsWKView::commitScaleChange()
{
#if ENABLE(TILED_BACKING_STORE)
    ASSERT(d->m_isChangingScale);
    d->m_isChangingScale = false;
    d->commitScale();
#endif
}

void QGraphicsWKViewPrivate::onScaleChanged()
{
#if ENABLE(TILED_BACKING_STORE)
    if (!m_isChangingScale)
        m_scaleCommitTimer.startOneShot(0.1);
#endif
}

void QGraphicsWKViewPrivate::onToolTipChanged(const QString& toolTip)
{
    q->setToolTip(toolTip);
}

void QGraphicsWKViewPrivate::commitScale()
{
#if ENABLE(TILED_BACKING_STORE)
    DrawingAreaProxy* drawingArea = page->d->page->drawingArea();
    float newScale = q->scale();
    if (drawingArea->type() == DrawingAreaTypeTiled) {
        TiledDrawingAreaProxy* tiledDrawingArea = static_cast<TiledDrawingAreaProxy*>(drawingArea);
        if (tiledDrawingArea->contentsScale() == newScale)
            return;
        tiledDrawingArea->setContentsScale(newScale);
        // For now we block until complete.
        tiledDrawingArea->waitUntilUpdatesComplete();
    }
#endif
}

#include "moc_qgraphicswkview.cpp"
