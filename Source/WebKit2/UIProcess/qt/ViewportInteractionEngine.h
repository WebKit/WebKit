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

#ifndef ViewportInteractionEngine_h
#define ViewportInteractionEngine_h

#include "qwebkitglobal.h"
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE
class QPointF;
class QSGItem;
QT_END_NAMESPACE

namespace WebKit {

class ViewportInteractionEngine : public QObject {
    Q_OBJECT

public:
    ViewportInteractionEngine(const QSGItem*, QSGItem*);

    struct Constraints {
        Constraints()
            : initialScale(1.0)
            , minimumScale(0.25)
            , maximumScale(1.8)
            , isUserScalable(true)
        { }

        qreal initialScale;
        qreal minimumScale;
        qreal maximumScale;
        bool isUserScalable;
    };

    void reset();
    void setConstraints(const Constraints&);

    void panGestureStarted();
    void panGestureRequestUpdate(qreal deltaX, qreal deltaY);
    void panGestureCancelled();
    void panGestureEnded();

    void pinchGestureStarted();
    void pinchGestureRequestUpdate(const QPointF& pinchCenterInContentCoordinate, qreal totalScaleFactor);
    void pinchGestureEnded();

Q_SIGNALS:
    void viewportUpdateRequested();

    void commitScaleChange();

private Q_SLOTS:
    // Respond to changes of content that are not driven by us, like the page resizing itself.
    void contentGeometryChanged();
    void contentScaleChanged();

private:
    void updateContentIfNeeded();
    void updateContentScaleIfNeeded();
    void updateContentPositionIfNeeded();

    void animateContentPositionToBoundaries();
    void animateContentScaleToBoundaries();

    void scaleContent(const QPointF& centerInContentCoordinate, qreal scale);

    friend class ViewportUpdateGuard;

    const QSGItem* const m_viewport;
    QSGItem* const m_content;

    Constraints m_constraints;
    bool m_isUpdatingContent;
    enum UserInteractionFlag {
        UserHasNotInteractedWithContent = 0,
        UserHasMovedContent = 1,
        UserHasScaledContent = 2
    };
    Q_DECLARE_FLAGS(UserInteractionFlags, UserInteractionFlag);
    UserInteractionFlags m_userInteractionFlags;

    qreal m_pinchStartScale;
};

inline bool operator==(const ViewportInteractionEngine::Constraints& a, const ViewportInteractionEngine::Constraints& b)
{
    return a.initialScale == b.initialScale
            && a.minimumScale == b.minimumScale
            && a.maximumScale == b.maximumScale
            && a.isUserScalable == b.isUserScalable;
}

}

#endif /* ViewportInteractionEngine_h */
