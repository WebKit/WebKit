/*
 * Copyright (C) 2011, 2012 Nokia Corporation and/or its subsidiary(-ies)
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
#include "PageViewportControllerClientQt.h"

#include "qquickwebpage_p.h"
#include "qquickwebview_p.h"
#include "qwebkittest_p.h"
#include <QPointF>
#include <QTransform>
#include <QtQuick/qquickitem.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>

namespace WebKit {

static const int kScaleAnimationDurationMillis = 250;

PageViewportControllerClientQt::PageViewportControllerClientQt(QQuickWebView* viewportItem, QQuickWebPage* pageItem)
    : m_viewportItem(viewportItem)
    , m_pageItem(pageItem)
    , m_scaleAnimation(new ScaleAnimation(this))
    , m_pinchStartScale(-1)
    , m_lastCommittedScale(-1)
    , m_zoomOutScale(0)
    , m_ignoreViewportChanges(true)
{
    m_scaleAnimation->setDuration(kScaleAnimationDurationMillis);
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_viewportItem, SIGNAL(movementStarted()), SLOT(flickMoveStarted()), Qt::DirectConnection);
    connect(m_viewportItem, SIGNAL(movementEnded()), SLOT(flickMoveEnded()), Qt::DirectConnection);
    connect(m_viewportItem, SIGNAL(contentXChanged()), SLOT(pageItemPositionChanged()));
    connect(m_viewportItem, SIGNAL(contentYChanged()), SLOT(pageItemPositionChanged()));


    connect(m_scaleAnimation, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
            SLOT(scaleAnimationStateChanged(QAbstractAnimation::State, QAbstractAnimation::State)));
}

void PageViewportControllerClientQt::ScaleAnimation::updateCurrentValue(const QVariant& value)
{
    // Resetting the end value, the easing curve or the duration of the scale animation
    // triggers a recalculation of the animation interval. This might change the current
    // value of the animated property.
    // Make sure we only act on animation value changes if the animation is active.
    if (!m_controllerClient->scaleAnimationActive())
        return;

    QRectF itemRect = value.toRectF();
    float itemScale = m_controllerClient->viewportScaleForRect(itemRect);

    m_controllerClient->setContentRectVisiblePositionAtScale(itemRect.topLeft(), itemScale);
}

PageViewportControllerClientQt::~PageViewportControllerClientQt()
{
}

void PageViewportControllerClientQt::setContentRectVisiblePositionAtScale(const QPointF& location, qreal itemScale)
{
    ASSERT(itemScale >= 0);

    scaleContent(itemScale);

    // To animate the position together with the scale we multiply the position with the current scale
    // and add it to the page position (displacement on the flickable contentItem because of additional items).
    QPointF newPosition(m_pageItem->pos() + location * itemScale);

    m_viewportItem->setContentPos(newPosition);
}

void PageViewportControllerClientQt::animateContentRectVisible(const QRectF& contentRect)
{
    ASSERT(m_scaleAnimation->state() == QAbstractAnimation::Stopped);

    ASSERT(!scrollAnimationActive());
    if (scrollAnimationActive())
        return;

    QRectF viewportRectInContentCoords = m_viewportItem->mapRectToWebContent(m_viewportItem->boundingRect());
    if (contentRect == viewportRectInContentCoords) {
        updateViewportController();
        return;
    }

    // Since we have to animate scale and position at the same time the scale animation interpolates
    // from the current viewport rect in content coordinates to a visible rect of the content.
    m_scaleAnimation->setStartValue(viewportRectInContentCoords);
    m_scaleAnimation->setEndValue(contentRect);

    // Inform the web process about the requested visible content rect
    // if zooming-out so that no white tiles are exposed during animation.
    if (viewportRectInContentCoords.width() / contentRect.width() < m_pageItem->contentsScale())
        m_controller->setVisibleContentsRect(contentRect, viewportScaleForRect(contentRect));

    m_scaleAnimation->start();
}

void PageViewportControllerClientQt::flickMoveStarted()
{
    Q_ASSERT(m_viewportItem->isMoving());
    m_scrollUpdateDeferrer.reset(new ViewportUpdateDeferrer(m_controller, ViewportUpdateDeferrer::DeferUpdateAndSuspendContent));

    m_lastScrollPosition = m_viewportItem->contentPos();

    m_ignoreViewportChanges = false;
}

void PageViewportControllerClientQt::flickMoveEnded()
{
    Q_ASSERT(!m_viewportItem->isMoving());
    // This method is called on the end of the pan or pan kinetic animation.

    m_ignoreViewportChanges = true;

    m_scrollUpdateDeferrer.reset();
}

void PageViewportControllerClientQt::pageItemPositionChanged()
{
    if (m_ignoreViewportChanges)
        return;

    QPointF newPosition = m_viewportItem->contentPos();

    updateViewportController(m_lastScrollPosition - newPosition);

    m_lastScrollPosition = newPosition;
}

void PageViewportControllerClientQt::scaleAnimationStateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State /*oldState*/)
{
    switch (newState) {
    case QAbstractAnimation::Running:
        m_viewportItem->cancelFlick();
        ASSERT(!m_animationUpdateDeferrer);
        m_animationUpdateDeferrer.reset(new ViewportUpdateDeferrer(m_controller, ViewportUpdateDeferrer::DeferUpdateAndSuspendContent));
        break;
    case QAbstractAnimation::Stopped:
        m_animationUpdateDeferrer.reset();
        break;
    default:
        break;
    }
}

void PageViewportControllerClientQt::touchBegin()
{
    m_controller->setHadUserInteraction(true);

    // Prevents resuming the page between the user's flicks of the page while the animation is running.
    if (scrollAnimationActive())
        m_touchUpdateDeferrer.reset(new ViewportUpdateDeferrer(m_controller, ViewportUpdateDeferrer::DeferUpdateAndSuspendContent));
}

void PageViewportControllerClientQt::touchEnd()
{
    m_touchUpdateDeferrer.reset();
}

void PageViewportControllerClientQt::focusEditableArea(const QRectF& caretArea, const QRectF& targetArea)
{
    // This can only happen as a result of a user interaction.
    ASSERT(m_controller->hadUserInteraction());

    QRectF endArea = m_controller->convertToViewport(targetArea);

    qreal endItemScale = m_controller->convertToViewport(m_controller->innerBoundedContentsScale(2.0));
    const QRectF viewportRect = m_viewportItem->boundingRect();

    qreal x;
    const qreal borderOffset = 10;
    if ((endArea.width() + borderOffset) * endItemScale <= viewportRect.width()) {
        // Center the input field in the middle of the view, if it is smaller than
        // the view at the scale target.
        x = viewportRect.center().x() - endArea.width() * endItemScale / 2.0;
    } else {
        // Ensure that the caret always has borderOffset contents pixels to the right
        // of it, and secondarily (if possible), that the area has borderOffset
        // contents pixels to the left of it.
        qreal caretOffset = m_controller->convertToViewport(caretArea.x()) - endArea.x();
        x = qMin(viewportRect.width() - (caretOffset + borderOffset) * endItemScale, borderOffset * endItemScale);
    }

    const QPointF hotspot = QPointF(endArea.x(), endArea.center().y());
    const QPointF viewportHotspot = QPointF(x, /* FIXME: visibleCenter */ viewportRect.center().y());

    QPointF endPosition = hotspot * endItemScale - viewportHotspot;
    QRectF endPosRange = m_controller->positionRangeForContentAtScale(endItemScale);

    endPosition = boundPosition(endPosRange.topLeft(), endPosition, endPosRange.bottomRight());

    QRectF endVisibleContentRect(endPosition / endItemScale, viewportRect.size() / endItemScale);

    animateContentRectVisible(endVisibleContentRect);
}

void PageViewportControllerClientQt::zoomToAreaGestureEnded(const QPointF& touchPoint, const QRectF& targetArea)
{
    // This can only happen as a result of a user interaction.
    ASSERT(m_controller->hadUserInteraction());

    if (!targetArea.isValid())
        return;

    if (m_controller->hasSuspendedContent())
        return;

    const int margin = 10; // We want at least a little bit of margin.
    QRectF endArea = m_controller->convertToViewport(targetArea.adjusted(-margin, -margin, margin, margin));

    const QRectF viewportRect = m_viewportItem->boundingRect();

    qreal targetCSSScale = viewportRect.size().width() / endArea.size().width();
    qreal endCSSScale = m_controller->innerBoundedContentsScale(qMin(targetCSSScale, qreal(2.5)));
    qreal endItemScale = m_controller->convertToViewport(endCSSScale);
    qreal currentScale = m_pageItem->contentsScale();

    // We want to end up with the target area filling the whole width of the viewport (if possible),
    // and centralized vertically where the user requested zoom. Thus our hotspot is the center of
    // the targetArea x-wise and the requested zoom position, y-wise.
    const QPointF hotspot = QPointF(endArea.center().x(), m_controller->convertToViewport(touchPoint.y()));
    const QPointF viewportHotspot = viewportRect.center();

    QPointF endPosition = hotspot * endCSSScale - viewportHotspot;

    QRectF endPosRange = m_controller->positionRangeForContentAtScale(endItemScale);
    endPosition = boundPosition(endPosRange.topLeft(), endPosition, endPosRange.bottomRight());

    QRectF endVisibleContentRect(endPosition / endItemScale, viewportRect.size() / endItemScale);

    enum { ZoomIn, ZoomBack, ZoomOut, NoZoom } zoomAction = ZoomIn;

    if (!m_scaleStack.isEmpty()) {
        // Zoom back out if attempting to scale to the same current scale, or
        // attempting to continue scaling out from the inner most level.
        // Use fuzzy compare with a fixed error to be able to deal with largish differences due to pixel rounding.
        if (fuzzyCompare(endItemScale, currentScale, 0.01)) {
            // If moving the viewport would expose more of the targetRect and move at least 40 pixels, update position but do not scale out.
            QRectF currentContentRect(visibleContentsRect());
            QRectF targetIntersection = endVisibleContentRect.intersected(targetArea);
            if (!currentContentRect.contains(targetIntersection)
                    && (qAbs(endVisibleContentRect.top() - currentContentRect.top()) >= 40
                    || qAbs(endVisibleContentRect.left() - currentContentRect.left()) >= 40))
                zoomAction = NoZoom;
            else
                zoomAction = ZoomBack;
        } else if (fuzzyCompare(endItemScale, m_zoomOutScale, 0.01))
            zoomAction = ZoomBack;
        else if (endItemScale < currentScale)
            zoomAction = ZoomOut;
    }

    switch (zoomAction) {
    case ZoomIn:
        m_scaleStack.append(ScaleStackItem(currentScale, m_viewportItem->contentPos().x()));
        m_zoomOutScale = endItemScale;
        break;
    case ZoomBack: {
        ScaleStackItem lastScale = m_scaleStack.takeLast();
        endItemScale = lastScale.scale;
        endCSSScale = m_controller->convertFromViewport(lastScale.scale);
        // Recalculate endPosition and bound it according to new scale.
        endPosition.setY(hotspot.y() * endCSSScale - viewportHotspot.y());
        endPosition.setX(lastScale.xPosition);
        endPosRange = m_controller->positionRangeForContentAtScale(endItemScale);
        endPosition = boundPosition(endPosRange.topLeft(), endPosition, endPosRange.bottomRight());
        endVisibleContentRect = QRectF(endPosition / endItemScale, viewportRect.size() / endItemScale);
        break;
    }
    case ZoomOut:
        // Unstack all scale-levels deeper than the new level, so a zoom-back won't end up zooming in.
        while (!m_scaleStack.isEmpty() && m_scaleStack.last().scale >= endItemScale)
            m_scaleStack.removeLast();
        m_zoomOutScale = endItemScale;
        break;
    case NoZoom:
        break;
    }

    animateContentRectVisible(endVisibleContentRect);
}

QRectF PageViewportControllerClientQt::nearestValidVisibleContentsRect() const
{
    float cssScale = m_controller->convertFromViewport(m_pageItem->contentsScale());
    float endItemScale = m_controller->convertToViewport(m_controller->innerBoundedContentsScale(cssScale));

    const QRectF viewportRect = m_viewportItem->boundingRect();
    QPointF viewportHotspot = viewportRect.center();
    QPointF endPosition = m_viewportItem->mapToWebContent(viewportHotspot) * endItemScale - viewportHotspot;

    FloatRect endPosRange = m_controller->positionRangeForContentAtScale(endItemScale);
    endPosition = boundPosition(endPosRange.minXMinYCorner(), endPosition, endPosRange.maxXMaxYCorner());

    QRectF endVisibleContentRect(endPosition / endItemScale, viewportRect.size() / endItemScale);

    return endVisibleContentRect;
}

void PageViewportControllerClientQt::setContentsPosition(const FloatPoint& localPoint)
{
    QPointF newPosition(m_pageItem->pos() + QPointF(localPoint));
    m_viewportItem->setContentPos(newPosition);
    updateViewportController();
}

void PageViewportControllerClientQt::setContentsScale(float localScale, bool treatAsInitialValue)
{
    if (treatAsInitialValue) {
        m_zoomOutScale = 0;
        m_scaleStack.clear();
        setContentRectVisiblePositionAtScale(QPointF(), localScale);
    } else
        scaleContent(localScale);

    updateViewportController();
}

void PageViewportControllerClientQt::setContentsRectToNearestValidBounds()
{
    ViewportUpdateDeferrer guard(m_controller);
    float validCSSScale = m_controller->innerBoundedContentsScale(m_controller->convertFromViewport(m_pageItem->contentsScale()));
    setContentRectVisiblePositionAtScale(nearestValidVisibleContentsRect().topLeft(), m_controller->convertToViewport(validCSSScale));
}

void PageViewportControllerClientQt::didResumeContent()
{
    updateViewportController();
}

bool PageViewportControllerClientQt::scrollAnimationActive() const
{
    return m_viewportItem->isFlicking();
}

bool PageViewportControllerClientQt::panGestureActive() const
{
    return m_controller->hadUserInteraction() && m_viewportItem->isDragging();
}

void PageViewportControllerClientQt::panGestureStarted(const QPointF& position, qint64 eventTimestampMillis)
{
    // This can only happen as a result of a user interaction.
    ASSERT(m_controller->hadUserInteraction());

    m_viewportItem->handleFlickableMousePress(position, eventTimestampMillis);
    m_lastPinchCenterInViewportCoordinates = position;
}

void PageViewportControllerClientQt::panGestureRequestUpdate(const QPointF& position, qint64 eventTimestampMillis)
{
    m_viewportItem->handleFlickableMouseMove(position, eventTimestampMillis);
    m_lastPinchCenterInViewportCoordinates = position;
}

void PageViewportControllerClientQt::panGestureEnded(const QPointF& position, qint64 eventTimestampMillis)
{
    m_viewportItem->handleFlickableMouseRelease(position, eventTimestampMillis);
    m_lastPinchCenterInViewportCoordinates = position;
}

void PageViewportControllerClientQt::panGestureCancelled()
{
    // Reset the velocity samples of the flickable.
    // This should only be called by the recognizer if we have a recognized
    // pan gesture and receive a touch event with multiple touch points
    // (ie. transition to a pinch gesture) as it does not move the content
    // back inside valid bounds.
    // When the pinch gesture ends, the content is positioned and scaled
    // back to valid boundaries.
    m_viewportItem->cancelFlick();
}

bool PageViewportControllerClientQt::scaleAnimationActive() const
{
    return m_scaleAnimation->state() == QAbstractAnimation::Running;
}

void PageViewportControllerClientQt::cancelScrollAnimation()
{
    if (!scrollAnimationActive())
        return;

    // If the pan gesture recognizer receives a touch begin event
    // during an ongoing kinetic scroll animation of a previous
    // pan gesture, the animation is stopped and the content is
    // immediately positioned back to valid boundaries.

    m_viewportItem->cancelFlick();
    setContentsRectToNearestValidBounds();
}

void PageViewportControllerClientQt::interruptScaleAnimation()
{
    // This interrupts the scale animation exactly where it is, even if it is out of bounds.
    m_scaleAnimation->stop();
}

bool PageViewportControllerClientQt::pinchGestureActive() const
{
    return m_controller->hadUserInteraction() && (m_pinchStartScale > 0);
}

void PageViewportControllerClientQt::pinchGestureStarted(const QPointF& pinchCenterInViewportCoordinates)
{
    // This can only happen as a result of a user interaction.
    ASSERT(m_controller->hadUserInteraction());

    if (!m_controller->allowsUserScaling())
        return;

    m_scaleStack.clear();
    m_zoomOutScale = 0.0;

    m_scaleUpdateDeferrer.reset(new ViewportUpdateDeferrer(m_controller, ViewportUpdateDeferrer::DeferUpdateAndSuspendContent));

    m_lastPinchCenterInViewportCoordinates = pinchCenterInViewportCoordinates;
    m_pinchStartScale = m_pageItem->contentsScale();
}

void PageViewportControllerClientQt::pinchGestureRequestUpdate(const QPointF& pinchCenterInViewportCoordinates, qreal totalScaleFactor)
{
    ASSERT(m_controller->hasSuspendedContent());

    if (!m_controller->allowsUserScaling())
        return;

    //  Changes of the center position should move the page even if the zoom factor does not change.
    const qreal cssScale = m_controller->convertFromViewport(m_pinchStartScale * totalScaleFactor);

    // Allow zooming out beyond mimimum scale on pages that do not explicitly disallow it.
    const qreal targetItemScale = m_controller->convertToViewport(m_controller->outerBoundedContentsScale(cssScale));

    scaleContent(targetItemScale, m_viewportItem->mapToWebContent(pinchCenterInViewportCoordinates));

    const QPointF positionDiff = pinchCenterInViewportCoordinates - m_lastPinchCenterInViewportCoordinates;
    m_lastPinchCenterInViewportCoordinates = pinchCenterInViewportCoordinates;

    m_viewportItem->setContentPos(m_viewportItem->contentPos() - positionDiff);

    // Inform the web process to render the currently visible area with low-resolution tiles not
    // to expose white tiles during pinch gestures and to show fixed position layers correctly.
    // The actual scale is restored after the pinch gesture ends.
    updateViewportController(QPointF(), 1);
}

void PageViewportControllerClientQt::pinchGestureEnded()
{
    ASSERT(m_controller->hasSuspendedContent());

    if (!m_controller->allowsUserScaling())
        return;

    m_pinchStartScale = -1;

    animateContentRectVisible(nearestValidVisibleContentsRect());
    m_scaleUpdateDeferrer.reset(); // Clear after starting potential animation, which takes over deferring.
}

void PageViewportControllerClientQt::pinchGestureCancelled()
{
    m_pinchStartScale = -1;
    m_scaleUpdateDeferrer.reset();
}

QRectF PageViewportControllerClientQt::visibleContentsRect() const
{
    const QRectF visibleRect(m_viewportItem->boundingRect().intersected(m_pageItem->boundingRect()));
    return m_viewportItem->mapRectToWebContent(visibleRect);
}

void PageViewportControllerClientQt::didChangeContentsSize()
{
    // Emit for testing purposes, so that it can be verified that
    // we didn't do scale adjustment.
    emit m_viewportItem->experimental()->test()->contentsScaleCommitted();

    if (!m_controller->hasSuspendedContent())
        setContentsRectToNearestValidBounds();
}

void PageViewportControllerClientQt::didChangeVisibleContents()
{
    qreal scale = m_pageItem->contentsScale();

    if (scale != m_lastCommittedScale)
        emit m_viewportItem->experimental()->test()->contentsScaleCommitted();
    m_lastCommittedScale = scale;

    // Ensure that updatePaintNode is always called before painting.
    m_pageItem->update();
}

void PageViewportControllerClientQt::didChangeViewportAttributes()
{
    // Make sure we apply the new initial scale when deferring ends.
    ViewportUpdateDeferrer guard(m_controller);

    emit m_viewportItem->experimental()->test()->devicePixelRatioChanged();
    emit m_viewportItem->experimental()->test()->viewportChanged();
}

void PageViewportControllerClientQt::updateViewportController(const QPointF& trajectory, qreal scale)
{
    FloatRect currentVisibleRect(visibleContentsRect());
    float viewportScale = (scale < 0) ? viewportScaleForRect(currentVisibleRect) : scale;
    m_controller->setVisibleContentsRect(currentVisibleRect, viewportScale, trajectory);
}

void PageViewportControllerClientQt::scaleContent(qreal itemScale, const QPointF& centerInCSSCoordinates)
{
    QPointF oldPinchCenterOnViewport = m_viewportItem->mapFromWebContent(centerInCSSCoordinates);
    m_pageItem->setContentsScale(itemScale);
    QPointF newPinchCenterOnViewport = m_viewportItem->mapFromWebContent(centerInCSSCoordinates);
    m_viewportItem->setContentPos(m_viewportItem->contentPos() + (newPinchCenterOnViewport - oldPinchCenterOnViewport));
}

float PageViewportControllerClientQt::viewportScaleForRect(const QRectF& rect) const
{
    return static_cast<float>(m_viewportItem->width()) / static_cast<float>(rect.width());
}

} // namespace WebKit

#include "moc_PageViewportControllerClientQt.cpp"
