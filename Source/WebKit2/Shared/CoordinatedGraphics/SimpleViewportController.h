/*
 * Copyright (C) 2011, 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Benjamin Poulain <benjamin@webkit.org>
 * Copyright (C) 2014 Igalia S.L.
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
 */

#ifndef SimpleViewportController_h
#define SimpleViewportController_h

#if USE(COORDINATED_GRAPHICS_THREADED)

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/ViewportArguments.h>
#include <wtf/Noncopyable.h>

namespace WebKit {

class SimpleViewportController {
    WTF_MAKE_NONCOPYABLE(SimpleViewportController);
public:
    class Client {
    public:
        virtual void didChangeVisibleRect() = 0;
    };

    explicit SimpleViewportController(Client*);

    void didChangeViewportSize(const WebCore::FloatSize&);
    void didChangeContentsSize(const WebCore::IntSize&);
    void didChangeViewportAttribute(const WebCore::ViewportAttributes&);

    void scrollBy(const WebCore::IntSize&);
    void scrollTo(const WebCore::IntPoint&);

    WebCore::FloatRect visibleContentsRect() const;
    float pageScaleFactor() const { return m_pageScaleFactor; }

private:

    WebCore::FloatSize visibleContentsSize() const;

    void syncVisibleContents();

    void applyScaleAfterRenderingContents(float scale);
    void applyPositionAfterRenderingContents(const WebCore::FloatPoint& pos);

    WebCore::FloatPoint boundContentsPosition(const WebCore::FloatPoint&) const;
    WebCore::FloatPoint boundContentsPositionAtScale(const WebCore::FloatPoint&, float scale) const;

    bool updateMinimumScaleToFit();
    float innerBoundedViewportScale(float) const;

    void resetViewportToDefaultState();

    Client* m_client;

    WebCore::FloatPoint m_contentsPosition;
    WebCore::FloatSize m_contentsSize;
    WebCore::FloatSize m_viewportSize;
    float m_pageScaleFactor;

    bool m_allowsUserScaling;
    float m_minimumScaleToFit;
    bool m_initiallyFitToViewport;

    bool m_hasViewportAttribute;
    WebCore::ViewportAttributes m_rawAttributes;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)

#endif // SimpleViewportController_h
