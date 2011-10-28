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

#ifndef QtTouchWebPageProxy_h
#define QtTouchWebPageProxy_h


#include "DrawingAreaProxy.h"
#include "QtPanGestureRecognizer.h"
#include "QtPinchGestureRecognizer.h"
#include "QtTouchViewInterface.h"
#include "QtWebPageProxy.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {
class QtViewportInteractionEngine;
}

using namespace WebKit;

class QtTouchWebPageProxy : public QtWebPageProxy {
public:
    QtTouchWebPageProxy(QtTouchViewInterface*, QtViewportInteractionEngine*);

    virtual bool handleEvent(QEvent*);

    void setVisibleContentRectAndScale(const QRectF&, float);
    void setVisibleContentRectTrajectoryVector(const QPointF&);
    void setResizesToContentsUsingLayoutSize(const QSize& targetLayoutSize);
    void findZoomableAreaForPoint(const QPoint&);
    void renderNextFrame();
    void renderToCurrentGLContext(const WebCore::TransformationMatrix&, float);

protected:
    virtual void paintContent(QPainter*, const QRect& area);

private:
    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();
    virtual void processDidCrash();
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled);
#endif

    QtTouchViewInterface* touchViewInterface() const { return static_cast<QtTouchViewInterface*>(m_viewInterface); }

    void touchEvent(QTouchEvent*);

    QtPanGestureRecognizer m_panGestureRecognizer;
    QtPinchGestureRecognizer m_pinchGestureRecognizer;
};

#endif // QtTouchWebPageProxy_h
