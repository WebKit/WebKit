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
#include <QWheelEvent>
#include <QtDeclarative/qquickitem.h>

namespace WebKit {

static const int kScaleAnimationDurationMillis = 250;

// UPDATE DEFERRING (SUSPEND/RESUME)
// =================================
//
// When interaction with the content, either by animating or by the hand of the user,
// it is important to ensure smooth animations of at least 60fps in order to give a
// good user experience.
//
// In order to do this we need to get rid of unknown factors. These include device
// sensors (geolocation, orientation updates etc), CSS3 animations, JavaScript
// exectution, sub resource loads etc. We do this by emitting suspend and resume
// signals, which are then handled by the viewport and propagates to the right place.
//
// For this reason the ViewportUpdateDeferrer guard must be used when we interact
// or animate the content.
//
// It should be noted that when we update content properties, we might receive notify
// signals send my the content item itself, and care should be taken to not act on
// these unconditionally. An example of this is the pinch zoom, which changes the
// position and will thus result in a QQuickWebPage::geometryChanged() signal getting
// emitted.
//
// If something should only be executed during update deferring, it is possible to
// check for that using ASSERT(m_suspendCount).

class ViewportUpdateDeferrer {
public:
    ViewportUpdateDeferrer(QtViewportInteractionEngine* engine)
        : engine(engine)
    {
        if (engine->m_suspendCount++)
            return;

        emit engine->contentSuspendRequested();
    }

    ~ViewportUpdateDeferrer()
    {
        if (--(engine->m_suspendCount))
            return;

        emit engine->contentResumeRequested();

        // Make sure that tiles all around the viewport will be requested.
        emit engine->viewportTrajectoryVectorChanged(QPointF());
    }

private:
    QtViewportInteractionEngine* const engine;
};

inline qreal QtViewportInteractionEngine::cssScaleFromItem(qreal itemScale)
{
    return itemScale / m_constraints.devicePixelRatio;
}

inline qreal QtViewportInteractionEngine::itemScaleFromCSS(qreal cssScale)
{
    return cssScale * m_constraints.devicePixelRatio;
}

QtViewportInteractionEngine::QtViewportInteractionEngine(const QQuickItem* viewport, QQuickItem* content)
    : m_viewport(viewport)
    , m_content(content)
    , m_suspendCount(0)
    , m_scaleAnimation(new ScaleAnimation(this))
    , m_pinchStartScale(1.f)
{
    reset();

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

    connect(m_content, SIGNAL(widthChanged()), this, SLOT(itemSizeChanged()), Qt::DirectConnection);
    connect(m_content, SIGNAL(heightChanged()), this, SLOT(itemSizeChanged()), Qt::DirectConnection);

    connect(m_scaleAnimation, SIGNAL(valueChanged(QVariant)),
            SLOT(scaleAnimationValueChanged(QVariant)), Qt::DirectConnection);
    connect(m_scaleAnimation, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
            SLOT(scaleAnimationStateChanged(QAbstractAnimation::State, QAbstractAnimation::State)), Qt::DirectConnection);

    connect(scroller(), SIGNAL(stateChanged(QScroller::State)),
            SLOT(scrollStateChanged(QScroller::State)), Qt::DirectConnection);
}

QtViewportInteractionEngine::~QtViewportInteractionEngine()
{
}

qreal QtViewportInteractionEngine::innerBoundedCSSScale(qreal cssScale)
{
    return qBound(m_constraints.minimumScale, cssScale, m_constraints.maximumScale);
}

qreal QtViewportInteractionEngine::outerBoundedCSSScale(qreal cssScale)
{
    if (m_constraints.isUserScalable) {
        // Bounded by [0.1, 10.0] like the viewport meta code in WebCore.
        qreal hardMin = qMax<qreal>(0.1, qreal(0.5) * m_constraints.minimumScale);
        qreal hardMax = qMin<qreal>(10, qreal(2.0) * m_constraints.maximumScale);
        return qBound(hardMin, cssScale, hardMax);
    }
    return innerBoundedCSSScale(cssScale);
}

void QtViewportInteractionEngine::setItemRectVisible(const QRectF& itemRect)
{
    ViewportUpdateDeferrer guard(this);

    qreal itemScale = m_viewport->width() / itemRect.width();

    m_content->setScale(itemScale);

    // We need to animate the content but the position represents the viewport hence we need to invert the position here.
    // To animate the position together with the scale we multiply the position with the current scale;
    m_content->setPos(- itemRect.topLeft() * itemScale);
}

void QtViewportInteractionEngine::animateItemRectVisible(const QRectF& itemRect)
{
    QRectF currentItemRectVisible = m_content->mapRectFromItem(m_viewport, m_viewport->boundingRect());
    if (itemRect == currentItemRectVisible)
        return;

    m_scaleAnimation->setDuration(kScaleAnimationDurationMillis);
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_scaleAnimation->setStartValue(currentItemRectVisible);
    m_scaleAnimation->setEndValue(itemRect);

    m_scaleAnimation->start();
}

void QtViewportInteractionEngine::scaleAnimationStateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State /*oldState*/)
{
    switch (newState) {
    case QAbstractAnimation::Running:
        m_scaleUpdateDeferrer = adoptPtr(new ViewportUpdateDeferrer(this));
        break;
    case QAbstractAnimation::Stopped:
        m_scaleUpdateDeferrer.clear();
        break;
    default:
        break;
    }
}

void QtViewportInteractionEngine::scrollStateChanged(QScroller::State newState)
{
    switch (newState) {
    case QScroller::Inactive:
        // FIXME: QScroller gets when even when tapping while it is scrolling.
        m_scrollUpdateDeferrer.clear();
        break;
    case QScroller::Pressed:
    case QScroller::Dragging:
    case QScroller::Scrolling:
        if (m_scrollUpdateDeferrer)
            break;
        m_scrollUpdateDeferrer = adoptPtr(new ViewportUpdateDeferrer(this));
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
        const QRectF contentRect = m_viewport->mapRectFromItem(m_content, m_content->boundingRect());
        const QRectF posRange = computePosRangeForItemAtScale(m_content->scale());
        prepareEvent->setContentPosRange(posRange);
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

static inline QPointF boundPosition(const QPointF minPosition, const QPointF& position, const QPointF& maxPosition)
{
    return QPointF(qBound(minPosition.x(), position.x(), maxPosition.x()),
                   qBound(minPosition.y(), position.y(), maxPosition.y()));
}

void QtViewportInteractionEngine::wheelEvent(QWheelEvent* ev)
{
    if (scrollAnimationActive() || scaleAnimationActive() || pinchGestureActive())
        return; // Ignore.

    int delta = ev->delta();
    QPointF newPos = -m_content->pos();

    // A delta that is not mod 120 indicates a device that is sending
    // fine-resolution scroll events, so use the delta as number of wheel ticks
    // and number of pixels to scroll. See also webkit.org/b/29601
    bool fullTick = !(delta % 120);

    static const int cDefaultQtScrollStep = 20;
    static const int wheelScrollLines = 3;
    int scrollLines = (fullTick) ? wheelScrollLines * cDefaultQtScrollStep : 1;

    delta = (fullTick) ? delta / 120.0f : delta;
    delta *= scrollLines;

    if (ev->orientation() == Qt::Horizontal)
        newPos.rx() += delta;
    else
        newPos.ry() += delta;

    QRectF endPosRange = computePosRangeForItemAtScale(m_content->scale());
    m_content->setPos(-boundPosition(endPosRange.topLeft(), newPos, endPosRange.bottomRight()));
}

void QtViewportInteractionEngine::pagePositionRequest(const QPoint& pagePosition)
{
    // Ignore the request if suspended. Can only happen due to delay in event delivery.
    if (m_suspendCount)
        return;

    qreal endItemScale = m_content->scale(); // Stay at same scale.

    QRectF endPosRange = computePosRangeForItemAtScale(endItemScale);
    QPointF endPosition = boundPosition(endPosRange.topLeft(), pagePosition * endItemScale, endPosRange.bottomRight());

    QRectF endVisibleContentRect(endPosition / endItemScale, m_viewport->boundingRect().size() / endItemScale);

    setItemRectVisible(endVisibleContentRect);
}

QRectF QtViewportInteractionEngine::computePosRangeForItemAtScale(qreal itemScale) const
{
    const QSizeF contentItemSize = m_content->boundingRect().size() * itemScale;
    const QSizeF viewportItemSize = m_viewport->boundingRect().size();

    const qreal horizontalRange = contentItemSize.width() - viewportItemSize.width();
    const qreal verticalRange = contentItemSize.height() - viewportItemSize.height();

    return QRectF(QPointF(0, 0), QSizeF(horizontalRange, verticalRange));
}

void QtViewportInteractionEngine::zoomToAreaGestureEnded(const QPointF& touchPoint, const QRectF& targetArea)
{
    if (!targetArea.isValid())
        return;

    if (scrollAnimationActive() || scaleAnimationActive())
        return;

    const int margin = 10; // We want at least a little bit or margin.
    QRectF endArea = targetArea.adjusted(-margin, -margin, margin, margin);
    endArea.setX(endArea.x() * m_constraints.devicePixelRatio);
    endArea.setY(endArea.y() * m_constraints.devicePixelRatio);
    endArea.setWidth(endArea.width() * m_constraints.devicePixelRatio);
    endArea.setHeight(endArea.height() * m_constraints.devicePixelRatio);

    const QRectF viewportRect = m_viewport->boundingRect();

    qreal targetCSSScale = cssScaleFromItem(viewportRect.size().width() / endArea.size().width());
    qreal endItemScale = itemScaleFromCSS(innerBoundedCSSScale(qMin(targetCSSScale, qreal(2.5))));

    // We want to end up with the target area filling the whole width of the viewport (if possible),
    // and centralized vertically where the user requested zoom. Thus our hotspot is the center of
    // the targetArea x-wise and the requested zoom position, y-wise.
    const QPointF hotspot = QPointF(endArea.center().x(), touchPoint.y() * m_constraints.devicePixelRatio);
    const QPointF viewportHotspot = viewportRect.center();

    QPointF endPosition = hotspot * endItemScale - viewportHotspot;

    QRectF endPosRange = computePosRangeForItemAtScale(endItemScale);
    endPosition = boundPosition(endPosRange.topLeft(), endPosition, endPosRange.bottomRight());

    QRectF endVisibleContentRect(endPosition / endItemScale, viewportRect.size() / endItemScale);

    animateItemRectVisible(endVisibleContentRect);
}

void QtViewportInteractionEngine::ensureContentWithinViewportBoundary(bool immediate)
{
    ASSERT(m_suspendCount);

    if (scrollAnimationActive() || scaleAnimationActive())
        return;

    qreal currentCSSScale = cssScaleFromItem(m_content->scale());

    qreal endItemScale = itemScaleFromCSS(innerBoundedCSSScale(currentCSSScale));

    const QRectF viewportRect = m_viewport->boundingRect();
    QPointF viewportHotspot = viewportRect.center();

    QPointF endPosition = m_content->mapFromItem(m_viewport, viewportHotspot) * endItemScale - viewportHotspot;

    QRectF endPosRange = computePosRangeForItemAtScale(endItemScale);
    endPosition = boundPosition(endPosRange.topLeft(), endPosition, endPosRange.bottomRight());

    QRectF endVisibleContentRect(endPosition / endItemScale, viewportRect.size() / endItemScale);

    if (immediate)
        setItemRectVisible(endVisibleContentRect);
    else
        animateItemRectVisible(endVisibleContentRect);
}

void QtViewportInteractionEngine::reset()
{
    ASSERT(!m_suspendCount);

    m_hadUserInteraction = false;

    scroller()->stop();
    m_scaleAnimation->stop();
}

void QtViewportInteractionEngine::applyConstraints(const Constraints& constraints)
{
    // We always have to apply the constrains even if they didn't change, as
    // the initial scale might need to be applied.

    ViewportUpdateDeferrer guard(this);

    m_constraints = constraints;

    if (!m_hadUserInteraction) {
        qreal initialScale = innerBoundedCSSScale(m_constraints.initialScale);
        m_content->setScale(itemScaleFromCSS(initialScale));
    }

    // If the web app changes successively changes the viewport on purpose
    // it wants to be in control and we should disable animations.
    ensureContentWithinViewportBoundary(/* immediate */ true);
}

bool QtViewportInteractionEngine::scrollAnimationActive() const
{
    QScroller* scroller = const_cast<QtViewportInteractionEngine*>(this)->scroller();
    return scroller->state() == QScroller::Scrolling;
}

void QtViewportInteractionEngine::interruptScrollAnimation()
{
    // Stopping the scroller immediately stops kinetic scrolling and if the view is out of bounds it
    // is moved inside valid bounds immediately as well. This is the behavior that we want.
    scroller()->stop();
}

bool QtViewportInteractionEngine::panGestureActive() const
{
    QScroller* scroller = const_cast<QtViewportInteractionEngine*>(this)->scroller();
    return scroller->state() == QScroller::Pressed || scroller->state() == QScroller::Dragging;
}

void QtViewportInteractionEngine::panGestureStarted(const QPointF& touchPoint, qint64 eventTimestampMillis)
{
    m_hadUserInteraction = true;
    scroller()->handleInput(QScroller::InputPress, m_viewport->mapFromItem(m_content, touchPoint), eventTimestampMillis);
}

void QtViewportInteractionEngine::panGestureRequestUpdate(const QPointF& touchPoint, qint64 eventTimestampMillis)
{
    scroller()->handleInput(QScroller::InputMove, m_viewport->mapFromItem(m_content, touchPoint), eventTimestampMillis);
}

void QtViewportInteractionEngine::panGestureCancelled()
{
    // Stopping the scroller immediately stops kinetic scrolling and if the view is out of bounds it
    // is moved inside valid bounds immediately as well. This is the behavior that we want.
    scroller()->stop();
}

void QtViewportInteractionEngine::panGestureEnded(const QPointF& touchPoint, qint64 eventTimestampMillis)
{
    scroller()->handleInput(QScroller::InputRelease, m_viewport->mapFromItem(m_content, touchPoint), eventTimestampMillis);
}

bool QtViewportInteractionEngine::scaleAnimationActive() const
{
    return m_scaleAnimation->state() == QAbstractAnimation::Running;
}

void QtViewportInteractionEngine::interruptScaleAnimation()
{
    // This interrupts the scale animation exactly where it is, even if it is out of bounds.
    m_scaleAnimation->stop();
}

bool QtViewportInteractionEngine::pinchGestureActive() const
{
    return m_pinchStartScale > 0;
}

void QtViewportInteractionEngine::pinchGestureStarted(const QPointF& pinchCenterInContentCoordinates)
{
    ASSERT(m_suspendCount);

    if (!m_constraints.isUserScalable)
        return;

    m_hadUserInteraction = true;

    m_scaleUpdateDeferrer = adoptPtr(new ViewportUpdateDeferrer(this));

    m_lastPinchCenterInViewportCoordinates = m_viewport->mapFromItem(m_content, pinchCenterInContentCoordinates);
    m_pinchStartScale = m_content->scale();

    // Reset the tiling look-ahead vector so that tiles all around the viewport will be requested on pinch-end.
    emit viewportTrajectoryVectorChanged(QPointF());
}

void QtViewportInteractionEngine::pinchGestureRequestUpdate(const QPointF& pinchCenterInContentCoordinates, qreal totalScaleFactor)
{
    ASSERT(m_suspendCount);

    if (!m_constraints.isUserScalable)
        return;

    //  Changes of the center position should move the page even if the zoom factor
    //  does not change.
    const qreal cssScale = cssScaleFromItem(m_pinchStartScale * totalScaleFactor);

    // Allow zooming out beyond mimimum scale on pages that do not explicitly disallow it.
    const qreal targetCSSScale = outerBoundedCSSScale(cssScale);

    QPointF pinchCenterInViewportCoordinates = m_viewport->mapFromItem(m_content, pinchCenterInContentCoordinates);

    scaleContent(pinchCenterInContentCoordinates, targetCSSScale);

    const QPointF positionDiff = pinchCenterInViewportCoordinates - m_lastPinchCenterInViewportCoordinates;
    m_lastPinchCenterInViewportCoordinates = pinchCenterInViewportCoordinates;
    m_content->setPos(m_content->pos() + positionDiff);
}

void QtViewportInteractionEngine::pinchGestureEnded()
{
    ASSERT(m_suspendCount);

    if (!m_constraints.isUserScalable)
        return;

    m_pinchStartScale = -1;
    ensureContentWithinViewportBoundary();
}

void QtViewportInteractionEngine::itemSizeChanged()
{
    // FIXME: This needs to be done smarter. What happens if it resizes when we were interacting?
    if (m_suspendCount)
        return;

    ViewportUpdateDeferrer guard(this);
    ensureContentWithinViewportBoundary();
}

void QtViewportInteractionEngine::scaleContent(const QPointF& centerInContentCoordinates, qreal cssScale)
{
    QPointF oldPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), centerInContentCoordinates);
    m_content->setScale(itemScaleFromCSS(cssScale));
    QPointF newPinchCenterOnParent = m_content->mapToItem(m_content->parentItem(), centerInContentCoordinates);
    m_content->setPos(m_content->pos() - (newPinchCenterOnParent - oldPinchCenterOnParent));
}

#include "moc_QtViewportInteractionEngine.cpp"

}
