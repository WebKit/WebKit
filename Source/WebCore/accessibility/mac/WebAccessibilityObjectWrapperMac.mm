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

#if HAVE(ACCESSIBILITY) && PLATFORM(MAC)

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
#import <pal/spi/mac/HIServicesSPI.h>
#import <pal/spi/mac/NSAccessibilitySPI.h>
#import <wtf/ObjCRuntimeExtras.h>
#if ENABLE(TREE_DEBUGGING) || ENABLE(METER_ELEMENT)
#import <wtf/text/StringBuilder.h>
#endif

using namespace WebCore;
using namespace HTMLNames;

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

#define _axBackingObject self.axBackingObject

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
    ASSERT(m_object->isAttachment());
    Widget* widget = m_object->widgetForAttachmentView();
    if (!widget)
        return nil;
    return NSAccessibilityUnignoredDescendant(widget->platformWidget());
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

static id AXTextMarkerRange(id startMarker, id endMarker)
{
    ASSERT(startMarker != nil);
    ASSERT(endMarker != nil);
    ASSERT(CFGetTypeID((__bridge CFTypeRef)startMarker) == AXTextMarkerGetTypeID());
    ASSERT(CFGetTypeID((__bridge CFTypeRef)endMarker) == AXTextMarkerGetTypeID());
    return CFBridgingRelease(AXTextMarkerRangeCreate(kCFAllocatorDefault, (AXTextMarkerRef)startMarker, (AXTextMarkerRef)endMarker));
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
    Document* document = m_object->document();
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

static AccessibilitySelectTextCriteria accessibilitySelectTextCriteriaForCriteriaParameterizedAttribute(const NSDictionary *parameterizedAttribute)
{
    NSString *activityParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextActivity];
    NSString *ambiguityResolutionParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextAmbiguityResolution];
    NSString *replacementStringParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextReplacementString];
    NSArray *searchStringsParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextSearchStrings];
    
    AccessibilitySelectTextActivity activity = AccessibilitySelectTextActivity::FindAndSelect;
    if ([activityParameter isKindOfClass:[NSString class]]) {
        if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndReplace])
            activity = AccessibilitySelectTextActivity::FindAndReplace;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndCapitalize])
            activity = AccessibilitySelectTextActivity::FindAndCapitalize;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndLowercase])
            activity = AccessibilitySelectTextActivity::FindAndLowercase;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndUppercase])
            activity = AccessibilitySelectTextActivity::FindAndUppercase;
    }
    
    AccessibilitySelectTextAmbiguityResolution ambiguityResolution = AccessibilitySelectTextAmbiguityResolution::ClosestTo;
    if ([ambiguityResolutionParameter isKindOfClass:[NSString class]]) {
        if ([ambiguityResolutionParameter isEqualToString:NSAccessibilitySelectTextAmbiguityResolutionClosestAfterSelection])
            ambiguityResolution = AccessibilitySelectTextAmbiguityResolution::ClosestAfter;
        else if ([ambiguityResolutionParameter isEqualToString:NSAccessibilitySelectTextAmbiguityResolutionClosestBeforeSelection])
            ambiguityResolution = AccessibilitySelectTextAmbiguityResolution::ClosestBefore;
    }
    
    String replacementString;
    if ([replacementStringParameter isKindOfClass:[NSString class]])
        replacementString = replacementStringParameter;
    
    AccessibilitySelectTextCriteria criteria(activity, ambiguityResolution, replacementString);
    
    if ([searchStringsParameter isKindOfClass:[NSArray class]]) {
        size_t searchStringsCount = static_cast<size_t>([searchStringsParameter count]);
        criteria.searchStrings.reserveInitialCapacity(searchStringsCount);
        for (NSString *searchString in searchStringsParameter) {
            if ([searchString isKindOfClass:[NSString class]])
                criteria.searchStrings.uncheckedAppend(searchString);
        }
    }
    
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
    return accessibilityObjectForTextMarker(m_object->axObjectCache(), textMarker);
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

- (id)textMarkerRangeFromRange:(const RefPtr<Range>)range
{
    return textMarkerRangeFromRange(m_object->axObjectCache(), range);
}

static id textMarkerRangeFromRange(AXObjectCache* cache, const RefPtr<Range> range)
{
    id startTextMarker = startOrEndTextmarkerForRange(cache, range, true);
    id endTextMarker = startOrEndTextmarkerForRange(cache, range, false);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

- (id)startOrEndTextMarkerForRange:(const RefPtr<Range>)range isStart:(BOOL)isStart
{
    return startOrEndTextmarkerForRange(m_object->axObjectCache(), range, isStart);
}

static id startOrEndTextmarkerForRange(AXObjectCache* cache, RefPtr<Range> range, bool isStart)
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->startOrEndTextMarkerDataForRange(textMarkerData, range, isStart);
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
    return nextTextMarkerForCharacterOffset(m_object->axObjectCache(), characterOffset);
}

- (id)previousTextMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    return previousTextMarkerForCharacterOffset(m_object->axObjectCache(), characterOffset);
}

- (id)textMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    return textMarkerForCharacterOffset(m_object->axObjectCache(), characterOffset);
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

- (RefPtr<Range>)rangeForTextMarkerRange:(id)textMarkerRange
{
    if (!textMarkerRange)
        return nullptr;
    
    id startTextMarker = AXTextMarkerRangeStart(textMarkerRange);
    id endTextMarker = AXTextMarkerRangeEnd(textMarkerRange);
    
    if (!startTextMarker || !endTextMarker)
        return nullptr;
    
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nullptr;
    
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
    return characterOffsetForTextMarker(m_object->axObjectCache(), (__bridge CFTypeRef)textMarker);
}

static id textMarkerForVisiblePosition(AXObjectCache* cache, const VisiblePosition& visiblePos)
{
    ASSERT(cache);
    if (!cache)
        return nil;
    
    auto textMarkerData = cache->textMarkerDataForVisiblePosition(visiblePos);
    if (!textMarkerData)
        return nil;

    return CFBridgingRelease(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&textMarkerData.value(), sizeof(textMarkerData.value())));
}

- (id)textMarkerForVisiblePosition:(const VisiblePosition &)visiblePos
{
    return textMarkerForVisiblePosition(m_object->axObjectCache(), visiblePos);
}

- (id)textMarkerForFirstPositionInTextControl:(HTMLTextFormControlElement &)textControl
{
    auto *cache = m_object->axObjectCache();
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

- (VisiblePosition)visiblePositionForTextMarker:(id)textMarker
{
    return visiblePositionForTextMarker(m_object->axObjectCache(), (__bridge CFTypeRef)textMarker);
}

static VisiblePosition visiblePositionForStartOfTextMarkerRange(AXObjectCache* cache, id textMarkerRange)
{
    return visiblePositionForTextMarker(cache, (__bridge CFTypeRef)AXTextMarkerRangeStart(textMarkerRange));
}

static VisiblePosition visiblePositionForEndOfTextMarkerRange(AXObjectCache* cache, id textMarkerRange)
{
    return visiblePositionForTextMarker(cache, (__bridge CFTypeRef)AXTextMarkerRangeEnd(textMarkerRange));
}

static id textMarkerRangeFromMarkers(id textMarker1, id textMarker2)
{
    if (!textMarker1 || !textMarker2)
        return nil;
    
    return AXTextMarkerRange(textMarker1, textMarker2);
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
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:(-1)], range);
    else if (alignment == VerticalAlign::Super)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:1], range);
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
        if (node->hasTagName(markTag))
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
        [attrString addAttribute:NSAccessibilityBlockQuoteLevelAttribute value:[NSNumber numberWithInt:quoteLevel] range:range];
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
        TextCheckerClient* checker = node->document().frame()->editor().textChecker();
        
        // checkTextOfParagraph is the only spelling/grammar checker implemented in WK1 and WK2
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(*checker, text, TextCheckingType::Spelling, results, node->document().frame()->selection().selection());
        
        size_t size = results.size();
        for (unsigned i = 0; i < size; i++) {
            const TextCheckingResult& result = results[i];
            AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, @YES, NSMakeRange(result.location + range.location, result.length));
#if PLATFORM(MAC)
            AXAttributeStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, @YES, NSMakeRange(result.location + range.location, result.length));
#endif
        }
        return;
    }

    for (unsigned currentPosition = 0; currentPosition < text.length(); ) {
        int misspellingLocation = -1;
        int misspellingLength = 0;
        node->document().frame()->editor().textChecker()->checkSpellingOfString(text.substring(currentPosition), &misspellingLocation, &misspellingLength);
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
        [attrString addAttribute:@"AXHeadingLevel" value:[NSNumber numberWithInt:parentHeadingLevel] range:range];
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

- (NSAttributedString*)doAXAttributedStringForTextMarkerRange:(id)textMarkerRange spellCheck:(BOOL)spellCheck
{
    if (!m_object)
        return nil;
    
    RefPtr<Range> range = [self rangeForTextMarkerRange:textMarkerRange];
    NSMutableAttributedString* attrString = [[NSMutableAttributedString alloc] init];
    TextIterator it(range.get());
    while (!it.atEnd()) {
        // locate the node and starting offset for this range
        Node& node = it.range()->startContainer();
        ASSERT(&node == &it.range()->endContainer());
        int offset = it.range()->startOffset();
        
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length()) {
            // Add the text of the list marker item if necessary.
            String listMarkerText = m_object->listMarkerTextForNodeAndPosition(&node, VisiblePosition(it.range()->startPosition()));
            if (!listMarkerText.isEmpty())
                AXAttributedStringAppendText(attrString, &node, listMarkerText, spellCheck);
            AXAttributedStringAppendText(attrString, &node, it.text(), spellCheck);
        } else {
            Node* replacedNode = node.traverseToChildAt(offset);
            NSString *attachmentString = nsStringForReplacedNode(replacedNode);
            if (attachmentString) {
                NSRange attrStringRange = NSMakeRange([attrString length], [attachmentString length]);
                
                // append the placeholder string
                [[attrString mutableString] appendString:attachmentString];
                
                // remove all inherited attributes
                [attrString setAttributes:nil range:attrStringRange];
                
                // add the attachment attribute
                AccessibilityObject* obj = replacedNode->renderer()->document().axObjectCache()->getOrCreate(replacedNode->renderer());
                AXAttributeStringSetElement(attrString, NSAccessibilityAttachmentTextAttribute, obj, attrStringRange);
            }
        }
        it.advance();
    }

    return [attrString autorelease];
}

static id textMarkerRangeFromVisiblePositions(AXObjectCache* cache, const VisiblePosition& startPosition, const VisiblePosition& endPosition)
{
    if (!cache)
        return nil;
    
    id startTextMarker = textMarkerForVisiblePosition(cache, startPosition);
    id endTextMarker = textMarkerForVisiblePosition(cache, endPosition);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

- (id)textMarkerRangeFromVisiblePositions:(const VisiblePosition&)startPosition endPosition:(const VisiblePosition&)endPosition
{
    return textMarkerRangeFromVisiblePositions(m_object->axObjectCache(), startPosition, endPosition);
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray*)accessibilityActionNames
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return nil;
    
    // All elements should get ShowMenu and ScrollToVisible.
    // But certain earlier VoiceOver versions do not support scroll to visible, and it confuses them to see it in the list.
    static NSArray *defaultElementActions = [[NSArray alloc] initWithObjects:NSAccessibilityShowMenuAction, NSAccessibilityScrollToVisibleAction, nil];

    // Action elements allow Press.
    // The order is important to VoiceOver, which expects the 'default' action to be the first action. In this case the default action should be press.
    static NSArray *actionElementActions = [[NSArray alloc] initWithObjects:NSAccessibilityPressAction, NSAccessibilityShowMenuAction, NSAccessibilityScrollToVisibleAction, nil];

    // Menu elements allow Press and Cancel.
    static NSArray *menuElementActions = [[actionElementActions arrayByAddingObject:NSAccessibilityCancelAction] retain];

    // Slider elements allow Increment/Decrement.
    static NSArray *sliderActions = [[defaultElementActions arrayByAddingObjectsFromArray:[NSArray arrayWithObjects:NSAccessibilityIncrementAction, NSAccessibilityDecrementAction, nil]] retain];
    
    NSArray *actions;
    if (m_object->supportsPressAction())
        actions = actionElementActions;
    else if (m_object->isMenuRelated())
        actions = menuElementActions;
    else if (m_object->isSlider())
        actions = sliderActions;
    else if (m_object->isAttachment())
        actions = [[self attachmentView] accessibilityActionNames];
    else
        actions = defaultElementActions;
    
    return actions;
}

- (NSArray*)additionalAccessibilityAttributeNames
{
    if (!m_object)
        return nil;
    
    NSMutableArray *additional = [NSMutableArray array];
    if (m_object->supportsARIAOwns())
        [additional addObject:NSAccessibilityOwnsAttribute];
    
    if (m_object->isToggleButton())
        [additional addObject:NSAccessibilityValueAttribute];
    
    if (m_object->supportsExpanded() || m_object->isSummary())
        [additional addObject:NSAccessibilityExpandedAttribute];
    
    if (m_object->isScrollbar()
        || m_object->isRadioGroup()
        || m_object->isSplitter()
        || m_object->isToolbar())
        [additional addObject:NSAccessibilityOrientationAttribute];
    
    if (m_object->supportsARIADragging())
        [additional addObject:NSAccessibilityGrabbedAttribute];
    
    if (m_object->supportsARIADropping())
        [additional addObject:NSAccessibilityDropEffectsAttribute];
    
    if (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility() && downcast<AccessibilityTable>(*m_object).supportsSelectedRows())
        [additional addObject:NSAccessibilitySelectedRowsAttribute];
    
    if (m_object->supportsLiveRegion()) {
        [additional addObject:NSAccessibilityARIALiveAttribute];
        [additional addObject:NSAccessibilityARIARelevantAttribute];
    }
    
    if (m_object->supportsSetSize())
        [additional addObject:NSAccessibilityARIASetSizeAttribute];
    if (m_object->supportsPosInSet())
        [additional addObject:NSAccessibilityARIAPosInSetAttribute];
    
    AccessibilitySortDirection sortDirection = m_object->sortDirection();
    if (sortDirection != AccessibilitySortDirection::None && sortDirection != AccessibilitySortDirection::Invalid)
        [additional addObject:NSAccessibilitySortDirectionAttribute];
    
    // If an object is a child of a live region, then add these
    if (m_object->isInsideLiveRegion())
        [additional addObject:NSAccessibilityARIAAtomicAttribute];
    // All objects should expose the ARIA busy attribute (ARIA 1.1 with ISSUE-538).
    [additional addObject:NSAccessibilityElementBusyAttribute];
    
    // Popup buttons on the Mac expose the value attribute.
    if (m_object->isPopUpButton()) {
        [additional addObject:NSAccessibilityValueAttribute];
    }

    if (m_object->supportsDatetimeAttribute())
        [additional addObject:NSAccessibilityDatetimeValueAttribute];
    
    if (m_object->supportsRequiredAttribute()) {
        [additional addObject:NSAccessibilityRequiredAttribute];
    }
    
    if (m_object->hasPopup())
        [additional addObject:NSAccessibilityHasPopupAttribute];
    
    if (m_object->isMathRoot()) {
        // The index of a square root is always known, so there's no object associated with it.
        if (!m_object->isMathSquareRoot())
            [additional addObject:NSAccessibilityMathRootIndexAttribute];
        [additional addObject:NSAccessibilityMathRootRadicandAttribute];
    } else if (m_object->isMathFraction()) {
        [additional addObject:NSAccessibilityMathFractionNumeratorAttribute];
        [additional addObject:NSAccessibilityMathFractionDenominatorAttribute];
        [additional addObject:NSAccessibilityMathLineThicknessAttribute];
    } else if (m_object->isMathSubscriptSuperscript()) {
        [additional addObject:NSAccessibilityMathBaseAttribute];
        [additional addObject:NSAccessibilityMathSubscriptAttribute];
        [additional addObject:NSAccessibilityMathSuperscriptAttribute];
    } else if (m_object->isMathUnderOver()) {
        [additional addObject:NSAccessibilityMathBaseAttribute];
        [additional addObject:NSAccessibilityMathUnderAttribute];
        [additional addObject:NSAccessibilityMathOverAttribute];
    } else if (m_object->isMathFenced()) {
        [additional addObject:NSAccessibilityMathFencedOpenAttribute];
        [additional addObject:NSAccessibilityMathFencedCloseAttribute];
    } else if (m_object->isMathMultiscript()) {
        [additional addObject:NSAccessibilityMathBaseAttribute];
        [additional addObject:NSAccessibilityMathPrescriptsAttribute];
        [additional addObject:NSAccessibilityMathPostscriptsAttribute];
    }
    
    if (m_object->supportsPath())
        [additional addObject:NSAccessibilityPathAttribute];
    
    if (m_object->supportsExpandedTextValue())
        [additional addObject:NSAccessibilityExpandedTextValueAttribute];
    
    return additional;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray*)accessibilityAttributeNames
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return nil;
    
    if (m_object->isAttachment())
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
            NSAccessibilityRelativeFrameAttribute,
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
    
    if (m_object->isPasswordField())
        objectAttributes = passwordFieldAttrs;
    
    else if (m_object->isWebArea())
        objectAttributes = webAreaAttrs;
    
    else if (m_object->isTextControl())
        objectAttributes = textAttrs;
    
    else if (_axBackingObject->isLink() || _axBackingObject->isImage())
        objectAttributes = anchorAttrs;
    
    else if (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility())
        objectAttributes = tableAttrs;
    else if (m_object->isTableColumn())
        objectAttributes = tableColAttrs;
    else if (m_object->isTableCell())
        objectAttributes = tableCellAttrs;
    else if (m_object->isTableRow()) {
        // An ARIA table row can be collapsed and expanded, so it needs the extra attributes.
        if (m_object->isARIATreeGridRow())
            objectAttributes = outlineRowAttrs;
        else
            objectAttributes = tableRowAttrs;
    }
    
    else if (m_object->isTree())
        objectAttributes = outlineAttrs;
    else if (m_object->isTreeItem())
        objectAttributes = outlineRowAttrs;
    
    else if (m_object->isListBox())
        objectAttributes = listBoxAttrs;
    else if (m_object->isList())
        objectAttributes = listAttrs;
    
    else if (m_object->isComboBox())
        objectAttributes = comboBoxAttrs;
    
    else if (m_object->isProgressIndicator() || m_object->isSlider())
        objectAttributes = rangeAttrs;
    
    // These are processed in order because an input image is a button, and a button is a control.
    else if (m_object->isInputImage())
        objectAttributes = inputImageAttrs;
    else if (m_object->isButton())
        objectAttributes = buttonAttrs;
    else if (m_object->isControl())
        objectAttributes = controlAttrs;
    
    else if (m_object->isGroup() || m_object->isListItem())
        objectAttributes = groupAttrs;
    else if (m_object->isTabList())
        objectAttributes = tabListAttrs;
    else if (m_object->isScrollView())
        objectAttributes = scrollViewAttrs;
    else if (m_object->isSpinButton())
        objectAttributes = incrementorAttrs;
    
    else if (m_object->isMenu())
        objectAttributes = menuAttrs;
    else if (m_object->isMenuBar())
        objectAttributes = menuBarAttrs;
    else if (m_object->isMenuButton())
        objectAttributes = menuButtonAttrs;
    else if (m_object->isMenuItem())
        objectAttributes = menuItemAttrs;
    
    NSArray *additionalAttributes = [self additionalAccessibilityAttributeNames];
    if ([additionalAttributes count])
        objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:additionalAttributes];
    
    // Only expose AXARIACurrent attribute when the element is set to be current item.
    if (m_object->currentState() != AccessibilityCurrentState::False)
        objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:@[ NSAccessibilityARIACurrentAttribute ]];
    
    // AppKit needs to know the screen height in order to do the coordinate conversion.
    objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:@[ NSAccessibilityPrimaryScreenHeightAttribute ]];
    
    return objectAttributes;
}

- (VisiblePositionRange)visiblePositionRangeForTextMarkerRange:(id)textMarkerRange
{
    if (!textMarkerRange)
        return VisiblePositionRange();
    AXObjectCache* cache = m_object->axObjectCache();
    return VisiblePositionRange(visiblePositionForStartOfTextMarkerRange(cache, textMarkerRange), visiblePositionForEndOfTextMarkerRange(cache, textMarkerRange));
}

- (NSArray*)renderWidgetChildren
{
    Widget* widget = m_object->widget();
    if (!widget)
        return nil;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [(widget->platformWidget()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (id)remoteAccessibilityParentObject
{
    if (!m_object)
        return nil;
    
    Document* document = m_object->document();
    if (!document)
        return nil;
    
    Frame* frame = document->frame();
    if (!frame)
        return nil;
    
    return frame->loader().client().accessibilityRemoteObject();
}

static void convertToVector(NSArray* array, AccessibilityObject::AccessibilityChildrenVector& vector)
{
    unsigned length = [array count];
    vector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        AccessibilityObject* obj = [[array objectAtIndex:i] accessibilityObject];
        if (obj)
            vector.append(obj);
    }
}

static NSMutableArray *convertStringsToNSArray(const Vector<String>& vector)
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:vector.size()];
    for (const auto& string : vector)
        [array addObject:string];
    return array;
}

- (id)textMarkerRangeForSelection
{
    VisibleSelection selection = m_object->selection();
    if (selection.isNone())
        return nil;
    return [self textMarkerRangeFromVisiblePositions:selection.visibleStart() endPosition:selection.visibleEnd()];
}

- (id)associatedPluginParent
{
    if (!m_object || !m_object->hasAttribute(x_apple_pdf_annotationAttr))
        return nil;
    
    if (!m_object->document()->isPluginDocument())
        return nil;
        
    Widget* pluginWidget = static_cast<PluginDocument*>(m_object->document())->pluginWidget();
    if (!pluginWidget || !pluginWidget->isPluginViewBase())
        return nil;

    return static_cast<PluginViewBase*>(pluginWidget)->accessibilityAssociatedPluginParentForElement(m_object->element());
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
    Path path = m_object->elementPath();
    if (path.isEmpty())
        return NULL;
    
    CGPathRef transformedPath = [self convertPathToScreenSpace:path];
    return [self bezierPathFromPath:transformedPath];
}

- (NSNumber *)primaryScreenHeight
{
    FloatRect screenRect = screenRectForPrimaryScreen();
    return [NSNumber numberWithFloat:screenRect.height()];
}

- (size_t)childrenVectorSize
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (_AXUIElementRequestServicedBySecondaryAXThread())
        return self.isolatedTreeNode->children().size();
#endif
    
    return m_object->children().size();
}

- (NSArray<WebAccessibilityObjectWrapper *> *)childrenVectorArray
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (_AXUIElementRequestServicedBySecondaryAXThread()) {
        auto treeNode = self.isolatedTreeNode;
        auto nodeChildren = treeNode->children();
        Vector<RefPtr<AXIsolatedTreeNode>> children;
        children.reserveInitialCapacity(nodeChildren.size());
        auto tree = treeNode->tree();
        for (auto childID : nodeChildren)
            children.uncheckedAppend(tree->nodeForID(child));
        return (NSArray *)convertToNSArray(children);
    }
#endif
    return (NSArray *)convertToNSArray(m_object->children());
}

- (NSValue *)position
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (_AXUIElementRequestServicedBySecondaryAXThread())
        return [NSValue valueWithPoint:(NSPoint)_axBackingObject->relativeFrame().location()];
#endif
        
    auto rect = snappedIntRect(m_object->elementRect());
    
    // The Cocoa accessibility API wants the lower-left corner.
    auto floatPoint = FloatPoint(rect.x(), rect.maxY());

    auto floatRect = FloatRect(floatPoint, FloatSize());
    CGPoint cgPoint = [self convertRectToSpace:floatRect space:AccessibilityConversionSpace::Screen].origin;
    return [NSValue valueWithPoint:NSPointFromCGPoint(cgPoint)];
}

using AccessibilityRoleMap = HashMap<int, CFStringRef>;

static AccessibilityRoleMap createAccessibilityRoleMap()
{
    struct RoleEntry {
        AccessibilityRole value;
        __unsafe_unretained NSString *string;
    };
    static const RoleEntry roles[] = {
        { AccessibilityRole::Unknown, NSAccessibilityUnknownRole },
        { AccessibilityRole::Button, NSAccessibilityButtonRole },
        { AccessibilityRole::RadioButton, NSAccessibilityRadioButtonRole },
        { AccessibilityRole::CheckBox, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::Slider, NSAccessibilitySliderRole },
        { AccessibilityRole::TabGroup, NSAccessibilityTabGroupRole },
        { AccessibilityRole::TextField, NSAccessibilityTextFieldRole },
        { AccessibilityRole::StaticText, NSAccessibilityStaticTextRole },
        { AccessibilityRole::TextArea, NSAccessibilityTextAreaRole },
        { AccessibilityRole::ScrollArea, NSAccessibilityScrollAreaRole },
        { AccessibilityRole::PopUpButton, NSAccessibilityPopUpButtonRole },
        { AccessibilityRole::MenuButton, NSAccessibilityMenuButtonRole },
        { AccessibilityRole::Table, NSAccessibilityTableRole },
        { AccessibilityRole::Application, NSAccessibilityApplicationRole },
        { AccessibilityRole::Group, NSAccessibilityGroupRole },
        { AccessibilityRole::TextGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::RadioGroup, NSAccessibilityRadioGroupRole },
        { AccessibilityRole::List, NSAccessibilityListRole },
        { AccessibilityRole::Directory, NSAccessibilityListRole },
        { AccessibilityRole::ScrollBar, NSAccessibilityScrollBarRole },
        { AccessibilityRole::ValueIndicator, NSAccessibilityValueIndicatorRole },
        { AccessibilityRole::Image, NSAccessibilityImageRole },
        { AccessibilityRole::MenuBar, NSAccessibilityMenuBarRole },
        { AccessibilityRole::Menu, NSAccessibilityMenuRole },
        { AccessibilityRole::MenuItem, NSAccessibilityMenuItemRole },
        { AccessibilityRole::MenuItemCheckbox, NSAccessibilityMenuItemRole },
        { AccessibilityRole::MenuItemRadio, NSAccessibilityMenuItemRole },
        { AccessibilityRole::Column, NSAccessibilityColumnRole },
        { AccessibilityRole::Row, NSAccessibilityRowRole },
        { AccessibilityRole::Toolbar, NSAccessibilityToolbarRole },
        { AccessibilityRole::BusyIndicator, NSAccessibilityBusyIndicatorRole },
        { AccessibilityRole::ProgressIndicator, NSAccessibilityProgressIndicatorRole },
        { AccessibilityRole::Window, NSAccessibilityWindowRole },
        { AccessibilityRole::Drawer, NSAccessibilityDrawerRole },
        { AccessibilityRole::SystemWide, NSAccessibilitySystemWideRole },
        { AccessibilityRole::Outline, NSAccessibilityOutlineRole },
        { AccessibilityRole::Incrementor, NSAccessibilityIncrementorRole },
        { AccessibilityRole::Browser, NSAccessibilityBrowserRole },
        { AccessibilityRole::ComboBox, NSAccessibilityComboBoxRole },
        { AccessibilityRole::SplitGroup, NSAccessibilitySplitGroupRole },
        { AccessibilityRole::Splitter, NSAccessibilitySplitterRole },
        { AccessibilityRole::ColorWell, NSAccessibilityColorWellRole },
        { AccessibilityRole::GrowArea, NSAccessibilityGrowAreaRole },
        { AccessibilityRole::Sheet, NSAccessibilitySheetRole },
        { AccessibilityRole::HelpTag, NSAccessibilityHelpTagRole },
        { AccessibilityRole::Matte, NSAccessibilityMatteRole },
        { AccessibilityRole::Ruler, NSAccessibilityRulerRole },
        { AccessibilityRole::RulerMarker, NSAccessibilityRulerMarkerRole },
        { AccessibilityRole::Link, NSAccessibilityLinkRole },
        { AccessibilityRole::DisclosureTriangle, NSAccessibilityDisclosureTriangleRole },
        { AccessibilityRole::Grid, NSAccessibilityTableRole },
        { AccessibilityRole::TreeGrid, NSAccessibilityTableRole },
        { AccessibilityRole::WebCoreLink, NSAccessibilityLinkRole },
        { AccessibilityRole::ImageMapLink, NSAccessibilityLinkRole },
        { AccessibilityRole::ImageMap, @"AXImageMap" },
        { AccessibilityRole::ListMarker, @"AXListMarker" },
        { AccessibilityRole::WebArea, @"AXWebArea" },
        { AccessibilityRole::Heading, @"AXHeading" },
        { AccessibilityRole::ListBox, NSAccessibilityListRole },
        { AccessibilityRole::ListBoxOption, NSAccessibilityStaticTextRole },
        { AccessibilityRole::Cell, NSAccessibilityCellRole },
        { AccessibilityRole::GridCell, NSAccessibilityCellRole },
        { AccessibilityRole::TableHeaderContainer, NSAccessibilityGroupRole },
        { AccessibilityRole::ColumnHeader, NSAccessibilityCellRole },
        { AccessibilityRole::RowHeader, NSAccessibilityCellRole },
        { AccessibilityRole::Definition, NSAccessibilityGroupRole },
        { AccessibilityRole::DescriptionListDetail, NSAccessibilityGroupRole },
        { AccessibilityRole::DescriptionListTerm, NSAccessibilityGroupRole },
        { AccessibilityRole::Term, NSAccessibilityGroupRole },
        { AccessibilityRole::DescriptionList, NSAccessibilityListRole },
        { AccessibilityRole::SliderThumb, NSAccessibilityValueIndicatorRole },
        { AccessibilityRole::WebApplication, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkBanner, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkComplementary, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkDocRegion, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkContentInfo, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkMain, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkNavigation, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkRegion, NSAccessibilityGroupRole },
        { AccessibilityRole::LandmarkSearch, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationAlert, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationAlertDialog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationDialog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationTextGroup, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationLog, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationMarquee, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationStatus, NSAccessibilityGroupRole },
        { AccessibilityRole::ApplicationTimer, NSAccessibilityGroupRole },
        { AccessibilityRole::Document, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentArticle, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentMath, NSAccessibilityGroupRole },
        { AccessibilityRole::DocumentNote, NSAccessibilityGroupRole },
        { AccessibilityRole::UserInterfaceTooltip, NSAccessibilityGroupRole },
        { AccessibilityRole::Tab, NSAccessibilityRadioButtonRole },
        { AccessibilityRole::TabList, NSAccessibilityTabGroupRole },
        { AccessibilityRole::TabPanel, NSAccessibilityGroupRole },
        { AccessibilityRole::Tree, NSAccessibilityOutlineRole },
        { AccessibilityRole::TreeItem, NSAccessibilityRowRole },
        { AccessibilityRole::ListItem, NSAccessibilityGroupRole },
        { AccessibilityRole::Paragraph, NSAccessibilityGroupRole },
        { AccessibilityRole::Label, NSAccessibilityGroupRole },
        { AccessibilityRole::Div, NSAccessibilityGroupRole },
        { AccessibilityRole::Form, NSAccessibilityGroupRole },
        { AccessibilityRole::SpinButton, NSAccessibilityIncrementorRole },
        { AccessibilityRole::Footer, NSAccessibilityGroupRole },
        { AccessibilityRole::ToggleButton, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::Canvas, NSAccessibilityImageRole },
        { AccessibilityRole::SVGRoot, NSAccessibilityGroupRole },
        { AccessibilityRole::Legend, NSAccessibilityGroupRole },
        { AccessibilityRole::MathElement, NSAccessibilityGroupRole },
        { AccessibilityRole::Audio, NSAccessibilityGroupRole },
        { AccessibilityRole::Video, NSAccessibilityGroupRole },
        { AccessibilityRole::HorizontalRule, NSAccessibilitySplitterRole },
        { AccessibilityRole::Blockquote, NSAccessibilityGroupRole },
        { AccessibilityRole::Switch, NSAccessibilityCheckBoxRole },
        { AccessibilityRole::SearchField, NSAccessibilityTextFieldRole },
        { AccessibilityRole::Pre, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyBase, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyBlock, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyInline, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyRun, NSAccessibilityGroupRole },
        { AccessibilityRole::RubyText, NSAccessibilityGroupRole },
        { AccessibilityRole::Details, NSAccessibilityGroupRole },
        { AccessibilityRole::Summary, NSAccessibilityButtonRole },
        { AccessibilityRole::SVGTextPath, NSAccessibilityGroupRole },
        { AccessibilityRole::SVGText, NSAccessibilityGroupRole },
        { AccessibilityRole::SVGTSpan, NSAccessibilityGroupRole },
        { AccessibilityRole::Inline, NSAccessibilityGroupRole },
        { AccessibilityRole::Mark, NSAccessibilityGroupRole },
        { AccessibilityRole::Time, NSAccessibilityGroupRole },
        { AccessibilityRole::Feed, NSAccessibilityGroupRole },
        { AccessibilityRole::Figure, NSAccessibilityGroupRole },
        { AccessibilityRole::Footnote, NSAccessibilityGroupRole },
        { AccessibilityRole::GraphicsDocument, NSAccessibilityGroupRole },
        { AccessibilityRole::GraphicsObject, NSAccessibilityGroupRole },
        { AccessibilityRole::GraphicsSymbol, NSAccessibilityImageRole },
        { AccessibilityRole::Caption, NSAccessibilityGroupRole },
    };
    AccessibilityRoleMap roleMap;
    for (auto& role : roles)
        roleMap.add(static_cast<int>(role.value), (__bridge CFStringRef)role.string);
    return roleMap;
}

static NSString *roleValueToNSString(AccessibilityRole value)
{
    static NeverDestroyed<AccessibilityRoleMap> roleMap = createAccessibilityRoleMap();
    return (__bridge NSString *)roleMap.get().get(static_cast<int>(value));
}

- (NSString*)role
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (_axBackingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END
    AccessibilityRole role = _axBackingObject->roleValue();

    if (role == AccessibilityRole::Label && is<AccessibilityLabel>(*m_object) && downcast<AccessibilityLabel>(*m_object).containsOnlyStaticText())
        role = AccessibilityRole::StaticText;

    // The mfenced element creates anonymous RenderMathMLOperators with no RenderText
    // descendants. Because these anonymous renderers are the only accessible objects
    // containing the operator, assign AccessibilityRole::StaticText.
    if (m_object->isAnonymousMathOperator())
        role = AccessibilityRole::StaticText;

    if (role == AccessibilityRole::Canvas && m_object->canvasHasFallbackContent())
        role = AccessibilityRole::Group;
    NSString* string = roleValueToNSString(role);
    if (string != nil)
        return string;
    return NSAccessibilityUnknownRole;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSString*)subrole
{
    if (m_object->isPasswordField())
        return NSAccessibilitySecureTextFieldSubrole;
    if (m_object->isSearchField())
        return NSAccessibilitySearchFieldSubrole;
    
    if (_axBackingObject->isAttachment()) {
        NSView* attachView = [self attachmentView];
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute])
            return [attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
    }
    
    if (m_object->isMeter())
        return @"AXMeter";
    
    AccessibilityRole role = m_object->roleValue();
    if (role == AccessibilityRole::HorizontalRule)
        return NSAccessibilityContentSeparatorSubrole;
    if (role == AccessibilityRole::ToggleButton)
        return NSAccessibilityToggleSubrole;
    
    if (is<AccessibilitySpinButtonPart>(*m_object)) {
        if (downcast<AccessibilitySpinButtonPart>(*m_object).isIncrementor())
            return NSAccessibilityIncrementArrowSubrole;
        
        return NSAccessibilityDecrementArrowSubrole;
    }
    
    if (_axBackingObject->isFileUploadButton())
        return @"AXFileUploadButton";
    
    if (m_object->isTreeItem())
        return NSAccessibilityOutlineRowSubrole;
    
    if (m_object->isFieldset())
        return @"AXFieldset";
    
    if (is<AccessibilityList>(*m_object)) {
        auto& listObject = downcast<AccessibilityList>(*m_object);
        if (listObject.isUnorderedList() || listObject.isOrderedList())
            return NSAccessibilityContentListSubrole;
        if (listObject.isDescriptionList()) {
            return NSAccessibilityDescriptionListSubrole;
        }
    }
    
    // ARIA content subroles.
    switch (role) {
    case AccessibilityRole::LandmarkBanner:
        return @"AXLandmarkBanner";
    case AccessibilityRole::LandmarkComplementary:
        return @"AXLandmarkComplementary";
    // Footer roles should appear as content info types.
    case AccessibilityRole::Footer:
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
        if (m_object->isMathFraction())
            return @"AXMathFraction";
        if (m_object->isMathFenced())
            return @"AXMathFenced";
        if (m_object->isMathSubscriptSuperscript())
            return @"AXMathSubscriptSuperscript";
        if (m_object->isMathRow())
            return @"AXMathRow";
        if (m_object->isMathUnderOver())
            return @"AXMathUnderOver";
        if (m_object->isMathSquareRoot())
            return @"AXMathSquareRoot";
        if (m_object->isMathRoot())
            return @"AXMathRoot";
        if (m_object->isMathText())
            return @"AXMathText";
        if (m_object->isMathNumber())
            return @"AXMathNumber";
        if (m_object->isMathIdentifier())
            return @"AXMathIdentifier";
        if (m_object->isMathTable())
            return @"AXMathTable";
        if (m_object->isMathTableRow())
            return @"AXMathTableRow";
        if (m_object->isMathTableCell())
            return @"AXMathTableCell";
        if (m_object->isMathFenceOperator())
            return @"AXMathFenceOperator";
        if (m_object->isMathSeparatorOperator())
            return @"AXMathSeparatorOperator";
        if (m_object->isMathOperator())
            return @"AXMathOperator";
        if (m_object->isMathMultiscript())
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

    if (m_object->isMediaTimeline())
        return NSAccessibilityTimelineSubrole;

    if (m_object->isSwitch())
        return NSAccessibilitySwitchSubrole;

    if (m_object->isStyleFormatGroup()) {
        if (Node* node = m_object->node()) {
            if (node->hasTagName(kbdTag))
                return @"AXKeyboardInputStyleGroup";
            if (node->hasTagName(codeTag))
                return @"AXCodeStyleGroup";
            if (node->hasTagName(preTag))
                return @"AXPreformattedStyleGroup";
            if (node->hasTagName(sampTag))
                return @"AXSampleStyleGroup";
            if (node->hasTagName(varTag))
                return @"AXVariableStyleGroup";
            if (node->hasTagName(citeTag))
                return @"AXCiteStyleGroup";
            if (node->hasTagName(insTag))
                return @"AXInsertStyleGroup";
            if (node->hasTagName(delTag))
                return @"AXDeleteStyleGroup";
            if (node->hasTagName(supTag))
                return @"AXSuperscriptStyleGroup";
            if (node->hasTagName(subTag))
                return @"AXSubscriptStyleGroup";
        }
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
    if (!m_object)
        return nil;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // attachments have the AXImage role, but a different subrole
    if (_axBackingObject->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END

    const AtomicString& overrideRoleDescription = m_object->roleDescription();
    if (!overrideRoleDescription.isNull() && !overrideRoleDescription.isEmpty())
        return overrideRoleDescription;
    
    NSString* axRole = [self role];
    
    if ([axRole isEqualToString:NSAccessibilityGroupRole]) {
        
        if (m_object->isOutput())
            return AXOutputText();
        
        NSString *ariaLandmarkRoleDescription = [self ariaLandmarkRoleDescription];
        if (ariaLandmarkRoleDescription)
            return ariaLandmarkRoleDescription;
        
        switch (m_object->roleValue()) {
        case AccessibilityRole::Audio:
            return localizedMediaControlElementString("AudioElement");
        case AccessibilityRole::Definition:
            return AXDefinitionText();
        case AccessibilityRole::DescriptionListTerm:
        case AccessibilityRole::Term:
            return AXDescriptionListTermText();
        case AccessibilityRole::DescriptionListDetail:
            return AXDescriptionListDetailText();
        case AccessibilityRole::Details:
            return AXDetailsText();
        case AccessibilityRole::Feed:
            return AXFeedText();
        case AccessibilityRole::Figure:
            return AXFigureText();
        case AccessibilityRole::Footer:
            return AXFooterRoleDescriptionText();
        case AccessibilityRole::Mark:
            return AXMarkText();
        case AccessibilityRole::Video:
            return localizedMediaControlElementString("VideoElement");
        case AccessibilityRole::GraphicsDocument:
            return AXARIAContentGroupText(@"ARIADocument");
        default:
            return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, [self subrole]);
        }
    }
    
    if ([axRole isEqualToString:@"AXWebArea"])
        return AXWebAreaText();
    
    if ([axRole isEqualToString:@"AXLink"])
        return AXLinkText();
    
    if ([axRole isEqualToString:@"AXListMarker"])
        return AXListMarkerText();
    
    if ([axRole isEqualToString:@"AXImageMap"])
        return AXImageMapText();
    
    if ([axRole isEqualToString:@"AXHeading"])
        return AXHeadingText();
    
    if ([axRole isEqualToString:NSAccessibilityTextFieldRole]) {
        auto* node = m_object->node();
        if (is<HTMLInputElement>(node)) {
            auto& input = downcast<HTMLInputElement>(*node);
            if (input.isEmailField())
                return AXEmailFieldText();
            if (input.isTelephoneField())
                return AXTelephoneFieldText();
            if (input.isURLField())
                return AXURLFieldText();
            if (input.isNumberField())
                return AXNumberFieldText();
            
            // These input types are not enabled on mac yet, we check the type attribute for now.
            auto& type = input.attributeWithoutSynchronization(typeAttr);
            if (equalLettersIgnoringASCIICase(type, "date"))
                return AXDateFieldText();
            if (equalLettersIgnoringASCIICase(type, "time"))
                return AXTimeFieldText();
            if (equalLettersIgnoringASCIICase(type, "week"))
                return AXWeekFieldText();
            if (equalLettersIgnoringASCIICase(type, "month"))
                return AXMonthFieldText();
            if (equalLettersIgnoringASCIICase(type, "datetime-local"))
                return AXDateTimeFieldText();
        }
    }
    
    if (_axBackingObject->isFileUploadButton())
        return AXFileUploadButtonText();
    
    // Only returning for DL (not UL or OL) because description changed with HTML5 from 'definition list' to
    // superset 'description list' and does not return the same values in AX API on some OS versions. 
    if (is<AccessibilityList>(*m_object)) {
        if (downcast<AccessibilityList>(*m_object).isDescriptionList())
            return AXDescriptionListText();
    }
    
    if (m_object->roleValue() == AccessibilityRole::HorizontalRule)
        return AXHorizontalRuleDescriptionText();
    
    // AppKit also returns AXTab for the role description for a tab item.
    if (m_object->isTabItem())
        return NSAccessibilityRoleDescription(@"AXTab", nil);
    
    if (m_object->isSummary())
        return AXSummaryText();
    
    // We should try the system default role description for all other roles.
    // If we get the same string back, then as a last resort, return unknown.
    NSString* defaultRoleDescription = NSAccessibilityRoleDescription(axRole, [self subrole]);
    
    // On earlier Mac versions (Lion), using a non-standard subrole would result in a role description
    // being returned that looked like AXRole:AXSubrole. To make all platforms have the same role descriptions
    // we should fallback on a role description ignoring the subrole in these cases.
    if ([defaultRoleDescription isEqualToString:[NSString stringWithFormat:@"%@:%@", axRole, [self subrole]]])
        defaultRoleDescription = NSAccessibilityRoleDescription(axRole, nil);
    
    if (![defaultRoleDescription isEqualToString:axRole])
        return defaultRoleDescription;
    
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
}

- (NSString *)computedRoleString
{
    if (!m_object)
        return nil;
    return m_object->computedRoleString();
}

- (id)scrollViewParent
{
    if (!is<AccessibilityScrollView>(m_object))
        return nil;
    
    // If this scroll view provides it's parent object (because it's a sub-frame), then
    // we should not find the remoteAccessibilityParent.
    if (m_object->parentObject())
        return nil;
    
    ScrollView* scroll = downcast<AccessibilityScrollView>(*m_object).scrollView();
    if (!scroll)
        return nil;
    
    if (scroll->platformWidget())
        return NSAccessibilityUnignoredAncestor(scroll->platformWidget());
    
    return [self remoteAccessibilityParentObject];
}

- (NSString *)valueDescriptionForMeter
{
    if (!m_object)
        return nil;
    
    String valueDescription = m_object->valueDescription();
#if ENABLE(METER_ELEMENT)
    if (!is<AccessibilityProgressIndicator>(m_object))
        return valueDescription;
    auto &meter = downcast<AccessibilityProgressIndicator>(*m_object);
    String gaugeRegionValue = meter.gaugeRegionValueDescription();
    if (!gaugeRegionValue.isEmpty()) {
        StringBuilder builder;
        builder.append(valueDescription);
        if (builder.length())
            builder.appendLiteral(", ");
        builder.append(gaugeRegionValue);
        return builder.toString();
    }
#endif
    return valueDescription;
}

// FIXME: split up this function in a better way.
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString*)attributeName
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return nil;
    
    if (m_object->isDetachedFromParent())
        return nil;
    
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
        if (_axBackingObject->isTreeItem()) {
            auto parent = _axBackingObject->parentObjectInterfaceUnignored();
            while (parent) {
                if (parent->isTree())
                    return parent->wrapper();
                parent = parent->parentObjectInterfaceUnignored();
            }
        }
        
        auto parent = _axBackingObject->parentObjectInterfaceUnignored();
        if (!parent)
            return nil;
        
        // In WebKit1, the scroll view is provided by the system (the attachment view), so the parent
        // should be reported directly as such.
        if (m_object->isWebArea() && parent->isAttachment())
            return [parent->wrapper() attachmentView];
        
        return parent->wrapper();
    }
    
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        if (!self.childrenVectorSize) {
            NSArray* children = [self renderWidgetChildren];
            if (children != nil)
                return children;
        }
        
        // The tree's (AXOutline) children are supposed to be its rows and columns.
        // The ARIA spec doesn't have columns, so we just need rows.
        if (m_object->isTree())
            return [self accessibilityAttributeValue:NSAccessibilityRowsAttribute];
        
        // A tree item should only expose its content as its children (not its rows)
        if (m_object->isTreeItem()) {
            AccessibilityObject::AccessibilityChildrenVector contentCopy;
            m_object->ariaTreeItemContent(contentCopy);
            return (NSArray *)convertToNSArray(contentCopy);
        }
        
        return self.childrenVectorArray;
    }
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (m_object->canHaveSelectedChildren()) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return (NSArray *)convertToNSArray(selectedChildrenCopy);
        }
        return nil;
    }
    
    if ([attributeName isEqualToString: NSAccessibilityVisibleChildrenAttribute]) {
        if (m_object->isListBox()) {
            AccessibilityObject::AccessibilityChildrenVector visibleChildrenCopy;
            m_object->visibleChildren(visibleChildrenCopy);
            return (NSArray *)convertToNSArray(visibleChildrenCopy);
        }
        else if (m_object->isList())
            return [self accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
        
        return nil;
    }
    
    
    if (m_object->isWebArea()) {
        if ([attributeName isEqualToString:@"AXLinkUIElements"]) {
            AccessibilityObject::AccessibilityChildrenVector links;
            downcast<AccessibilityRenderObject>(*m_object).getDocumentLinks(links);
            return (NSArray *)convertToNSArray(links);
        }
        if ([attributeName isEqualToString:@"AXLoaded"])
            return [NSNumber numberWithBool:m_object->isLoaded()];
        if ([attributeName isEqualToString:@"AXLayoutCount"])
            return [NSNumber numberWithInt:m_object->layoutCount()];
        if ([attributeName isEqualToString:NSAccessibilityLoadingProgressAttribute])
            return [NSNumber numberWithDouble:m_object->estimatedLoadingProgress()];
        if ([attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
            return [NSNumber numberWithBool:m_object->preventKeyboardDOMEventDispatch()];
        if ([attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
            return [NSNumber numberWithBool:m_object->caretBrowsingEnabled()];
        if ([attributeName isEqualToString:NSAccessibilityWebSessionIDAttribute]) {
            if (Document* doc = m_object->topDocument())
                return [NSNumber numberWithUnsignedLongLong:doc->sessionID().sessionID()];
        }
    }
    
    if (m_object->isTextControl()) {
        if ([attributeName isEqualToString: NSAccessibilityNumberOfCharactersAttribute]) {
            int length = m_object->textLength();
            if (length < 0)
                return nil;
            return [NSNumber numberWithUnsignedInt:length];
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            String selectedText = m_object->selectedText();
            if (selectedText.isNull())
                return nil;
            return (NSString*)selectedText;
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            PlainTextRange textRange = m_object->selectedTextRange();
            if (textRange.isNull())
                return [NSValue valueWithRange:NSMakeRange(0, 0)];
            return [NSValue valueWithRange:NSMakeRange(textRange.start, textRange.length)];
        }
        // TODO: Get actual visible range. <rdar://problem/4712101>
        if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
            return m_object->isPasswordField() ? nil : [NSValue valueWithRange: NSMakeRange(0, m_object->textLength())];
        if ([attributeName isEqualToString: NSAccessibilityInsertionPointLineNumberAttribute]) {
            // if selectionEnd > 0, then there is selected text and this question should not be answered
            if (m_object->isPasswordField() || m_object->selectionEnd() > 0)
                return nil;
            
            auto focusedObject = downcast<AccessibilityObject>(m_object->focusedUIElement());
            if (focusedObject != m_object)
                return nil;
            
            VisiblePosition focusedPosition = focusedObject->visiblePositionForIndex(focusedObject->selectionStart(), true);
            int lineNumber = m_object->lineForPosition(focusedPosition);
            if (lineNumber < 0)
                return nil;
            
            return [NSNumber numberWithInt:lineNumber];
        }
    }
    
    if ([attributeName isEqualToString: NSAccessibilityURLAttribute]) {
        URL url = m_object->url();
        if (url.isNull())
            return nil;
        return (NSURL*)url;
    }

    // Only native spin buttons have increment and decrement buttons.
    if (is<AccessibilitySpinButton>(*m_object)) {
        if ([attributeName isEqualToString:NSAccessibilityIncrementButtonAttribute])
            return downcast<AccessibilitySpinButton>(*m_object).incrementButton()->wrapper();
        if ([attributeName isEqualToString:NSAccessibilityDecrementButtonAttribute])
            return downcast<AccessibilitySpinButton>(*m_object).decrementButton()->wrapper();
    }
    
    if ([attributeName isEqualToString: @"AXVisited"])
        return [NSNumber numberWithBool: m_object->isVisited()];
    
    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute]) {
        if (_axBackingObject->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }
        
        // Meter elements should communicate their content via AXValueDescription.
        if (m_object->isMeter())
            return [NSString string];
        
        // Summary element should use its text node as AXTitle.
        if (m_object->isSummary())
            return m_object->textUnderElement();
        
        return [self baseAccessibilityTitle];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute]) {
        if (_axBackingObject->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return [self baseAccessibilityDescription];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (_axBackingObject->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityValueAttribute];
        }
        if (m_object->supportsRangeValue())
            return [NSNumber numberWithFloat:m_object->valueForRange()];
        if (m_object->roleValue() == AccessibilityRole::SliderThumb)
            return [NSNumber numberWithFloat:m_object->parentObject()->valueForRange()];
        if (m_object->isHeading())
            return [NSNumber numberWithInt:m_object->headingLevel()];
        
        if (m_object->isCheckboxOrRadio() || m_object->isMenuItem() || m_object->isSwitch() || m_object->isToggleButton()) {
            switch (m_object->checkboxOrRadioValue()) {
            case AccessibilityButtonState::Off:
                return [NSNumber numberWithInt:0];
            case AccessibilityButtonState::On:
                return [NSNumber numberWithInt:1];
            case AccessibilityButtonState::Mixed:
                return [NSNumber numberWithInt:2];
            }
        }
        
        // radio groups return the selected radio button as the AXValue
        if (m_object->isRadioGroup()) {
            AccessibilityObject* radioButton = m_object->selectedRadioButton();
            if (!radioButton)
                return nil;
            return radioButton->wrapper();
        }
        
        if (m_object->isTabList()) {
            AccessibilityObject* tabItem = m_object->selectedTabItem();
            if (!tabItem)
                return nil;
            return tabItem->wrapper();
        }
        
        if (m_object->isTabItem())
            return [NSNumber numberWithInt:m_object->isSelected()];
        
        if (m_object->isColorWell()) {
            int r, g, b;
            m_object->colorValue(r, g, b);
            return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1", r / 255., g / 255., b / 255.];
        }
        
        return m_object->stringValue();
    }

    if ([attributeName isEqualToString:(NSString *)kAXMenuItemMarkCharAttribute]) {
        const unichar ch = 0x2713; // ✓ used on Mac for selected menu items.
        return (m_object->isChecked()) ? [NSString stringWithCharacters:&ch length:1] : nil;
    }
    
    if ([attributeName isEqualToString: NSAccessibilityMinValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (m_object->ariaRoleAttribute() == AccessibilityRole::ProgressIndicator && !m_object->hasAttribute(aria_valuenowAttr))
            return @0;
        return [NSNumber numberWithFloat:m_object->minValueForRange()];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityMaxValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (m_object->ariaRoleAttribute() == AccessibilityRole::ProgressIndicator && !m_object->hasAttribute(aria_valuenowAttr))
            return @0;
        return [NSNumber numberWithFloat:m_object->maxValueForRange()];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return [self baseAccessibilityHelpText];
    
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [NSNumber numberWithBool: m_object->isFocused()];
    
    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: m_object->isEnabled()];
    
    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute]) {
        IntSize s = snappedIntRect(m_object->elementRect()).size();
        return [NSValue valueWithSize: NSMakeSize(s.width(), s.height())];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityPrimaryScreenHeightAttribute])
        return [self primaryScreenHeight];
    if ([attributeName isEqualToString: NSAccessibilityPositionAttribute])
        return [self position];
    if ([attributeName isEqualToString:NSAccessibilityPathAttribute])
        return [self path];
    
    if ([attributeName isEqualToString: NSAccessibilityWindowAttribute] ||
        [attributeName isEqualToString: NSAccessibilityTopLevelUIElementAttribute]) {
        
        id remoteParent = [self remoteAccessibilityParentObject];
        if (remoteParent)
            return [remoteParent accessibilityAttributeValue:attributeName];
        
        FrameView* fv = m_object->documentFrameView();
        if (fv)
            return [fv->platformWidget() window];
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityAccessKeyAttribute]) {
        AtomicString accessKey = m_object->accessKey();
        if (accessKey.isNull())
            return nil;
        return accessKey;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityLinkRelationshipTypeAttribute])
        return m_object->linkRelValue();
    
    if ([attributeName isEqualToString:NSAccessibilityTabsAttribute]) {
        if (m_object->isTabList()) {
            AccessibilityObject::AccessibilityChildrenVector tabsChildren;
            m_object->tabChildren(tabsChildren);
            return (NSArray *)convertToNSArray(tabsChildren);
        }
    }
    
    if ([attributeName isEqualToString:NSAccessibilityContentsAttribute]) {
        // The contents of a tab list are all the children except the tabs.
        if (m_object->isTabList()) {
            auto children = self.childrenVectorArray;
            AccessibilityObject::AccessibilityChildrenVector tabs;
            m_object->tabChildren(tabs);
            auto tabsChildren = (NSArray *)convertToNSArray(tabs);

            NSMutableArray *contents = [NSMutableArray array];
            for (id childWrapper in children) {
                if ([tabsChildren containsObject:childWrapper])
                    [contents addObject:childWrapper];
            }
            return contents;
        } else if (m_object->isScrollView()) {
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
    
    if (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility()) {
        auto& table = downcast<AccessibilityTable>(*m_object);
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute])
            return (NSArray *)convertToNSArray(table.rows());
        
        if ([attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector visibleRows;
            table.visibleRows(visibleRows);
            return (NSArray *)convertToNSArray(visibleRows);
        }
        
        // TODO: distinguish between visible and non-visible columns
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute] ||
            [attributeName isEqualToString:NSAccessibilityVisibleColumnsAttribute]) {
            return (NSArray *)convertToNSArray(table.columns());
        }
        
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return (NSArray *)convertToNSArray(selectedChildrenCopy);
        }
        
        // HTML tables don't support these
        if ([attributeName isEqualToString:NSAccessibilitySelectedColumnsAttribute] ||
            [attributeName isEqualToString:NSAccessibilitySelectedCellsAttribute])
            return nil;
        
        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector columnHeaders;
            table.columnHeaders(columnHeaders);
            return (NSArray *)convertToNSArray(columnHeaders);
        }
        
        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            AccessibilityObject* headerContainer = table.headerContainer();
            if (headerContainer)
                return headerContainer->wrapper();
            return nil;
        }
        
        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowHeaders;
            table.rowHeaders(rowHeaders);
            return (NSArray *)convertToNSArray(rowHeaders);
        }
        
        if ([attributeName isEqualToString:NSAccessibilityVisibleCellsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector cells;
            table.cells(cells);
            return (NSArray *)convertToNSArray(cells);
        }
        
        if ([attributeName isEqualToString:NSAccessibilityColumnCountAttribute])
            return @(table.columnCount());
        
        if ([attributeName isEqualToString:NSAccessibilityRowCountAttribute])
            return @(table.rowCount());
        
        if ([attributeName isEqualToString:NSAccessibilityARIAColumnCountAttribute])
            return @(table.axColumnCount());
        
        if ([attributeName isEqualToString:NSAccessibilityARIARowCountAttribute])
            return @(table.axRowCount());
    }
    
    if (is<AccessibilityTableColumn>(*m_object)) {
        auto& column = downcast<AccessibilityTableColumn>(*m_object);
        if ([attributeName isEqualToString:NSAccessibilityIndexAttribute])
            return [NSNumber numberWithInt:column.columnIndex()];
        
        // rows attribute for a column is the list of all the elements in that column at each row
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute] ||
            [attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute]) {
            return (NSArray *)convertToNSArray(column.children());
        }
        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            AccessibilityObject* header = column.headerObject();
            if (!header)
                return nil;
            return header->wrapper();
        }
    }
    
    if (is<AccessibilityTableCell>(*m_object)) {
        auto& cell = downcast<AccessibilityTableCell>(*m_object);
        if ([attributeName isEqualToString:NSAccessibilityRowIndexRangeAttribute]) {
            std::pair<unsigned, unsigned> rowRange;
            cell.rowIndexRange(rowRange);
            return [NSValue valueWithRange:NSMakeRange(rowRange.first, rowRange.second)];
        }
        if ([attributeName isEqualToString:NSAccessibilityColumnIndexRangeAttribute]) {
            std::pair<unsigned, unsigned> columnRange;
            cell.columnIndexRange(columnRange);
            return [NSValue valueWithRange:NSMakeRange(columnRange.first, columnRange.second)];
        }
        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector columnHeaders;
            cell.columnHeaders(columnHeaders);
            return (NSArray *)convertToNSArray(columnHeaders);
        }
        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowHeaders;
            cell.rowHeaders(rowHeaders);
            return (NSArray *)convertToNSArray(rowHeaders);
        }
        if ([attributeName isEqualToString:NSAccessibilityARIAColumnIndexAttribute])
            return @(cell.axColumnIndex());
        
        if ([attributeName isEqualToString:NSAccessibilityARIARowIndexAttribute])
            return @(cell.axRowIndex());
    }
    
    if (m_object->isTree()) {
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return (NSArray *)convertToNSArray(selectedChildrenCopy);
        }
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            m_object->ariaTreeRows(rowsCopy);
            return (NSArray *)convertToNSArray(rowsCopy);
        }
        
        // TreeRoles do not support columns, but Mac AX expects to be able to ask about columns at the least.
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute])
            return [NSArray array];
    }
    
    if ([attributeName isEqualToString:NSAccessibilityIndexAttribute]) {
        if (m_object->isTreeItem()) {
            AccessibilityObject* parent = m_object->parentObject();
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
                    return [NSNumber numberWithUnsignedInt:k];
            
            return nil;
        }
        if (is<AccessibilityTableRow>(*m_object)) {
            if ([attributeName isEqualToString:NSAccessibilityIndexAttribute])
                return [NSNumber numberWithInt:downcast<AccessibilityTableRow>(*m_object).rowIndex()];
        }
    }
    
    // The rows that are considered inside this row.
    if ([attributeName isEqualToString:NSAccessibilityDisclosedRowsAttribute]) {
        if (m_object->isTreeItem()) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            m_object->ariaTreeItemDisclosedRows(rowsCopy);
            return (NSArray *)convertToNSArray(rowsCopy);
        } else if (is<AccessibilityARIAGridRow>(*m_object)) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            downcast<AccessibilityARIAGridRow>(*m_object).disclosedRows(rowsCopy);
            return (NSArray *)convertToNSArray(rowsCopy);
        }
    }
    
    // The row that contains this row. It should be the same as the first parent that is a treeitem.
    if ([attributeName isEqualToString:NSAccessibilityDisclosedByRowAttribute]) {
        if (m_object->isTreeItem()) {
            AccessibilityObject* parent = m_object->parentObject();
            while (parent) {
                if (parent->isTreeItem())
                    return parent->wrapper();
                // If the parent is the tree itself, then this value == nil.
                if (parent->isTree())
                    return nil;
                parent = parent->parentObject();
            }
            return nil;
        } else if (is<AccessibilityARIAGridRow>(*m_object)) {
            AccessibilityObject* row = downcast<AccessibilityARIAGridRow>(*m_object).disclosedByRow();
            if (!row)
                return nil;
            return row->wrapper();
        }
    }
    
    if ([attributeName isEqualToString:NSAccessibilityDisclosureLevelAttribute]) {
        // Convert from 1-based level (from aria-level spec) to 0-based level (Mac)
        int level = m_object->hierarchicalLevel();
        if (level > 0)
            level -= 1;
        return [NSNumber numberWithInt:level];
    }
    if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute])
        return [NSNumber numberWithBool:m_object->isExpanded()];
    
    if (m_object->isList() && [attributeName isEqualToString:NSAccessibilityOrientationAttribute])
        return NSAccessibilityVerticalOrientationValue;
    
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return [self textMarkerRangeForSelection];
    
    if (m_object->renderer()) {
        if ([attributeName isEqualToString: @"AXStartTextMarker"])
            return [self textMarkerForVisiblePosition:startOfDocument(&m_object->renderer()->document())];
        if ([attributeName isEqualToString: @"AXEndTextMarker"])
            return [self textMarkerForVisiblePosition:endOfDocument(&m_object->renderer()->document())];
    }
    
    if ([attributeName isEqualToString:NSAccessibilityBlockQuoteLevelAttribute])
        return [NSNumber numberWithUnsignedInt:m_object->blockquoteLevel()];
    if ([attributeName isEqualToString:@"AXTableLevel"])
        return [NSNumber numberWithInt:m_object->tableLevel()];
    
    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector linkedUIElements;
        m_object->linkedUIElements(linkedUIElements);
        return (NSArray *)convertToNSArray(linkedUIElements);
    }
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return [NSNumber numberWithBool:m_object->isSelected()];
    
    if ([attributeName isEqualToString: NSAccessibilityARIACurrentAttribute])
        return m_object->currentValue();
    
    if ([attributeName isEqualToString: NSAccessibilityServesAsTitleForUIElementsAttribute] && m_object->isMenuButton()) {
        AccessibilityObject* uiElement = downcast<AccessibilityRenderObject>(*m_object).menuForMenuButton();
        if (uiElement)
            return [NSArray arrayWithObject:uiElement->wrapper()];
    }
    
    if ([attributeName isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
        if (!m_object->exposesTitleUIElement())
            return nil;
        
        AccessibilityObject* obj = m_object->titleUIElement();
        if (obj)
            return obj->wrapper();
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityValueDescriptionAttribute]) {
        if (m_object->isMeter())
            return [self valueDescriptionForMeter];
        return m_object->valueDescription();
    }
    
    if ([attributeName isEqualToString:NSAccessibilityOrientationAttribute]) {
        AccessibilityOrientation elementOrientation = m_object->orientation();
        if (elementOrientation == AccessibilityOrientation::Vertical)
            return NSAccessibilityVerticalOrientationValue;
        if (elementOrientation == AccessibilityOrientation::Horizontal)
            return NSAccessibilityHorizontalOrientationValue;
        if (elementOrientation == AccessibilityOrientation::Undefined)
            return NSAccessibilityUnknownOrientationValue;
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityHorizontalScrollBarAttribute]) {
        AccessibilityObject* scrollBar = m_object->scrollBar(AccessibilityOrientation::Horizontal);
        if (scrollBar)
            return scrollBar->wrapper();
        return nil;
    }
    if ([attributeName isEqualToString:NSAccessibilityVerticalScrollBarAttribute]) {
        AccessibilityObject* scrollBar = m_object->scrollBar(AccessibilityOrientation::Vertical);
        if (scrollBar)
            return scrollBar->wrapper();
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilitySortDirectionAttribute]) {
        switch (m_object->sortDirection()) {
        case AccessibilitySortDirection::Ascending:
            return NSAccessibilityAscendingSortDirectionValue;
        case AccessibilitySortDirection::Descending:
            return NSAccessibilityDescendingSortDirectionValue;
        default:
            return NSAccessibilityUnknownSortDirectionValue;
        }
    }
    
    if ([attributeName isEqualToString:NSAccessibilityLanguageAttribute])
        return m_object->language();
    
    if ([attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        return [NSNumber numberWithBool:m_object->isExpanded()];
    
    if ([attributeName isEqualToString:NSAccessibilityRequiredAttribute])
        return [NSNumber numberWithBool:m_object->isRequired()];
    
    if ([attributeName isEqualToString:NSAccessibilityInvalidAttribute])
        return m_object->invalidStatus();
    
    if ([attributeName isEqualToString:NSAccessibilityOwnsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector ariaOwns;
        m_object->ariaOwnsElements(ariaOwns);
        return (NSArray *)convertToNSArray(ariaOwns);
    }
    
    if ([attributeName isEqualToString:NSAccessibilityARIAPosInSetAttribute])
        return [NSNumber numberWithInt:m_object->posInSet()];
    if ([attributeName isEqualToString:NSAccessibilityARIASetSizeAttribute])
        return [NSNumber numberWithInt:m_object->setSize()];
    
    if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        return [NSNumber numberWithBool:m_object->isARIAGrabbed()];
    
    if ([attributeName isEqualToString:NSAccessibilityDropEffectsAttribute]) {
        Vector<String> dropEffects = m_object->determineARIADropEffects();
        return convertStringsToNSArray(dropEffects);
    }
    
    if ([attributeName isEqualToString:NSAccessibilityPlaceholderValueAttribute])
        return m_object->placeholderValue();

    if ([attributeName isEqualToString:NSAccessibilityValueAutofillAvailableAttribute])
        return @(m_object->isValueAutofillAvailable());
    
    if ([attributeName isEqualToString:NSAccessibilityValueAutofillTypeAttribute]) {
        switch (m_object->valueAutofillButtonType()) {
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
        return @(m_object->isValueAutofilled());

    if ([attributeName isEqualToString:NSAccessibilityHasPopupAttribute])
        return [NSNumber numberWithBool:m_object->hasPopup()];

    if ([attributeName isEqualToString:NSAccessibilityDatetimeValueAttribute])
        return m_object->datetimeAttributeValue();
    
    if ([attributeName isEqualToString:NSAccessibilityInlineTextAttribute])
        return @(m_object->renderer() && is<RenderInline>(m_object->renderer()));
    
    // ARIA Live region attributes.
    if ([attributeName isEqualToString:NSAccessibilityARIALiveAttribute])
        return m_object->liveRegionStatus();
    if ([attributeName isEqualToString:NSAccessibilityARIARelevantAttribute])
        return m_object->liveRegionRelevant();
    if ([attributeName isEqualToString:NSAccessibilityARIAAtomicAttribute])
        return [NSNumber numberWithBool:m_object->liveRegionAtomic()];
    if ([attributeName isEqualToString:NSAccessibilityElementBusyAttribute])
        return [NSNumber numberWithBool:m_object->isBusy()];
    
    // MathML Attributes.
    if (m_object->isMathElement()) {
        if ([attributeName isEqualToString:NSAccessibilityMathRootIndexAttribute])
            return (m_object->mathRootIndexObject()) ? m_object->mathRootIndexObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathRootRadicandAttribute])
            return (m_object->mathRadicandObject()) ? m_object->mathRadicandObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathFractionNumeratorAttribute])
            return (m_object->mathNumeratorObject()) ? m_object->mathNumeratorObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathFractionDenominatorAttribute])
            return (m_object->mathDenominatorObject()) ? m_object->mathDenominatorObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathBaseAttribute])
            return (m_object->mathBaseObject()) ? m_object->mathBaseObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathSubscriptAttribute])
            return (m_object->mathSubscriptObject()) ? m_object->mathSubscriptObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathSuperscriptAttribute])
            return (m_object->mathSuperscriptObject()) ? m_object->mathSuperscriptObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathUnderAttribute])
            return (m_object->mathUnderObject()) ? m_object->mathUnderObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathOverAttribute])
            return (m_object->mathOverObject()) ? m_object->mathOverObject()->wrapper() : 0;
        if ([attributeName isEqualToString:NSAccessibilityMathFencedOpenAttribute])
            return m_object->mathFencedOpenString();
        if ([attributeName isEqualToString:NSAccessibilityMathFencedCloseAttribute])
            return m_object->mathFencedCloseString();
        if ([attributeName isEqualToString:NSAccessibilityMathLineThicknessAttribute])
            return [NSNumber numberWithInteger:m_object->mathLineThickness()];
        if ([attributeName isEqualToString:NSAccessibilityMathPostscriptsAttribute])
            return [self accessibilityMathPostscriptPairs];
        if ([attributeName isEqualToString:NSAccessibilityMathPrescriptsAttribute])
            return [self accessibilityMathPrescriptPairs];
    }
    
    if ([attributeName isEqualToString:NSAccessibilityExpandedTextValueAttribute])
        return m_object->expandedTextValue();
    
    if ([attributeName isEqualToString:NSAccessibilityDOMIdentifierAttribute])
        return m_object->identifierAttribute();
    if ([attributeName isEqualToString:NSAccessibilityDOMClassListAttribute]) {
        Vector<String> classList;
        m_object->classList(classList);
        return convertStringsToNSArray(classList);
    }
    
    // This allows us to connect to a plugin that creates a shadow node for editing (like PDFs).
    if ([attributeName isEqualToString:@"_AXAssociatedPluginParent"])
        return [self associatedPluginParent];
    
    // this is used only by DumpRenderTree for testing
    if ([attributeName isEqualToString:@"AXClickPoint"])
        return [NSValue valueWithPoint:m_object->clickPoint()];
    
    // This is used by DRT to verify CSS3 speech works.
    if ([attributeName isEqualToString:@"AXDRTSpeechAttribute"])
        return [self baseAccessibilitySpeechHint];
    
    // Used by DRT to find an accessible node by its element id.
    if ([attributeName isEqualToString:@"AXDRTElementIdAttribute"])
        return m_object->getAttribute(idAttr);
    
    if ([attributeName isEqualToString:@"AXAutocompleteValue"])
        return m_object->autoCompleteValue();
    
    if ([attributeName isEqualToString:@"AXHasPopUpValue"])
        return m_object->hasPopupValue();
    
    if ([attributeName isEqualToString:@"AXKeyShortcutsValue"])
        return m_object->keyShortcutsValue();
    
    if ([attributeName isEqualToString:@"AXARIAPressedIsPresent"])
        return [NSNumber numberWithBool:m_object->pressedIsPresent()];
    
    if ([attributeName isEqualToString:@"AXIsMultiline"])
        return [NSNumber numberWithBool:m_object->ariaIsMultiline()];
    
    if ([attributeName isEqualToString:@"AXReadOnlyValue"])
        return m_object->readOnlyValue();

    if ([attributeName isEqualToString:@"AXIsActiveDescendantOfFocusedContainer"])
        return [NSNumber numberWithBool:m_object->isActiveDescendantOfFocusedContainer()];

    if ([attributeName isEqualToString:@"AXDetailsElements"]) {
        AccessibilityObject::AccessibilityChildrenVector details;
        m_object->ariaDetailsElements(details);
        return (NSArray *)convertToNSArray(details);
    }

    if ([attributeName isEqualToString:NSAccessibilityRelativeFrameAttribute])
        return [NSValue valueWithRect:NSRectFromCGRect(_axBackingObject->relativeFrame())];

    if ([attributeName isEqualToString:@"AXErrorMessageElements"]) {
        AccessibilityObject::AccessibilityChildrenVector errorMessages;
        m_object->ariaErrorMessageElements(errorMessages);
        return (NSArray *)convertToNSArray(errorMessages);
    }

    // Multi-selectable
    if ([attributeName isEqualToString:NSAccessibilityIsMultiSelectableAttribute])
        return [NSNumber numberWithBool:m_object->isMultiSelectable()];
    
    // Document attributes
    if ([attributeName isEqualToString:NSAccessibilityDocumentURIAttribute]) {
        if (Document* document = m_object->document())
            return document->documentURI();
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityDocumentEncodingAttribute]) {
        if (Document* document = m_object->document())
            return document->encoding();
        return nil;
    }
    
    // Aria controls element
    if ([attributeName isEqualToString:NSAccessibilityAriaControlsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector ariaControls;
        m_object->ariaControlsElements(ariaControls);
        return (NSArray *)convertToNSArray(ariaControls);
    }

    if ([attributeName isEqualToString:NSAccessibilityFocusableAncestorAttribute]) {
        AccessibilityObject* object = m_object->focusableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityEditableAncestorAttribute]) {
        AccessibilityObject* object = m_object->editableAncestor();
        return object ? object->wrapper() : nil;
    }

    if ([attributeName isEqualToString:NSAccessibilityHighestEditableAncestorAttribute]) {
        AccessibilityObject* object = m_object->highestEditableAncestor();
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
    if (![self updateObjectBackingStore])
        return nil;
    
    auto focusedObject = _axBackingObject->focusedUIElement();
    if (!focusedObject)
        return nil;
    
    return focusedObject->wrapper();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (![self updateObjectBackingStore])
        return nil;
    
    _axBackingObject->updateChildrenIfNecessary();
    AccessibilityObjectInterface* axObject = _axBackingObject->accessibilityHitTest(IntPoint(point));
    if (axObject) {
        if (axObject->isAttachment() && [axObject->wrapper() attachmentView])
            return [axObject->wrapper() attachmentView];
        return NSAccessibilityUnignoredAncestor(axObject->wrapper());
    }
    return NSAccessibilityUnignoredAncestor(self);
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return NO;
    
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return YES;
    
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return m_object->canSetFocusAttribute();
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return m_object->canSetValueAttribute();
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return m_object->canSetSelectedAttribute();
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute])
        return m_object->canSetSelectedChildrenAttribute();
    
    if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute]
        || [attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        return m_object->canSetExpandedAttribute();
    
    if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute])
        return YES;
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute] ||
        [attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute] ||
        [attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
        return m_object->canSetTextRangeAttributes();
    
    if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        return YES;
    
    if (m_object->isWebArea() && [attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
        return YES;
    
    if (m_object->isWebArea() && [attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
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
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)accessibilityIsIgnored
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return YES;
    
    if (_axBackingObject->isAttachment())
        return [[self attachmentView] accessibilityIsIgnored];
    return _axBackingObject->accessibilityIsIgnored();
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray* )accessibilityParameterizedAttributeNames
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return nil;
    
    if (m_object->isAttachment())
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
    
    if (m_object->isPasswordField())
        return @[ NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute ];
    
    if (!m_object->isAccessibilityRenderObject())
        return paramAttrs;
    
    if (m_object->isTextControl())
        return textParamAttrs;
    
    if (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility())
        return tableParamAttrs;
    
    if (m_object->isMenuRelated())
        return nil;
    
    if (m_object->isWebArea())
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
    if (![self updateObjectBackingStore])
        return;
    
    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityPressAction];
    else
        m_object->press();
}

- (void)accessibilityPerformIncrementAction
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self _accessibilityPerformIncrementAction];
    });
}

- (void)_accessibilityPerformIncrementAction
{
    if (![self updateObjectBackingStore])
        return;
    
    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityIncrementAction];
    else
        m_object->increment();
}

- (void)accessibilityPerformDecrementAction
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self _accessibilityPerformDecrementAction];
    });
}

- (void)_accessibilityPerformDecrementAction
{
    if (![self updateObjectBackingStore])
        return;
    
    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityDecrementAction];
    else
        m_object->decrement();
}

ALLOW_DEPRECATED_DECLARATIONS_END

- (void)accessibilityPerformShowMenuAction
{
    if (m_object && m_object->dispatchAccessibilityEventWithType(AccessibilityEventType::ContextMenu))
        return;
    
    if (m_object->roleValue() == AccessibilityRole::ComboBox)
        m_object->setIsExpanded(true);
    else {
        // This needs to be performed in an iteration of the run loop that did not start from an AX call.
        // If it's the same run loop iteration, the menu open notification won't be sent
        [self performSelector:@selector(accessibilityShowContextMenu) withObject:nil afterDelay:0.0];
    }
}

- (void)accessibilityShowContextMenu
{
    if (!m_object)
        return;
    
    Page* page = m_object->page();
    if (!page)
        return;
    
    IntRect rect = snappedIntRect(m_object->elementRect());
    FrameView* frameView = m_object->documentFrameView();
    
    // On WK2, we need to account for the scroll position.
    // On WK1, this isn't necessary, it's taken care of by the attachment views.
    if (frameView && !frameView->platformWidget()) {
        // Find the appropriate scroll view to use to convert the contents to the window.
        for (AccessibilityObject* parent = m_object->parentObject(); parent; parent = parent->parentObject()) {
            if (is<AccessibilityScrollView>(*parent)) {
                ScrollView* scrollView = downcast<AccessibilityScrollView>(*parent).scrollView();
                rect = scrollView->contentsToRootView(rect);
                break;
            }
        }
    }
    
    page->contextMenuController().showContextMenuAt(page->mainFrame(), rect.center());
}

- (void)accessibilityScrollToVisible
{
    m_object->scrollToMakeVisible();
}

- (void)_accessibilityScrollToMakeVisibleWithSubFocus:(NSRect)rect
{
    m_object->scrollToMakeVisibleWithSubFocus(IntRect(rect));
}

- (void)_accessibilityScrollToGlobalPoint:(NSPoint)point
{
    m_object->scrollToGlobalPoint(IntPoint(point));
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)accessibilityPerformAction:(NSString*)action
IGNORE_WARNINGS_END
{
    if (![self updateObjectBackingStore])
        return;
    
    if ([action isEqualToString:NSAccessibilityPressAction])
        [self accessibilityPerformPressAction];
    
    // Used in layout tests, so that we don't have to wait for the async press action.
    else if ([action isEqualToString:@"AXSyncPressAction"])
        [self _accessibilityPerformPressAction];
    
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
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
IGNORE_WARNINGS_END
{
#if PLATFORM(MAC)
    // In case anything we do by changing values causes an alert or other modal
    // behaviors, we need to return now, so that VoiceOver doesn't hang indefinitely.
    dispatch_async(dispatch_get_main_queue(), ^{
        [self _accessibilitySetValue:value forAttribute:attributeName];
    });
#else
    // dispatch_async on earlier versions can cause focus not to track.
    [self _accessibilitySetValue:value forAttribute:attributeName];
#endif
}

- (void)_accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
{
    if (![self updateObjectBackingStore])
        return;
    
    id textMarkerRange = nil;
    NSNumber*               number = nil;
    NSString*               string = nil;
    NSRange                 range = {0, 0};
    NSArray*                array = nil;
    
    // decode the parameter
    if (AXObjectIsTextMarkerRange(value))
        textMarkerRange = value;
    
    else if ([value isKindOfClass:[NSNumber self]])
        number = value;
    
    else if ([value isKindOfClass:[NSString self]])
        string = value;
    
    else if ([value isKindOfClass:[NSValue self]])
        range = [value rangeValue];
    
    else if ([value isKindOfClass:[NSArray self]])
        array = value;
    
    // handle the command
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        ASSERT(textMarkerRange);
        m_object->setSelectedVisiblePositionRange([self visiblePositionRangeForTextMarkerRange:textMarkerRange]);
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        ASSERT(number);
        
        bool focus = [number boolValue];
        
        // If focus is just set without making the view the first responder, then keyboard focus won't move to the right place.
        if (focus && !m_object->document()->frame()->selection().isFocusedAndActive()) {
            FrameView* frameView = m_object->documentFrameView();
            Page* page = m_object->page();
            if (page && frameView) {
                ChromeClient& chromeClient = page->chrome().client();
                chromeClient.focus();
                
                // Legacy WebKit1 case.
                if (frameView->platformWidget())
                    chromeClient.makeFirstResponder(frameView->platformWidget());
                else
                    chromeClient.assistiveTechnologyMakeFirstResponder();
            }
        }
        
        m_object->setFocused(focus);
    } else if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (number && m_object->canSetNumericValue())
            m_object->setValue([number floatValue]);
        else if (string)
            m_object->setValue(string);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute]) {
        if (!number)
            return;
        m_object->setSelected([number boolValue]);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (!array)
            return;
        if (m_object->roleValue() != AccessibilityRole::ListBox)
            return;
        AccessibilityObject::AccessibilityChildrenVector selectedChildren;
        convertToVector(array, selectedChildren);
        downcast<AccessibilityListBox>(*m_object).setSelectedChildren(selectedChildren);
    } else if (m_object->isTextControl()) {
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            m_object->setSelectedText(string);
        } else if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            m_object->setSelectedTextRange(PlainTextRange(range.location, range.length));
        } else if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute]) {
            m_object->makeRangeVisible(PlainTextRange(range.location, range.length));
        }
    } else if ([attributeName isEqualToString:NSAccessibilityDisclosingAttribute] || [attributeName isEqualToString:NSAccessibilityExpandedAttribute])
        m_object->setIsExpanded([number boolValue]);
    else if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector selectedRows;
        convertToVector(array, selectedRows);
        if (m_object->isTree() || (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility()))
            m_object->setSelectedRows(selectedRows);
    } else if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        m_object->setARIAGrabbed([number boolValue]);
    else if (m_object->isWebArea() && [attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
        m_object->setPreventKeyboardDOMEventDispatch([number boolValue]);
    else if (m_object->isWebArea() && [attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
        m_object->setCaretBrowsingEnabled([number boolValue]);
}

// Used to set attributes synchronously on accessibility elements within tests.
// For use with DumpRenderTree only.
- (void)_accessibilitySetTestValue:(id)value forAttribute:(NSString*)attributeName
{
    [self _accessibilitySetValue:value forAttribute:attributeName];
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

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSString*)accessibilityActionDescription:(NSString*)action
IGNORE_WARNINGS_END
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}

// The CFAttributedStringType representation of the text associated with this accessibility
// object that is specified by the given range.
- (NSAttributedString*)doAXAttributedStringForRange:(NSRange)range
{
    PlainTextRange textRange = PlainTextRange(range.location, range.length);
    RefPtr<Range> webRange = m_object->rangeForPlainTextRange(textRange);
    return [self doAXAttributedStringForTextMarkerRange:[self textMarkerRangeFromRange:webRange] spellCheck:YES];
}

- (NSRange)_convertToNSRange:(Range*)range
{
    NSRange result = NSMakeRange(NSNotFound, 0);
    if (!range)
        return result;
    
    Document* document = m_object->document();
    if (!document)
        return result;
    
    size_t location;
    size_t length;
    TextIterator::getLocationAndLengthFromRange(document->documentElement(), range, location, length);
    result.location = location;
    result.length = length;
    
    return result;
}

- (NSInteger)_indexForTextMarker:(id)marker
{
    if (!marker)
        return NSNotFound;
    
    if (AXObjectCache* cache = m_object->axObjectCache()) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:marker];
        // Create a collapsed range from the CharacterOffset object.
        RefPtr<Range> range = cache->rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
        return [self _convertToNSRange:range.get()].location;
    }
    return NSNotFound;
}

- (id)_textMarkerForIndex:(NSInteger)textIndex
{
    Document* document = m_object->document();
    if (!document)
        return nil;
    
    RefPtr<Range> textRange = TextIterator::rangeFromLocationAndLength(document->documentElement(), textIndex, 0);
    if (!textRange || !textRange->boundaryPointsValid())
        return nil;
    
    if (AXObjectCache* cache = m_object->axObjectCache()) {
        CharacterOffset characterOffset = cache->startOrEndCharacterOffsetForRange(textRange, true);
        return [self textMarkerForCharacterOffset:characterOffset];
    }
    return nil;
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

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
IGNORE_WARNINGS_END
{
    id textMarker = nil;
    id textMarkerRange = nil;
    NSNumber* number = nil;
    NSArray* array = nil;
    NSDictionary* dictionary = nil;
    RefPtr<AccessibilityObject> uiElement = nullptr;
    NSPoint point = NSZeroPoint;
    bool pointSet = false;
    NSRange range = {0, 0};
    bool rangeSet = false;
    NSRect rect = NSZeroRect;
    
    // basic parameter validation
    if (!m_object || !attribute || !parameter)
        return nil;
    
    if (![self updateObjectBackingStore])
        return nil;
    
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nil;
    
    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because it is indistinguishable from
    // a parameter of the wrong type.
    if (AXObjectIsTextMarker(parameter))
        textMarker = parameter;
    
    else if (AXObjectIsTextMarkerRange(parameter))
        textMarkerRange = parameter;
    
    else if ([parameter isKindOfClass:[WebAccessibilityObjectWrapper self]])
        uiElement = [(WebAccessibilityObjectWrapper*)parameter accessibilityObject];
    
    else if ([parameter isKindOfClass:[NSNumber self]])
        number = parameter;
    
    else if ([parameter isKindOfClass:[NSArray self]])
        array = parameter;
    
    else if ([parameter isKindOfClass:[NSDictionary self]])
        dictionary = parameter;
    
    else if ([parameter isKindOfClass:[NSValue self]] && !strcmp([(NSValue*)parameter objCType], @encode(NSPoint))) {
        pointSet = true;
        point = [(NSValue*)parameter pointValue];
        
    } else if ([parameter isKindOfClass:[NSValue self]] && !strcmp([(NSValue*)parameter objCType], @encode(NSRange))) {
        rangeSet = true;
        range = [(NSValue*)parameter rangeValue];
    } else if ([parameter isKindOfClass:[NSValue self]] && !strcmp([(NSValue*)parameter objCType], @encode(NSRect)))
        rect = [(NSValue*)parameter rectValue];
    else {
        // Attribute type is not supported. Allow super to handle.
        return [super accessibilityAttributeValue:attribute forParameter:parameter];
    }
    
    // dispatch
    if ([attribute isEqualToString:NSAccessibilitySelectTextWithCriteriaParameterizedAttribute]) {
        AccessibilitySelectTextCriteria criteria = accessibilitySelectTextCriteriaForCriteriaParameterizedAttribute(dictionary);
        return m_object->selectText(&criteria);
    }
    
    if ([attribute isEqualToString:NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute]) {
        AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(dictionary);
        AccessibilityObject::AccessibilityChildrenVector results;
        m_object->findMatchingObjects(&criteria, results);
        return @(results.size());
    }
    
    if ([attribute isEqualToString:NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute]) {
        AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(dictionary);
        AccessibilityObject::AccessibilityChildrenVector results;
        m_object->findMatchingObjects(&criteria, results);
        return (NSArray *)convertToNSArray(results);
    }
    
    if ([attribute isEqualToString:NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute]) {
        IntRect webCoreRect = [self screenToContents:enclosingIntRect(rect)];
        CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, false);
        return [self textMarkerForCharacterOffset:characterOffset];
    }
    if ([attribute isEqualToString:NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute]) {
        IntRect webCoreRect = [self screenToContents:enclosingIntRect(rect)];
        CharacterOffset characterOffset = cache->characterOffsetForBounds(webCoreRect, true);
        return [self textMarkerForCharacterOffset:characterOffset];
    }
    
    if ([attribute isEqualToString:NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
        VisiblePosition visiblePosition = [self visiblePositionForTextMarker:textMarker];
        VisiblePositionRange visiblePositionRange = m_object->lineRangeForPosition(visiblePosition);
        return [self textMarkerRangeFromVisiblePositions:visiblePositionRange.start endPosition:visiblePositionRange.end];
    }
    
    if ([attribute isEqualToString:NSAccessibilityTextMarkerIsValidParameterizedAttribute]) {
        VisiblePosition pos = [self visiblePositionForTextMarker:textMarker];
        return [NSNumber numberWithBool:!pos.isNull()];
    }
    if ([attribute isEqualToString:NSAccessibilityIndexForTextMarkerParameterizedAttribute]) {
        return [NSNumber numberWithInteger:[self _indexForTextMarker:textMarker]];
    }
    if ([attribute isEqualToString:NSAccessibilityTextMarkerForIndexParameterizedAttribute]) {
        return [self _textMarkerForIndex:[number integerValue]];
    }
    
    if ([attribute isEqualToString:@"AXUIElementForTextMarker"]) {
        AccessibilityObject* axObject = [self accessibilityObjectForTextMarker:textMarker];
        if (!axObject)
            return nil;
        if (axObject->isAttachment() && [axObject->wrapper() attachmentView])
            return [axObject->wrapper() attachmentView];
        return axObject->wrapper();
    }
    
    if ([attribute isEqualToString:@"AXTextMarkerRangeForUIElement"]) {
        RefPtr<Range> range = uiElement.get()->elementRange();
        return [self textMarkerRangeFromRange:range];
    }
    
    if ([attribute isEqualToString:@"AXLineForTextMarker"]) {
        VisiblePosition visiblePos = [self visiblePositionForTextMarker:(textMarker)];
        return [NSNumber numberWithUnsignedInt:m_object->lineForPosition(visiblePos)];
    }
    
    if ([attribute isEqualToString:@"AXTextMarkerRangeForLine"]) {
        VisiblePositionRange vpRange = m_object->visiblePositionRangeForLine([number intValue]);
        return [self textMarkerRangeFromVisiblePositions:vpRange.start endPosition:vpRange.end];
    }
    
    if ([attribute isEqualToString:@"AXStringForTextMarkerRange"]) {
        RefPtr<Range> range = [self rangeForTextMarkerRange:textMarkerRange];
        return m_object->stringForRange(range);
    }
    
    if ([attribute isEqualToString:@"AXTextMarkerForPosition"]) {
        IntPoint webCorePoint = IntPoint(point);
        if (!pointSet)
            return nil;
        CharacterOffset characterOffset = cache->characterOffsetForPoint(webCorePoint, m_object);
        return [self textMarkerForCharacterOffset:characterOffset];
    }
    
    if ([attribute isEqualToString:@"AXBoundsForTextMarkerRange"]) {
        RefPtr<Range> range = [self rangeForTextMarkerRange:textMarkerRange];
        NSRect rect = m_object->boundsForRange(range);
        return [NSValue valueWithRect:rect];
    }
    
    if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
        CharacterOffset start = cache->characterOffsetForIndex(range.location, m_object);
        CharacterOffset end = cache->characterOffsetForIndex(range.location+range.length, m_object);
        if (start.isNull() || end.isNull())
            return nil;
        RefPtr<Range> range = cache->rangeForUnorderedCharacterOffsets(start, end);
        NSRect rect = m_object->boundsForRange(range);
        return [NSValue valueWithRect:rect];
    }
    
    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute]) {
        if (m_object->isTextControl()) {
            PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);
            return m_object->doAXStringForRange(plainTextRange);
        }
        
        CharacterOffset start = cache->characterOffsetForIndex(range.location, m_object);
        CharacterOffset end = cache->characterOffsetForIndex(range.location + range.length, m_object);
        if (start.isNull() || end.isNull())
            return nil;
        RefPtr<Range> range = cache->rangeForUnorderedCharacterOffsets(start, end);
        return m_object->stringForRange(range);
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
        
        CharacterOffset characterOffset1 = [self characterOffsetForTextMarker:textMarker1];
        CharacterOffset characterOffset2 = [self characterOffsetForTextMarker:textMarker2];
        RefPtr<Range> range = cache->rangeForUnorderedCharacterOffsets(characterOffset1, characterOffset2);
        return [self textMarkerRangeFromRange:range];
    }
    
    if ([attribute isEqualToString:@"AXNextTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        return [self nextTextMarkerForCharacterOffset:characterOffset];
    }
    
    if ([attribute isEqualToString:@"AXPreviousTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        return [self previousTextMarkerForCharacterOffset:characterOffset];
    }
    
    if ([attribute isEqualToString:@"AXLeftWordTextMarkerRangeForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        RefPtr<Range> range = cache->leftWordRange(characterOffset);
        return [self textMarkerRangeFromRange:range];
    }
    
    if ([attribute isEqualToString:@"AXRightWordTextMarkerRangeForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        RefPtr<Range> range = cache->rightWordRange(characterOffset);
        return [self textMarkerRangeFromRange:range];
    }
    
    if ([attribute isEqualToString:@"AXLeftLineTextMarkerRangeForTextMarker"]) {
        VisiblePosition visiblePos = [self visiblePositionForTextMarker:(textMarker)];
        VisiblePositionRange vpRange = m_object->leftLineVisiblePositionRange(visiblePos);
        return [self textMarkerRangeFromVisiblePositions:vpRange.start endPosition:vpRange.end];
    }
    
    if ([attribute isEqualToString:@"AXRightLineTextMarkerRangeForTextMarker"]) {
        VisiblePosition visiblePos = [self visiblePositionForTextMarker:(textMarker)];
        VisiblePositionRange vpRange = m_object->rightLineVisiblePositionRange(visiblePos);
        return [self textMarkerRangeFromVisiblePositions:vpRange.start endPosition:vpRange.end];
    }
    
    if ([attribute isEqualToString:@"AXSentenceTextMarkerRangeForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        RefPtr<Range> range = cache->sentenceForCharacterOffset(characterOffset);
        return [self textMarkerRangeFromRange:range];
    }
    
    if ([attribute isEqualToString:@"AXParagraphTextMarkerRangeForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        RefPtr<Range> range = cache->paragraphForCharacterOffset(characterOffset);
        return [self textMarkerRangeFromRange:range];
    }
    
    if ([attribute isEqualToString:@"AXNextWordEndTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        CharacterOffset nextEnd = cache->nextWordEndCharacterOffset(characterOffset);
        return [self textMarkerForCharacterOffset:nextEnd];
    }
    
    if ([attribute isEqualToString:@"AXPreviousWordStartTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        CharacterOffset previousStart = cache->previousWordStartCharacterOffset(characterOffset);
        return [self textMarkerForCharacterOffset:previousStart];
    }
    
    if ([attribute isEqualToString:@"AXNextLineEndTextMarkerForTextMarker"]) {
        VisiblePosition visiblePos = [self visiblePositionForTextMarker:(textMarker)];
        return [self textMarkerForVisiblePosition:m_object->nextLineEndPosition(visiblePos)];
    }
    
    if ([attribute isEqualToString:@"AXPreviousLineStartTextMarkerForTextMarker"]) {
        VisiblePosition visiblePos = [self visiblePositionForTextMarker:(textMarker)];
        return [self textMarkerForVisiblePosition:m_object->previousLineStartPosition(visiblePos)];
    }
    
    if ([attribute isEqualToString:@"AXNextSentenceEndTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        CharacterOffset nextEnd = cache->nextSentenceEndCharacterOffset(characterOffset);
        return [self textMarkerForCharacterOffset:nextEnd];
    }
    
    if ([attribute isEqualToString:@"AXPreviousSentenceStartTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        CharacterOffset previousStart = cache->previousSentenceStartCharacterOffset(characterOffset);
        return [self textMarkerForCharacterOffset:previousStart];
    }
    
    if ([attribute isEqualToString:@"AXNextParagraphEndTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        CharacterOffset nextEnd = cache->nextParagraphEndCharacterOffset(characterOffset);
        return [self textMarkerForCharacterOffset:nextEnd];
    }
    
    if ([attribute isEqualToString:@"AXPreviousParagraphStartTextMarkerForTextMarker"]) {
        CharacterOffset characterOffset = [self characterOffsetForTextMarker:textMarker];
        CharacterOffset previousStart = cache->previousParagraphStartCharacterOffset(characterOffset);
        return [self textMarkerForCharacterOffset:previousStart];
    }
    
    if ([attribute isEqualToString:@"AXStyleTextMarkerRangeForTextMarker"]) {
        VisiblePosition visiblePos = [self visiblePositionForTextMarker:(textMarker)];
        VisiblePositionRange vpRange = m_object->styleRangeForPosition(visiblePos);
        return [self textMarkerRangeFromVisiblePositions:vpRange.start endPosition:vpRange.end];
    }
    
    if ([attribute isEqualToString:@"AXLengthForTextMarkerRange"]) {
        RefPtr<Range> range = [self rangeForTextMarkerRange:textMarkerRange];
        int length = AXObjectCache::lengthForRange(range.get());
        if (length < 0)
            return nil;
        return [NSNumber numberWithInt:length];
    }
    
    // Used only by DumpRenderTree (so far).
    if ([attribute isEqualToString:@"AXStartTextMarkerForTextMarkerRange"]) {
        RefPtr<Range> range = [self rangeForTextMarkerRange:textMarkerRange];
        return [self startOrEndTextMarkerForRange:range isStart:YES];
    }
    
    if ([attribute isEqualToString:@"AXEndTextMarkerForTextMarkerRange"]) {
        RefPtr<Range> range = [self rangeForTextMarkerRange:textMarkerRange];
        return [self startOrEndTextMarkerForRange:range isStart:NO];
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

    if (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility()) {
        if ([attribute isEqualToString:NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
            if (array == nil || [array count] != 2)
                return nil;
            AccessibilityTableCell* cell = downcast<AccessibilityTable>(*m_object).cellForColumnAndRow([[array objectAtIndex:0] unsignedIntValue], [[array objectAtIndex:1] unsignedIntValue]);
            if (!cell)
                return nil;
            
            return cell->wrapper();
        }
    }
    
    if (m_object->isTextControl()) {
        if ([attribute isEqualToString: (NSString *)kAXLineForIndexParameterizedAttribute]) {
            int lineNumber = m_object->doAXLineForIndex([number intValue]);
            if (lineNumber < 0)
                return nil;
            return [NSNumber numberWithUnsignedInt:lineNumber];
        }
        
        if ([attribute isEqualToString: (NSString *)kAXRangeForLineParameterizedAttribute]) {
            PlainTextRange textRange = m_object->doAXRangeForLine([number intValue]);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }
        
        if ([attribute isEqualToString: (NSString*)kAXStringForRangeParameterizedAttribute]) {
            PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);
            return rangeSet ? (id)(m_object->doAXStringForRange(plainTextRange)) : nil;
        }
        
        if ([attribute isEqualToString: (NSString*)kAXRangeForPositionParameterizedAttribute]) {
            if (!pointSet)
                return nil;
            IntPoint webCorePoint = IntPoint(point);
            PlainTextRange textRange = m_object->doAXRangeForPosition(webCorePoint);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }
        
        if ([attribute isEqualToString: (NSString*)kAXRangeForIndexParameterizedAttribute]) {
            PlainTextRange textRange = m_object->doAXRangeForIndex([number intValue]);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }
        
        if ([attribute isEqualToString: (NSString*)kAXBoundsForRangeParameterizedAttribute]) {
            if (!rangeSet)
                return nil;
            PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);
            NSRect rect = m_object->doAXBoundsForRangeUsingCharacterOffset(plainTextRange);
            return [NSValue valueWithRect:rect];
        }
        
        if ([attribute isEqualToString: (NSString*)kAXRTFForRangeParameterizedAttribute])
            return rangeSet ? [self doAXRTFForRange:range] : nil;
        
        if ([attribute isEqualToString: (NSString*)kAXAttributedStringForRangeParameterizedAttribute])
            return rangeSet ? [self doAXAttributedStringForRange:range] : nil;
        
        if ([attribute isEqualToString: (NSString*)kAXStyleRangeForIndexParameterizedAttribute]) {
            PlainTextRange textRange = m_object->doAXStyleRangeForIndex([number intValue]);
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
    if (![self updateObjectBackingStore])
        return NSNotFound;
    
    // Tree objects return their rows as their children. We can use the original method
    // here, because we won't gain any speed up.
    if (m_object->isTree())
        return [super accessibilityIndexOfChild:child];
    
    NSArray *children = self.childrenVectorArray;
    if (!children.count)
        return [[self renderWidgetChildren] indexOfObject:child];
    
    NSUInteger count = [children count];
    for (NSUInteger i = 0; i < count; ++i) {
        WebAccessibilityObjectWrapper *wrapper = children[i];
        auto backingObject = [wrapper axBackingObject];
        if (!backingObject)
            continue;

        if (wrapper == child || (backingObject->isAttachment() && [wrapper attachmentView] == child))
            return i;
    }
    
    return NSNotFound;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    if (![self updateObjectBackingStore])
        return 0;
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        // Tree items object returns a different set of children than those that are in children()
        // because an AXOutline (the mac role is becomes) has some odd stipulations.
        if (m_object->isTree() || m_object->isTreeItem())
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
    if (![self updateObjectBackingStore])
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

        if (_axBackingObject->isTree() || _axBackingObject->isTreeItem()) {
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
            BOOL isAttachment = [wrapper isKindOfClass:[WebAccessibilityObjectWrapper class]] && wrapper.axBackingObject->isAttachment() && [wrapper attachmentView];
            [subarray addObject:isAttachment ? [wrapper attachmentView] : wrapper];
        }
        
        return subarray;
    }
    
    return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
}

@end

#endif // HAVE(ACCESSIBILITY) && PLATFORM(MAC)
