/*
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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
#import "AXLiveRegionManager.h"
#import "AXNotifications.h"
#import "AXObjectCacheInlines.h"
#import "AXSearchManager.h"
#import "AccessibilityObject.h"
#import "CocoaAccessibilityConstants.h"
#import "DeprecatedGlobalSettings.h"
#import "DocumentView.h"
#import "LocalFrameView.h"
#import "RenderObject.h"
#import "RenderView.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <wtf/Scope.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if USE(APPLE_INTERNAL_SDK)
#import <ApplicationServices/ApplicationServicesPriv.h>
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
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
    case WebCore::AXTextEditTypeReplace:
        return kAXTextEditTypeUnknown; // Does not exist in platform enum.
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

void AXObjectCache::initializeUserDefaultValues()
{
    // This is only set in the constructor, so the page must be reloaded if this default is changed.
    RetainPtr userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:@"com.apple.Accessibility"]);
    gAccessibilityDOMIdentifiersEnabled = [userDefaults boolForKey:@"AXEnableWebKitDOMIdentifier"];
}

void AXObjectCache::attachWrapper(AccessibilityObject& object)
{
    RetainPtr<WebAccessibilityObjectWrapper> wrapper = adoptNS([[WebAccessibilityObjectWrapper alloc] initWithAccessibilityObject:object]);
    object.setWrapper(wrapper.get());
}

static void AXPostNotificationWithUserInfo(AccessibilityObjectWrapper *object, NSString *notification, id userInfo, bool skipSystemNotification = false)
{
    if (id associatedPluginParent = [object _associatedPluginParent])
        object = associatedPluginParent;

    // To simplify monitoring for notifications in tests, repost as a simple NSNotification instead of forcing test infrastucture to setup an IPC client and do all the translation between WebCore types and platform specific IPC types and back
    if (AXObjectCache::shouldRepostNotificationsForTests()) [[unlikely]]
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
    case AXNotification::ActiveDescendantChanged:
        macNotification = NSAccessibilityActiveElementChangedNotification;
        break;
    case AXNotification::AutocorrectionOccured:
        macNotification = NSAccessibilityAutocorrectionOccurredNotification;
        break;
    case AXNotification::CurrentStateChanged:
        macNotification = NSAccessibilityCurrentStateChangedNotification;
        break;
    case AXNotification::FocusedUIElementChanged:
        macNotification = NSAccessibilityFocusedUIElementChangedNotification;
        break;
    case AXNotification::ImageOverlayChanged:
        macNotification = NSAccessibilityImageOverlayChangedNotification;
        break;
    case AXNotification::LayoutComplete:
        macNotification = NSAccessibilityLayoutCompleteNotification;
        break;
    case AXNotification::LabelChanged:
        macNotification = NSAccessibilityTitleChangedNotification;
        break;
    case AXNotification::LoadComplete:
    case AXNotification::FrameLoadComplete:
        macNotification = NSAccessibilityLoadCompleteNotification;
        // Frame loading events are handled by the UIProcess on macOS to improve reliability.
        // On macOS, before notifications are allowed by AppKit to be sent to clients, you need to have a client (e.g. VoiceOver)
        // register for that notification. Because these new processes appear before VO has a chance to register, it will often
        // miss AXLoadComplete notifications. By moving them to the UIProcess, we can eliminate that issue.
        skipSystemNotification = true;
        break;
    case AXNotification::InvalidStatusChanged:
        macNotification = NSAccessibilityInvalidStatusChangedNotification;
        break;
    case AXNotification::SelectedChildrenChanged:
        if (object.isExposableTable())
            macNotification = NSAccessibilitySelectedRowsChangedNotification;
        else
            macNotification = NSAccessibilitySelectedChildrenChangedNotification;
        break;
    case AXNotification::SelectedCellsChanged:
        macNotification = NSAccessibilitySelectedCellsChangedNotification;
        break;
    case AXNotification::SelectedTextChanged:
        macNotification = NSAccessibilitySelectedTextChangedNotification;
        break;
    case AXNotification::CheckedStateChanged:
    case AXNotification::ValueChanged:
        macNotification = NSAccessibilityValueChangedNotification;
        break;
    case AXNotification::LiveRegionCreated:
        macNotification = NSAccessibilityLiveRegionCreatedNotification;
        break;
    case AXNotification::LiveRegionChanged:
        macNotification = NSAccessibilityLiveRegionChangedNotification;
        break;
    case AXNotification::RowCountChanged:
        macNotification = NSAccessibilityRowCountChangedNotification;
        break;
    case AXNotification::RowExpanded:
        macNotification = NSAccessibilityRowExpandedNotification;
        break;
    case AXNotification::RowCollapsed:
        macNotification = NSAccessibilityRowCollapsedNotification;
        break;
    case AXNotification::ElementBusyChanged:
        macNotification = NSAccessibilityElementBusyChangedNotification;
        break;
    case AXNotification::ExpandedChanged:
        macNotification = NSAccessibilityExpandedChangedNotification;
        break;
    case AXNotification::SortDirectionChanged:
        macNotification = NSAccessibilitySortDirectionChangedNotification;
        break;
    case AXNotification::MenuClosed:
        macNotification = (id)kAXMenuClosedNotification;
        break;
    case AXNotification::MenuListItemSelected:
    case AXNotification::MenuListValueChanged:
        macNotification = (id)kAXMenuItemSelectedNotification;
        break;
    case AXNotification::PressDidSucceed:
        macNotification = NSAccessibilityPressDidSucceedNotification;
        break;
    case AXNotification::PressDidFail:
        macNotification = NSAccessibilityPressDidFailNotification;
        break;
    case AXNotification::MenuOpened:
        macNotification = (id)kAXMenuOpenedNotification;
        break;
    case AXNotification::DraggingStarted:
        macNotification = (id)NSAccessibilityDraggingSourceDragBeganNotification;
        break;
    case AXNotification::DraggingEnded:
        macNotification = (id)NSAccessibilityDraggingSourceDragEndedNotification;
        break;
    case AXNotification::DraggingEnteredDropZone:
        macNotification = (id)NSAccessibilityDraggingDestinationDropAllowedNotification;
        break;
    case AXNotification::DraggingDropped:
        macNotification = (id)NSAccessibilityDraggingDestinationDragAcceptedNotification;
        break;
    case AXNotification::DraggingExitedDropZone:
        macNotification = (id)NSAccessibilityDraggingDestinationDragNotAcceptedNotification;
        break;
    case AXNotification::TextCompositionBegan:
        macNotification = NSAccessibilityTextInputMarkingSessionBeganNotification;
        break;
    case AXNotification::TextCompositionEnded:
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
        NSAccessibilityAnnouncementKey: message.createNSString().get(),
    };
    NSAccessibilityPostNotificationWithUserInfo(NSApp, NSAccessibilityAnnouncementRequestedNotification, userInfo);

    // To simplify monitoring of notifications in tests, repost as a simple NSNotification instead of forcing test infrastucture to setup an IPC client and do all the translation between WebCore types and platform specific IPC types and back.
    if (gShouldRepostNotificationsForTests) [[unlikely]] {
        if (RefPtr root = getOrCreate(m_document->view()))
            [root->wrapper() accessibilityPostedNotification:NSAccessibilityAnnouncementRequestedNotification userInfo:userInfo];
    }
}

void AXObjectCache::postPlatformARIANotifyNotification(const String& announcement, NotifyPriority priority, InterruptBehavior interruptBehavior, const String& language)
{
    ASSERT(isMainThread());

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    NSDictionary *userInfo = @{
        NSAccessibilityARIAAnnouncementPriority: notifyPriorityToAXValueString(priority).get(),
        NSAccessibilityARIAAnnouncementInterrupt: interruptBehaviorToAXValueString(interruptBehavior).get(),
        NSAccessibilityAnnouncementKey: announcement.createNSString().get(),
        NSAccessibilityAnnouncementLanguageKey: language.createNSString().get()
    };
    NSAccessibilityPostNotificationWithUserInfo(NSApp, NSAccessibilityAnnouncementRequestedNotification, userInfo);

    if (gShouldRepostNotificationsForTests) [[unlikely]] {
        if (RefPtr root = getOrCreate(m_document->view()))
            [root->wrapper() accessibilityPostedNotification:NSAccessibilityAnnouncementRequestedNotification userInfo:userInfo];
    }
}

void AXObjectCache::postPlatformLiveRegionNotification(AccessibilityObject& object, LiveRegionStatus status, const AttributedString& announcement)
{
    RetainPtr userInfo = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:announcement.nsAttributedString().get(), NSAccessibilityAnnouncementKey, @(status == LiveRegionStatus::Assertive ? NSAccessibilityPriorityHigh : NSAccessibilityPriorityLow), NSAccessibilityPriorityKey, @(YES), NSAccessibilityAnnouncementIsLiveRegionKey, nil]);

    NSAccessibilityPostNotificationWithUserInfo(object.wrapper(), NSAccessibilityAnnouncementRequestedNotification, userInfo.get());

    if (gShouldRepostNotificationsForTests) [[unlikely]] {
        if (RefPtr root = getOrCreate(m_document->view()))
            [root->wrapper() accessibilityPostedNotification:NSAccessibilityAnnouncementRequestedNotification userInfo:userInfo.get()];
    }
}

void AXObjectCache::onDocumentRenderTreeCreation(const Document& document)
{
    RefPtr object = getOrCreate(document.renderView());
    if (!object || !object->isWebArea())
        return;
    queueUnsortedObject(object.releaseNonNull(), PreSortedObjectType::WebArea);
}

void AXObjectCache::deferSortForNewLiveRegion(Ref<AccessibilityObject>&& object)
{
#if PLATFORM(COCOA)
    if (m_liveRegionManager)
        return;
#endif

    queueUnsortedObject(WTFMove(object), PreSortedObjectType::LiveRegion);
}

void AXObjectCache::queueUnsortedObject(Ref<AccessibilityObject>&& object, PreSortedObjectType type)
{
    if (!m_sortedIDListsInitialized)
        return;
    // We only need to do this work if the sorted ID list has already been initialized.

    auto unsortedObjectListIterator = m_deferredUnsortedObjects.ensure(type, [&] {
        return Vector<Ref<AccessibilityObject>>();
    }).iterator;
    unsortedObjectListIterator->value.appendIfNotContains(WTFMove(object));

    if (!m_performCacheUpdateTimer.isActive() && !m_performingDeferredCacheUpdate)
        m_performCacheUpdateTimer.startOneShot(0_s);
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

void AXObjectCache::postTextSelectionChangePlatformNotification(AccessibilityObject* object, const AXTextStateChangeIntent& originalIntent, const VisibleSelection& selection)
{
    RefPtr axObject = object;
    if (!axObject)
        axObject = rootWebArea();

    if (!axObject)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    auto intent = inferDirectionFromIntent(*axObject, originalIntent, selection);

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
            [userInfo setObject:(id)textMarkerRange forKey:NSAccessibilitySelectedTextMarkerRangeAttribute];
    }

    if (id wrapper = axObject->wrapper()) {
        [userInfo setObject:wrapper forKey:NSAccessibilityTextChangeElement];
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        createIsolatedObjectIfNeeded(*axObject);
#endif
    }

    if (RefPtr root = rootWebArea()) {
        AXPostNotificationWithUserInfo(root->wrapper(), NSAccessibilitySelectedTextChangedNotification, userInfo.get());
        if (root->wrapper() != axObject->wrapper())
            AXPostNotificationWithUserInfo(axObject->wrapper(), NSAccessibilitySelectedTextChangedNotification, userInfo.get());
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
    RetainPtr text = string.createNSString();
    NSUInteger length = [text length];
    if (!length)
        return nil;

    auto change = adoptNS([[NSMutableDictionary alloc] initWithCapacity:4]);
    [change setObject:@(platformEditTypeForWebCoreEditType(type)) forKey:NSAccessibilityTextEditType];
    if (length > AXValueChangeTruncationLength) {
        [change setObject:@(length) forKey:NSAccessibilityTextChangeValueLength];
        text = [text substringToIndex:AXValueChangeTruncationLength];
    }
    [change setObject:text.get() forKey:NSAccessibilityTextChangeValue];

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
    RefPtr axObject = object;
    if (!axObject)
        axObject = rootWebArea();

    if (!axObject)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    auto changes = adoptNS([[NSMutableArray alloc] initWithCapacity:2]);
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *axObject, deletionType, deletedText, position))
        [changes addObject:change];
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *axObject, insertionType, insertedText, position))
        [changes addObject:change];

    if (RefPtr root = rootWebArea())
        postUserInfoForChanges(*root, *axObject, changes.get());
}

void AXObjectCache::postTextReplacementPlatformNotificationForTextControl(AccessibilityObject* object, const String& deletedText, const String& insertedText)
{
    RefPtr axObject = object;
    if (!axObject)
        axObject = rootWebArea();

    if (!axObject)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    processQueuedIsolatedNodeUpdates();
#endif

    auto changes = adoptNS([[NSMutableArray alloc] initWithCapacity:2]);
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *axObject, AXTextEditTypeDelete, deletedText, { }))
        [changes addObject:change];
    if (NSDictionary *change = textReplacementChangeDictionary(*this, *axObject, AXTextEditTypeInsert, insertedText, { }))
        [changes addObject:change];

    if (RefPtr root = rootWebArea())
        postUserInfoForChanges(*root, *axObject, changes.get());
}

void AXObjectCache::frameLoadingEventPlatformNotification(RenderView* renderView, AXLoadingEvent loadingEvent)
{
    if (!renderView)
        return;

    if (loadingEvent == AXLoadingEvent::Finished) {
        // It's not always safe to call getOrCreate (e.g. if layout is dirty), so
        // only do so if necessary based on the loading event type.
        RefPtr axWebArea = getOrCreate(*renderView);
        RefPtr document = axWebArea ? axWebArea->document() : nullptr;
        if (document.get() == axWebArea->topDocument())
            postNotification(axWebArea.get(), document.get(), AXNotification::LoadComplete);
        else
            postNotification(axWebArea.get(), document.get(), AXNotification::FrameLoadComplete);
    }
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Element*, Element*)
{
    NSAccessibilityHandleFocusChanged();
    // AXFocusChanged is a test specific notification name and not something a real AT will be listening for
    if (!gShouldRepostNotificationsForTests) [[unlikely]]
        return;

    RefPtr rootWebArea = this->rootWebArea();
    if (!rootWebArea)
        return;

    [rootWebArea->wrapper() accessibilityPostedNotification:NSAccessibilityFocusChangedNotification userInfo:nil];
}

void AXObjectCache::handleScrolledToAnchor(const Node&)
{
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
    for (auto& unsortedObjectsEntry : m_deferredUnsortedObjects)
        addSortedObjects(WTFMove(unsortedObjectsEntry.value), unsortedObjectsEntry.key);
    m_deferredUnsortedObjects.clear();
}

static bool isTestAXClientType(AXClientType client)
{
    return client == kAXClientTypeWebKitTesting || client == kAXClientTypeXCTest;
}

bool AXObjectCache::clientIsInTestMode()
{
    if (isTestAXClientType(_AXGetClientForCurrentRequestUntrusted())) [[unlikely]]
        return true;
    return false;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
bool AXObjectCache::clientSupportsIsolatedTree()
{
    auto client = _AXGetClientForCurrentRequestUntrusted();
    if (client == kAXClientTypeVoiceOver)
        return true;
    if (client == kAXClientTypeWebKitTesting || client == kAXClientTypeXCTest) [[unlikely]]
        return true;
    return false;
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


static bool axThreadInitialized = false;

void AXObjectCache::initializeAXThreadIfNeeded()
{
    if (axThreadInitialized || !isMainThread()) [[likely]]
        return;

    if (_AXSIsolatedTreeModeFunctionIsAvailable() && _AXSIsolatedTreeMode_Soft() == AXSIsolatedTreeModeSecondaryThread) {
        // Initialize the role map before the accessibility thread starts so that it's safe for both threads
        // to use (the only thing that needs to be thread-safe about it is initialization since it's not modified
        // after creation and is never destroyed).
        Accessibility::initializeRoleMap();

        _AXUIElementUseSecondaryAXThread(true);
        axThreadInitialized = true;
    }
}

bool AXObjectCache::isAXThreadInitialized()
{
    return axThreadInitialized;
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

bool AXObjectCache::shouldSpellCheck()
{
    // This method can be called from non-accessibility contexts, so we need to allow spellchecking if accessibility is disabled.
    if (!accessibilityEnabled())
        return true;

    if (forceDeferredSpellChecking()) [[unlikely]]
        return false;

    auto client = _AXGetClientForCurrentRequestUntrusted();
    // The only AT that we know can handle deferred spellchecking is VoiceOver.
    if (client == kAXClientTypeVoiceOver)
        return false;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (isTestAXClientType(client)) [[unlikely]]
        return true;
    // ITM is currently only ever enabled for VoiceOver, so if it's enabled we can defer spell-checking.
    return !isIsolatedTreeEnabled();
#else
    return true;
#endif
}

AXCoreObject::AccessibilityChildrenVector AXObjectCache::sortedLiveRegions()
{
#if PLATFORM(COCOA)
    if (m_liveRegionManager)
        return { };
#endif

    if (!m_sortedIDListsInitialized)
        initializeSortedIDLists();
    return objectsForIDs(m_sortedLiveRegionIDs);
}

AXCoreObject::AccessibilityChildrenVector AXObjectCache::sortedNonRootWebAreas()
{
    if (!m_sortedIDListsInitialized)
        initializeSortedIDLists();
    return objectsForIDs(m_sortedNonRootWebAreaIDs);
}

void AXObjectCache::addSortedObjects(Vector<Ref<AccessibilityObject>>&& objectsToSort, PreSortedObjectType type)
{
    ASSERT(type == PreSortedObjectType::LiveRegion || type == PreSortedObjectType::WebArea);

    if (!m_sortedIDListsInitialized) {
        // Once the sorted ID lists have been initialized for the first time, we rely
        // on handling these dynamic updates to keep them up-to-date. But if that hasn't
        // happened yet, don't bother doing any work.
        return;
    }

    Vector<AXID>& sortedList = type == PreSortedObjectType::LiveRegion ? m_sortedLiveRegionIDs : m_sortedNonRootWebAreaIDs;
    auto updateIsolatedTree = [&] () {
        if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
            if (type == PreSortedObjectType::LiveRegion)
                tree->sortedLiveRegionsDidChange(m_sortedLiveRegionIDs);
            else
                tree->sortedNonRootWebAreasDidChange(m_sortedNonRootWebAreaIDs);
        }
    };

    if (sortedList.isEmpty() && objectsToSort.size() == 1) {
        // Fast path for when there's only object of the given type, avoiding the need for a tree scan.
        sortedList.append(objectsToSort[0]->objectID());
        updateIsolatedTree();
        return;
    }

    RefPtr webArea = rootWebArea();
    if (!webArea)
        return;

    // Any remaining path must update the isolated tree.
    auto onExit = makeScopeExit([&] () {
        updateIsolatedTree();
    });

    unsigned totalExpectedObjectCount = objectsToSort.size();
    for (const auto& existingSortedObjectID : sortedList) {
        bool containedExistingObject = objectsToSort.containsIf([&] (const auto& objectToSort) {
            return objectToSort->objectID() == existingSortedObjectID;
        });

        if (!containedExistingObject)
            ++totalExpectedObjectCount;
    }

    sortedList.clear();
    sortedList.reserveCapacity(totalExpectedObjectCount);

    RefPtr current = rootWebArea();
    while ((current = current ? downcast<AccessibilityObject>(current->nextInPreOrder()) : nullptr)) {
        bool shouldAppend = type == PreSortedObjectType::LiveRegion && current->supportsLiveRegion();
        shouldAppend = shouldAppend || (type == PreSortedObjectType::WebArea && current->isWebArea());

        if (shouldAppend) {
            // There's no reason to ever add the same object twice, as that means we walked over it twice
            // in our pre-order tree traversal.
            ASSERT(!sortedList.contains(current->objectID()));
            sortedList.appendIfNotContains(current->objectID());

            if (sortedList.size() >= totalExpectedObjectCount)
                break;
        }
    }
    sortedList.shrinkToFit();

#if ASSERT_ENABLED
    for (const auto& object : objectsToSort)
        ASSERT(sortedList.contains(object->objectID()));
#endif
}

void AXObjectCache::removeLiveRegion(AccessibilityObject& object)
{
    if (!m_sortedIDListsInitialized)
        return;

    if (m_sortedLiveRegionIDs.removeAll(object.objectID())) {
        if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID))
            tree->sortedLiveRegionsDidChange(m_sortedLiveRegionIDs);
    }
}

void AXObjectCache::initializeSortedIDLists()
{
    if (m_sortedIDListsInitialized)
        return;
    m_sortedIDListsInitialized = true;

#if PLATFORM(COCOA)
    bool includeLiveRegions = !m_liveRegionManager;
#else
    bool includeLiveRegions = true;
#endif

    RefPtr current = rootWebArea();
    while ((current = current ? downcast<AccessibilityObject>(current->nextInPreOrder()) : nullptr)) {
        if (includeLiveRegions && current->supportsLiveRegion()) {
            // There's no reason to ever add the same object twice, as that means we walked over it twice
            // in our pre-order tree traversal.
            ASSERT(!m_sortedLiveRegionIDs.contains(current->objectID()));
            m_sortedLiveRegionIDs.appendIfNotContains(current->objectID());
        } else if (current->isWebArea()) {
            ASSERT(!m_sortedNonRootWebAreaIDs.contains(current->objectID()));
            m_sortedNonRootWebAreaIDs.appendIfNotContains(current->objectID());
        }
    }

    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
        if (m_sortedLiveRegionIDs.size())
            tree->sortedLiveRegionsDidChange(m_sortedLiveRegionIDs);
        if (m_sortedNonRootWebAreaIDs.size())
            tree->sortedNonRootWebAreasDidChange(m_sortedNonRootWebAreaIDs);
    }
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

    memcpySpan(asMutableByteSpan(textMarkerData), AXTextMarkerGetByteSpan(textMarker));
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

AXTextMarkerRef textMarkerForCharacterOffset(AXObjectCache* cache, const CharacterOffset& characterOffset, TextMarkerOrigin origin)
{
    ASSERT(isMainThread());

    if (!cache)
        return nil;

    auto textMarkerData = cache->textMarkerDataForCharacterOffset(characterOffset, origin);
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

} // namespace WebCore

#endif // PLATFORM(MAC)
