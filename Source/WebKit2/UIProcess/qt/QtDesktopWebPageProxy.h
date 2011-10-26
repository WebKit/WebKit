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

#ifndef QtDesktopWebPageProxy_h
#define QtDesktopWebPageProxy_h

#include "DrawingAreaProxy.h"
#include "QtWebPageProxy.h"
#include <wtf/PassOwnPtr.h>

class QDesktopWebViewPrivate;

using namespace WebKit;

class QtDesktopWebPageProxy : public QtWebPageProxy {
public:
    QtDesktopWebPageProxy(QDesktopWebViewPrivate*, WKContextRef, WKPageGroupRef = 0);

    virtual bool handleEvent(QEvent*);

protected:
    void paintContent(QPainter*, const QRect& area);

private:
    /* PageClient overrides. */
    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();
    virtual void didChangeViewportProperties(const WebCore::ViewportArguments&);
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled);
#endif

    virtual PassRefPtr<WebKit::WebPopupMenuProxy> createPopupMenuProxy(WebKit::WebPageProxy*);

    virtual void timerEvent(QTimerEvent*);

    bool handleMouseMoveEvent(QMouseEvent*);
    bool handleMousePressEvent(QMouseEvent*);
    bool handleMouseReleaseEvent(QMouseEvent*);
    bool handleMouseDoubleClickEvent(QMouseEvent*);
    bool handleWheelEvent(QWheelEvent*);
    bool handleHoverLeaveEvent(QHoverEvent*);
    bool handleHoverMoveEvent(QHoverEvent*);
    bool handleDragEnterEvent(QDragEnterEvent*);
    bool handleDragLeaveEvent(QDragLeaveEvent*);
    bool handleDragMoveEvent(QDragMoveEvent*);
    bool handleDropEvent(QDropEvent*);

    QPoint m_tripleClick;
    QBasicTimer m_tripleClickTimer;
};

#endif // QtDesktopWebPageProxy_h
