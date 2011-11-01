/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef qtouchwebview_p_h
#define qtouchwebview_p_h

#include "QtTouchViewInterface.h"
#include "QtTouchWebPageProxy.h"
#include "QtViewportInteractionEngine.h"
#include "qbasewebview_p.h"
#include <QScopedPointer>

class QTouchWebPage;
class QTouchWebView;

class QTouchWebViewPrivate : QBaseWebViewPrivate
{
    Q_DECLARE_PUBLIC(QTouchWebView)
public:
    void init(QTouchWebView* viewport);

    void loadDidCommit();
    void _q_viewportUpdated();
    void _q_viewportTrajectoryVectorChanged(const QPointF&);
    void updateViewportConstraints();
    QtTouchWebPageProxy* touchPageProxy() { return static_cast<QtTouchWebPageProxy*>(QBaseWebViewPrivate::pageProxy.data()); }

    void didChangeViewportProperties(const WebCore::ViewportArguments& args);

    QScopedPointer<QTouchWebPage> pageView;
    QScopedPointer<WebKit::QtTouchViewInterface> viewInterface;
    QScopedPointer<QtViewportInteractionEngine> interactionEngine;

    WebCore::ViewportArguments viewportArguments;
};

#endif /* qtouchwebview_p_h */
