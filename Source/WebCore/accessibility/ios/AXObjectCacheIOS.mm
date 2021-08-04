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

#if ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)

#import "AccessibilityObject.h"
#import "WebAccessibilityObjectWrapperIOS.h"
#import "RenderObject.h"

#import <wtf/RetainPtr.h>

namespace WebCore {
    
void AXObjectCache::attachWrapper(AXCoreObject* obj)
{
    RetainPtr<AccessibilityObjectWrapper> wrapper = adoptNS([[WebAccessibilityObjectWrapper alloc] initWithAccessibilityObject:obj]);
    obj->setWrapper(wrapper.get());
}

String AXObjectCache::notificationPlatformName(AXNotification notification)
{
    String name;

    switch (notification) {
    case AXActiveDescendantChanged:
    case AXFocusedUIElementChanged:
        name = "AXFocusChanged";
        break;
    case AXImageOverlayChanged:
        name = "AXImageOverlayChanged";
        break;
    case AXPageScrolled:
        name = "AXPageScrolled";
        break;
    case AXSelectedStateChanged:
        name = "AXSelectedCellsChanged";
        break;
    case AXSelectedTextChanged:
        name = "AXSelectedTextChanged";
        break;
    case AXLiveRegionChanged:
    case AXLiveRegionCreated:
        name = "AXLiveRegionChanged";
        break;
    case AXInvalidStatusChanged:
        name = "AXInvalidStatusChanged";
        break;
    case AXCheckedStateChanged:
    case AXValueChanged:
        name = "AXValueChanged";
        break;
    case AXExpandedChanged:
        name = "AXExpandedChanged";
        break;
    case AXCurrentStateChanged:
        name = "AXCurrentStateChanged";
        break;
    case AXSortDirectionChanged:
        name = "AXSortDirectionChanged";
        break;
    default:
        break;
    }

    return name;
}

void AXObjectCache::postPlatformNotification(AXCoreObject* object, AXNotification notification)
{
    if (!object)
        return;

    // iOS notifications must ultimately call UIKit UIAccessibilityPostNotification.
    // But WebCore is not linked with UIKit. So a workaround is to override the wrapper's
    // postNotification method in the system WebKitAccessibility bundle that does link UIKit.
    String notificationName = notificationPlatformName(notification);
    if (notificationName.isEmpty())
        return;

    [object->wrapper() postNotification:notificationName];

    // To simulate AX notifications for LayoutTests on the simulator, call
    // the wrapper's accessibilityPostedNotification.
    [object->wrapper() accessibilityPostedNotification:notificationName];
}

void AXObjectCache::postTextStateChangePlatformNotification(AXCoreObject* object, const AXTextStateChangeIntent&, const VisibleSelection&)
{
    postPlatformNotification(object, AXSelectedTextChanged);
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* object, AXTextEditType, const String&, const VisiblePosition&)
{
    postPlatformNotification(object, AXValueChanged);
}

void AXObjectCache::postTextReplacementPlatformNotification(AXCoreObject* object, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&)
{
    postPlatformNotification(object, AXValueChanged);
}

void AXObjectCache::postTextReplacementPlatformNotificationForTextControl(AXCoreObject* object, const String&, const String&, HTMLTextFormControlElement&)
{
    postPlatformNotification(object, AXValueChanged);
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* axFrameObject, AXLoadingEvent loadingEvent)
{
    if (!axFrameObject)
        return;
    
    if (loadingEvent == AXLoadingFinished && axFrameObject->document() == axFrameObject->topDocument())
        postPlatformNotification(axFrameObject, AXLoadComplete);
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node*, Node* newNode)
{
    postNotification(newNode, AXFocusedUIElementChanged);
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

}

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)
