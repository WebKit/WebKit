/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AXObjectCache.h"

#if PLATFORM(IOS_FAMILY)

#import "AccessibilityObject.h"
#import "Chrome.h"
#import "RenderObject.h"
#import "WebAccessibilityObjectWrapperIOS.h"
#import <wtf/RetainPtr.h>

namespace WebCore {

void AXObjectCache::attachWrapper(AccessibilityObject& object)
{
    RetainPtr<AccessibilityObjectWrapper> wrapper = adoptNS([[WebAccessibilityObjectWrapper alloc] initWithAccessibilityObject:object]);
    object.setWrapper(wrapper.get());
}

ASCIILiteral AXObjectCache::notificationPlatformName(AXNotification notification)
{
    ASCIILiteral name;

    switch (notification) {
    case AXNotification::ActiveDescendantChanged:
    case AXNotification::FocusedUIElementChanged:
        name = "AXFocusChanged"_s;
        break;
    case AXNotification::ImageOverlayChanged:
        name = "AXImageOverlayChanged"_s;
        break;
    case AXNotification::PageScrolled:
        name = "AXPageScrolled"_s;
        break;
    case AXNotification::SelectedCellsChanged:
        name = "AXSelectedCellsChanged"_s;
        break;
    case AXNotification::SelectedTextChanged:
        name = "AXSelectedTextChanged"_s;
        break;
    case AXNotification::LiveRegionChanged:
    case AXNotification::LiveRegionCreated:
        name = "AXLiveRegionChanged"_s;
        break;
    case AXNotification::InvalidStatusChanged:
        name = "AXInvalidStatusChanged"_s;
        break;
    case AXNotification::CheckedStateChanged:
    case AXNotification::ValueChanged:
        name = "AXValueChanged"_s;
        break;
    case AXNotification::ExpandedChanged:
        name = "AXExpandedChanged"_s;
        break;
    case AXNotification::CurrentStateChanged:
        name = "AXCurrentStateChanged"_s;
        break;
    case AXNotification::SortDirectionChanged:
        name = "AXSortDirectionChanged"_s;
        break;
    case AXNotification::AnnouncementRequested:
        name = "AXAnnouncementRequested"_s;
        break;
    default:
        break;
    }

    return name;
}

void AXObjectCache::relayNotification(const String& notificationName, RetainPtr<NSData> notificationData)
{
    if (auto* page = document().page())
        page->chrome().relayAccessibilityNotification(notificationName, notificationData);
}

void AXObjectCache::postPlatformNotification(AccessibilityObject& object, AXNotification notification)
{
    auto stringNotification = notificationPlatformName(notification);
    if (stringNotification.isEmpty())
        return;

    auto notificationName = stringNotification.createNSString();
    [object.wrapper() accessibilityOverrideProcessNotification:notificationName.get() notificationData:nil];

    // To simulate AX notifications for LayoutTests on the simulator, call
    // the wrapper's accessibilityPostedNotification.
    [object.wrapper() accessibilityPostedNotification:notificationName.get()];
}

void AXObjectCache::postPlatformAnnouncementNotification(const String& message)
{
    auto notificationName = notificationPlatformName(AXNotification::AnnouncementRequested).createNSString();
    NSString *nsMessage = static_cast<NSString *>(message);
    if (RefPtr root = getOrCreate(m_document->view())) {
        [root->wrapper() accessibilityOverrideProcessNotification:notificationName.get() notificationData:[nsMessage dataUsingEncoding:NSUTF8StringEncoding]];

        // To simulate AX notifications for LayoutTests on the simulator, call
        // the wrapper's accessibilityPostedNotification.
        [root->wrapper() accessibilityPostedNotification:notificationName.get() userInfo:@{ notificationName.get() : nsMessage }];
    }
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* object, const AXTextStateChangeIntent&, const VisibleSelection&)
{
    if (object)
        postPlatformNotification(*object, AXNotification::SelectedTextChanged);
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* object, AXTextEditType, const String&, const VisiblePosition&)
{
    if (object)
        postPlatformNotification(*object, AXNotification::ValueChanged);
}

void AXObjectCache::postTextReplacementPlatformNotification(AccessibilityObject* object, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&)
{
    if (object)
        postPlatformNotification(*object, AXNotification::ValueChanged);
}

void AXObjectCache::postTextReplacementPlatformNotificationForTextControl(AccessibilityObject* object, const String&, const String&)
{
    if (object)
        postPlatformNotification(*object, AXNotification::ValueChanged);
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* axFrameObject, AXLoadingEvent loadingEvent)
{
    if (!axFrameObject)
        return;

    if (loadingEvent == AXLoadingEvent::Finished && axFrameObject->document() == axFrameObject->topDocument())
        postPlatformNotification(*axFrameObject, AXNotification::LoadComplete);
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Element*, Element* newElement)
{
    postNotification(newElement, AXNotification::FocusedUIElementChanged);
}

void AXObjectCache::handleScrolledToAnchor(const Node&)
{
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

}

#endif // PLATFORM(IOS_FAMILY)
