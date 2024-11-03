/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "AXIsolatedObject.h"
#import "AccessibilityObject.h"
#import "AccessibilityTable.h"
#import "DeprecatedGlobalSettings.h"
#import "LocalFrameView.h"
#import "RenderObject.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <ApplicationServices/ApplicationServicesPriv.h>
#endif

#ifndef NSAccessibilityCurrentStateChangedNotification
#define NSAccessibilityCurrentStateChangedNotification @"AXCurrentStateChanged"
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

#ifndef NSAccessibilityTextInputMarkingSessionBeganNotification
#define NSAccessibilityTextInputMarkingSessionBeganNotification @"AXTextInputMarkingSessionBegan"
#endif

#ifndef NSAccessibilityTextInputMarkingSessionEndedNotification
#define NSAccessibilityTextInputMarkingSessionEndedNotification @"AXTextInputMarkingSessionEnded"
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

void AXObjectCache::attachWrapper(AccessibilityObject& object)
{
    RetainPtr<WebAccessibilityObjectWrapper> wrapper = adoptNS([[WebAccessibilityObjectWrapper alloc] initWithAccessibilityObject:object]);
    object.setWrapper(wrapper.get());
}

static BOOL axShouldRepostNotificationsForTests = false;

void AXObjectCache::setShouldRepostNotificationsForTests(bool value)
{
    axShouldRepostNotificationsForTests = value;
}

static void AXPostNotificationWithUserInfo(AccessibilityObjectWrapper *object, NSString *notification, id userInfo, bool skipSystemNotification = false)
{
    if (id associatedPluginParent = [object _associatedPluginParent])
        object = associatedPluginParent;

    // To simplify monitoring for notifications in tests, repost as a simple NSNotification instead of forcing test infrastucture to setup an IPC client and do all the translation between WebCore types and platform specific IPC types and back
    if (UNLIKELY(axShouldRepostNotificationsForTests))
        [object accessibilityPostedNotification:notification userInfo:userInfo];
    else if (skipSystemNotification)
        return;

    NSAccessibilityPostNotificationWithUserInfo(object, notification, userInfo);
}

#ifndef NDEBUG
// This function exercises, for debugging and testing purposes, the AXObject methods called in [WebAccessibilityObjectWrapper accessibilityIsIgnored].
// It is useful in cases like AXObjectCache::postPlatformNotification which calls NSAccessibilityPostNotification, which in turn calls accessibilityIsIgnored, except when it is running layout tests.
// Thus, calling exerciseIsIgnored during AXObjectCache::postPlatformNotification helps detect issues during layout tests.
// Example of such issues is the crash described in: https://bugs.webkit.org/show_bug.cgi?id=46662.
static void exerciseIsIgnored(AccessibilityObject& object)
{
    object.updateBackingStore();
    if (object.isAttachment()) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [[object.wrapper() attachmentView] accessibilityIsIgnored];
ALLOW_DEPRECATED_DECLARATIONS_END
        return;
    }
    object.isIgnored();
}
#endif

void AXObjectCache::postPlatformNotification(AccessibilityObject& object, AXNotification notification)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    bool skipSystemNotification = false;
    // Some notifications are unique to Safari and do not have NSAccessibility equivalents.
    NSString *macNotification;
    switch (notification) {
    case AXActiveDescendantChanged:
        macNotification = @"AXActiveElementChanged";
        break;
    case AXAutocorrectionOccured:
        macNotification = @"AXAutocorrectionOccurred";
        break;
    case AXCurrentStateChanged:
        macNotification = NSAccessibilityCurrentStateChangedNotification;
        break;
    case AXFocusedUIElementChanged:
        macNotification = NSAccessibilityFocusedUIElementChangedNotification;
        break;
    case AXImageOverlayChanged:
        macNotification = @"AXImageOverlayChanged";
        break;
    case AXLayoutComplete:
        macNotification = @"AXLayoutComplete";
        break;
    case AXLabelChanged:
        macNotification = NSAccessibilityTitleChangedNotification;
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
        if (object.isTable() && object.isExposable())
            macNotification = NSAccessibilitySelectedRowsChangedNotification;
        else
            macNotification = NSAccessibilitySelectedChildrenChangedNotification;
        break;
    case AXSelectedCellsChanged:
        macNotification = NSAccessibilitySelectedCellsChangedNotification;
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
    case AXSortDirectionChanged:
        macNotification = @"AXSortDirectionChanged";
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
    case AXTextCompositionBegan:
        macNotification = NSAccessibilityTextInputMarkingSessionBeganNotification;
        break;
    case AXTextCompositionEnded:
        macNotification = NSAccessibilityTextInputMarkingSessionEndedNotification;
        break;
    default:
        return;
    }

    Ref protectedObject = object;

#ifndef NDEBUG
    exerciseIsIgnored(object);
#endif

    AXPostNotificationWithUserInfo(object.wrapper(), macNotification, nil, skipSystemNotification);
}

void AXObjectCache::postPlatformAnnouncementNotification(const String& message)
{
    ASSERT(isMainThread());

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    NSDictionary *userInfo = @{ NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey: message,
    };
    NSAccessibilityPostNotificationWithUserInfo(NSApp, NSAccessibilityAnnouncementRequestedNotification, userInfo);

    // To simplify monitoring of notifications in tests, repost as a simple NSNotification instead of forcing test infrastucture to setup an IPC client and do all the translation between WebCore types and platform specific IPC types and back.
    if (UNLIKELY(axShouldRepostNotificationsForTests)) {
        if (RefPtr root = getOrCreate(m_document->view()))
            [root->wrapper() accessibilityPostedNotification:NSAccessibilityAnnouncementRequestedNotification userInfo:userInfo];
    }
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::createIsolatedObjectIfNeeded(AccessibilityObject& object)
{
    // The wrapper associated with a published notification may not have an isolated object yet.
    // This should only happen when the live object is ignored, meaning we will never create an isolated object for it.
    // This is generally correct, but not in this case, since AX clients will try to query this wrapper but the wrapper
    // will consider itself detached due to the lack of an isolated object.
    //
    // Detect this and create an isolated object if necessary.
    id wrapper = object.wrapper();
    if (!wrapper || [wrapper hasIsolatedObject])
        return;

    if (object.isIgnored())
        deferAddUnconnectedNode(object);
}
#endif

AXTextStateChangeIntent AXObjectCache::inferDirectionFromIntent(AccessibilityObject& object, const AXTextStateChangeIntent& originalIntent, const VisibleSelection& selection)
{
    if (!object.isTextControl() && !object.editableAncestor())
        return originalIntent;

    if (originalIntent.selection.direction != AXTextSelectionDirectionDiscontiguous || object.objectID() != m_lastTextFieldAXID || m_lastSelection == selection) {
        m_lastTextFieldAXID = object.objectID();
        m_lastSelection = selection;
        return originalIntent;
    }

    auto intent = originalIntent;
    if (m_lastSelection.isCaret() && selection.isCaret()) {
        // Cursor movement
        if (selection.visibleStart() == m_lastSelection.visibleStart().next(CannotCrossEditingBoundary)) {
            intent.type = AXTextStateChangeTypeSelectionMove;
            intent.selection.direction = AXTextSelectionDirectionNext;
            intent.selection.granularity = AXTextSelectionGranularityCharacter;
        } else if (selection.visibleStart() == m_lastSelection.visibleStart().previous(CannotCrossEditingBoundary)) {
            intent.type = AXTextStateChangeTypeSelectionMove;
            intent.selection.direction = AXTextSelectionDirectionPrevious;
            intent.selection.granularity = AXTextSelectionGranularityCharacter;
        }
    } else if (selection.visibleBase() == m_lastSelection.visibleBase()) {
        // Selection
        if (selection.visibleExtent() == m_lastSelection.visibleExtent().next(CannotCrossEditingBoundary)) {
            intent.type = AXTextStateChangeTypeSelectionExtend;
            intent.selection.direction = AXTextSelectionDirectionNext;
            intent.selection.granularity = AXTextSelectionGranularityCharacter;
        } else if (selection.visibleExtent() == m_lastSelection.visibleExtent().previous(CannotCrossEditingBoundary)) {
            intent.type = AXTextStateChangeTypeSelectionExtend;
            intent.selection.direction = AXTextSelectionDirectionPrevious;
            intent.selection.granularity = AXTextSelectionGranularityCharacter;
        }
    }

    m_lastTextFieldAXID = object.objectID();
    m_lastSelection = selection;

    return intent;
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* object, const AXTextStateChangeIntent& originalIntent, const VisibleSelection& selection)
{
    if (!object)
        object = rootWebArea();

    if (!object)
        return;
    RefPtr protectedObject = object;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    auto intent = inferDirectionFromIntent(*object, originalIntent, selection);

    auto userInfo = adoptNS([[NSMutableDictionary alloc] initWithCapacity:5]);
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
        if (auto textMarkerRange = textMarkerRangeFromVisiblePositions(this, selection.visibleStart(), selection.visibleEnd()))
            [userInfo setObject:(id)textMarkerRange forKey:AXSelectedTextMarkerRangeAttribute];
    }

    if (id wrapper = object->wrapper()) {
        [userInfo setObject:wrapper forKey:NSAccessibilityTextChangeElement];
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        createIsolatedObjectIfNeeded(*object);
#endif
    }

    if (RefPtr root = rootWebArea()) {
        AXPostNotificationWithUserInfo(root->wrapper(), NSAccessibilitySelectedTextChangedNotification, userInfo.get());
        if (root->wrapper() != object->wrapper())
            AXPostNotificationWithUserInfo(object->wrapper(), NSAccessibilitySelectedTextChangedNotification, userInfo.get());
    }
}

static void addTextMarkerForVisiblePosition(NSMutableDictionary *change, AXObjectCache& cache, const VisiblePosition& position)
{
    ASSERT(!position.isNull());

    if (RetainPtr marker = textMarkerForVisiblePosition(&cache, position))
        [change setObject:(__bridge id)marker.get() forKey:NSAccessibilityTextChangeValueStartMarker];
}

static void addFirstTextMarker(NSMutableDictionary *change, AXObjectCache& cache, AccessibilityObject& object)
{
    TextMarkerData textMarkerData { cache.treeID(), object.objectID(), 0 };
    auto textMarkerDataRef = adoptCF(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData)));
    [change setObject:bridge_id_cast(textMarkerDataRef.get()) forKey:NSAccessibilityTextChangeValueStartMarker];
}

static NSDictionary *textReplacementChangeDictionary(AXObjectCache& cache, AccessibilityObject& object, AXTextEditType type, const String& string, const VisiblePosition& visiblePosition = { })
{
    NSString *text = (NSString *)string;
    NSUInteger length = [text length];
    if (!length)
        return nil;

    auto change = adoptNS([[NSMutableDictionary alloc] initWithCapacity:4]);
    [change setObject:@(platformEditTypeForWebCoreEditType(type)) forKey:NSAccessibilityTextEditType];
    if (length > AXValueChangeTruncationLength) {
        [change setObject:@(length) forKey:NSAccessibilityTextChangeValueLength];
        text = [text substringToIndex:AXValueChangeTruncationLength];
    }
    [change setObject:text forKey:NSAccessibilityTextChangeValue];

    if (!visiblePosition.isNull())
        addTextMarkerForVisiblePosition(change.get(), cache, visiblePosition);
    else
        addFirstTextMarker(change.get(), cache, object);
    return change.autorelease();
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* object, AXTextEditType type, const String& text, const VisiblePosition& position)
{
    if (!text.length())
        return;

    postTextReplacementPlatformNotification(object, AXTextEditTypeUnknown, emptyString(), type, text, position);
}

void AXObjectCache::postUserInfoForChanges(AccessibilityObject& rootWebArea, AccessibilityObject& object, RetainPtr<NSMutableArray> changes)
{
    auto userInfo = adoptNS([[NSMutableDictionary alloc] initWithCapacity:4]);
    [userInfo setObject:@(platformChangeTypeForWebCoreChangeType(AXTextStateChangeTypeEdit)) forKey:NSAccessibilityTextStateChangeTypeKey];
    auto changesArray = changes.autorelease();
    if (changesArray.count)
        [userInfo setObject:changesArray forKey:NSAccessibilityTextChangeValues];

    if (id wrapper = object.wrapper()) {
        [userInfo setObject:wrapper forKey:NSAccessibilityTextChangeElement];
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        createIsolatedObjectIfNeeded(object);
#endif
    }

    AXPostNotificationWithUserInfo(rootWebArea.wrapper(), NSAccessibilityValueChangedNotification, userInfo.get());
    if (rootWebArea.wrapper() != object.wrapper())
        AXPostNotificationWithUserInfo(object.wrapper(), NSAccessibilityValueChangedNotification, userInfo.get());
}

void AXObjectCache::postTextReplacementPlatformNotification(AccessibilityObject* object, AXTextEditType deletionType, const String& deletedText, AXTextEditType insertionType, const String& insertedText, const VisiblePosition& position)
{
    if (!object)
        object = rootWebArea();

    if (!object)
        return;
    RefPtr protectedObject = object;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    auto changes = adoptNS([[NSMutableArray alloc] initWithCapacity:2]);
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *object, deletionType, deletedText, position))
        [changes addObject:change];
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *object, insertionType, insertedText, position))
        [changes addObject:change];

    if (RefPtr root = rootWebArea())
        postUserInfoForChanges(*root, *object, changes.get());
}

void AXObjectCache::postTextReplacementPlatformNotificationForTextControl(AccessibilityObject* object, const String& deletedText, const String& insertedText)
{
    if (!object)
        object = rootWebArea();

    if (!object)
        return;
    RefPtr protectedObject = object;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    auto changes = adoptNS([[NSMutableArray alloc] initWithCapacity:2]);
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *object, AXTextEditTypeDelete, deletedText, { }))
        [changes addObject:change];
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *object, AXTextEditTypeInsert, insertedText, { }))
        [changes addObject:change];

    if (RefPtr root = rootWebArea())
        postUserInfoForChanges(*root, *object, changes.get());
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

void AXObjectCache::platformHandleFocusedUIElementChanged(Element*, Element*)
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

void AXObjectCache::handleScrolledToAnchor(const Node&)
{
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

static bool isTestAXClientType(AXClientType client)
{
    return client == kAXClientTypeWebKitTesting || client == kAXClientTypeXCTest;
}

bool AXObjectCache::clientIsInTestMode()
{
    return UNLIKELY(isTestAXClientType(_AXGetClientForCurrentRequestUntrusted()));
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
bool AXObjectCache::clientSupportsIsolatedTree()
{
    auto client = _AXGetClientForCurrentRequestUntrusted();
    return client == kAXClientTypeVoiceOver
        || UNLIKELY(client == kAXClientTypeWebKitTesting
        || client == kAXClientTypeXCTest);
}

bool AXObjectCache::isIsolatedTreeEnabled()
{
    static std::atomic<bool> enabled { false };
    if (enabled)
        return true;

    if (!isMainThread()) {
        ASSERT(_AXUIElementRequestServicedBySecondaryAXThread());
        enabled = true;
    } else {
        enabled = DeprecatedGlobalSettings::isAccessibilityIsolatedTreeEnabled() // Used to turn off in apps other than Safari, e.g., Mail.
            && _AXSIsolatedTreeModeFunctionIsAvailable()
            && _AXSIsolatedTreeMode_Soft() != AXSIsolatedTreeModeOff // Used to switch via system defaults.
            && clientSupportsIsolatedTree();
    }

    return enabled;
}

void AXObjectCache::initializeAXThreadIfNeeded()
{
    static bool axThreadInitialized = false;
    if (LIKELY(axThreadInitialized || !isMainThread()))
        return;

    if (_AXSIsolatedTreeModeFunctionIsAvailable() && _AXSIsolatedTreeMode_Soft() == AXSIsolatedTreeModeSecondaryThread) {
        _AXUIElementUseSecondaryAXThread(true);
        axThreadInitialized = true;
    }
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

bool AXObjectCache::shouldSpellCheck()
{
    // This method can be called from non-accessibility contexts, so we need to allow spellchecking if accessibility is disabled.
    if (!accessibilityEnabled())
        return true;

    if (UNLIKELY(forceDeferredSpellChecking()))
        return false;

    auto client = _AXGetClientForCurrentRequestUntrusted();
    // The only AT that we know can handle deferred spellchecking is VoiceOver.
    if (client == kAXClientTypeVoiceOver)
        return false;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (UNLIKELY(isTestAXClientType(client)))
        return true;
    // ITM is currently only ever enabled for VoiceOver, so if it's enabled we can defer spell-checking.
    return !isIsolatedTreeEnabled();
#else
    return true;
#endif
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
Seconds AXObjectCache::platformSelectedTextRangeDebounceInterval() const
{
    return 100_ms;
}
#endif

// TextMarker and TextMarkerRange funcstions.
// FIXME: TextMarker and TextMarkerRange should become classes wrapping the system objects.

RetainPtr<AXTextMarkerRangeRef> textMarkerRangeFromMarkers(AXTextMarkerRef startMarker, AXTextMarkerRef endMarker)
{
    if (!startMarker || !endMarker)
        return nil;

    ASSERT(CFGetTypeID((__bridge CFTypeRef)startMarker) == AXTextMarkerGetTypeID());
    ASSERT(CFGetTypeID((__bridge CFTypeRef)endMarker) == AXTextMarkerGetTypeID());
    return adoptCF(AXTextMarkerRangeCreate(kCFAllocatorDefault, startMarker, endMarker));
}

static RetainPtr<AXTextMarkerRef> AXTextMarkerRangeStart(AXTextMarkerRangeRef textMarkerRange)
{
    ASSERT(textMarkerRange);
    ASSERT(CFGetTypeID((__bridge CFTypeRef)textMarkerRange) == AXTextMarkerRangeGetTypeID());
    return adoptCF(AXTextMarkerRangeCopyStartMarker(textMarkerRange));
}

static RetainPtr<AXTextMarkerRef> AXTextMarkerRangeEnd(AXTextMarkerRangeRef textMarkerRange)
{
    ASSERT(textMarkerRange);
    ASSERT(CFGetTypeID((__bridge CFTypeRef)textMarkerRange) == AXTextMarkerRangeGetTypeID());
    return adoptCF(AXTextMarkerRangeCopyEndMarker(textMarkerRange));
}

static TextMarkerData getBytesFromAXTextMarker(AXTextMarkerRef textMarker)
{
    if (!textMarker)
        return { };

    ASSERT(CFGetTypeID(textMarker) == AXTextMarkerGetTypeID());
    if (CFGetTypeID(textMarker) != AXTextMarkerGetTypeID())
        return { };

    TextMarkerData textMarkerData;
    ASSERT(AXTextMarkerGetLength(textMarker) == sizeof(textMarkerData));
    if (AXTextMarkerGetLength(textMarker) != sizeof(textMarkerData))
        return { };

    memcpy(&textMarkerData, AXTextMarkerGetBytePtr(textMarker), sizeof(textMarkerData));
    return textMarkerData;
}

AccessibilityObject* accessibilityObjectForTextMarker(AXObjectCache* cache, AXTextMarkerRef textMarker)
{
    ASSERT(isMainThread());
    if (!cache || !textMarker)
        return nullptr;

    auto textMarkerData = getBytesFromAXTextMarker(textMarker);
    return cache->objectForTextMarkerData(textMarkerData);
}

// TextMarker <-> VisiblePosition conversion.

AXTextMarkerRef textMarkerForVisiblePosition(AXObjectCache* cache, const VisiblePosition& visiblePos)
{
    ASSERT(isMainThread());
    if (!cache)
        return nil;

    auto textMarkerData = cache->textMarkerDataForVisiblePosition(visiblePos);
    if (!textMarkerData)
        return nil;
    return adoptCF(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&(*textMarkerData), sizeof(*textMarkerData))).autorelease();
}

VisiblePosition visiblePositionForTextMarker(AXObjectCache* cache, AXTextMarkerRef textMarker)
{
    ASSERT(isMainThread());
    if (!cache || !textMarker)
        return { };

    auto textMarkerData = getBytesFromAXTextMarker(textMarker);
    return cache->visiblePositionForTextMarkerData(textMarkerData);
}

// TextMarkerRange <-> VisiblePositionRange conversion.

AXTextMarkerRangeRef textMarkerRangeFromVisiblePositions(AXObjectCache* cache, const VisiblePosition& startPosition, const VisiblePosition& endPosition)
{
    ASSERT(isMainThread());
    if (!cache)
        return nil;

    auto startTextMarker = textMarkerForVisiblePosition(cache, startPosition);
    auto endTextMarker = textMarkerForVisiblePosition(cache, endPosition);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker).autorelease();
}

VisiblePositionRange visiblePositionRangeForTextMarkerRange(AXObjectCache* cache, AXTextMarkerRangeRef textMarkerRange)
{
    ASSERT(isMainThread());

    return {
        visiblePositionForTextMarker(cache, AXTextMarkerRangeStart(textMarkerRange).get()),
        visiblePositionForTextMarker(cache, AXTextMarkerRangeEnd(textMarkerRange).get())
    };
}

// TextMarker <-> CharacterOffset conversion.

AXTextMarkerRef textMarkerForCharacterOffset(AXObjectCache* cache, const CharacterOffset& characterOffset)
{
    ASSERT(isMainThread());

    if (!cache)
        return nil;

    auto textMarkerData = cache->textMarkerDataForCharacterOffset(characterOffset);
    if (!textMarkerData.objectID || textMarkerData.ignored)
        return nil;
    return adoptCF(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData))).autorelease();
}

CharacterOffset characterOffsetForTextMarker(AXObjectCache* cache, AXTextMarkerRef textMarker)
{
    ASSERT(isMainThread());
    if (!cache || !textMarker)
        return { };

    auto textMarkerData = getBytesFromAXTextMarker(textMarker);
    return cache->characterOffsetForTextMarkerData(textMarkerData);
}

// TextMarkerRange <-> SimpleRange conversion.

AXTextMarkerRef startOrEndTextMarkerForRange(AXObjectCache* cache, const std::optional<SimpleRange>& range, bool isStart)
{
    ASSERT(isMainThread());
    if (!cache || !range)
        return nil;

    auto textMarkerData = cache->startOrEndTextMarkerDataForRange(*range, isStart);
    if (!textMarkerData.objectID)
        return nil;
    return adoptCF(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData))).autorelease();
}

AXTextMarkerRangeRef textMarkerRangeFromRange(AXObjectCache* cache, const std::optional<SimpleRange>& range)
{
    ASSERT(isMainThread());
    if (!cache)
        return nil;

    auto startTextMarker = startOrEndTextMarkerForRange(cache, range, true);
    auto endTextMarker = startOrEndTextMarkerForRange(cache, range, false);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker).autorelease();
}

std::optional<SimpleRange> rangeForTextMarkerRange(AXObjectCache* cache, AXTextMarkerRangeRef textMarkerRange)
{
    ASSERT(isMainThread());
    if (!cache || !textMarkerRange)
        return std::nullopt;

    auto startTextMarker = AXTextMarkerRangeStart(textMarkerRange);
    auto endTextMarker = AXTextMarkerRangeEnd(textMarkerRange);
    if (!startTextMarker || !endTextMarker)
        return std::nullopt;

    CharacterOffset startCharacterOffset = characterOffsetForTextMarker(cache, startTextMarker.get());
    CharacterOffset endCharacterOffset = characterOffsetForTextMarker(cache, endTextMarker.get());
    return cache->rangeForUnorderedCharacterOffsets(startCharacterOffset, endCharacterOffset);
}

void AXObjectCache::onSelectedTextChanged(const VisiblePositionRange& selection)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID)) {
        if (selection.isNull())
            tree->setSelectedTextMarkerRange({ });
        else {
            auto startPosition = selection.start.deepEquivalent();
            auto endPosition = selection.end.deepEquivalent();

            if (startPosition.isNull() || endPosition.isNull())
                tree->setSelectedTextMarkerRange({ });
            else {
                if (auto* startObject = get(startPosition.anchorNode()))
                    createIsolatedObjectIfNeeded(*startObject);
                if (auto* endObject = get(endPosition.anchorNode()))
                    createIsolatedObjectIfNeeded(*endObject);

                tree->setSelectedTextMarkerRange({ selection });
            }
        }
    }
#else
    UNUSED_PARAM(selection);
#endif
}

}

#endif // PLATFORM(MAC)
