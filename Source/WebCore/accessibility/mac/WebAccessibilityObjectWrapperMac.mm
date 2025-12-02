/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebAccessibilityObjectWrapperMac.h"

#import "AXIsolatedTree.h"
#import "AXObjectCacheInlines.h"

#if PLATFORM(MAC)

#import "AXIsolatedObject.h"
#import "AXLogger.h"
#import "AXRemoteFrame.h"
#import "AXSearchManager.h"
#import "AXTextMarker.h"
#import "AXTreeStore.h"
#import "AXTreeStoreInlines.h"
#import "AccessibilityObjectInlines.h"
#import "AccessibilityProgressIndicator.h"
#import "AccessibilityRenderObject.h"
#import "AccessibilityScrollView.h"
#import "AccessibilitySpinButton.h"
#import "AccessibilityTableColumn.h"
#import "CGUtilities.h"
#import "Chrome.h"
#import "ChromeClient.h"
#import "ContextMenuController.h"
#import "DateComponents.h"
#import "DocumentView.h"
#import "ElementInlines.h"
#import "Font.h"
#import "FontCascade.h"
#import "FrameSelection.h"
#import "HTMLAnchorElement.h"
#import "HTMLAreaElement.h"
#import "HTMLFrameOwnerElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "IntRect.h"
#import "LocalFrame.h"
#import "LocalFrameLoaderClient.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "PluginDocument.h"
#import "PluginViewBase.h"
#import "Range.h"
#import "RenderInline.h"
#import "RenderObjectInlines.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "ScrollView.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import "WebCoreFrameView.h"
#import <AppKit/NSAccessibilityConstants.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
#import "ModelPlayerAccessibilityChildren.h"
#endif

using namespace WebCore;

static id attributeValueForTesting(const RefPtr<AXCoreObject>&, NSString *);

// Static C helper functions.

// The CFAttributedStringType representation of the text associated with this accessibility
// object that is specified by the given range.
static NSAttributedString *attributedStringForNSRange(const AXCoreObject& backingObject, NSRange range)
{
    if (!range.length)
        return nil;

    auto markerRange = backingObject.textMarkerRangeForNSRange(range);
    if (!markerRange)
        return nil;

    auto attributedString = backingObject.attributedStringForTextMarkerRange(WTFMove(markerRange), AXCoreObject::SpellCheck::Yes);
    return [attributedString length] ? attributedString.autorelease() : nil;
}

// The RTF representation of the text associated with this accessibility object that is
// specified by the given range.
static NSData *rtfForNSRange(const AXCoreObject& backingObject, NSRange range)
{
    RetainPtr<NSAttributedString> attributedString = attributedStringForNSRange(backingObject, range);
    return [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:@{ }];
}

// Date time helpers.

// VO requests a bit-wise combination of these constants via the API
// AXDateTimeComponents to determine which fields of a datetime value are presented to the user.
typedef NS_OPTIONS(NSUInteger, AXFDateTimeComponent) {
    AXFDateTimeComponentSeconds = 0x0002,
    AXFDateTimeComponentMinutes = 0x0004,
    AXFDateTimeComponentHours = 0x0008,
    AXFDateTimeComponentDays = 0x0020,
    AXFDateTimeComponentMonths = 0x0040,
    AXFDateTimeComponentYears = 0x0080,
    AXFDateTimeComponentEras = 0x0100
};

static inline unsigned convertToAXFDateTimeComponents(DateComponentsType type)
{
    switch (type) {
    case DateComponentsType::Invalid:
        return 0;
    case DateComponentsType::Date:
        return AXFDateTimeComponentDays | AXFDateTimeComponentMonths | AXFDateTimeComponentYears;
    case DateComponentsType::DateTimeLocal:
        return AXFDateTimeComponentSeconds | AXFDateTimeComponentMinutes | AXFDateTimeComponentHours
            | AXFDateTimeComponentDays | AXFDateTimeComponentMonths | AXFDateTimeComponentYears;
    case DateComponentsType::Month:
        return AXFDateTimeComponentMonths | AXFDateTimeComponentYears;
    case DateComponentsType::Time:
        return AXFDateTimeComponentSeconds | AXFDateTimeComponentMinutes | AXFDateTimeComponentHours;
    case DateComponentsType::Week:
        return 0;
    };
}

// VoiceOver expects the datetime value in the local time zone. Since we store it in GMT, we need to convert it to local before returning it to VoiceOver.
// This helper funtion computes the offset to go from local to GMT and returns its opposite.
static inline NSInteger gmtToLocalTimeOffset(DateComponentsType type)
{
    NSTimeZone *timeZone = [NSTimeZone localTimeZone];
    NSDate *now = [NSDate date];
    NSInteger offset = -1 * [timeZone secondsFromGMTForDate:now];
    if (type != DateComponentsType::DateTimeLocal && [timeZone isDaylightSavingTimeForDate:now])
        return offset + 3600; // + number of seconds in an hour.
    return offset;
}

@implementation WebAccessibilityObjectWrapper

- (void)detach
{
    ASSERT(isMainThread());

    // If the IsolatedObject is initialized, do not UnregisterUniqueIdForUIElement here because the wrapper may be in the middle of serving a request on the AX thread.
    // The IsolatedObject is capable to tend to some requests after the live object is gone.
    // In regular mode, UnregisterUniqueIdForUIElement immediately.
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!m_isolatedObjectInitialized)
#endif
        NSAccessibilityUnregisterUniqueIdForUIElement(self);

    [super detach];
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (void)detachIsolatedObject:(AccessibilityDetachmentType)detachmentType
{
    // Only unregister this wrapper if the underlying object or cache is being destroyed. Unregistering it in other cases (like `ElementChanged`)
    // would cause AX clients to get a notification that this wrapper was destroyed, which wouldn't be true.
    if (detachmentType == AccessibilityDetachmentType::ElementDestroyed || detachmentType == AccessibilityDetachmentType::CacheDestroyed)
        NSAccessibilityUnregisterUniqueIdForUIElement(self);
    [super detachIsolatedObject:detachmentType];
}
#endif

- (id)attachmentView
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        RefPtr widget = backingObject->widgetForAttachmentView();
        return widget ? NSAccessibilityUnignoredDescendant(widget->platformWidget()) : nil;
    });
}

#pragma mark SystemInterface wrappers

static inline BOOL AXObjectIsTextMarker(id object)
{
    return object && CFGetTypeID((__bridge CFTypeRef)object) == AXTextMarkerGetTypeID();
}

static inline BOOL AXObjectIsTextMarkerRange(id object)
{
    return object && CFGetTypeID((__bridge CFTypeRef)object) == AXTextMarkerRangeGetTypeID();
}

#pragma mark Other helpers

static IntRect screenToContents(AXCoreObject& axObject, IntRect&& rect)
{
    ASSERT(isMainThread());

    RefPtr document = axObject.document();
    RefPtr frameView = document ? document->view() : nullptr;
    if (!frameView)
        return { };

    IntPoint startPoint = frameView->screenToContents(rect.minXMaxYCorner());
    IntPoint endPoint = frameView->screenToContents(rect.maxXMinYCorner());
    return IntRect(startPoint.x(), startPoint.y(), endPoint.x() - startPoint.x(), endPoint.y() - startPoint.y());
}

#pragma mark Select text helpers

// To be deprecated.
static std::pair<AccessibilitySearchTextCriteria, AccessibilityTextOperation> accessibilityTextCriteriaForParameterizedAttribute(const NSDictionary *parameterizedAttribute)
{
    AccessibilitySearchTextCriteria criteria;
    AccessibilityTextOperation operation;

    RetainPtr<NSString> activityParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextActivity];
    RetainPtr<NSString> ambiguityResolutionParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextAmbiguityResolution];
    RetainPtr<NSString> replacementStringParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextReplacementString];
    RetainPtr<NSArray> searchStringsParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextSearchStrings];

    if ([activityParameter isKindOfClass:[NSString class]]) {
        if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndReplace])
            operation.type = AccessibilityTextOperationType::Replace;
        else if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndCapitalize])
            operation.type = AccessibilityTextOperationType::Capitalize;
        else if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndLowercase])
            operation.type = AccessibilityTextOperationType::Lowercase;
        else if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndUppercase])
            operation.type = AccessibilityTextOperationType::Uppercase;
    }

    criteria.direction = AccessibilitySearchTextDirection::Closest;
    if ([ambiguityResolutionParameter isKindOfClass:[NSString class]]) {
        if ([ambiguityResolutionParameter isEqualToString:NSAccessibilitySelectTextAmbiguityResolutionClosestAfterSelection])
            criteria.direction = AccessibilitySearchTextDirection::Forward;
        else if ([ambiguityResolutionParameter isEqualToString:NSAccessibilitySelectTextAmbiguityResolutionClosestBeforeSelection])
            criteria.direction = AccessibilitySearchTextDirection::Backward;
    }

    if ([replacementStringParameter isKindOfClass:[NSString class]])
        operation.replacementStrings = { String(replacementStringParameter.get()) };

    if ([searchStringsParameter isKindOfClass:[NSArray class]])
        criteria.searchStrings = makeVector<String>(searchStringsParameter.get());

    return std::make_pair(criteria, operation);
}

static AccessibilitySearchTextCriteria accessibilitySearchTextCriteriaForParameterizedAttribute(const NSDictionary *params)
{
    AccessibilitySearchTextCriteria criteria;

    RetainPtr<NSArray> searchStrings = [params objectForKey:NSAccessibilitySearchTextSearchStrings];
    RetainPtr<NSString> start = [params objectForKey:NSAccessibilitySearchTextStartFrom];
    RetainPtr<NSString> direction = [params objectForKey:NSAccessibilitySearchTextDirection];

    if ([searchStrings isKindOfClass:[NSArray class]])
        criteria.searchStrings = makeVector<String>(searchStrings.get());

    if ([start isKindOfClass:[NSString class]]) {
        if ([start isEqualToString:NSAccessibilitySearchTextStartFromBegin])
            criteria.start = AccessibilitySearchTextStartFrom::Begin;
        else if ([start isEqualToString:NSAccessibilitySearchTextStartFromEnd])
            criteria.start = AccessibilitySearchTextStartFrom::End;
    }

    if ([direction isKindOfClass:[NSString class]]) {
        if ([direction isEqualToString:NSAccessibilitySearchTextDirectionBackward])
            criteria.direction = AccessibilitySearchTextDirection::Backward;
        else if ([direction isEqualToString:NSAccessibilitySearchTextDirectionClosest])
            criteria.direction = AccessibilitySearchTextDirection::Closest;
        else if ([direction isEqualToString:NSAccessibilitySearchTextDirectionAll])
            criteria.direction = AccessibilitySearchTextDirection::All;
    }

    return criteria;
}

static AccessibilityTextOperation accessibilityTextOperationForParameterizedAttribute(AXObjectCache* axObjectCache, const NSDictionary *parameterizedAttribute)
{
    AccessibilityTextOperation operation;

    RetainPtr<NSArray> markerRanges = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationMarkerRanges];
    RetainPtr<NSString> operationType = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationType];
    RetainPtr<NSArray> individualReplacementStrings = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationIndividualReplacementStrings];
    RetainPtr<NSString> replacementString = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationReplacementString];
    RetainPtr<NSNumber> smartReplace = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationSmartReplace];

    if ([markerRanges isKindOfClass:[NSArray class]]) {
        operation.textRanges = makeVector(markerRanges.get(), [&axObjectCache] (id markerRange) {
            ASSERT(AXObjectIsTextMarkerRange(markerRange));
            return rangeForTextMarkerRange(axObjectCache, (AXTextMarkerRangeRef)markerRange);
        });
    }

    if ([operationType isKindOfClass:[NSString class]]) {
        if ([operationType isEqualToString:NSAccessibilityTextOperationReplace])
            operation.type = AccessibilityTextOperationType::Replace;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationReplacePreserveCase])
            operation.type = AccessibilityTextOperationType::ReplacePreserveCase;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationCapitalize])
            operation.type = AccessibilityTextOperationType::Capitalize;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationLowercase])
            operation.type = AccessibilityTextOperationType::Lowercase;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationUppercase])
            operation.type = AccessibilityTextOperationType::Uppercase;
    }

    if ([individualReplacementStrings isKindOfClass:[NSArray class]]) {
        operation.replacementStrings = makeVector<String>(individualReplacementStrings.get());
    } else if ([replacementString isKindOfClass:[NSString class]])
        operation.replacementStrings = { String(replacementString.get()) };

    if ([smartReplace isKindOfClass:[NSNumber class]])
        operation.smartReplace = [smartReplace boolValue] ? AccessibilityTextOperationSmartReplace::Yes : AccessibilityTextOperationSmartReplace::No;

    return operation;
}

static std::pair<AXTextMarkerRange, AccessibilitySearchDirection> misspellingSearchCriteriaForParameterizedAttribute(const NSDictionary *params)
{
    RetainPtr<id> markerRangeRef = [params objectForKey:NSAccessibilityStartTextMarkerRangeParam];
    if (!AXObjectIsTextMarkerRange(markerRangeRef.get()))
        return { };

    std::pair<AXTextMarkerRange, AccessibilitySearchDirection> criteria;
    criteria.first = AXTextMarkerRange { (AXTextMarkerRangeRef)markerRangeRef.get() };

    RetainPtr<NSNumber> forward = [params objectForKey:NSAccessibilitySearchTextDirection];
    if ([forward isKindOfClass:[NSNumber class]])
        criteria.second = [forward boolValue] ? AccessibilitySearchDirection::Next : AccessibilitySearchDirection::Previous;
    else
        criteria.second = AccessibilitySearchDirection::Next;

    return criteria;
}

#pragma mark Text Marker helpers

static RetainPtr<AXTextMarkerRef> nextTextMarker(AXObjectCache* cache, const AXTextMarker& marker)
{
    if (!cache)
        return nil;

    auto nextMarker = cache->nextTextMarker(marker);
    return nextMarker ? nextMarker.platformData() : nil;
}

static RetainPtr<AXTextMarkerRef> previousTextMarker(AXObjectCache* cache, const AXTextMarker& marker)
{
    if (!cache)
        return nil;

    auto previousMarker = cache->previousTextMarker(marker);
    return previousMarker ? previousMarker.platformData() : nil;
}

static NSAttributedString *attributedStringForTextMarkerRange(const AXCoreObject& object, AXTextMarkerRangeRef textMarkerRangeRef, AXCoreObject::SpellCheck spellCheck)
{
    if (!textMarkerRangeRef)
        return nil;

    return object.attributedStringForTextMarkerRange({ textMarkerRangeRef }, spellCheck).autorelease();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray*)accessibilityActionNames
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    // All elements should get ShowMenu and ScrollToVisible.
    // But certain earlier VoiceOver versions do not support scroll to visible, and it confuses them to see it in the list.
    static NeverDestroyed<RetainPtr<NSArray>> defaultElementActions = @[NSAccessibilityShowMenuAction, NSAccessibilityScrollToVisibleAction];

    // Action elements allow Press.
    // The order is important to VoiceOver, which expects the 'default' action to be the first action. In this case the default action should be press.
    static NeverDestroyed<RetainPtr<NSArray>> actionElementActions = @[NSAccessibilityPressAction, NSAccessibilityShowMenuAction, NSAccessibilityScrollToVisibleAction];

    // Menu elements allow Press and Cancel.
    static NeverDestroyed<RetainPtr<NSArray>> menuElementActions = [actionElementActions.get() arrayByAddingObject:NSAccessibilityCancelAction];

    static NeverDestroyed<RetainPtr<NSArray>> incrementorActions = [defaultElementActions.get() arrayByAddingObjectsFromArray:@[NSAccessibilityIncrementAction, NSAccessibilityDecrementAction]];

    if (backingObject->isSlider() || (backingObject->isSpinButton() && backingObject->spinButtonType() == SpinButtonType::Standalone)) {
        // Non-standalone spinbuttons should not advertise the increment and decrement actions because they have separate increment and decrement controls.
        return incrementorActions.get().get();
    }

    if (backingObject->isMenuRelated())
        return menuElementActions.get().get();
    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityActionNames];
    if (backingObject->supportsPressAction())
        return actionElementActions.get().get();

    return defaultElementActions.get().get();
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (NSArray *)_additionalAccessibilityAttributeNames:(const RefPtr<AXCoreObject>&)backingObject
{
    NSMutableArray *additional = [NSMutableArray array];
    if (backingObject->supportsActiveDescendant())
        [additional addObject:NSAccessibilityActiveElementAttribute];

    if (backingObject->supportsARIAOwns())
        [additional addObject:NSAccessibilityOwnsAttribute];

    if (backingObject->supportsExpanded() || backingObject->isSummary())
        [additional addObject:NSAccessibilityExpandedAttribute];

    if (backingObject->isScrollbar()
        || backingObject->isRadioGroup()
        || backingObject->isSplitter()
        || backingObject->isToolbar()
        || backingObject->role() == AccessibilityRole::HorizontalRule)
        [additional addObject:NSAccessibilityOrientationAttribute];

    if (backingObject->supportsDragging())
        [additional addObject:NSAccessibilityGrabbedAttribute];

    if (backingObject->supportsDropping())
        [additional addObject:NSAccessibilityDropEffectsAttribute];

    if (backingObject->isExposableTable() && backingObject->supportsSelectedRows())
        [additional addObject:NSAccessibilitySelectedRowsAttribute];

    if (backingObject->isTreeGrid() && backingObject->isExposableTable())
        [additional addObject:NSAccessibilityOrientationAttribute];

    if (backingObject->supportsSetSize())
        [additional addObject:NSAccessibilityARIASetSizeAttribute];

    if (backingObject->supportsPosInSet())
        [additional addObject:NSAccessibilityARIAPosInSetAttribute];

    if (backingObject->supportsKeyShortcuts())
        [additional addObject:NSAccessibilityKeyShortcutsAttribute];

    if (backingObject->supportsLiveRegion()) {
        [additional addObject:NSAccessibilityARIALiveAttribute];
        [additional addObject:NSAccessibilityARIARelevantAttribute];
    }

    bool addedHasPopup = false;
    if (backingObject->hasPopup()) {
        addedHasPopup = true;
        [additional addObject:NSAccessibilityHasPopupAttribute];
    }
    bool addedAriaAtomic = false;
    bool addedSortDirection = false;
    for (RefPtr ancestor = backingObject; ancestor; ancestor = ancestor->parentObject()) {
        if (!addedHasPopup && ancestor->isLink() && ancestor->hasPopup()) {
            // Require ancestor to be a link, matching AXCoreObject::selfOrAncestorLinkHasPopup.
            addedHasPopup = true;
            [additional addObject:NSAccessibilityHasPopupAttribute];
        }

        if (!addedAriaAtomic && ancestor->supportsLiveRegion()) {
            addedAriaAtomic = true;
            [additional addObject:NSAccessibilityARIAAtomicAttribute];
        }

        if (!addedSortDirection) {
            auto sortDirection = ancestor->sortDirection();
            if (sortDirection != AccessibilitySortDirection::None && sortDirection != AccessibilitySortDirection::Invalid) {
                addedSortDirection = true;
                [additional addObject:NSAccessibilitySortDirectionAttribute];
            }
        }
    }

    if (backingObject->supportsDatetimeAttribute())
        [additional addObject:NSAccessibilityDatetimeValueAttribute];

    if (backingObject->supportsRequiredAttribute())
        [additional addObject:NSAccessibilityRequiredAttribute];

    if (backingObject->isMathRoot()) {
        // The index of a square root is always known, so there's no object associated with it.
        if (!backingObject->isMathSquareRoot())
            [additional addObject:NSAccessibilityMathRootIndexAttribute];
        [additional addObject:NSAccessibilityMathRootRadicandAttribute];
    } else if (backingObject->isMathFraction()) {
        [additional addObject:NSAccessibilityMathFractionNumeratorAttribute];
        [additional addObject:NSAccessibilityMathFractionDenominatorAttribute];
        [additional addObject:NSAccessibilityMathLineThicknessAttribute];
    } else if (backingObject->isMathSubscriptSuperscript()) {
        [additional addObject:NSAccessibilityMathBaseAttribute];
        [additional addObject:NSAccessibilityMathSubscriptAttribute];
        [additional addObject:NSAccessibilityMathSuperscriptAttribute];
    } else if (backingObject->isMathUnderOver()) {
        [additional addObject:NSAccessibilityMathBaseAttribute];
        [additional addObject:NSAccessibilityMathUnderAttribute];
        [additional addObject:NSAccessibilityMathOverAttribute];
    } else if (backingObject->isMathFenced()) {
        [additional addObject:NSAccessibilityMathFencedOpenAttribute];
        [additional addObject:NSAccessibilityMathFencedCloseAttribute];
    } else if (backingObject->isMathMultiscript()) {
        [additional addObject:NSAccessibilityMathBaseAttribute];
        [additional addObject:NSAccessibilityMathPrescriptsAttribute];
        [additional addObject:NSAccessibilityMathPostscriptsAttribute];
    }

    // isStaticText() objects already note their support for path in `accessibilityAttributeNames`.
    if (!backingObject->isStaticText() && backingObject->supportsPath())
        [additional addObject:NSAccessibilityPathAttribute];

    if (backingObject->supportsExpandedTextValue())
        [additional addObject:NSAccessibilityExpandedTextValueAttribute];

    if (!backingObject->brailleLabel().isEmpty())
        [additional addObject:NSAccessibilityBrailleLabelAttribute];

    if (!backingObject->brailleRoleDescription().isEmpty())
        [additional addObject:NSAccessibilityBrailleRoleDescriptionAttribute];

    if (backingObject->detailedByObjects().size())
        [additional addObject:NSAccessibilityDetailsElementsAttribute];

    if (backingObject->errorMessageObjects().size())
        [additional addObject:NSAccessibilityErrorMessageElementsAttribute];

    if (!backingObject->keyShortcuts().isEmpty())
        [additional addObject:NSAccessibilityKeyShortcutsAttribute];

    if (backingObject->titleUIElement())
        [additional addObject:NSAccessibilityTitleUIElementAttribute];

    if (backingObject->isColumnHeader() || backingObject->isRowHeader())
        [additional addObject:NSAccessibilitySortDirectionAttribute];

    // Only expose AXARIACurrent attribute when the element is set to be current item.
    if (backingObject->currentState() != AccessibilityCurrentState::False)
        [additional addObject:NSAccessibilityARIACurrentAttribute];

    if (backingObject->isTreeItem() && backingObject->supportsCheckedState()) {
        // Tree items normally do not support value, but should if they are checkable.
        [additional addObject:NSAccessibilityValueAttribute];
    }

    return additional;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityAttributeNames
{
    AXTRACE("WebAccessibilityObjectWrapper accessibilityAttributeNames"_s);

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    // TODO: we definitely don't want to return accessibilityAttributeNames for AccessibilityScrollViews,
    // only for external attachments like PDFs. Figure out some more specific logic to do this only when needed.
#if !ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeNames];
#endif // !ENABLE_ACCESSIBILITY_LOCAL_FRAME

    static NeverDestroyed<RetainPtr<NSArray>> attributes = @[
        NSAccessibilityRoleAttribute,
        NSAccessibilitySubroleAttribute,
        NSAccessibilityRoleDescriptionAttribute,
        NSAccessibilityChildrenAttribute,
        NSAccessibilityChildrenInNavigationOrderAttribute,
        NSAccessibilityHelpAttribute,
        NSAccessibilityParentAttribute,
        NSAccessibilityPositionAttribute,
        NSAccessibilitySizeAttribute,
        NSAccessibilityTitleAttribute,
        NSAccessibilityDescriptionAttribute,
        NSAccessibilityValueAttribute,
        NSAccessibilityFocusedAttribute,
        NSAccessibilityEnabledAttribute,
        NSAccessibilityWindowAttribute,
        NSAccessibilitySelectedTextMarkerRangeAttribute,
        NSAccessibilityStartTextMarkerAttribute,
        NSAccessibilityEndTextMarkerAttribute,
        NSAccessibilityVisitedAttribute,
        NSAccessibilityLinkedUIElementsAttribute,
        NSAccessibilitySelectedAttribute,
        NSAccessibilityBlockQuoteLevelAttribute,
        NSAccessibilityTopLevelUIElementAttribute,
        NSAccessibilityLanguageAttribute,
        NSAccessibilityDOMIdentifierAttribute,
        NSAccessibilityDOMClassListAttribute,
        NSAccessibilityFocusableAncestorAttribute,
        NSAccessibilityEditableAncestorAttribute,
        NSAccessibilityHighestEditableAncestorAttribute,
        NSAccessibilityTextInputMarkedRangeAttribute,
        NSAccessibilityTextInputMarkedTextMarkerRangeAttribute,
        NSAccessibilityVisibleCharacterRangeAttribute,
        NSAccessibilityRelativeFrameAttribute,
        // AppKit needs to know the screen height in order to do the coordinate conversion.
        NSAccessibilityPrimaryScreenHeightAttribute,
        // All objects should expose the ARIA busy attribute (ARIA 1.1 with ISSUE-538).
        NSAccessibilityElementBusyAttribute
    ];
    static NeverDestroyed spinButtonCommonAttributes = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityValueDescriptionAttribute];
        [tempArray addObject:NSAccessibilityMinValueAttribute];
        [tempArray addObject:NSAccessibilityMaxValueAttribute];
        return tempArray;
    }();
    static NeverDestroyed compositeSpinButtonAttributes = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:spinButtonCommonAttributes.get().get()]);
        [tempArray addObject:NSAccessibilityIncrementButtonAttribute];
        [tempArray addObject:NSAccessibilityDecrementButtonAttribute];
        return tempArray;
    }();
    static NeverDestroyed anchorAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityURLAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityLinkRelationshipTypeAttribute];
        return tempArray;
    }();
    static NeverDestroyed webAreaAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        // WebAreas should not expose AXSubrole.
        [tempArray removeObject:NSAccessibilitySubroleAttribute];
        // WebAreas should not expose ancestor attributes
        [tempArray removeObject:NSAccessibilityFocusableAncestorAttribute];
        [tempArray removeObject:NSAccessibilityEditableAncestorAttribute];
        [tempArray removeObject:NSAccessibilityHighestEditableAncestorAttribute];
        [tempArray addObject:NSAccessibilityLinkUIElementsAttribute];
        [tempArray addObject:NSAccessibilityLoadedAttribute];
        [tempArray addObject:NSAccessibilityLayoutCountAttribute];
        [tempArray addObject:NSAccessibilityLoadingProgressAttribute];
        [tempArray addObject:NSAccessibilityURLAttribute];
        [tempArray addObject:NSAccessibilityCaretBrowsingEnabledAttribute];
        [tempArray addObject:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute];
        return tempArray;
    }();
    static NeverDestroyed textAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityNumberOfCharactersAttribute];
        [tempArray addObject:NSAccessibilitySelectedTextAttribute];
        [tempArray addObject:NSAccessibilitySelectedTextRangeAttribute];
        [tempArray addObject:NSAccessibilityInsertionPointLineNumberAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityPlaceholderValueAttribute];
        [tempArray addObject:NSAccessibilityValueAutofillAvailableAttribute];
        return tempArray;
    }();
    static NeverDestroyed listAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityActiveElementAttribute];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed listBoxAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:listAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed rangeAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityMinValueAttribute];
        [tempArray addObject:NSAccessibilityMaxValueAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilityValueDescriptionAttribute];
        return tempArray;
    }();
    static NeverDestroyed menuBarAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed menuAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed menuItemAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityTitleAttribute];
        [tempArray addObject:NSAccessibilityDescriptionAttribute];
        [tempArray addObject:NSAccessibilityHelpAttribute];
        [tempArray addObject:NSAccessibilitySelectedAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdCharAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdVirtualKeyAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdGlyphAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdModifiersAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemMarkCharAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemPrimaryUIElementAttribute];
        [tempArray addObject:NSAccessibilityServesAsTitleForUIElementsAttribute];
        [tempArray addObject:NSAccessibilityFocusedAttribute];
        return tempArray;
    }();
    static NeverDestroyed<RetainPtr<NSArray>> sharedControlAttrs = @[
        NSAccessibilityAccessKeyAttribute,
        NSAccessibilityRequiredAttribute,
        NSAccessibilityInvalidAttribute,
    ];
    static NeverDestroyed<RetainPtr<NSArray>> sharedComboBoxAttrs = @[
        NSAccessibilitySelectedChildrenAttribute,
        NSAccessibilityExpandedAttribute,
        NSAccessibilityOrientationAttribute,
    ];
    static NeverDestroyed controlAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObjectsFromArray:sharedControlAttrs.get().get()];
        return tempArray;
    }();
    static NeverDestroyed buttonAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        // Buttons should not expose AXValue.
        [tempArray removeObject:NSAccessibilityValueAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        return tempArray;
    }();
    static NeverDestroyed popupOrToggleButtonAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:buttonAttrs.get().get()]);
        // Popup and toggle buttons do expose AXValue.
        [tempArray addObject:NSAccessibilityValueAttribute];
        return tempArray;
    }();
    static NeverDestroyed comboBoxAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:controlAttrs.get().get()]);
        [tempArray addObjectsFromArray:sharedComboBoxAttrs.get().get()];
        return tempArray;
    }();
    static NeverDestroyed textComboBoxAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:textAttrs.get().get()]);
        [tempArray addObjectsFromArray:sharedControlAttrs.get().get()];
        [tempArray addObjectsFromArray:sharedComboBoxAttrs.get().get()];
        return tempArray;
    }();
    static NeverDestroyed tableAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityVisibleRowsAttribute];
        [tempArray addObject:NSAccessibilityColumnsAttribute];
        [tempArray addObject:NSAccessibilityVisibleColumnsAttribute];
        [tempArray addObject:NSAccessibilityVisibleCellsAttribute];
        [tempArray addObject:NSAccessibilityColumnHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityRowHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityHeaderAttribute];
        [tempArray addObject:NSAccessibilityColumnCountAttribute];
        [tempArray addObject:NSAccessibilityRowCountAttribute];
        [tempArray addObject:NSAccessibilityARIAColumnCountAttribute];
        [tempArray addObject:NSAccessibilityARIARowCountAttribute];
        [tempArray addObject:NSAccessibilitySelectedCellsAttribute];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityTableLevelAttribute];
        return tempArray;
    }();
    static NeverDestroyed tableRowAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityIndexAttribute];
        // FIXME: Consider exposing NSAccessibilityARIARowIndexAttribute and NSAccessibilityRowIndexDescriptionAttribute,
        // i.e. aria-rowindex and aria-rowindextext, just like already done for cells
        // https://bugs.webkit.org/show_bug.cgi?id=296857
        return tempArray;
    }();
    static NeverDestroyed tableColAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityIndexAttribute];
        [tempArray addObject:NSAccessibilityHeaderAttribute];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityVisibleRowsAttribute];
        return tempArray;
    }();
    static NeverDestroyed tableCellAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityRowIndexRangeAttribute];
        [tempArray addObject:NSAccessibilityColumnIndexRangeAttribute];
        [tempArray addObject:NSAccessibilityColumnHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityRowHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityARIAColumnIndexAttribute];
        [tempArray addObject:NSAccessibilityARIARowIndexAttribute];
        [tempArray addObject:NSAccessibilityColumnIndexDescriptionAttribute];
        [tempArray addObject:NSAccessibilityRowIndexDescriptionAttribute];
        return tempArray;
    }();
    static NeverDestroyed groupAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityInlineTextAttribute];
        return tempArray;
    }();
    static NeverDestroyed inputImageAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:buttonAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityURLAttribute];
        return tempArray;
    }();
    static NeverDestroyed secureFieldAttributes = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityPlaceholderValueAttribute];
        [tempArray addObject:NSAccessibilitySelectedTextRangeAttribute];
        return tempArray;
    }();
    static NeverDestroyed tabListAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityTabsAttribute];
        [tempArray addObject:NSAccessibilityContentsAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        return tempArray;
    }();
    static NeverDestroyed outlineAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilitySelectedRowsAttribute];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityColumnsAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        return tempArray;
    }();
    static NeverDestroyed outlineRowAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:tableRowAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityDisclosingAttribute];
        [tempArray addObject:NSAccessibilityDisclosedByRowAttribute];
        [tempArray addObject:NSAccessibilityDisclosureLevelAttribute];
        [tempArray addObject:NSAccessibilityDisclosedRowsAttribute];
        [tempArray removeObject:NSAccessibilityValueAttribute];
        return tempArray;
    }();
    static NeverDestroyed scrollViewAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityContentsAttribute];
        [tempArray addObject:NSAccessibilityHorizontalScrollBarAttribute];
        [tempArray addObject:NSAccessibilityVerticalScrollBarAttribute];
        return tempArray;
    }();
    static NeverDestroyed imageAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityImageOverlayElementsAttribute];
        [tempArray addObject:NSAccessibilityEmbeddedImageDescriptionAttribute];
        [tempArray addObject:NSAccessibilityURLAttribute];
        return tempArray;
    }();
    static NeverDestroyed videoAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        // This should represent the URL of the video content, not the poster.
        [tempArray addObject:NSAccessibilityURLAttribute];
        return tempArray;
    }();
    static NeverDestroyed staticTextAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityIntersectionWithSelectionRangeAttribute];
        [tempArray addObject:NSAccessibilityPathAttribute];
        return tempArray;
    }();

    RetainPtr<NSArray> objectAttributes = nil;
    if (backingObject->isSecureField())
        objectAttributes = secureFieldAttributes.get().get();
    else if (backingObject->isWebArea())
        objectAttributes = webAreaAttrs.get().get();
    else if (backingObject->isStaticText())
        objectAttributes = staticTextAttrs.get().get();
    else if (backingObject->isComboBox() && backingObject->isTextControl())
        objectAttributes = textComboBoxAttrs.get().get();
    else if (backingObject->isComboBox())
        objectAttributes = comboBoxAttrs.get().get();
    else if (backingObject->isTextControl())
        objectAttributes = textAttrs.get().get();
    else if (backingObject->isLink())
        objectAttributes = anchorAttrs.get().get();
    else if (backingObject->isImage())
        objectAttributes = imageAttrs.get().get();
    else if (backingObject->isTree())
        objectAttributes = outlineAttrs.get().get();
    else if (backingObject->isExposableTable())
        objectAttributes = tableAttrs.get().get();
    else if (backingObject->isTableColumn())
        objectAttributes = tableColAttrs.get().get();
    else if (backingObject->isExposedTableCell())
        objectAttributes = tableCellAttrs.get().get();
    else if (backingObject->isExposedTableRow()) {
        // An ARIA table row can be collapsed and expanded, so it needs the extra attributes.
        if (backingObject->isARIATreeGridRow())
            objectAttributes = outlineRowAttrs.get().get();
        else
            objectAttributes = tableRowAttrs.get().get();
    } else if (backingObject->isTreeItem())
        objectAttributes = outlineRowAttrs.get().get();
    else if (backingObject->isListBox())
        objectAttributes = listBoxAttrs.get().get();
    else if (backingObject->isList())
        objectAttributes = listAttrs.get().get();
    else if (backingObject->isProgressIndicator() || backingObject->isSlider() || backingObject->isSplitter())
        objectAttributes = rangeAttrs.get().get();
    // These are processed in order because an input image is a button, and a button is a control.
    else if (backingObject->isInputImage())
        objectAttributes = inputImageAttrs.get().get();
    else if (backingObject->isPopUpButton() || backingObject->isToggleButton())
        objectAttributes = popupOrToggleButtonAttrs.get().get();
    else if (backingObject->isButton())
        objectAttributes = buttonAttrs.get().get();
    else if (backingObject->isControl())
        objectAttributes = controlAttrs.get().get();

    else if (backingObject->isGroup() || backingObject->isListItem() || backingObject->role() == AccessibilityRole::Figure)
        objectAttributes = groupAttrs.get().get();
    else if (backingObject->isTabList())
        objectAttributes = tabListAttrs.get().get();
    else if (backingObject->isScrollView())
        objectAttributes = scrollViewAttrs.get().get();
    else if (backingObject->isSpinButton()) {
        if (backingObject->spinButtonType() == SpinButtonType::Composite)
            objectAttributes = compositeSpinButtonAttributes.get().get();
        else
            objectAttributes = spinButtonCommonAttributes.get().get();
    } else if (backingObject->isMenu())
        objectAttributes = menuAttrs.get().get();
    else if (backingObject->isMenuBar())
        objectAttributes = menuBarAttrs.get().get();
    else if (backingObject->isMenuItem())
        objectAttributes = menuItemAttrs.get().get();
    else if (backingObject->isVideo())
        objectAttributes = videoAttrs.get().get();

    if (!objectAttributes)
        objectAttributes = attributes.get().get();

    // Adding these additional attributes is separated so that we only call arrayByAddingObjectsFromArray
    // (which does a copy) if we have any uncommon attributes to add.
    RetainPtr<NSArray> additionalAttributes = [self _additionalAccessibilityAttributeNames:backingObject];
    if ([additionalAttributes count])
        objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:additionalAttributes.get()];

    return objectAttributes.unsafeGet();
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (id)remoteAccessibilityParentObject
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    return backingObject ? backingObject->remoteParent().unsafeGet() : nil;
}

static void convertToVector(NSArray* array, AccessibilityObject::AccessibilityChildrenVector& vector)
{
    unsigned length = [array count];
    vector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        if (RefPtr object = [[array objectAtIndex:i] axBackingObject])
            vector.append(object.releaseNonNull());
    }
}

- (AXTextMarkerRangeRef)selectedTextMarkerRange
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    auto range = backingObject->selectedTextMarkerRange();
    if (!range.start().isValid() || !range.end().isValid())
        return nil;

    return range;
}

- (id)_associatedPluginParent
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    return [self _associatedPluginParentWith:self.axBackingObject];
}

- (id)_associatedPluginParentWith:(const RefPtr<AXCoreObject>&)backingObject
{
    if (!backingObject || !backingObject->hasApplePDFAnnotationAttribute())
        return nil;

    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject || !backingObject->hasApplePDFAnnotationAttribute())
            return nil;
        RefPtr document = dynamicDowncast<PluginDocument>(backingObject->document());
        if (!document)
            return nil;
        RefPtr widget = document->pluginWidget();
        if (!widget)
            return nil;
        RefPtr element = backingObject->element();
        return widget->accessibilityAssociatedPluginParentForElement(element.get());
    });
}

static void WebTransformCGPathToNSBezierPath(void* info, const CGPathElement *element)
{
    auto points = pointsSpan(element);
    NSBezierPath *bezierPath = (__bridge NSBezierPath *)info;
    switch (element->type) {
    case kCGPathElementMoveToPoint:
        [bezierPath moveToPoint:NSPointFromCGPoint(points[0])];
        break;
    case kCGPathElementAddLineToPoint:
        [bezierPath lineToPoint:NSPointFromCGPoint(points[0])];
        break;
    case kCGPathElementAddCurveToPoint: {
        [bezierPath curveToPoint:NSPointFromCGPoint(points[0]) controlPoint1:NSPointFromCGPoint(points[1]) controlPoint2:NSPointFromCGPoint(points[2])];
        break;
    }
    case kCGPathElementCloseSubpath:
        [bezierPath closePath];
        break;
    default:
        break;
    }
}

- (NSBezierPath *)bezierPathFromPath:(CGPathRef)path
{
    NSBezierPath *bezierPath = [NSBezierPath bezierPath];
    CGPathApply(path, (__bridge void*)bezierPath, WebTransformCGPathToNSBezierPath);
    return bezierPath;
}

- (NSBezierPath *)path
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject)
        return NULL;

    Path path = backingObject->elementPath();
    if (path.isEmpty())
        return NULL;

    RetainPtr<CGPathRef> transformedPath = [self convertPathToScreenSpace:path];
    return [self bezierPathFromPath:transformedPath.get()];
}

// `unignoredChildren` must be the children of `backingObject`.
static NSArray *transformSpecialChildrenCases(AXCoreObject& backingObject, const Vector<Ref<AXCoreObject>>& unignoredChildren)
{
#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
    if (backingObject.isModel()) {
        auto modelChildren = backingObject.modelElementChildren();
        if (modelChildren.children.size()) {
            return createNSArray(WTFMove(modelChildren.children), [](auto&& child) -> id {
                return child.get();
            }).autorelease();
        }
    }
#endif

    if (!unignoredChildren.size()) {
        if (RetainPtr widgetChildren = renderWidgetChildren(backingObject))
            return widgetChildren.unsafeGet();
    }
    return nil;
}

static NSArray *children(AXCoreObject& backingObject)
{
    const auto& unignoredChildren = backingObject.stitchedUnignoredChildren();
    RetainPtr<NSArray> specialChildren = transformSpecialChildrenCases(backingObject, unignoredChildren);
    if ([specialChildren count])
        return specialChildren.unsafeGet();

    // The tree's (AXOutline) children are supposed to be its rows and columns.
    // The ARIA spec doesn't have columns, so we just need rows.
    if (backingObject.isTree())
        return makeNSArray(backingObject.ariaTreeRows());

    // A tree item should only expose its content as its children (not its rows)
    if (backingObject.isTreeItem())
        return makeNSArray(backingObject.ariaTreeItemContent());

    return makeNSArray(unignoredChildren);
}

static RetainPtr<NSString> roleString(AXCoreObject& backingObject)
{
    String roleString = backingObject.rolePlatformString();
    if (!roleString.isEmpty())
        return roleString.createNSString();
    return NSAccessibilityUnknownRole;
}

static RetainPtr<NSString> subroleString(AXCoreObject& backingObject)
{
    if (backingObject.isEmptyGroup())
        return NSAccessibilityEmptyGroupSubrole;

    String subrole = backingObject.subrolePlatformString();
    if (!subrole.isEmpty())
        return subrole.createNSString();
    return nil;
}

static RetainPtr<NSString> roleDescription(AXCoreObject& backingObject)
{
    String roleDescription = backingObject.roleDescription();
    if (!roleDescription.isEmpty())
        return roleDescription.createNSString();

    RetainPtr axRole = roleString(backingObject);
    RetainPtr subrole = subroleString(backingObject);
    // Fallback to the system role description.
    // If we get the same string back, then as a last resort, return unknown.
    RetainPtr<NSString> systemRoleDescription = NSAccessibilityRoleDescription(axRole.get(), subrole.get());
    if (![systemRoleDescription isEqualToString:axRole.get()])
        return systemRoleDescription;
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
}

static id scrollViewParent(AXCoreObject& axObject)
{
    if (!axObject.isScrollView())
        return nil;

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (RefPtr crossFrameParent = axObject.crossFrameParentObject())
        return crossFrameParent->wrapper();
#endif

    // If this scroll view provides it's parent object (because it's a sub-frame), then
    // we should not find the remoteAccessibilityParent.
    if (axObject.parentObject())
        return nil;

    if (RetainPtr platformWidget = axObject.platformWidget())
        return NSAccessibilityUnignoredAncestor(platformWidget.get());

    return axObject.remoteParent().unsafeGet();
}

- (id)windowElement:(NSString *)attributeName
{
    if (RetainPtr<id> remoteParent = self.remoteAccessibilityParentObject) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        return [remoteParent accessibilityAttributeValue:attributeName];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    RefPtr axScrollView = backingObject ? backingObject->axScrollView() : nullptr;
    return axScrollView ? [axScrollView->protectedPlatformWidget() window] : nil;
}

// FIXME: split up this function in a better way.
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attributeName
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityAttributeValue:"_s, String(attributeName)));

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper "_s, hex(reinterpret_cast<uintptr_t>(self))));
        return nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityRoleAttribute])
        return roleString(*backingObject).autorelease();

    if ([attributeName isEqualToString: NSAccessibilitySubroleAttribute])
        return subroleString(*backingObject).autorelease();

    if ([attributeName isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return roleDescription(*backingObject).autorelease();

    if ([attributeName isEqualToString: NSAccessibilityParentAttribute]) {
        // This will return the parent of the AXScrollArea, if this is a AccessibilityScrollView.
        if (RetainPtr scrollView = scrollViewParent(*backingObject))
            return scrollView.unsafeGet();

        // Tree item (changed to AXRows) can only report the tree (AXOutline) as its parent.
        if (backingObject->isTreeItem()) {
            RefPtr parent = backingObject->parentObjectUnignored();
            while (parent) {
                if (parent->isTree())
                    return parent->wrapper();
                parent = parent->parentObjectUnignored();
            }
        }

        RefPtr parent = backingObject->parentObjectUnignored();
        if (!parent)
            return nil;

#if !ENABLE_ACCESSIBILITY_LOCAL_FRAME
        // In WebKit1, the scroll view is provided by the system (the attachment view), so the parent
        // should be reported directly as such.
        if (backingObject->isWebArea() && parent->isAttachment())
            return [parent->protectedWrapper() attachmentView];
#endif // !ENABLE_ACCESSIBILITY_LOCAL_FRAME

        return parent->wrapper();
    }

    if ([attributeName isEqualToString:NSAccessibilityChildrenAttribute] || [attributeName isEqualToString:NSAccessibilityChildrenInNavigationOrderAttribute])
        return children(*backingObject);

    if ([attributeName isEqualToString:NSAccessibilitySelectedChildrenAttribute]) {
        auto selectedChildren = backingObject->selectedChildren();
        return selectedChildren.size() ? makeNSArray(WTFMove(selectedChildren)) : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityActiveElementAttribute]) {
        RefPtr activeDescendant = backingObject->activeDescendant();
        return activeDescendant ? activeDescendant->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityVisibleChildrenAttribute]) {
        if (backingObject->isListBox())
            return makeNSArray(backingObject->visibleChildren());
        if (backingObject->isList())
            return children(*backingObject);
        return nil;
    }

    if (backingObject->isWebArea()) {
        if ([attributeName isEqualToString:NSAccessibilityLinkUIElementsAttribute])
            return makeNSArray(backingObject->documentLinks());

        if ([attributeName isEqualToString:NSAccessibilityLoadedAttribute])
            return [NSNumber numberWithBool:backingObject->isLoaded()];
        if ([attributeName isEqualToString:NSAccessibilityLayoutCountAttribute])
            return @(backingObject->layoutCount());
        if ([attributeName isEqualToString:NSAccessibilityLoadingProgressAttribute])
            return @(backingObject->loadingProgress());
        if ([attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
            return [NSNumber numberWithBool:backingObject->preventKeyboardDOMEventDispatch()];
        if ([attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
            return [NSNumber numberWithBool:backingObject->caretBrowsingEnabled()];
    }

    if (backingObject->isTextControl()) {
        if ([attributeName isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
            return @(backingObject->textLength());

        if ([attributeName isEqualToString:NSAccessibilitySelectedTextAttribute]) {
            String selectedText = backingObject->selectedText();
            if (selectedText.isNull())
                return nil;
            return selectedText.createNSString().autorelease();
        }

        if ([attributeName isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
            return [NSValue valueWithRange:backingObject->selectedTextRange()];

        if ([attributeName isEqualToString:NSAccessibilityInsertionPointLineNumberAttribute]) {
            int lineNumber = backingObject->insertionPointLineNumber();
            return lineNumber >= 0 ? @(lineNumber) : nil;
        }
    }

    if (backingObject->isStaticText()) {
        if ([attributeName isEqualToString:NSAccessibilityIntersectionWithSelectionRangeAttribute])
            return [self intersectionWithSelectionRange];
    }

    if ([attributeName isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute]) {
        if (backingObject->isSecureField())
            return nil;
        // FIXME: Get actual visible range. <rdar://problem/4712101>
        if (backingObject->isTextControl())
            return [NSValue valueWithRange:NSMakeRange(0, backingObject->textLength())];
        return [NSValue valueWithRange:[self accessibilityVisibleCharacterRange]];
    }

    if ([attributeName isEqualToString: NSAccessibilityURLAttribute]) {
        URL url = backingObject->url();
        if (url.isNull())
            return nil;
        return url.createNSURL().autorelease();
    }

    if ([attributeName isEqualToString:NSAccessibilityIncrementButtonAttribute]) {
        RefPtr incrementButton = backingObject->incrementButton();
        return incrementButton ? incrementButton->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityDecrementButtonAttribute]) {
        RefPtr decrementButton = backingObject->decrementButton();
        return decrementButton ? decrementButton->wrapper() : nil;
    }

    if ([attributeName isEqualToString: NSAccessibilityVisitedAttribute])
        return [NSNumber numberWithBool: backingObject->isVisitedLink()];

    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute]) {
        if (backingObject->isAttachment()) {
            RetainPtr<id> attachmentView = [self attachmentView];
            if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute])
                return [attachmentView accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }

        return backingObject->title().createNSString().autorelease();
    }

    if ([attributeName isEqualToString:NSAccessibilityDescriptionAttribute]) {
        if (backingObject->isAttachment()) {
            id attachmentView = [self attachmentView];
            if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [attachmentView accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return backingObject->descriptionAttributeValue().createNSString().autorelease();
    }

    if ([attributeName isEqualToString:NSAccessibilityValueAttribute]) {
        if (backingObject->isAttachment()) {
            id attachmentView = [self attachmentView];
            if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute])
                return [attachmentView accessibilityAttributeValue:NSAccessibilityValueAttribute];
        }

        auto value = backingObject->value();
        return WTF::switchOn(value,
            [] (bool& typedValue) -> id { return @(typedValue); },
            [] (unsigned& typedValue) -> id { return @(typedValue); },
            [] (float& typedValue) -> id { return @(typedValue); },
            [] (String& typedValue) -> id { return typedValue.createNSString().autorelease(); },
            [&backingObject] (WallTime& typedValue) -> id {
                NSInteger offset = gmtToLocalTimeOffset(backingObject->dateTimeComponentsType());
                auto time = typedValue.secondsSinceEpoch().value();
                NSDate *gmtDate = [NSDate dateWithTimeIntervalSince1970:time];
                return [NSDate dateWithTimeInterval:offset sinceDate:gmtDate];
            },
            [] (AccessibilityButtonState& typedValue) -> id { return @((unsigned)typedValue); },
            [] (AXCoreObject*& typedValue) { return typedValue ? (id)typedValue->wrapper() : nil; },
            [] (auto&) { return nil; }
        );
    }

    if ([attributeName isEqualToString:NSAccessibilityDateTimeComponentsAttribute])
        return @(convertToAXFDateTimeComponents(backingObject->dateTimeComponentsType()));

    if ([attributeName isEqualToString:bridge_cast(kAXMenuItemMarkCharAttribute)]) {
        const unichar ch = 0x2713; // ✓ used on Mac for selected menu items.
        return (backingObject->isChecked()) ? [NSString stringWithCharacters:&ch length:1] : nil;
    }

    if ([attributeName isEqualToString: NSAccessibilityMinValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (backingObject->isIndeterminate())
            return @0;
        return [NSNumber numberWithFloat:backingObject->minValueForRange()];
    }

    if ([attributeName isEqualToString: NSAccessibilityMaxValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (backingObject->isIndeterminate())
            return @0;
        return [NSNumber numberWithFloat:backingObject->maxValueForRange()];
    }

    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return [self baseAccessibilityHelpText];

    if ([attributeName isEqualToString:NSAccessibilityFocusedAttribute])
        return @(backingObject->isFocused());

    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: backingObject->isEnabled()];

    if ([attributeName isEqualToString:NSAccessibilitySizeAttribute]) {
        auto size = backingObject->size();
        if (backingObject->isControl())
            Accessibility::adjustControlSize(size);
        return [NSValue valueWithSize:(CGSize)size];
    }

    if ([attributeName isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return @(backingObject->primaryScreenRect().height());

    if ([attributeName isEqualToString:NSAccessibilityPositionAttribute])
        return [NSValue valueWithPoint:(CGPoint)backingObject->screenRelativePosition()];

    if ([attributeName isEqualToString:NSAccessibilityPathAttribute])
        return [self path];

    if ([attributeName isEqualToString:NSAccessibilityLineRectsAndTextAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<NSArray *>([protectedSelf = retainPtr(self)] () -> RetainPtr<NSArray> {
            return protectedSelf.get().lineRectsAndText;
        });
    }

    if ([attributeName isEqualToString:NSAccessibilityImageOverlayElementsAttribute]) {
        auto imageOverlayElements = backingObject->imageOverlayElements();
        return imageOverlayElements ? makeNSArray(*imageOverlayElements) : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityEmbeddedImageDescriptionAttribute])
        return backingObject->embeddedImageDescription().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityWindowAttribute]
        || [attributeName isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [self windowElement:attributeName];

    if ([attributeName isEqualToString:NSAccessibilityAccessKeyAttribute]) {
        auto accessKey = backingObject->accessKey();
        if (accessKey.isNull())
            return nil;
        return accessKey.createNSString().autorelease();
    }

    if ([attributeName isEqualToString:NSAccessibilityLinkRelationshipTypeAttribute])
        return backingObject->linkRelValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityTabsAttribute] && backingObject->isTabList())
        return makeNSArray(backingObject->tabChildren());

    if ([attributeName isEqualToString:NSAccessibilityContentsAttribute])
        return makeNSArray(backingObject->contents());

    if (backingObject->isExposableTable()) {
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute])
            return makeNSArray(backingObject->rows());

        if ([attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute])
            return makeNSArray(backingObject->visibleRows());

        // TODO: distinguish between visible and non-visible columns
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute]
            || [attributeName isEqualToString:NSAccessibilityVisibleColumnsAttribute])
            return makeNSArray(backingObject->columns());

        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            auto selectedChildren = backingObject->selectedChildren();
            return selectedChildren.size() ? makeNSArray(WTFMove(selectedChildren)) : nil;
        }

        // HTML tables don't support this attribute yet.
        if ([attributeName isEqualToString:NSAccessibilitySelectedColumnsAttribute])
            return nil;

        if ([attributeName isEqualToString:NSAccessibilitySelectedCellsAttribute])
            return makeNSArray(backingObject->selectedCells());

        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute])
            return makeNSArray(backingObject->columnHeaders());

        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            RefPtr headerContainer = backingObject->tableHeaderContainer();
            return headerContainer ? headerContainer->wrapper() : nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute])
            return makeNSArray(backingObject->rowHeaders());

        if ([attributeName isEqualToString:NSAccessibilityVisibleCellsAttribute])
            return makeNSArray(backingObject->cells());

        if ([attributeName isEqualToString:NSAccessibilityColumnCountAttribute])
            return @(backingObject->columnCount());

        if ([attributeName isEqualToString:NSAccessibilityRowCountAttribute])
            return @(backingObject->rowCount());

        if ([attributeName isEqualToString:NSAccessibilityARIAColumnCountAttribute])
            return @(backingObject->axColumnCount());

        if ([attributeName isEqualToString:NSAccessibilityARIARowCountAttribute])
            return @(backingObject->axRowCount());
    }

    if (backingObject->isTableColumn()) {
        if ([attributeName isEqualToString:NSAccessibilityIndexAttribute])
            return @(backingObject->columnIndex());

        // rows attribute for a column is the list of all the elements in that column at each row
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute]
            || [attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute])
            return makeNSArray(backingObject->unignoredChildren());

        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            RefPtr header = backingObject->columnHeader();
            return header ? header->wrapper() : nil;
        }
    }

    if (backingObject->isExposedTableCell()) {
        if ([attributeName isEqualToString:NSAccessibilityRowIndexRangeAttribute]) {
            auto rowRange = backingObject->rowIndexRange();
            return [NSValue valueWithRange:NSMakeRange(rowRange.first, rowRange.second)];
        }

        if ([attributeName isEqualToString:NSAccessibilityColumnIndexRangeAttribute]) {
            auto columnRange = backingObject->columnIndexRange();
            return [NSValue valueWithRange:NSMakeRange(columnRange.first, columnRange.second)];
        }

        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute])
            return makeNSArray(backingObject->columnHeaders());

        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute])
            return makeNSArray(backingObject->rowHeaders());

        if ([attributeName isEqualToString:NSAccessibilityARIAColumnIndexAttribute]) {
            if (std::optional columnIndex = backingObject->axColumnIndex())
                return @(*columnIndex);
            return @(-1);
        }

        if ([attributeName isEqualToString:NSAccessibilityARIARowIndexAttribute]) {
            if (std::optional rowIndex = backingObject->axRowIndex())
                return @(*rowIndex);
            return @(-1);
        }

        if ([attributeName isEqualToString:NSAccessibilityColumnIndexDescriptionAttribute]) {
            String columnIndexText = backingObject->axColumnIndexText();
            return !columnIndexText.isNull() ? columnIndexText.createNSString().autorelease() : nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityRowIndexDescriptionAttribute]) {
            String rowIndexText = backingObject->axRowIndexText();
            return !rowIndexText.isNull() ? rowIndexText.createNSString().autorelease() : nil;
        }
    }

    if (backingObject->isTree()) {
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            auto selectedChildren = backingObject->selectedChildren();
            return selectedChildren.size() ? makeNSArray(WTFMove(selectedChildren)) : nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute])
            return makeNSArray(backingObject->ariaTreeRows());

        // TreeRoles do not support columns, but Mac AX expects to be able to ask about columns at the least.
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute])
            return @[];
    }

    if ([attributeName isEqualToString:NSAccessibilityIndexAttribute]) {
        if (backingObject->isTreeItem()) {
            RefPtr parent = backingObject->parentObject();
            for (; parent && !parent->isTree(); parent = parent->parentObject())
            { }

            if (!parent)
                return nil;

            // Find the index of this item by iterating the parents.
            auto rowsCopy = parent->ariaTreeRows();
            size_t count = rowsCopy.size();
            for (size_t k = 0; k < count; ++k) {
                if (rowsCopy[k]->wrapper() == self)
                    return @(k);
            }

            return nil;
        }

        if (backingObject->isExposedTableRow())
            return @(backingObject->rowIndex());
    }

    // The rows that are considered inside this row.
    if ([attributeName isEqualToString:NSAccessibilityDisclosedRowsAttribute]) {
        if (backingObject->isTreeItem() || backingObject->isARIATreeGridRow())
            return makeNSArray(backingObject->disclosedRows());
    }

    // The row that contains this row. It should be the same as the first parent that is a treeitem.
    if ([attributeName isEqualToString:NSAccessibilityDisclosedByRowAttribute]) {
        if (backingObject->isTreeItem()) {
            RefPtr<AXCoreObject> parent = backingObject->parentObject();
            while (parent) {
                if (parent->isTreeItem())
                    return parent->wrapper();
                // If the parent is the tree itself, then this value == nil.
                if (parent->isTree())
                    return nil;
                parent = parent->parentObject();
            }
            return nil;
        }

        if (backingObject->isARIATreeGridRow()) {
            RefPtr row = backingObject->disclosedByRow();
            return row ? row->wrapper() : nil;
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityDisclosureLevelAttribute]) {
        // Convert from 1-based level (from aria-level spec) to 0-based level (Mac)
        int level = backingObject->hierarchicalLevel();
        if (level > 0)
            level -= 1;
        return @(level);
    }
    if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute])
        return [NSNumber numberWithBool:backingObject->isExpanded()];

    if ([attributeName isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute])
        return (id)[self selectedTextMarkerRange];

    if ([attributeName isEqualToString:NSAccessibilityStartTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            if (RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(backingObject->treeID())))
                return tree->firstMarker().platformData().bridgingAutorelease();
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            if (!backingObject)
                return nil;

            CheckedPtr cache = backingObject->axObjectCache();
            RefPtr document = backingObject->document();
            return (id)textMarkerForVisiblePosition(cache.get(), startOfDocument(document.get()));
        });
    }

    if ([attributeName isEqualToString:NSAccessibilityEndTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            if (RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(backingObject->treeID())))
                return tree->lastMarker().platformData().bridgingAutorelease();
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            if (!backingObject)
                return nil;

            CheckedPtr cache = backingObject->axObjectCache();
            RefPtr document = backingObject->document();
            return (id)textMarkerForVisiblePosition(cache.get(), endOfDocument(document.get()));
        });
    }

    if ([attributeName isEqualToString:NSAccessibilityBlockQuoteLevelAttribute])
        return @(backingObject->blockquoteLevel());
    if ([attributeName isEqualToString:NSAccessibilityTableLevelAttribute])
        return @(backingObject->tableLevel());

    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute])
        return makeNSArray(backingObject->linkedObjects());

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return [NSNumber numberWithBool:backingObject->isSelected()];

    if ([attributeName isEqualToString: NSAccessibilityARIACurrentAttribute])
        return backingObject->currentValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
        // FIXME: change to return an array instead of a single object.
        RefPtr object = backingObject->titleUIElement();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityValueDescriptionAttribute])
        return backingObject->valueDescription().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityOrientationAttribute]) {
        AccessibilityOrientation elementOrientation = backingObject->orientation();
        if (elementOrientation == AccessibilityOrientation::Vertical)
            return NSAccessibilityVerticalOrientationValue;
        if (elementOrientation == AccessibilityOrientation::Horizontal)
            return NSAccessibilityHorizontalOrientationValue;
        if (elementOrientation == AccessibilityOrientation::Undefined)
            return NSAccessibilityUnknownOrientationValue;
        return nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityHorizontalScrollBarAttribute]) {
        RefPtr scrollBar = backingObject->scrollBar(AccessibilityOrientation::Horizontal);
        return scrollBar ? scrollBar->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityVerticalScrollBarAttribute]) {
        RefPtr scrollBar = backingObject->scrollBar(AccessibilityOrientation::Vertical);
        return scrollBar ? scrollBar->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilitySortDirectionAttribute]) {
        switch (backingObject->sortDirectionIncludingAncestors()) {
        case AccessibilitySortDirection::Ascending:
            return NSAccessibilityAscendingSortDirectionValue;
        case AccessibilitySortDirection::Descending:
            return NSAccessibilityDescendingSortDirectionValue;
        default:
            return NSAccessibilityUnknownSortDirectionValue;
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityLanguageAttribute])
        return backingObject->languageIncludingAncestors().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        return [NSNumber numberWithBool:backingObject->isExpanded()];

    if ([attributeName isEqualToString:NSAccessibilityRequiredAttribute])
        return [NSNumber numberWithBool:backingObject->isRequired()];

    if ([attributeName isEqualToString:NSAccessibilityInvalidAttribute])
        return backingObject->invalidStatus().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityOwnsAttribute])
        return makeNSArray(backingObject->ownedObjects());

    if ([attributeName isEqualToString:NSAccessibilityARIAPosInSetAttribute])
        return @(backingObject->posInSet());
    if ([attributeName isEqualToString:NSAccessibilityARIASetSizeAttribute])
        return @(backingObject->setSize());

    if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        return [NSNumber numberWithBool:backingObject->isGrabbed()];

    if ([attributeName isEqualToString:NSAccessibilityDropEffectsAttribute])
        return createNSArray(backingObject->determineDropEffects()).autorelease();

    if ([attributeName isEqualToString:NSAccessibilityPlaceholderValueAttribute])
        return backingObject->placeholderValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityValueAutofillAvailableAttribute])
        return @(backingObject->isValueAutofillAvailable());

    if ([attributeName isEqualToString:NSAccessibilityValueAutofillTypeAttribute]) {
        switch (backingObject->valueAutofillButtonType()) {
        case AutoFillButtonType::None:
            return @"none";
        case AutoFillButtonType::Credentials:
            return @"credentials";
        case AutoFillButtonType::Contacts:
            return @"contacts";
        case AutoFillButtonType::StrongPassword:
            return @"strong password";
        case AutoFillButtonType::CreditCard:
            return @"credit card";
        case AutoFillButtonType::Loading:
            return @"loading";
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityHasPopupAttribute])
        return [NSNumber numberWithBool:backingObject->selfOrAncestorLinkHasPopup()];

    if ([attributeName isEqualToString:NSAccessibilityDatetimeValueAttribute])
        return backingObject->datetimeAttributeValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityInlineTextAttribute]) {
        // FIXME: After the AccessibilityStitchedText feature ships, we can probably remove this
        // and all related code (making sure not to break backported versions of WebKit which won't
        // have the corresponding VoiceOver changes).
        return @(backingObject->isInlineText());
    }

    // ARIA Live region attributes.
    if ([attributeName isEqualToString:NSAccessibilityARIALiveAttribute])
        return backingObject->liveRegionStatus().createNSString().autorelease();
    if ([attributeName isEqualToString:NSAccessibilityARIARelevantAttribute])
        return backingObject->liveRegionRelevant().createNSString().autorelease();
    if ([attributeName isEqualToString:NSAccessibilityARIAAtomicAttribute])
        return [NSNumber numberWithBool:backingObject->liveRegionAtomic()];
    if ([attributeName isEqualToString:NSAccessibilityElementBusyAttribute])
        return [NSNumber numberWithBool:backingObject->isBusy()];

    // MathML Attributes.
    if (backingObject->isMathElement()) {
        if ([attributeName isEqualToString:NSAccessibilityMathRootIndexAttribute]) {
            RefPtr rootIndex = backingObject->mathRootIndexObject();
            return rootIndex ? rootIndex->wrapper() : nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityMathRootRadicandAttribute]) {
            auto radicand = backingObject->mathRadicand();
            return radicand ? makeNSArray(*radicand) : nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityMathFractionNumeratorAttribute])
            return (backingObject->mathNumeratorObject()) ? backingObject->mathNumeratorObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathFractionDenominatorAttribute])
            return (backingObject->mathDenominatorObject()) ? backingObject->mathDenominatorObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathBaseAttribute])
            return (backingObject->mathBaseObject()) ? backingObject->mathBaseObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathSubscriptAttribute])
            return (backingObject->mathSubscriptObject()) ? backingObject->mathSubscriptObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathSuperscriptAttribute])
            return (backingObject->mathSuperscriptObject()) ? backingObject->mathSuperscriptObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathUnderAttribute])
            return (backingObject->mathUnderObject()) ? backingObject->mathUnderObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathOverAttribute])
            return (backingObject->mathOverObject()) ? backingObject->mathOverObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathFencedOpenAttribute])
            return backingObject->mathFencedOpenString().createNSString().autorelease();
        if ([attributeName isEqualToString:NSAccessibilityMathFencedCloseAttribute])
            return backingObject->mathFencedCloseString().createNSString().autorelease();
        if ([attributeName isEqualToString:NSAccessibilityMathLineThicknessAttribute])
            return [NSNumber numberWithInteger:backingObject->mathLineThickness()];
        if ([attributeName isEqualToString:NSAccessibilityMathPostscriptsAttribute])
            return [self accessibilityMathPostscriptPairs];
        if ([attributeName isEqualToString:NSAccessibilityMathPrescriptsAttribute])
            return [self accessibilityMathPrescriptPairs];
    }

    if ([attributeName isEqualToString:NSAccessibilityExpandedTextValueAttribute])
        return backingObject->expandedTextValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityDOMIdentifierAttribute])
        return backingObject->identifierAttribute().createNSString().autorelease();
    if ([attributeName isEqualToString:NSAccessibilityDOMClassListAttribute])
        return createNSArray(backingObject->classList()).autorelease();

    // This allows us to connect to a plugin that creates a shadow node for editing (like PDFs).
    if ([attributeName isEqualToString:NSAccessibilityAssociatedPluginParentAttribute])
        return [self _associatedPluginParentWith:backingObject];

    // This used to be a testing-only attribute, but unfortunately some ATs do actually request it.
    if ([attributeName isEqualToString:NSAccessibilityDRTSpeechAttributeAttribute])
        return [self baseAccessibilitySpeechHint];

    if ([attributeName isEqualToString:NSAccessibilityPopupValueAttribute])
        return backingObject->popupValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityKeyShortcutsAttribute])
        return backingObject->keyShortcuts().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityIsInDescriptionListTermAttribute])
        return [NSNumber numberWithBool:backingObject->isInDescriptionListTerm()];

    if ([attributeName isEqualToString:NSAccessibilityDetailsElementsAttribute])
        return makeNSArray(backingObject->detailedByObjects());

    if ([attributeName isEqualToString:NSAccessibilityBrailleLabelAttribute])
        return backingObject->brailleLabel().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityBrailleRoleDescriptionAttribute])
        return backingObject->brailleRoleDescription().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityRelativeFrameAttribute]) {
        auto frame = backingObject->relativeFrame();
        if (backingObject->isControl())
            Accessibility::adjustControlSize(frame);
        return [NSValue valueWithRect:(NSRect)frame];
    }

    if ([attributeName isEqualToString:NSAccessibilityErrorMessageElementsAttribute]) {
        // Only expose error messages for objects in an invalid state.
        // https://www.w3.org/TR/wai-aria-1.2/#aria-errormessage
        if (backingObject->invalidStatus() == "false"_s)
            return nil;
        return makeNSArray(backingObject->errorMessageObjects());
    }

    if ([attributeName isEqualToString:NSAccessibilityFocusableAncestorAttribute]) {
        RefPtr object = backingObject->focusableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityEditableAncestorAttribute]) {
        RefPtr object = backingObject->editableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityHighestEditableAncestorAttribute]) {
        RefPtr object = backingObject->highestEditableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityTextInputMarkedRangeAttribute]) {
        auto range = backingObject->textInputMarkedTextMarkerRange();
        auto nsRange = range.nsRange();
        return range && nsRange ? [NSValue valueWithRange:*nsRange] : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityTextInputMarkedTextMarkerRangeAttribute]) {
        auto range = backingObject->textInputMarkedTextMarkerRange();
        return range ? range.platformData().bridgingAutorelease() : nil;
    }

    // VoiceOver property to ignore certain groups.
    if ([attributeName isEqualToString:NSAccessibilityAutoInteractableAttribute])
        return @(backingObject->isRemoteFrame());

    if (AXObjectCache::clientIsInTestMode())
        return attributeValueForTesting(backingObject, attributeName);
    return nil;
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

id attributeValueForTesting(const RefPtr<AXCoreObject>& backingObject, NSString *attributeName)
{
    ASSERT_WITH_MESSAGE(AXObjectCache::clientIsInTestMode(), "Should be used for testing only, not for AT clients.");

    if ([attributeName isEqualToString:NSAccessibilityARIARoleAttribute])
        return backingObject->computedRoleString().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityStringValueAttribute])
        return backingObject->stringValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityDateTimeComponentsTypeAttribute])
        return [NSNumber numberWithUnsignedShort:(uint8_t)backingObject->dateTimeComponentsType()];

    if ([attributeName isEqualToString:NSAccessibilityControllersAttribute])
        return makeNSArray(backingObject->controllers());

    if ([attributeName isEqualToString:NSAccessibilityControllerForAttribute])
        return makeNSArray(backingObject->controlledObjects());

    if ([attributeName isEqualToString:NSAccessibilityDescribedByAttribute])
        return makeNSArray(backingObject->describedByObjects());

    if ([attributeName isEqualToString:NSAccessibilityDescriptionForAttribute])
        return makeNSArray(backingObject->descriptionForObjects());

    if ([attributeName isEqualToString:NSAccessibilityDetailsForAttribute])
        return makeNSArray(backingObject->detailsForObjects());

    if ([attributeName isEqualToString:NSAccessibilityErrorMessageForAttribute])
        return makeNSArray(backingObject->errorMessageForObjects());

    if ([attributeName isEqualToString:NSAccessibilityFlowFromAttribute])
        return makeNSArray(backingObject->flowFromObjects());

    if ([attributeName isEqualToString:NSAccessibilityFlowToAttribute])
        return makeNSArray(backingObject->flowToObjects());

    if ([attributeName isEqualToString:NSAccessibilityLabelledByAttribute])
        return makeNSArray(backingObject->labeledByObjects());

    if ([attributeName isEqualToString:NSAccessibilityLabelForAttribute])
        return makeNSArray(backingObject->labelForObjects());

    if ([attributeName isEqualToString:NSAccessibilityOwnersAttribute])
        return makeNSArray(backingObject->owners());

    if ([attributeName isEqualToString:NSAccessibilityARIAPressedIsPresentAttribute])
        return [NSNumber numberWithBool:backingObject->pressedIsPresent()];

    if ([attributeName isEqualToString:NSAccessibilityAutocompleteValueAttribute])
        return backingObject->autoCompleteValue().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityClickPointAttribute])
        return [NSValue valueWithPoint:backingObject->clickPoint()];

    if ([attributeName isEqualToString:NSAccessibilityIsIndeterminateAttribute])
        return [NSNumber numberWithBool:backingObject->isIndeterminate()];

    if ([attributeName isEqualToString:NSAccessibilityIsMultiSelectableAttribute])
        return [NSNumber numberWithBool:backingObject->isMultiSelectable()];

    if ([attributeName isEqualToString:NSAccessibilityIsOnScreenAttribute])
        return [NSNumber numberWithBool:backingObject->isOnScreen()];

    if ([attributeName isEqualToString:NSAccessibilityIsInTableAttribute]) {
        RefPtr table = Accessibility::findAncestor(*backingObject, false, [&] (const auto& ancestor) {
            return ancestor.isTable();
        });
        return [NSNumber numberWithBool:!!table.get()];
    }

    if ([attributeName isEqualToString:NSAccessibilityIsRemoteFrameAttribute])
        return [NSNumber numberWithBool:backingObject->isRemoteFrame()];

    if ([attributeName isEqualToString:NSAccessibilityInfoStringForTestingAttribute])
        return backingObject->infoStringForTesting().createNSString().autorelease();

    if ([attributeName isEqualToString:NSAccessibilityPageRelativePositionAttribute])
        return [NSValue valueWithPoint:(CGPoint)backingObject->relativeFrame().location()];

    return nil;
}

- (id)parameterizedAttributeValueForTesting:(NSString *)attribute parameter:(id)parameter backingObject:(const RefPtr<AXCoreObject>&)backingObject
{
    // This should've been null-checked already.
    RELEASE_ASSERT(parameter);

    AXTextMarkerRef markerRef = nil;
    AXTextMarkerRangeRef markerRangeRef = nil;
    NSRange nsRange = { 0, 0 };

    if (AXObjectIsTextMarker(parameter))
        markerRef = (AXTextMarkerRef)parameter;
    else if (AXObjectIsTextMarkerRange(parameter))
        markerRangeRef = (AXTextMarkerRangeRef)parameter;
    else if ([parameter isKindOfClass:[NSValue class]] && nsValueHasObjCType<NSRange>((NSValue *)parameter))
        nsRange = [(NSValue*)parameter rangeValue];
    else
        return nil;

    if ([attribute isEqualToString:NSAccessibilityTextMarkerIsNullParameterizedAttribute])
        return [NSNumber numberWithBool:AXTextMarker(markerRef).isNull()];

    if ([attribute isEqualToString:NSAccessibilityTextMarkerRangeIsValidParameterizedAttribute]) {
        AXTextMarkerRange markerRange { markerRangeRef };
        return [NSNumber numberWithBool:markerRange.start().isValid() && markerRange.end().isValid()];
    }

    if ([attribute isEqualToString:_AXStartTextMarkerForTextMarkerRangeAttribute]) {
        AXTextMarkerRange markerRange { markerRangeRef };
        return markerRange.start().platformData().bridgingAutorelease();
    }

    if ([attribute isEqualToString:_AXEndTextMarkerForTextMarkerRangeAttribute]) {
        AXTextMarkerRange markerRange { markerRangeRef };
        return markerRange.end().platformData().bridgingAutorelease();
    }

    if ([attribute isEqualToString:_AXTextMarkerRangeForNSRangeAttribute])
        return backingObject->textMarkerRangeForNSRange(nsRange).platformData().bridgingAutorelease();

    if ([attribute isEqualToString:_AXPageBoundsForTextMarkerRangeAttribute]) {
        NSRect rect = CGRectZero;
        if (backingObject)
            rect = [self computeTextBoundsForRange:nsRange backingObject:*backingObject];

        return [NSValue valueWithRect:rect];
    }

    return nil;
}

- (NSValue *)intersectionWithSelectionRange
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    auto objectRange = backingObject->textMarkerRange();
    auto selectionRange = backingObject->selectedTextMarkerRange();

    auto intersection = selectionRange.intersectionWith(objectRange);
    if (intersection.has_value()) {
        auto intersectionCharacterRange = intersection->characterRange();
        if (intersectionCharacterRange.has_value())
            return [NSValue valueWithRange:intersectionCharacterRange.value()];
    }

    return nil;
}

- (NSString *)accessibilityPlatformMathSubscriptKey
{
    return NSAccessibilityMathSubscriptAttribute;
}

- (NSString *)accessibilityPlatformMathSuperscriptKey
{
    return NSAccessibilityMathSuperscriptAttribute;
}

- (id)accessibilityFocusedUIElement
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    RefPtr focusedObject = backingObject->focusedUIElementInAnyLocalFrame();
    return focusedObject ? focusedObject->wrapper() : nil;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    return [self _accessibilityHitTest:point returnPlatformElements:YES];
}

- (id)_accessibilityHitTest:(NSPoint)point returnPlatformElements:(BOOL)returnPlatformElements
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    if (RefPtr axObject = dynamicDowncast<AccessibilityObject>(backingObject))
        axObject->updateChildrenIfNecessary();
    RefPtr axObject = backingObject->accessibilityHitTest(IntPoint(point));

    RetainPtr<id> hit = nil;
    if (axObject) {
        if (axObject->isAttachment()) {
            RetainPtr wrapper = axObject->wrapper();
            if (RetainPtr<id> attachmentView = [wrapper.get() attachmentView])
                return attachmentView.autorelease();
        } else if (axObject->isRemoteFrame()) {
            if (returnPlatformElements)
                return axObject->remoteFramePlatformElement().autorelease();
        } else if (axObject->isWidget()) {
            // Only call out to the main-thread if this object has a backing widget to query.
            hit = Accessibility::retrieveAutoreleasedValueFromMainThread<id>([axObject, &point] () -> RetainPtr<id> {
                RefPtr widget = axObject->widget();
                if (is<PluginViewBase>(widget))
                    return widget->accessibilityHitTest(IntPoint(point));
                return nil;
            });
        }

        if (!hit)
            hit = axObject->wrapper();
    } else
        hit = self;

    return NSAccessibilityUnignoredAncestor(hit.get());
}

- (void)_accessibilityHitTestResolvingRemoteFrame:(NSPoint)point callback:(void(^)(NSString *))callback
{
    if (!AXObjectCache::clientIsInTestMode()) {
        callback(@"");
        return;
    }

    RetainPtr<id> hitTestResult = [self accessibilityHitTest:point];
    if (!hitTestResult) {
        callback(@"");
        return;
    }

    if ([hitTestResult isKindOfClass:[NSAccessibilityRemoteUIElement class]]) {
        RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
        if (!backingObject)
            return callback(@"no backing object");

        RefPtr axObject = backingObject->accessibilityHitTest(IntPoint(point));
        if (axObject && axObject->isRemoteFrame()) {
            RefPtr page = backingObject ? backingObject->page() : nullptr;
            RefPtr axRemoteFrame = dynamicDowncast<AXRemoteFrame>(axObject);
            if (page && axRemoteFrame) {
                auto clientCallback = [callback = makeBlockPtr(callback)] (String result) {
                    callback(nsStringNilIfEmpty(result).get());
                };

                page->chrome().client().resolveAccessibilityHitTestForTesting(*axRemoteFrame->frameID(), IntPoint(point), WTFMove(clientCallback));
            }
        }
    } else {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        callback([hitTestResult accessibilityAttributeValue:NSAccessibilityInfoStringForTestingAttribute]);
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NO;

    if ([attributeName isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute])
        return YES;

    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return backingObject->canSetFocusAttribute();

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return backingObject->canSetValueAttribute();

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return backingObject->canSetSelectedAttribute();

    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute])
        return backingObject->canSetSelectedChildren();

    if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute]
        || [attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        return backingObject->canSetExpandedAttribute();

    if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute])
        return YES;

    if ([attributeName isEqualToString:NSAccessibilitySelectedTextAttribute]
        || [attributeName isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
        return backingObject->canSetTextRangeAttributes();

    if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        return YES;

    if (backingObject->isWebArea()
        && ([attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute]
            || [attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute]))
        return YES;

    return NO;
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsIgnored
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return YES;

    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityIsIgnored];
    return backingObject->isIgnored();
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityParameterizedAttributeNames
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

#if !ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (backingObject->isAttachment())
        return nil;
#endif // !ENABLE_ACCESSIBILITY_LOCAL_FRAME

    static NeverDestroyed<RetainPtr<NSArray>> paramAttrs = @[
        NSAccessibilityUIElementForTextMarkerAttribute,
        NSAccessibilityTextMarkerRangeForUIElementAttribute,
        NSAccessibilityLineForTextMarkerAttribute,
        NSAccessibilityTextMarkerRangeForLineAttribute,
        NSAccessibilityStringForTextMarkerRangeAttribute,
        NSAccessibilityTextMarkerForPositionAttribute,
        NSAccessibilityBoundsForTextMarkerRangeAttribute,
        NSAccessibilityAttributedStringForTextMarkerRangeAttribute,
        NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsAttribute,
        NSAccessibilityTextMarkerRangeForTextMarkersAttribute,
        NSAccessibilityTextMarkerRangeForUnorderedTextMarkersAttribute,
        NSAccessibilityNextTextMarkerForTextMarkerAttribute,
        NSAccessibilityPreviousTextMarkerForTextMarkerAttribute,
        NSAccessibilityLeftWordTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilityRightWordTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilityLeftLineTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilityRightLineTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilitySentenceTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilityParagraphTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilityNextWordEndTextMarkerForTextMarkerAttribute,
        NSAccessibilityPreviousWordStartTextMarkerForTextMarkerAttribute,
        NSAccessibilityNextLineEndTextMarkerForTextMarkerAttribute,
        NSAccessibilityPreviousLineStartTextMarkerForTextMarkerAttribute,
        NSAccessibilityNextSentenceEndTextMarkerForTextMarkerAttribute,
        NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerAttribute,
        NSAccessibilityNextParagraphEndTextMarkerForTextMarkerAttribute,
        NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerAttribute,
        NSAccessibilityStyleTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilityLengthForTextMarkerRangeAttribute,
        NSAccessibilityBoundsForRangeParameterizedAttribute,
        NSAccessibilityStringForRangeParameterizedAttribute,
        NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
        NSAccessibilityEndTextMarkerForBoundsAttribute,
        NSAccessibilityStartTextMarkerForBoundsAttribute,
        NSAccessibilityLineTextMarkerRangeForTextMarkerAttribute,
        NSAccessibilitySelectTextWithCriteriaParameterizedAttribute,
        NSAccessibilitySearchTextWithCriteriaParameterizedAttribute,
        NSAccessibilityTextOperationParameterizedAttribute
    ];

    static NeverDestroyed textParamAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:paramAttrs.get().get()]);
        [tempArray addObject:(NSString*)kAXLineForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForLineParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForPositionParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXBoundsForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRTFForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXAttributedStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStyleRangeForIndexParameterizedAttribute];
        return tempArray;
    }();
    static NeverDestroyed tableParamAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:paramAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityCellForColumnAndRowParameterizedAttribute];
        return tempArray;
    }();
    static NeverDestroyed webAreaParamAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:paramAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityTextMarkerForIndexAttribute];
        [tempArray addObject:NSAccessibilityTextMarkerIsValidAttribute];
        [tempArray addObject:NSAccessibilityIndexForTextMarkerAttribute];
        return tempArray;
    }();
    static NeverDestroyed secureFieldParamAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] init]);
        [tempArray addObject:NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute];
        return tempArray;
    }();

    if (backingObject->isSecureField())
        return secureFieldParamAttrs.get().get();

    if (backingObject->isTextControl())
        return textParamAttrs.get().get();

    if (backingObject->isExposableTable())
        return tableParamAttrs.get().get();

    if (backingObject->isWebArea())
        return webAreaParamAttrs.get().get();

    // The object that serves up the remote frame also is the one that does the frame conversion.
    if (backingObject->hasRemoteFrameChild())
        return [paramAttrs.get().get() arrayByAddingObject:NSAccessibilityConvertRelativeFrameParameterizedAttribute];

    return paramAttrs.get().get();
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

- (NSString *)accessibilityIdentifier
{
    if (AXObjectCache::accessibilityDOMIdentifiersEnabled()) {
        if (RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore)
            return backingObject->identifierAttribute().createNSString().autorelease();
    }

    return nil;
}

- (void)accessibilityPerformPressAction
{
    // In case anything we do by performing the press action causes an alert or other modal
    // behaviors, we need to return now, so that VoiceOver doesn't hang indefinitely.
    RunLoop::mainSingleton().dispatch([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityPerformPressAction];
    });
}

- (void)_accessibilityPerformPressAction
{
    ASSERT(isMainThread());
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if (backingObject->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityPressAction];
    else
        backingObject->press();
}

- (void)accessibilityPerformIncrementAction
{
    RunLoop::mainSingleton().dispatch([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityPerformIncrementAction];
    });
}

- (void)_accessibilityPerformIncrementAction
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if (backingObject->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityIncrementAction];
    else
        backingObject->increment();
}

- (void)accessibilityPerformDecrementAction
{
    RunLoop::mainSingleton().dispatch([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityPerformDecrementAction];
    });
}

- (void)_accessibilityPerformDecrementAction
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if (backingObject->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityDecrementAction];
    else
        backingObject->decrement();
}

ALLOW_DEPRECATED_DECLARATIONS_END

- (void)accessibilityPerformShowMenuAction
{
    AXTRACE("WebAccessibilityObjectWrapper accessibilityPerformShowMenuAction"_s);

    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper "_s, hex(reinterpret_cast<uintptr_t>(self))));
        return;
    }

    if (backingObject->role() == AccessibilityRole::ComboBox) {
        backingObject->setIsExpanded(true);
        return;
    }

    Accessibility::performFunctionOnMainThread([protectedSelf = retainPtr(self)] {
        // This needs to be performed in an iteration of the run loop that did not start from an AX call.
        // If it's the same run loop iteration, the menu open notification won't be sent.
        [protectedSelf performSelector:@selector(_accessibilityShowContextMenu) withObject:nil afterDelay:0.0];
    });
}

- (void)_accessibilityShowContextMenu
{
    AXTRACE("WebAccessibilityObjectWrapper _accessibilityShowContextMenu"_s);
    ASSERT(isMainThread());

    RefPtr<AccessibilityObject> backingObject = dynamicDowncast<AccessibilityObject>(self.axBackingObject);
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper "_s, hex(reinterpret_cast<uintptr_t>(self))));
        return;
    }

    RefPtr page = backingObject->page();
    if (!page)
        return;

    IntRect rect = snappedIntRect(backingObject->elementRect());
    // On WK2, we need to account for the scroll position with regards to root view.
    // On WK1, we need to convert rect to window space to match mouse clicking.
    RefPtr frameView = backingObject->documentFrameView();
    if (frameView) {
        // Find the appropriate scroll view to convert the coordinates to window space.
        RefPtr axScrollView = Accessibility::findAncestor(*backingObject, false, [] (const auto& ancestor) {
            return ancestor.isScrollView() && ancestor.scrollView();
        });

        if (RefPtr scrollView = axScrollView ? axScrollView->scrollView() : nullptr) {
            if (!frameView->platformWidget())
                rect = scrollView->contentsToRootView(rect);
            else
                rect = scrollView->contentsToWindow(rect);
        }
    }

    if (RefPtr localMainFrame = page->localMainFrame())
        page->contextMenuController().showContextMenuAt(*localMainFrame, rect.center());
}

- (void)accessibilityScrollToVisible
{
    if (RefPtr<AXCoreObject> backingObject = self.axBackingObject)
        backingObject->scrollToMakeVisible();
}

- (void)_accessibilityScrollToMakeVisibleWithSubFocus:(NSRect)rect
{
    if (RefPtr<AXCoreObject> backingObject = self.axBackingObject)
        backingObject->scrollToMakeVisibleWithSubFocus(IntRect(rect));
}

- (void)_accessibilityScrollToGlobalPoint:(NSPoint)point
{
    if (RefPtr<AXCoreObject> backingObject = self.axBackingObject)
        backingObject->scrollToGlobalPoint(IntPoint(point));
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilityPerformAction:(NSString*)action
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if ([action isEqualToString:NSAccessibilityPressAction])
        [self accessibilityPerformPressAction];
    else if ([action isEqualToString:NSAccessibilitySyncPressAction]) {
        // Used in layout tests, so that we don't have to wait for the async press action.
        [self _accessibilityPerformPressAction];
    } else if ([action isEqualToString:NSAccessibilitySyncIncrementAction])
        [self _accessibilityPerformIncrementAction];
    else if ([action isEqualToString:NSAccessibilitySyncDecrementAction])
        [self _accessibilityPerformDecrementAction];
    else if ([action isEqualToString:NSAccessibilityShowMenuAction])
        [self accessibilityPerformShowMenuAction];
    else if ([action isEqualToString:NSAccessibilityIncrementAction])
        [self accessibilityPerformIncrementAction];
    else if ([action isEqualToString:NSAccessibilityDecrementAction])
        [self accessibilityPerformDecrementAction];
    else if ([action isEqualToString:NSAccessibilityScrollToVisibleAction])
        [self accessibilityScrollToVisible];
    else if ([action isEqualToString:NSAccessibilityDismissAction])
        backingObject->performDismissActionIgnoringResult();
    else if (AXObjectCache::clientIsInTestMode() && [action isEqualToString:@"AXLogTrees"])
        [self _accessibilityPrintTrees];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

// Internal method to print the accessibility trees to standard error.
- (void)_accessibilityPrintTrees
{
    Accessibility::performFunctionOnMainThread([protectedSelf = retainPtr(self)] {
        RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
        if (!backingObject)
            return;

        CheckedPtr cache = backingObject->axObjectCache();
        if (!cache)
            return;

        AXTreeData data = cache->treeData({ { AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::RendererOrNode } }); // Can specify AXStreamOptions here if needed (e.g., TextRuns)
        SAFE_FPRINTF(stderr, "==AX Trees==\n%s\n%s\n", data.liveTree.utf8(), data.isolatedTree.utf8());
    });
}

- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    return backingObject ? backingObject->replaceTextInRange(String(string), range) : NO;
}

- (BOOL)accessibilityInsertText:(NSString *)text
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    return backingObject ? backingObject->insertText(String(text)) : NO;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
{
#if PLATFORM(MAC)

    RetainPtr retainedValue = value;
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        if (AXObjectIsTextMarkerRange(value))
            retainedValue = AXTextMarkerRange { (AXTextMarkerRangeRef)value }.convertToDomOffsetRange().platformData().bridgingAutorelease();
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    // In case anything we do by changing values causes an alert or other modal
    // behaviors, we need to return now, so that VoiceOver doesn't hang indefinitely.
    callOnMainThread([retainedValue = WTFMove(retainedValue), attributeName = retainPtr(attributeName), protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilitySetValue:retainedValue.get() forAttribute:attributeName.get()];
    });
#else
    // dispatch_async on earlier versions can cause focus not to track.
    [self _accessibilitySetValue:value forAttribute:attributeName];
#endif // PLATFORM(MAC)
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)_accessibilitySetValue:(id)value forAttribute:(NSString *)attributeName
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper _accessibilitySetValue: forAttribute:"_s, String(attributeName)));

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper "_s, hex(reinterpret_cast<uintptr_t>(self))));
        return;
    }

    AXTextMarkerRangeRef textMarkerRange = nil;
    NSNumber* number = nil;
    NSString* string = nil;
    NSRange range = { 0, 0 };
    NSArray* array = nil;

    // decode the parameter
    if (AXObjectIsTextMarkerRange(value))
        textMarkerRange = (AXTextMarkerRangeRef)value;
    else if ([value isKindOfClass:[NSNumber class]])
        number = value;
    else if ([value isKindOfClass:[NSString class]])
        string = value;
    else if ([value isKindOfClass:[NSValue class]])
        range = [value rangeValue];
    else if ([value isKindOfClass:[NSArray class]])
        array = value;

    // handle the command
    if ([attributeName isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute]) {
        ASSERT(textMarkerRange);
        Accessibility::performFunctionOnMainThread([textMarkerRange = retainPtr(textMarkerRange), protectedSelf = retainPtr(self)] {
            if (RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject)
                backingObject->setSelectedVisiblePositionRange(AXTextMarkerRange { textMarkerRange.get() });
        });
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        backingObject->setFocused([number boolValue]);
    } else if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (number && backingObject->canSetNumericValue())
            backingObject->setValueIgnoringResult([number floatValue]);
        else if (string)
            backingObject->setValueIgnoringResult(string);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute]) {
        if (!number)
            return;
        backingObject->setSelected([number boolValue]);
    } else if ([attributeName isEqualToString:NSAccessibilitySelectedChildrenAttribute]) {
        if (!array || !backingObject->canSetSelectedChildren())
            return;

        AXCoreObject::AccessibilityChildrenVector selectedChildren;
        convertToVector(array, selectedChildren);
        backingObject->setSelectedChildren(selectedChildren);
    } else if (backingObject->isTextControl()) {
        if ([attributeName isEqualToString:NSAccessibilitySelectedTextAttribute])
            backingObject->setSelectedText(string);
        else if ([attributeName isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
            backingObject->setSelectedTextRange(range);
    } else if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute] || [attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        backingObject->setIsExpanded([number boolValue]);
    else if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector selectedRows;
        convertToVector(array, selectedRows);
        if (backingObject->isTree() || backingObject->isExposableTable())
            backingObject->setSelectedRows(WTFMove(selectedRows));
    } else if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        backingObject->setARIAGrabbed([number boolValue]);
    else if (backingObject->isWebArea() && [attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
        backingObject->setPreventKeyboardDOMEventDispatch([number boolValue]);
    else if (backingObject->isWebArea() && [attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
        backingObject->setCaretBrowsingEnabled([number boolValue]);
}

static RenderObject* rendererForView(NSView* view)
{
    if (![view conformsToProtocol:@protocol(WebCoreFrameView)])
        return nullptr;

    NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
    auto frame = [frameView _web_frame];
    if (!frame)
        return nullptr;

    RefPtr<Node> node = frame->protectedDocument()->ownerElement();
    if (!node)
        return nullptr;

    return node->renderer();
}

- (id)_accessibilityParentForSubview:(NSView*)subview
{
    CheckedPtr renderer = rendererForView(subview);
    if (!renderer)
        return nil;

    CheckedPtr cache = renderer->protectedDocument()->axObjectCache();
    RefPtr object = cache ? cache->getOrCreate(*renderer) : nil;
    RefPtr parent = object ? object->parentObjectUnignored() : nil;
    return parent ? parent->wrapper() : nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSString*)accessibilityActionDescription:(NSString*)action
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (NSInteger)_indexForTextMarker:(AXTextMarkerRef)markerRef
{
    if (!markerRef)
        return NSNotFound;

    return Accessibility::retrieveValueFromMainThread<NSInteger>([markerRef = retainPtr(markerRef)] () -> NSInteger {
        AXTextMarker marker { markerRef.get() };
        if (!marker.isValid())
            return NSNotFound;
        return makeNSRange(AXTextMarkerRange { marker, marker }.simpleRange()).location;
    });
}

- (AXTextMarkerRef)_textMarkerForIndex:(NSInteger)textIndex
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRef>([&textIndex, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRef> {
        RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
        if (!backingObject)
            return nil;

        CheckedPtr cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        RefPtr document = backingObject->document();
        if (!document)
            return nil;

        RefPtr documentElement = document->documentElement();
        if (!documentElement)
            return nil;

        auto boundary = resolveCharacterLocation(makeRangeSelectingNodeContents(*documentElement), textIndex);
        auto characterOffset = cache->startOrEndCharacterOffsetForRange(makeSimpleRange(boundary), true);

        return textMarkerForCharacterOffset(cache.get(), characterOffset);
    });
}

#if ENABLE(TREE_DEBUGGING)
- (void)showNodeForTextMarker:(AXTextMarkerRef)textMarker
{
    CheckedPtr cache = downcast<AccessibilityObject>(self.axBackingObject)->axObjectCache();
    auto visiblePosition = visiblePositionForTextMarker(cache.get(), textMarker);
    RefPtr node = visiblePosition.deepEquivalent().deprecatedNode();
    if (!node)
        return;
    node->showNode();
    node->showNodePathForThis();
}

- (void)showNodeTreeForTextMarker:(AXTextMarkerRef)textMarker
{
    CheckedPtr cache = downcast<AccessibilityObject>(self.axBackingObject)->axObjectCache();
    auto visiblePosition = visiblePositionForTextMarker(cache.get(), textMarker);
    RefPtr node = visiblePosition.deepEquivalent().deprecatedNode();
    if (!node)
        return;
    node->showTreeForThis();
}
#endif

enum class TextUnit {
    LeftWord = 1,
    RightWord,
    NextWordEnd,
    PreviousWordStart,
    Sentence,
    NextSentenceEnd,
    PreviousSentenceStart,
    Paragraph,
    NextParagraphEnd,
    PreviousParagraphStart,
    Line,
    LeftLine,
    RightLine,
    NextLineEnd,
    PreviousLineStart,
};

- (AXTextMarkerRangeRef)textMarkerRangeAtTextMarker:(AXTextMarkerRef)textMarker forUnit:(TextUnit)textUnit
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        AXTextMarker inputMarker { textMarker };
        switch (textUnit) {
        case TextUnit::LeftWord:
            return inputMarker.wordRange(WordRangeType::Left).platformData().autorelease();
        case TextUnit::RightWord:
            return inputMarker.wordRange(WordRangeType::Right).platformData().autorelease();
        case TextUnit::Sentence:
            return inputMarker.sentenceRange(SentenceRangeType::Current).platformData().autorelease();
        case TextUnit::Paragraph:
            return inputMarker.paragraphRange().platformData().autorelease();
        default:
            ASSERT_NOT_REACHED();
            return nil;
        }
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([textMarker = retainPtr(textMarker), &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRangeRef> {
        RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
        if (!backingObject)
            return nil;

        CheckedPtr cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        AXTextMarker marker { textMarker.get() };
        std::optional<SimpleRange> range;
        switch (textUnit) {
        case TextUnit::LeftWord:
            range = cache->leftWordRange(marker);
            break;
        case TextUnit::RightWord:
            range = cache->rightWordRange(marker);
            break;
        case TextUnit::Sentence:
            range = cache->sentenceForCharacterOffset(marker);
            break;
        case TextUnit::Paragraph:
            range = cache->paragraphForCharacterOffset(marker);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return textMarkerRangeFromRange(cache.get(), range);
    });
}

- (id)lineTextMarkerRangeForTextMarker:(AXTextMarkerRef)textMarker forUnit:(TextUnit)textUnit
{
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            auto rangeType = LineRangeType::Current;
            switch (textUnit) {
            case TextUnit::Line:
                break;
            case TextUnit::LeftLine:
                rangeType = LineRangeType::Left;
                break;
            case TextUnit::RightLine:
                rangeType = LineRangeType::Right;
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            return AXTextMarker { textMarker }.lineRange(rangeType).platformData().bridgingAutorelease();
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return (id)Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([textMarker = retainPtr(textMarker), &textUnit, protectedSelf = retainPtr(self)] () ->  RetainPtr<AXTextMarkerRangeRef> {
        RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        AXTextMarker marker { textMarker.get() };
        VisiblePositionRange visiblePositionRange;
        switch (textUnit) {
        case TextUnit::Line:
            visiblePositionRange = backingObject->lineRangeForPosition(marker);
            break;
        case TextUnit::LeftLine:
            visiblePositionRange = backingObject->leftLineVisiblePositionRange(marker);
            break;
        case TextUnit::RightLine:
            visiblePositionRange = backingObject->rightLineVisiblePositionRange(marker);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return AXTextMarkerRange(visiblePositionRange).platformData();
    });
}

- (AXTextMarkerRef)textMarkerForTextMarker:(AXTextMarkerRef)textMarkerRef atUnit:(TextUnit)textUnit
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        AXTextMarker inputMarker { textMarkerRef };
        switch (textUnit) {
        case TextUnit::NextSentenceEnd:
            return inputMarker.nextSentenceEnd().platformData().autorelease();
        case TextUnit::PreviousSentenceStart:
            return inputMarker.previousSentenceStart().platformData().autorelease();
        case TextUnit::NextParagraphEnd:
            return inputMarker.nextParagraphEnd().platformData().autorelease();
        case TextUnit::PreviousParagraphStart:
            return inputMarker.previousParagraphStart().platformData().autorelease();
        case TextUnit::NextWordEnd:
            return inputMarker.nextWordEnd().platformData().autorelease();
        case TextUnit::PreviousWordStart:
            return inputMarker.previousWordStart().platformData().autorelease();
        default:
            // TODO: Not implemented!
            break;
        }
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRef>([textMarkerRef = retainPtr(textMarkerRef), &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRef> {
        RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
        if (!backingObject)
            return nil;

        CheckedPtr cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        AXTextMarker oldMarker { textMarkerRef.get() };
        AXTextMarker newMarker;
        switch (textUnit) {
        case TextUnit::NextWordEnd:
            newMarker = cache->nextWordEndCharacterOffset(oldMarker);
            break;
        case TextUnit::PreviousWordStart:
            newMarker = cache->previousWordStartCharacterOffset(oldMarker);
            break;
        case TextUnit::NextSentenceEnd:
            newMarker = cache->nextSentenceEndCharacterOffset(oldMarker);
            break;
        case TextUnit::PreviousSentenceStart:
            newMarker = cache->previousSentenceStartCharacterOffset(oldMarker);
            break;
        case TextUnit::NextParagraphEnd:
            newMarker = cache->nextParagraphEndCharacterOffset(oldMarker);
            break;
        case TextUnit::PreviousParagraphStart:
            newMarker = cache->previousParagraphStartCharacterOffset(oldMarker);
            break;
        case TextUnit::NextLineEnd:
            return textMarkerForVisiblePosition(cache.get(), backingObject->nextLineEndPosition(oldMarker));
        case TextUnit::PreviousLineStart:
            return textMarkerForVisiblePosition(cache.get(), backingObject->previousLineStartPosition(oldMarker));
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return newMarker.platformData();
    });
}

static bool isMatchingPlugin(AXCoreObject& axObject, const AccessibilitySearchCriteria& criteria)
{
    if (!axObject.isPlugin())
        return false;

    return criteria.searchKeys.contains(AccessibilitySearchKey::AnyType)
        && (!criteria.visibleOnly || axObject.isVisible());
}

- (NSRect)computeTextBoundsForRange:(NSRange)range backingObject:(const AXCoreObject&)backingObject
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        std::optional markerRange = Accessibility::markerRangeFrom(range, backingObject);
        return markerRange ? static_cast<CGRect>(markerRange->viewportRelativeFrame()) : CGRectZero;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<NSRect>([range, protectedSelf = retainPtr(self)] () -> NSRect {
        RefPtr<AXCoreObject> backingObject = protectedSelf.get().updateObjectBackingStore;
        if (!backingObject)
            return CGRectZero;
        auto start = backingObject->visiblePositionForIndex(range.location);
        auto end = backingObject->visiblePositionForIndex(range.location + range.length);
        auto webRange = makeSimpleRange({ start, end });
        if (!webRange)
            return CGRectZero;

        return FloatRect(backingObject->boundsForRange(*webRange));
    });
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityAttributeValue:"_s, String(attribute)));
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    // Basic parameter validation.
    if (!attribute || !parameter)
        return nil;

    AXTextMarkerRef textMarker = nil;
    AXTextMarkerRangeRef textMarkerRange = nil;
    NSNumber *number = nil;
    NSArray *array = nil;
    NSDictionary *dictionary = nil;
    RefPtr<AXCoreObject> uiElement;
    NSPoint point = NSZeroPoint;
    bool pointSet = false;
    NSRange range = { 0, 0 };
    bool rangeSet = false;
    NSRect rect = NSZeroRect;

    // common parameter type check/casting. Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because it is indistinguishable from
    // a parameter of the wrong type.
    if (AXObjectIsTextMarker(parameter))
        textMarker = (AXTextMarkerRef)parameter;
    else if (AXObjectIsTextMarkerRange(parameter))
        textMarkerRange = (AXTextMarkerRangeRef)parameter;
    else if ([parameter isKindOfClass:[WebAccessibilityObjectWrapper class]]) {
        uiElement = [(WebAccessibilityObjectWrapper*)parameter axBackingObject];
        // The parameter wrapper object has lost its AX object since being given to the client, so bail early.
        if (!uiElement)
            return nil;
    } else if ([parameter isKindOfClass:[NSNumber class]])
        number = parameter;
    else if ([parameter isKindOfClass:[NSArray class]])
        array = parameter;
    else if ([parameter isKindOfClass:[NSDictionary class]])
        dictionary = parameter;
    else if ([parameter isKindOfClass:[NSValue class]] && nsValueHasObjCType<NSPoint>((NSValue*)parameter)) {
        pointSet = true;
        point = [(NSValue*)parameter pointValue];
    } else if ([parameter isKindOfClass:[NSValue class]] && nsValueHasObjCType<NSRange>((NSValue*)parameter)) {
        rangeSet = true;
        range = [(NSValue*)parameter rangeValue];
    } else if ([parameter isKindOfClass:[NSValue class]] && nsValueHasObjCType<NSRect>((NSValue*)parameter))
        rect = [(NSValue*)parameter rectValue];
    else {
        // Attribute type is not supported. Allow super to handle.
        return [super accessibilityAttributeValue:attribute forParameter:parameter];
    }

    // dispatch
    if ([attribute isEqualToString:NSAccessibilitySelectTextWithCriteriaParameterizedAttribute]) {
        // To be deprecated.
        auto result = Accessibility::retrieveValueFromMainThread<Vector<String>>([protectedDictionary = retainPtr(dictionary), protectedSelf = retainPtr(self)] () -> Vector<String> {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return Vector<String>();

            auto criteria = accessibilityTextCriteriaForParameterizedAttribute(protectedDictionary.get());
            criteria.second.textRanges = backingObject->findTextRanges(criteria.first);
            ASSERT(criteria.second.textRanges.size() <= 1);
            return backingObject->performTextOperation(criteria.second);
        });
        ASSERT(result.size() <= 1);
        if (result.size() > 0)
            return result[0].createNSString().autorelease();
        return @"";
    }

    if ([attribute isEqualToString:NSAccessibilitySearchTextWithCriteriaParameterizedAttribute]) {
        auto criteria = accessibilitySearchTextCriteriaForParameterizedAttribute(dictionary);
        return Accessibility::retrieveAutoreleasedValueFromMainThread<NSArray *>([&criteria, protectedSelf = retainPtr(self)] () -> RetainPtr<NSArray> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            if (!backingObject)
                return nil;
            auto ranges = backingObject->findTextRanges(criteria);
            if (ranges.isEmpty())
                return nil;
            CheckedPtr cache = backingObject->axObjectCache();
            return createNSArray(WTFMove(ranges), [&] (SimpleRange&& range) {
                return (id)textMarkerRangeFromRange(cache.get(), WTFMove(range));
            }).autorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityTextOperationParameterizedAttribute]) {
        auto operationResult = Accessibility::retrieveValueFromMainThread<Vector<String>>([protectedDictionary = retainPtr(dictionary), protectedSelf = retainPtr(self)] () -> Vector<String> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            if (!backingObject)
                return Vector<String>();

            CheckedPtr cache = backingObject->axObjectCache();
            auto textOperation = accessibilityTextOperationForParameterizedAttribute(cache.get(), protectedDictionary.get());
            return backingObject->performTextOperation(textOperation);
        });
        if (operationResult.isEmpty())
            return nil;
        return createNSArray(operationResult).autorelease();
    }

    if ([attribute isEqualToString:NSAccessibilityRangesForSearchPredicateParameterizedAttribute]) {
        auto criteria = accessibilitySearchCriteriaForSearchPredicate(*backingObject, dictionary);
        if (criteria.searchKeys.size() == 1 && criteria.searchKeys[0] == AccessibilitySearchKey::MisspelledWord) {
            // Request for the next/previous misspelling.
            auto textMarkerRange = AXSearchManager().findMatchingRange(WTFMove(criteria));
            if (!textMarkerRange)
                return nil;

            RefPtr object = textMarkerRange->start().object();
            if (!object)
                return nil;

            RetainPtr result = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
                object->protectedWrapper().get(), NSAccessibilitySearchResultElementKey,
                textMarkerRange->platformData().bridgingAutorelease(), NSAccessibilitySearchResultRangeKey,
                nil]);
            return [[[NSArray alloc] initWithObjects:result.get(), nil] autorelease];
        }
    }

    if ([attribute isEqualToString:NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute]) {
        auto criteria = accessibilitySearchCriteriaForSearchPredicate(*backingObject, dictionary);
        RetainPtr<NSArray> widgetChildren;
        if (isMatchingPlugin(*backingObject, criteria)) {
            // FIXME: We should also be searching the tree(s) resulting from `renderWidgetChildren` for matches.
            // This is tracked by https://bugs.webkit.org/show_bug.cgi?id=230167.
            if (RetainPtr children = renderWidgetChildren(*backingObject)) {
                NSUInteger includedChildrenCount = std::min([children count], NSUInteger(criteria.resultsLimit));
                widgetChildren = [children subarrayWithRange:NSMakeRange(0, includedChildrenCount)];
                if ([widgetChildren count] >= criteria.resultsLimit)
                    return widgetChildren.autorelease();
                criteria.resultsLimit -= [widgetChildren count];
            }
        } else if (backingObject->isRemoteFrame()
            && criteria.searchKeys.contains(AccessibilitySearchKey::AnyType)
            && (!criteria.visibleOnly || backingObject->isVisible())) {
            RetainPtr remoteFrameChildren = children(*backingObject);
            ASSERT([remoteFrameChildren count] == 1);
            if ([remoteFrameChildren count] == 1) {
                NSUInteger includedChildrenCount = std::min([remoteFrameChildren count], NSUInteger(criteria.resultsLimit));
                widgetChildren = [remoteFrameChildren subarrayWithRange:NSMakeRange(0, includedChildrenCount)];
                if ([widgetChildren count] >= criteria.resultsLimit)
                    return remoteFrameChildren.autorelease();
                criteria.resultsLimit -= [widgetChildren count];
            }
        }

        auto results = backingObject->findMatchingObjects(WTFMove(criteria));
        if (widgetChildren)
            return [widgetChildren arrayByAddingObjectsFromArray:makeNSArray(results)];
        return makeNSArray(results);
    }

    // TextMarker attributes.

    if ([attribute isEqualToString:NSAccessibilityEndTextMarkerForBoundsAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&rect, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            CheckedPtr cache = backingObject ? backingObject->axObjectCache() : nullptr;
            if (!cache)
                return nil;

            IntRect webCoreRect = screenToContents(*backingObject, enclosingIntRect(rect));
            CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, false);

            return (id)textMarkerForCharacterOffset(cache.get(), characterOffset, TextMarkerOrigin::EndTextMarkerForBounds);
        });
    }

    if ([attribute isEqualToString:NSAccessibilityStartTextMarkerForBoundsAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&rect, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            CheckedPtr cache = backingObject ? backingObject->axObjectCache() : nullptr;
            if (!cache)
                return nil;

            IntRect webCoreRect = screenToContents(*backingObject, enclosingIntRect(rect));
            CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, true);

            return (id)textMarkerForCharacterOffset(cache.get(), characterOffset, TextMarkerOrigin::StartTextMarkerForBounds);
        });
    }

    // TextMarkerRange attributes.
    if ([attribute isEqualToString:NSAccessibilityLineTextMarkerRangeForTextMarkerAttribute])
        return [self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::Line];

    if ([attribute isEqualToString:NSAccessibilityMisspellingTextMarkerRangeAttribute]) {
        return (id)Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([protectedDictionary = retainPtr(dictionary), protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRangeRef> {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto criteria = misspellingSearchCriteriaForParameterizedAttribute(protectedDictionary.get());
            if (!criteria.first)
                return nil;

            auto characterRange = criteria.first.characterRange();
            if (!characterRange)
                return nil;

            RefPtr startObject = criteria.second == AccessibilitySearchDirection::Next ? criteria.first.end().object() : criteria.first.start().object();
            auto misspellingRange = AXSearchManager().findMatchingRange(AccessibilitySearchCriteria {
                backingObject.get(), startObject.get(), *characterRange,
                criteria.second,
                { AccessibilitySearchKey::MisspelledWord }, { }, 1
            });
            if (!misspellingRange)
                return nil;
            return misspellingRange->platformData();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerIsValidAttribute])
        return [NSNumber numberWithBool:AXTextMarker(textMarker).isValid()];

    if ([attribute isEqualToString:NSAccessibilityIndexForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis())
            return [NSNumber numberWithUnsignedInt:AXTextMarker { textMarker }.offsetFromRoot()];
#endif
        return [NSNumber numberWithInteger:[self _indexForTextMarker:textMarker]];
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerForIndexAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            long index = [number longValue];
            if (index < 0)
                return nil;

            RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(backingObject->treeID()));
            if (RefPtr root = tree ? tree->rootNode() : nullptr) {
                AXTextMarker rootMarker { root->treeID(), root->objectID(), 0 };
                return rootMarker.nextMarkerFromOffset(static_cast<unsigned>(index)).platformData().bridgingAutorelease();
            }
            return nil;
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        return (id)[self _textMarkerForIndex:[number integerValue]];
    }

    if ([attribute isEqualToString:NSAccessibilityUIElementForTextMarkerAttribute]) {
        AXTextMarker marker { textMarker };
        RefPtr object = marker.object();
        if (!object)
            return nil;

        RetainPtr wrapper = object->wrapper();
        if (!wrapper)
            return nil;

        if (object->isAttachment()) {
            if (RetainPtr<id> attachmentView = [wrapper attachmentView])
                return attachmentView.autorelease();
        }
        return wrapper.autorelease();
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerRangeForUIElementAttribute]) {
        if (uiElement) {
            if (auto markerRange = uiElement->textMarkerRange())
                return markerRange.platformData().bridgingAutorelease();
        }
        return nil;
    }

    if ([attribute isEqualToString:NSAccessibilityLineForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis())
            return @(AXTextMarker { textMarker }.lineIndex());
#endif

        int result = Accessibility::retrieveValueFromMainThread<int>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> int {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return -1;
            return backingObject->lineForPosition(AXTextMarker { textMarker.get() });
        });
        return @(result);
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerRangeForLineAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            // Unfortunately, the main-thread version of this function expects a 1-indexed line, so callers pass it that way.
            // The text marker code expects normal 0-based indices, so we need to subtract one from the given value.
            unsigned lineIndex = [number unsignedIntValue];
            if (!lineIndex) {
                // Match the main-thread implementation's behavior.
                return nil;
            }
            if (RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(backingObject->treeID())))
                return tree->firstMarker().markerRangeForLineIndex(lineIndex - 1).platformData().bridgingAutorelease();
            return nil;
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedNumber = retainPtr(number), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            if (!backingObject)
                return nil;

            VisiblePositionRange vpRange;
            if ([protectedNumber.get() unsignedIntegerValue] != NSNotFound)
                vpRange = backingObject->visiblePositionRangeForLine([protectedNumber.get() unsignedIntValue]);

            CheckedPtr cache = backingObject->axObjectCache();
            return (id)textMarkerRangeFromVisiblePositions(cache.get(), vpRange.start, vpRange.end);
        });
    }

    if ([attribute isEqualToString:NSAccessibilityStringForTextMarkerRangeAttribute])
        return AXTextMarkerRange { textMarkerRange }.toString().createNSString().autorelease();

    if ([attribute isEqualToString:NSAccessibilityTextMarkerForPositionAttribute]) {
        if (!pointSet)
            return nil;
        IntPoint webCorePoint = IntPoint(point);

        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&webCorePoint, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            return AXTextMarker(backingObject->visiblePositionForPoint(webCorePoint), TextMarkerOrigin::Position).platformData().bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityBoundsForTextMarkerRangeAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            AXTextMarkerRange markerRange { textMarkerRange };
            if (!markerRange)
                return [NSValue valueWithRect:CGRectZero];

            return [NSValue valueWithRect:[self convertRectToSpace:markerRange.viewportRelativeFrame() space:AccessibilityConversionSpace::Screen]];
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

        NSRect rect = Accessibility::retrieveValueFromMainThread<NSRect>([textMarkerRange = retainPtr(textMarkerRange), protectedSelf = retainPtr(self)] () -> NSRect {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return CGRectZero;

            AXTextMarkerRange markerRange { textMarkerRange.get() };
            auto range = markerRange.simpleRange();
            if (!range)
                return CGRectZero;

            auto bounds = FloatRect(backingObject->boundsForRange(*range));
            return [protectedSelf convertRectToSpace:bounds space:AccessibilityConversionSpace::Screen];
        });
        return [NSValue valueWithRect:rect];
    }

    if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
        NSRect rect = CGRectZero;
        if (backingObject)
            rect = [self computeTextBoundsForRange:range backingObject:*backingObject];

        return [NSValue valueWithRect:[self convertRectToSpace:rect space:AccessibilityConversionSpace::Screen]];
    }

    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute]) {
        if (backingObject->isTextControl())
            return backingObject->doAXStringForRange(range).createNSString().autorelease();

#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            std::optional markerRange = Accessibility::markerRangeFrom(range, *backingObject);
            return markerRange ? markerRange->toString().createNSString().autorelease() : @"";
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

        return Accessibility::retrieveValueFromMainThread<RetainPtr<NSString>>([&range, protectedSelf = retainPtr(self)] () -> RetainPtr<NSString> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            if (!backingObject)
                return @"";
            CheckedPtr cache = backingObject->axObjectCache();
            if (!cache)
                return @"";

            auto start = cache->characterOffsetForIndex(range.location, backingObject.get());
            auto end = cache->characterOffsetForIndex(range.location + range.length, backingObject.get());
            auto range = cache->rangeForUnorderedCharacterOffsets(start, end);
            return AXTextMarkerRange { range }.toString().createNSString().autorelease();
        }).autorelease();
    }

    if ([attribute isEqualToString:NSAccessibilityAttributedStringForTextMarkerRangeAttribute])
        return attributedStringForTextMarkerRange(*backingObject, textMarkerRange, AXCoreObject::SpellCheck::Yes);

    if ([attribute isEqualToString:NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsAttribute]) {
        if (textMarkerRange)
            return attributedStringForTextMarkerRange(*backingObject, textMarkerRange, AXCoreObject::SpellCheck::No);

        if (dictionary) {
            RetainPtr<AXTextMarkerRangeRef> textMarkerRange = nil;
            RetainPtr<id> parameter = [dictionary objectForKey:NSAccessibilityTextMarkerRangeParam];
            if (AXObjectIsTextMarkerRange(parameter.get()))
                textMarkerRange = (AXTextMarkerRangeRef)parameter.get();

            auto spellCheck = AXCoreObject::SpellCheck::No;
            parameter = [dictionary objectForKey:NSAccessibilitySpellCheckParam];
            if ([parameter isKindOfClass:[NSNumber class]] && [parameter boolValue])
                spellCheck = AXCoreObject::SpellCheck::Yes;
            return attributedStringForTextMarkerRange(*backingObject, textMarkerRange.get(), spellCheck);
        }

        return nil;
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerRangeForTextMarkersAttribute]
        || [attribute isEqualToString:NSAccessibilityTextMarkerRangeForUnorderedTextMarkersAttribute]) {
        if (array.count < 2
            || !AXObjectIsTextMarker([array objectAtIndex:0])
            || !AXObjectIsTextMarker([array objectAtIndex:1]))
            return nil;

        return AXTextMarkerRange { { (AXTextMarkerRef)[array objectAtIndex:0] }, { (AXTextMarkerRef)[array objectAtIndex:1] } }.platformData().bridgingAutorelease();
    }

    if ([attribute isEqualToString:NSAccessibilityNextTextMarkerForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            AXTextMarker inputMarker { textMarker };
            return inputMarker.findMarker(AXDirection::Next).platformData().bridgingAutorelease();
        }
#endif
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            CheckedPtr cache = backingObject ? backingObject->axObjectCache() : nullptr;

            return nextTextMarker(cache.get(), AXTextMarker { textMarker.get() }).bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityPreviousTextMarkerForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            AXTextMarker inputMarker { textMarker };
            return inputMarker.findMarker(AXDirection::Previous).platformData().bridgingAutorelease();
        }
#endif
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr backingObject = downcast<AccessibilityObject>(protectedSelf.get().axBackingObject);
            CheckedPtr cache = backingObject ? backingObject->axObjectCache() : nullptr;

            return previousTextMarker(cache.get(), AXTextMarker { textMarker.get() }).bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityLeftWordTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::LeftWord];

    if ([attribute isEqualToString:NSAccessibilityRightWordTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::RightWord];

    if ([attribute isEqualToString:NSAccessibilityLeftLineTextMarkerRangeForTextMarkerAttribute])
        return [self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::LeftLine];

    if ([attribute isEqualToString:NSAccessibilityRightLineTextMarkerRangeForTextMarkerAttribute])
        return [self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::RightLine];

    if ([attribute isEqualToString:NSAccessibilitySentenceTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::Sentence];

    if ([attribute isEqualToString:NSAccessibilityParagraphTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::Paragraph];

    if ([attribute isEqualToString:NSAccessibilityNextWordEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextWordEnd];

    if ([attribute isEqualToString:NSAccessibilityPreviousWordStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousWordStart];

    if ([attribute isEqualToString:NSAccessibilityNextLineEndTextMarkerForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            AXTextMarker inputMarker { textMarker };
            return inputMarker.nextLineEnd().platformData().bridgingAutorelease();
        }
#endif
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextLineEnd];
    }

    if ([attribute isEqualToString:NSAccessibilityPreviousLineStartTextMarkerForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            AXTextMarker inputMarker { textMarker };
            return inputMarker.previousLineStart().platformData().bridgingAutorelease();
        }
#endif
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousLineStart];
    }

    if ([attribute isEqualToString:NSAccessibilityNextSentenceEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextSentenceEnd];

    if ([attribute isEqualToString:NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousSentenceStart];

    if ([attribute isEqualToString:NSAccessibilityNextParagraphEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextParagraphEnd];

    if ([attribute isEqualToString:NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousParagraphStart];

    if ([attribute isEqualToString:NSAccessibilityStyleTextMarkerRangeForTextMarkerAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis())
            return AXTextMarker { textMarker }.rangeWithSameStyle().platformData().bridgingAutorelease();
#endif // ENABLE(AX_THREAD_TEXT_APIS)

        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;
            return AXTextMarkerRange { backingObject->styleRangeForPosition(AXTextMarker { textMarker.get() }) }.platformData().bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityLengthForTextMarkerRangeAttribute]) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        if (AXObjectCache::useAXThreadTextApis()) {
            AXTextMarkerRange range = { textMarkerRange };
            return @(range.toString().length());
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        unsigned length = Accessibility::retrieveValueFromMainThread<unsigned>([textMarkerRange = retainPtr(textMarkerRange), protectedSelf = retainPtr(self)] () -> unsigned {
            RefPtr<AXCoreObject> backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return 0;

            AXTextMarkerRange markerRange { textMarkerRange.get() };
            auto range = markerRange.simpleRange();
            return range ? AXObjectCache::lengthForRange(*range) : 0;
        });
        return @(length);
    }

    if (backingObject->isExposableTable()) {
        if ([attribute isEqualToString:NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
            if (array == nil || [array count] != 2)
                return nil;
            RefPtr cell = backingObject->cellForColumnAndRow([[array objectAtIndex:0] unsignedIntValue], [[array objectAtIndex:1] unsignedIntValue]);
            return cell ? cell->wrapper() : nil;
        }
    }

    if (backingObject->isTextControl()) {
        if ([attribute isEqualToString: bridge_cast(kAXLineForIndexParameterizedAttribute)]) {
            int lineNumber = backingObject->doAXLineForIndex([number intValue]);
            if (lineNumber < 0)
                return nil;
            return @(lineNumber);
        }

        if ([attribute isEqualToString:bridge_cast(kAXRangeForLineParameterizedAttribute)]) {
            auto textRange = backingObject->doAXRangeForLine([number intValue]);
            return [NSValue valueWithRange:textRange];
        }

        if ([attribute isEqualToString:bridge_cast(kAXStringForRangeParameterizedAttribute)])
            return rangeSet ? (id)backingObject->doAXStringForRange(range).createNSString().autorelease() : nil;

        if ([attribute isEqualToString:bridge_cast(kAXRangeForPositionParameterizedAttribute)]) {
            if (!pointSet)
                return nil;

            auto webCorePoint = IntPoint(point);
            auto textRange = backingObject->characterRangeForPoint(webCorePoint);
            return [NSValue valueWithRange:textRange];
        }

        if ([attribute isEqualToString:bridge_cast(kAXRangeForIndexParameterizedAttribute)]) {
            auto textRange = backingObject->doAXRangeForIndex([number intValue]);
            return [NSValue valueWithRange:textRange];
        }

        if ([attribute isEqualToString:bridge_cast(kAXBoundsForRangeParameterizedAttribute)]) {
            if (!rangeSet)
                return nil;

            auto bounds = FloatRect(backingObject->doAXBoundsForRangeUsingCharacterOffset(range));
            NSRect rect = [self convertRectToSpace:bounds space:AccessibilityConversionSpace::Screen];
            return [NSValue valueWithRect:rect];
        }

        if ([attribute isEqualToString:bridge_cast(kAXRTFForRangeParameterizedAttribute)])
            return rangeSet ? rtfForNSRange(*backingObject, range) : nil;

        if ([attribute isEqualToString:bridge_cast(kAXAttributedStringForRangeParameterizedAttribute)])
            return rangeSet ? attributedStringForNSRange(*backingObject, range) : nil;

        if ([attribute isEqualToString:bridge_cast(kAXStyleRangeForIndexParameterizedAttribute)]) {
            auto textRange = backingObject->doAXStyleRangeForIndex([number intValue]);
            return [NSValue valueWithRange:textRange];
        }
    }

    if ([attribute isEqualToString:NSAccessibilityConvertRelativeFrameParameterizedAttribute]) {
        RefPtr parent = backingObject->parentObject();
        return parent ? [NSValue valueWithRect:parent->convertFrameToSpace(FloatRect(rect), AccessibilityConversionSpace::Page)] : nil;
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerDebugDescriptionAttribute])
        return AXTextMarker { textMarker }.debugDescription().createNSString().autorelease();

    if ([attribute isEqualToString:NSAccessibilityTextMarkerRangeDebugDescriptionAttribute])
        return AXTextMarkerRange { textMarkerRange }.debugDescription().createNSString().autorelease();

#if ENABLE(TREE_DEBUGGING)
    if ([attribute isEqualToString:AXTextMarkerNodeDebugDescriptionAttribute]) {
        [self showNodeForTextMarker:textMarker];
        return nil;
    }

    if ([attribute isEqualToString:AXTextMarkerNodeTreeDebugDescriptionAttribute]) {
        [self showNodeTreeForTextMarker:textMarker];
        return nil;
    }
#endif // ENABLE(TREE_DEBUGGING)

    if (AXObjectCache::clientIsInTestMode()) {
        if (RetainPtr<id> value = [self parameterizedAttributeValueForTesting:attribute parameter:parameter backingObject:backingObject])
            return value.autorelease();
    }

    // There are some parameters that super handles that are not explicitly returned by the list of the element's attributes.
    // In that case it must be passed to super.
    return [super accessibilityAttributeValue:attribute forParameter:parameter];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (BOOL)accessibilitySupportsOverriddenAttributes
{
    return YES;
}

// accessibilityShouldUseUniqueId is an AppKit method we override so that
// objects will be given a unique ID, and therefore allow AppKit to know when they
// become obsolete (e.g. when the user navigates to a new web page, making this one
// unrendered but not deallocated because it is in the back/forward cache).
// It is important to call NSAccessibilityUnregisterUniqueIdForUIElement in the
// appropriate place (e.g. dealloc) to remove these non-retained references from
// AppKit's id mapping tables. We do this in detach by calling unregisterUniqueIdForUIElement.
//
// Registering an object is also required for observing notifications. Only registered objects can be observed.
- (BOOL)accessibilityShouldUseUniqueId
{
    // All AX object wrappers should use unique ID's because it's faster within AppKit to look them up.
    return YES;
}

// API that AppKit uses for faster access
- (NSUInteger)accessibilityIndexOfChild:(id)targetChild
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NSNotFound;

    // Tree objects return their rows as their children. We can use the original method
    // here, because we won't gain any speed up.
    if (backingObject->isTree())
        return [super accessibilityIndexOfChild:targetChild];

    const auto& children = backingObject->stitchedUnignoredChildren();
    if (!children.size()) {
        if (RetainPtr widgetChildren = renderWidgetChildren(*backingObject))
            return [widgetChildren.get() indexOfObject:targetChild];
#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
        if (backingObject->isModel())
            return backingObject->modelElementChildren().children.find(targetChild);
#endif
    }

    size_t childCount = children.size();
    for (size_t i = 0; i < childCount; i++) {
        const auto& child = children[i];
        RetainPtr childWrapper = child->wrapper();
        if (childWrapper == targetChild || (child->isAttachment() && [childWrapper attachmentView] == targetChild)
            || (child->isRemoteFrame() && child->remoteFramePlatformElement() == targetChild)) {
            return i;
        }
    }
    return NSNotFound;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityArrayAttributeCount:"_s, String(attribute)));

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return 0;

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        // Tree items object returns a different set of children than those that are in children()
        // because an AXOutline (the mac role is becomes) has some odd stipulations.
        if (backingObject->isTree() || backingObject->isTreeItem() || backingObject->isRemoteFrame())
            return children(*backingObject).count;

        // FIXME: this is duplicating the logic in children(AXCoreObject&) so it should be reworked.
        size_t childrenSize = backingObject->stitchedUnignoredChildren().size();
        if (!childrenSize) {
#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
            if (backingObject->isModel())
                return backingObject->modelElementChildren().children.size();
#endif
            if (RetainPtr widgetChildren = renderWidgetChildren(*backingObject))
                return [widgetChildren.get() count];
        }
        return childrenSize;
    }

    return [super accessibilityArrayAttributeCount:attribute];
}
ALLOW_DEPRECATED_DECLARATIONS_END

// Implement this for performance reasons, as the default AppKit implementation will iterate upwards
// until it finds something that responds to this method.
- (pid_t)accessibilityPresenterProcessIdentifier
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    return backingObject ? backingObject->processID() : 0;
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityArrayAttributeValue:"_s, String(attribute)));

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return [self _accessibilityChildrenFromIndex:index maxCount:maxCount returnPlatformElements:YES];

    return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
}

- (NSArray *)_accessibilityChildrenFromIndex:(NSUInteger)index maxCount:(NSUInteger)maxCount returnPlatformElements:(BOOL)returnPlatformElements
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    const auto& unignoredChildren = backingObject->stitchedUnignoredChildren();
    if (unignoredChildren.isEmpty()) {
        RetainPtr<NSArray> children = transformSpecialChildrenCases(*backingObject, unignoredChildren);
        if (!children)
            return nil;

        NSUInteger childCount = [children count];
        if (index >= childCount)
            return nil;

        NSUInteger arrayLength = std::min(childCount - index, maxCount);
        return [children subarrayWithRange:NSMakeRange(index, arrayLength)];
    }

    if (backingObject->isTree() || backingObject->isTreeItem()) {
        // Tree objects return their rows as their children & tree items return their contents sans rows.
        // We can use the original method in this case.
        return [super accessibilityArrayAttributeValues:NSAccessibilityChildrenAttribute index:index maxCount:maxCount];
    }

    RetainPtr<NSArray> children = makeNSArray(unignoredChildren, returnPlatformElements);
    unsigned childCount = [children count];
    if (index >= childCount)
        return nil;

    unsigned available = std::min(childCount - index, maxCount);

    NSMutableArray *subarray = [NSMutableArray arrayWithCapacity:available];
    for (unsigned added = 0; added < available; ++index, ++added) {
        RetainPtr<WebAccessibilityObjectWrapper> wrapper = [children objectAtIndex:index];

        // The attachment view should be returned, otherwise AX palindrome errors occur.
        RetainPtr<id> attachmentView;
        if (RefPtr childObject = [wrapper isKindOfClass:[WebAccessibilityObjectWrapper class]] ? wrapper.get().axBackingObject : nullptr) {
            if (childObject->isAttachment())
                attachmentView = [wrapper attachmentView];
            else if (childObject->isRemoteFrame() && returnPlatformElements)
                attachmentView = childObject->remoteFramePlatformElement();
        }

        [subarray addObject:attachmentView ? attachmentView.get() : wrapper.get()];
    }

    return subarray;
}
@end

#endif // PLATFORM(MAC)
