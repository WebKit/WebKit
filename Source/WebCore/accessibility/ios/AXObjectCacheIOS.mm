/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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

#import "AXNotifications.h"
#import "AccessibilityObject.h"
#import "Chrome.h"
#import "DocumentPage.h"
#import "DocumentView.h"
#import "RenderObject.h"
#import "WebAccessibilityObjectWrapperIOS.h"
#import <wtf/RetainPtr.h>

namespace WebCore {

void AXObjectCache::initializeUserDefaultValues()
{
}

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

void AXObjectCache::relayNotification(String&& notificationName, RetainPtr<NSData>&& notificationData)
{
    if (RefPtr page = document() ? document()->page() : nullptr)
        page->chrome().relayAccessibilityNotification(WTFMove(notificationName), WTFMove(notificationData));
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
    RetainPtr nsMessage = message.createNSString();
    if (RefPtr root = getOrCreate(m_document->view())) {
        [root->wrapper() accessibilityOverrideProcessNotification:notificationName.get() notificationData:[nsMessage dataUsingEncoding:NSUTF8StringEncoding]];

        // To simulate AX notifications for LayoutTests on the simulator, call
        // the wrapper's accessibilityPostedNotification.
        [root->wrapper() accessibilityPostedNotification:notificationName.get() userInfo:@{ notificationName.get() : nsMessage.get() }];
    }
}

void AXObjectCache::postPlatformARIANotifyNotification(const String& announcement, NotifyPriority priority, InterruptBehavior interruptBehavior, const String& language)
{
    AriaNotifyData notificationData { announcement, priority, interruptBehavior, language };

    if (RefPtr page = document() ? document()->page() : nullptr)
        page->chrome().relayAriaNotifyNotification(WTFMove(notificationData));

    // For tests, also call the wrapper's accessibilityPostedNotification.
    if (gShouldRepostNotificationsForTests) [[unlikely]] {
        if (RefPtr root = getOrCreate(m_document->view())) {
            RetainPtr notificationName = notificationPlatformName(AXNotification::AnnouncementRequested).createNSString();
            RetainPtr message = announcement.createNSString();
            RetainPtr announcementString = adoptNS([[NSAttributedString alloc] initWithString:message.get() attributes:@{
                @"UIAccessibilityARIAPriority": notifyPriorityToAXValueString(priority).get(),
                @"UIAccessibilityARIAInterruptBehavior": interruptBehaviorToAXValueString(interruptBehavior).get(),
                @"UIAccessibilitySpeechAttributeLanguage": language.createNSString().get()
            }]);
            [root->wrapper() accessibilityPostedNotification:notificationName.get() userInfo:@{ notificationName.get() : announcementString.get() }];
        }
    }
}

// These are re-defined here for testing purposes. They are not in a header to prevent colliding with the SDK constants.
static NSString * const UIAccessibilityPriorityLow = @"UIAccessibilityPriorityLow";
static NSString * const UIAccessibilityPriorityDefault = @"UIAccessibilityPriorityDefault";
static NSString * const UIAccessibilitySpeechAttributeAnnouncementPriority = @"UIAccessibilitySpeechAttributeAnnouncementPriority";
static NSString * const UIAccessibilitySpeechAttributeIsLiveRegion = @"UIAccessibilitySpeechAttributeIsLiveRegion";

void AXObjectCache::postPlatformLiveRegionNotification(AccessibilityObject&, LiveRegionStatus status, const AttributedString& announcement)
{
    LiveRegionAnnouncementData notificationData { announcement, status };

    if (RefPtr page = document() ? document()->page() : nullptr)
        page->chrome().relayLiveRegionNotification(WTFMove(notificationData));

    // For tests, also call the wrapper's accessibilityPostedNotification.
    if (gShouldRepostNotificationsForTests) [[unlikely]] {
        if (RefPtr root = getOrCreate(m_document->view())) {
            RetainPtr notificationName = notificationPlatformName(AXNotification::AnnouncementRequested).createNSString();
            RetainPtr priority = status == LiveRegionStatus::Assertive ? UIAccessibilityPriorityDefault : UIAccessibilityPriorityLow;

            auto mutableAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithAttributedString:announcement.nsAttributedString().get()]);
            [mutableAttributedString addAttribute:UIAccessibilitySpeechAttributeAnnouncementPriority value:priority.get() range:NSMakeRange(0, [mutableAttributedString length])];
            [mutableAttributedString addAttribute:UIAccessibilitySpeechAttributeIsLiveRegion value:@(YES) range:NSMakeRange(0, [mutableAttributedString length])];

            [root->wrapper() accessibilityPostedNotification:notificationName.get() userInfo:@{ notificationName.get() : mutableAttributedString.get() }];
        }
    }
}

void AXObjectCache::postTextSelectionChangePlatformNotification(AccessibilityObject* object, const AXTextStateChangeIntent&, const VisibleSelection&)
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

void AXObjectCache::frameLoadingEventPlatformNotification(RenderView* renderView, AXLoadingEvent loadingEvent)
{
    if (!renderView || loadingEvent != AXLoadingEvent::Finished) {
        // It's not always safe to call getOrCreate (e.g. if layout is dirty), so
        // only do so if necessary based on the loading event type.
        return;
    }

    if (renderView->document().isTopDocument()) {
        if (RefPtr axWebArea = getOrCreate(*renderView))
            postPlatformNotification(*axWebArea, AXNotification::LoadComplete);
    }
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

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
