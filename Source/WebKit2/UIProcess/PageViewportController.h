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

    float innerBoundedViewportScale(float) const;
    float outerBoundedViewportScale(float) const;
    WebCore::FloatPoint clampViewportToContents(const WebCore::FloatPoint& viewportPos, float viewportScale);

    bool hasSuspendedContent() const { return m_hasSuspendedContent; }
    bool hadUserInteraction() const { return m_hadUserInteraction; }
    bool allowsUserScaling() const { return m_allowsUserScaling; }

    WebCore::FloatSize contentsLayoutSize() const { return m_rawAttributes.layoutSize; }
    float devicePixelRatio() const;
    float minimumContentsScale() const { return m_minimumScaleToFit; }
    float maximumContentsScale() const { return m_rawAttributes.maximumScale; }
    float currentContentsScale() const { return fromViewportScale(m_effectiveScale); }

    void setHadUserInteraction(bool didUserInteract) { m_hadUserInteraction = didUserInteract; }

    // Notifications from the viewport.
    void didChangeViewportSize(const WebCore::FloatSize& newSize);
    void didChangeContentsVisibility(const WebCore::FloatPoint& viewportPos, float viewportScale, const WebCore::FloatPoint& trajectoryVector = WebCore::FloatPoint::zero());

    // Notifications from the WebProcess.
    void didChangeContentsSize(const WebCore::IntSize& newSize);
    void didChangeViewportAttributes(const WebCore::ViewportAttributes&);
    void pageTransitionViewportReady();
    void pageDidRequestScroll(const WebCore::IntPoint& cssPosition);

private:
    float fromViewportScale(float scale) const { return scale / devicePixelRatio(); }
    float toViewportScale(float scale) const { return scale * devicePixelRatio(); }
    void syncVisibleContents(const WebCore::FloatPoint &trajectoryVector = WebCore::FloatPoint::zero());
    void updateMinimumScaleToFit();

    WebPageProxy* const m_webPageProxy;
    PageViewportControllerClient* m_client;

    WebCore::ViewportAttributes m_rawAttributes;

    bool m_allowsUserScaling;
    float m_minimumScaleToFit;

    int m_activeDeferrerCount;
    bool m_hasSuspendedContent;
    bool m_hadUserInteraction;

    WebCore::FloatPoint m_viewportPos;
    WebCore::FloatSize m_viewportSize;
    WebCore::FloatSize m_contentsSize;
    float m_effectiveScale; // Should always be cssScale * devicePixelRatio.

    friend class ViewportUpdateDeferrer;
};

bool fuzzyCompare(float, float, float epsilon);

} // namespace WebKit

#endif // PageViewportController_h
