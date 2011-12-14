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
#include "qquickwebpage_p.h"

#include "LayerTreeHostProxy.h"
#include "QtWebPageEventHandler.h"
#include "QtWebPageProxy.h"
#include "TransformationMatrix.h"
#include "qquickwebpage_p_p.h"
#include <QtQuick/QQuickCanvas>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGMaterial>

QQuickWebPage::QQuickWebPage(QQuickItem* parent)
    : QQuickItem(parent)
    , d(new QQuickWebPagePrivate(this))
{
    setFlag(ItemHasContents);

    // We do the transform from the top left so the viewport can assume the position 0, 0
    // is always where rendering starts.
    setTransformOrigin(TopLeft);
}

QQuickWebPage::~QQuickWebPage()
{
    delete d;
}

QtSGUpdateQueue *QQuickWebPage::sceneGraphUpdateQueue() const
{
    return &d->sgUpdateQueue;
}

void QQuickWebPage::keyPressEvent(QKeyEvent* event)
{
    this->event(event);
}

void QQuickWebPage::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QQuickWebPage::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QQuickWebPage::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QQuickWebPage::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QQuickWebPage::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus();
    this->event(event);
}

void QQuickWebPage::mouseMoveEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebPage::mouseReleaseEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebPage::mouseDoubleClickEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebPage::wheelEvent(QWheelEvent* event)
{
    this->event(event);
}

void QQuickWebPage::hoverEnterEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebPage::hoverMoveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebPage::hoverLeaveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dragMoveEvent(QDragMoveEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dragEnterEvent(QDragEnterEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dragLeaveEvent(QDragLeaveEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dropEvent(QDropEvent* event)
{
    this->event(event);
}

void QQuickWebPage::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->setDrawingAreaSize(newGeometry.size().toSize());
}

bool QQuickWebPage::event(QEvent* ev)
{
    if (d->eventHandler.data()->handleEvent(ev))
        return true;
    if (ev->type() == QEvent::InputMethod)
        return false; // This is necessary to avoid an endless loop in connection with QQuickItem::event().
    return QQuickItem::event(ev);
}

void QQuickWebPage::touchEvent(QTouchEvent* event)
{
    forceActiveFocus();
    this->event(event);
}

QQuickWebPagePrivate::QQuickWebPagePrivate(QQuickWebPage* q)
    : q(q)
    , webPageProxy(0)
    , sgUpdateQueue(q)
    , paintingIsInitialized(false)
    , m_paintNode(0)
{
}

void QQuickWebPagePrivate::initialize(WebKit::WebPageProxy* webPageProxy)
{
    this->webPageProxy = webPageProxy;
    eventHandler.reset(new QtWebPageEventHandler(toAPI(webPageProxy), q));
}

static float computeEffectiveOpacity(const QQuickItem* item)
{
    if (!item)
        return 1;

    float opacity = item->opacity();
    if (opacity < 0.01)
        return 0;

    return opacity * computeEffectiveOpacity(item->parentItem());
}

void QQuickWebPagePrivate::setDrawingAreaSize(const QSize& size)
{
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;
    drawingArea->setSize(WebCore::IntSize(size), WebCore::IntSize());
}

void QQuickWebPagePrivate::paintToCurrentGLContext()
{
    if (!q->isVisible())
        return;

    QTransform transform = q->itemTransform(0, 0);

    float opacity = computeEffectiveOpacity(q);
    QRectF clipRect = q->parentItem()->mapRectToScene(q->parentItem()->boundingRect());

    if (!clipRect.isValid())
        return;

    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;

    // Make sure that no GL error code stays from previous QT operations.
    glGetError();

    glEnable(GL_SCISSOR_TEST);
    ASSERT(!glGetError());
    const int left = clipRect.left();
    const int width = clipRect.width();
    const int bottom = q->canvas()->height() - (clipRect.bottom() + 1);
    const int height = clipRect.height();

    glScissor(left, bottom, width, height);
    ASSERT(!glGetError());

    drawingArea->paintToCurrentGLContext(transform, opacity);

    glDisable(GL_SCISSOR_TEST);
    ASSERT(!glGetError());
}

struct PageProxyMaterial;
struct PageProxyNode;

// FIXME: temporary until Qt Scenegraph will support custom painting.
struct PageProxyMaterialShader : public QSGMaterialShader {
    virtual void updateState(const RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial);
    virtual char const* const* attributeNames() const
    {
        static char const* const attr[] = { "vertex", 0 };
        return attr;
    }

    // vertexShader and fragmentShader are no-op shaders.
    // All real painting is gone by TextureMapper through LayerTreeHostProxy.
    virtual const char* vertexShader() const
    {
        return "attribute highp vec4 vertex; \n"
               "void main() { gl_Position = vertex; }";
    }

    virtual const char* fragmentShader() const
    {
        return "void main() { gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0); }";
    }
};

struct PageProxyMaterial : public QSGMaterial {
    PageProxyMaterial(PageProxyNode* node) : m_node(node) { }

    QSGMaterialType* type() const
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader* createShader() const
    {
        return new PageProxyMaterialShader;
    }

    PageProxyNode* m_node;
};

struct PageProxyNode : public QSGGeometryNode {
    PageProxyNode(QQuickWebPagePrivate* page) :
        m_pagePrivate(page)
      , m_material(this)
      , m_geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
    {
        setGeometry(&m_geometry);
        setMaterial(&m_material);
    }

    ~PageProxyNode()
    {
        if (m_pagePrivate)
            m_pagePrivate->resetPaintNode();
    }

    QQuickWebPagePrivate* m_pagePrivate;
    PageProxyMaterial m_material;
    QSGGeometry m_geometry;
};

void PageProxyMaterialShader::updateState(const RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial)
{
    if (!newMaterial)
        return;

    PageProxyNode* node = static_cast<PageProxyMaterial*>(newMaterial)->m_node;
    // FIXME: Normally we wouldn't paint inside QSGMaterialShader::updateState,
    // but this is a temporary hack until custom paint nodes are available.
    if (node->m_pagePrivate)
        node->m_pagePrivate->paintToCurrentGLContext();
}

QSGNode* QQuickWebPage::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!(flags() & ItemHasContents)) {
        if (oldNode)
            delete oldNode;
        return 0;
    }

    PageProxyNode* proxyNode = static_cast<PageProxyNode*>(oldNode);
    if (!proxyNode) {
        proxyNode = new PageProxyNode(d);
        d->m_paintNode = proxyNode;
    }

    return proxyNode;
}

void QQuickWebPagePrivate::resetPaintNode()
{
    m_paintNode = 0;
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (drawingArea && drawingArea->layerTreeHostProxy())
        drawingArea->layerTreeHostProxy()->purgeGLResources();
}

QQuickWebPagePrivate::~QQuickWebPagePrivate()
{
    if (m_paintNode)
        static_cast<PageProxyNode*>(m_paintNode)->m_pagePrivate = 0;
}

#include "moc_qquickwebpage_p.cpp"
