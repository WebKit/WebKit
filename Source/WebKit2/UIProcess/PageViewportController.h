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

#ifndef PageViewportController_h
#define PageViewportController_h

#if USE(TILED_BACKING_STORE)

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/ViewportArguments.h>

namespace WebCore {
class IntPoint;
class IntSize;
}

namespace WebKit {

class WebPageProxy;
class PageViewportController;
class PageViewportControllerClient;

class PageViewportController {
    WTF_MAKE_NONCOPYABLE(PageViewportController);

public:
    PageViewportController(WebKit::WebPageProxy*, PageViewportControllerClient*);
    virtual ~PageViewportController() { }

    void suspendContent();
    void resumeContent();

    float innerBoundedViewportScale(float) const;
    float outerBoundedViewportScale(float) const;

    WebCore::FloatPoint pixelAlignedFloatPoint(const WebCore::FloatPoint&);

    WebCore::FloatPoint boundContentsPosition(const WebCore::FloatPoint&);
    WebCore::FloatPoint boundContentsPositionAtScale(const WebCore::FloatPoint&, float scale);

    WebCore::FloatSize visibleContentsSize() const;

    bool hasSuspendedContent() const { return m_hasSuspendedContent; }
    bool hadUserInteraction() const { return m_hadUserInteraction; }
    bool allowsUserScaling() const { return m_allowsUserScaling; }

    WebCore::FloatSize contentsLayoutSize() const { return m_rawAttributes.layoutSize; }
    float deviceScaleFactor() const;
    float minimumContentsScale() const { return m_minimumScaleToFit; }
    float maximumContentsScale() const { return m_rawAttributes.maximumScale; }
    float currentContentsScale() const { return m_pageScaleFactor; }

    void setHadUserInteraction(bool didUserInteract) { m_hadUserInteraction = didUserInteract; }

    // Notifications from the viewport.
    void didChangeViewportSize(const WebCore::FloatSize& newSize);
    void didChangeContentsVisibility(const WebCore::FloatPoint&, float scale, const WebCore::FloatPoint& trajectoryVector = WebCore::FloatPoint::zero());

    // Notifications from the WebProcess.
    void didCommitLoad();
    void didChangeContentsSize(const WebCore::IntSize& newSize);
    void didChangeViewportAttributes(const WebCore::ViewportAttributes&);
    void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect);
    void pageTransitionViewportReady();
    void pageDidRequestScroll(const WebCore::IntPoint& cssPosition);

private:
    void syncVisibleContents(const WebCore::FloatPoint &trajectoryVector = WebCore::FloatPoint::zero());
    void applyScaleAfterRenderingContents(float scale);
    void applyPositionAfterRenderingContents(const WebCore::FloatPoint& pos);
    bool updateMinimumScaleToFit(bool userInitiatedUpdate);

    WebPageProxy* const m_webPageProxy;
    PageViewportControllerClient* m_client;

    WebCore::ViewportAttributes m_rawAttributes;

    bool m_allowsUserScaling;
    float m_minimumScaleToFit;
    bool m_initiallyFitToViewport;

    bool m_hasSuspendedContent;
    bool m_hadUserInteraction;

    WebCore::FloatPoint m_contentsPosition;
    WebCore::FloatSize m_contentsSize;
    WebCore::FloatSize m_viewportSize;
    WebCore::IntSize m_clientContentsSize;
    float m_pageScaleFactor;

    bool m_pendingPositionChange;
    bool m_pendingScaleChange;
    WebCore::FloatRect m_lastFrameCoveredRect;
};

bool fuzzyCompare(float, float, float epsilon);

} // namespace WebKit

#endif

#endif // PageViewportController_h
