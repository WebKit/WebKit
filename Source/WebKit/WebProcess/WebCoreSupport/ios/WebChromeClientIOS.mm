/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebChromeClient.h"

#if PLATFORM(IOS_FAMILY)

#import "DrawingArea.h"
#import "InteractionInformationAtPosition.h"
#import "InteractionInformationRequest.h"
#import "MessageSenderInlines.h"
#import "UIKitSPI.h"
#import "WebCoreArgumentCoders.h"
#import "WebFrame.h"
#import "WebIconUtilities.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/AudioSession.h>
#import <WebCore/ContentChangeObserver.h>
#import <WebCore/Icon.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformMouseEvent.h>
#import <wtf/RefPtr.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(IOS_TOUCH_EVENTS)

void WebChromeClient::didPreventDefaultForEvent()
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page().mainFrame());
    if (!localMainFrame)
        return;
    ContentChangeObserver::didPreventDefaultForEvent(*localMainFrame);
}

#endif

void WebChromeClient::didReceiveMobileDocType(bool isMobileDoctype)
{
    protectedPage()->didReceiveMobileDocType(isMobileDoctype);
}

void WebChromeClient::setNeedsScrollNotifications(WebCore::LocalFrame&, bool)
{
    notImplemented();
}

void WebChromeClient::didFinishContentChangeObserving(WebCore::LocalFrame&, WKContentChange observedContentChange)
{
    protectedPage()->didFinishContentChangeObserving(observedContentChange);
}

void WebChromeClient::notifyRevealedSelectionByScrollingFrame(WebCore::LocalFrame&)
{
    protectedPage()->didScrollSelection();
}

bool WebChromeClient::isStopping()
{
    notImplemented();
    return false;
}

void WebChromeClient::didLayout(LayoutType type)
{
    if (type == Scroll)
        protectedPage()->didScrollSelection();
}

void WebChromeClient::didStartOverflowScroll()
{
    // FIXME: This is only relevant for legacy touch-driven overflow in the web process (see ScrollAnimatorIOS::handleTouchEvent), and should be removed.
    protectedPage()->send(Messages::WebPageProxy::ScrollingNodeScrollWillStartScroll(0));
}

void WebChromeClient::didEndOverflowScroll()
{
    // FIXME: This is only relevant for legacy touch-driven overflow in the web process (see ScrollAnimatorIOS::handleTouchEvent), and should be removed.
    protectedPage()->send(Messages::WebPageProxy::ScrollingNodeScrollDidEndScroll(0));
}

bool WebChromeClient::hasStablePageScaleFactor() const
{
    return protectedPage()->hasStablePageScaleFactor();
}

void WebChromeClient::suppressFormNotifications()
{
    notImplemented();
}

void WebChromeClient::restoreFormNotifications()
{
    notImplemented();
}

void WebChromeClient::addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool, bool)
{
    notImplemented();
}

void WebChromeClient::removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*)
{
    notImplemented();
}

void WebChromeClient::webAppOrientationsUpdated()
{
    notImplemented();
}

void WebChromeClient::showPlaybackTargetPicker(bool hasVideo, WebCore::RouteSharingPolicy policy, const String& routingContextUID)
{
    auto page = protectedPage();
    page->send(Messages::WebPageProxy::ShowPlaybackTargetPicker(hasVideo, page->rectForElementAtInteractionLocation(), policy, routingContextUID));
}

Seconds WebChromeClient::eventThrottlingDelay()
{
    return protectedPage()->eventThrottlingDelay();
}

#if ENABLE(ORIENTATION_EVENTS)
IntDegrees WebChromeClient::deviceOrientation() const
{
    return protectedPage()->deviceOrientation();
}
#endif

bool WebChromeClient::shouldUseMouseEventForSelection(const WebCore::PlatformMouseEvent& event)
{
    // In iPadOS and macCatalyst, despite getting mouse events, we still want UITextInteraction and friends to own selection gestures.
    // However, we need to allow single-clicks to set the selection, because that is how UITextInteraction is activated.
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    return event.clickCount() <= 1;
#else
    return true;
#endif
}

bool WebChromeClient::showDataDetectorsUIForElement(const Element& element, const Event& event)
{
    if (!event.isMouseEvent())
        return false;

    // FIXME: Ideally, we would be able to generate InteractionInformationAtPosition without re-hit-testing the element.
    auto& mouseEvent = downcast<MouseEvent>(event);
    auto request = InteractionInformationRequest { roundedIntPoint(mouseEvent.locationInRootViewCoordinates()) };
    request.includeLinkIndicator = true;
    auto page = protectedPage();
    auto positionInformation = page->positionInformation(request);
    page->send(Messages::WebPageProxy::ShowDataDetectorsUIForPositionInformation(positionInformation));
    return true;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
