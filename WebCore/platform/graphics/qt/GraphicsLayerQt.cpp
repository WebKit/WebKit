/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#include "config.h"
#include "GraphicsLayerQt.h"

#include "CurrentTime.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "RefCounted.h"
#include "TranslateTransformOperation.h"
#include "UnitBezier.h"
#include <QtCore/qabstractanimation.h>
#include <QtCore/qdebug.h>
#include <QtCore/qset.h>
#include <QtCore/qtimer.h>
#include <QtGui/qbitmap.h>
#include <QtGui/qcolor.h>
#include <QtGui/qgraphicseffect.h>
#include <QtGui/qgraphicsitem.h>
#include <QtGui/qgraphicsscene.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qstyleoption.h>

namespace WebCore {

class GraphicsLayerQtImpl : public QGraphicsObject {
    Q_OBJECT

public:
    // this set of flags help us defer which properties of the layer have been
    // modified by the compositor, so we can know what to look for in the next flush
    enum ChangeMask {
        NoChanges =                 0,
        ChildrenChange =            (1L << 1),
        MaskLayerChange =           (1L << 2),
        PositionChange =            (1L << 3),
        AnchorPointChange =         (1L << 4),
        SizeChange  =               (1L << 5),
        TransformChange =           (1L << 6),
        ContentChange =             (1L << 7),
        GeometryOrientationChange = (1L << 8),
        ContentsOrientationChange = (1L << 9),
        OpacityChange =             (1L << 10),
        ContentsRectChange =        (1L << 11),
        Preserves3DChange =         (1L << 12),
        MasksToBoundsChange =       (1L << 13),
        DrawsContentChange =        (1L << 14),
        ContentsOpaqueChange =      (1L << 15),
        BackfaceVisibilityChange =  (1L << 16),
        ChildrenTransformChange =   (1L << 17),
        DisplayChange =             (1L << 18),
        BackgroundColorChange =     (1L << 19),
        ParentChange =              (1L << 20),
        DistributesOpacityChange =  (1L << 21)
    };

    // the compositor lets us special-case images and colors, so we try to do so
    enum StaticContentType { HTMLContentType, PixmapContentType, ColorContentType};

    GraphicsLayerQtImpl(GraphicsLayerQt* newLayer);
    virtual ~GraphicsLayerQtImpl();

    // reimps from QGraphicsItem
    virtual QPainterPath opaqueArea() const;
    virtual QRectF boundingRect() const;
    virtual void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);

    // we manage transforms ourselves because transform-origin acts differently in webkit and in Qt
    void setBaseTransform(const QTransform&);
    void drawContents(QPainter*, const QRectF&, bool mask = false);

    // let the compositor-API tell us which properties were changed
    void notifyChange(ChangeMask);

    // called when the compositor is ready for us to show the changes on screen
    // this is called indirectly from ChromeClientQt::setNeedsOneShotDrawingSynchronization
    // (meaning the sync would happen together with the next draw)
    // or ChromeClientQt::scheduleCompositingLayerSync (meaning the sync will happen ASAP)
    void flushChanges(bool recursive = true);

public slots:
    // we need to notify the client (aka the layer compositor) when the animation actually starts
    void notifyAnimationStarted();

public:
    GraphicsLayerQt* m_layer;

    QTransform m_baseTransfom;
    bool m_transformAnimationRunning;
    bool m_opacityAnimationRunning;

    struct ContentData {
        QPixmap pixmap;
        QRegion regionToUpdate;
        bool updateAll;
        QColor contentsBackgroundColor;
        QColor backgroundColor;
        StaticContentType contentType;
        float opacity;
        ContentData()
                : updateAll(false)
                , contentType(HTMLContentType)
                , opacity(1.f)
        {
        }

    };

    ContentData m_pendingContent;
    ContentData m_currentContent;

    int m_changeMask;

    QSizeF m_size;
    QList<QWeakPointer<QAbstractAnimation> > m_animations;
    QTimer m_suspendTimer;

    struct State {
        GraphicsLayer* maskLayer;
        FloatPoint pos;
        FloatPoint3D anchorPoint;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        Color backgroundColor;
        Color currentColor;
        GraphicsLayer::CompositingCoordinatesOrientation geoOrientation;
        GraphicsLayer::CompositingCoordinatesOrientation contentsOrientation;
        float opacity;
        QRect contentsRect;

        bool preserves3D: 1;
        bool masksToBounds: 1;
        bool drawsContent: 1;
        bool contentsOpaque: 1;
        bool backfaceVisibility: 1;
        bool distributeOpacity: 1;
        bool align: 2;
        State(): maskLayer(0), opacity(1), preserves3D(false), masksToBounds(false),
                  drawsContent(false), contentsOpaque(false), backfaceVisibility(false),
                  distributeOpacity(false)
        {
        }
    } m_state;
};

GraphicsLayerQtImpl::GraphicsLayerQtImpl(GraphicsLayerQt* newLayer)
    : QGraphicsObject(0)
    , m_layer(newLayer)
    , m_transformAnimationRunning(false)
    , m_changeMask(NoChanges)
{
    // better to calculate the exposed rect in QGraphicsView than over-render in WebCore
    // FIXME: test different approaches
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);

    // we use graphics-view for compositing, not for interactivity
    setAcceptedMouseButtons(Qt::NoButton);
    setEnabled(false);

    // we'll set the cache when we know what's going on
    setCacheMode(NoCache);
}

GraphicsLayerQtImpl::~GraphicsLayerQtImpl()
{
    // the compositor manages item lifecycle - we don't want the graphics-view
    // system to automatically delete our items

    const QList<QGraphicsItem*> children = childItems();
    for (QList<QGraphicsItem*>::const_iterator it = children.begin(); it != children.end(); ++it) {
        if (QGraphicsItem* item = *it) {
            if (scene())
                scene()->removeItem(item);
            item->setParentItem(0);
        }
    }
    
    // we do, however, own the animations...
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_animations.begin(); it != m_animations.end(); ++it)
        if (QAbstractAnimation* anim = it->data())
            delete anim;
}

void GraphicsLayerQtImpl::setBaseTransform(const QTransform& transform)
{
    if (!m_layer)
        return;
    // webkit has relative-to-size originPoint, graphics-view has a pixel originPoint
    // here we convert
    QPointF originTranslate(
            m_layer->anchorPoint().x() * m_layer->size().width(), m_layer->anchorPoint().y() * m_layer->size().height());

    resetTransform();

    // we have to manage this ourselves because QGraphicsView's transformOrigin is incomplete
    translate(originTranslate.x(), originTranslate.y());
    setTransform(transform, true);
    translate(-originTranslate.x(), -originTranslate.y());
    m_baseTransfom = transform;
}

QPainterPath GraphicsLayerQtImpl::opaqueArea() const
{
    QPainterPath painterPath;
    // we try out best to return the opaque area, maybe it will help graphics-view render less items
    if (m_currentContent.backgroundColor.isValid() && m_currentContent.backgroundColor.alpha() == 0xff)
        painterPath.addRect(boundingRect());
    else {
        if (m_state.contentsOpaque
            || (m_currentContent.contentType == ColorContentType && m_currentContent.contentsBackgroundColor.alpha() == 0xff)
            || (m_currentContent.contentType == PixmapContentType && !m_currentContent.pixmap.hasAlpha())) {

            painterPath.addRect(m_state.contentsRect);
        }
    }
    return painterPath;
}

QRectF GraphicsLayerQtImpl::boundingRect() const
{
    return QRectF(QPointF(0, 0), QSizeF(m_size));
}

void GraphicsLayerQtImpl::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if (m_state.maskLayer && m_state.maskLayer->platformLayer()) {
        // FIXME: see if this is better done somewhere else
        GraphicsLayerQtImpl* otherMask = static_cast<GraphicsLayerQtImpl*>(m_state.maskLayer->platformLayer());
        otherMask->flushChanges(true);
        
        // CSS3 mask and QGraphicsOpacityEffect are the same thing! we just need to convert...
        // The conversion is as fast as we can make it - we render the layer once and send it to the QGraphicsOpacityEffect
        if (!graphicsEffect()) {
            QPixmap mask(QSize(m_state.maskLayer->size().width(), m_state.maskLayer->size().height()));
            mask.fill(Qt::transparent);
            {
                QPainter p(&mask);
                p.setRenderHints(painter->renderHints(), true);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                static_cast<GraphicsLayerQtImpl*>(m_state.maskLayer->platformLayer())->drawContents(&p, option->exposedRect, true);
            }
            QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(this);
            opacityEffect->setOpacity(1);
            opacityEffect->setOpacityMask(QBrush(mask));
            setGraphicsEffect(opacityEffect);
        }
    }
    drawContents(painter, option->exposedRect);
}

void GraphicsLayerQtImpl::drawContents(QPainter* painter, const QRectF& r, bool mask)
{
    QRect rect = r.toAlignedRect();
    
    if (m_currentContent.contentType != HTMLContentType && !m_state.contentsRect.isEmpty())
        rect = rect.intersected(m_state.contentsRect);

    if (m_currentContent.backgroundColor.isValid())
        painter->fillRect(r, QColor(m_currentContent.backgroundColor));

    if (!rect.isEmpty()) {
        switch (m_currentContent.contentType) {
        case PixmapContentType:
            // we have to scale the image to the contentsRect
            // FIXME: a better way would probably be drawPixmap with a src/target rect
            painter->drawPixmap(rect.topLeft(), m_currentContent.pixmap.scaled(m_state.contentsRect.size()), r);
            break;
        case ColorContentType:
            painter->fillRect(rect, m_currentContent.contentsBackgroundColor);
            break;
        default:
            if (m_state.drawsContent) {
                // this is the "expensive" bit. we try to minimize calls to this
                // neck of the woods by proper caching
                GraphicsContext gc(painter);
                m_layer->paintGraphicsLayerContents(gc, rect);
            }
            break;
        }
    }
}

void GraphicsLayerQtImpl::notifyChange(ChangeMask changeMask)
{
    if (!this)
        return;

    m_changeMask |= changeMask;

    if (m_layer->client())
        m_layer->client()->notifySyncRequired(m_layer);
}

void GraphicsLayerQtImpl::flushChanges(bool recursive)
{
    // this is the bulk of the work. understanding what the compositor is trying to achieve,
    // what graphics-view can do, and trying to find a sane common-grounds
    if (!m_layer || m_changeMask == NoChanges)
        goto afterLayerChanges;

    if (m_currentContent.contentType == HTMLContentType && (m_changeMask & ParentChange)) {
        // the WebCore compositor manages item ownership. We have to make sure
        // graphics-view doesn't try to snatch that ownership...
        if (!m_layer->parent() && !parentItem())
            setParentItem(0);
        else if (m_layer && m_layer->parent() && m_layer->parent()->nativeLayer() != parentItem())
            setParentItem(m_layer->parent()->nativeLayer());
    }

    if (m_changeMask & ChildrenChange) {
        // we basically do an XOR operation on the list of current children
        // and the list of wanted children, and remove/add
        QSet<QGraphicsItem*> newChildren;
        const Vector<GraphicsLayer*> newChildrenVector = (m_layer->children());
        newChildren.reserve(newChildrenVector.size());

        for (size_t i = 0; i < newChildrenVector.size(); ++i)
            newChildren.insert(newChildrenVector[i]->platformLayer());

        const QSet<QGraphicsItem*> currentChildren = childItems().toSet();
        const QSet<QGraphicsItem*> childrenToAdd = newChildren - currentChildren;
        const QSet<QGraphicsItem*> childrenToRemove = currentChildren - newChildren;
        for (QSet<QGraphicsItem*>::const_iterator it = childrenToAdd.begin(); it != childrenToAdd.end(); ++it) {
             if (QGraphicsItem* w = *it)
                w->setParentItem(this);
        }
        for (QSet<QGraphicsItem*>::const_iterator it = childrenToRemove.begin(); it != childrenToRemove.end(); ++it) {
             if (QGraphicsItem* w = *it)
                w->setParentItem(0);
        }

        // children are ordered by z-value, let graphics-view know.
        for (size_t i = 0; i < newChildrenVector.size(); ++i)
            if (newChildrenVector[i]->platformLayer())
                newChildrenVector[i]->platformLayer()->setZValue(i);
    }

    if (m_changeMask & MaskLayerChange) {
        // we can't paint here, because we don't know if the mask layer
        // itself is ready... we'll have to wait till this layer tries to paint
        setGraphicsEffect(0);
        if (m_layer->maskLayer())
            setFlag(ItemClipsChildrenToShape, true);
        else
            setFlag(ItemClipsChildrenToShape, m_layer->masksToBounds());
        update();
    }

    if ((m_changeMask & PositionChange) && (m_layer->position() != m_state.pos))
        setPos(m_layer->position().x(), m_layer->position().y());

    if (m_changeMask & SizeChange) {
        if (m_layer->size() != m_state.size) {
            prepareGeometryChange();
            m_size = QSizeF(m_layer->size().width(), m_layer->size().height());
        }
    }

    if (m_changeMask & (TransformChange | AnchorPointChange | SizeChange)) {
        // since we convert a percentage-based origin-point to a pixel-based one,
        // the anchor-point, transform and size from WebCore all affect the one
        // that we give Qt
        if (m_state.transform != m_layer->transform() || m_state.anchorPoint != m_layer->anchorPoint() || m_state.size != m_layer->size())
            setBaseTransform(QTransform(m_layer->transform()));
    }

    if (m_changeMask & (ContentChange | DrawsContentChange)) {
        switch (m_pendingContent.contentType) {
        case PixmapContentType:
            // we need cache even for images, because they need to be resized
            // to the contents rect. maybe this can be optimized though
            setCacheMode(m_transformAnimationRunning ? ItemCoordinateCache : DeviceCoordinateCache);
            update();
            setFlag(ItemHasNoContents, false);
            break;

        case ColorContentType:
            // no point in caching a solid-color rectangle
            setCacheMode(QGraphicsItem::NoCache);
            if (m_pendingContent.contentType != m_currentContent.contentType || m_pendingContent.contentsBackgroundColor != m_currentContent.contentsBackgroundColor)
                update();
            m_state.drawsContent = false;
            setFlag(ItemHasNoContents, false);
            break;

        case HTMLContentType:
            if (m_pendingContent.contentType != m_currentContent.contentType)
                update();
            if (!m_state.drawsContent && m_layer->drawsContent())
                update();
            if (m_layer->drawsContent())
                setCacheMode(m_transformAnimationRunning ? ItemCoordinateCache : DeviceCoordinateCache);
            else
                setCacheMode(NoCache);

            setFlag(ItemHasNoContents, !m_layer->drawsContent());
            break;
        }
    }

    if ((m_changeMask & OpacityChange) && m_state.opacity != m_layer->opacity())
        setOpacity(m_layer->opacity());

    if (m_changeMask & ContentsRectChange) {
        const QRect rect(m_layer->contentsRect());
        if (m_state.contentsRect != rect) {
            m_state.contentsRect = rect;
            update();
        }
    }

    if ((m_changeMask & MasksToBoundsChange)
        && m_state.masksToBounds != m_layer->masksToBounds()) {

        setFlag(QGraphicsItem::ItemClipsToShape, m_layer->masksToBounds());
        setFlag(QGraphicsItem::ItemClipsChildrenToShape, m_layer->masksToBounds());
    }

    if ((m_changeMask & ContentsOpaqueChange) && m_state.contentsOpaque != m_layer->contentsOpaque())
        prepareGeometryChange();

    if (m_changeMask & DisplayChange)
        update(m_pendingContent.regionToUpdate.boundingRect());

    if ((m_changeMask & BackgroundColorChange) && (m_pendingContent.backgroundColor != m_currentContent.backgroundColor))
        update();

    // FIXME: the following flags are currently not handled, as they don't have a clear test or are in low priority
    // GeometryOrientationChange, ContentsOrientationChange, BackfaceVisibilityChange, ChildrenTransformChange

    m_state.maskLayer = m_layer->maskLayer();
    m_state.pos = m_layer->position();
    m_state.anchorPoint = m_layer->anchorPoint();
    m_state.size = m_layer->size();
    m_state.transform = m_layer->transform();
    m_state.geoOrientation = m_layer->geometryOrientation();
    m_state.contentsOrientation =m_layer->contentsOrientation();
    m_state.opacity = m_layer->opacity();
    m_state.contentsRect = m_layer->contentsRect();
    m_state.preserves3D = m_layer->preserves3D();
    m_state.masksToBounds = m_layer->masksToBounds();
    m_state.drawsContent = m_layer->drawsContent();
    m_state.contentsOpaque = m_layer->contentsOpaque();
    m_state.backfaceVisibility = m_layer->backfaceVisibility();
    m_currentContent.pixmap = m_pendingContent.pixmap;
    m_currentContent.contentType = m_pendingContent.contentType;
    m_currentContent.backgroundColor = m_pendingContent.backgroundColor;
    m_currentContent.regionToUpdate |= m_pendingContent.regionToUpdate;
    m_currentContent.contentsBackgroundColor = m_pendingContent.contentsBackgroundColor;
    m_pendingContent.regionToUpdate = QRegion();
    m_changeMask = NoChanges;


afterLayerChanges:
    if (!recursive)
        return;    

    const QList<QGraphicsItem*> children = childItems();

    for (QList<QGraphicsItem*>::const_iterator it = children.begin(); it != children.end(); ++it) {
        if (QGraphicsItem* item = *it)
            if (GraphicsLayerQtImpl* layer = qobject_cast<GraphicsLayerQtImpl*>(item->toGraphicsObject()))
                layer->flushChanges(true);
    }
}

void GraphicsLayerQtImpl::notifyAnimationStarted()
{
    // WebCore notifies javascript when the animation starts
    // here we're letting it know
    m_layer->client()->notifyAnimationStarted(m_layer, WTF::currentTime());
}

GraphicsLayerQt::GraphicsLayerQt(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_impl(PassOwnPtr<GraphicsLayerQtImpl>(new GraphicsLayerQtImpl(this)))
{
}

GraphicsLayerQt::~GraphicsLayerQt()
{
}

// this is the hook for WebCore compositor to know that Qt implements compositing with GraphicsLayerQt
PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerQt(client);
}

// reimp from GraphicsLayer.h: Qt is top-down
GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayer::compositingCoordinatesOrientation()
{
    return CompositingCoordinatesTopDown;
}

// reimp from GraphicsLayer.h: we'll need to update the whole display, and we can't count on the current size because it might change
void GraphicsLayerQt::setNeedsDisplay()
{
    m_impl->m_pendingContent.regionToUpdate = QRegion(QRect(QPoint(0, 0), QSize(size().width(), size().height())));
    m_impl->notifyChange(GraphicsLayerQtImpl::DisplayChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setNeedsDisplayInRect(const FloatRect& r)
{
    m_impl->m_pendingContent.regionToUpdate|= QRectF(r).toAlignedRect();
    m_impl->notifyChange(GraphicsLayerQtImpl::DisplayChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setName(const String& name)
{
    m_impl->setObjectName(name);
    GraphicsLayer::setName(name);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setParent(GraphicsLayer* layer)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ParentChange);
    GraphicsLayer::setParent(layer);
}

// reimp from GraphicsLayer.h
bool GraphicsLayerQt::setChildren(const Vector<GraphicsLayer*>& children)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
    return GraphicsLayer::setChildren(children);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChild(GraphicsLayer* layer)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
    GraphicsLayer::addChild(layer);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
     GraphicsLayer::addChildAbove(layer, sibling);
     m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{

    GraphicsLayer::addChildBelow(layer, sibling);
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
}

// reimp from GraphicsLayer.h
bool GraphicsLayerQt::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
        return true;
    }

    return false;
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::removeFromParent()
{
    if (parent())
        m_impl->notifyChange(GraphicsLayerQtImpl::ParentChange);
    GraphicsLayer::removeFromParent();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setMaskLayer(GraphicsLayer* layer)
{
    GraphicsLayer::setMaskLayer(layer);
    m_impl->notifyChange(GraphicsLayerQtImpl::MaskLayerChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setPosition(const FloatPoint& p)
{
    if (position() != p)
       m_impl->notifyChange(GraphicsLayerQtImpl::PositionChange);
    GraphicsLayer::setPosition(p);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setAnchorPoint(const FloatPoint3D& p)
{
    if (anchorPoint() != p)
        m_impl->notifyChange(GraphicsLayerQtImpl::AnchorPointChange);
    GraphicsLayer::setAnchorPoint(p);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setSize(const FloatSize& size)
{
    if (this->size() != size)
        m_impl->notifyChange(GraphicsLayerQtImpl::SizeChange);
    GraphicsLayer::setSize(size);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setTransform(const TransformationMatrix& t)
{
    if (!m_impl->m_transformAnimationRunning && transform() != t)
       m_impl->notifyChange(GraphicsLayerQtImpl::TransformChange);
    GraphicsLayer::setTransform(t);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setChildrenTransform(const TransformationMatrix& t)
{
    GraphicsLayer::setChildrenTransform(t);
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenTransformChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setPreserves3D(bool b)
{
    if (b != preserves3D());
       m_impl->notifyChange(GraphicsLayerQtImpl::Preserves3DChange);
    GraphicsLayer::setPreserves3D(b);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setMasksToBounds(bool b)
{
    GraphicsLayer::setMasksToBounds(b);
    m_impl->notifyChange(GraphicsLayerQtImpl::MasksToBoundsChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setDrawsContent(bool b)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::DrawsContentChange);
    GraphicsLayer::setDrawsContent(b);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setBackgroundColor(const Color& c)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::BackgroundColorChange);
    m_impl->m_pendingContent.backgroundColor = c;
    GraphicsLayer::setBackgroundColor(c);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::clearBackgroundColor()
{
    m_impl->m_pendingContent.backgroundColor = QColor();
    m_impl->notifyChange(GraphicsLayerQtImpl::BackgroundColorChange);
    GraphicsLayer::clearBackgroundColor();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsOpaque(bool b)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentsOpaqueChange);
    GraphicsLayer::setContentsOpaque(b);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setBackfaceVisibility(bool b)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::BackfaceVisibilityChange);
    GraphicsLayer::setBackfaceVisibility(b);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setOpacity(float o)
{
    if (!m_impl->m_opacityAnimationRunning && opacity() != o)
       m_impl->notifyChange(GraphicsLayerQtImpl::OpacityChange);
    GraphicsLayer::setOpacity(o);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsRect(const IntRect& r)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentsRectChange);
    GraphicsLayer::setContentsRect(r);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsToImage(Image* image)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentChange);
    m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::HTMLContentType;
    GraphicsLayer::setContentsToImage(image);
    if (image) {
        QPixmap* pxm = image->nativeImageForCurrentFrame();
        if (pxm) {
            m_impl->m_pendingContent.pixmap = *pxm;
            m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::PixmapContentType;
            return;
        }        
    }
    m_impl->m_pendingContent.pixmap = QPixmap();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsBackgroundColor(const Color& color)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentChange);
    m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::ColorContentType;
    m_impl->m_pendingContent.contentsBackgroundColor = QColor(color);
    GraphicsLayer::setContentsBackgroundColor(color);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setGeometryOrientation(CompositingCoordinatesOrientation orientation)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::GeometryOrientationChange);
    GraphicsLayer::setGeometryOrientation(orientation);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsOrientation(CompositingCoordinatesOrientation orientation)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentsOrientationChange);
    GraphicsLayer::setContentsOrientation(orientation);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::distributeOpacity(float o)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::OpacityChange);
    m_impl->m_state.distributeOpacity = true;
}

// reimp from GraphicsLayer.h
float GraphicsLayerQt::accumulatedOpacity() const
{
    return m_impl->effectiveOpacity();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::syncCompositingState()
{
    m_impl->flushChanges();
    GraphicsLayer::syncCompositingState();
}

// reimp from GraphicsLayer.h
NativeLayer GraphicsLayerQt::nativeLayer() const
{
    return m_impl.get();
}

// reimp from GraphicsLayer.h
PlatformLayer* GraphicsLayerQt::platformLayer() const
{
    return m_impl.get();
}

// now we start dealing with WebCore animations translated to Qt animations

template <typename T>
struct KeyframeValueQt {
    TimingFunction timingFunction;
    T value;
};

// we copy this from the AnimationBase.cpp
static inline double solveEpsilon(double duration)
{
    return 1.0 / (200.0 * duration);
}

static inline double solveCubicBezierFunction(qreal p1x, qreal p1y, qreal p2x, qreal p2y, double t, double duration)
{
    UnitBezier bezier(p1x, p1y, p2x, p2y);
    return bezier.solve(t, solveEpsilon(duration));
}

// we want the timing function to be as close as possible to what the web-developer intended, so we're using the same function used by WebCore when compositing is disabled
// Using easing-curves would probably work for some of the cases, but wouldn't really buy us anything as we'd have to convert the bezier function back to an easing curve
static inline qreal applyTimingFunction(const TimingFunction& timingFunction, qreal progress, int duration)
{
    if (timingFunction.type() == LinearTimingFunction)
        return progress;
    if (timingFunction.type() == CubicBezierTimingFunction) {
        return solveCubicBezierFunction(timingFunction.x1(),
                                        timingFunction.y1(),
                                        timingFunction.x2(),
                                        timingFunction.y2(),
                                        double(progress), double(duration) / 1000);
    }
    return progress;
}

// helper functions to safely get a value out of WebCore's AnimationValue*
static void webkitAnimationToQtAnimationValue(const AnimationValue* animationValue, TransformOperations& transformOperations)
{
    transformOperations = TransformOperations();
    if (!animationValue)
        return;

    const TransformOperations* ops = static_cast<const TransformAnimationValue*>(animationValue)->value();

    if (ops)
        transformOperations = *ops;
}

static void webkitAnimationToQtAnimationValue(const AnimationValue* animationValue, qreal& realValue)
{
    realValue = animationValue ? static_cast<const FloatAnimationValue*>(animationValue)->value() : 0;
}

// we put a bit of the functionality in a base class to allow casting and to save some code size
class AnimationQtBase : public QAbstractAnimation {
public:
    AnimationQtBase(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
        : QAbstractAnimation(0)
        , m_layer(layer)
        , m_boxSize(boxSize)
        , m_duration(anim->duration() * 1000)
        , m_isAlternate(anim->direction() == Animation::AnimationDirectionAlternate)
        , m_webkitPropertyID(values.property())
        , m_keyframesName(name)
    {
    }

    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        QAbstractAnimation::updateState(newState, oldState);

        // for some reason I have do this asynchronously - or the animation won't work
        if (newState == Running && oldState == Stopped)
            QTimer::singleShot(0, m_layer.data(), SLOT(notifyAnimationStarted()));
    }

    virtual int duration() const { return m_duration; }

    QWeakPointer<GraphicsLayerQtImpl> m_layer;
    IntSize m_boxSize;
    int m_duration;
    bool m_isAlternate;
    AnimatedPropertyID m_webkitPropertyID;
    QString m_keyframesName;
};

// we'd rather have a templatized QAbstractAnimation than QPropertyAnimation / QVariantAnimation;
// Since we know the types that we're dealing with, the QObject/QProperty/QVariant abstraction
// buys us very little in this case, for too much overhead
template <typename T>
class AnimationQt : public AnimationQtBase {

public:
    AnimationQt(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
        :AnimationQtBase(layer, values, boxSize, anim, name)
    {
        // copying those WebCore structures is not trivial, we have to do it like this
        for (size_t i = 0; i < values.size(); ++i) {
            const AnimationValue* animationValue = values.at(i);
            KeyframeValueQt<T> keyframeValue;
            if (animationValue->timingFunction())
                keyframeValue.timingFunction = *animationValue->timingFunction();
            webkitAnimationToQtAnimationValue(animationValue, keyframeValue.value);
            m_keyframeValues[animationValue->keyTime()] = keyframeValue;
        }
    }

protected:

    // this is the part that differs between animated properties
    virtual void applyFrame(const T& fromValue, const T& toValue, qreal progress) = 0;

    virtual void updateCurrentTime(int currentTime)
    {
        if (!m_layer)
            return;

        qreal progress = qreal(currentLoopTime()) / duration();

        if (m_isAlternate && currentLoop()%2)
            progress = 1-progress;

        if (m_keyframeValues.isEmpty())
            return;

        // we find the current from-to keyframes in our little map
        typename QMap<qreal, KeyframeValueQt<T> >::iterator it = m_keyframeValues.find(progress);

        // we didn't find an exact match, we try the closest match (lower bound)
        if (it == m_keyframeValues.end())
            it = m_keyframeValues.lowerBound(progress)-1;

        // we didn't find any match - we use the first keyframe
        if (it == m_keyframeValues.end())
            it = m_keyframeValues.begin();

        typename QMap<qreal, KeyframeValueQt<T> >::iterator it2 = it+1;
        if (it2 == m_keyframeValues.end())
            it2 = m_keyframeValues.begin();
        const KeyframeValueQt<T>& fromKeyframe = it.value();
        const KeyframeValueQt<T>& toKeyframe = it2.value();

        const TimingFunction& timingFunc = fromKeyframe.timingFunction;
        const T& fromValue = fromKeyframe.value;
        const T& toValue = toKeyframe.value;

        // now we have a source keyframe, origin keyframe and a timing function
        // we can now process the progress and apply the frame
        qreal normalizedProgress = (it.key() == it2.key()) ? 0 : (progress - it.key()) / (it2.key() - it.key());
        normalizedProgress = applyTimingFunction(timingFunc, normalizedProgress, duration() / 1000);
        applyFrame(fromValue, toValue, normalizedProgress);
    }

    QMap<qreal, KeyframeValueQt<T> > m_keyframeValues;
};

class TransformAnimationQt : public AnimationQt<TransformOperations> {
public:
    TransformAnimationQt(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
                : AnimationQt<TransformOperations>(layer, values, boxSize, anim, name)
    {
    }

    ~TransformAnimationQt()
    {
        // this came up during the compositing/animation LayoutTests
        // when the animation dies, the transform has to go back to default
        if (m_layer)
            m_layer.data()->setBaseTransform(QTransform(m_layer.data()->m_layer->transform()));
    }

    // the idea is that we let WebCore manage the transform-operations
    // and Qt just manages the animation heartbeat and the bottom-line QTransform
    // we get the performance not by using QTransform instead of TransformationMatrix, but by proper caching of
    // items that are expensive for WebCore to render. We want the rest to be as close to WebCore's idea as possible.
    virtual void applyFrame(const TransformOperations& sourceOperations, const TransformOperations& targetOperations, qreal progress)
    {
        TransformationMatrix transformMatrix;

        // this looks simple but is really tricky to get right. Use caution.
        for (size_t i = 0; i < targetOperations.size(); ++i)
            targetOperations.operations()[i]->blend(sourceOperations.at(i), progress)->apply(transformMatrix, m_boxSize);

        m_layer.data()->setBaseTransform(QTransform(transformMatrix));
    }

    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        AnimationQtBase::updateState(newState, oldState);
        if (!m_layer)
            return;
        m_layer.data()->flushChanges(true);

        // to increase FPS, we use a less accurate caching mechanism while animation is going on
        // this is a UX choice that should probably be customizable
        if (newState == QAbstractAnimation::Running) {
            m_layer.data()->m_transformAnimationRunning = true;
            if (m_layer.data()->cacheMode() == QGraphicsItem::DeviceCoordinateCache)
                m_layer.data()->setCacheMode(QGraphicsItem::ItemCoordinateCache);
        } else {
            m_layer.data()->m_transformAnimationRunning = false;
            if (m_layer.data()->cacheMode() == QGraphicsItem::ItemCoordinateCache)
                m_layer.data()->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        }
    }
};

class OpacityAnimationQt : public AnimationQt<qreal> {
public:
    OpacityAnimationQt(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
         : AnimationQt<qreal>(layer, values, boxSize, anim, name)
    {
    }

    virtual void applyFrame(const qreal& fromValue, const qreal& toValue, qreal progress)
    {
        m_layer.data()->setOpacity(qMin<qreal>(qMax<qreal>(fromValue + (toValue-fromValue)*progress, 0), 1));
    }

    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        QAbstractAnimation::updateState(newState, oldState);
        if (m_layer)
            m_layer.data()->m_opacityAnimationRunning = (newState == QAbstractAnimation::Running);
    }
};

bool GraphicsLayerQt::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    if (!anim->duration() || !anim->iterationCount())
        return false;

    QAbstractAnimation* newAnim;

    switch (values.property()) {
    case AnimatedPropertyOpacity:
        newAnim = new OpacityAnimationQt(m_impl.get(), values, boxSize, anim, keyframesName);
        break;
    case AnimatedPropertyWebkitTransform:
        newAnim = new TransformAnimationQt(m_impl.get(), values, boxSize, anim, keyframesName);
        break;
    default:
        return false;
    }

    // we make sure WebCore::Animation and QAnimation are on the same terms
    newAnim->setLoopCount(anim->iterationCount());
    m_impl->m_animations.append(QWeakPointer<QAbstractAnimation>(newAnim));
    QObject::connect(&m_impl->m_suspendTimer, SIGNAL(timeout()), newAnim, SLOT(resume()));
    timeOffset += anim->delay();

    // flush now or flicker...
    m_impl->flushChanges(false);

    if (timeOffset)
        QTimer::singleShot(timeOffset * 1000, newAnim, SLOT(start()));
    else
        newAnim->start();

    QObject::connect(newAnim, SIGNAL(finished()), newAnim, SLOT(deleteLater()));

    return true;
}

void GraphicsLayerQt::removeAnimationsForProperty(AnimatedPropertyID id)
{
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (*it) {
            AnimationQtBase* anim = static_cast<AnimationQtBase*>(it->data());
            if (anim && anim->m_webkitPropertyID == id) {
                delete anim;
                it = m_impl->m_animations.erase(it);
                --it;
            }
        }
    }
}

void GraphicsLayerQt::removeAnimationsForKeyframes(const String& name)
{
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (*it) {
            AnimationQtBase* anim = static_cast<AnimationQtBase*>((*it).data());
            if (anim && anim->m_keyframesName == QString(name)) {
                (*it).data()->deleteLater();
                it = m_impl->m_animations.erase(it);
                --it;
            }
        }
    }
}

void GraphicsLayerQt::pauseAnimation(const String& name, double timeOffset)
{
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (*it) {
            AnimationQtBase* anim = static_cast<AnimationQtBase*>((*it).data());
            if (anim && anim->m_keyframesName == QString(name))
                QTimer::singleShot(timeOffset * 1000, anim, SLOT(pause()));
        }
    }
}

void GraphicsLayerQt::suspendAnimations(double time)
{
    if (m_impl->m_suspendTimer.isActive()) {
        m_impl->m_suspendTimer.stop();
        m_impl->m_suspendTimer.start(time * 1000);
    } else {
        for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
            QAbstractAnimation* anim = it->data();
            if (anim)
                anim->pause();
        }
    }
}

void GraphicsLayerQt::resumeAnimations()
{
    if (m_impl->m_suspendTimer.isActive()) {
        m_impl->m_suspendTimer.stop();
        for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
            QAbstractAnimation* anim = (*it).data();
            if (anim)
                anim->resume();
        }
    }
}

}

#include <GraphicsLayerQt.moc>
