/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "PageClientQt.h"

#if defined(Q_WS_X11)
#include <QX11Info>
#endif

namespace WebCore {

void PageClientQWidget::scroll(int dx, int dy, const QRect& rectToScroll)
{
    view->scroll(qreal(dx), qreal(dy), rectToScroll);
}

void PageClientQWidget::update(const QRect & dirtyRect)
{
    view->update(dirtyRect);
}

void PageClientQWidget::setInputMethodEnabled(bool enable)
{
    view->setAttribute(Qt::WA_InputMethodEnabled, enable);
}

bool PageClientQWidget::inputMethodEnabled() const
{
    return view->testAttribute(Qt::WA_InputMethodEnabled);
}

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
void PageClientQWidget::setInputMethodHints(Qt::InputMethodHints hints)
{
    view->setInputMethodHints(hints);
}
#endif

#ifndef QT_NO_CURSOR
QCursor PageClientQWidget::cursor() const
{
    return view->cursor();
}

void PageClientQWidget::updateCursor(const QCursor& cursor)
{
    view->setCursor(cursor);
}
#endif

QPalette PageClientQWidget::palette() const
{
    return view->palette();
}

int PageClientQWidget::screenNumber() const
{
#if defined(Q_WS_X11)
    return view->x11Info().screen();
#endif
    return 0;
}

QWidget* PageClientQWidget::ownerWidget() const
{
    return view;
}

QRect PageClientQWidget::geometryRelativeToOwnerWidget() const
{
    return view->geometry();
}

QObject* PageClientQWidget::pluginParent() const
{
    return view;
}

QStyle* PageClientQWidget::style() const
{
    return view->style();
}

QRectF PageClientQWidget::windowRect() const
{
    return QRectF(view->window()->geometry());
}

PageClientQGraphicsWidget::~PageClientQGraphicsWidget()
{
#if USE(ACCELERATED_COMPOSITING)
    if (!rootGraphicsLayer)
        return;
    // we don't need to delete the root graphics layer. The lifecycle is managed in GraphicsLayerQt.cpp.
    rootGraphicsLayer.data()->setParentItem(0);
    view->scene()->removeItem(rootGraphicsLayer.data());
#endif
}

void PageClientQGraphicsWidget::scroll(int dx, int dy, const QRect& rectToScroll)
{
    view->scroll(qreal(dx), qreal(dy), rectToScroll);

#if USE(ACCELERATED_COMPOSITING)
    updateCompositingScrollPosition();
#endif
}

void PageClientQGraphicsWidget::update(const QRect& dirtyRect)
{
    view->update(dirtyRect);

    createOrDeleteOverlay();
    if (overlay)
        overlay->update(QRectF(dirtyRect));
#if USE(ACCELERATED_COMPOSITING)
    syncLayers();
    // This might be a slow-scroll. We ensure that the compositing layers are in the right position.
    updateCompositingScrollPosition();
#endif
}

void PageClientQGraphicsWidget::createOrDeleteOverlay()
{
    bool useOverlay = false;
    if (!viewResizesToContents) {
#if USE(ACCELERATED_COMPOSITING)
        useOverlay = useOverlay || rootGraphicsLayer;
#endif
#if ENABLE(TILED_BACKING_STORE)
        useOverlay = useOverlay || QWebFramePrivate::core(page->mainFrame())->tiledBackingStore();
#endif
    }
    if (useOverlay == !!overlay)
        return;
    if (useOverlay) {
        overlay = QSharedPointer<QGraphicsItemOverlay>(new QGraphicsItemOverlay(view, page));
        overlay->setZValue(OverlayZValue);
    } else
        overlay.clear();
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientQGraphicsWidget::syncLayers()
{
    if (shouldSync) {
        QWebFramePrivate::core(page->mainFrame())->view()->syncCompositingStateRecursive();
        shouldSync = false;
    }
}

void PageClientQGraphicsWidget::setRootGraphicsLayer(QGraphicsItem* layer)
{
    if (rootGraphicsLayer) {
        rootGraphicsLayer.data()->setParentItem(0);
        view->scene()->removeItem(rootGraphicsLayer.data());
        QWebFramePrivate::core(page->mainFrame())->view()->syncCompositingStateRecursive();
    }

    rootGraphicsLayer = layer ? layer->toGraphicsObject() : 0;

    if (layer) {
        layer->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
        layer->setParentItem(view);
        layer->setZValue(RootGraphicsLayerZValue);
        updateCompositingScrollPosition();
    }
    createOrDeleteOverlay();
}

void PageClientQGraphicsWidget::markForSync(bool scheduleSync)
{
    shouldSync = true;
    if (scheduleSync)
        syncMetaMethod.invoke(view, Qt::QueuedConnection);
}

void PageClientQGraphicsWidget::updateCompositingScrollPosition()
{
    if (rootGraphicsLayer && page && page->mainFrame()) {
        const QPoint scrollPosition = page->mainFrame()->scrollPosition();
        rootGraphicsLayer.data()->setPos(-scrollPosition);
    }
}
#endif

#if ENABLE(TILED_BACKING_STORE)
void PageClientQGraphicsWidget::updateTiledBackingStoreScale()
{
    WebCore::TiledBackingStore* backingStore = QWebFramePrivate::core(page->mainFrame())->tiledBackingStore();
    if (!backingStore)
        return;
    backingStore->setContentsScale(view->scale());
}
#endif

void PageClientQGraphicsWidget::setInputMethodEnabled(bool enable)
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    view->setFlag(QGraphicsItem::ItemAcceptsInputMethod, enable);
#endif
}

bool PageClientQGraphicsWidget::inputMethodEnabled() const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    return view->flags() & QGraphicsItem::ItemAcceptsInputMethod;
#else
    return false;
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
void PageClientQGraphicsWidget::setInputMethodHints(Qt::InputMethodHints hints)
{
    view->setInputMethodHints(hints);
}
#endif

#ifndef QT_NO_CURSOR
QCursor PageClientQGraphicsWidget::cursor() const
{
    return view->cursor();
}

void PageClientQGraphicsWidget::updateCursor(const QCursor& cursor)
{
    view->setCursor(cursor);
}
#endif

QPalette PageClientQGraphicsWidget::palette() const
{
    return view->palette();
}

int PageClientQGraphicsWidget::screenNumber() const
{
#if defined(Q_WS_X11)
    if (QGraphicsScene* scene = view->scene()) {
        const QList<QGraphicsView*> views = scene->views();

        if (!views.isEmpty())
            return views.at(0)->x11Info().screen();
    }
#endif

    return 0;
}

QWidget* PageClientQGraphicsWidget::ownerWidget() const
{
    if (QGraphicsScene* scene = view->scene()) {
        const QList<QGraphicsView*> views = scene->views();
        return views.value(0);
    }
    return 0;
}

QRect PageClientQGraphicsWidget::geometryRelativeToOwnerWidget() const
{
    if (!view->scene())
        return QRect();

    QList<QGraphicsView*> views = view->scene()->views();
    if (views.isEmpty())
        return QRect();

    QGraphicsView* graphicsView = views.at(0);
    return graphicsView->mapFromScene(view->boundingRect()).boundingRect();
}

#if ENABLE(TILED_BACKING_STORE)
QRectF PageClientQGraphicsWidget::graphicsItemVisibleRect() const
{
    if (!view->scene())
        return QRectF();

    QList<QGraphicsView*> views = view->scene()->views();
    if (views.isEmpty())
        return QRectF();

    QGraphicsView* graphicsView = views.at(0);
    int xOffset = graphicsView->horizontalScrollBar()->value();
    int yOffset = graphicsView->verticalScrollBar()->value();
    return view->mapRectFromScene(QRectF(QPointF(xOffset, yOffset), graphicsView->viewport()->size()));
}
#endif

QObject* PageClientQGraphicsWidget::pluginParent() const
{
    return view;
}

QStyle* PageClientQGraphicsWidget::style() const
{
    return view->style();
}

QRectF PageClientQGraphicsWidget::windowRect() const
{
    if (!view->scene())
        return QRectF();

    // The sceneRect is a good approximation of the size of the application, independent of the view.
    return view->scene()->sceneRect();
}

} // namespace WebCore
