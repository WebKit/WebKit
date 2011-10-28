/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtViewportInteractionEngine.h"

#include "PassOwnPtr.h"
#include <QPointF>
#include <QScrollEvent>
#include <QScrollPrepareEvent>
#include <QScrollerProperties>
#include <QtDeclarative/qsgitem.h>

namespace WebKit {

static const int kScaleAnimationDurationMillis = 400;

static inline QRectF contentRectInViewportCoordinates(const QSGItem* content, const QSGItem* viewport)
{
    return viewport->mapRectFromItem(content, content->boundingRect());
}

static inline qreal bindToScaleLimits(qreal scale)
{
    // Bounded by [0.1, 10.0] like the viewport meta code in WebCore.
    return qBound(qreal(0.1), scale, qreal(10.0));
}

// Updating content properties cause the notify signals to be sent by the content item itself.
// Since we manage those differently, we do not want to respond
// to them when we are the one changing the content.
//
// The guard makes sure that the signal viewportUpdateRequested() is sent if necessary.
// When multiple guards are alive, their lifetime must be perfectly imbricated (e.g. if used ouside stack frames).
// We rely on the first one to trigger the update at the end.
//
// The public methods should create the guard if they might update content.
class ViewportUpdateGuard {
public:
    ViewportUpdateGuard(QtViewportInteractionEngine* viewportInteractionEngine)
        : viewportInteractionEngine(viewportInteractionEngine)
        , previousPosition(viewportInteractionEngine->m_content->pos())
        , previousSize(viewportInteractionEngine->m_content->width(), viewportInteractionEngine->m_content->height())
        , previousScale(viewportInteractionEngine->m_content->scale())

    {
        ++(viewportInteractionEngine->m_pendingUpdates);
    }

    ~ViewportUpdateGuard()
    {
        --(viewportInteractionEngine->m_pendingUpdates);
        if (!viewportInteractionEngine->m_pendingUpdates) {
            // Reset the tiling look-ahead vector so that tiles all around the viewport will be requested.
            emit viewportInteractionEngine->viewportTrajectoryVectorChanged(QPointF());
            if (previousPosition != viewportInteractionEngine->m_content->pos()
                    || previousSize.width() != viewportInteractionEngine->m_content->width()
                    || previousSize.height() != viewportInteractionEngine->m_content->height()
                    || previousScale != viewportInteractionEngine->m_content->scale())
                // We must notify the change so the client can rely on us for all changes of geometry.
                emit viewportInteractionEngine->viewportUpdateRequested();
        }
    }

private:
    QtViewportInteractionEngine* const viewportInteractionEngine;
    const QPointF previousPosition;
    const QSizeF previousSize;
    const qreal previousScale;
};

QtViewportInteractionEngine::QtViewportInteractionEngine(const QSGItem* viewport, QSGItem* content)
    : m_viewport(viewport)
    , m_content(content)
    , m_pendingUpdates(0)
    , m_scaleAnimation(new ScaleAnimation(this))
    , m_pinchStartScale(1.f)
{
    reset();
    connect(m_content, SIGNAL(xChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(yChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(widthChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(heightChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(scaleChanged()), this, SLOT(contentViewportChanged()), Qt::DirectConnection);
    connect(m_scaleAnimation, SIGNAL(valueChanged(QVariant)), SLOT(updateVisibleRect(QVariant)), Qt::DirectConnection);
    connect(m_scaleAnimation, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State))
            , SLOT(scaleAnimationStateChanged(QAbstractAnimation::State, QAbstractAnimation::State)), Qt::DirectConnection);
}

QtViewportInteractionEngine::~QtViewportInteractionEngine()
{
}

qreal QtViewportInteractionEngine::innerBoundedScale(qreal scale)
{
    return qBound(m_constraints.minimumScale, scale, m_constraints.maximumScale);
}

qreal QtViewportInteractionEngine::outerBoundedScale(qreal scale)
{
    if (m_constraints.isUserScalable) {
        qreal hardMin = qreal(0.5) * m_constraints.minimumScale;
        qreal hardMax = qreal(2.0) * m_constraints.maximumScale;
        return bindToScaleLimits(qBound(hardMin, scale, hardMax));
    }
    return innerBoundedScale(scale);
}

void QtViewportInteractionEngine::updateVisibleRect(QVariant rect)
{
    ViewportUpdateGuard guard(this);

    QRectF visibleRect = rect.toRectF();
    qreal currentScale = m_viewport->width() / visibleRect.width();

    m_content->setScale(currentScale);

    // We need to animate the content but the position represents the viewport hence we need to invert the position here.
    // To animate the position together with the scale we multiply the position with the current scale;
    m_content->setPos(-visibleRect.topLeft() * currentScale);
}

void QtViewportInteractionEngine::scaleAnimationStateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State /*oldState*/)
{
    switch (newState) {
    case QAbstractAnimation::Running:
        m_pinchViewportUpdateDeferrer = adoptPtr(new ViewportUpdateGuard(this));
        break;
    case QAbstractAnimation::Stopped:
        m_pinchViewportUpdateDeferrer.clear();
        break;
    default:
        break;
    }
}

bool QtViewportInteractionEngine::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::ScrollPrepare: {
        QScrollPrepareEvent* prepareEvent = static_cast<QScrollPrepareEvent*>(event);
        const QRectF viewportRect = m_viewport->boundingRect();
        const QRectF contentRect = contentRectInViewportCoordinates(m_content, m_viewport);
        const QRectF contentPositionRange = calculateBoundariesForScale(contentRect.size(), viewportRect.size(), 1);
        prepareEvent->setContentPosRange(contentPositionRange);
        prepareEvent->setViewportSize(viewportRect.size());

        // As we want to push the contents and not actually scroll it, we need to invert the positions here.
        prepareEvent->setContentPos(-contentRect.topLeft());
        prepareEvent->accept();
        return true;
    }
    case QEvent::Scroll: {
        QScrollEvent* scrollEvent = static_cast<QScrollEvent*>(event);
        QPointF newPos = -scrollEvent->contentPos() - scrollEvent->overshootDistance();
        if (m_content->pos() != newPos) {
            ViewportUpdateGuard guard(this);

            QPointF currentPosInContentCoordinates = m_content->mapToItem(m_content->parentItem(), m_content->pos());
            QPointF newPosInContentCoordinates = m_content->mapToItem(m_content->parentItem(), newPos);

            // This must be emitted before viewportUpdateRequested so that the web process knows where to look for tiles.
            emit viewportTrajectoryVectorChanged(currentPosInContentCoordinates- newPosInContentCoordinates);
            m_content->setPos(newPos);
        }
        return true;
    }
    default:
        break;
    }
    return QObject::event(event);
}

void QtViewportInteractionEngine::stopAnimations()
{
    m_scaleAnimation->stop();
    scroller()->stop();

    // If the animations were stopped we need to scale and reposition the content into valid boundaries immediately.
    m_userInteractionFlags |= UserHasStoppedAnimations;
    animateContentIntoBoundariesIfNeeded();
    m_userInteractionFlags &= ~UserHasStoppedAnimations;
}

const QRectF QtViewportInteractionEngine::calculateBoundariesForScale(const QSizeF contentSize, const QSizeF viewportSize, qreal scale)
{
    const QSizeF scaledViewportSize = viewportSize / scale;

    const qreal horizontalOffset = contentSize.width() - scaledViewportSize.width();
    const qreal verticalOffset = contentSize.height() - scaledViewportSize.height();

    const QPointF contentPositionRangeTopLeft(qMin<qreal>(0, horizontalOffset / 2), 0);
    const qreal contentPositionRangeWidth = qMax<qreal>(0, horizontalOffset);
    const qreal contentPositionRangeHeight = qMax<qreal>(0, verticalOffset);

    return QRectF(contentPositionRangeTopLeft, QSizeF(contentPositionRangeWidth, contentPositionRangeHeight));
}

void QtViewportInteractionEngine::animateContentIntoBoundariesIfNeeded()
{
    m_scaleAnimation->stop();

    qreal currentScale = m_content->scale();
    bool userHasScaledContent = m_userInteractionFlags & UserHasScaledContent;
    bool userHasStoppedAnimations = m_userInteractionFlags & UserHasStoppedAnimations;

    if (!userHasScaledContent)
        currentScale = m_constraints.initialScale;

    qreal boundedScale = innerBoundedScale(currentScale);

    QRectF viewportRect = m_viewport->boundingRect();
    QPointF viewportHotspot = viewportRect.center();
    QPointF position = m_content->mapFromItem(m_viewport, viewportHotspot) - viewportHotspot / boundedScale;
    QRectF positionBoundaries = calculateBoundariesForScale(m_content->boundingRect().size(), viewportRect.size(), boundedScale);
    QPointF minOffset = positionBoundaries.topLeft();
    QPointF maxOffset = positionBoundaries.bottomRight();
    QPointF boundedPosition;
    boundedPosition.setX(qBound(minOffset.x(), position.x(), maxOffset.x()));
    boundedPosition.setY(qBound(minOffset.y(), position.y(), maxOffset.y()));

    QRectF scaledRect(boundedPosition, viewportRect.size() / boundedScale);
    QRectF currentRect = m_viewport->mapRectToItem(m_content, viewportRect);

    if (scaledRect == currentRect)
        return;

    if (userHasScaledContent && !userHasStoppedAnimations) {
        m_scaleAnimation->setDuration(kScaleAnimationDurationMillis);
        m_scaleAnimation->setEasingCurve(QEasingCurve::OutCubic);
        m_scaleAnimation->setStartValue(currentRect);
        m_scaleAnimation->setEndValue(scaledRect);
        m_scaleAnimation->start();
    } else
        updateVisibleRect(scaledRect);
}

void QtViewportInteractionEngine::reset()
{
    ViewportUpdateGuard guard(this);
    m_userInteractionFlags = UserHasNotInteractedWithContent;

    stopAnimations();

    QScrollerProperties properties = scroller()->scrollerProperties();

    // The QtPanGestureRecognizer is responsible for recognizing the gesture
    // thus we need to disable the drag start distance.
    properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0);

    // Set some default QScroller constrains to mimic the physics engine of the N9 browser.
    properties.setScrollMetric(QScrollerProperties::AxisLockThreshold, 0.66);
    properties.setScrollMetric(QScrollerProperties::ScrollingCurve, QEasingCurve(QEasingCurve::OutExpo));
    properties.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.05);
    properties.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.635);
    properties.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.33);
    properties.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.33);

    scroller()->setScrollerProperties(properties);
    setConstraints(Constraints());
}

void QtViewportInteractionEngine::setConstraints(const Constraints& constraints)
{
    if (m_constraints == constraints)
        return;

    ViewportUpdateGuard guard(this);
    m_constraints = constraints;
    animateContentIntoBoundariesIfNeeded();
}

void QtViewportInteractionEngine::panGestureStarted(const QPointF& touchPoint, qint64 eventTimestampMillis)
{
    // FIXME: suspend the Web engine (stop animated GIF, etc).
    m_userInteractionFlags |= UserHasMovedContent;
    scroller()->handleInput(QScroller::InputPress, m_viewport->mapFromItem(m_content, touchPoint), eventTimestampMillis);
}

void QtViewportInteractionEngine::panGestureRequestUpdate(const QPointF& touchPoint, qint64 eventTimestampMillis)
{
    ViewportUpdateGuard guard(this);
    scroller()->handleInput(QScroller::InputMove, m_viewport->mapFromItem(m_content, touchPoint), eventTimestampMillis);
}

void QtViewportInteractionEngine::panGestureCancelled()
{
    ViewportUpdateGuard guard(this);
    stopAnimations();
}

void QtViewportInteractionEngine::panGestureEnded(const QPointF& touchPoint, qint64 eventTimestampMillis)
{
    ViewportUpdateGuard guard(this);
    scroller()->handleInput(QScroller::InputRelease, m_viewport->mapFromItem(m_content, touchPoint), eventTimestampMillis);
}

void QtViewportInteractionEngine::pinchGestureStarted(const QPointF& pinchCenterInContentCoordinates)
{
    if (!m_constraints.isUserScalable)
        return;

    m_pinchViewportUpdateDeferrer = adoptPtr(new ViewportUpdateGuard(this));
    m_lastPinchCenterInViewportCoordinates = m_viewport->mapFromItem(m_content, pinchCenterInContentCoordinates);
    m_userInteractionFlags |= UserHasScaledContent;
    m_userInteractionFlags |= UserHasMovedContent;
    m_pinchStartScale = m_content->scale();

    // Reset the tiling look-ahead vector so that tiles all around the viewport will be requested on pinch-end.
    emit viewportTrajectoryVectorChanged(QPointF());
}

void QtViewportInteractionEngine::pinchGestureRequestUpdate(const QPointF& pinchCenterInContentCoordinates, qreal totalScaleFactor)
{
    if (!m_constraints.isUserScalable)
        return;

    ViewportUpdateGuard guard(this);

    //  Changes of the center position should move the page even if the zoom factor
    //  does not change.
    const qreal scale = m_pinchStartScale * totalScaleFactor;

    // Allow zooming out beyond mimimum scale on pages that do not explicitly disallow it.
    const qreal boundedScale = outerBoundedScale(scale);

    QPointF pinchCenterInViewportCoordinates = m_viewport->mapFromItem(m_content, pinchCenterInContentCoordinates);

    scaleContent(pinchCenterInContentCoordinates, boundedScale);

    const QPointF positionDiff = pinchCenterInViewportCoordinates - m_lastPinchCenterInViewportCoordinates;
    m_lastPinchCenterInViewportCoordinates = pinchCenterInViewportCoordinates;
    m_content->setPos(m_content->pos() + positionDiff);
}

void QtViewportInteractionEngine::pinchGestureEnded()
{
    if (!m_constraints.isUserScalable)
        return;

    m_pinchViewportUpdateDeferrer.clear();
    // FIXME: resume the engine after the animation.
    animateContentIntoBoundariesIfNeeded();
}

void QtViewportInteractionEngine::contentViewportChanged()
{
    if (m_pendingUpdates)
        return;

    ViewportUpdateGuard guard(this);

    animateContentIntoBoundariesIfNeeded();
}

void QtViewportInteractionEngine::scaleContent(const QPointF& centerInContentCoordinates, qreal scale)
{
    QPointF oldPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), centerInContentCoordinates);
    m_content->setScale(scale);
    QPointF newPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), centerInContentCoordinates);
    m_content->setPos(m_content->pos() - (newPinchCenterOnParent - oldPinchCenterOnParent));
}

#include "moc_QtViewportInteractionEngine.cpp"

}
