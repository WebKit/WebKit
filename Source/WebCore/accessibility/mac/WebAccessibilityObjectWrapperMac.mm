/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#import "ColorMac.h"
#import "ContextMenuController.h"
#import "Editing.h"
#import "Editor.h"
#import "Font.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoaderClient.h"
#import "FrameSelection.h"
#import "HTMLAnchorElement.h"
#import "HTMLAreaElement.h"
#import "HTMLFrameOwnerElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
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
#import "TextCheckerClient.h"
#import "TextCheckingHelper.h"
#import "TextDecorationPainter.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import "WebCoreFrameView.h"
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(TREE_DEBUGGING)
#import <wtf/text/StringBuilder.h>
#endif

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

// Lists
#ifndef NSAccessibilityContentListSubrole
#define NSAccessibilityContentListSubrole @"AXContentList"
#endif

#ifndef NSAccessibilityDefinitionListSubrole
#define NSAccessibilityDefinitionListSubrole @"AXDefinitionList"
#endif

#ifndef NSAccessibilityDescriptionListSubrole
#define NSAccessibilityDescriptionListSubrole @"AXDescriptionList"
#endif

#ifndef NSAccessibilityContentSeparatorSubrole
#define NSAccessibilityContentSeparatorSubrole @"AXContentSeparator"
#endif

#ifndef NSAccessibilityRubyBaseSubRole
#define NSAccessibilityRubyBaseSubrole @"AXRubyBase"
#endif

#ifndef NSAccessibilityRubyBlockSubrole
#define NSAccessibilityRubyBlockSubrole @"AXRubyBlock"
#endif

#ifndef NSAccessibilityRubyInlineSubrole
#define NSAccessibilityRubyInlineSubrole @"AXRubyInline"
#endif

#ifndef NSAccessibilityRubyRunSubrole
#define NSAccessibilityRubyRunSubrole @"AXRubyRun"
#endif

#ifndef NSAccessibilityRubyTextSubrole
#define NSAccessibilityRubyTextSubrole @"AXRubyText"
#endif

// Miscellaneous
#ifndef NSAccessibilityBlockQuoteLevelAttribute
#define NSAccessibilityBlockQuoteLevelAttribute @"AXBlockQuoteLevel"
#endif

#ifndef NSAccessibilityChildrenInNavigationOrderAttribute
#define NSAccessibilityChildrenInNavigationOrderAttribute @"AXChildrenInNavigationOrder"
#endif

#ifndef NSAccessibilityAccessKeyAttribute
#define NSAccessibilityAccessKeyAttribute @"AXAccessKey"
#endif

#ifndef NSAccessibilityValueAutofilledAttribute
#define NSAccessibilityValueAutofilledAttribute @"AXValueAutofilled"
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

#define NSAccessibilityTextMarkerIsValidParameterizedAttribute @"AXTextMarkerIsValid"
#define NSAccessibilityIndexForTextMarkerParameterizedAttribute @"AXIndexForTextMarker"
#define NSAccessibilityTextMarkerForIndexParameterizedAttribute @"AXTextMarkerForIndex"

#ifndef NSAccessibilityScrollToVisibleAction
#define NSAccessibilityScrollToVisibleAction @"AXScrollToVisible"
#endif

#ifndef NSAccessibilityPathAttribute
#define NSAccessibilityPathAttribute @"AXPath"
#endif

#ifndef NSAccessibilityExpandedTextValueAttribute
#define NSAccessibilityExpandedTextValueAttribute @"AXExpandedTextValue"
#endif

#ifndef NSAccessibilityIsMultiSelectableAttribute
#define NSAccessibilityIsMultiSelectableAttribute @"AXIsMultiSelectable"
#endif

#ifndef NSAccessibilityDocumentURIAttribute
#define NSAccessibilityDocumentURIAttribute @"AXDocumentURI"
#endif

#ifndef NSAccessibilityDocumentEncodingAttribute
#define NSAccessibilityDocumentEncodingAttribute @"AXDocumentEncoding"
#endif

#ifndef NSAccessibilityAriaControlsAttribute
#define NSAccessibilityAriaControlsAttribute @"AXARIAControls"
#endif

#define NSAccessibilityDOMIdentifierAttribute @"AXDOMIdentifier"
#define NSAccessibilityDOMClassListAttribute @"AXDOMClassList"

#ifndef NSAccessibilityARIACurrentAttribute
#define NSAccessibilityARIACurrentAttribute @"AXARIACurrent"
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

// Text markers
#ifndef NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute
#define NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute @"AXEndTextMarkerForBounds"
#endif

#ifndef NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute
#define NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute @"AXStartTextMarkerForBounds"
#endif

#ifndef NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute
#define NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute @"AXLineTextMarkerRangeForTextMarker"
#endif

#ifndef NSAccessibilityMisspellingTextMarkerRangeParameterizedAttribute
#define NSAccessibilityMisspellingTextMarkerRangeParameterizedAttribute @"AXMisspellingTextMarkerRange"
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

#ifndef NSAccessibilityWebSessionIDAttribute
#define NSAccessibilityWebSessionIDAttribute @"AXWebSessionID"
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

extern "C" AXUIElementRef NSAccessibilityCreateAXUIElementRef(id element);

@implementation WebAccessibilityObjectWrapper

- (void)unregisterUniqueIdForUIElement
{
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
}

- (void)detach
{
    // Send unregisterUniqueIdForUIElement unconditionally because if it is
    // ever accidentally not done (via other bugs in our AX implementation) you
    // end up with a crash like <rdar://problem/4273149>.  It is safe and not
    // expensive to send even if the object is not registered.
    [self unregisterUniqueIdForUIElement];
    [super detach];
}

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

static id AXTextMarkerRangeStart(id range)
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID((__bridge CFTypeRef)range) == AXTextMarkerRangeGetTypeID());
    return CFBridgingRelease(AXTextMarkerRangeCopyStartMarker((AXTextMarkerRangeRef)range));
}

static id AXTextMarkerRangeEnd(id range)
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID((__bridge CFTypeRef)range) == AXTextMarkerRangeGetTypeID());
    return CFBridgingRelease(AXTextMarkerRangeCopyEndMarker((AXTextMarkerRangeRef)range));
}

#pragma mark Other helpers

- (IntRect)screenToContents:(const IntRect&)rect
{
    ASSERT(isMainThread());

    Document* document = self.axBackingObject->document();
    if (!document)
        return IntRect();
    
    FrameView* frameView = document->view();
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

static AccessibilityTextOperation accessibilityTextOperationForParameterizedAttribute(WebAccessibilityObjectWrapper *wrapper, const NSDictionary *parameterizedAttribute)
{
    AccessibilityTextOperation operation;

    NSArray *markerRanges = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationMarkerRanges];
    NSString *operationType = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationType];
    NSString *replacementString = [parameterizedAttribute objectForKey:NSAccessibilityTextOperationReplacementString];

    if ([markerRanges isKindOfClass:[NSArray class]]) {
        operation.textRanges = makeVector(markerRanges, [wrapper] (id markerRange) {
            return [wrapper rangeForTextMarkerRange:markerRange];
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

static std::pair<Optional<SimpleRange>, AccessibilitySearchDirection> accessibilityMisspellingSearchCriteriaForParameterizedAttribute(WebAccessibilityObjectWrapper *object, const NSDictionary *params)
{
    std::pair<Optional<SimpleRange>, AccessibilitySearchDirection> criteria;

    criteria.first = [object rangeForTextMarkerRange:[params objectForKey:@"AXStartTextMarkerRange"]];

    NSNumber *forward = [params objectForKey:NSAccessibilitySearchTextDirection];
    if ([forward isKindOfClass:[NSNumber class]])
        criteria.second = [forward boolValue] ? AccessibilitySearchDirection::Next : AccessibilitySearchDirection::Previous;
    else
        criteria.second = AccessibilitySearchDirection::Next;

    return criteria;
}

#pragma mark Text Marker helpers

static BOOL getBytesFromAXTextMarker(CFTypeRef textMarker, void* bytes, size_t length)
{
    if (!textMarker)
        return NO;

    AXTextMarkerRef ref = (AXTextMarkerRef)textMarker;
    ASSERT(CFGetTypeID(ref) == AXTextMarkerGetTypeID());
    if (CFGetTypeID(ref) != AXTextMarkerGetTypeID())
        return NO;

    CFIndex expectedLength = length;
    if (AXTextMarkerGetLength(ref) != expectedLength)
        return NO;

    memcpy(bytes, AXTextMarkerGetBytePtr(ref), length);
    return YES;
}

static bool isTextMarkerIgnored(id textMarker)
{
    if (!textMarker)
        return false;
    
    TextMarkerData textMarkerData;
    if (!getBytesFromAXTextMarker((__bridge CFTypeRef)textMarker, &textMarkerData, sizeof(textMarkerData)))
        return false;
    
    return textMarkerData.ignored;
}

- (AccessibilityObject*)accessibilityObjectForTextMarker:(id)textMarker
{
    return accessibilityObjectForTextMarker(self.axBackingObject->axObjectCache(), textMarker);
}

static AccessibilityObject* accessibilityObjectForTextMarker(AXObjectCache* cache, id textMarker)
{
    if (!textMarker || !cache || isTextMarkerIgnored(textMarker))
        return nullptr;
    
    TextMarkerData textMarkerData;
    if (!getBytesFromAXTextMarker((__bridge CFTypeRef)textMarker, &textMarkerData, sizeof(textMarkerData)))
        return nullptr;
    return cache->accessibilityObjectForTextMarkerData(textMarkerData);
}

- (id)textMarkerRangeFromRange:(const Optional<SimpleRange>&)range
{
    if (auto* backingObject = self.axBackingObject)
        return textMarkerRangeFromRange(backingObject->axObjectCache(), range);
    return nil;
}

static id textMarkerRangeFromRange(AXObjectCache* cache, const Optional<SimpleRange>& range)
{
    id startTextMarker = startOrEndTextmarkerForRange(cache, range, true);
    id endTextMarker = startOrEndTextmarkerForRange(cache, range, false);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

- (id)startOrEndTextMarkerForRange:(const Optional<SimpleRange>&)range isStart:(BOOL)isStart
{
    return startOrEndTextmarkerForRange(self.axBackingObject->axObjectCache(), range, isStart);
}

static id startOrEndTextmarkerForRange(AXObjectCache* cache, const Optional<SimpleRange>& range, bool isStart)
{
    if (!cache)
        return nil;

    if (!range)
        return nil;

    TextMarkerData textMarkerData;
    cache->startOrEndTextMarkerDataForRange(textMarkerData, *range, isStart);
    if (!textMarkerData.axID)
        return nil;

    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData)));
}

static id nextTextMarkerForCharacterOffset(AXObjectCache* cache, CharacterOffset& characterOffset)
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForNextCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData)));
}

static id previousTextMarkerForCharacterOffset(AXObjectCache* cache, CharacterOffset& characterOffset)
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForPreviousCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData)));
}

- (id)nextTextMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    return nextTextMarkerForCharacterOffset(self.axBackingObject->axObjectCache(), characterOffset);
}

- (id)previousTextMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    return previousTextMarkerForCharacterOffset(self.axBackingObject->axObjectCache(), characterOffset);
}

- (id)textMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    return textMarkerForCharacterOffset(self.axBackingObject->axObjectCache(), characterOffset);
}

static id textMarkerForCharacterOffset(AXObjectCache* cache, const CharacterOffset& characterOffset)
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID && !textMarkerData.ignored)
        return nil;
    
    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData, sizeof(textMarkerData)));
}

- (Optional<SimpleRange>)rangeForTextMarkerRange:(id)textMarkerRange
{
    if (!textMarkerRange)
        return WTF::nullopt;
    
    id startTextMarker = AXTextMarkerRangeStart(textMarkerRange);
    id endTextMarker = AXTextMarkerRangeEnd(textMarkerRange);
    
    if (!startTextMarker || !endTextMarker)
        return WTF::nullopt;
    
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return WTF::nullopt;
    
    CharacterOffset startCharacterOffset = [self characterOffsetForTextMarker:startTextMarker];
    CharacterOffset endCharacterOffset = [self characterOffsetForTextMarker:endTextMarker];
    return cache->rangeForUnorderedCharacterOffsets(startCharacterOffset, endCharacterOffset);
}

static CharacterOffset characterOffsetForTextMarker(AXObjectCache* cache, CFTypeRef textMarker)
{
    if (!cache || !textMarker)
        return CharacterOffset();
    
    TextMarkerData textMarkerData;
    if (!getBytesFromAXTextMarker(textMarker, &textMarkerData, sizeof(textMarkerData)))
        return CharacterOffset();
    
    return cache->characterOffsetForTextMarkerData(textMarkerData);
}

- (CharacterOffset)characterOffsetForTextMarker:(id)textMarker
{
    auto *backingObject = self.axBackingObject;
    return backingObject ? characterOffsetForTextMarker(backingObject->axObjectCache(), (__bridge CFTypeRef)textMarker) : nil;
}

- (id)textMarkerForVisiblePosition:(const VisiblePosition &)visiblePos
{
    auto *backingObject = self.axBackingObject;
    return backingObject ? textMarkerForVisiblePosition(backingObject->axObjectCache(), visiblePos) : nil;
}

- (id)textMarkerForFirstPositionInTextControl:(HTMLTextFormControlElement &)textControl
{
    auto *cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;

    auto textMarkerData = cache->textMarkerDataForFirstPositionInTextControl(textControl);
    if (!textMarkerData)
        return nil;

    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData.value(), sizeof(textMarkerData.value())));
}

static VisiblePosition visiblePositionForTextMarker(AXObjectCache* cache, CFTypeRef textMarker)
{
    ASSERT(cache);
    if (!textMarker)
        return VisiblePosition();
    TextMarkerData textMarkerData;
    if (!getBytesFromAXTextMarker(textMarker, &textMarkerData, sizeof(textMarkerData)))
        return VisiblePosition();
    return cache->visiblePositionForTextMarkerData(textMarkerData);
}

static VisiblePositionRange visiblePositionRangeForTextMarkerRange(AXObjectCache* cache, id textMarkerRange)
{
    return {
        visiblePositionForTextMarker(cache, (__bridge CFTypeRef)AXTextMarkerRangeStart(textMarkerRange)),
        visiblePositionForTextMarker(cache, (__bridge CFTypeRef)AXTextMarkerRangeEnd(textMarkerRange))
    };
}

- (VisiblePosition)visiblePositionForTextMarker:(id)textMarker
{
    return visiblePositionForTextMarker(self.axBackingObject->axObjectCache(), (__bridge CFTypeRef)textMarker);
}

// When modifying attributed strings, the range can come from a source which may provide faulty information (e.g. the spell checker).
// To protect against such cases the range should be validated before adding or removing attributes.
static BOOL AXAttributedStringRangeIsValid(NSAttributedString *attrString, NSRange range)
{
    return (range.location < [attrString length] && NSMaxRange(range) <= [attrString length]);
}

static void AXAttributeStringSetFont(NSMutableAttributedString *attrString, NSString *attribute, CTFontRef font, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;

    if (font) {
        NSDictionary *dictionary = @{
            NSAccessibilityFontNameKey: (__bridge NSString *)adoptCF(CTFontCopyPostScriptName(font)).get(),
            NSAccessibilityFontFamilyKey: (__bridge NSString *)adoptCF(CTFontCopyFamilyName(font)).get(),
            NSAccessibilityVisibleNameKey: (__bridge NSString *)adoptCF(CTFontCopyDisplayName(font)).get(),
            NSAccessibilityFontSizeKey: @(CTFontGetSize(font)),
        };
        [attrString addAttribute:attribute value:dictionary range:range];
    } else
        [attrString removeAttribute:attribute range:range];
    
}

static void AXAttributeStringSetColor(NSMutableAttributedString* attrString, NSString* attribute, NSColor* color, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;

    if (color) {
        CGColorRef cgColor = color.CGColor;
        id existingColor = [attrString attribute:attribute atIndex:range.location effectiveRange:nil];
        if (!existingColor || !CGColorEqualToColor((__bridge CGColorRef)existingColor, cgColor))
            [attrString addAttribute:attribute value:(__bridge id)cgColor range:range];
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetNumber(NSMutableAttributedString* attrString, NSString* attribute, NSNumber* number, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;
    
    if (number)
        [attrString addAttribute:attribute value:number range:range];
    else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetStyle(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    const RenderStyle& style = renderer->style();
    
    // set basic font info
    AXAttributeStringSetFont(attrString, NSAccessibilityFontTextAttribute, style.fontCascade().primaryFont().getCTFont(), range);
    
    // set basic colors
    AXAttributeStringSetColor(attrString, NSAccessibilityForegroundColorTextAttribute, nsColor(style.visitedDependentColor(CSSPropertyColor)), range);
    AXAttributeStringSetColor(attrString, NSAccessibilityBackgroundColorTextAttribute, nsColor(style.visitedDependentColor(CSSPropertyBackgroundColor)), range);
    
    // set super/sub scripting
    VerticalAlign alignment = style.verticalAlign();
    if (alignment == VerticalAlign::Sub)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, @(-1), range);
    else if (alignment == VerticalAlign::Super)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, @(1), range);
    else
        [attrString removeAttribute:NSAccessibilitySuperscriptTextAttribute range:range];
    
    // set shadow
    if (style.textShadow())
        AXAttributeStringSetNumber(attrString, NSAccessibilityShadowTextAttribute, @YES, range);
    else
        [attrString removeAttribute:NSAccessibilityShadowTextAttribute range:range];
    
    // set underline and strikethrough
    auto decor = style.textDecorationsInEffect();
    if (!(decor & TextDecoration::Underline)) {
        [attrString removeAttribute:NSAccessibilityUnderlineTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityUnderlineColorTextAttribute range:range];
    }
    
    if (!(decor & TextDecoration::LineThrough)) {
        [attrString removeAttribute:NSAccessibilityStrikethroughTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityStrikethroughColorTextAttribute range:range];
    }

    if (decor & TextDecoration::Underline || decor & TextDecoration::LineThrough) {
        // FIXME: Should the underline style be reported here?
        auto decorationStyles = TextDecorationPainter::stylesForRenderer(*renderer, decor);

        if (decor & TextDecoration::Underline) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityUnderlineTextAttribute, @YES, range);
            AXAttributeStringSetColor(attrString, NSAccessibilityUnderlineColorTextAttribute, nsColor(decorationStyles.underlineColor), range);
        }
        
        if (decor & TextDecoration::LineThrough) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityStrikethroughTextAttribute, @YES, range);
            AXAttributeStringSetColor(attrString, NSAccessibilityStrikethroughColorTextAttribute, nsColor(decorationStyles.linethroughColor), range);
        }
    }
    
    // Indicate background highlighting.
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (node->hasTagName(HTMLNames::markTag))
            AXAttributeStringSetNumber(attrString, @"AXHighlight", @YES, range);
    }
}

static void AXAttributeStringSetBlockquoteLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;
    
    AccessibilityObject* obj = renderer->document().axObjectCache()->getOrCreate(renderer);
    int quoteLevel = obj->blockquoteLevel();
    
    if (quoteLevel)
        [attrString addAttribute:NSAccessibilityBlockQuoteLevelAttribute value:@(quoteLevel) range:range];
    else
        [attrString removeAttribute:NSAccessibilityBlockQuoteLevelAttribute range:range];
}

static void AXAttributeStringSetSpelling(NSMutableAttributedString* attrString, Node* node, StringView text, NSRange range)
{
    ASSERT(node);
    
    // If this node is not inside editable content, do not run the spell checker on the text.
    if (!node->document().axObjectCache()->rootAXEditableElement(node))
        return;

    if (unifiedTextCheckerEnabled(node->document().frame())) {
        // Check the spelling directly since document->markersForNode() does not store the misspelled marking when the cursor is in a word.
        TextCheckerClient* checker = node->document().editor().textChecker();
        
        // checkTextOfParagraph is the only spelling/grammar checker implemented in WK1 and WK2
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(*checker, text, TextCheckingType::Spelling, results, node->document().frame()->selection().selection());
        
        size_t size = results.size();
        for (unsigned i = 0; i < size; i++) {
            const TextCheckingResult& result = results[i];
            AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, @YES, NSMakeRange(result.range.location + range.location, result.range.length));
#if PLATFORM(MAC)
            AXAttributeStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, @YES, NSMakeRange(result.range.location + range.location, result.range.length));
#endif
        }
        return;
    }

    for (unsigned currentPosition = 0; currentPosition < text.length(); ) {
        int misspellingLocation = -1;
        int misspellingLength = 0;
        node->document().editor().textChecker()->checkSpellingOfString(text.substring(currentPosition), &misspellingLocation, &misspellingLength);
        if (misspellingLocation == -1 || !misspellingLength)
            break;
        
        NSRange spellRange = NSMakeRange(range.location + currentPosition + misspellingLocation, misspellingLength);
        AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, @YES, spellRange);
#if PLATFORM(MAC)
        AXAttributeStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, @YES, spellRange);
#endif

        currentPosition += misspellingLocation + misspellingLength;
    }
}

static void AXAttributeStringSetExpandedTextValue(NSMutableAttributedString *attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer || !AXAttributedStringRangeIsValid(attrString, range))
        return;
    AccessibilityObject* axObject = renderer->document().axObjectCache()->getOrCreate(renderer);
    if (axObject->supportsExpandedTextValue())
        [attrString addAttribute:NSAccessibilityExpandedTextValueAttribute value:axObject->expandedTextValue() range:range];
    else
        [attrString removeAttribute:NSAccessibilityExpandedTextValueAttribute range:range];
}

static void AXAttributeStringSetHeadingLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer)
        return;
    
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;
    
    // Sometimes there are objects between the text and the heading.
    // In those cases the parent hierarchy should be queried to see if there is a heading level.
    int parentHeadingLevel = 0;
    AccessibilityObject* parentObject = renderer->document().axObjectCache()->getOrCreate(renderer->parent());
    for (; parentObject; parentObject = parentObject->parentObject()) {
        parentHeadingLevel = parentObject->headingLevel();
        if (parentHeadingLevel)
            break;
    }
    
    if (parentHeadingLevel)
        [attrString addAttribute:@"AXHeadingLevel" value:@(parentHeadingLevel) range:range];
    else
        [attrString removeAttribute:@"AXHeadingLevel" range:range];
}

static void AXAttributeStringSetElement(NSMutableAttributedString* attrString, NSString* attribute, AccessibilityObject* object, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;
    
    if (is<AccessibilityRenderObject>(object)) {
        // make a serializable AX object
        
        RenderObject* renderer = downcast<AccessibilityRenderObject>(*object).renderer();
        if (!renderer)
            return;
        
        AXObjectCache* cache = renderer->document().axObjectCache();
        if (!cache)
            return;
        
        id objectWrapper = object->wrapper();
        if ([attribute isEqualToString:NSAccessibilityAttachmentTextAttribute] && object->isAttachment() && [objectWrapper attachmentView])
            objectWrapper = [objectWrapper attachmentView];
        
        AXUIElementRef axElement = NSAccessibilityCreateAXUIElementRef(objectWrapper);
        if (axElement) {
            [attrString addAttribute:attribute value:(__bridge id)axElement range:range];
            CFRelease(axElement);
        }
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributedStringAppendText(NSMutableAttributedString* attrString, Node* node, StringView text, bool spellCheck)
{
    // skip invisible text
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return;
    
    // easier to calculate the range before appending the string
    NSRange attrStringRange = NSMakeRange([attrString length], text.length());
    
    // append the string from this node
    [[attrString mutableString] appendString:text.createNSStringWithoutCopying().get()];
    
    // add new attributes and remove irrelevant inherited ones
    // NOTE: color attributes are handled specially because -[NSMutableAttributedString addAttribute: value: range:] does not merge
    // identical colors.  Workaround is to not replace an existing color attribute if it matches what we are adding.  This also means
    // we cannot just pre-remove all inherited attributes on the appended string, so we have to remove the irrelevant ones individually.
    
    // remove inherited attachment from prior AXAttributedStringAppendReplaced
    [attrString removeAttribute:NSAccessibilityAttachmentTextAttribute range:attrStringRange];
    if (spellCheck) {
#if PLATFORM(MAC)
        [attrString removeAttribute:NSAccessibilityMarkedMisspelledTextAttribute range:attrStringRange];
#endif
        [attrString removeAttribute:NSAccessibilityMisspelledTextAttribute range:attrStringRange];
    }
    
    // set new attributes
    AXAttributeStringSetStyle(attrString, renderer, attrStringRange);
    AXAttributeStringSetHeadingLevel(attrString, renderer, attrStringRange);
    AXAttributeStringSetBlockquoteLevel(attrString, renderer, attrStringRange);
    AXAttributeStringSetExpandedTextValue(attrString, renderer, attrStringRange);
    AXAttributeStringSetElement(attrString, NSAccessibilityLinkTextAttribute, AccessibilityObject::anchorElementForNode(node), attrStringRange);
    
    // do spelling last because it tends to break up the range
    if (spellCheck)
        AXAttributeStringSetSpelling(attrString, node, text, attrStringRange);
}

static NSString* nsStringForReplacedNode(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !isRendererReplacedElement(replacedNode->renderer()) || replacedNode->isTextNode()) {
        ASSERT_NOT_REACHED();
        return nil;
    }
    
    // create an AX object, but skip it if it is not supposed to be seen
    RefPtr<AccessibilityObject> obj = replacedNode->renderer()->document().axObjectCache()->getOrCreate(replacedNode->renderer());
    if (obj->accessibilityIsIgnored())
        return nil;
    
    // use the attachmentCharacter to represent the replaced node
    const UniChar attachmentChar = NSAttachmentCharacter;
    return [NSString stringWithCharacters:&attachmentChar length:1];
}

- (NSAttributedString *)doAXAttributedStringForTextMarkerRange:(id)textMarkerRange spellCheck:(BOOL)spellCheck
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<NSAttributedString *>([&textMarkerRange, &spellCheck, protectedSelf = retainPtr(self)] () -> RetainPtr<NSAttributedString> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto range = [protectedSelf rangeForTextMarkerRange:textMarkerRange];
        if (!range)
            return nil;
        auto attrString = adoptNS([[NSMutableAttributedString alloc] init]);
        TextIterator it(*range);
        while (!it.atEnd()) {
            Node& node = it.range().start.container;

            // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
            if (it.text().length()) {
                // Add the text of the list marker item if necessary.
                String listMarkerText = AccessibilityObject::listMarkerTextForNodeAndPosition(&node, VisiblePosition(createLegacyEditingPosition(it.range().start)));
                if (!listMarkerText.isEmpty())
                    AXAttributedStringAppendText(attrString.get(), &node, listMarkerText, spellCheck);
                AXAttributedStringAppendText(attrString.get(), &node, it.text(), spellCheck);
            } else {
                Node* replacedNode = it.node();
                NSString *attachmentString = nsStringForReplacedNode(replacedNode);
                if (attachmentString) {
                    NSRange attrStringRange = NSMakeRange([attrString length], [attachmentString length]);

                    // append the placeholder string
                    [[attrString mutableString] appendString:attachmentString];

                    // remove all inherited attributes
                    [attrString setAttributes:nil range:attrStringRange];

                    // add the attachment attribute
                    AccessibilityObject* obj = replacedNode->renderer()->document().axObjectCache()->getOrCreate(replacedNode->renderer());
                    AXAttributeStringSetElement(attrString.get(), NSAccessibilityAttachmentTextAttribute, obj, attrStringRange);
                }
            }
            it.advance();
        }

        return attrString;
    });
}

- (id)textMarkerRangeFromVisiblePositions:(const VisiblePosition&)startPosition endPosition:(const VisiblePosition&)endPosition
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&startPosition, &endPosition, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        return textMarkerRangeFromVisiblePositions(backingObject->axObjectCache(), startPosition, endPosition);
    });
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray*)accessibilityActionNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    // All elements should get ShowMenu and ScrollToVisible.
    // But certain earlier VoiceOver versions do not support scroll to visible, and it confuses them to see it in the list.
    static NSArray *defaultElementActions = [@[NSAccessibilityShowMenuAction, NSAccessibilityScrollToVisibleAction] retain];

    // Action elements allow Press.
    // The order is important to VoiceOver, which expects the 'default' action to be the first action. In this case the default action should be press.
    static NSArray *actionElementActions = [@[NSAccessibilityPressAction, NSAccessibilityShowMenuAction, NSAccessibilityScrollToVisibleAction] retain];

    // Menu elements allow Press and Cancel.
    static NSArray *menuElementActions = [[actionElementActions arrayByAddingObject:NSAccessibilityCancelAction] retain];

    // Slider elements allow Increment/Decrement.
    static NSArray *sliderActions = [[defaultElementActions arrayByAddingObjectsFromArray:@[NSAccessibilityIncrementAction, NSAccessibilityDecrementAction]] retain];

    NSArray *actions;
    if (backingObject->supportsPressAction())
        actions = actionElementActions;
    else if (backingObject->isMenuRelated())
        actions = menuElementActions;
    else if (backingObject->isSlider())
        actions = sliderActions;
    else if (backingObject->isAttachment())
        actions = [[self attachmentView] accessibilityActionNames];
    else
        actions = defaultElementActions;

    return actions;
}

- (NSArray*)additionalAccessibilityAttributeNames
{
    auto* backingObject = self.axBackingObject;
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
    if (AXObjectCache::isIsolatedTreeEnabled() && _AXUIElementRequestServicedBySecondaryAXThread())
        [additional addObject:NSAccessibilityRelativeFrameAttribute];
#endif
    
    return additional;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray*)accessibilityAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    if (backingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeNames];

    static NSArray* attributes = nil;
    static NSArray* anchorAttrs = nil;
    static NSArray* webAreaAttrs = nil;
    static NSArray* textAttrs = nil;
    static NSArray* listAttrs = nil;
    static NSArray* listBoxAttrs = nil;
    static NSArray* rangeAttrs = nil;
    static NSArray* commonMenuAttrs = nil;
    static NSArray* menuAttrs = nil;
    static NSArray* menuBarAttrs = nil;
    static NSArray* menuItemAttrs = nil;
    static NSArray* menuButtonAttrs = nil;
    static NSArray* controlAttrs = nil;
    static NSArray* tableAttrs = nil;
    static NSArray* tableRowAttrs = nil;
    static NSArray* tableColAttrs = nil;
    static NSArray* tableCellAttrs = nil;
    static NSArray* groupAttrs = nil;
    static NSArray* inputImageAttrs = nil;
    static NSArray* passwordFieldAttrs = nil;
    static NSArray* tabListAttrs = nil;
    static NSArray* comboBoxAttrs = nil;
    static NSArray* outlineAttrs = nil;
    static NSArray* outlineRowAttrs = nil;
    static NSArray* buttonAttrs = nil;
    static NSArray* scrollViewAttrs = nil;
    static NSArray* incrementorAttrs = nil;
    NSMutableArray* tempArray;
    if (attributes == nil) {
        attributes = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            NSAccessibilitySubroleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityChildrenAttribute,
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
            @"AXSelectedTextMarkerRange",
            @"AXStartTextMarker",
            @"AXEndTextMarker",
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
            nil];
    }
    if (commonMenuAttrs == nil) {
        commonMenuAttrs = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
                           NSAccessibilityRoleDescriptionAttribute,
                           NSAccessibilityChildrenAttribute,
                           NSAccessibilityParentAttribute,
                           NSAccessibilityEnabledAttribute,
                           NSAccessibilityPositionAttribute,
                           NSAccessibilitySizeAttribute,
                           nil];
    }
    if (anchorAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityURLAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityLinkRelationshipTypeAttribute];
        anchorAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (webAreaAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
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
        [tempArray addObject:NSAccessibilityWebSessionIDAttribute];
        webAreaAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (textAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
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
        [tempArray addObject:NSAccessibilityValueAutofilledAttribute];
        textAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (listAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        listAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (listBoxAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:listAttrs];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        listBoxAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (rangeAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityMinValueAttribute];
        [tempArray addObject:NSAccessibilityMaxValueAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilityValueDescriptionAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        rangeAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuBarAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:commonMenuAttrs];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        menuBarAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:commonMenuAttrs];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        menuAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuItemAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:commonMenuAttrs];
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
        menuItemAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuButtonAttrs == nil) {
        menuButtonAttrs = [[NSArray alloc] initWithObjects:NSAccessibilityRoleAttribute,
                           NSAccessibilityRoleDescriptionAttribute,
                           NSAccessibilityParentAttribute,
                           NSAccessibilityPositionAttribute,
                           NSAccessibilitySizeAttribute,
                           NSAccessibilityWindowAttribute,
                           NSAccessibilityEnabledAttribute,
                           NSAccessibilityFocusedAttribute,
                           NSAccessibilityTitleAttribute,
                           NSAccessibilityChildrenAttribute, nil];
    }
    if (controlAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        controlAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (incrementorAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityIncrementButtonAttribute];
        [tempArray addObject:NSAccessibilityDecrementButtonAttribute];
        [tempArray addObject:NSAccessibilityValueDescriptionAttribute];
        [tempArray addObject:NSAccessibilityMinValueAttribute];
        [tempArray addObject:NSAccessibilityMaxValueAttribute];
        incrementorAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (buttonAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        // Buttons should not expose AXValue.
        [tempArray removeObject:NSAccessibilityValueAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        buttonAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (comboBoxAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:controlAttrs];
        [tempArray addObject:NSAccessibilityExpandedAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        comboBoxAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
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
        tableAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableRowAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityIndexAttribute];
        tableRowAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableColAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityIndexAttribute];
        [tempArray addObject:NSAccessibilityHeaderAttribute];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityVisibleRowsAttribute];
        tableColAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableCellAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityRowIndexRangeAttribute];
        [tempArray addObject:NSAccessibilityColumnIndexRangeAttribute];
        [tempArray addObject:NSAccessibilityColumnHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityRowHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityARIAColumnIndexAttribute];
        [tempArray addObject:NSAccessibilityARIARowIndexAttribute];
        tableCellAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (groupAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        groupAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (inputImageAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:buttonAttrs];
        [tempArray addObject:NSAccessibilityURLAttribute];
        inputImageAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (passwordFieldAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        [tempArray addObject:NSAccessibilityInvalidAttribute];
        [tempArray addObject:NSAccessibilityPlaceholderValueAttribute];
        [tempArray addObject:NSAccessibilityValueAutofilledAttribute];
        passwordFieldAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tabListAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTabsAttribute];
        [tempArray addObject:NSAccessibilityContentsAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        tabListAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (outlineAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilitySelectedRowsAttribute];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityColumnsAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        outlineAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (outlineRowAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:tableRowAttrs];
        [tempArray addObject:NSAccessibilityDisclosingAttribute];
        [tempArray addObject:NSAccessibilityDisclosedByRowAttribute];
        [tempArray addObject:NSAccessibilityDisclosureLevelAttribute];
        [tempArray addObject:NSAccessibilityDisclosedRowsAttribute];
        outlineRowAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (scrollViewAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityContentsAttribute];
        [tempArray addObject:NSAccessibilityHorizontalScrollBarAttribute];
        [tempArray addObject:NSAccessibilityVerticalScrollBarAttribute];
        scrollViewAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }

    NSArray *objectAttributes = attributes;

    if (backingObject->isPasswordField())
        objectAttributes = passwordFieldAttrs;
    else if (backingObject->isWebArea())
        objectAttributes = webAreaAttrs;
    else if (backingObject->isTextControl())
        objectAttributes = textAttrs;
    else if (backingObject->isLink() || backingObject->isImage())
        objectAttributes = anchorAttrs;
    else if (backingObject->isTable() && backingObject->isExposable())
        objectAttributes = tableAttrs;
    else if (backingObject->isTableColumn())
        objectAttributes = tableColAttrs;
    else if (backingObject->isTableCell())
        objectAttributes = tableCellAttrs;
    else if (backingObject->isTableRow()) {
        // An ARIA table row can be collapsed and expanded, so it needs the extra attributes.
        if (backingObject->isARIATreeGridRow())
            objectAttributes = outlineRowAttrs;
        else
            objectAttributes = tableRowAttrs;
    } else if (backingObject->isTree())
        objectAttributes = outlineAttrs;
    else if (backingObject->isTreeItem())
        objectAttributes = outlineRowAttrs;
    else if (backingObject->isListBox())
        objectAttributes = listBoxAttrs;
    else if (backingObject->isList())
        objectAttributes = listAttrs;
    else if (backingObject->isComboBox())
        objectAttributes = comboBoxAttrs;
    else if (backingObject->isProgressIndicator() || backingObject->isSlider())
        objectAttributes = rangeAttrs;
    // These are processed in order because an input image is a button, and a button is a control.
    else if (backingObject->isInputImage())
        objectAttributes = inputImageAttrs;
    else if (backingObject->isButton())
        objectAttributes = buttonAttrs;
    else if (backingObject->isControl())
        objectAttributes = controlAttrs;

    else if (backingObject->isGroup() || backingObject->isListItem())
        objectAttributes = groupAttrs;
    else if (backingObject->isTabList())
        objectAttributes = tabListAttrs;
    else if (backingObject->isScrollView())
        objectAttributes = scrollViewAttrs;
    else if (backingObject->isSpinButton())
        objectAttributes = incrementorAttrs;
    else if (backingObject->isMenu())
        objectAttributes = menuAttrs;
    else if (backingObject->isMenuBar())
        objectAttributes = menuBarAttrs;
    else if (backingObject->isMenuButton())
        objectAttributes = menuButtonAttrs;
    else if (backingObject->isMenuItem())
        objectAttributes = menuItemAttrs;

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

- (VisiblePositionRange)visiblePositionRangeForTextMarkerRange:(id)textMarkerRange
{
    return visiblePositionRangeForTextMarkerRange(self.axBackingObject->axObjectCache(), textMarkerRange);
}

- (NSArray*)renderWidgetChildren
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

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

- (id)textMarkerRangeForSelection
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        VisibleSelection selection = backingObject->selection();
        if (selection.isNone())
            return nil;
        return textMarkerRangeFromVisiblePositions(backingObject->axObjectCache(), selection.visibleStart(), selection.visibleEnd());
    });
}

- (id)associatedPluginParent
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject || !backingObject->hasApplePDFAnnotationAttribute())
            return nil;

        if (!backingObject->document()->isPluginDocument())
            return nil;

        Widget* pluginWidget = static_cast<PluginDocument*>(backingObject->document())->pluginWidget();
        if (!pluginWidget || !pluginWidget->isPluginViewBase())
            return nil;

        return static_cast<PluginViewBase*>(pluginWidget)->accessibilityAssociatedPluginParentForElement(backingObject->element());
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

- (NSNumber *)primaryScreenHeight
{
    // The rect for primary screen should not change in normal use. So cache it
    // here to avoid hitting the main thread repeatedly.
    static FloatRect screenRect = Accessibility::retrieveValueFromMainThread<FloatRect>([] () -> FloatRect {
        return screenRectForPrimaryScreen();
    });
    return [NSNumber numberWithFloat:screenRect.height()];
}

- (size_t)childrenVectorSize
{
    return self.axBackingObject->children().size();
}

- (NSArray<WebAccessibilityObjectWrapper *> *)childrenVectorArray
{
    return convertToNSArray(self.axBackingObject->children());
}

- (NSValue *)position
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (AXObjectCache::isIsolatedTreeEnabled())
        return [NSValue valueWithPoint:self.axBackingObject->relativeFrame().location()];
#endif

    auto rect = snappedIntRect(self.axBackingObject->elementRect());
    
    // The Cocoa accessibility API wants the lower-left corner.
    auto floatPoint = FloatPoint(rect.x(), rect.maxY());

    auto floatRect = FloatRect(floatPoint, FloatSize());
    CGPoint cgPoint = [self convertRectToSpace:floatRect space:AccessibilityConversionSpace::Screen].origin;
    return [NSValue valueWithPoint:NSPointFromCGPoint(cgPoint)];
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

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSString*)subrole
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    // FIXME: create AXCoreObject::subrolePlatformString to replace the following linear search and heuristics, similar to rolePlatformString.
    if (backingObject->isPasswordField())
        return NSAccessibilitySecureTextFieldSubrole;
    if (backingObject->isSearchField())
        return NSAccessibilitySearchFieldSubrole;

    if (backingObject->isAttachment()) {
        NSView* attachView = [self attachmentView];
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute])
            return [attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
    }

    if (backingObject->isMeter())
        return @"AXMeter";

    AccessibilityRole role = backingObject->roleValue();
    if (role == AccessibilityRole::HorizontalRule)
        return NSAccessibilityContentSeparatorSubrole;
    if (role == AccessibilityRole::ToggleButton)
        return NSAccessibilityToggleSubrole;
    if (role == AccessibilityRole::Footer)
        return @"AXFooter";
    if (role == AccessibilityRole::SpinButtonPart) {
        if (backingObject->isIncrementor())
            return NSAccessibilityIncrementArrowSubrole;
        return NSAccessibilityDecrementArrowSubrole;
    }

    if (backingObject->isFileUploadButton())
        return @"AXFileUploadButton";

    if (backingObject->isTreeItem())
        return NSAccessibilityOutlineRowSubrole;

    if (backingObject->isFieldset())
        return @"AXFieldset";

    if (backingObject->isList()) {
        if (backingObject->isUnorderedList() || backingObject->isOrderedList())
            return NSAccessibilityContentListSubrole;
        if (backingObject->isDescriptionList()) {
            return NSAccessibilityDescriptionListSubrole;
        }
    }

    // ARIA content subroles.
    switch (role) {
    case AccessibilityRole::LandmarkBanner:
        return @"AXLandmarkBanner";
    case AccessibilityRole::LandmarkComplementary:
        return @"AXLandmarkComplementary";
    case AccessibilityRole::LandmarkContentInfo:
        return @"AXLandmarkContentInfo";
    case AccessibilityRole::LandmarkMain:
        return @"AXLandmarkMain";
    case AccessibilityRole::LandmarkNavigation:
        return @"AXLandmarkNavigation";
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkRegion:
        return @"AXLandmarkRegion";
    case AccessibilityRole::LandmarkSearch:
        return @"AXLandmarkSearch";
    case AccessibilityRole::ApplicationAlert:
        return @"AXApplicationAlert";
    case AccessibilityRole::ApplicationAlertDialog:
        return @"AXApplicationAlertDialog";
    case AccessibilityRole::ApplicationDialog:
        return @"AXApplicationDialog";
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ApplicationTextGroup:
    case AccessibilityRole::Feed:
    case AccessibilityRole::Footnote:
        return @"AXApplicationGroup";
    case AccessibilityRole::ApplicationLog:
        return @"AXApplicationLog";
    case AccessibilityRole::ApplicationMarquee:
        return @"AXApplicationMarquee";
    case AccessibilityRole::ApplicationStatus:
        return @"AXApplicationStatus";
    case AccessibilityRole::ApplicationTimer:
        return @"AXApplicationTimer";
    case AccessibilityRole::Document:
    case AccessibilityRole::GraphicsDocument:
        return @"AXDocument";
    case AccessibilityRole::DocumentArticle:
        return @"AXDocumentArticle";
    case AccessibilityRole::DocumentMath:
        return @"AXDocumentMath";
    case AccessibilityRole::DocumentNote:
        return @"AXDocumentNote";
    case AccessibilityRole::UserInterfaceTooltip:
        return @"AXUserInterfaceTooltip";
    case AccessibilityRole::TabPanel:
        return @"AXTabPanel";
    case AccessibilityRole::Definition:
        return @"AXDefinition";
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::Term:
        return @"AXTerm";
    case AccessibilityRole::DescriptionListDetail:
        return @"AXDescription";
    case AccessibilityRole::WebApplication:
        return @"AXWebApplication";
        // Default doesn't return anything, so roles defined below can be chosen.
    default:
        break;
    }

    if (role == AccessibilityRole::MathElement) {
        if (backingObject->isMathFraction())
            return @"AXMathFraction";
        if (backingObject->isMathFenced())
            return @"AXMathFenced";
        if (backingObject->isMathSubscriptSuperscript())
            return @"AXMathSubscriptSuperscript";
        if (backingObject->isMathRow())
            return @"AXMathRow";
        if (backingObject->isMathUnderOver())
            return @"AXMathUnderOver";
        if (backingObject->isMathSquareRoot())
            return @"AXMathSquareRoot";
        if (backingObject->isMathRoot())
            return @"AXMathRoot";
        if (backingObject->isMathText())
            return @"AXMathText";
        if (backingObject->isMathNumber())
            return @"AXMathNumber";
        if (backingObject->isMathIdentifier())
            return @"AXMathIdentifier";
        if (backingObject->isMathTable())
            return @"AXMathTable";
        if (backingObject->isMathTableRow())
            return @"AXMathTableRow";
        if (backingObject->isMathTableCell())
            return @"AXMathTableCell";
        if (backingObject->isMathFenceOperator())
            return @"AXMathFenceOperator";
        if (backingObject->isMathSeparatorOperator())
            return @"AXMathSeparatorOperator";
        if (backingObject->isMathOperator())
            return @"AXMathOperator";
        if (backingObject->isMathMultiscript())
            return @"AXMathMultiscript";
    }

    if (role == AccessibilityRole::Video)
        return @"AXVideo";
    if (role == AccessibilityRole::Audio)
        return @"AXAudio";
    if (role == AccessibilityRole::Details)
        return @"AXDetails";
    if (role == AccessibilityRole::Summary)
        return @"AXSummary";
    if (role == AccessibilityRole::Time)
        return @"AXTimeGroup";

    if (backingObject->isMediaTimeline())
        return NSAccessibilityTimelineSubrole;

    if (backingObject->isSwitch())
        return NSAccessibilitySwitchSubrole;

    if (role == AccessibilityRole::Insertion)
        return @"AXInsertStyleGroup";
    if (role == AccessibilityRole::Deletion)
        return @"AXDeleteStyleGroup";
    if (role == AccessibilityRole::Superscript)
        return @"AXSuperscriptStyleGroup";
    if (role == AccessibilityRole::Subscript)
        return @"AXSubscriptStyleGroup";

    if (backingObject->isStyleFormatGroup()) {
        using namespace HTMLNames;
        auto tagName = backingObject->tagName();
        if (tagName == kbdTag)
            return @"AXKeyboardInputStyleGroup";
        if (tagName == codeTag)
            return @"AXCodeStyleGroup";
        if (tagName == preTag)
            return @"AXPreformattedStyleGroup";
        if (tagName == sampTag)
            return @"AXSampleStyleGroup";
        if (tagName == varTag)
            return @"AXVariableStyleGroup";
        if (tagName == citeTag)
            return @"AXCiteStyleGroup";
        ASSERT_NOT_REACHED();
    }

    // Ruby subroles
    switch (role) {
    case AccessibilityRole::RubyBase:
        return NSAccessibilityRubyBaseSubrole;
    case AccessibilityRole::RubyBlock:
        return NSAccessibilityRubyBlockSubrole;
    case AccessibilityRole::RubyInline:
        return NSAccessibilityRubyInlineSubrole;
    case AccessibilityRole::RubyRun:
        return NSAccessibilityRubyRunSubrole;
    case AccessibilityRole::RubyText:
        return NSAccessibilityRubyTextSubrole;
    default:
        break;
    }

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

- (NSString *)computedRoleString
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

- (id)windowElement:(NSString*)attributeName
{
    if (id remoteParent = self.remoteAccessibilityParentObject) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        return [remoteParent accessibilityAttributeValue:attributeName];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        if (auto* fv = backingObject->documentFrameView())
            return [fv->platformWidget() window];

        return nil;
    });
}

// FIXME: split up this function in a better way.
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString*)attributeName
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityAttributeValue:", String(attributeName)));
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    if (backingObject->isDetachedFromParent()) {
        AXLOG("backingObject is detached from parent!!!");
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
        return [self computedRoleString];

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

    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute] || [attributeName isEqualToString: NSAccessibilityChildrenInNavigationOrderAttribute]) {
        if (!self.childrenVectorSize) {
            NSArray* children = [self renderWidgetChildren];
            if (children != nil)
                return children;
        }

        // The tree's (AXOutline) children are supposed to be its rows and columns.
        // The ARIA spec doesn't have columns, so we just need rows.
        if (backingObject->isTree())
            return [self accessibilityAttributeValue:NSAccessibilityRowsAttribute];

        // A tree item should only expose its content as its children (not its rows)
        if (backingObject->isTreeItem()) {
            AccessibilityObject::AccessibilityChildrenVector contentCopy;
            backingObject->ariaTreeItemContent(contentCopy);
            return convertToNSArray(contentCopy);
        }

        return self.childrenVectorArray;
    }

    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (backingObject->canHaveSelectedChildren()) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            backingObject->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }
        return nil;
    }

    if ([attributeName isEqualToString: NSAccessibilityVisibleChildrenAttribute]) {
        if (backingObject->isListBox()) {
            AccessibilityObject::AccessibilityChildrenVector visibleChildrenCopy;
            backingObject->visibleChildren(visibleChildrenCopy);
            return convertToNSArray(visibleChildrenCopy);
        }

        if (backingObject->isList())
            return [self accessibilityAttributeValue:NSAccessibilityChildrenAttribute];

        return nil;
    }


    if (backingObject->isWebArea()) {
        if ([attributeName isEqualToString:@"AXLinkUIElements"])
            return convertToNSArray(backingObject->documentLinks());

        if ([attributeName isEqualToString:@"AXLoaded"])
            return [NSNumber numberWithBool:backingObject->isLoaded()];
        if ([attributeName isEqualToString:@"AXLayoutCount"])
            return @(backingObject->layoutCount());
        if ([attributeName isEqualToString:NSAccessibilityLoadingProgressAttribute])
            return @(backingObject->estimatedLoadingProgress());
        if ([attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
            return [NSNumber numberWithBool:backingObject->preventKeyboardDOMEventDispatch()];
        if ([attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
            return [NSNumber numberWithBool:backingObject->caretBrowsingEnabled()];
        if ([attributeName isEqualToString:NSAccessibilityWebSessionIDAttribute])
            return @(backingObject->sessionID());
    }

    if (backingObject->isTextControl()) {
        if ([attributeName isEqualToString: NSAccessibilityNumberOfCharactersAttribute]) {
            int length = backingObject->textLength();
            if (length < 0)
                return nil;
            return @(length);
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            String selectedText = backingObject->selectedText();
            if (selectedText.isNull())
                return nil;
            return (NSString*)selectedText;
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            PlainTextRange textRange = backingObject->selectedTextRange();
            return [NSValue valueWithRange:NSMakeRange(textRange.start, textRange.length)];
        }
        // TODO: Get actual visible range. <rdar://problem/4712101>
        if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
            return backingObject->isPasswordField() ? nil : [NSValue valueWithRange: NSMakeRange(0, backingObject->textLength())];
        if ([attributeName isEqualToString: NSAccessibilityInsertionPointLineNumberAttribute]) {
            // if selectionEnd > 0, then there is selected text and this question should not be answered
            if (backingObject->isPasswordField() || backingObject->selectionEnd() > 0)
                return nil;

            auto *focusedObject = backingObject->focusedUIElement();
            if (focusedObject != backingObject)
                return nil;

            VisiblePosition focusedPosition = focusedObject->visiblePositionForIndex(focusedObject->selectionStart(), true);
            int lineNumber = backingObject->lineForPosition(focusedPosition);
            if (lineNumber < 0)
                return nil;

            return @(lineNumber);
        }
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
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }

        // Meter elements should communicate their content via AXValueDescription.
        if (backingObject->isMeter())
            return [NSString string];

        // Summary element should use its text node as AXTitle.
        if (backingObject->isSummary())
            return backingObject->textUnderElement();

        return [self baseAccessibilityTitle];
    }

    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute]) {
        if (backingObject->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return [self baseAccessibilityDescription];
    }

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (backingObject->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityValueAttribute];
        }
        if (backingObject->supportsRangeValue())
            return [NSNumber numberWithFloat:backingObject->valueForRange()];
        if (backingObject->roleValue() == AccessibilityRole::SliderThumb)
            return [NSNumber numberWithFloat:backingObject->parentObject()->valueForRange()];
        if (backingObject->isHeading())
            return @(backingObject->headingLevel());

        if (backingObject->isCheckboxOrRadio() || backingObject->isMenuItem() || backingObject->isSwitch() || backingObject->isToggleButton()) {
            switch (backingObject->checkboxOrRadioValue()) {
            case AccessibilityButtonState::Off:
                return @(0);
            case AccessibilityButtonState::On:
                return @(1);
            case AccessibilityButtonState::Mixed:
                return @(2);
            }
        }

        // radio groups return the selected radio button as the AXValue
        if (backingObject->isRadioGroup()) {
            AXCoreObject* radioButton = backingObject->selectedRadioButton();
            if (!radioButton)
                return nil;
            return radioButton->wrapper();
        }

        if (backingObject->isTabList()) {
            AXCoreObject* tabItem = backingObject->selectedTabItem();
            if (!tabItem)
                return nil;
            return tabItem->wrapper();
        }

        if (backingObject->isTabItem())
            return @(backingObject->isSelected());

        if (backingObject->isColorWell()) {
            auto color = convertToComponentFloats(backingObject->colorValue());
            return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1", color.red, color.green, color.blue];
        }

        return backingObject->stringValue();
    }

    if ([attributeName isEqualToString:(NSString *)kAXMenuItemMarkCharAttribute]) {
        const unichar ch = 0x2713; // ✓ used on Mac for selected menu items.
        return (backingObject->isChecked()) ? [NSString stringWithCharacters:&ch length:1] : nil;
    }

    if ([attributeName isEqualToString: NSAccessibilityMinValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (backingObject->ariaRoleAttribute() == AccessibilityRole::ProgressIndicator && !backingObject->hasARIAValueNow())
            return @0;
        return [NSNumber numberWithFloat:backingObject->minValueForRange()];
    }

    if ([attributeName isEqualToString: NSAccessibilityMaxValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (backingObject->ariaRoleAttribute() == AccessibilityRole::ProgressIndicator && !backingObject->hasARIAValueNow())
            return @0;
        return [NSNumber numberWithFloat:backingObject->maxValueForRange()];
    }

    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return [self baseAccessibilityHelpText];

    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [NSNumber numberWithBool: backingObject->isFocused()];

    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: backingObject->isEnabled()];

    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute]) {
        IntSize s = snappedIntRect(backingObject->elementRect()).size();
        return [NSValue valueWithSize: NSMakeSize(s.width(), s.height())];
    }

    if ([attributeName isEqualToString: NSAccessibilityPrimaryScreenHeightAttribute])
        return [self primaryScreenHeight];
    if ([attributeName isEqualToString: NSAccessibilityPositionAttribute])
        return [self position];
    if ([attributeName isEqualToString:NSAccessibilityPathAttribute])
        return [self path];

    if ([attributeName isEqualToString:NSAccessibilityWindowAttribute]
        || [attributeName isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [self windowElement:attributeName];

    if ([attributeName isEqualToString:NSAccessibilityAccessKeyAttribute]) {
        AtomString accessKey = backingObject->accessKey();
        if (accessKey.isNull())
            return nil;
        return accessKey;
    }

    if ([attributeName isEqualToString:NSAccessibilityLinkRelationshipTypeAttribute])
        return backingObject->linkRelValue();

    if ([attributeName isEqualToString:NSAccessibilityTabsAttribute]) {
        if (backingObject->isTabList()) {
            AccessibilityObject::AccessibilityChildrenVector tabsChildren;
            backingObject->tabChildren(tabsChildren);
            return convertToNSArray(tabsChildren);
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityContentsAttribute]) {
        // The contents of a tab list are all the children except the tabs.
        if (backingObject->isTabList()) {
            auto children = self.childrenVectorArray;
            AccessibilityObject::AccessibilityChildrenVector tabs;
            backingObject->tabChildren(tabs);
            auto tabsChildren = convertToNSArray(tabs);

            NSMutableArray *contents = [NSMutableArray array];
            for (id childWrapper in children) {
                if ([tabsChildren containsObject:childWrapper])
                    [contents addObject:childWrapper];
            }
            return contents;
        }

        if (backingObject->isScrollView()) {
            // A scrollView's contents are everything except the scroll bars.
            auto children = self.childrenVectorArray;
            NSMutableArray *contents = [NSMutableArray array];

            for (WebAccessibilityObjectWrapper *childWrapper in children) {
                if (auto backingObject = [childWrapper axBackingObject]) {
                    if (!backingObject->isScrollbar())
                        [contents addObject:childWrapper];
                }
            }
            return contents;
        }
    }

    if (backingObject->isTable() && backingObject->isExposable()) {
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute])
            return convertToNSArray(backingObject->rows());

        if ([attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute])
            return convertToNSArray(backingObject->visibleRows());

        // TODO: distinguish between visible and non-visible columns
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute]
            || [attributeName isEqualToString:NSAccessibilityVisibleColumnsAttribute])
            return convertToNSArray(backingObject->columns());

        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            backingObject->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }

        // HTML tables don't support these attributes.
        if ([attributeName isEqualToString:NSAccessibilitySelectedColumnsAttribute]
            || [attributeName isEqualToString:NSAccessibilitySelectedCellsAttribute])
            return nil;

        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute])
            return convertToNSArray(backingObject->columnHeaders());

        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            auto* headerContainer = backingObject->headerContainer();
            return headerContainer ? headerContainer->wrapper() : nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute])
            return convertToNSArray(backingObject->rowHeaders());

        if ([attributeName isEqualToString:NSAccessibilityVisibleCellsAttribute])
            return convertToNSArray(backingObject->cells());

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
            return convertToNSArray(backingObject->children());

        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            auto* header = backingObject->columnHeader();
            return header ? header->wrapper() : nil;
        }
    }

    if (backingObject->isTableCell()) {
        if ([attributeName isEqualToString:NSAccessibilityRowIndexRangeAttribute]) {
            auto rowRange = backingObject->rowIndexRange();
            return [NSValue valueWithRange:NSMakeRange(rowRange.first, rowRange.second)];
        }

        if ([attributeName isEqualToString:NSAccessibilityColumnIndexRangeAttribute]) {
            auto columnRange = backingObject->columnIndexRange();
            return [NSValue valueWithRange:NSMakeRange(columnRange.first, columnRange.second)];
        }

        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute])
            return convertToNSArray(backingObject->columnHeaders());

        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute])
            return convertToNSArray(backingObject->rowHeaders());

        if ([attributeName isEqualToString:NSAccessibilityARIAColumnIndexAttribute])
            return @(backingObject->axColumnIndex());

        if ([attributeName isEqualToString:NSAccessibilityARIARowIndexAttribute])
            return @(backingObject->axRowIndex());
    }

    if (backingObject->isTree()) {
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            backingObject->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            backingObject->ariaTreeRows(rowsCopy);
            return convertToNSArray(rowsCopy);
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
            return convertToNSArray(backingObject->disclosedRows());
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

    if ([attributeName isEqualToString:@"AXSelectedTextMarkerRange"])
        return [self textMarkerRangeForSelection];

    if ([attributeName isEqualToString:@"AXStartTextMarker"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            if (auto* backingObject = protectedSelf.get().axBackingObject)
                return [protectedSelf textMarkerForVisiblePosition:startOfDocument(backingObject->document())];
            return nil;
        });
    }

    if ([attributeName isEqualToString:@"AXEndTextMarker"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            if (auto* backingObject = protectedSelf.get().axBackingObject)
                return [protectedSelf textMarkerForVisiblePosition:endOfDocument(backingObject->document())];
            return nil;
        });
    }

    if ([attributeName isEqualToString:NSAccessibilityBlockQuoteLevelAttribute])
        return @(backingObject->blockquoteLevel());
    if ([attributeName isEqualToString:@"AXTableLevel"])
        return @(backingObject->tableLevel());

    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector linkedUIElements;
        backingObject->linkedUIElements(linkedUIElements);
        return convertToNSArray(linkedUIElements);
    }

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return [NSNumber numberWithBool:backingObject->isSelected()];

    if ([attributeName isEqualToString: NSAccessibilityARIACurrentAttribute])
        return backingObject->currentValue();

    if ([attributeName isEqualToString: NSAccessibilityServesAsTitleForUIElementsAttribute] && backingObject->isMenuButton()) {
        AccessibilityObject* uiElement = downcast<AccessibilityRenderObject>(*backingObject).menuForMenuButton();
        if (uiElement)
            return @[uiElement->wrapper()];
    }

    if ([attributeName isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
        if (!backingObject->exposesTitleUIElement())
            return nil;

        AXCoreObject* obj = backingObject->titleUIElement();
        if (obj)
            return obj->wrapper();
        return nil;
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
        AXCoreObject* scrollBar = backingObject->scrollBar(AccessibilityOrientation::Horizontal);
        if (scrollBar)
            return scrollBar->wrapper();
        return nil;
    }
    if ([attributeName isEqualToString:NSAccessibilityVerticalScrollBarAttribute]) {
        AXCoreObject* scrollBar = backingObject->scrollBar(AccessibilityOrientation::Vertical);
        if (scrollBar)
            return scrollBar->wrapper();
        return nil;
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

    if ([attributeName isEqualToString:NSAccessibilityOwnsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector ariaOwns;
        backingObject->ariaOwnsElements(ariaOwns);
        return convertToNSArray(ariaOwns);
    }

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
        }
    }

    if ([attributeName isEqualToString:NSAccessibilityValueAutofilledAttribute])
        return @(backingObject->isValueAutofilled());

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
        if ([attributeName isEqualToString:NSAccessibilityMathRootIndexAttribute])
            return (backingObject->mathRootIndexObject()) ? backingObject->mathRootIndexObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathRootRadicandAttribute])
            return (backingObject->mathRadicandObject()) ? backingObject->mathRadicandObject()->wrapper() : 0;
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
    if ([attributeName isEqualToString:NSAccessibilityDOMClassListAttribute]) {
        Vector<String> classList;
        backingObject->classList(classList);
        return createNSArray(classList).autorelease();
    }

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

    // Used by TestRunner and DRT AccessibilityController to find an accessible node by its element id.
    if ([attributeName isEqualToString:@"AXDRTElementIdAttribute"])
        return backingObject->identifierAttribute();

    if ([attributeName isEqualToString:@"AXAutocompleteValue"])
        return backingObject->autoCompleteValue();

    if ([attributeName isEqualToString:NSAccessibilityPopupValueAttribute])
        return backingObject->popupValue();

    if ([attributeName isEqualToString:@"AXKeyShortcutsValue"])
        return backingObject->keyShortcutsValue();

    if ([attributeName isEqualToString:@"AXARIAPressedIsPresent"])
        return [NSNumber numberWithBool:backingObject->pressedIsPresent()];

    if ([attributeName isEqualToString:@"AXIsMultiline"])
        return [NSNumber numberWithBool:backingObject->ariaIsMultiline()];

    if ([attributeName isEqualToString:@"AXReadOnlyValue"])
        return backingObject->readOnlyValue();

    if ([attributeName isEqualToString:@"AXIsActiveDescendantOfFocusedContainer"])
        return [NSNumber numberWithBool:backingObject->isActiveDescendantOfFocusedContainer()];

    if ([attributeName isEqualToString:@"AXDetailsElements"]) {
        AccessibilityObject::AccessibilityChildrenVector details;
        backingObject->ariaDetailsElements(details);
        return convertToNSArray(details);
    }

    if ([attributeName isEqualToString:NSAccessibilityRelativeFrameAttribute])
        return [NSValue valueWithRect:(NSRect)backingObject->relativeFrame()];

    if ([attributeName isEqualToString:@"AXErrorMessageElements"]) {
        AccessibilityObject::AccessibilityChildrenVector errorMessages;
        backingObject->ariaErrorMessageElements(errorMessages);
        return convertToNSArray(errorMessages);
    }

    // Multi-selectable
    if ([attributeName isEqualToString:NSAccessibilityIsMultiSelectableAttribute])
        return [NSNumber numberWithBool:backingObject->isMultiSelectable()];

    // Document attributes
    if ([attributeName isEqualToString:NSAccessibilityDocumentURIAttribute])
        return backingObject->documentURI();

    if ([attributeName isEqualToString:NSAccessibilityDocumentEncodingAttribute])
        return backingObject->documentEncoding();

    // Aria controls element
    if ([attributeName isEqualToString:NSAccessibilityAriaControlsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector ariaControls;
        backingObject->ariaControlsElements(ariaControls);
        return convertToNSArray(ariaControls);
    }

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
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    auto focusedObject = backingObject->focusedUIElement();
    return focusedObject ? focusedObject->wrapper() : nil;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    backingObject->updateChildrenIfNecessary();
    AXCoreObject* axObject = backingObject->accessibilityHitTest(IntPoint(point));

    if (axObject) {
        if (axObject->isAttachment() && [axObject->wrapper() attachmentView])
            return [axObject->wrapper() attachmentView];
        return NSAccessibilityUnignoredAncestor(axObject->wrapper());
    }
    return NSAccessibilityUnignoredAncestor(self);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NO;

    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return YES;

    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return backingObject->canSetFocusAttribute();

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return backingObject->canSetValueAttribute();

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return backingObject->canSetSelectedAttribute();

    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute])
        return backingObject->canSetSelectedChildrenAttribute();

    if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute]
        || [attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        return backingObject->canSetExpandedAttribute();

    if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute])
        return YES;

    if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]
        || [attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]
        || [attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
        return backingObject->canSetTextRangeAttributes();

    if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        return YES;

    if (backingObject->isWebArea()
        && ([attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute]
            || [attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute]))
        return YES;

    return NO;
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
- (NSArray* )accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    if (backingObject->isAttachment())
        return nil;

    static NSArray* paramAttrs = nil;
    static NSArray* textParamAttrs = nil;
    static NSArray* tableParamAttrs = nil;
    static NSArray* webAreaParamAttrs = nil;
    if (paramAttrs == nil) {
        paramAttrs = [[NSArray alloc] initWithObjects:
            @"AXUIElementForTextMarker",
            @"AXTextMarkerRangeForUIElement",
            @"AXLineForTextMarker",
            @"AXTextMarkerRangeForLine",
            @"AXStringForTextMarkerRange",
            @"AXTextMarkerForPosition",
            @"AXBoundsForTextMarkerRange",
            @"AXAttributedStringForTextMarkerRange",
            @"AXAttributedStringForTextMarkerRangeWithOptions",
            @"AXTextMarkerRangeForUnorderedTextMarkers",
            @"AXNextTextMarkerForTextMarker",
            @"AXPreviousTextMarkerForTextMarker",
            @"AXLeftWordTextMarkerRangeForTextMarker",
            @"AXRightWordTextMarkerRangeForTextMarker",
            @"AXLeftLineTextMarkerRangeForTextMarker",
            @"AXRightLineTextMarkerRangeForTextMarker",
            @"AXSentenceTextMarkerRangeForTextMarker",
            @"AXParagraphTextMarkerRangeForTextMarker",
            @"AXNextWordEndTextMarkerForTextMarker",
            @"AXPreviousWordStartTextMarkerForTextMarker",
            @"AXNextLineEndTextMarkerForTextMarker",
            @"AXPreviousLineStartTextMarkerForTextMarker",
            @"AXNextSentenceEndTextMarkerForTextMarker",
            @"AXPreviousSentenceStartTextMarkerForTextMarker",
            @"AXNextParagraphEndTextMarkerForTextMarker",
            @"AXPreviousParagraphStartTextMarkerForTextMarker",
            @"AXStyleTextMarkerRangeForTextMarker",
            @"AXLengthForTextMarkerRange",
            NSAccessibilityBoundsForRangeParameterizedAttribute,
            NSAccessibilityStringForRangeParameterizedAttribute,
            NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute,
            NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
            NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
            NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
            NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute,
            NSAccessibilitySelectTextWithCriteriaParameterizedAttribute,
            NSAccessibilitySearchTextWithCriteriaParameterizedAttribute,
            NSAccessibilityTextOperationParameterizedAttribute,
            nil];
    }

    if (textParamAttrs == nil) {
        NSMutableArray* tempArray = [[NSMutableArray alloc] initWithArray:paramAttrs];
        [tempArray addObject:(NSString*)kAXLineForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForLineParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForPositionParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXBoundsForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRTFForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXAttributedStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStyleRangeForIndexParameterizedAttribute];
        textParamAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableParamAttrs == nil) {
        NSMutableArray* tempArray = [[NSMutableArray alloc] initWithArray:paramAttrs];
        [tempArray addObject:NSAccessibilityCellForColumnAndRowParameterizedAttribute];
        tableParamAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (!webAreaParamAttrs) {
        NSMutableArray* tempArray = [[NSMutableArray alloc] initWithArray:paramAttrs];
        [tempArray addObject:NSAccessibilityTextMarkerForIndexParameterizedAttribute];
        [tempArray addObject:NSAccessibilityTextMarkerIsValidParameterizedAttribute];
        [tempArray addObject:NSAccessibilityIndexForTextMarkerParameterizedAttribute];
        webAreaParamAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }

    if (backingObject->isPasswordField())
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
    dispatch_async(dispatch_get_main_queue(), ^ {
        [self _accessibilityPerformPressAction];
    });
}

- (void)_accessibilityPerformPressAction
{
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
    dispatch_async(dispatch_get_main_queue(), ^{
        [self _accessibilityPerformIncrementAction];
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
    dispatch_async(dispatch_get_main_queue(), ^{
        [self _accessibilityPerformDecrementAction];
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
    if (self.axBackingObject->roleValue() == AccessibilityRole::ComboBox)
        self.axBackingObject->setIsExpanded(true);
    else {
        // This needs to be performed in an iteration of the run loop that did not start from an AX call.
        // If it's the same run loop iteration, the menu open notification won't be sent
        [self performSelector:@selector(accessibilityShowContextMenu) withObject:nil afterDelay:0.0];
    }
}

- (void)accessibilityShowContextMenu
{
    Accessibility::performFunctionOnMainThread([protectedSelf = retainPtr(self)] {
        [protectedSelf _accessibilityShowContextMenu];
    });
}

- (void)_accessibilityShowContextMenu
{
    ASSERT(isMainThread());
    
    if (!self.axBackingObject)
        return;
    
    Page* page = self.axBackingObject->page();
    if (!page)
        return;
    
    IntRect rect = snappedIntRect(self.axBackingObject->elementRect());
    FrameView* frameView = self.axBackingObject->documentFrameView();
    
    // On WK2, we need to account for the scroll position with regards to root view.
    // On WK1, we need to convert rect to window space to match mouse clicking.
    if (frameView) {
        // Find the appropriate scroll view to use to convert the contents to the window.
        for (auto* parent = self.axBackingObject->parentObject(); parent; parent = parent->parentObject()) {
            if (parent->isScrollView()) {
                if (auto* scrollView = parent->scrollView()) {
                    if (!frameView->platformWidget())
                        rect = scrollView->contentsToRootView(rect);
                    else
                        rect = scrollView->contentsToWindow(rect);
                }
                break;
            }
        }
    }

    page->contextMenuController().showContextMenuAt(page->mainFrame(), rect.center());
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
    auto* backingObject = self.updateObjectBackingStore;
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
        backingObject->performDismissAction();
}

- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NO;

    return backingObject->replaceTextInRange(String(string).isolatedCopy(), PlainTextRange(range));
}

- (BOOL)accessibilityInsertText:(NSString *)text
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NO;

    return backingObject->insertText(String(text).isolatedCopy());
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

- (void)_accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return;

    id textMarkerRange = nil;
    NSNumber*               number = nil;
    NSString*               string = nil;
    NSRange                 range = {0, 0};
    NSArray*                array = nil;

    // decode the parameter
    if (AXObjectIsTextMarkerRange(value))
        textMarkerRange = value;
    else if ([value isKindOfClass:[NSNumber class]])
        number = value;
    else if ([value isKindOfClass:[NSString class]])
        string = value;
    else if ([value isKindOfClass:[NSValue class]])
        range = [value rangeValue];
    else if ([value isKindOfClass:[NSArray class]])
        array = value;

    // handle the command
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        ASSERT(textMarkerRange);
        Accessibility::performFunctionOnMainThread([&textMarkerRange, protectedSelf = retainPtr(self)] {
            if (auto* backingObject = protectedSelf.get().axBackingObject)
                backingObject->setSelectedVisiblePositionRange([protectedSelf visiblePositionRangeForTextMarkerRange:textMarkerRange]);
        });
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        [self baseAccessibilitySetFocus:[number boolValue]];
    } else if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (number && backingObject->canSetNumericValue())
            backingObject->setValue([number floatValue]);
        else if (string)
            backingObject->setValue(string);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute]) {
        if (!number)
            return;
        backingObject->setSelected([number boolValue]);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (!array)
            return;
        if (!backingObject->isNativeListBox())
            return;
        AccessibilityObject::AccessibilityChildrenVector selectedChildren;
        convertToVector(array, selectedChildren);
        downcast<AccessibilityListBox>(*backingObject).setSelectedChildren(selectedChildren);
    } else if (backingObject->isTextControl()) {
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            backingObject->setSelectedText(string);
        } else if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            backingObject->setSelectedTextRange(PlainTextRange(range.location, range.length));
        } else if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute]) {
            backingObject->makeRangeVisible(PlainTextRange(range.location, range.length));
        }
    } else if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute] || [attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        backingObject->setIsExpanded([number boolValue]);
    else if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector selectedRows;
        convertToVector(array, selectedRows);
        if (backingObject->isTree() || (backingObject->isTable() && backingObject->isExposable()))
            backingObject->setSelectedRows(selectedRows);
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
    Frame* frame = [frameView _web_frame];
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
- (NSAttributedString *)doAXAttributedStringForRange:(NSRange)range
{
    auto webRange = self.axBackingObject->rangeForPlainTextRange(range);
    return [self doAXAttributedStringForTextMarkerRange:[self textMarkerRangeFromRange:webRange] spellCheck:YES];
}

// FIXME: No reason for this to be a method instead of a function; can get document from range.
- (NSRange)_convertToNSRange:(const SimpleRange&)range
{
    ASSERT(isMainThread());
    
    auto document = self.axBackingObject->document();
    if (!document)
        return NSMakeRange(NSNotFound, 0);

    auto documentElement = document->documentElement();
    if (!documentElement)
        return NSMakeRange(NSNotFound, 0);

    return characterRange(makeBoundaryPointBeforeNodeContents(*documentElement), range);
}

- (NSInteger)_indexForTextMarker:(id)marker
{
    if (!marker)
        return NSNotFound;
    
    if (AXObjectCache* cache = self.axBackingObject->axObjectCache()) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:marker];
        auto range = cache->rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
        if (!range)
            return NSNotFound;
        return [self _convertToNSRange:*range].location;
    }
    return NSNotFound;
}

- (id)_textMarkerForIndex:(NSInteger)textIndex
{
    ASSERT(isMainThread());

    auto* document = self.axBackingObject->document();
    if (!document)
        return nil;

    auto* documentElement = document->documentElement();
    if (!document)
        return nil;

    auto* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;

    auto boundary = resolveCharacterLocation(makeRangeSelectingNodeContents(*documentElement), textIndex);
    auto characterOffset = cache->startOrEndCharacterOffsetForRange(makeSimpleRange(boundary), true);
    return [self textMarkerForCharacterOffset:characterOffset];
}

// The RTF representation of the text associated with this accessibility object that is
// specified by the given range.
- (NSData*)doAXRTFForRange:(NSRange)range
{
    NSAttributedString* attrString = [self doAXAttributedStringForRange:range];
    return [attrString RTFFromRange: NSMakeRange(0, [attrString length]) documentAttributes:@{ }];
}

#if ENABLE(TREE_DEBUGGING)
- (NSString *)debugDescriptionForTextMarker:(id)textMarker
{
    char description[1024];
    [self visiblePositionForTextMarker:textMarker].formatForDebugger(description, sizeof(description));
    return [NSString stringWithUTF8String:description];

}

- (NSString *)debugDescriptionForTextMarkerRange:(id)textMarkerRange
{
    VisiblePositionRange visiblePositionRange = [self visiblePositionRangeForTextMarkerRange:textMarkerRange];
    if (visiblePositionRange.isNull())
        return @"<null>";
    char description[2048];
    formatForDebugger(visiblePositionRange, description, sizeof(description));
    return [NSString stringWithUTF8String:description];

}

- (void)showNodeForTextMarker:(id)textMarker
{
    VisiblePosition visiblePosition = [self visiblePositionForTextMarker:textMarker];
    Node* node = visiblePosition.deepEquivalent().deprecatedNode();
    if (!node)
        return;
    node->showNode();
    node->showNodePathForThis();
}

- (void)showNodeTreeForTextMarker:(id)textMarker
{
    VisiblePosition visiblePosition = [self visiblePositionForTextMarker:textMarker];
    Node* node = visiblePosition.deepEquivalent().deprecatedNode();
    if (!node)
        return;
    node->showTreeForThis();
}

static void formatForDebugger(const VisiblePositionRange& range, char* buffer, unsigned length)
{
    StringBuilder result;
    
    const int FormatBufferSize = 1024;
    char format[FormatBufferSize];
    result.appendLiteral("from ");
    range.start.formatForDebugger(format, FormatBufferSize);
    result.append(format);
    result.appendLiteral(" to ");
    range.end.formatForDebugger(format, FormatBufferSize);
    result.append(format);
    
    strlcpy(buffer, result.toString().utf8().data(), length);
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

- (id)textMarkerRangeAtTextMarker:(id)textMarker forUnit:(TextUnit)textUnit
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        auto characterOffset = characterOffsetForTextMarker(cache, textMarker);
        Optional<SimpleRange> range;
        switch (textUnit) {
        case TextUnit::LeftWord:
            range = cache->leftWordRange(characterOffset);
            break;
        case TextUnit::RightWord:
            range = cache->rightWordRange(characterOffset);
            break;
        case TextUnit::Sentence:
            range = cache->sentenceForCharacterOffset(characterOffset);
            break;
        case TextUnit::Paragraph:
            range = cache->paragraphForCharacterOffset(characterOffset);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return textMarkerRangeFromRange(cache, range);
    });
}

- (id)lineTextMarkerRangeForTextMarker:(id)textMarker forUnit:(TextUnit)textUnit
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        auto visiblePosition = visiblePositionForTextMarker(cache, textMarker);
        VisiblePositionRange visiblePositionRange;
        switch (textUnit) {
        case TextUnit::Line:
            visiblePositionRange = backingObject->lineRangeForPosition(visiblePosition);
            break;
        case TextUnit::LeftLine:
            visiblePositionRange = backingObject->leftLineVisiblePositionRange(visiblePosition);
            break;
        case TextUnit::RightLine:
            visiblePositionRange = backingObject->rightLineVisiblePositionRange(visiblePosition);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return textMarkerRangeFromVisiblePositions(cache, visiblePositionRange.start, visiblePositionRange.end);
    });
}

- (id)textMarkerForTextMarker:(id)textMarker atUnit:(TextUnit)textUnit
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, &textUnit, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
        auto* backingObject = protectedSelf.get().axBackingObject;
        if (!backingObject)
            return nil;

        auto* cache = backingObject->axObjectCache();
        if (!cache)
            return nil;

        CharacterOffset oldOffset = characterOffsetForTextMarker(cache, textMarker);
        CharacterOffset newOffset;
        switch (textUnit) {
        case TextUnit::NextWordEnd:
            newOffset = cache->nextWordEndCharacterOffset(oldOffset);
            break;
        case TextUnit::PreviousWordStart:
            newOffset = cache->previousWordStartCharacterOffset(oldOffset);
            break;
        case TextUnit::NextSentenceEnd:
            newOffset = cache->nextSentenceEndCharacterOffset(oldOffset);
            break;
        case TextUnit::PreviousSentenceStart:
            newOffset = cache->previousSentenceStartCharacterOffset(oldOffset);
            break;
        case TextUnit::NextParagraphEnd:
            newOffset = cache->nextParagraphEndCharacterOffset(oldOffset);
            break;
        case TextUnit::PreviousParagraphStart:
            newOffset = cache->previousParagraphStartCharacterOffset(oldOffset);
            break;
        case TextUnit::NextLineEnd: {
            auto visiblePosition = visiblePositionForTextMarker(cache, textMarker);
            return textMarkerForVisiblePosition(cache, backingObject->nextLineEndPosition(visiblePosition));
        }
        case TextUnit::PreviousLineStart: {
            auto visiblePosition = visiblePositionForTextMarker(cache, textMarker);
            return textMarkerForVisiblePosition(cache, backingObject->previousLineStartPosition(visiblePosition));
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        return textMarkerForCharacterOffset(cache, newOffset);
    });
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityAttributeValue:", String(attribute)));
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    // Basic parameter validation.
    if (!attribute || !parameter)
        return nil;

    id textMarker = nil;
    id textMarkerRange = nil;
    NSNumber* number = nil;
    NSArray* array = nil;
    NSDictionary* dictionary = nil;
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
        textMarker = parameter;
    else if (AXObjectIsTextMarkerRange(parameter))
        textMarkerRange = parameter;
    else if ([parameter isKindOfClass:[WebAccessibilityObjectWrapper class]])
        uiElement = [(WebAccessibilityObjectWrapper*)parameter axBackingObject];
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
            return createNSArray(ranges, [&] (auto& range) {
                return [protectedSelf textMarkerRangeFromRange:range];
            }).autorelease();
        });
    }

    if ([attribute isEqualToString:NSAccessibilityTextOperationParameterizedAttribute]) {
        auto operationResult = Accessibility::retrieveValueFromMainThread<Vector<String>>([dictionary, protectedSelf = retainPtr(self)] () -> Vector<String> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return Vector<String>();

            auto textOperation = accessibilityTextOperationForParameterizedAttribute(protectedSelf.get(), dictionary);
            return backingObject->performTextOperation(textOperation);
        });
        if (operationResult.isEmpty())
            return nil;
        return createNSArray(operationResult).autorelease();
    }

    if ([attribute isEqualToString:NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute]) {
        AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(dictionary);
        AccessibilityObject::AccessibilityChildrenVector results;
        backingObject->findMatchingObjects(&criteria, results);
        return @(results.size());
    }

    if ([attribute isEqualToString:NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute]) {
        AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(dictionary);
        AccessibilityObject::AccessibilityChildrenVector results;
        backingObject->findMatchingObjects(&criteria, results);
        return convertToNSArray(results);
    }

    if ([attribute isEqualToString:NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&rect, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            IntRect webCoreRect = [protectedSelf screenToContents:enclosingIntRect(rect)];
            CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, false);
            return [protectedSelf textMarkerForCharacterOffset:characterOffset];
        });
    }

    if ([attribute isEqualToString:NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&rect, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            IntRect webCoreRect = [protectedSelf screenToContents:enclosingIntRect(rect)];
            CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, true);
            return [protectedSelf textMarkerForCharacterOffset:characterOffset];
        });
    }

    if ([attribute isEqualToString:NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::Line];

    if ([attribute isEqualToString:NSAccessibilityMisspellingTextMarkerRangeParameterizedAttribute]) {
        auto criteria = accessibilityMisspellingSearchCriteriaForParameterizedAttribute(self, dictionary);
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&criteria, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            if (auto misspellingRange = backingObject->misspellingRange(*criteria.first, criteria.second))
                return [protectedSelf textMarkerRangeFromRange:*misspellingRange];
            return nil;
        });
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerIsValidParameterizedAttribute]) {
        bool result = Accessibility::retrieveValueFromMainThread<bool>([&textMarker, protectedSelf = retainPtr(self)] () -> bool {
            VisiblePosition pos = [protectedSelf visiblePositionForTextMarker:textMarker];
            return !pos.isNull();
        });
        return [NSNumber numberWithBool:result];
    }

    if ([attribute isEqualToString:NSAccessibilityIndexForTextMarkerParameterizedAttribute]) {
        auto result = Accessibility::retrieveValueFromMainThread<NSInteger>([&textMarker, protectedSelf = retainPtr(self)] () -> NSInteger {
            return [protectedSelf _indexForTextMarker:textMarker];
        });
        return [NSNumber numberWithInteger:result];
    }

    if ([attribute isEqualToString:NSAccessibilityTextMarkerForIndexParameterizedAttribute]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&number, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            return [protectedSelf _textMarkerForIndex:[number integerValue]];
        });
    }

    if ([attribute isEqualToString:@"AXUIElementForTextMarker"]) {
        // FIXME: we need to return an IsolatedObject if this is called on the AX thread.
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* axObject = [protectedSelf accessibilityObjectForTextMarker:textMarker];
            if (!axObject)
                return nil;
            if (axObject->isAttachment() && [axObject->wrapper() attachmentView])
                return [axObject->wrapper() attachmentView];
            return axObject->wrapper();
        });
    }

    if ([attribute isEqualToString:@"AXTextMarkerRangeForUIElement"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&uiElement, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            return [protectedSelf textMarkerRangeFromRange:uiElement.get()->elementRange()];
        });
    }

    if ([attribute isEqualToString:@"AXLineForTextMarker"]) {
        int result = Accessibility::retrieveValueFromMainThread<int>([&textMarker, protectedSelf = retainPtr(self)] () -> int {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return -1;

            VisiblePosition visiblePos = [protectedSelf visiblePositionForTextMarker:textMarker];
            return backingObject->lineForPosition(visiblePos);
        });
        return @(result);
    }

    if ([attribute isEqualToString:@"AXTextMarkerRangeForLine"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&number, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            VisiblePositionRange vpRange;
            if ([number unsignedIntegerValue] != NSNotFound)
                vpRange = backingObject->visiblePositionRangeForLine([number unsignedIntValue]);
            return textMarkerRangeFromVisiblePositions(backingObject->axObjectCache(), vpRange.start, vpRange.end);
        });
    }

    if ([attribute isEqualToString:@"AXStringForTextMarkerRange"]) {
        return Accessibility::retrieveValueFromMainThread<String>([&textMarkerRange, protectedSelf = retainPtr(self)] () -> String {
            auto range = [protectedSelf rangeForTextMarkerRange:textMarkerRange];
            auto* backingObject = protectedSelf.get().axBackingObject;
            return (range && backingObject) ? backingObject->stringForRange(*range) : String();
        });
    }

    if ([attribute isEqualToString:@"AXTextMarkerForPosition"]) {
        IntPoint webCorePoint = IntPoint(point);
        if (!pointSet)
            return nil;

        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&webCorePoint, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            CharacterOffset characterOffset = cache->characterOffsetForPoint(webCorePoint, backingObject);
            return [protectedSelf textMarkerForCharacterOffset:characterOffset];
        });
    }

    if ([attribute isEqualToString:@"AXBoundsForTextMarkerRange"]) {
        NSRect rect = Accessibility::retrieveValueFromMainThread<NSRect>([&textMarkerRange, protectedSelf = retainPtr(self)] () -> NSRect {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return CGRectZero;
            auto range = [protectedSelf rangeForTextMarkerRange:textMarkerRange];
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
            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return CGRectZero;
            CharacterOffset start = cache->characterOffsetForIndex(range.location, backingObject);
            CharacterOffset end = cache->characterOffsetForIndex(range.location+range.length, backingObject);
            auto range = cache->rangeForUnorderedCharacterOffsets(start, end);
            if (!range)
                return CGRectZero;
            auto bounds = FloatRect(backingObject->boundsForRange(*range));
            return [protectedSelf convertRectToSpace:bounds space:AccessibilityConversionSpace::Screen];
        });
        return [NSValue valueWithRect:rect];
    }

    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute]) {
        if (backingObject->isTextControl()) {
            PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);
            return backingObject->doAXStringForRange(plainTextRange);
        }

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

    if ([attribute isEqualToString:@"AXAttributedStringForTextMarkerRange"])
        return [self doAXAttributedStringForTextMarkerRange:textMarkerRange spellCheck:YES];

    if ([attribute isEqualToString:@"AXAttributedStringForTextMarkerRangeWithOptions"]) {
        if (textMarkerRange)
            return [self doAXAttributedStringForTextMarkerRange:textMarkerRange spellCheck:NO];
        if (dictionary) {
            id textMarkerRange = nil;
            id parameter = [dictionary objectForKey:@"AXTextMarkerRange"];
            if (AXObjectIsTextMarkerRange(parameter))
                textMarkerRange = parameter;
            BOOL spellCheck = NO;
            parameter = [dictionary objectForKey:@"AXSpellCheck"];
            if ([parameter isKindOfClass:[NSNumber class]])
                spellCheck = [parameter boolValue];
            return [self doAXAttributedStringForTextMarkerRange:textMarkerRange spellCheck:spellCheck];
        }
        return nil;
    }

    if ([attribute isEqualToString:@"AXTextMarkerRangeForUnorderedTextMarkers"]) {
        if ([array count] < 2)
            return nil;

        id textMarker1 = [array objectAtIndex:0];
        id textMarker2 = [array objectAtIndex:1];
        if (!AXObjectIsTextMarker(textMarker1) || !AXObjectIsTextMarker(textMarker2))
            return nil;

        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker1, &textMarker2, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            CharacterOffset characterOffset1 = [protectedSelf characterOffsetForTextMarker:textMarker1];
            CharacterOffset characterOffset2 = [protectedSelf characterOffsetForTextMarker:textMarker2];
            auto range = cache->rangeForUnorderedCharacterOffsets(characterOffset1, characterOffset2);
            return [protectedSelf textMarkerRangeFromRange:range];
        });
    }

    if ([attribute isEqualToString:@"AXNextTextMarkerForTextMarker"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            CharacterOffset characterOffset = [protectedSelf characterOffsetForTextMarker:textMarker];
            return [protectedSelf nextTextMarkerForCharacterOffset:characterOffset];
        });
    }

    if ([attribute isEqualToString:@"AXPreviousTextMarkerForTextMarker"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            CharacterOffset characterOffset = [protectedSelf characterOffsetForTextMarker:textMarker];
            return [protectedSelf previousTextMarkerForCharacterOffset:characterOffset];
        });
    }

    if ([attribute isEqualToString:@"AXLeftWordTextMarkerRangeForTextMarker"])
        return [self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::LeftWord];

    if ([attribute isEqualToString:@"AXRightWordTextMarkerRangeForTextMarker"])
        return [self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::RightWord];

    if ([attribute isEqualToString:@"AXLeftLineTextMarkerRangeForTextMarker"])
        return [self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::LeftLine];

    if ([attribute isEqualToString:@"AXRightLineTextMarkerRangeForTextMarker"])
        return [self lineTextMarkerRangeForTextMarker:textMarker forUnit:TextUnit::RightLine];

    if ([attribute isEqualToString:@"AXSentenceTextMarkerRangeForTextMarker"])
        return [self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::Sentence];

    if ([attribute isEqualToString:@"AXParagraphTextMarkerRangeForTextMarker"])
        return [self textMarkerRangeAtTextMarker:textMarker forUnit:TextUnit::Paragraph];

    if ([attribute isEqualToString:@"AXNextWordEndTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextWordEnd];

    if ([attribute isEqualToString:@"AXPreviousWordStartTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousWordStart];

    if ([attribute isEqualToString:@"AXNextLineEndTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextLineEnd];

    if ([attribute isEqualToString:@"AXPreviousLineStartTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousLineStart];

    if ([attribute isEqualToString:@"AXNextSentenceEndTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextSentenceEnd];

    if ([attribute isEqualToString:@"AXPreviousSentenceStartTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousSentenceStart];

    if ([attribute isEqualToString:@"AXNextParagraphEndTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::NextParagraphEnd];

    if ([attribute isEqualToString:@"AXPreviousParagraphStartTextMarkerForTextMarker"])
        return [self textMarkerForTextMarker:textMarker atUnit:TextUnit::PreviousParagraphStart];

    if ([attribute isEqualToString:@"AXStyleTextMarkerRangeForTextMarker"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarker, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return nil;

            auto* cache = backingObject->axObjectCache();
            if (!cache)
                return nil;

            VisiblePosition visiblePos = visiblePositionForTextMarker(cache, textMarker);
            VisiblePositionRange vpRange = backingObject->styleRangeForPosition(visiblePos);
            return textMarkerRangeFromVisiblePositions(cache, vpRange.start, vpRange.end);
        });
    }

    if ([attribute isEqualToString:@"AXLengthForTextMarkerRange"]) {
        int length = Accessibility::retrieveValueFromMainThread<int>([&textMarkerRange, protectedSelf = retainPtr(self)] () -> int {
            auto* backingObject = protectedSelf.get().axBackingObject;
            if (!backingObject)
                return 0;

            auto range = [protectedSelf rangeForTextMarkerRange:textMarkerRange];
            if (!range)
                return 0;
            return AXObjectCache::lengthForRange(SimpleRange { *range });
        });
        if (length < 0)
            return nil;
        return @(length);
    }

    // Used only by DumpRenderTree (so far).
    if ([attribute isEqualToString:@"AXStartTextMarkerForTextMarkerRange"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarkerRange, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto range = [protectedSelf rangeForTextMarkerRange:textMarkerRange];
            return [protectedSelf startOrEndTextMarkerForRange:range isStart:YES];
        });
    }

    if ([attribute isEqualToString:@"AXEndTextMarkerForTextMarkerRange"]) {
        return Accessibility::retrieveAutoreleasedValueFromMainThread<id>([&textMarkerRange, protectedSelf = retainPtr(self)] () -> RetainPtr<id> {
            auto range = [protectedSelf rangeForTextMarkerRange:textMarkerRange];
            return [protectedSelf startOrEndTextMarkerForRange:range isStart:NO];
        });
    }

#if ENABLE(TREE_DEBUGGING)
    if ([attribute isEqualToString:@"AXTextMarkerDebugDescription"])
        return [self debugDescriptionForTextMarker:textMarker];

    if ([attribute isEqualToString:@"AXTextMarkerRangeDebugDescription"])
        return [self debugDescriptionForTextMarkerRange:textMarkerRange];

    if ([attribute isEqualToString:@"AXTextMarkerNodeDebugDescription"]) {
        [self showNodeForTextMarker:textMarker];
        return nil;
    }

    if ([attribute isEqualToString:@"AXTextMarkerNodeTreeDebugDescription"]) {
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

        if ([attribute isEqualToString: (NSString *)kAXRangeForLineParameterizedAttribute]) {
            PlainTextRange textRange = backingObject->doAXRangeForLine([number intValue]);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXStringForRangeParameterizedAttribute]) {
            PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);
            return rangeSet ? (id)(backingObject->doAXStringForRange(plainTextRange)) : nil;
        }

        if ([attribute isEqualToString: (NSString*)kAXRangeForPositionParameterizedAttribute]) {
            if (!pointSet)
                return nil;
            IntPoint webCorePoint = IntPoint(point);
            PlainTextRange textRange = backingObject->doAXRangeForPosition(webCorePoint);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXRangeForIndexParameterizedAttribute]) {
            PlainTextRange textRange = backingObject->doAXRangeForIndex([number intValue]);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXBoundsForRangeParameterizedAttribute]) {
            if (!rangeSet)
                return nil;
            PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);
            auto bounds = FloatRect(backingObject->doAXBoundsForRangeUsingCharacterOffset(plainTextRange));
            NSRect rect = [self convertRectToSpace:bounds space:AccessibilityConversionSpace::Screen];
            return [NSValue valueWithRect:rect];
        }

        if ([attribute isEqualToString: (NSString*)kAXRTFForRangeParameterizedAttribute])
            return rangeSet ? [self doAXRTFForRange:range] : nil;

        if ([attribute isEqualToString: (NSString*)kAXAttributedStringForRangeParameterizedAttribute])
            return rangeSet ? [self doAXAttributedStringForRange:range] : nil;

        if ([attribute isEqualToString: (NSString*)kAXStyleRangeForIndexParameterizedAttribute]) {
            PlainTextRange textRange = backingObject->doAXStyleRangeForIndex([number intValue]);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
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

- (BOOL)accessibilityShouldUseUniqueId
{
    // All AX object wrappers should use unique ID's because it's faster within AppKit to look them up.
    return YES;
}

// API that AppKit uses for faster access
- (NSUInteger)accessibilityIndexOfChild:(id)child
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return NSNotFound;

    // Tree objects return their rows as their children. We can use the original method
    // here, because we won't gain any speed up.
    if (backingObject->isTree())
        return [super accessibilityIndexOfChild:child];

    NSArray *children = self.childrenVectorArray;
    if (!children.count)
        return [[self renderWidgetChildren] indexOfObject:child];

    NSUInteger count = [children count];
    for (NSUInteger i = 0; i < count; ++i) {
        WebAccessibilityObjectWrapper *wrapper = children[i];
        auto* object = wrapper.axBackingObject;
        if (!object)
            continue;

        if (wrapper == child || (object->isAttachment() && [wrapper attachmentView] == child))
            return i;
    }

    return NSNotFound;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return 0;

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        // Tree items object returns a different set of children than those that are in children()
        // because an AXOutline (the mac role is becomes) has some odd stipulations.
        if (backingObject->isTree() || backingObject->isTreeItem())
            return [[self accessibilityAttributeValue:NSAccessibilityChildrenAttribute] count];

        auto childrenSize = self.childrenVectorSize;
        if (!childrenSize)
            return [[self renderWidgetChildren] count];

        return childrenSize;
    }

    return [super accessibilityArrayAttributeCount:attribute];
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount
{
    AXTRACE(makeString("WebAccessibilityObjectWrapper accessibilityArrayAttributeValue:", String(attribute)));
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nil;

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        if (!self.childrenVectorSize) {
            NSArray *children = [self renderWidgetChildren];
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
            BOOL isAttachment = [wrapper isKindOfClass:[WebAccessibilityObjectWrapper class]] && wrapper.axBackingObject && wrapper.axBackingObject->isAttachment() && [wrapper attachmentView];
            [subarray addObject:isAttachment ? [wrapper attachmentView] : wrapper];
        }

        return subarray;
    }

    return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
}

@end

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(MAC)
