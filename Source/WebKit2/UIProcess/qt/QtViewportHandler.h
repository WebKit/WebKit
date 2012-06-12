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

#ifndef QtViewportHandler_h
#define QtViewportHandler_h

#include <QtCore/QObject>
#include <QtCore/QRectF>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <WebCore/ViewportArguments.h>
#include <wtf/OwnPtr.h>

QT_BEGIN_NAMESPACE
class QPointF;
class QQuickWebPage;
class QQuickWebView;
class QWheelEvent;
QT_END_NAMESPACE

class QWebKitTest;

namespace WebKit {

class WebPageProxy;
class ViewportUpdateDeferrer;

class QtViewportHandler : public QObject {
    Q_OBJECT

public:
    QtViewportHandler(WebPageProxy*, QQuickWebView*, QQuickWebPage*);
    ~QtViewportHandler();

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

    void pageContentPositionRequested(const QPoint& position);

    void viewportItemSizeChanged();
    void viewportAttributesChanged(const WebCore::ViewportAttributes&);
    void informVisibleContentChange(const QPointF& trajectory = QPointF());
    void pageContentsSizeChanged(const QSize& newSize, const QSize& viewportSize);

private Q_SLOTS:
    // Respond to changes of position that are not driven by us.
    void pageItemPositionChanged();

    void scaleAnimationStateChanged(QAbstractAnimation::State, QAbstractAnimation::State);
    void scaleAnimationValueChanged(QVariant value);

    void flickMoveStarted(); // Called when panning starts.
    void flickMoveEnded(); //   Called when panning (+ kinetic animation) ends.

private:
    friend class ViewportUpdateDeferrer;
    friend class ::QWebKitTest;

    WebPageProxy* const m_webPageProxy;
    QQuickWebView* const m_viewportItem;
    QQuickWebPage* const m_pageItem;

    qreal cssScaleFromItem(qreal) const;
    qreal itemScaleFromCSS(qreal) const;
    qreal itemCoordFromCSS(qreal) const;
    QRectF itemRectFromCSS(const QRectF&) const;

    qreal innerBoundedCSSScale(qreal) const;
    qreal outerBoundedCSSScale(qreal) const;

    void setInitialScaleIfNeeded();

    void setCSSScale(qreal);
    qreal currentCSSScale() const;

    void setPageItemRectVisible(const QRectF&);
    void animatePageItemRectVisible(const QRectF&);

    QRect visibleContentsRect() const;
    QRectF initialRect() const;
    QRectF nearestValidBounds() const;

    QRectF computePosRangeForPageItemAtScale(qreal itemScale) const;
    void scaleContent(const QPointF& centerInCSSCoordinates, qreal cssScale);

    void suspendPageContent();
    void resumePageContent();

    WebCore::ViewportAttributes m_rawAttributes;

    bool m_allowsUserScaling;
    qreal m_minimumScale;
    qreal m_maximumScale;
    qreal m_devicePixelRatio;

    QSize m_layoutSize;

    int m_suspendCount;
    bool m_hasSuspendedContent;

    OwnPtr<ViewportUpdateDeferrer> m_scaleUpdateDeferrer;
    OwnPtr<ViewportUpdateDeferrer> m_scrollUpdateDeferrer;
    OwnPtr<ViewportUpdateDeferrer> m_touchUpdateDeferrer;
    OwnPtr<ViewportUpdateDeferrer> m_animationUpdateDeferrer;

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
    qreal m_lastCommittedScale;
    QRectF m_lastVisibleContentsRect;
    qreal m_zoomOutScale;
    QList<ScaleStackItem> m_scaleStack;
};

} // namespace WebKit

#endif // QtViewportHandler_h
