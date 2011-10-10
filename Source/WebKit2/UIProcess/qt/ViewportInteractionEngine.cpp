/*
 * Copyright (C) 2011 Benjamin Poulain <benjamin@webkit.org>
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
#include "ViewportInteractionEngine.h"

#include "PassOwnPtr.h"
#include <QPointF>
#include <QtDeclarative/qsgitem.h>

namespace WebKit {

static inline QRectF visibleRectInContentCoordinate(const QSGItem* content, const QSGItem* viewport)
{
    const QRectF viewportInContentCoordinate = content->mapRectFromItem(viewport, viewport->boundingRect());
    const QRectF visibleArea = content->boundingRect().intersected(viewportInContentCoordinate);
    return visibleArea;
}

static inline QRectF contentRectInViewportCoordinate(const QSGItem* content, const QSGItem* viewport)
{
    return viewport->mapRectFromItem(content, content->boundingRect());
}

// Updating content properties cause the notify signals to be sent by the content item itself.
// Since we manage those differently, we do not want to respond
// to them when we are the one changing the content.
//
// The guard make sure the signal viewportUpdateRequested() is sent if necessary.
// When multiple guards are alive, their lifetime must be perfectly imbricated (e.g. if used ouside stack frames).
// We rely on the first one to trigger the update at the end since it only uses one bool internally.
//
// The public methods should create the guard if they might update content.
class ViewportUpdateGuard {
public:
    ViewportUpdateGuard(ViewportInteractionEngine* viewportInteractionEngine)
        : viewportInteractionEngine(viewportInteractionEngine)
        , wasUpdatingContent(viewportInteractionEngine->m_isUpdatingContent)
        , previousPosition(viewportInteractionEngine->m_content->pos())
        , previousSize(viewportInteractionEngine->m_content->width(), viewportInteractionEngine->m_content->height())
        , previousScale(viewportInteractionEngine->m_content->scale())
    {
        viewportInteractionEngine->m_isUpdatingContent = true;
    }

    ~ViewportUpdateGuard()
    {
        if (!wasUpdatingContent) {
            if (previousPosition != viewportInteractionEngine->m_content->pos()
                    || previousSize.width() != viewportInteractionEngine->m_content->width()
                    || previousSize.height() != viewportInteractionEngine->m_content->height()
                    || previousScale != viewportInteractionEngine->m_content->scale())
                emit viewportInteractionEngine->viewportUpdateRequested();
        }
        viewportInteractionEngine->m_isUpdatingContent = wasUpdatingContent;
    }

private:
    ViewportInteractionEngine* const viewportInteractionEngine;
    const bool wasUpdatingContent;
    const QPointF previousPosition;
    const QSizeF previousSize;
    const qreal previousScale;
};

ViewportInteractionEngine::ViewportInteractionEngine(const QSGItem* viewport, QSGItem* content)
    : m_viewport(viewport)
    , m_content(content)
    , m_isUpdatingContent(false)
    , m_pinchStartScale(1.f)
{
    reset();
    connect(m_content, SIGNAL(xChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(yChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(widthChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(heightChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(scaleChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
}

ViewportInteractionEngine::~ViewportInteractionEngine()
{
}

void ViewportInteractionEngine::reset()
{
    ViewportUpdateGuard guard(this);
    m_userInteractionFlags = UserHasNotInteractedWithContent;
    setConstraints(Constraints());
}

void ViewportInteractionEngine::setConstraints(const Constraints& constraints)
{
    if (m_constraints == constraints)
        return;

    // FIXME: if a pinch gesture is ongoing, we have to:
    // -cancel the gesture if isUserScalable becomes true
    // -update the page on the fly without modifying its position

    // FIXME: if a pan gesture is ongoing, we have to
    // -update the page without changing the position
    // -animate the page back in viewport if necessary (if the page is fully in
    //  viewport, it does not pan horizontally anymore).

    ViewportUpdateGuard guard(this);
    m_constraints = constraints;
    updateContentIfNeeded();
}

void ViewportInteractionEngine::panGestureStarted()
{
    // FIXME: suspend the Web engine (stop animated GIF, etc).
    // FIXME: initialize physics for panning (stop animation, etc).
    m_userInteractionFlags |= UserHasMovedContent;
}

void ViewportInteractionEngine::panGestureRequestUpdate(qreal deltaX, qreal deltaY)
{
    ViewportUpdateGuard guard(this);

    QPointF itemPositionInItemCoords = m_content->mapFromItem(m_content->parentItem(), m_content->pos());
    QPointF destInViewportCoords = m_viewport->mapFromItem(m_content, itemPositionInItemCoords + QPointF(deltaX, deltaY));

    m_content->setPos(destInViewportCoords);
    // This must be emitted before viewportUpdateRequested so that the web process knows where to look for tiles.
    emit viewportTrajectoryVectorChanged(QPointF(-deltaX, -deltaY));
}

void ViewportInteractionEngine::panGestureCancelled()
{
    ViewportUpdateGuard guard(this);
    // FIXME: reset physics.
    panGestureEnded();
}

void ViewportInteractionEngine::panGestureEnded()
{
    ViewportUpdateGuard guard(this);
    animateContentIntoBoundariesIfNeeded();
    // FIXME: emit viewportTrajectoryVectorChanged(QPointF()) when the pan throw animation ends and the page stops moving.
}

void ViewportInteractionEngine::pinchGestureStarted()
{
    if (!m_constraints.isUserScalable)
        return;

    m_pinchViewportUpdateDeferrer = adoptPtr(new ViewportUpdateGuard(this));

    m_userInteractionFlags |= UserHasScaledContent;
    m_userInteractionFlags |= UserHasMovedContent;
    m_pinchStartScale = m_content->scale();

    // Reset the tiling look-ahead vector so that tiles all around the viewport will be requested on pinch-end.
    emit viewportTrajectoryVectorChanged(QPointF());
}

void ViewportInteractionEngine::pinchGestureRequestUpdate(const QPointF& pinchCenterInContentCoordinate, qreal totalScaleFactor)
{
    if (!m_constraints.isUserScalable)
        return;

    ViewportUpdateGuard guard(this);

    //  Changes of the center position should move the page even if the zoom factor
    //  does not change. Both the zoom and the panning should be handled through the physics engine.
    const qreal scale = m_pinchStartScale * totalScaleFactor;
    QPointF oldPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), pinchCenterInContentCoordinate);
    m_content->setScale(scale);
    QPointF newPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), pinchCenterInContentCoordinate);
    m_content->setPos(m_content->pos() - (newPinchCenterOnParent - oldPinchCenterOnParent));
}

void ViewportInteractionEngine::pinchGestureEnded()
{
    if (!m_constraints.isUserScalable)
        return;

    {
        ViewportUpdateGuard guard(this);
        // FIXME: resume the engine after the animation.
        animateContentIntoBoundariesIfNeeded();
    }
    m_pinchViewportUpdateDeferrer.clear();
}

void ViewportInteractionEngine::contentViewportChanged()
{
    if (m_isUpdatingContent)
        return;

    ViewportUpdateGuard guard(this);
    updateContentIfNeeded();

    // We must notify the change so the client can rely on us for all change of Geometry.
    emit viewportUpdateRequested();
}

void ViewportInteractionEngine::updateContentIfNeeded()
{
    updateContentScaleIfNeeded();
    updateContentPositionIfNeeded();
}

void ViewportInteractionEngine::updateContentScaleIfNeeded()
{
    const qreal currentContentScale = m_content->scale();
    qreal contentScale = m_content->scale();
    if (!(m_userInteractionFlags & UserHasScaledContent))
        contentScale = m_constraints.initialScale;

    contentScale = qBound(m_constraints.minimumScale, contentScale, m_constraints.maximumScale);

    if (contentScale != currentContentScale) {
        const QPointF centerOfInterest = visibleRectInContentCoordinate(m_content, m_viewport).center();
        scaleContent(centerOfInterest, contentScale);
    }
}

void ViewportInteractionEngine::updateContentPositionIfNeeded()
{
    if (!(m_userInteractionFlags & UserHasMovedContent)) {
        m_content->setX((m_viewport->width() - contentRectInViewportCoordinate(m_content, m_viewport).width()) / 2);
        m_content->setY(0);
    }

    // FIXME: if the item can be fully in the viewport and is over a side, push it back in view
    // FIXME: if the item cannot be fully in viewport, and is not covering the viewport, push it back in view
}

void ViewportInteractionEngine::animateContentIntoBoundariesIfNeeded()
{
    animateContentScaleIntoBoundariesIfNeeded();
    animateContentPositionIntoBoundariesIfNeeded();
}

void ViewportInteractionEngine::animateContentPositionIntoBoundariesIfNeeded()
{
    const QRectF contentGeometry = m_viewport->mapRectFromItem(m_content, m_content->boundingRect());
    QPointF newPos = contentGeometry.topLeft();

    // Horizontal correction.
    if (contentGeometry.width() < m_viewport->width())
        newPos.setX((m_viewport->width() - contentGeometry.width()) / 2);
    else {
        newPos.setX(qMin<qreal>(0., newPos.x()));
        const qreal rightSideGap = m_viewport->boundingRect().right() - contentGeometry.right();
        if (rightSideGap > 0.)
            newPos.setX(newPos.x() + rightSideGap);
    }

    // Vertical correction.
    if (contentGeometry.height() < m_viewport->height())
        newPos.setY(0);
    else {
        newPos.setY(qMin<qreal>(0., newPos.y()));
        const qreal bottomSideGap = m_viewport->boundingRect().bottom() - contentGeometry.bottom();
        if (bottomSideGap > 0.)
            newPos.setY(newPos.y() + bottomSideGap);
    }

    if (newPos != m_content->pos())
        // FIXME: Do you know what ANIMATE means?
        m_content->setPos(newPos);
}

void ViewportInteractionEngine::animateContentScaleIntoBoundariesIfNeeded()
{
    const qreal currentScale = m_content->scale();
    const qreal boundedScale = qBound(m_constraints.minimumScale, currentScale, m_constraints.maximumScale);
    if (currentScale != boundedScale) {
        // FIXME: Do you know what ANIMATE means?
        const QPointF centerOfInterest = visibleRectInContentCoordinate(m_content, m_viewport).center();
        scaleContent(centerOfInterest, boundedScale);
    }
}

void ViewportInteractionEngine::scaleContent(const QPointF& centerInContentCoordinate, qreal scale)
{
    QPointF oldPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), centerInContentCoordinate);
    m_content->setScale(scale);
    QPointF newPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), centerInContentCoordinate);
    m_content->setPos(m_content->pos() - (newPinchCenterOnParent - oldPinchCenterOnParent));
}

#include "moc_ViewportInteractionEngine.cpp"

}
