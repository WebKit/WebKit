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

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/ViewportArguments.h>

namespace WebCore {
class IntPoint;
class IntSize;
}

using namespace WebCore;

namespace WebKit {

class WebPageProxy;
class PageViewportController;
class PageViewportControllerClient;

// When interacting with the content, either by animating or by the hand of the user,
// it is important to ensure smooth animations of at least 60fps in order to give a
// good user experience.
//
// In order to do this we need to get rid of unknown factors. These include device
// sensors (geolocation, orientation updates etc), CSS3 animations, JavaScript
// exectution, sub resource loads etc.
// We do this by sending suspend and resume notifications to the web process.
//
// For this purpose the ViewportUpdateDeferrer guard can be used when interacting
// with or animating the content to scope suspend / resume and defer update
// notifications.
//
// If something should only be executed when the content is suspended, it is possible
// to check for that using ASSERT(hasSuspendedContent()).

class ViewportUpdateDeferrer {
public:
    enum SuspendContentFlag { DeferUpdate, DeferUpdateAndSuspendContent };
    ViewportUpdateDeferrer(PageViewportController*, SuspendContentFlag = DeferUpdate);
    ~ViewportUpdateDeferrer();

private:
    PageViewportController* const m_controller;
};

class PageViewportController {
    WTF_MAKE_NONCOPYABLE(PageViewportController);

public:
    PageViewportController(WebKit::WebPageProxy*, PageViewportControllerClient*);
    virtual ~PageViewportController() { }

    void suspendContent();
    void resumeContent();

    FloatRect positionRangeForContentAtScale(float viewportScale) const;

    float convertFromViewport(float value) const { return value / m_devicePixelRatio; }
    float convertToViewport(float value) const { return value * m_devicePixelRatio; }
    FloatRect convertToViewport(const FloatRect&) const;

    float innerBoundedContentsScale(float) const;
    float outerBoundedContentsScale(float) const;

    bool hasSuspendedContent() const { return m_hasSuspendedContent; }
    bool hadUserInteraction() const { return m_hadUserInteraction; }
    bool allowsUserScaling() const { return m_allowsUserScaling; }

    FloatSize contentsLayoutSize() const { return m_rawAttributes.layoutSize; }
    float devicePixelRatio() const { return m_devicePixelRatio; }
    float minimumContentsScale() const { return m_minimumScale; }
    float maximumContentsScale() const { return m_maximumScale; }
    float currentContentsScale() const { return convertFromViewport(m_effectiveScale); }

    void setHadUserInteraction(bool didUserInteract) { m_hadUserInteraction = didUserInteract; }

    // Notifications to the WebProcess.
    void setViewportSize(const FloatSize& newSize);
    void setVisibleContentsRect(const FloatRect& visibleContentsRect, float viewportScale, const FloatPoint& trajectoryVector = FloatPoint::zero());

    // Notifications from the WebProcess.
    void didChangeContentsSize(const IntSize& newSize);
    void didChangeViewportAttributes(const ViewportAttributes&);
    void pageDidRequestScroll(const IntPoint& cssPosition);

private:
    void syncVisibleContents(const FloatPoint &trajectoryVector = FloatPoint::zero());

    WebPageProxy* const m_webPageProxy;
    PageViewportControllerClient* m_client;

    ViewportAttributes m_rawAttributes;

    bool m_allowsUserScaling;
    float m_minimumScale;
    float m_maximumScale;
    float m_devicePixelRatio;

    int m_activeDeferrerCount;
    bool m_hasSuspendedContent;
    bool m_hadUserInteraction;

    FloatSize m_viewportSize;
    FloatSize m_contentsSize;
    FloatRect m_visibleContentsRect;
    float m_effectiveScale; // Should always be cssScale * devicePixelRatio.

    friend class ViewportUpdateDeferrer;
};

bool fuzzyCompare(float, float, float epsilon);
FloatPoint boundPosition(const FloatPoint minPosition, const FloatPoint& position, const FloatPoint& maxPosition);

} // namespace WebKit

#endif // PageViewportController_h
