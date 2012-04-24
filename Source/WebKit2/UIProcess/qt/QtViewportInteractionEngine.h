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

#ifndef QtViewportInteractionEngine_h
#define QtViewportInteractionEngine_h

#include <QtCore/QObject>
#include <QtCore/QRectF>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <wtf/OwnPtr.h>

QT_BEGIN_NAMESPACE
class QPointF;
class QQuickWebPage;
class QQuickWebView;
class QWheelEvent;
QT_END_NAMESPACE

namespace WebKit {

class ViewportUpdateDeferrer;

class QtViewportInteractionEngine : public QObject {
    Q_OBJECT

public:
    QtViewportInteractionEngine(QQuickWebView*, QQuickWebPage*);
    ~QtViewportInteractionEngine();

    void reset();

    bool hadUserInteraction() const { return m_hadUserInteraction; }

    void setCSSScaleBounds(qreal minimum, qreal maximum);
    void setCSSScale(qreal);

    qreal currentCSSScale();

    bool ensureContentWithinViewportBoundary(bool immediate = false);

    void setAllowsUserScaling(bool allow) { m_allowsUserScaling = allow; }
    void setContentToDevicePixelRatio(qreal ratio) {m_devicePixelRatio = ratio; }

    void setItemRectVisible(const QRectF&);
    bool animateItemRectVisible(const QRectF&);

    void wheelEvent(QWheelEvent*);
    void pagePositionRequest(const QPoint& pos);
    void touchBegin();
    void touchEnd();

    bool scrollAnimationActive() const;
    void cancelScrollAnimation();

    bool panGestureActive() const;

    void panGestureStarted(const QPointF& position, qint64 eventTimestampMillis);
    void panGestureRequestUpdate(const QPointF& position, qint64 eventTimestampMillis);
    void panGestureEnded(const QPointF& position, qint64 eventTimestampMillis);

    void panGestureCancelled();

    bool scaleAnimationActive() const;
    void interruptScaleAnimation();

    bool pinchGestureActive() const;
    void pinchGestureStarted(const QPointF& pinchCenterInViewportCoordinates);
    void pinchGestureRequestUpdate(const QPointF& pinchCenterInViewportCoordinates, qreal totalScaleFactor);
    void pinchGestureEnded();
    void pinchGestureCancelled();

    void zoomToAreaGestureEnded(const QPointF& touchPoint, const QRectF& targetArea);
    void focusEditableArea(const QRectF& caretArea, const QRectF& targetArea);

Q_SIGNALS:
    void contentSuspendRequested();
    void contentResumeRequested();

    void contentViewportChanged(const QPointF& trajectory = QPointF());

    void viewportTrajectoryVectorChanged(const QPointF&);
    void visibleContentRectAndScaleChanged();

private Q_SLOTS:
    // Respond to changes of content that are not driven by us, like the page resizing itself.
    void itemSizeChanged();

    void flickableMovingPositionUpdate();

    void scaleAnimationStateChanged(QAbstractAnimation::State, QAbstractAnimation::State);
    void scaleAnimationValueChanged(QVariant value) { setItemRectVisible(value.toRectF()); }

    void flickableMoveStarted(); // Called when panning starts.
    void flickableMoveEnded(); //   Called when panning (+ kinetic animation) ends.

private:
    friend class ViewportUpdateDeferrer;
    friend class QWebViewportInfo;

    QQuickWebView* const m_viewport;
    QQuickWebPage* const m_content;

    qreal cssScaleFromItem(qreal);
    qreal itemScaleFromCSS(qreal);
    qreal itemCoordFromCSS(qreal);
    QRectF itemRectFromCSS(const QRectF&);

    qreal innerBoundedCSSScale(qreal);
    qreal outerBoundedCSSScale(qreal);

    bool m_allowsUserScaling;
    qreal m_minimumScale;
    qreal m_maximumScale;
    qreal m_devicePixelRatio;

    QSize m_layoutSize;

    QRectF computePosRangeForItemAtScale(qreal itemScale) const;

    void scaleContent(const QPointF& centerInCSSCoordinates, qreal cssScale);

    int m_suspendCount;
    bool m_hasSuspendedContent;
    OwnPtr<ViewportUpdateDeferrer> m_scaleUpdateDeferrer;
    OwnPtr<ViewportUpdateDeferrer> m_scrollUpdateDeferrer;
    OwnPtr<ViewportUpdateDeferrer> m_touchUpdateDeferrer;

    bool m_hadUserInteraction;

    class ScaleAnimation : public QVariantAnimation {
    public:
        ScaleAnimation(QObject* parent = 0)
            : QVariantAnimation(parent)
        { }

        virtual void updateCurrentValue(const QVariant&) { }
    };

    struct ScaleStackItem {
        ScaleStackItem(qreal scale, qreal xPosition)
            : scale(scale)
            , xPosition(xPosition)
        { }

        qreal scale;
        qreal xPosition;
    };

    ScaleAnimation* m_scaleAnimation;
    QPointF m_lastPinchCenterInViewportCoordinates;
    QPointF m_lastScrollPosition;
    qreal m_pinchStartScale;
    qreal m_zoomOutScale;
    QList<ScaleStackItem> m_scaleStack;
};

} // namespace WebKit

#endif // QtViewportInteractionEngine_h
