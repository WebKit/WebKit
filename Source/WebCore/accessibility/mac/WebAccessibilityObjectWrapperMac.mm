/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#if ENABLE(ACCESSIBILITY) && PLATFORM(MAC)

#import "AXLogger.h"
#import "AXObjectCache.h"
#import "AXTextMarker.h"
#import "AccessibilityARIAGridRow.h"
#import "AccessibilityLabel.h"
#import "AccessibilityList.h"
#import "AccessibilityListBox.h"
#import "AccessibilityProgressIndicator.h"
#import "AccessibilityRenderObject.h"
#import "AccessibilityScrollView.h"
#import "AccessibilitySpinButton.h"
#import "AccessibilityTable.h"
#import "AccessibilityTableCell.h"
#import "AccessibilityTableColumn.h"
#import "AccessibilityTableRow.h"
#import "Chrome.h"
#import "ChromeClient.h"
#import "ContextMenuController.h"
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
#import "LocalFrame.h"
#import "LocalFrameLoaderClient.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "PluginDocument.h"
#import "PluginViewBase.h"
#import "Range.h"
#import "RenderInline.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "ScrollView.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import "WebCoreFrameView.h"
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

using namespace WebCore;

// Cell Tables
#ifndef NSAccessibilitySelectedCellsAttribute
#define NSAccessibilitySelectedCellsAttribute @"AXSelectedCells"
#endif

#ifndef NSAccessibilityVisibleCellsAttribute
#define NSAccessibilityVisibleCellsAttribute @"AXVisibleCells"
#endif

#ifndef NSAccessibilityRowIndexRangeAttribute
#define NSAccessibilityRowIndexRangeAttribute @"AXRowIndexRange"
#endif

#ifndef NSAccessibilityColumnIndexRangeAttribute
#define NSAccessibilityColumnIndexRangeAttribute @"AXColumnIndexRange"
#endif

#ifndef NSAccessibilityCellForColumnAndRowParameterizedAttribute
#define NSAccessibilityCellForColumnAndRowParameterizedAttribute @"AXCellForColumnAndRow"
#endif

#ifndef NSAccessibilityCellRole
#define NSAccessibilityCellRole @"AXCell"
#endif

#ifndef NSAccessibilityDefinitionListSubrole
#define NSAccessibilityDefinitionListSubrole @"AXDefinitionList"
#endif

// Miscellaneous
#ifndef NSAccessibilityAccessKeyAttribute
#define NSAccessibilityAccessKeyAttribute @"AXAccessKey"
#endif

#ifndef NSAccessibilityValueAutofillAvailableAttribute
#define NSAccessibilityValueAutofillAvailableAttribute @"AXValueAutofillAvailable"
#endif

#ifndef NSAccessibilityValueAutofillTypeAttribute
#define NSAccessibilityValueAutofillTypeAttribute @"AXValueAutofillType"
#endif

#ifndef NSAccessibilityLanguageAttribute
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#endif

#ifndef NSAccessibilityRequiredAttribute
#define NSAccessibilityRequiredAttribute @"AXRequired"
#endif

#ifndef NSAccessibilityInvalidAttribute
#define NSAccessibilityInvalidAttribute @"AXInvalid"
#endif

#ifndef NSAccessibilityOwnsAttribute
#define NSAccessibilityOwnsAttribute @"AXOwns"
#endif

#ifndef NSAccessibilityGrabbedAttribute
#define NSAccessibilityGrabbedAttribute @"AXGrabbed"
#endif

#ifndef NSAccessibilityDatetimeValueAttribute
#define NSAccessibilityDatetimeValueAttribute @"AXDateTimeValue"
#endif

#ifndef NSAccessibilityInlineTextAttribute
#define NSAccessibilityInlineTextAttribute @"AXInlineText"
#endif

#ifndef NSAccessibilityDropEffectsAttribute
#define NSAccessibilityDropEffectsAttribute @"AXDropEffects"
#endif

#ifndef NSAccessibilityARIALiveAttribute
#define NSAccessibilityARIALiveAttribute @"AXARIALive"
#endif

#ifndef NSAccessibilityARIAAtomicAttribute
#define NSAccessibilityARIAAtomicAttribute @"AXARIAAtomic"
#endif

#ifndef NSAccessibilityARIARelevantAttribute
#define NSAccessibilityARIARelevantAttribute @"AXARIARelevant"
#endif

#ifndef NSAccessibilityElementBusyAttribute
#define NSAccessibilityElementBusyAttribute @"AXElementBusy"
#endif

#ifndef NSAccessibilityARIAPosInSetAttribute
#define NSAccessibilityARIAPosInSetAttribute @"AXARIAPosInSet"
#endif

#ifndef NSAccessibilityARIASetSizeAttribute
#define NSAccessibilityARIASetSizeAttribute @"AXARIASetSize"
#endif

#ifndef NSAccessibilityLoadingProgressAttribute
#define NSAccessibilityLoadingProgressAttribute @"AXLoadingProgress"
#endif

#ifndef NSAccessibilityHasPopupAttribute
#define NSAccessibilityHasPopupAttribute @"AXHasPopup"
#endif

#ifndef NSAccessibilityPopupValueAttribute
#define NSAccessibilityPopupValueAttribute @"AXPopupValue"
#endif

#ifndef NSAccessibilityPlaceholderValueAttribute
#define NSAccessibilityPlaceholderValueAttribute @"AXPlaceholderValue"
#endif

#ifndef NSAccessibilityScrollToVisibleAction
#define NSAccessibilityScrollToVisibleAction @"AXScrollToVisible"
#endif

#ifndef NSAccessibilityPathAttribute
#define NSAccessibilityPathAttribute @"AXPath"
#endif

#ifndef NSAccessibilityIsMultiSelectableAttribute
#define NSAccessibilityIsMultiSelectableAttribute @"AXIsMultiSelectable"
#endif

#define NSAccessibilityDOMIdentifierAttribute @"AXDOMIdentifier"
#define NSAccessibilityDOMClassListAttribute @"AXDOMClassList"

#ifndef NSAccessibilityARIACurrentAttribute
#define NSAccessibilityARIACurrentAttribute @"AXARIACurrent"
#endif

#ifndef NSAccessibilityKeyShortcutsAttribute
#define NSAccessibilityKeyShortcutsAttribute @"AXKeyShortcutsValue"
#endif

// Table/grid attributes
#ifndef NSAccessibilityARIAColumnIndexAttribute
#define NSAccessibilityARIAColumnIndexAttribute @"AXARIAColumnIndex"
#endif

#ifndef NSAccessibilityARIARowIndexAttribute
#define NSAccessibilityARIARowIndexAttribute @"AXARIARowIndex"
#endif

#ifndef NSAccessibilityARIAColumnCountAttribute
#define NSAccessibilityARIAColumnCountAttribute @"AXARIAColumnCount"
#endif

#ifndef NSAccessibilityARIARowCountAttribute
#define NSAccessibilityARIARowCountAttribute @"AXARIARowCount"
#endif


#ifndef NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute
#define NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute @"AXUIElementCountForSearchPredicate"
#endif

#ifndef NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute
#define NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute @"AXUIElementsForSearchPredicate"
#endif

// Text selection
#ifndef NSAccessibilitySelectTextActivity
#define NSAccessibilitySelectTextActivity @"AXSelectTextActivity"
#endif

#ifndef NSAccessibilitySelectTextActivityFindAndReplace
#define NSAccessibilitySelectTextActivityFindAndReplace @"AXSelectTextActivityFindAndReplace"
#endif

#ifndef NSAccessibilitySelectTextActivityFindAndSelect
#define NSAccessibilitySelectTextActivityFindAndSelect @"AXSelectTextActivityFindAndSelect"
#endif

#ifndef kAXSelectTextActivityFindAndCapitalize
#define kAXSelectTextActivityFindAndCapitalize @"AXSelectTextActivityFindAndCapitalize"
#endif

#ifndef kAXSelectTextActivityFindAndLowercase
#define kAXSelectTextActivityFindAndLowercase @"AXSelectTextActivityFindAndLowercase"
#endif

#ifndef kAXSelectTextActivityFindAndUppercase
#define kAXSelectTextActivityFindAndUppercase @"AXSelectTextActivityFindAndUppercase"
#endif

#ifndef NSAccessibilitySelectTextAmbiguityResolution
#define NSAccessibilitySelectTextAmbiguityResolution @"AXSelectTextAmbiguityResolution"
#endif

#ifndef NSAccessibilitySelectTextAmbiguityResolutionClosestAfterSelection
#define NSAccessibilitySelectTextAmbiguityResolutionClosestAfterSelection @"AXSelectTextAmbiguityResolutionClosestAfterSelection"
#endif

#ifndef NSAccessibilitySelectTextAmbiguityResolutionClosestBeforeSelection
#define NSAccessibilitySelectTextAmbiguityResolutionClosestBeforeSelection @"AXSelectTextAmbiguityResolutionClosestBeforeSelection"
#endif

#ifndef NSAccessibilitySelectTextAmbiguityResolutionClosestToSelection
#define NSAccessibilitySelectTextAmbiguityResolutionClosestToSelection @"AXSelectTextAmbiguityResolutionClosestToSelection"
#endif

#ifndef NSAccessibilitySelectTextReplacementString
#define NSAccessibilitySelectTextReplacementString @"AXSelectTextReplacementString"
#endif

#ifndef NSAccessibilitySelectTextSearchStrings
#define NSAccessibilitySelectTextSearchStrings @"AXSelectTextSearchStrings"
#endif

#ifndef NSAccessibilitySelectTextWithCriteriaParameterizedAttribute
#define NSAccessibilitySelectTextWithCriteriaParameterizedAttribute @"AXSelectTextWithCriteria"
#endif

// Text search

#ifndef NSAccessibilitySearchTextWithCriteriaParameterizedAttribute
/* Performs a text search with the given parameters.
 Returns an NSArray of text marker ranges of the search hits.
 */
#define NSAccessibilitySearchTextWithCriteriaParameterizedAttribute @"AXSearchTextWithCriteria"
#endif

#ifndef NSAccessibilitySearchTextSearchStrings
// NSArray of strings to search for.
#define NSAccessibilitySearchTextSearchStrings @"AXSearchTextSearchStrings"
#endif

#ifndef NSAccessibilitySearchTextStartFrom
// NSString specifying the start point of the search: selection, begin or end.
#define NSAccessibilitySearchTextStartFrom @"AXSearchTextStartFrom"
#endif

#ifndef NSAccessibilitySearchTextStartFromBegin
// Value for SearchTextStartFrom.
#define NSAccessibilitySearchTextStartFromBegin @"AXSearchTextStartFromBegin"
#endif

#ifndef NSAccessibilitySearchTextStartFromSelection
// Value for SearchTextStartFrom.
#define NSAccessibilitySearchTextStartFromSelection @"AXSearchTextStartFromSelection"
#endif

#ifndef NSAccessibilitySearchTextStartFromEnd
// Value for SearchTextStartFrom.
#define NSAccessibilitySearchTextStartFromEnd @"AXSearchTextStartFromEnd"
#endif

#ifndef NSAccessibilitySearchTextDirection
// NSString specifying the direction of the search: forward, backward, closest, all.
#define NSAccessibilitySearchTextDirection @"AXSearchTextDirection"
#endif

#ifndef NSAccessibilitySearchTextDirectionForward
// Value for SearchTextDirection.
#define NSAccessibilitySearchTextDirectionForward @"AXSearchTextDirectionForward"
#endif

#ifndef NSAccessibilitySearchTextDirectionBackward
// Value for SearchTextDirection.
#define NSAccessibilitySearchTextDirectionBackward @"AXSearchTextDirectionBackward"
#endif

#ifndef NSAccessibilitySearchTextDirectionClosest
// Value for SearchTextDirection.
#define NSAccessibilitySearchTextDirectionClosest @"AXSearchTextDirectionClosest"
#endif

#ifndef NSAccessibilitySearchTextDirectionAll
// Value for SearchTextDirection.
#define NSAccessibilitySearchTextDirectionAll @"AXSearchTextDirectionAll"
#endif

// Text operations

#ifndef NSAccessibilityTextOperationParameterizedAttribute
// Performs an operation on the given text.
#define NSAccessibilityTextOperationParameterizedAttribute @"AXTextOperation"
#endif

#ifndef NSAccessibilityTextOperationMarkerRanges
// Text on which to perform operation.
#define NSAccessibilityTextOperationMarkerRanges @"AXTextOperationMarkerRanges"
#endif

#ifndef NSAccessibilityTextOperationType
// The type of operation to be performed: select, replace, capitalize....
#define NSAccessibilityTextOperationType @"AXTextOperationType"
#endif

#ifndef NSAccessibilityTextOperationSelect
// Value for TextOperationType.
#define NSAccessibilityTextOperationSelect @"TextOperationSelect"
#endif

#ifndef NSAccessibilityTextOperationReplace
// Value for TextOperationType.
#define NSAccessibilityTextOperationReplace @"TextOperationReplace"
#endif

#ifndef NSAccessibilityTextOperationCapitalize
// Value for TextOperationType.
#define NSAccessibilityTextOperationCapitalize @"Capitalize"
#endif

#ifndef NSAccessibilityTextOperationLowercase
// Value for TextOperationType.
#define NSAccessibilityTextOperationLowercase @"Lowercase"
#endif

#ifndef NSAccessibilityTextOperationUppercase
// Value for TextOperationType.
#define NSAccessibilityTextOperationUppercase @"Uppercase"
#endif

#ifndef NSAccessibilityTextOperationReplacementString
// Replacement text for operation replace.
#define NSAccessibilityTextOperationReplacementString @"AXTextOperationReplacementString"
#endif

// Math attributes
#define NSAccessibilityMathRootRadicandAttribute @"AXMathRootRadicand"
#define NSAccessibilityMathRootIndexAttribute @"AXMathRootIndex"
#define NSAccessibilityMathFractionDenominatorAttribute @"AXMathFractionDenominator"
#define NSAccessibilityMathFractionNumeratorAttribute @"AXMathFractionNumerator"
#define NSAccessibilityMathBaseAttribute @"AXMathBase"
#define NSAccessibilityMathSubscriptAttribute @"AXMathSubscript"
#define NSAccessibilityMathSuperscriptAttribute @"AXMathSuperscript"
#define NSAccessibilityMathUnderAttribute @"AXMathUnder"
#define NSAccessibilityMathOverAttribute @"AXMathOver"
#define NSAccessibilityMathFencedOpenAttribute @"AXMathFencedOpen"
#define NSAccessibilityMathFencedCloseAttribute @"AXMathFencedClose"
#define NSAccessibilityMathLineThicknessAttribute @"AXMathLineThickness"
#define NSAccessibilityMathPrescriptsAttribute @"AXMathPrescripts"
#define NSAccessibilityMathPostscriptsAttribute @"AXMathPostscripts"

#ifndef NSAccessibilityPreventKeyboardDOMEventDispatchAttribute
#define NSAccessibilityPreventKeyboardDOMEventDispatchAttribute @"AXPreventKeyboardDOMEventDispatch"
#endif

#ifndef NSAccessibilityCaretBrowsingEnabledAttribute
#define NSAccessibilityCaretBrowsingEnabledAttribute @"AXCaretBrowsingEnabled"
#endif

#ifndef NSAccessibilitFocusableAncestorAttribute
#define NSAccessibilityFocusableAncestorAttribute @"AXFocusableAncestor"
#endif

#ifndef NSAccessibilityEditableAncestorAttribute
#define NSAccessibilityEditableAncestorAttribute @"AXEditableAncestor"
#endif

#ifndef NSAccessibilityHighestEditableAncestorAttribute
#define NSAccessibilityHighestEditableAncestorAttribute @"AXHighestEditableAncestor"
#endif

#ifndef NSAccessibilityLinkRelationshipTypeAttribute
#define NSAccessibilityLinkRelationshipTypeAttribute @"AXLinkRelationshipType"
#endif

#ifndef NSAccessibilityRelativeFrameAttribute
#define NSAccessibilityRelativeFrameAttribute @"AXRelativeFrame"
#endif

#ifndef NSAccessibilityBrailleLabelAttribute
#define NSAccessibilityBrailleLabelAttribute @"AXBrailleLabel"
#endif

#ifndef NSAccessibilityBrailleRoleDescriptionAttribute
#define NSAccessibilityBrailleRoleDescriptionAttribute @"AXBrailleRoleDescription"
#endif

#ifndef NSAccessibilityEmbeddedImageDescriptionAttribute
#define NSAccessibilityEmbeddedImageDescriptionAttribute @"AXEmbeddedImageDescription"
#endif

#ifndef NSAccessibilityImageOverlayElementsAttribute
#define NSAccessibilityImageOverlayElementsAttribute @"AXImageOverlayElements"
#endif

#ifndef AXHasDocumentRoleAncestorAttribute
#define AXHasDocumentRoleAncestorAttribute @"AXHasDocumentRoleAncestor"
#endif

#ifndef AXHasWebApplicationAncestorAttribute
#define AXHasWebApplicationAncestorAttribute @"AXHasWebApplicationAncestor"
#endif

#ifndef NSAccessibilityTextInputMarkedRangeAttribute
#define NSAccessibilityTextInputMarkedRangeAttribute @"AXTextInputMarkedRange"
#endif

#ifndef NSAccessibilityTextInputMarkedTextMarkerRangeAttribute
#define NSAccessibilityTextInputMarkedTextMarkerRangeAttribute @"AXTextInputMarkedTextMarkerRange"
#endif

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
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* widget = backingObject->widgetForAttachmentView();
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

- (IntRect)screenToContents:(const IntRect&)rect
{
    ASSERT(isMainThread());

    Document* document = self.axBackingObject->document();
    if (!document)
        return IntRect();
    
    auto* frameView = document->view();
    if (!frameView)
        return IntRect();
    
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

    NSString *activityParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextActivity];
    NSString *ambiguityResolutionParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextAmbiguityResolution];
    NSString *replacementStringParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextReplacementString];
    NSArray *searchStringsParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextSearchStrings];

    if ([activityParameter isKindOfClass:[NSString class]]) {
        if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndReplace])
            operation.type = AccessibilityTextOperationType::Replace;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndCapitalize])
            operation.type = AccessibilityTextOperationType::Capitalize;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndLowercase])
            operation.type = AccessibilityTextOperationType::Lowercase;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndUppercase])
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
        operation.replacementText = replacementStringParameter;

    if ([searchStringsParameter isKindOfClass:[NSArray class]])
        criteria.searchStrings = makeVector<String>(searchStringsParameter);

    return std::make_pair(criteria, operation);
}

static AccessibilitySearchTextCriteria accessibilitySearchTextCriteriaForParameterizedAttribute(const NSDictionary *params)
{
    AccessibilitySearchTextCriteria criteria;

    NSArray *searchStrings = [params objectForKey:NSAccessibilitySearchTextSearchStrings];
    NSString *start = [params objectForKey:NSAccessibilitySearchTextStartFrom];
    NSString *direction = [params objectForKey:NSAccessibilitySearchTextDirection];

    if ([searchStrings isKindOfClass:[NSArray class]])
        criteria.searchStrings = makeVector<String>(searchStrings);

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

    NSArray *markerRanges = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationMarkerRanges];
    NSString *operationType = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationType];
    NSString *replacementString = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationReplacementString];

    if ([markerRanges isKindOfClass:[NSArray class]]) {
        operation.textRanges = makeVector(markerRanges, [&axObjectCache] (id markerRange) {
            ASSERT(AXObjectIsTextMarkerRange(markerRange));
            return rangeForTextMarkerRange(axObjectCache, (AXTextMarkerRangeRef)markerRange);
        });
    }

    if ([operationType isKindOfClass:[NSString class]]) {
        if ([operationType isEqualToString:NSAccessibilityTextOperationReplace])
            operation.type = AccessibilityTextOperationType::Replace;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationCapitalize])
            operation.type = AccessibilityTextOperationType::Capitalize;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationLowercase])
            operation.type = AccessibilityTextOperationType::Lowercase;
        else if ([operationType isEqualToString:NSAccessibilityTextOperationUppercase])
            operation.type = AccessibilityTextOperationType::Uppercase;
    }

    if ([replacementString isKindOfClass:[NSString class]])
        operation.replacementText = replacementString;

    return operation;
}

static std::pair<AXTextMarkerRange, AccessibilitySearchDirection> misspellingSearchCriteriaForParameterizedAttribute(const NSDictionary *params)
{
    id markerRangeRef = [params objectForKey:@"AXStartTextMarkerRange"];
    if (!AXObjectIsTextMarkerRange(markerRangeRef))
        return { };

    std::pair<AXTextMarkerRange, AccessibilitySearchDirection> criteria;
    criteria.first = AXTextMarkerRange { (AXTextMarkerRangeRef)markerRangeRef };

    NSNumber *forward = [params objectForKey:NSAccessibilitySearchTextDirection];
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

- (RetainPtr<AXTextMarkerRef>)textMarkerForFirstPositionInTextControl:(HTMLTextFormControlElement &)textControl
{
    ASSERT(isMainThread());

    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    auto* cache = backingObject->axObjectCache();
    if (!cache)
        return nil;

    auto textMarkerData = cache->textMarkerDataForFirstPositionInTextControl(textControl);
    if (!textMarkerData)
        return nil;

    return adoptCF(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData.value(), sizeof(textMarkerData.value())));
}

- (NSAttributedString *)attributedStringForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRangeRef spellCheck:(AXCoreObject::SpellCheck)spellCheck
{
    if (!textMarkerRangeRef)
        return nil;

    RefPtr<AXCoreObject> object = self.axBackingObject;
    return object ? object->attributedStringForTextMarkerRange({ textMarkerRangeRef }, spellCheck).autorelease() : nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray*)accessibilityActionNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
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

    NSArray *actions;
    if (backingObject->supportsPressAction())
        actions = actionElementActions.get().get();
    else if (backingObject->isMenuRelated())
        actions = menuElementActions.get().get();
    else if (backingObject->isSlider() || (backingObject->isSpinButton() && backingObject->spinButtonType() == SpinButtonType::Standalone)) {
        // Non-standalone spinbuttons should not advertise the increment and decrement actions because they have separate increment and decrement controls.
        actions = incrementorActions.get().get();
    }
    else if (backingObject->isAttachment())
        actions = [[self attachmentView] accessibilityActionNames];
    else
        actions = defaultElementActions.get().get();

    return actions;
}

- (NSArray*)additionalAccessibilityAttributeNames
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    NSMutableArray *additional = [NSMutableArray array];
    if (backingObject->supportsARIAOwns())
        [additional addObject:NSAccessibilityOwnsAttribute];

    if (backingObject->isToggleButton())
        [additional addObject:NSAccessibilityValueAttribute];

    if (backingObject->supportsExpanded() || backingObject->isSummary())
        [additional addObject:NSAccessibilityExpandedAttribute];

    if (backingObject->isScrollbar()
        || backingObject->isRadioGroup()
        || backingObject->isSplitter()
        || backingObject->isToolbar())
        [additional addObject:NSAccessibilityOrientationAttribute];

    if (backingObject->supportsDragging())
        [additional addObject:NSAccessibilityGrabbedAttribute];

    if (backingObject->supportsDropping())
        [additional addObject:NSAccessibilityDropEffectsAttribute];

    if (backingObject->isTable() && backingObject->isExposable() && backingObject->supportsSelectedRows())
        [additional addObject:NSAccessibilitySelectedRowsAttribute];

    if (backingObject->supportsLiveRegion()) {
        [additional addObject:NSAccessibilityARIALiveAttribute];
        [additional addObject:NSAccessibilityARIARelevantAttribute];
    }

    if (backingObject->supportsSetSize())
        [additional addObject:NSAccessibilityARIASetSizeAttribute];

    if (backingObject->supportsPosInSet())
        [additional addObject:NSAccessibilityARIAPosInSetAttribute];

    if (backingObject->supportsKeyShortcuts())
        [additional addObject:NSAccessibilityKeyShortcutsAttribute];

    AccessibilitySortDirection sortDirection = backingObject->sortDirection();
    if (sortDirection != AccessibilitySortDirection::None && sortDirection != AccessibilitySortDirection::Invalid)
        [additional addObject:NSAccessibilitySortDirectionAttribute];

    // If an object is a child of a live region, then add these
    if (backingObject->isInsideLiveRegion())
        [additional addObject:NSAccessibilityARIAAtomicAttribute];

    // All objects should expose the ARIA busy attribute (ARIA 1.1 with ISSUE-538).
    [additional addObject:NSAccessibilityElementBusyAttribute];

    // Popup buttons on the Mac expose the value attribute.
    if (backingObject->isPopUpButton())
        [additional addObject:NSAccessibilityValueAttribute];

    if (backingObject->supportsDatetimeAttribute())
        [additional addObject:NSAccessibilityDatetimeValueAttribute];

    if (backingObject->supportsRequiredAttribute())
        [additional addObject:NSAccessibilityRequiredAttribute];

    if (backingObject->hasPopup())
        [additional addObject:NSAccessibilityHasPopupAttribute];

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

    if (backingObject->supportsPath())
        [additional addObject:NSAccessibilityPathAttribute];

    if (backingObject->supportsExpandedTextValue())
        [additional addObject:NSAccessibilityExpandedTextValueAttribute];

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (AXObjectCache::isIsolatedTreeEnabled())
        [additional addObject:NSAccessibilityRelativeFrameAttribute];
#endif
    
    return additional;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray*)accessibilityAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeNames];

    static NeverDestroyed<RetainPtr<NSArray>> attributes = @[
        AXHasDocumentRoleAncestorAttribute,
        AXHasWebApplicationAncestorAttribute,
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
        AXSelectedTextMarkerRangeAttribute,
        AXStartTextMarkerAttribute,
        AXEndTextMarkerAttribute,
        @"AXVisited",
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
    static NeverDestroyed<RetainPtr<NSArray>> commonMenuAttrs = @[
        NSAccessibilityRoleAttribute,
        NSAccessibilityRoleDescriptionAttribute,
        NSAccessibilityChildrenAttribute,
        NSAccessibilityParentAttribute,
        NSAccessibilityEnabledAttribute,
        NSAccessibilityPositionAttribute,
        NSAccessibilitySizeAttribute,
    ];
    static NeverDestroyed webAreaAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        // WebAreas should not expose AXSubrole.
        [tempArray removeObject:NSAccessibilitySubroleAttribute];
        // WebAreas should not expose ancestor attributes
        [tempArray removeObject:NSAccessibilityFocusableAncestorAttribute];
        [tempArray removeObject:NSAccessibilityEditableAncestorAttribute];
        [tempArray removeObject:NSAccessibilityHighestEditableAncestorAttribute];
        [tempArray addObject:@"AXLinkUIElements"];
        [tempArray addObject:@"AXLoaded"];
        [tempArray addObject:@"AXLayoutCount"];
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
        [tempArray addObject:NSAccessibilityVisibleCharacterRangeAttribute];
        [tempArray addObject:NSAccessibilityInsertionPointLineNumberAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityPlaceholderValueAttribute];
        return tempArray;
    }();
    static NeverDestroyed listAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
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
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        return tempArray;
    }();
    static NeverDestroyed menuBarAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:commonMenuAttrs.get().get()]);
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed menuAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:commonMenuAttrs.get().get()]);
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed menuItemAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:commonMenuAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityTitleAttribute];
        [tempArray addObject:NSAccessibilityDescriptionAttribute];
        [tempArray addObject:NSAccessibilityHelpAttribute];
        [tempArray addObject:NSAccessibilitySelectedAttribute];
        [tempArray addObject:NSAccessibilityValueAttribute];
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
    static NeverDestroyed<RetainPtr<NSArray>> menuButtonAttrs = @[
        NSAccessibilityRoleAttribute,
        NSAccessibilityRoleDescriptionAttribute,
        NSAccessibilityParentAttribute,
        NSAccessibilityPositionAttribute,
        NSAccessibilitySizeAttribute,
        NSAccessibilityWindowAttribute,
        NSAccessibilityEnabledAttribute,
        NSAccessibilityFocusedAttribute,
        NSAccessibilityTitleAttribute,
        NSAccessibilityChildrenAttribute,
    ];
    static NeverDestroyed controlAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        return tempArray;
    }();
    static NeverDestroyed buttonAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        // Buttons should not expose AXValue.
        [tempArray removeObject:NSAccessibilityValueAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        return tempArray;
    }();
    static NeverDestroyed comboBoxAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:controlAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityExpandedAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
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
        return tempArray;
    }();
    static NeverDestroyed tableRowAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityIndexAttribute];
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
        return tempArray;
    }();
    static NeverDestroyed groupAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        return tempArray;
    }();
    static NeverDestroyed inputImageAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:buttonAttrs.get().get()]);
        [tempArray addObject:NSAccessibilityURLAttribute];
        return tempArray;
    }();
    static NeverDestroyed secureFieldAttributes = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityPlaceholderValueAttribute];
        return tempArray;
    }();
    static NeverDestroyed tabListAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilityTabsAttribute];
        [tempArray addObject:NSAccessibilityContentsAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        return tempArray;
    }();
    static NeverDestroyed outlineAttrs = [] {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:attributes.get().get()]);
        [tempArray addObject:NSAccessibilitySelectedRowsAttribute];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityColumnsAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
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

    NSArray *objectAttributes = attributes.get().get();

    if (backingObject->isSecureField())
        objectAttributes = secureFieldAttributes.get().get();
    else if (backingObject->isWebArea())
        objectAttributes = webAreaAttrs.get().get();
    else if (backingObject->isTextControl())
        objectAttributes = textAttrs.get().get();
    else if (backingObject->isLink())
        objectAttributes = anchorAttrs.get().get();
    else if (backingObject->isImage())
        objectAttributes = imageAttrs.get().get();
    else if (backingObject->isTable() && backingObject->isExposable())
        objectAttributes = tableAttrs.get().get();
    else if (backingObject->isTableColumn())
        objectAttributes = tableColAttrs.get().get();
    else if (backingObject->isExposedTableCell())
        objectAttributes = tableCellAttrs.get().get();
    else if (backingObject->isTableRow()) {
        // An ARIA table row can be collapsed and expanded, so it needs the extra attributes.
        if (backingObject->isARIATreeGridRow())
            objectAttributes = outlineRowAttrs.get().get();
        else
            objectAttributes = tableRowAttrs.get().get();
    } else if (backingObject->isTree())
        objectAttributes = outlineAttrs.get().get();
    else if (backingObject->isTreeItem()) {
        if (backingObject->supportsCheckedState())
            objectAttributes = [outlineRowAttrs.get() arrayByAddingObject:NSAccessibilityValueAttribute];
        else
            objectAttributes = outlineRowAttrs.get().get();
    }
    else if (backingObject->isListBox())
        objectAttributes = listBoxAttrs.get().get();
    else if (backingObject->isList())
        objectAttributes = listAttrs.get().get();
    else if (backingObject->isComboBox())
        objectAttributes = comboBoxAttrs.get().get();
    else if (backingObject->isProgressIndicator() || backingObject->isSlider())
        objectAttributes = rangeAttrs.get().get();
    // These are processed in order because an input image is a button, and a button is a control.
    else if (backingObject->isInputImage())
        objectAttributes = inputImageAttrs.get().get();
    else if (backingObject->isButton())
        objectAttributes = buttonAttrs.get().get();
    else if (backingObject->isControl())
        objectAttributes = controlAttrs.get().get();

    else if (backingObject->isGroup() || backingObject->isListItem())
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
    }
    else if (backingObject->isMenu())
        objectAttributes = menuAttrs.get().get();
    else if (backingObject->isMenuBar())
        objectAttributes = menuBarAttrs.get().get();
    else if (backingObject->isMenuButton())
        objectAttributes = menuButtonAttrs.get().get();
    else if (backingObject->isMenuItem())
        objectAttributes = menuItemAttrs.get().get();
    else if (backingObject->isVideo())
        objectAttributes = videoAttrs.get().get();

    NSArray *additionalAttributes = [self additionalAccessibilityAttributeNames];
    if ([additionalAttributes count])
        objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:additionalAttributes];

    // Only expose AXARIACurrent attribute when the element is set to be current item.
    if (backingObject->currentState() != AccessibilityCurrentState::False)
        objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:@[ NSAccessibilityARIACurrentAttribute ]];

    // AppKit needs to know the screen height in order to do the coordinate conversion.
    objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:@[ NSAccessibilityPrimaryScreenHeightAttribute ]];

    return objectAttributes;
}

- (NSArray *)renderWidgetChildren
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject || !backingObject->isWidget())
        return nil;

    id child = Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* widget = backingObject->widget();
        return widget ? widget->accessibilityObject() : nil;
    });

    if (child)
        return @[child];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [backingObject->platformWidget() accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (id)remoteAccessibilityParentObject
{
    auto* backingObject = self.axBackingObject;
    return backingObject ? backingObject->remoteParentObject() : nil;
}

static void convertToVector(NSArray* array, AccessibilityObject::AccessibilityChildrenVector& vector)
{
    unsigned length = [array count];
    vector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        AXCoreObject* obj = [[array objectAtIndex:i] axBackingObject];
        if (obj)
            vector.append(obj);
    }
}

- (AXTextMarkerRangeRef)selectedTextMarkerRange
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    auto range = backingObject->selectedTextMarkerRange();
    if (!range.start().isValid() || !range.end().isValid())
        return nil;

    return range;
}

- (id)associatedPluginParent
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject || !backingObject->hasApplePDFAnnotationAttribute())
            return nil;
        auto* document = dynamicDowncast<PluginDocument>(backingObject->document());
        if (!document)
            return nil;
        auto* widget = document->pluginWidget();
        if (!widget)
            return nil;
        return widget->accessibilityAssociatedPluginParentForElement(backingObject->element());
    });
}

static void WebTransformCGPathToNSBezierPath(void* info, const CGPathElement *element)
{
    NSBezierPath *bezierPath = (__bridge NSBezierPath *)info;
    switch (element->type) {
    case kCGPathElementMoveToPoint:
        [bezierPath moveToPoint:NSPointFromCGPoint(element->points[0])];
        break;
    case kCGPathElementAddLineToPoint:
        [bezierPath lineToPoint:NSPointFromCGPoint(element->points[0])];
        break;
    case kCGPathElementAddCurveToPoint:
        [bezierPath curveToPoint:NSPointFromCGPoint(element->points[0]) controlPoint1:NSPointFromCGPoint(element->points[1]) controlPoint2:NSPointFromCGPoint(element->points[2])];
        break;
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
    Path path = self.axBackingObject->elementPath();
    if (path.isEmpty())
        return NULL;
    
    CGPathRef transformedPath = [self convertPathToScreenSpace:path];
    return [self bezierPathFromPath:transformedPath];
}

- (size_t)childrenVectorSize
{
    return self.axBackingObject->children().size();
}

- (NSArray<WebAccessibilityObjectWrapper *> *)childrenVectorArray
{
    return makeNSArray(self.axBackingObject->children());
}

- (NSString*)role
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (self.axBackingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    NSString *string = self.axBackingObject->rolePlatformString();
    if (string.length)
        return string;
    return NSAccessibilityUnknownRole;
}

- (BOOL)isEmptyGroup
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return false;

#if ENABLE(MODEL_ELEMENT)
    if (backingObject->isModel())
        return false;
#endif

    return [[self role] isEqual:NSAccessibilityGroupRole]
        && backingObject->children().isEmpty()
        && ![[self renderWidgetChildren] count];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSString*)subrole
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    if ([self isEmptyGroup])
        return @"AXEmptyGroup";

    auto subrole = backingObject->subrolePlatformString();
    if (!subrole.isEmpty())
        return subrole;

    return nil;
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (NSString*)roleDescription
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // attachments have the AXImage role, but a different subrole
    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END

    String roleDescription = backingObject->roleDescription();
    if (!roleDescription.isEmpty())
        return roleDescription;

    NSString *axRole = backingObject->rolePlatformString();
    NSString *subrole = self.subrole;
    // Fallback to the system default role description.
    // If we get the same string back, then as a last resort, return unknown.
    NSString *defaultRoleDescription = NSAccessibilityRoleDescription(axRole, subrole);

    // On earlier Mac versions (Lion), using a non-standard subrole would result in a role description
    // being returned that looked like AXRole:AXSubrole. To make all platforms have the same role descriptions
    // we should fallback on a role description ignoring the subrole in these cases.
    if ([defaultRoleDescription isEqualToString:[NSString stringWithFormat:@"%@:%@", axRole, subrole]])
        defaultRoleDescription = NSAccessibilityRoleDescription(axRole, nil);

    if (![defaultRoleDescription isEqualToString:axRole])
        return defaultRoleDescription;

    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
}

- (NSString *)_computedRoleString
{
    if (!self.axBackingObject)
        return nil;
    return self.axBackingObject->computedRoleString();
}

- (id)scrollViewParent
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject || !backingObject->isScrollView())
        return nil;

    // If this scroll view provides it's parent object (because it's a sub-frame), then
    // we should not find the remoteAccessibilityParent.
    if (backingObject->parentObject())
        return nil;

    if (auto platformWidget = backingObject->platformWidget())
        return NSAccessibilityUnignoredAncestor(platformWidget);

    return backingObject->remoteParentObject();
}

- (id)windowElement:(NSString *)attributeName
{
    if (id remoteParent = self.remoteAccessibilityParentObject) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        return [remoteParent accessibilityAttributeValue:attributeName];
ALLOW_DEPRECATED_DECLARATIONS_END
    }

    RefPtr axScrollView = self.axBackingObject->axScrollView();
    return axScrollView ? [axScrollView->platformWidget() window] : nil;
}

// FIXME: split up this function in a better way.
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString*)attributeName
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityAttributeValue:", String(attributeName)));

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper ", hex(reinterpret_cast<uintptr_t>(self))));
        return nil;
    }

    if (backingObject->isDetachedFromParent()) {
        AXLOG("backingObject is detached from parent!!!");
        AXLOG(backingObject);
        return nil;
    }

    if ([attributeName isEqualToString: NSAccessibilityRoleAttribute])
        return [self role];

    if ([attributeName isEqualToString: NSAccessibilitySubroleAttribute])
        return [self subrole];

    if ([attributeName isEqualToString: NSAccessibilityRoleDescriptionAttribute])
        return [self roleDescription];

    // AXARIARole is only used by DumpRenderTree (so far).
    if ([attributeName isEqualToString:@"AXARIARole"])
        return [self _computedRoleString];

    if ([attributeName isEqualToString: NSAccessibilityParentAttribute]) {
        // This will return the parent of the AXWebArea, if this is a web area.
        id scrollViewParent = [self scrollViewParent];
        if (scrollViewParent)
            return scrollViewParent;

        // Tree item (changed to AXRows) can only report the tree (AXOutline) as its parent.
        if (backingObject->isTreeItem()) {
            auto parent = backingObject->parentObjectUnignored();
            while (parent) {
                if (parent->isTree())
                    return parent->wrapper();
                parent = parent->parentObjectUnignored();
            }
        }

        auto parent = backingObject->parentObjectUnignored();
        if (!parent)
            return nil;

        // In WebKit1, the scroll view is provided by the system (the attachment view), so the parent
        // should be reported directly as such.
        if (backingObject->isWebArea() && parent->isAttachment())
            return [parent->wrapper() attachmentView];

        return parent->wrapper();
    }

    if ([attributeName isEqualToString:NSAccessibilityChildrenAttribute] || [attributeName isEqualToString:NSAccessibilityChildrenInNavigationOrderAttribute]) {
#if ENABLE(MODEL_ELEMENT)
        if (backingObject->isModel()) {
            auto modelChildren = backingObject->modelElementChildren();
            if (modelChildren.size()) {
                return createNSArray(WTFMove(modelChildren), [] (auto&& child) -> id {
                    return child.get();
                }).autorelease();
            }
        }
#endif

        if (!self.childrenVectorSize) {
            if (NSArray *children = [self renderWidgetChildren])
                return children;
        }

        // The tree's (AXOutline) children are supposed to be its rows and columns.
        // The ARIA spec doesn't have columns, so we just need rows.
        if (backingObject->isTree())
            return [self accessibilityAttributeValue:NSAccessibilityRowsAttribute];

        // A tree item should only expose its content as its children (not its rows)
        if (backingObject->isTreeItem())
            return makeNSArray(backingObject->ariaTreeItemContent());

        return self.childrenVectorArray;
    }

    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (backingObject->canHaveSelectedChildren())
            return makeNSArray(backingObject->selectedChildren());
        return nil;
    }

    if ([attributeName isEqualToString: NSAccessibilityVisibleChildrenAttribute]) {
        if (backingObject->isListBox())
            return makeNSArray(backingObject->visibleChildren());

        if (backingObject->isList())
            return [self accessibilityAttributeValue:NSAccessibilityChildrenAttribute];

        return nil;
    }


    if (backingObject->isWebArea()) {
        if ([attributeName isEqualToString:@"AXLinkUIElements"])
            return makeNSArray(backingObject->documentLinks());

        if ([attributeName isEqualToString:@"AXLoaded"])
            return [NSNumber numberWithBool:backingObject->isLoaded()];
        if ([attributeName isEqualToString:@"AXLayoutCount"])
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
            return (NSString*)selectedText;
        }

        if ([attributeName isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
            return [NSValue valueWithRange:backingObject->selectedTextRange()];

        if ([attributeName isEqualToString:NSAccessibilityInsertionPointLineNumberAttribute]) {
            int lineNumber = backingObject->insertionPointLineNumber();
            return lineNumber >= 0 ? @(lineNumber) : nil;
        }
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
        return (NSURL*)url;
    }

    if ([attributeName isEqualToString:NSAccessibilityIncrementButtonAttribute]) {
        auto incrementButton = backingObject->incrementButton();
        return incrementButton ? incrementButton->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityDecrementButtonAttribute]) {
        auto decrementButton = backingObject->decrementButton();
        return decrementButton ? decrementButton->wrapper() : nil;
    }

    if ([attributeName isEqualToString: @"AXVisited"])
        return [NSNumber numberWithBool: backingObject->isVisited()];

    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute]) {
        if (backingObject->isAttachment()) {
            id attachmentView = [self attachmentView];
            if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute])
                return [attachmentView accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }

        return backingObject->titleAttributeValue();
    }

    if ([attributeName isEqualToString:NSAccessibilityDescriptionAttribute]) {
        if (backingObject->isAttachment()) {
            id attachmentView = [self attachmentView];
            if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [attachmentView accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return backingObject->descriptionAttributeValue();
    }

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
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
            [] (String& typedValue) -> id { return (NSString *)typedValue; },
            [] (AccessibilityButtonState& typedValue) -> id { return @((unsigned)typedValue); },
            [] (AXCoreObject*& typedValue) { return typedValue ? (id)typedValue->wrapper() : nil; },
            [] (auto&) { return nil; }
        );
    }

    if ([attributeName isEqualToString:(NSString *)kAXMenuItemMarkCharAttribute]) {
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

    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute]) {
        auto size = backingObject->size();
        return [NSValue valueWithSize:NSMakeSize(size.width(), size.height())];
    }

    if ([attributeName isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return @(backingObject->primaryScreenRect().height());

    if ([attributeName isEqualToString:NSAccessibilityPositionAttribute])
        return @(CGPoint(backingObject->screenRelativePosition()));

    if ([attributeName isEqualToString:NSAccessibilityPathAttribute])
        return [self path];

    if ([attributeName isEqualToString:@"AXLineRectsAndText"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<NSArray *>([protectedSelf = retainPtr(self)] () -> RetainPtr<NSArray> {
            return protectedSelf.get().lineRectsAndText;
        });
    }

    if ([attributeName isEqualToString:NSAccessibilityImageOverlayElementsAttribute]) {
        auto imageOverlayElements = backingObject->imageOverlayElements();
        return imageOverlayElements ? makeNSArray(*imageOverlayElements) : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityEmbeddedImageDescriptionAttribute])
        return backingObject->embeddedImageDescription();

    if ([attributeName isEqualToString:NSAccessibilityWindowAttribute]
        || [attributeName isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [self windowElement:attributeName];

    if ([attributeName isEqualToString:NSAccessibilityAccessKeyAttribute]) {
        auto accessKey = backingObject->accessKey();
        if (accessKey.isNull())
            return nil;
        return accessKey;
    }

    if ([attributeName isEqualToString:NSAccessibilityLinkRelationshipTypeAttribute])
        return backingObject->linkRelValue();

    if ([attributeName isEqualToString:NSAccessibilityTabsAttribute] && backingObject->isTabList())
        return makeNSArray(backingObject->tabChildren());

    if ([attributeName isEqualToString:NSAccessibilityContentsAttribute])
        return makeNSArray(backingObject->contents());

    if (backingObject->isTable() && backingObject->isExposable()) {
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute])
            return makeNSArray(backingObject->rows());

        if ([attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute])
            return makeNSArray(backingObject->visibleRows());

        // TODO: distinguish between visible and non-visible columns
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute]
            || [attributeName isEqualToString:NSAccessibilityVisibleColumnsAttribute])
            return makeNSArray(backingObject->columns());

        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute])
            return makeNSArray(backingObject->selectedChildren());

        // HTML tables don't support this attribute yet.
        if ([attributeName isEqualToString:NSAccessibilitySelectedColumnsAttribute])
            return nil;

        if ([attributeName isEqualToString:NSAccessibilitySelectedCellsAttribute])
            return makeNSArray(backingObject->selectedCells());

        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute])
            return makeNSArray(backingObject->columnHeaders());

        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            auto* headerContainer = backingObject->headerContainer();
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
            return makeNSArray(backingObject->children());

        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            auto* header = backingObject->columnHeader();
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

        if ([attributeName isEqualToString:NSAccessibilityARIAColumnIndexAttribute])
            return @(backingObject->axColumnIndex());

        if ([attributeName isEqualToString:NSAccessibilityARIARowIndexAttribute])
            return @(backingObject->axRowIndex());
    }

    if (backingObject->isTree()) {
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute])
            return makeNSArray(backingObject->selectedChildren());

        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            backingObject->ariaTreeRows(rowsCopy);
            return makeNSArray(rowsCopy);
        }

        // TreeRoles do not support columns, but Mac AX expects to be able to ask about columns at the least.
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute])
            return @[];
    }

    if ([attributeName isEqualToString:NSAccessibilityIndexAttribute]) {
        if (backingObject->isTreeItem()) {
            AXCoreObject* parent = backingObject->parentObject();
            for (; parent && !parent->isTree(); parent = parent->parentObject())
            { }

            if (!parent)
                return nil;

            // Find the index of this item by iterating the parents.
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            parent->ariaTreeRows(rowsCopy);
            size_t count = rowsCopy.size();
            for (size_t k = 0; k < count; ++k)
                if (rowsCopy[k]->wrapper() == self)
                    return @(k);

            return nil;
        }

        if (backingObject->isTableRow())
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
            AXCoreObject* parent = backingObject->parentObject();
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
            auto* row = backingObject->disclosedByRow();
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

    if (backingObject->isList() && [attributeName isEqualToString:NSAccessibilityOrientationAttribute])
        return NSAccessibilityVerticalOrientationValue;

    if ([attributeName isEqualToString:AXSelectedTextMarkerRangeAttribute])
        return (id)[self selectedTextMarkerRange];

    if ([attributeName isEqualToString:AXStartTextMarkerAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            return (id)textMarkerForVisiblePosition(backingObject->axObjectCache(), startOfDocument(backingObject->document()));
        });
    }

    if ([attributeName isEqualToString:AXEndTextMarkerAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            return (id)textMarkerForVisiblePosition(backingObject->axObjectCache(), endOfDocument(backingObject->document()));
        });
    }

    if ([attributeName isEqualToString:NSAccessibilityBlockQuoteLevelAttribute])
        return @(backingObject->blockquoteLevel());
    if ([attributeName isEqualToString:@"AXTableLevel"])
        return @(backingObject->tableLevel());

    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute])
        return makeNSArray(backingObject->linkedObjects());

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return [NSNumber numberWithBool:backingObject->isSelected()];

    if ([attributeName isEqualToString: NSAccessibilityARIACurrentAttribute])
        return backingObject->currentValue();

    if ([attributeName isEqualToString: NSAccessibilityServesAsTitleForUIElementsAttribute] && backingObject->isMenuButton()) {
        if (auto* axRenderObject = dynamicDowncast<AccessibilityRenderObject>(backingObject.get())) {
            if (auto* uiElement = axRenderObject->menuForMenuButton())
                return @[uiElement->wrapper()];
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
        auto* object = backingObject->titleUIElement();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityValueDescriptionAttribute])
        return backingObject->valueDescription();

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
        switch (backingObject->sortDirection()) {
        case AccessibilitySortDirection::Ascending:
            return NSAccessibilityAscendingSortDirectionValue;
        case AccessibilitySortDirection::Descending:
            return NSAccessibilityDescendingSortDirectionValue;
        default:
            return NSAccessibilityUnknownSortDirectionValue;
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityLanguageAttribute])
        return backingObject->language();

    if ([attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        return [NSNumber numberWithBool:backingObject->isExpanded()];

    if ([attributeName isEqualToString:NSAccessibilityRequiredAttribute])
        return [NSNumber numberWithBool:backingObject->isRequired()];

    if ([attributeName isEqualToString:NSAccessibilityInvalidAttribute])
        return backingObject->invalidStatus();

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
        return backingObject->placeholderValue();

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
        return [NSNumber numberWithBool:backingObject->hasPopup()];

    if ([attributeName isEqualToString:NSAccessibilityDatetimeValueAttribute])
        return backingObject->datetimeAttributeValue();

    if ([attributeName isEqualToString:NSAccessibilityInlineTextAttribute])
        return @(backingObject->isInlineText());

    // ARIA Live region attributes.
    if ([attributeName isEqualToString:NSAccessibilityARIALiveAttribute])
        return backingObject->liveRegionStatus();
    if ([attributeName isEqualToString:NSAccessibilityARIARelevantAttribute])
        return backingObject->liveRegionRelevant();
    if ([attributeName isEqualToString:NSAccessibilityARIAAtomicAttribute])
        return [NSNumber numberWithBool:backingObject->liveRegionAtomic()];
    if ([attributeName isEqualToString:NSAccessibilityElementBusyAttribute])
        return [NSNumber numberWithBool:backingObject->isBusy()];

    // MathML Attributes.
    if (backingObject->isMathElement()) {
        if ([attributeName isEqualToString:NSAccessibilityMathRootIndexAttribute]) {
            auto* rootIndex = backingObject->mathRootIndexObject();
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
            return backingObject->mathFencedOpenString();
        if ([attributeName isEqualToString:NSAccessibilityMathFencedCloseAttribute])
            return backingObject->mathFencedCloseString();
        if ([attributeName isEqualToString:NSAccessibilityMathLineThicknessAttribute])
            return [NSNumber numberWithInteger:backingObject->mathLineThickness()];
        if ([attributeName isEqualToString:NSAccessibilityMathPostscriptsAttribute])
            return [self accessibilityMathPostscriptPairs];
        if ([attributeName isEqualToString:NSAccessibilityMathPrescriptsAttribute])
            return [self accessibilityMathPrescriptPairs];
    }

    if ([attributeName isEqualToString:NSAccessibilityExpandedTextValueAttribute])
        return backingObject->expandedTextValue();

    if ([attributeName isEqualToString:NSAccessibilityDOMIdentifierAttribute])
        return backingObject->identifierAttribute();
    if ([attributeName isEqualToString:NSAccessibilityDOMClassListAttribute])
        return createNSArray(backingObject->classList()).autorelease();

    if ([attributeName isEqualToString:@"AXResolvedEditingStyles"])
        return [self baseAccessibilityResolvedEditingStyles];
    
    // This allows us to connect to a plugin that creates a shadow node for editing (like PDFs).
    if ([attributeName isEqualToString:@"_AXAssociatedPluginParent"])
        return [self associatedPluginParent];

    // this is used only by DumpRenderTree for testing
    if ([attributeName isEqualToString:@"AXClickPoint"])
        return [NSValue valueWithPoint:backingObject->clickPoint()];

    // This is used by DRT to verify CSS3 speech works.
    if ([attributeName isEqualToString:@"AXDRTSpeechAttribute"])
        return [self baseAccessibilitySpeechHint];

    if ([attributeName isEqualToString:@"AXAutocompleteValue"])
        return backingObject->autoCompleteValue();

    if ([attributeName isEqualToString:NSAccessibilityPopupValueAttribute])
        return backingObject->popupValue();

    if ([attributeName isEqualToString:NSAccessibilityKeyShortcutsAttribute])
        return backingObject->keyShortcuts();

    if ([attributeName isEqualToString:@"AXARIAPressedIsPresent"])
        return [NSNumber numberWithBool:backingObject->pressedIsPresent()];

    if ([attributeName isEqualToString:AXHasDocumentRoleAncestorAttribute])
        return [NSNumber numberWithBool:backingObject->hasDocumentRoleAncestor()];

    if ([attributeName isEqualToString:AXHasWebApplicationAncestorAttribute])
        return [NSNumber numberWithBool:backingObject->hasWebApplicationAncestor()];

    if ([attributeName isEqualToString:@"AXIsInDescriptionListDetail"])
        return [NSNumber numberWithBool:backingObject->isInDescriptionListDetail()];

    if ([attributeName isEqualToString:@"AXIsInDescriptionListTerm"])
        return [NSNumber numberWithBool:backingObject->isInDescriptionListTerm()];

    if ([attributeName isEqualToString:@"AXIsInCell"])
        return [NSNumber numberWithBool:backingObject->isInCell()];

    if ([attributeName isEqualToString:@"AXDetailsElements"])
        return makeNSArray(backingObject->detailedByObjects());

    if ([attributeName isEqualToString:NSAccessibilityBrailleLabelAttribute])
        return backingObject->brailleLabel();

    if ([attributeName isEqualToString:NSAccessibilityBrailleRoleDescriptionAttribute])
        return backingObject->brailleRoleDescription();

    if ([attributeName isEqualToString:NSAccessibilityRelativeFrameAttribute])
        return [NSValue valueWithRect:(NSRect)backingObject->relativeFrame()];

    if ([attributeName isEqualToString:@"AXErrorMessageElements"]) {
        // Only expose error messages for objects in an invalid state.
        // https://www.w3.org/TR/wai-aria-1.2/#aria-errormessage
        if (backingObject->invalidStatus() == "false"_s)
            return nil;
        return makeNSArray(backingObject->errorMessageObjects());
    }

    // Multi-selectable
    if ([attributeName isEqualToString:NSAccessibilityIsMultiSelectableAttribute])
        return [NSNumber numberWithBool:backingObject->isMultiSelectable()];

    if ([attributeName isEqualToString:NSAccessibilityFocusableAncestorAttribute]) {
        AXCoreObject* object = backingObject->focusableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityEditableAncestorAttribute]) {
        AXCoreObject* object = backingObject->editableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityHighestEditableAncestorAttribute]) {
        AXCoreObject* object = backingObject->highestEditableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:@"AXIsOnScreen"])
        return [NSNumber numberWithBool:backingObject->isOnScreen()];

    if ([attributeName isEqualToString:@"AXIsIndeterminate"])
        return [NSNumber numberWithBool: backingObject->isIndeterminate()];

    if ([attributeName isEqualToString:NSAccessibilityTextInputMarkedRangeAttribute]) {
        auto range = backingObject->textInputMarkedTextMarkerRange();
        auto nsRange = range.nsRange();
        return range && nsRange ? [NSValue valueWithRange:*nsRange] : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityTextInputMarkedTextMarkerRangeAttribute]) {
        auto range = backingObject->textInputMarkedTextMarkerRange();
        return range ? range.platformData().bridgingAutorelease() : nil;
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

    auto focusedObject = backingObject->focusedUIElement();
    return focusedObject ? focusedObject->wrapper() : nil;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    backingObject->updateChildrenIfNecessary();
    auto* axObject = backingObject->accessibilityHitTest(IntPoint(point));

    id hit = nil;
    if (axObject) {
        if (axObject->isAttachment()) {
            if (id attachmentView = [axObject->wrapper() attachmentView])
                return attachmentView;
        }

        // Only call out to the main-thread if this object has a backing widget to query.
        if (axObject->isWidget()) {
            hit = Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&axObject, &point] () -> RetainPtr<id> {
                auto* widget = axObject->widget();
                if (is<PluginViewBase>(widget))
                    return widget->accessibilityHitTest(IntPoint(point));
                return nil;
            });
        }

        if (!hit)
            hit = axObject->wrapper();
    } else
        hit = self;

    return NSAccessibilityUnignoredAncestor(hit);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NO;

    if ([attributeName isEqualToString:AXSelectedTextMarkerRangeAttribute])
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

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsIgnored
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return YES;

    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityIsIgnored];
    return backingObject->accessibilityIsIgnored();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    if (backingObject->isAttachment())
        return nil;

    static NSArray *paramAttrs;
    static NSArray *textParamAttrs;
    static NSArray *tableParamAttrs;
    static NSArray *webAreaParamAttrs;
    if (paramAttrs == nil) {
        paramAttrs = [[NSArray alloc] initWithObjects:
            AXUIElementForTextMarkerAttribute,
            AXTextMarkerRangeForUIElementAttribute,
            AXLineForTextMarkerAttribute,
            AXTextMarkerRangeForLineAttribute,
            AXStringForTextMarkerRangeAttribute,
            AXTextMarkerForPositionAttribute,
            AXBoundsForTextMarkerRangeAttribute,
            AXAttributedStringForTextMarkerRangeAttribute,
            AXAttributedStringForTextMarkerRangeWithOptionsAttribute,
            AXTextMarkerRangeForTextMarkersAttribute,
            AXTextMarkerRangeForUnorderedTextMarkersAttribute,
            AXNextTextMarkerForTextMarkerAttribute,
            AXPreviousTextMarkerForTextMarkerAttribute,
            AXLeftWordTextMarkerRangeForTextMarkerAttribute,
            AXRightWordTextMarkerRangeForTextMarkerAttribute,
            AXLeftLineTextMarkerRangeForTextMarkerAttribute,
            AXRightLineTextMarkerRangeForTextMarkerAttribute,
            AXSentenceTextMarkerRangeForTextMarkerAttribute,
            AXParagraphTextMarkerRangeForTextMarkerAttribute,
            AXNextWordEndTextMarkerForTextMarkerAttribute,
            AXPreviousWordStartTextMarkerForTextMarkerAttribute,
            AXNextLineEndTextMarkerForTextMarkerAttribute,
            AXPreviousLineStartTextMarkerForTextMarkerAttribute,
            AXNextSentenceEndTextMarkerForTextMarkerAttribute,
            AXPreviousSentenceStartTextMarkerForTextMarkerAttribute,
            AXNextParagraphEndTextMarkerForTextMarkerAttribute,
            AXPreviousParagraphStartTextMarkerForTextMarkerAttribute,
            AXStyleTextMarkerRangeForTextMarkerAttribute,
            AXLengthForTextMarkerRangeAttribute,
            NSAccessibilityBoundsForRangeParameterizedAttribute,
            NSAccessibilityStringForRangeParameterizedAttribute,
            NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute,
            NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
            AXEndTextMarkerForBoundsAttribute,
            AXStartTextMarkerForBoundsAttribute,
            AXLineTextMarkerRangeForTextMarkerAttribute,
            NSAccessibilitySelectTextWithCriteriaParameterizedAttribute,
            NSAccessibilitySearchTextWithCriteriaParameterizedAttribute,
            NSAccessibilityTextOperationParameterizedAttribute,
            nil];
    }

    if (textParamAttrs == nil) {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:paramAttrs]);
        [tempArray addObject:(NSString*)kAXLineForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForLineParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForPositionParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXBoundsForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRTFForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXAttributedStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStyleRangeForIndexParameterizedAttribute];
        textParamAttrs = [[NSArray alloc] initWithArray:tempArray.get()];
    }
    if (tableParamAttrs == nil) {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:paramAttrs]);
        [tempArray addObject:NSAccessibilityCellForColumnAndRowParameterizedAttribute];
        tableParamAttrs = [[NSArray alloc] initWithArray:tempArray.get()];
    }
    if (!webAreaParamAttrs) {
        auto tempArray = adoptNS([[NSMutableArray alloc] initWithArray:paramAttrs]);
        [tempArray addObject:AXTextMarkerForIndexAttribute];
        [tempArray addObject:AXTextMarkerIsValidAttribute];
        [tempArray addObject:AXIndexForTextMarkerAttribute];
        webAreaParamAttrs = [[NSArray alloc] initWithArray:tempArray.get()];
    }

    if (backingObject->isSecureField())
        return @[ NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute ];

    if (backingObject->isTextControl())
        return textParamAttrs;

    if (backingObject->isTable() && backingObject->isExposable())
        return tableParamAttrs;

    if (backingObject->isMenuRelated())
        return nil;

    if (backingObject->isWebArea())
        return webAreaParamAttrs;

    return paramAttrs;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

- (void)accessibilityPerformPressAction
{
    // In case anything we do by performing the press action causes an alert or other modal
    // behaviors, we need to return now, so that VoiceOver doesn't hang indefinitely.
    RunLoop::main().dispatch([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityPerformPressAction];
    });
}

- (void)_accessibilityPerformPressAction
{
    ASSERT(isMainThread());
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if (backingObject->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityPressAction];
    else
        backingObject->press();
}

- (void)accessibilityPerformIncrementAction
{
    RunLoop::main().dispatch([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityPerformIncrementAction];
    });
}

- (void)_accessibilityPerformIncrementAction
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if (backingObject->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityIncrementAction];
    else
        backingObject->increment();
}

- (void)accessibilityPerformDecrementAction
{
    RunLoop::main().dispatch([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityPerformDecrementAction];
    });
}

- (void)_accessibilityPerformDecrementAction
{
    auto* backingObject = self.updateObjectBackingStore;
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

    auto* backingObject = self.axBackingObject;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper ", hex(reinterpret_cast<uintptr_t>(self))));
        return;
    }

    if (backingObject->roleValue() == AccessibilityRole::ComboBox) {
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

    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper ", hex(reinterpret_cast<uintptr_t>(self))));
        return;
    }

    Page* page = backingObject->page();
    if (!page)
        return;

    IntRect rect = snappedIntRect(backingObject->elementRect());
    // On WK2, we need to account for the scroll position with regards to root view.
    // On WK1, we need to convert rect to window space to match mouse clicking.
    auto* frameView = backingObject->documentFrameView();
    if (frameView) {
        // Find the appropriate scroll view to convert the coordinates to window space.
        auto* axScrollView = Accessibility::findAncestor(*backingObject, false, [] (const auto& ancestor) {
            return ancestor.isScrollView() && ancestor.scrollView();
        });
        if (axScrollView) {
            if (!frameView->platformWidget())
                rect = axScrollView->scrollView()->contentsToRootView(rect);
            else
                rect = axScrollView->scrollView()->contentsToWindow(rect);
        }
    }

    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame()))
        page->contextMenuController().showContextMenuAt(*localMainFrame, rect.center());
}

- (void)accessibilityScrollToVisible
{
    self.axBackingObject->scrollToMakeVisible();
}

- (void)_accessibilityScrollToMakeVisibleWithSubFocus:(NSRect)rect
{
    self.axBackingObject->scrollToMakeVisibleWithSubFocus(IntRect(rect));
}

- (void)_accessibilityScrollToGlobalPoint:(NSPoint)point
{
    self.axBackingObject->scrollToGlobalPoint(IntPoint(point));
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilityPerformAction:(NSString*)action
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    if ([action isEqualToString:NSAccessibilityPressAction])
        [self accessibilityPerformPressAction];
    else if ([action isEqualToString:@"AXSyncPressAction"]) {
        // Used in layout tests, so that we don't have to wait for the async press action.
        [self _accessibilityPerformPressAction];
    }
    else if ([action isEqualToString:@"AXSyncIncrementAction"])
        [self _accessibilityPerformIncrementAction];
    else if ([action isEqualToString:@"AXSyncDecrementAction"])
        [self _accessibilityPerformDecrementAction];
    else if ([action isEqualToString:NSAccessibilityShowMenuAction])
        [self accessibilityPerformShowMenuAction];
    else if ([action isEqualToString:NSAccessibilityIncrementAction])
        [self accessibilityPerformIncrementAction];
    else if ([action isEqualToString:NSAccessibilityDecrementAction])
        [self accessibilityPerformDecrementAction];
    else if ([action isEqualToString:NSAccessibilityScrollToVisibleAction])
        [self accessibilityScrollToVisible];
    else if ([action isEqualToString:@"AXDismissAction"])
        backingObject->performDismissActionIgnoringResult();
}

- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string
{
    auto* backingObject = self.updateObjectBackingStore;
    return backingObject ? backingObject->replaceTextInRange(String(string), range) : NO;
}

- (BOOL)accessibilityInsertText:(NSString *)text
{
    auto* backingObject = self.updateObjectBackingStore;
    return backingObject ? backingObject->insertText(String(text)) : NO;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
#if PLATFORM(MAC)
    // In case anything we do by changing values causes an alert or other modal
    // behaviors, we need to return now, so that VoiceOver doesn't hang indefinitely.
    callOnMainThread([value = retainPtr(value), attributeName = retainPtr(attributeName), protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilitySetValue:value.get() forAttribute:attributeName.get()];
    });
#else
    // dispatch_async on earlier versions can cause focus not to track.
    [self _accessibilitySetValue:value forAttribute:attributeName];
#endif
}

- (void)_accessibilitySetValue:(id)value forAttribute:(NSString *)attributeName
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper _accessibilitySetValue: forAttribute:", String(attributeName)));

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject) {
        AXLOG(makeString("No backingObject for wrapper ", hex(reinterpret_cast<uintptr_t>(self))));
        return;
    }

    AXTextMarkerRangeRef textMarkerRange = nil;
    NSNumber*               number = nil;
    NSString*               string = nil;
    NSRange                 range = {0, 0};
    NSArray*                array = nil;

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
    if ([attributeName isEqualToString:AXSelectedTextMarkerRangeAttribute]) {
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
        if (backingObject->isTree() || (backingObject->isTable() && backingObject->isExposable()))
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
    
    Node* node = frame->document()->ownerElement();
    if (!node)
        return nullptr;
    
    return node->renderer();
}

- (id)_accessibilityParentForSubview:(NSView*)subview
{
    RenderObject* renderer = rendererForView(subview);
    if (!renderer)
        return nil;
    
    AccessibilityObject* obj = renderer->document().axObjectCache()->getOrCreate(renderer);
    if (obj)
        return obj->parentObjectUnignored()->wrapper();
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSString*)accessibilityActionDescription:(NSString*)action
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}

// The CFAttributedStringType representation of the text associated with this accessibility
// object that is specified by the given range.
- (NSAttributedString *)attributedStringForNSRange:(const NSRange&)range
{
    if (!range.length)
        return nil;

    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    auto attributedString = backingObject->attributedStringForTextMarkerRange(backingObject->textMarkerRangeForNSRange(range), AXCoreObject::SpellCheck::Yes);
    return [attributedString length] ? attributedString.autorelease() : nil;
}

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
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        auto* document = backingObject->document();
        if (!document)
            return nil;

        auto* documentElement = document->documentElement();
        if (!documentElement)
            return nil;

        auto boundary = resolveCharacterLocation(makeRangeSelectingNodeContents(*documentElement), textIndex);
        auto characterOffset = cache->startOrEndCharacterOffsetForRange(makeSimpleRange(boundary), true);

        return textMarkerForCharacterOffset(cache, characterOffset);
    });
}

// The RTF representation of the text associated with this accessibility object that is
// specified by the given range.
- (NSData *)rtfForNSRange:(const NSRange&)range
{
    NSAttributedString *attrString = [self attributedStringForNSRange:range];
    return [attrString RTFFromRange:NSMakeRange(0, attrString.length) documentAttributes:@{ }];
}

#if ENABLE(TREE_DEBUGGING)
- (NSString *)debugDescriptionForTextMarker:(AXTextMarkerRef)textMarker
{
    return visiblePositionForTextMarker(self.axBackingObject->axObjectCache(), textMarker).debugDescription();
}

- (NSString *)debugDescriptionForTextMarkerRange:(AXTextMarkerRangeRef)textMarkerRange
{
    RefPtr<AXCoreObject> backingObject = self.axBackingObject;
    if (!backingObject)
        return @"<null>";

    auto visiblePositionRange = visiblePositionRangeForTextMarkerRange(backingObject->axObjectCache(), textMarkerRange);
    if (visiblePositionRange.isNull())
        return @"<null>";

    char description[2048];
    formatForDebugger(visiblePositionRange, description, sizeof(description));

    return [NSString stringWithUTF8String:description];
}

- (void)showNodeForTextMarker:(AXTextMarkerRef)textMarker
{
    auto visiblePosition = visiblePositionForTextMarker(self.axBackingObject->axObjectCache(), textMarker);
    auto node = visiblePosition.deepEquivalent().protectedDeprecatedNode();
    if (!node)
        return;
    node->showNode();
    node->showNodePathForThis();
}

- (void)showNodeTreeForTextMarker:(AXTextMarkerRef)textMarker
{
    auto visiblePosition = visiblePositionForTextMarker(self.axBackingObject->axObjectCache(), textMarker);
    auto node = visiblePosition.deepEquivalent().protectedDeprecatedNode();
    if (!node)
        return;
    node->showTreeForThis();
}

static void formatForDebugger(const VisiblePositionRange& range, char* buffer, unsigned length)
{
    strlcpy(buffer, makeString("from ", range.start.debugDescription(), " to ", range.end.debugDescription()).utf8().data(), length);
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
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([textMarker = retainPtr(textMarker), &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRangeRef> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* cache = backingObject->axObjectCache();
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

        return textMarkerRangeFromRange(cache, range);
    });
}

- (AXTextMarkerRangeRef)lineTextMarkerRangeForTextMarker:(AXTextMarkerRef)textMarker forUnit:(TextUnit)textUnit
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([textMarker = retainPtr(textMarker), &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRangeRef> {
        auto* backingObject = protectedSelf.get().axBackingObject;
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
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRef>([textMarkerRef = retainPtr(textMarkerRef), &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRef> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* cache = backingObject->axObjectCache();
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
            return textMarkerForVisiblePosition(cache, backingObject->nextLineEndPosition(oldMarker));
        case TextUnit::PreviousLineStart:
            return textMarkerForVisiblePosition(cache, backingObject->previousLineStartPosition(oldMarker));
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return newMarker.platformData();
    });
}

static bool isMatchingPlugin(AXCoreObject* axObject, const AccessibilitySearchCriteria& criteria)
{
    if (!axObject->isWidget())
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&axObject, &criteria] () -> bool {
        auto* widget = axObject->widget();
        if (!is<PluginViewBase>(widget))
            return false;

        return criteria.searchKeys.contains(AccessibilitySearchKey::AnyType)
            && (!criteria.visibleOnly || downcast<PluginViewBase>(widget)->isVisible());
    });
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityAttributeValue:", String(attribute)));
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
    NSRange range = {0, 0};
    bool rangeSet = false;
    NSRect rect = NSZeroRect;

    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
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
    }
    else if ([parameter isKindOfClass:[NSNumber class]])
        number = parameter;
    else if ([parameter isKindOfClass:[NSArray class]])
        array = parameter;
    else if ([parameter isKindOfClass:[NSDictionary class]])
        dictionary = parameter;
    else if ([parameter isKindOfClass:[NSValue class]] && !strcmp([(NSValue*)parameter objCType], @encode(NSPoint))) {
        pointSet = true;
        point = [(NSValue*)parameter pointValue];
    } else if ([parameter isKindOfClass:[NSValue class]] && !strcmp([(NSValue*)parameter objCType], @encode(NSRange))) {
        rangeSet = true;
        range = [(NSValue*)parameter rangeValue];
    } else if ([parameter isKindOfClass:[NSValue class]] && !strcmp([(NSValue*)parameter objCType], @encode(NSRect)))
        rect = [(NSValue*)parameter rectValue];
    else {
        // Attribute type is not supported. Allow super to handle.
        return [super accessibilityAttributeValue:attribute forParameter:parameter];
    }

    // dispatch
    if ([attribute isEqualToString:NSAccessibilitySelectTextWithCriteriaParameterizedAttribute]) {
        // To be deprecated.
        auto result = Accessibility::retrieveValueFromMainThread<Vector<String>>([dictionary, protectedSelf = retainPtr(self)] () -> Vector<String> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return Vector<String>();

            auto criteria = accessibilityTextCriteriaForParameterizedAttribute(dictionary);
            criteria.second.textRanges = backingObject->findTextRanges(criteria.first);
            ASSERT(criteria.second.textRanges.size() <= 1);
            return backingObject->performTextOperation(criteria.second);
        });
        ASSERT(result.size() <= 1);
        if (result.size() > 0)
            return result[0];
        return String();
    }

    if ([attribute isEqualToString:NSAccessibilitySearchTextWithCriteriaParameterizedAttribute]) {
        auto criteria = accessibilitySearchTextCriteriaForParameterizedAttribute(dictionary);
        return Accessibility::retrieveAutoreleasedValueFromMainThread<NSArray *>([&criteria, protectedSelf = retainPtr(self)] () -> RetainPtr<NSArray> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;
            auto ranges = backingObject->findTextRanges(criteria);
            if (ranges.isEmpty())
                return nil;
            return createNSArray(WTFMove(ranges), [&] (SimpleRange&& range) {
                return (id)textMarkerRangeFromRange(backingObject->axObjectCache(), WTFMove(range));
            }).autorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityTextOperationParameterizedAttribute]) {
        auto operationResult = Accessibility::retrieveValueFromMainThread<Vector<String>>([dictionary, protectedSelf = retainPtr(self)] () -> Vector<String> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return Vector<String>();

            auto textOperation = accessibilityTextOperationForParameterizedAttribute(backingObject->axObjectCache(), dictionary);
            return backingObject->performTextOperation(textOperation);
        });
        if (operationResult.isEmpty())
            return nil;
        return createNSArray(operationResult).autorelease();
    }

    if ([attribute isEqualToString:NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute]) {
        AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(dictionary);
        NSUInteger widgetChildrenSize = 0;
        if (isMatchingPlugin(backingObject.get(), criteria)) {
            // FIXME: We should also be searching the tree(s) resulting from `renderWidgetChildren` for matches.
            // This is tracked by https://bugs.webkit.org/show_bug.cgi?id=230167.
            if (auto* widgetChildren = [self renderWidgetChildren]) {
                widgetChildrenSize = [widgetChildren count];
                if (widgetChildrenSize >= criteria.resultsLimit)
                    return @(std::min(widgetChildrenSize, NSUInteger(criteria.resultsLimit)));
                criteria.resultsLimit -= widgetChildrenSize;
            }
        }

        AccessibilityObject::AccessibilityChildrenVector results;
        backingObject->findMatchingObjects(&criteria, results);
        return @(results.size() + widgetChildrenSize);
    }

    if ([attribute isEqualToString:NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute]) {
        AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(dictionary);
        NSArray *widgetChildren = nil;
        if (isMatchingPlugin(backingObject.get(), criteria)) {
            // FIXME: We should also be searching the tree(s) resulting from `renderWidgetChildren` for matches.
            // This is tracked by https://bugs.webkit.org/show_bug.cgi?id=230167.
            if (auto* children = [self renderWidgetChildren]) {
                NSUInteger includedChildrenCount = std::min([children count], NSUInteger(criteria.resultsLimit));
                widgetChildren = [children subarrayWithRange:NSMakeRange(0, includedChildrenCount)];
                if ([widgetChildren count] >= criteria.resultsLimit)
                    return widgetChildren;
                criteria.resultsLimit -= [widgetChildren count];
            }
        }

        AccessibilityObject::AccessibilityChildrenVector results;
        backingObject->findMatchingObjects(&criteria, results);
        if (widgetChildren)
            return [widgetChildren arrayByAddingObjectsFromArray:makeNSArray(results)];
        return makeNSArray(results);
    }

    // TextMarker attributes.

    if ([attribute isEqualToString:AXEndTextMarkerForBoundsAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&rect, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            IntRect webCoreRect = [protectedSelf screenToContents:enclosingIntRect(rect)];
            CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, false);

            return (id)textMarkerForCharacterOffset(cache, characterOffset);
        });
    }

    if ([attribute isEqualToString:AXStartTextMarkerForBoundsAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&rect, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            IntRect webCoreRect = [protectedSelf screenToContents:enclosingIntRect(rect)];
            CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, true);

            return (id)textMarkerForCharacterOffset(cache, characterOffset);
        });
    }

    // TextMarkerRange attributes.
    if ([attribute isEqualToString:AXTextMarkerRangeForNSRangeAttribute])
        return backingObject->textMarkerRangeForNSRange(range).platformData().bridgingAutorelease();

    if ([attribute isEqualToString:AXLineTextMarkerRangeForTextMarkerAttribute])
        return (id)[self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::Line];

    if ([attribute isEqualToString:AXMisspellingTextMarkerRangeAttribute]) {
        return (id)Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([&dictionary, protectedSelf = retainPtr(self)] () -> RetainPtr<AXTextMarkerRangeRef> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto criteria = misspellingSearchCriteriaForParameterizedAttribute(dictionary);
            if (!criteria.first)
                return nil;

            auto range = criteria.first.simpleRange();
            if (!range)
                return nil;

            if (auto misspellRange = backingObject->misspellingRange(*range, criteria.second))
                return AXTextMarkerRange(*misspellRange).platformData();
            return nil;
        });
    }

    if ([attribute isEqualToString:AXTextMarkerIsValidAttribute])
        return [NSNumber numberWithBool:AXTextMarker(textMarker).isValid()];

    if ([attribute isEqualToString:AXTextMarkerIsNullAttribute])
        return [NSNumber numberWithBool:AXTextMarker(textMarker).isNull()];

    if ([attribute isEqualToString:AXIndexForTextMarkerAttribute])
        return [NSNumber numberWithInteger:[self _indexForTextMarker:textMarker]];

    if ([attribute isEqualToString:AXTextMarkerForIndexAttribute])
        return (id)[self _textMarkerForIndex:[number integerValue]];

    if ([attribute isEqualToString:AXUIElementForTextMarkerAttribute]) {
        AXTextMarker marker { textMarker };
        RefPtr object = marker.object();
        if (!object)
            return nil;

        auto* wrapper = object->wrapper();
        if (!wrapper)
            return nil;

        if (object->isAttachment()) {
            if (id attachmentView = wrapper.attachmentView)
                return attachmentView;
        }
        return wrapper;
    }

    if ([attribute isEqualToString:AXTextMarkerRangeForUIElementAttribute]) {
        if (uiElement) {
            if (auto markerRange = uiElement->textMarkerRange())
                return markerRange.platformData().bridgingAutorelease();
        }
        return nil;
    }

    if ([attribute isEqualToString:AXLineForTextMarkerAttribute]) {
        int result = Accessibility::retrieveValueFromMainThread<int>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> int {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return -1;
            return backingObject->lineForPosition(AXTextMarker { textMarker.get() });
        });
        return @(result);
    }

    if ([attribute isEqualToString:AXTextMarkerRangeForLineAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&number, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            VisiblePositionRange vpRange;
            if ([number unsignedIntegerValue] != NSNotFound)
                vpRange = backingObject->visiblePositionRangeForLine([number unsignedIntValue]);

            return (id)textMarkerRangeFromVisiblePositions(backingObject->axObjectCache(), vpRange.start, vpRange.end);
        });
    }

    if ([attribute isEqualToString:AXStringForTextMarkerRangeAttribute]) {
        return Accessibility::retrieveValueFromMainThread<String>([textMarkerRange = retainPtr(textMarkerRange), protectedSelf = retainPtr(self)] () -> String {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return String();

            AXTextMarkerRange markerRange { textMarkerRange.get() };
            auto range = markerRange.simpleRange();
            return range ? backingObject->stringForRange(*range) : String();
        });
    }

    if ([attribute isEqualToString:AXTextMarkerForPositionAttribute]) {
        if (!pointSet)
            return nil;
        IntPoint webCorePoint = IntPoint(point);

        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&webCorePoint, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            return AXTextMarker(backingObject->visiblePositionForPoint(webCorePoint)).platformData().bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:AXBoundsForTextMarkerRangeAttribute]) {
        NSRect rect = Accessibility::retrieveValueFromMainThread<NSRect>([textMarkerRange = retainPtr(textMarkerRange), protectedSelf = retainPtr(self)] () -> NSRect {
            auto* backingObject = protectedSelf.get().axBackingObject;
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
        NSRect rect = Accessibility::retrieveValueFromMainThread<NSRect>([&range, protectedSelf = retainPtr(self)] () -> NSRect {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return CGRectZero;

            auto start = backingObject->visiblePositionForIndex(range.location);
            auto end = backingObject->visiblePositionForIndex(range.location + range.length);
            auto webRange = makeSimpleRange({ start, end });
            if (!webRange)
                return CGRectZero;

            auto bounds = FloatRect(backingObject->boundsForRange(*webRange));
            return [protectedSelf convertRectToSpace:bounds space:AccessibilityConversionSpace::Screen];
        });
        return [NSValue valueWithRect:rect];
    }

    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute]) {
        if (backingObject->isTextControl())
            return backingObject->doAXStringForRange(range);

        return Accessibility::retrieveValueFromMainThread<String>([&range, protectedSelf = retainPtr(self)] () -> String {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return String();
            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return String();

            auto start = cache->characterOffsetForIndex(range.location, backingObject);
            auto end = cache->characterOffsetForIndex(range.location + range.length, backingObject);
            auto range = cache->rangeForUnorderedCharacterOffsets(start, end);
            if (!range)
                return { };
            return backingObject->stringForRange(*range);
        });
    }

    if ([attribute isEqualToString:AXAttributedStringForTextMarkerRangeAttribute])
        return [self attributedStringForTextMarkerRange:textMarkerRange spellCheck:AXCoreObject::SpellCheck::Yes];

    if ([attribute isEqualToString:AXAttributedStringForTextMarkerRangeWithOptionsAttribute]) {
        if (textMarkerRange)
            return [self attributedStringForTextMarkerRange:textMarkerRange spellCheck:AXCoreObject::SpellCheck::No];

        if (dictionary) {
            AXTextMarkerRangeRef textMarkerRange = nil;
            id parameter = [dictionary objectForKey:@"AXTextMarkerRange"];
            if (AXObjectIsTextMarkerRange(parameter))
                textMarkerRange = (AXTextMarkerRangeRef)parameter;

            auto spellCheck = AXCoreObject::SpellCheck::No;
            parameter = [dictionary objectForKey:@"AXSpellCheck"];
            if ([parameter isKindOfClass:[NSNumber class]] && [parameter boolValue])
                spellCheck = AXCoreObject::SpellCheck::Yes;
            return [self attributedStringForTextMarkerRange:textMarkerRange spellCheck:spellCheck];
        }

        return nil;
    }

    if ([attribute isEqualToString:AXTextMarkerRangeForTextMarkersAttribute]
        || [attribute isEqualToString:AXTextMarkerRangeForUnorderedTextMarkersAttribute]) {
        if (array.count < 2
            || !AXObjectIsTextMarker([array objectAtIndex:0])
            || !AXObjectIsTextMarker([array objectAtIndex:1]))
            return nil;

        return AXTextMarkerRange { { (AXTextMarkerRef)[array objectAtIndex:0] }, { (AXTextMarkerRef)[array objectAtIndex:1] } }.platformData().bridgingAutorelease();
    }

    if ([attribute isEqualToString:AXNextTextMarkerForTextMarkerAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;
            return nextTextMarker(cache, AXTextMarker { textMarker.get() }).bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:AXPreviousTextMarkerForTextMarkerAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;
            return previousTextMarker(cache, AXTextMarker { textMarker.get() }).bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:AXLeftWordTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::LeftWord];

    if ([attribute isEqualToString:AXRightWordTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::RightWord];

    if ([attribute isEqualToString:AXLeftLineTextMarkerRangeForTextMarkerAttribute])
        return (id)[self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::LeftLine];

    if ([attribute isEqualToString:AXRightLineTextMarkerRangeForTextMarkerAttribute])
        return (id)[self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::RightLine];

    if ([attribute isEqualToString:AXSentenceTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::Sentence];

    if ([attribute isEqualToString:AXParagraphTextMarkerRangeForTextMarkerAttribute])
        return (id)[self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::Paragraph];

    if ([attribute isEqualToString:AXNextWordEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextWordEnd];

    if ([attribute isEqualToString:AXPreviousWordStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousWordStart];

    if ([attribute isEqualToString:AXNextLineEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextLineEnd];

    if ([attribute isEqualToString:AXPreviousLineStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousLineStart];

    if ([attribute isEqualToString:AXNextSentenceEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextSentenceEnd];

    if ([attribute isEqualToString:AXPreviousSentenceStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousSentenceStart];

    if ([attribute isEqualToString:AXNextParagraphEndTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextParagraphEnd];

    if ([attribute isEqualToString:AXPreviousParagraphStartTextMarkerForTextMarkerAttribute])
        return (id)[self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousParagraphStart];

    if ([attribute isEqualToString:AXStyleTextMarkerRangeForTextMarkerAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([textMarker = retainPtr(textMarker), protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;
            return AXTextMarkerRange { backingObject->styleRangeForPosition(AXTextMarker { textMarker.get() }) }.platformData().bridgingAutorelease();
        });
    }

    if ([attribute isEqualToString:AXLengthForTextMarkerRangeAttribute]) {
        unsigned length = Accessibility::retrieveValueFromMainThread<unsigned>([textMarkerRange = retainPtr(textMarkerRange), protectedSelf = retainPtr(self)] () -> unsigned {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return 0;

            AXTextMarkerRange markerRange { textMarkerRange.get() };
            auto range = markerRange.simpleRange();
            return range ? AXObjectCache::lengthForRange(*range) : 0;
        });
        return @(length);
    }

    // Used only by DumpRenderTree (so far).
    if ([attribute isEqualToString:AXStartTextMarkerForTextMarkerRangeAttribute]) {
        AXTextMarkerRange markerRange { textMarkerRange };
        return markerRange.start().platformData().bridgingAutorelease();
    }

    if ([attribute isEqualToString:AXEndTextMarkerForTextMarkerRangeAttribute]) {
        AXTextMarkerRange markerRange { textMarkerRange };
        return markerRange.end().platformData().bridgingAutorelease();
    }

#if ENABLE(TREE_DEBUGGING)
    if ([attribute isEqualToString:AXTextMarkerDebugDescriptionAttribute])
        return [self debugDescriptionForTextMarker:textMarker];

    if ([attribute isEqualToString:AXTextMarkerRangeDebugDescriptionAttribute])
        return [self debugDescriptionForTextMarkerRange:textMarkerRange];

    if ([attribute isEqualToString:AXTextMarkerNodeDebugDescriptionAttribute]) {
        [self showNodeForTextMarker:textMarker];
        return nil;
    }

    if ([attribute isEqualToString:AXTextMarkerNodeTreeDebugDescriptionAttribute]) {
        [self showNodeTreeForTextMarker:textMarker];
        return nil;
    }
#endif

    if (backingObject->isTable() && backingObject->isExposable()) {
        if ([attribute isEqualToString:NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
            if (array == nil || [array count] != 2)
                return nil;
            auto* cell = backingObject->cellForColumnAndRow([[array objectAtIndex:0] unsignedIntValue], [[array objectAtIndex:1] unsignedIntValue]);
            return cell ? cell->wrapper() : nil;
        }
    }

    if (backingObject->isTextControl()) {
        if ([attribute isEqualToString: (NSString *)kAXLineForIndexParameterizedAttribute]) {
            int lineNumber = backingObject->doAXLineForIndex([number intValue]);
            if (lineNumber < 0)
                return nil;
            return @(lineNumber);
        }

        if ([attribute isEqualToString:(NSString *)kAXRangeForLineParameterizedAttribute]) {
            auto textRange = backingObject->doAXRangeForLine([number intValue]);
            return [NSValue valueWithRange:textRange];
        }

        if ([attribute isEqualToString:(NSString *)kAXStringForRangeParameterizedAttribute])
            return rangeSet ? (id)backingObject->doAXStringForRange(range) : nil;

        if ([attribute isEqualToString:(NSString *)kAXRangeForPositionParameterizedAttribute]) {
            if (!pointSet)
                return nil;

            auto webCorePoint = IntPoint(point);
            auto textRange = backingObject->characterRangeForPoint(webCorePoint);
            return [NSValue valueWithRange:textRange];
        }

        if ([attribute isEqualToString:(NSString *)kAXRangeForIndexParameterizedAttribute]) {
            auto textRange = backingObject->doAXRangeForIndex([number intValue]);
            return [NSValue valueWithRange:textRange];
        }

        if ([attribute isEqualToString:(NSString *)kAXBoundsForRangeParameterizedAttribute]) {
            if (!rangeSet)
                return nil;

            auto bounds = FloatRect(backingObject->doAXBoundsForRangeUsingCharacterOffset(range));
            NSRect rect = [self convertRectToSpace:bounds space:AccessibilityConversionSpace::Screen];
            return [NSValue valueWithRect:rect];
        }

        if ([attribute isEqualToString:(NSString *)kAXRTFForRangeParameterizedAttribute])
            return rangeSet ? [self rtfForNSRange:range] : nil;

        if ([attribute isEqualToString:(NSString *)kAXAttributedStringForRangeParameterizedAttribute])
            return rangeSet ? [self attributedStringForNSRange:range] : nil;

        if ([attribute isEqualToString:(NSString *)kAXStyleRangeForIndexParameterizedAttribute]) {
            auto textRange = backingObject->doAXStyleRangeForIndex([number intValue]);
            return [NSValue valueWithRange:textRange];
        }
    }

    // There are some parameters that super handles that are not explicitly returned by the list of the element's attributes.
    // In that case it must be passed to super.
    return [super accessibilityAttributeValue:attribute forParameter:parameter];
}

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

    const auto& children = backingObject->children();
    if (!children.size()) {
        if (auto *renderWidgetChildren = [self renderWidgetChildren])
            return [renderWidgetChildren indexOfObject:targetChild];
#if ENABLE(MODEL_ELEMENT)
        if (backingObject->isModel())
            return backingObject->modelElementChildren().find(targetChild);
#endif
    }

    size_t childCount = children.size();
    for (size_t i = 0; i < childCount; i++) {
        const auto& child = children[i];
        if (!child)
            continue;
        WebAccessibilityObjectWrapper *childWrapper = child->wrapper();
        if (childWrapper == targetChild || (child->isAttachment() && [childWrapper attachmentView] == targetChild))
            return i;
    }
    return NSNotFound;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityArrayAttributeCount:", String(attribute)));

    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return 0;

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        // Tree items object returns a different set of children than those that are in children()
        // because an AXOutline (the mac role is becomes) has some odd stipulations.
        if (backingObject->isTree() || backingObject->isTreeItem())
            return [[self accessibilityAttributeValue:NSAccessibilityChildrenAttribute] count];

        auto childrenSize = self.childrenVectorSize;
        if (!childrenSize) {
#if ENABLE(MODEL_ELEMENT)
            if (backingObject->isModel())
                return backingObject->modelElementChildren().size();
#endif
            if (NSArray *renderWidgetChildren = [self renderWidgetChildren])
                return [renderWidgetChildren count];
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
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityArrayAttributeValue:", String(attribute)));
    RefPtr<AXCoreObject> backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        if (!self.childrenVectorSize) {
            NSArray *children = nil;
#if ENABLE(MODEL_ELEMENT)
            if (backingObject->isModel()) {
                children = createNSArray(backingObject->modelElementChildren(), [] (auto&& child) -> id {
                    return child.get();
                }).autorelease();
            } else
#endif
                children = [self renderWidgetChildren];
            
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
            return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
        }

        auto children = self.childrenVectorArray;
        unsigned childCount = [children count];
        if (index >= childCount)
            return nil;

        unsigned available = std::min(childCount - index, maxCount);

        NSMutableArray *subarray = [NSMutableArray arrayWithCapacity:available];
        for (unsigned added = 0; added < available; ++index, ++added) {
            WebAccessibilityObjectWrapper* wrapper = children[index];

            // The attachment view should be returned, otherwise AX palindrome errors occur.
            id attachmentView = nil;
            if ([wrapper isKindOfClass:[WebAccessibilityObjectWrapper class]] && wrapper.axBackingObject && wrapper.axBackingObject->isAttachment())
                attachmentView = [wrapper attachmentView];
            [subarray addObject:attachmentView ? attachmentView : wrapper];
        }

        return subarray;
    }

    return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
}

@end

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(MAC)
