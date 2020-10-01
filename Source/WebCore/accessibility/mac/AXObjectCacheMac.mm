/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#if ENABLE(ACCESSIBILITY) && PLATFORM(MAC)

#import "AXIsolatedObject.h"
#import "AccessibilityObject.h"
#import "AccessibilityTable.h"
#import "RenderObject.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/HIServicesSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#import <ApplicationServices/ApplicationServicesPriv.h>
#endif

#ifndef NSAccessibilityLiveRegionChangedNotification
#define NSAccessibilityLiveRegionChangedNotification @"AXLiveRegionChanged"
#endif

#ifndef NSAccessibilityLiveRegionCreatedNotification 
#define NSAccessibilityLiveRegionCreatedNotification @"AXLiveRegionCreated"
#endif

#ifndef NSAccessibilityTextStateChangeTypeKey
#define NSAccessibilityTextStateChangeTypeKey @"AXTextStateChangeType"
#endif

#ifndef NSAccessibilityTextStateSyncKey
#define NSAccessibilityTextStateSyncKey @"AXTextStateSync"
#endif

#ifndef NSAccessibilityTextSelectionDirection
#define NSAccessibilityTextSelectionDirection @"AXTextSelectionDirection"
#endif

#ifndef NSAccessibilityTextSelectionGranularity
#define NSAccessibilityTextSelectionGranularity @"AXTextSelectionGranularity"
#endif

#ifndef NSAccessibilityTextSelectionChangedFocus
#define NSAccessibilityTextSelectionChangedFocus @"AXTextSelectionChangedFocus"
#endif

#ifndef NSAccessibilityTextEditType
#define NSAccessibilityTextEditType @"AXTextEditType"
#endif

#ifndef NSAccessibilityTextChangeValues
#define NSAccessibilityTextChangeValues @"AXTextChangeValues"
#endif

#ifndef NSAccessibilityTextChangeValue
#define NSAccessibilityTextChangeValue @"AXTextChangeValue"
#endif

#ifndef NSAccessibilityTextChangeValueLength
#define NSAccessibilityTextChangeValueLength @"AXTextChangeValueLength"
#endif

#ifndef NSAccessibilityTextChangeValueStartMarker
#define NSAccessibilityTextChangeValueStartMarker @"AXTextChangeValueStartMarker"
#endif

#ifndef NSAccessibilityTextChangeElement
#define NSAccessibilityTextChangeElement @"AXTextChangeElement"
#endif

#ifndef NSAccessibilitySelectedTextMarkerRangeAttribute
#define NSAccessibilitySelectedTextMarkerRangeAttribute @"AXSelectedTextMarkerRange"
#endif

#ifndef kAXDraggingSourceDragBeganNotification
#define kAXDraggingSourceDragBeganNotification CFSTR("AXDraggingSourceDragBegan")
#endif

#ifndef kAXDraggingSourceDragEndedNotification
#define kAXDraggingSourceDragEndedNotification CFSTR("AXDraggingSourceDragEnded")
#endif

#ifndef kAXDraggingDestinationDropAllowedNotification
#define kAXDraggingDestinationDropAllowedNotification CFSTR("AXDraggingDestinationDropAllowed")
#endif

#ifndef kAXDraggingDestinationDropNotAllowedNotification
#define kAXDraggingDestinationDropNotAllowedNotification CFSTR("AXDraggingDestinationDropNotAllowed")
#endif

#ifndef kAXDraggingDestinationDragAcceptedNotification
#define kAXDraggingDestinationDragAcceptedNotification CFSTR("AXDraggingDestinationDragAccepted")
#endif

#ifndef kAXDraggingDestinationDragNotAcceptedNotification
#define kAXDraggingDestinationDragNotAcceptedNotification CFSTR("AXDraggingDestinationDragNotAccepted")
#endif

// Very large strings can negatively impact the performance of notifications, so this length is chosen to try to fit an average paragraph or line of text, but not allow strings to be large enough to hurt performance.
static const NSUInteger AXValueChangeTruncationLength = 1000;

// Check if platform provides enums for text change notifications
#ifndef AXTextStateChangeDefined
#define AXTextStateChangeDefined

typedef CF_ENUM(UInt32, AXTextStateChangeType)
{
    kAXTextStateChangeTypeUnknown,
    kAXTextStateChangeTypeEdit,
    kAXTextStateChangeTypeSelectionMove,
    kAXTextStateChangeTypeSelectionExtend,
    kAXTextStateChangeTypeSelectionBoundary
};

typedef CF_ENUM(UInt32, AXTextEditType)
{
    kAXTextEditTypeUnknown,
    kAXTextEditTypeDelete,
    kAXTextEditTypeInsert,
    kAXTextEditTypeTyping,
    kAXTextEditTypeDictation,
    kAXTextEditTypeCut,
    kAXTextEditTypePaste,
    kAXTextEditTypeAttributesChange
};

typedef CF_ENUM(UInt32, AXTextSelectionDirection)
{
    kAXTextSelectionDirectionUnknown = 0,
    kAXTextSelectionDirectionBeginning,
    kAXTextSelectionDirectionEnd,
    kAXTextSelectionDirectionPrevious,
    kAXTextSelectionDirectionNext,
    kAXTextSelectionDirectionDiscontiguous
};

typedef CF_ENUM(UInt32, AXTextSelectionGranularity)
{
    kAXTextSelectionGranularityUnknown,
    kAXTextSelectionGranularityCharacter,
    kAXTextSelectionGranularityWord,
    kAXTextSelectionGranularityLine,
    kAXTextSelectionGranularitySentence,
    kAXTextSelectionGranularityParagraph,
    kAXTextSelectionGranularityPage,
    kAXTextSelectionGranularityDocument,
    kAXTextSelectionGranularityAll
};

#endif // AXTextStateChangeDefined

static AXTextStateChangeType platformChangeTypeForWebCoreChangeType(WebCore::AXTextStateChangeType changeType)
{
    switch (changeType) {
    case WebCore::AXTextStateChangeTypeUnknown:
        return kAXTextStateChangeTypeUnknown;
    case WebCore::AXTextStateChangeTypeEdit:
        return kAXTextStateChangeTypeEdit;
    case WebCore::AXTextStateChangeTypeSelectionMove:
        return kAXTextStateChangeTypeSelectionMove;
    case WebCore::AXTextStateChangeTypeSelectionExtend:
        return kAXTextStateChangeTypeSelectionExtend;
    case WebCore::AXTextStateChangeTypeSelectionBoundary:
        return kAXTextStateChangeTypeSelectionBoundary;
    }
}

static AXTextEditType platformEditTypeForWebCoreEditType(WebCore::AXTextEditType changeType)
{
    switch (changeType) {
    case WebCore::AXTextEditTypeUnknown:
        return kAXTextEditTypeUnknown;
    case WebCore::AXTextEditTypeDelete:
        return kAXTextEditTypeDelete;
    case WebCore::AXTextEditTypeInsert:
        return kAXTextEditTypeInsert;
    case WebCore::AXTextEditTypeTyping:
        return kAXTextEditTypeTyping;
    case WebCore::AXTextEditTypeDictation:
        return kAXTextEditTypeDictation;
    case WebCore::AXTextEditTypeCut:
        return kAXTextEditTypeCut;
    case WebCore::AXTextEditTypePaste:
        return kAXTextEditTypePaste;
    case WebCore::AXTextEditTypeAttributesChange:
        return kAXTextEditTypeAttributesChange;
    }
}

static AXTextSelectionDirection platformDirectionForWebCoreDirection(WebCore::AXTextSelectionDirection direction)
{
    switch (direction) {
    case WebCore::AXTextSelectionDirectionUnknown:
        return kAXTextSelectionDirectionUnknown;
    case WebCore::AXTextSelectionDirectionBeginning:
        return kAXTextSelectionDirectionBeginning;
    case WebCore::AXTextSelectionDirectionEnd:
        return kAXTextSelectionDirectionEnd;
    case WebCore::AXTextSelectionDirectionPrevious:
        return kAXTextSelectionDirectionPrevious;
    case WebCore::AXTextSelectionDirectionNext:
        return kAXTextSelectionDirectionNext;
    case WebCore::AXTextSelectionDirectionDiscontiguous:
        return kAXTextSelectionDirectionDiscontiguous;
    }
}

static AXTextSelectionGranularity platformGranularityForWebCoreGranularity(WebCore::AXTextSelectionGranularity granularity)
{
    switch (granularity) {
    case WebCore::AXTextSelectionGranularityUnknown:
        return kAXTextSelectionGranularityUnknown;
    case WebCore::AXTextSelectionGranularityCharacter:
        return kAXTextSelectionGranularityCharacter;
    case WebCore::AXTextSelectionGranularityWord:
        return kAXTextSelectionGranularityWord;
    case WebCore::AXTextSelectionGranularityLine:
        return kAXTextSelectionGranularityLine;
    case WebCore::AXTextSelectionGranularitySentence:
        return kAXTextSelectionGranularitySentence;
    case WebCore::AXTextSelectionGranularityParagraph:
        return kAXTextSelectionGranularityParagraph;
    case WebCore::AXTextSelectionGranularityPage:
        return kAXTextSelectionGranularityPage;
    case WebCore::AXTextSelectionGranularityDocument:
        return kAXTextSelectionGranularityDocument;
    case WebCore::AXTextSelectionGranularityAll:
        return kAXTextSelectionGranularityAll;
    }
}

// The simple Cocoa calls in this file don't throw exceptions.

namespace WebCore {

void AXObjectCache::attachWrapper(AXCoreObject* obj)
{
    RetainPtr<WebAccessibilityObjectWrapper> wrapper = adoptNS([[WebAccessibilityObjectWrapper alloc] initWithAccessibilityObject:obj]);
    obj->setWrapper(wrapper.get());
}

static BOOL axShouldRepostNotificationsForTests = false;

void AXObjectCache::setShouldRepostNotificationsForTests(bool value)
{
    axShouldRepostNotificationsForTests = value;
}

static void AXPostNotificationWithUserInfo(AccessibilityObjectWrapper *object, NSString *notification, id userInfo, bool skipSystemNotification = false)
{
    if (id associatedPluginParent = [object associatedPluginParent])
        object = associatedPluginParent;

    // To simplify monitoring for notifications in tests, repost as a simple NSNotification instead of forcing test infrastucture to setup an IPC client and do all the translation between WebCore types and platform specific IPC types and back
    if (UNLIKELY(axShouldRepostNotificationsForTests))
        [object accessibilityPostedNotification:notification userInfo:userInfo];
    else if (skipSystemNotification)
        return;

    NSAccessibilityPostNotificationWithUserInfo(object, notification, userInfo);
}

void AXObjectCache::postPlatformNotification(AXCoreObject* obj, AXNotification notification)
{
    if (!obj)
        return;

    bool skipSystemNotification = false;
    // Some notifications are unique to Safari and do not have NSAccessibility equivalents.
    NSString *macNotification;
    switch (notification) {
        case AXActiveDescendantChanged:
            // An active descendant change for trees means a selected rows change.
            if (obj->isTree() || obj->isTable())
                macNotification = NSAccessibilitySelectedRowsChangedNotification;
            
            // When a combobox uses active descendant, it means the selected item in its associated
            // list has changed. In these cases we should use selected children changed, because
            // we don't want the focus to change away from the combobox where the user is typing.
            else if (obj->isComboBox() || obj->isList() || obj->isListBox())
                macNotification = NSAccessibilitySelectedChildrenChangedNotification;
            else
                macNotification = NSAccessibilityFocusedUIElementChangedNotification;                
            break;
        case AXAutocorrectionOccured:
            macNotification = @"AXAutocorrectionOccurred";
            break;
        case AXFocusedUIElementChanged:
            macNotification = NSAccessibilityFocusedUIElementChangedNotification;
            break;
        case AXLayoutComplete:
            macNotification = @"AXLayoutComplete";
            break;
        case AXLoadComplete:
        case AXFrameLoadComplete:
            macNotification = @"AXLoadComplete";
            // Frame loading events are handled by the UIProcess on macOS to improve reliability.
            // On macOS, before notifications are allowed by AppKit to be sent to clients, you need to have a client (e.g. VoiceOver)
            // register for that notification. Because these new processes appear before VO has a chance to register, it will often
            // miss AXLoadComplete notifications. By moving them to the UIProcess, we can eliminate that issue.
            skipSystemNotification = true;
            break;
        case AXInvalidStatusChanged:
            macNotification = @"AXInvalidStatusChanged";
            break;
        case AXSelectedChildrenChanged:
            if (obj->isTable() && obj->isExposable())
                macNotification = NSAccessibilitySelectedRowsChangedNotification;
            else
                macNotification = NSAccessibilitySelectedChildrenChangedNotification;
            break;
        case AXSelectedTextChanged:
            macNotification = NSAccessibilitySelectedTextChangedNotification;
            break;
        case AXCheckedStateChanged:
        case AXValueChanged:
            macNotification = NSAccessibilityValueChangedNotification;
            break;
        case AXLiveRegionCreated:
            macNotification = NSAccessibilityLiveRegionCreatedNotification;
            break;
        case AXLiveRegionChanged:
            macNotification = NSAccessibilityLiveRegionChangedNotification;
            break;
        case AXRowCountChanged:
            macNotification = NSAccessibilityRowCountChangedNotification;
            break;
        case AXRowExpanded:
            macNotification = NSAccessibilityRowExpandedNotification;
            break;
        case AXRowCollapsed:
            macNotification = NSAccessibilityRowCollapsedNotification;
            break;
        case AXElementBusyChanged:
            macNotification = @"AXElementBusyChanged";
            break;
        case AXExpandedChanged:
            macNotification = @"AXExpandedChanged";
            break;
        case AXMenuClosed:
            macNotification = (id)kAXMenuClosedNotification;
            break;
        case AXMenuListItemSelected:
        case AXMenuListValueChanged:
            macNotification = (id)kAXMenuItemSelectedNotification;
            break;
        case AXPressDidSucceed:
            macNotification = @"AXPressDidSucceed";
            break;
        case AXPressDidFail:
            macNotification = @"AXPressDidFail";
            break;
        case AXMenuOpened:
            macNotification = (id)kAXMenuOpenedNotification;
            break;
        case AXDraggingStarted:
            macNotification = (id)kAXDraggingSourceDragBeganNotification;
            break;
        case AXDraggingEnded:
            macNotification = (id)kAXDraggingSourceDragEndedNotification;
            break;
        case AXDraggingEnteredDropZone:
            macNotification = (id)kAXDraggingDestinationDropAllowedNotification;
            break;
        case AXDraggingDropped:
            macNotification = (id)kAXDraggingDestinationDragAcceptedNotification;
            break;
        case AXDraggingExitedDropZone:
            macNotification = (id)kAXDraggingDestinationDragNotAcceptedNotification;
            break;
        default:
            return;
    }
    
    // NSAccessibilityPostNotification will call this method, (but not when running DRT), so ASSERT here to make sure it does not crash.
    // https://bugs.webkit.org/show_bug.cgi?id=46662
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    ASSERT([obj->wrapper() accessibilityIsIgnored] || true);
    ALLOW_DEPRECATED_DECLARATIONS_END

    AXPostNotificationWithUserInfo(obj->wrapper(), macNotification, nil, skipSystemNotification);
}

void AXObjectCache::postTextStateChangePlatformNotification(AXCoreObject* object, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    if (!object)
        object = rootWebArea();

    if (!object)
        return;

    NSMutableDictionary *userInfo = [[NSMutableDictionary alloc] initWithCapacity:5];
    if (m_isSynchronizingSelection)
        [userInfo setObject:@YES forKey:NSAccessibilityTextStateSyncKey];
    if (intent.type != AXTextStateChangeTypeUnknown) {
        [userInfo setObject:@(platformChangeTypeForWebCoreChangeType(intent.type)) forKey:NSAccessibilityTextStateChangeTypeKey];
        switch (intent.type) {
        case AXTextStateChangeTypeSelectionMove:
        case AXTextStateChangeTypeSelectionExtend:
        case AXTextStateChangeTypeSelectionBoundary:
            [userInfo setObject:@(platformDirectionForWebCoreDirection(intent.selection.direction)) forKey:NSAccessibilityTextSelectionDirection];
            switch (intent.selection.direction) {
            case AXTextSelectionDirectionUnknown:
                break;
            case AXTextSelectionDirectionBeginning:
            case AXTextSelectionDirectionEnd:
            case AXTextSelectionDirectionPrevious:
            case AXTextSelectionDirectionNext:
                [userInfo setObject:@(platformGranularityForWebCoreGranularity(intent.selection.granularity)) forKey:NSAccessibilityTextSelectionGranularity];
                break;
            case AXTextSelectionDirectionDiscontiguous:
                break;
            }
            if (intent.selection.focusChange)
                [userInfo setObject:@(intent.selection.focusChange) forKey:NSAccessibilityTextSelectionChangedFocus];
            break;
        case AXTextStateChangeTypeUnknown:
        case AXTextStateChangeTypeEdit:
            break;
        }
    }
    if (!selection.isNone()) {
        if (id textMarkerRange = textMarkerRangeFromVisiblePositions(this, selection.visibleStart(), selection.visibleEnd()))
            [userInfo setObject:textMarkerRange forKey:NSAccessibilitySelectedTextMarkerRangeAttribute];
    }

    if (id wrapper = object->wrapper())
        [userInfo setObject:wrapper forKey:NSAccessibilityTextChangeElement];

    if (auto root = rootWebArea()) {
        AXPostNotificationWithUserInfo(rootWebArea()->wrapper(), NSAccessibilitySelectedTextChangedNotification, userInfo);
        if (root->wrapper() != object->wrapper())
            AXPostNotificationWithUserInfo(object->wrapper(), NSAccessibilitySelectedTextChangedNotification, userInfo);
    }

    [userInfo release];
}

static void addTextMarkerFor(NSMutableDictionary* change, AXCoreObject& object, const VisiblePosition& position)
{
    if (position.isNull())
        return;
    if (id textMarker = [object.wrapper() textMarkerForVisiblePosition:position])
        [change setObject:textMarker forKey:NSAccessibilityTextChangeValueStartMarker];
}

static void addTextMarkerFor(NSMutableDictionary* change, AXCoreObject& object, HTMLTextFormControlElement& textControl)
{
    if (id textMarker = [object.wrapper() textMarkerForFirstPositionInTextControl:textControl])
        [change setObject:textMarker forKey:NSAccessibilityTextChangeValueStartMarker];
}

template <typename TextMarkerTargetType>
static NSDictionary *textReplacementChangeDictionary(AXCoreObject& object, AXTextEditType type, const String& string, TextMarkerTargetType& markerTarget)
{
    NSString *text = (NSString *)string;
    NSUInteger length = [text length];
    if (!length)
        return nil;
    NSMutableDictionary *change = [[NSMutableDictionary alloc] initWithCapacity:4];
    [change setObject:@(platformEditTypeForWebCoreEditType(type)) forKey:NSAccessibilityTextEditType];
    if (length > AXValueChangeTruncationLength) {
        [change setObject:@(length) forKey:NSAccessibilityTextChangeValueLength];
        text = [text substringToIndex:AXValueChangeTruncationLength];
    }
    [change setObject:text forKey:NSAccessibilityTextChangeValue];
    addTextMarkerFor(change, object, markerTarget);
    return [change autorelease];
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* object, AXTextEditType type, const String& text, const VisiblePosition& position)
{
    if (!text.length())
        return;

    postTextReplacementPlatformNotification(object, AXTextEditTypeUnknown, emptyString(), type, text, position);
}

static void postUserInfoForChanges(AXCoreObject& rootWebArea, AXCoreObject& object, NSMutableArray* changes)
{
    NSMutableDictionary *userInfo = [[NSMutableDictionary alloc] initWithCapacity:4];
    [userInfo setObject:@(platformChangeTypeForWebCoreChangeType(AXTextStateChangeTypeEdit)) forKey:NSAccessibilityTextStateChangeTypeKey];
    if (changes.count)
        [userInfo setObject:changes forKey:NSAccessibilityTextChangeValues];

    if (id wrapper = object.wrapper())
        [userInfo setObject:wrapper forKey:NSAccessibilityTextChangeElement];

    AXPostNotificationWithUserInfo(rootWebArea.wrapper(), NSAccessibilityValueChangedNotification, userInfo);
    if (rootWebArea.wrapper() != object.wrapper())
        AXPostNotificationWithUserInfo(object.wrapper(), NSAccessibilityValueChangedNotification, userInfo);

    [userInfo release];
}

void AXObjectCache::postTextReplacementPlatformNotification(AXCoreObject* object, AXTextEditType deletionType, const String& deletedText, AXTextEditType insertionType, const String& insertedText, const VisiblePosition& position)
{
    if (!object)
        object = rootWebArea();

    if (!object)
        return;

    NSMutableArray *changes = [[NSMutableArray alloc] initWithCapacity:2];
    if (NSDictionary *change = textReplacementChangeDictionary(*object, deletionType, deletedText, position))
        [changes addObject:change];
    if (NSDictionary *change = textReplacementChangeDictionary(*object, insertionType, insertedText, position))
        [changes addObject:change];

    if (auto* root = rootWebArea())
        postUserInfoForChanges(*root, *object, changes);
    [changes release];
}

void AXObjectCache::postTextReplacementPlatformNotificationForTextControl(AXCoreObject* object, const String& deletedText, const String& insertedText, HTMLTextFormControlElement& textControl)
{
    if (!object)
        object = rootWebArea();

    if (!object)
        return;

    NSMutableArray *changes = [[NSMutableArray alloc] initWithCapacity:2];
    if (NSDictionary *change = textReplacementChangeDictionary(*object, AXTextEditTypeDelete, deletedText, textControl))
        [changes addObject:change];
    if (NSDictionary *change = textReplacementChangeDictionary(*object, AXTextEditTypeInsert, insertedText, textControl))
        [changes addObject:change];

    if (auto* root = rootWebArea())
        postUserInfoForChanges(*root, *object, changes);
    [changes release];
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* axFrameObject, AXLoadingEvent loadingEvent)
{
    if (!axFrameObject)
        return;

    if (loadingEvent == AXLoadingFinished) {
        if (axFrameObject->document() == axFrameObject->topDocument())
            postNotification(axFrameObject, axFrameObject->document(), AXLoadComplete);
        else
            postNotification(axFrameObject, axFrameObject->document(), AXFrameLoadComplete);
    }
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node*, Node*)
{
    NSAccessibilityHandleFocusChanged();
    // AXFocusChanged is a test specific notification name and not something a real AT will be listening for
    if (UNLIKELY(!axShouldRepostNotificationsForTests))
        return;

    auto* rootWebArea = this->rootWebArea();
    if (!rootWebArea)
        return;

    [rootWebArea->wrapper() accessibilityPostedNotification:@"AXFocusChanged" userInfo:nil];
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

// TextMarker utility functions.

static id AXTextMarkerRange(id startMarker, id endMarker)
{
    ASSERT(startMarker);
    ASSERT(endMarker);
    ASSERT(CFGetTypeID((__bridge CFTypeRef)startMarker) == AXTextMarkerGetTypeID());
    ASSERT(CFGetTypeID((__bridge CFTypeRef)endMarker) == AXTextMarkerGetTypeID());
    return CFBridgingRelease(AXTextMarkerRangeCreate(kCFAllocatorDefault, (AXTextMarkerRef)startMarker, (AXTextMarkerRef)endMarker));
}

id textMarkerRangeFromMarkers(id textMarker1, id textMarker2)
{
    if (!textMarker1 || !textMarker2)
        return nil;

    return AXTextMarkerRange(textMarker1, textMarker2);
}

id textMarkerForVisiblePosition(AXObjectCache* cache, const VisiblePosition& visiblePos)
{
    ASSERT(isMainThread());
    ASSERT(cache);
    if (!cache)
        return nil;

    auto textMarkerData = cache->textMarkerDataForVisiblePosition(visiblePos);
    if (!textMarkerData)
        return nil;

    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData.value(), sizeof(textMarkerData.value())));
}

id textMarkerRangeFromVisiblePositions(AXObjectCache* cache, const VisiblePosition& startPosition, const VisiblePosition& endPosition)
{
    ASSERT(isMainThread());
    if (!cache)
        return nil;

    id startTextMarker = textMarkerForVisiblePosition(cache, startPosition);
    id endTextMarker = textMarkerForVisiblePosition(cache, endPosition);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

}

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(MAC)
