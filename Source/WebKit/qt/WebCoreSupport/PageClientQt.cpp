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

#include <QGraphicsScene>
#include <QGraphicsView>

#if defined(Q_WS_X11)
#include <QX11Info>
#endif

#ifdef QT_OPENGL_LIB
#include <QGLWidget>
#endif

#if USE(3D_GRAPHICS)
#if HAVE(QT5)
#include <QWindow>
#endif

static void createPlatformGraphicsContext3DFromWidget(QWidget* widget, PlatformGraphicsContext3D* context,
                                                      PlatformGraphicsSurface3D* surface)
{
#ifdef QT_OPENGL_LIB
    *context = 0;
    *surface = 0;
    QAbstractScrollArea* scrollArea = qobject_cast<QAbstractScrollArea*>(widget);
    if (!scrollArea)
        return;

    QGLWidget* glViewport = qobject_cast<QGLWidget*>(scrollArea->viewport());
    if (!glViewport)
        return;
    QGLWidget* glWidget = new QGLWidget(0, glViewport);
    if (glWidget->isValid()) {
        // Geometry can be set to zero because m_glWidget is used only for its QGLContext.
        glWidget->setGeometry(0, 0, 0, 0);
#if HAVE(QT5)
        *surface = glWidget->windowHandle();
        *context = glWidget->context()->contextHandle();
#else
        *surface = glWidget;
        *context = const_cast<QGLContext*>(glWidget->context());
#endif
    } else {
        delete glWidget;
        glWidget = 0;
    }
#endif
}
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "TextureMapper.h"
#include "texmap/TextureMapperLayer.h"
#endif

#if HAVE(QT5)
QWindow* QWebPageClient::ownerWindow() const
{
    QWidget* widget = ownerWidget();
    if (!widget)
        return 0;
    if (QWindow *window = widget->windowHandle())
        return window;
    if (const QWidget *nativeParent = widget->nativeParentWidget())
        return nativeParent->windowHandle();
    return 0;
}
#endif

namespace WebCore {

#if USE(ACCELERATED_COMPOSITING)
TextureMapperLayerClientQt::TextureMapperLayerClientQt(QWebFrame* frame, GraphicsLayer* layer)
    : m_frame(frame)
    , m_rootGraphicsLayer(GraphicsLayer::create(0))
{
    m_frame->d->rootTextureMapperLayer = rootLayer();
    m_rootGraphicsLayer->addChild(layer);
    m_rootGraphicsLayer->setDrawsContent(false);
    m_rootGraphicsLayer->setMasksToBounds(false);
    m_rootGraphicsLayer->setSize(IntSize(1, 1));
}

void TextureMapperLayerClientQt::setTextureMapper(const PassOwnPtr<TextureMapper>& textureMapper)
{
    m_frame->d->textureMapper = textureMapper;
    m_frame->d->rootTextureMapperLayer->setTextureMapper(m_frame->d->textureMapper.get());
}

TextureMapperLayerClientQt::~TextureMapperLayerClientQt()
{
    m_frame->d->rootTextureMapperLayer = 0;
}

void TextureMapperLayerClientQt::syncRootLayer()
{
    m_rootGraphicsLayer->syncCompositingStateForThisLayerOnly();
}

TextureMapperLayer* TextureMapperLayerClientQt::rootLayer()
{
    return toTextureMapperLayer(m_rootGraphicsLayer.get());
}


void PageClientQWidget::setRootGraphicsLayer(GraphicsLayer* layer)
{
    if (layer) {
        TextureMapperLayerClient = adoptPtr(new TextureMapperLayerClientQt(page->mainFrame(), layer));
        TextureMapperLayerClient->setTextureMapper(TextureMapper::create());
        TextureMapperLayerClient->syncRootLayer();
        return;
    }
    TextureMapperLayerClient.clear();
}

void PageClientQWidget::markForSync(bool scheduleSync)
{
    if (syncTimer.isActive())
        return;
    syncTimer.startOneShot(0);
}

void PageClientQWidget::syncLayers(Timer<PageClientQWidget>*)
{
    if (TextureMapperLayerClient)
        TextureMapperLayerClient->syncRootLayer();
    QWebFramePrivate::core(page->mainFrame())->view()->syncCompositingStateIncludingSubframes();
    if (!TextureMapperLayerClient)
        return;
    if (TextureMapperLayerClient->rootLayer()->descendantsOrSelfHaveRunningAnimations() && !syncTimer.isActive())
        syncTimer.startOneShot(1.0 / 60.0);
    update(view->rect());
}
#endif

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

void PageClientQWidget::setInputMethodHints(Qt::InputMethodHints hints)
{
    view->setInputMethodHints(hints);
}

PageClientQWidget::~PageClientQWidget()
{
}

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

void PageClientQWidget::setWidgetVisible(Widget* widget, bool visible)
{
    QWidget* qtWidget = qobject_cast<QWidget*>(widget->platformWidget());
    if (!qtWidget)
        return;
    qtWidget->setVisible(visible);
}

#if USE(3D_GRAPHICS)
void PageClientQWidget::createPlatformGraphicsContext3D(PlatformGraphicsContext3D* context,
                                                        PlatformGraphicsSurface3D* surface)
{
    createPlatformGraphicsContext3DFromWidget(view, context, surface);
}
#endif

#if !defined(QT_NO_GRAPHICSVIEW)
PageClientQGraphicsWidget::~PageClientQGraphicsWidget()
{
    delete overlay;
}

void PageClientQGraphicsWidget::scroll(int dx, int dy, const QRect& rectToScroll)
{
    view->scroll(qreal(dx), qreal(dy), rectToScroll);
}

void PageClientQGraphicsWidget::update(const QRect& dirtyRect)
{
    view->update(dirtyRect);

    if (overlay)
        overlay->update(QRectF(dirtyRect));
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientQGraphicsWidget::syncLayers()
{
    if (TextureMapperLayerClient)
        TextureMapperLayerClient->syncRootLayer();

    QWebFramePrivate::core(page->mainFrame())->view()->syncCompositingStateIncludingSubframes();

    if (!TextureMapperLayerClient)
        return;

    if (TextureMapperLayerClient->rootLayer()->descendantsOrSelfHaveRunningAnimations() && !syncTimer.isActive())
        syncTimer.startOneShot(1.0 / 60.0);
    update(view->boundingRect().toAlignedRect());
}

void PageClientQGraphicsWidget::setRootGraphicsLayer(GraphicsLayer* layer)
{
    if (layer) {
        TextureMapperLayerClient = adoptPtr(new TextureMapperLayerClientQt(page->mainFrame(), layer));
#if USE(TEXTURE_MAPPER_GL)
        QGraphicsView* graphicsView = view->scene()->views()[0];
        if (graphicsView && graphicsView->viewport() && graphicsView->viewport()->inherits("QGLWidget")) {
            TextureMapperLayerClient->setTextureMapper(TextureMapper::create(TextureMapper::OpenGLMode));
            return;
        }
#endif
        TextureMapperLayerClient->setTextureMapper(TextureMapper::create());
        return;
    }
    TextureMapperLayerClient.clear();
}

void PageClientQGraphicsWidget::markForSync(bool scheduleSync)
{
    if (syncTimer.isActive())
        return;
    syncTimer.startOneShot(0);
}

#endif

#if USE(TILED_BACKING_STORE)
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
    view->setFlag(QGraphicsItem::ItemAcceptsInputMethod, enable);
}

bool PageClientQGraphicsWidget::inputMethodEnabled() const
{
    return view->flags() & QGraphicsItem::ItemAcceptsInputMethod;
}

void PageClientQGraphicsWidget::setInputMethodHints(Qt::InputMethodHints hints)
{
    view->setInputMethodHints(hints);
}

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

#if USE(TILED_BACKING_STORE)
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

void PageClientQGraphicsWidget::setWidgetVisible(Widget*, bool)
{
    // Doesn't make sense, does it?
}

QRectF PageClientQGraphicsWidget::windowRect() const
{
    if (!view->scene())
        return QRectF();

    // The sceneRect is a good approximation of the size of the application, independent of the view.
    return view->scene()->sceneRect();
}
#endif // QT_NO_GRAPHICSVIEW

#if USE(3D_GRAPHICS)
void PageClientQGraphicsWidget::createPlatformGraphicsContext3D(PlatformGraphicsContext3D* context,
                                                                PlatformGraphicsSurface3D* surface)
{
    createPlatformGraphicsContext3DFromWidget(ownerWidget(), context, surface);
}
#endif

} // namespace WebCore
