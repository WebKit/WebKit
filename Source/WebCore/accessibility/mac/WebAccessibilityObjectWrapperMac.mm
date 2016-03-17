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

#if HAVE(ACCESSIBILITY)

#import "AXObjectCache.h"
#import "AccessibilityARIAGridRow.h"
#import "AccessibilityList.h"
#import "AccessibilityListBox.h"
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
#import "Editor.h"
#import "Font.h"
#import "FontCascade.h"
#import "FrameLoaderClient.h"
#import "FrameSelection.h"
#import "HTMLAnchorElement.h"
#import "HTMLAreaElement.h"
#import "HTMLFrameOwnerElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLTextAreaElement.h"
#import "LocalizedStrings.h"
#import "MainFrame.h"
#import "Page.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "ScrollView.h"
#import "TextCheckerClient.h"
#import "TextCheckingHelper.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import "WebCoreFrameView.h"
#import "WebCoreObjCExtras.h"
#import "WebCoreSystemInterface.h"
#import "htmlediting.h"
#import <wtf/ObjcRuntimeExtras.h>
#if ENABLE(TREE_DEBUGGING)
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

// Search
#ifndef NSAccessibilityImmediateDescendantsOnly
#define NSAccessibilityImmediateDescendantsOnly @"AXImmediateDescendantsOnly"
#endif

#ifndef NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute
#define NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute @"AXUIElementCountForSearchPredicate"
#endif

#ifndef NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute
#define NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute @"AXUIElementsForSearchPredicate"
#endif

// Search Keys
#ifndef NSAccessibilityAnyTypeSearchKey
#define NSAccessibilityAnyTypeSearchKey @"AXAnyTypeSearchKey"
#endif

#ifndef NSAccessibilityBlockquoteSameLevelSearchKey
#define NSAccessibilityBlockquoteSameLevelSearchKey @"AXBlockquoteSameLevelSearchKey"
#endif

#ifndef NSAccessibilityBlockquoteSearchKey
#define NSAccessibilityBlockquoteSearchKey @"AXBlockquoteSearchKey"
#endif

#ifndef NSAccessibilityBoldFontSearchKey
#define NSAccessibilityBoldFontSearchKey @"AXBoldFontSearchKey"
#endif

#ifndef NSAccessibilityButtonSearchKey
#define NSAccessibilityButtonSearchKey @"AXButtonSearchKey"
#endif

#ifndef NSAccessibilityCheckBoxSearchKey
#define NSAccessibilityCheckBoxSearchKey @"AXCheckBoxSearchKey"
#endif

#ifndef NSAccessibilityControlSearchKey
#define NSAccessibilityControlSearchKey @"AXControlSearchKey"
#endif

#ifndef NSAccessibilityDifferentTypeSearchKey
#define NSAccessibilityDifferentTypeSearchKey @"AXDifferentTypeSearchKey"
#endif

#ifndef NSAccessibilityFontChangeSearchKey
#define NSAccessibilityFontChangeSearchKey @"AXFontChangeSearchKey"
#endif

#ifndef NSAccessibilityFontColorChangeSearchKey
#define NSAccessibilityFontColorChangeSearchKey @"AXFontColorChangeSearchKey"
#endif

#ifndef NSAccessibilityFrameSearchKey
#define NSAccessibilityFrameSearchKey @"AXFrameSearchKey"
#endif

#ifndef NSAccessibilityGraphicSearchKey
#define NSAccessibilityGraphicSearchKey @"AXGraphicSearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel1SearchKey
#define NSAccessibilityHeadingLevel1SearchKey @"AXHeadingLevel1SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel2SearchKey
#define NSAccessibilityHeadingLevel2SearchKey @"AXHeadingLevel2SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel3SearchKey
#define NSAccessibilityHeadingLevel3SearchKey @"AXHeadingLevel3SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel4SearchKey
#define NSAccessibilityHeadingLevel4SearchKey @"AXHeadingLevel4SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel5SearchKey
#define NSAccessibilityHeadingLevel5SearchKey @"AXHeadingLevel5SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel6SearchKey
#define NSAccessibilityHeadingLevel6SearchKey @"AXHeadingLevel6SearchKey"
#endif

#ifndef NSAccessibilityHeadingSameLevelSearchKey
#define NSAccessibilityHeadingSameLevelSearchKey @"AXHeadingSameLevelSearchKey"
#endif

#ifndef NSAccessibilityHeadingSearchKey
#define NSAccessibilityHeadingSearchKey @"AXHeadingSearchKey"
#endif

#ifndef NSAccessibilityHighlightedSearchKey
#define NSAccessibilityHighlightedSearchKey @"AXHighlightedSearchKey"
#endif

#ifndef NSAccessibilityItalicFontSearchKey
#define NSAccessibilityItalicFontSearchKey @"AXItalicFontSearchKey"
#endif

#ifndef NSAccessibilityLandmarkSearchKey
#define NSAccessibilityLandmarkSearchKey @"AXLandmarkSearchKey"
#endif

#ifndef NSAccessibilityLinkSearchKey
#define NSAccessibilityLinkSearchKey @"AXLinkSearchKey"
#endif

#ifndef NSAccessibilityListSearchKey
#define NSAccessibilityListSearchKey @"AXListSearchKey"
#endif

#ifndef NSAccessibilityLiveRegionSearchKey
#define NSAccessibilityLiveRegionSearchKey @"AXLiveRegionSearchKey"
#endif

#ifndef NSAccessibilityMisspelledWordSearchKey
#define NSAccessibilityMisspelledWordSearchKey @"AXMisspelledWordSearchKey"
#endif

#ifndef NSAccessibilityOutlineSearchKey
#define NSAccessibilityOutlineSearchKey @"AXOutlineSearchKey"
#endif

#ifndef NSAccessibilityPlainTextSearchKey
#define NSAccessibilityPlainTextSearchKey @"AXPlainTextSearchKey"
#endif

#ifndef NSAccessibilityRadioGroupSearchKey
#define NSAccessibilityRadioGroupSearchKey @"AXRadioGroupSearchKey"
#endif

#ifndef NSAccessibilitySameTypeSearchKey
#define NSAccessibilitySameTypeSearchKey @"AXSameTypeSearchKey"
#endif

#ifndef NSAccessibilityStaticTextSearchKey
#define NSAccessibilityStaticTextSearchKey @"AXStaticTextSearchKey"
#endif

#ifndef NSAccessibilityStyleChangeSearchKey
#define NSAccessibilityStyleChangeSearchKey @"AXStyleChangeSearchKey"
#endif

#ifndef NSAccessibilityTableSameLevelSearchKey
#define NSAccessibilityTableSameLevelSearchKey @"AXTableSameLevelSearchKey"
#endif

#ifndef NSAccessibilityTableSearchKey
#define NSAccessibilityTableSearchKey @"AXTableSearchKey"
#endif

#ifndef NSAccessibilityTextFieldSearchKey
#define NSAccessibilityTextFieldSearchKey @"AXTextFieldSearchKey"
#endif

#ifndef NSAccessibilityUnderlineSearchKey
#define NSAccessibilityUnderlineSearchKey @"AXUnderlineSearchKey"
#endif

#ifndef NSAccessibilityUnvisitedLinkSearchKey
#define NSAccessibilityUnvisitedLinkSearchKey @"AXUnvisitedLinkSearchKey"
#endif

#ifndef NSAccessibilityVisitedLinkSearchKey
#define NSAccessibilityVisitedLinkSearchKey @"AXVisitedLinkSearchKey"
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

@implementation WebAccessibilityObjectWrapper

- (void)unregisterUniqueIdForUIElement
{
    wkUnregisterUniqueIdForElement(self);
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

static inline BOOL AXObjectIsTextMarker(id obj)
{
    return obj != nil && CFGetTypeID(obj) == wkGetAXTextMarkerTypeID();
}

static inline BOOL AXObjectIsTextMarkerRange(id obj)
{
    return obj != nil && CFGetTypeID(obj) == wkGetAXTextMarkerRangeTypeID();
}

static id AXTextMarkerRange(id startMarker, id endMarker)
{
    ASSERT(startMarker != nil);
    ASSERT(endMarker != nil);
    ASSERT(CFGetTypeID(startMarker) == wkGetAXTextMarkerTypeID());
    ASSERT(CFGetTypeID(endMarker) == wkGetAXTextMarkerTypeID());
    return CFBridgingRelease(wkCreateAXTextMarkerRange((CFTypeRef)startMarker, (CFTypeRef)endMarker));
}

static id AXTextMarkerRangeStart(id range)
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID(range) == wkGetAXTextMarkerRangeTypeID());
    return CFBridgingRelease(wkCopyAXTextMarkerRangeStart(range));
}

static id AXTextMarkerRangeEnd(id range)
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID(range) == wkGetAXTextMarkerRangeTypeID());
    return CFBridgingRelease(wkCopyAXTextMarkerRangeEnd(range));
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

#pragma mark Search helpers

typedef HashMap<String, AccessibilitySearchKey> AccessibilitySearchKeyMap;

struct SearchKeyEntry {
    String key;
    AccessibilitySearchKey value;
};

static AccessibilitySearchKeyMap* createAccessibilitySearchKeyMap()
{
    const SearchKeyEntry searchKeys[] = {
        { NSAccessibilityAnyTypeSearchKey, AnyTypeSearchKey },
        { NSAccessibilityBlockquoteSameLevelSearchKey, BlockquoteSameLevelSearchKey },
        { NSAccessibilityBlockquoteSearchKey, BlockquoteSearchKey },
        { NSAccessibilityBoldFontSearchKey, BoldFontSearchKey },
        { NSAccessibilityButtonSearchKey, ButtonSearchKey },
        { NSAccessibilityCheckBoxSearchKey, CheckBoxSearchKey },
        { NSAccessibilityControlSearchKey, ControlSearchKey },
        { NSAccessibilityDifferentTypeSearchKey, DifferentTypeSearchKey },
        { NSAccessibilityFontChangeSearchKey, FontChangeSearchKey },
        { NSAccessibilityFontColorChangeSearchKey, FontColorChangeSearchKey },
        { NSAccessibilityFrameSearchKey, FrameSearchKey },
        { NSAccessibilityGraphicSearchKey, GraphicSearchKey },
        { NSAccessibilityHeadingLevel1SearchKey, HeadingLevel1SearchKey },
        { NSAccessibilityHeadingLevel2SearchKey, HeadingLevel2SearchKey },
        { NSAccessibilityHeadingLevel3SearchKey, HeadingLevel3SearchKey },
        { NSAccessibilityHeadingLevel4SearchKey, HeadingLevel4SearchKey },
        { NSAccessibilityHeadingLevel5SearchKey, HeadingLevel5SearchKey },
        { NSAccessibilityHeadingLevel6SearchKey, HeadingLevel6SearchKey },
        { NSAccessibilityHeadingSameLevelSearchKey, HeadingSameLevelSearchKey },
        { NSAccessibilityHeadingSearchKey, HeadingSearchKey },
        { NSAccessibilityHighlightedSearchKey, HighlightedSearchKey },
        { NSAccessibilityItalicFontSearchKey, ItalicFontSearchKey },
        { NSAccessibilityLandmarkSearchKey, LandmarkSearchKey },
        { NSAccessibilityLinkSearchKey, LinkSearchKey },
        { NSAccessibilityListSearchKey, ListSearchKey },
        { NSAccessibilityLiveRegionSearchKey, LiveRegionSearchKey },
        { NSAccessibilityMisspelledWordSearchKey, MisspelledWordSearchKey },
        { NSAccessibilityOutlineSearchKey, OutlineSearchKey },
        { NSAccessibilityPlainTextSearchKey, PlainTextSearchKey },
        { NSAccessibilityRadioGroupSearchKey, RadioGroupSearchKey },
        { NSAccessibilitySameTypeSearchKey, SameTypeSearchKey },
        { NSAccessibilityStaticTextSearchKey, StaticTextSearchKey },
        { NSAccessibilityStyleChangeSearchKey, StyleChangeSearchKey },
        { NSAccessibilityTableSameLevelSearchKey, TableSameLevelSearchKey },
        { NSAccessibilityTableSearchKey, TableSearchKey },
        { NSAccessibilityTextFieldSearchKey, TextFieldSearchKey },
        { NSAccessibilityUnderlineSearchKey, UnderlineSearchKey },
        { NSAccessibilityUnvisitedLinkSearchKey, UnvisitedLinkSearchKey },
        { NSAccessibilityVisitedLinkSearchKey, VisitedLinkSearchKey }
    };
    
    AccessibilitySearchKeyMap* searchKeyMap = new AccessibilitySearchKeyMap;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(searchKeys); i++)
        searchKeyMap->set(searchKeys[i].key, searchKeys[i].value);
    
    return searchKeyMap;
}

static AccessibilitySearchKey accessibilitySearchKeyForString(const String& value)
{
    if (value.isEmpty())
        return AnyTypeSearchKey;
    
    static const AccessibilitySearchKeyMap* searchKeyMap = createAccessibilitySearchKeyMap();
    
    AccessibilitySearchKey searchKey = searchKeyMap->get(value);
    
    return searchKey ? searchKey : AnyTypeSearchKey;
}

static AccessibilitySearchCriteria accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(const NSDictionary *parameterizedAttribute)
{
    NSString *directionParameter = [parameterizedAttribute objectForKey:@"AXDirection"];
    NSNumber *immediateDescendantsOnlyParameter = [parameterizedAttribute objectForKey:NSAccessibilityImmediateDescendantsOnly];
    NSNumber *resultsLimitParameter = [parameterizedAttribute objectForKey:@"AXResultsLimit"];
    NSString *searchTextParameter = [parameterizedAttribute objectForKey:@"AXSearchText"];
    WebAccessibilityObjectWrapper *startElementParameter = [parameterizedAttribute objectForKey:@"AXStartElement"];
    NSNumber *visibleOnlyParameter = [parameterizedAttribute objectForKey:@"AXVisibleOnly"];
    id searchKeyParameter = [parameterizedAttribute objectForKey:@"AXSearchKey"];
    
    AccessibilitySearchDirection direction = SearchDirectionNext;
    if ([directionParameter isKindOfClass:[NSString class]])
        direction = [directionParameter isEqualToString:@"AXDirectionNext"] ? SearchDirectionNext : SearchDirectionPrevious;
    
    bool immediateDescendantsOnly = false;
    if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
        immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];
    
    unsigned resultsLimit = 0;
    if ([resultsLimitParameter isKindOfClass:[NSNumber class]])
        resultsLimit = [resultsLimitParameter unsignedIntValue];
    
    String searchText;
    if ([searchTextParameter isKindOfClass:[NSString class]])
        searchText = searchTextParameter;
    
    AccessibilityObject *startElement = nullptr;
    if ([startElementParameter isKindOfClass:[WebAccessibilityObjectWrapper class]])
        startElement = [startElementParameter accessibilityObject];
    
    bool visibleOnly = false;
    if ([visibleOnlyParameter isKindOfClass:[NSNumber class]])
        visibleOnly = [visibleOnlyParameter boolValue];
    
    AccessibilitySearchCriteria criteria = AccessibilitySearchCriteria(startElement, direction, searchText, resultsLimit, visibleOnly, immediateDescendantsOnly);
    
    if ([searchKeyParameter isKindOfClass:[NSString class]])
        criteria.searchKeys.append(accessibilitySearchKeyForString(searchKeyParameter));
    else if ([searchKeyParameter isKindOfClass:[NSArray class]]) {
        size_t searchKeyCount = static_cast<size_t>([searchKeyParameter count]);
        criteria.searchKeys.reserveInitialCapacity(searchKeyCount);
        for (size_t i = 0; i < searchKeyCount; ++i) {
            NSString *searchKey = [searchKeyParameter objectAtIndex:i];
            if ([searchKey isKindOfClass:[NSString class]])
                criteria.searchKeys.uncheckedAppend(accessibilitySearchKeyForString(searchKey));
        }
    }
    
    return criteria;
}

#pragma mark Select text helpers

static AccessibilitySelectTextCriteria accessibilitySelectTextCriteriaForCriteriaParameterizedAttribute(const NSDictionary *parameterizedAttribute)
{
    NSString *activityParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextActivity];
    NSString *ambiguityResolutionParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextAmbiguityResolution];
    NSString *replacementStringParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextReplacementString];
    NSArray *searchStringsParameter = [parameterizedAttribute objectForKey:NSAccessibilitySelectTextSearchStrings];
    
    AccessibilitySelectTextActivity activity = FindAndSelectActivity;
    if ([activityParameter isKindOfClass:[NSString class]]) {
        if ([activityParameter isEqualToString:NSAccessibilitySelectTextActivityFindAndReplace])
            activity = FindAndReplaceActivity;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndCapitalize])
            activity = FindAndCapitalize;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndLowercase])
            activity = FindAndLowercase;
        else if ([activityParameter isEqualToString:kAXSelectTextActivityFindAndUppercase])
            activity = FindAndUppercase;
    }
    
    AccessibilitySelectTextAmbiguityResolution ambiguityResolution = ClosestToSelectionAmbiguityResolution;
    if ([ambiguityResolutionParameter isKindOfClass:[NSString class]]) {
        if ([ambiguityResolutionParameter isEqualToString:NSAccessibilitySelectTextAmbiguityResolutionClosestAfterSelection])
            ambiguityResolution = ClosestAfterSelectionAmbiguityResolution;
        else if ([ambiguityResolutionParameter isEqualToString:NSAccessibilitySelectTextAmbiguityResolutionClosestBeforeSelection])
            ambiguityResolution = ClosestBeforeSelectionAmbiguityResolution;
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

static bool isTextMarkerIgnored(id textMarker)
{
    if (!textMarker)
        return false;
    
    TextMarkerData textMarkerData;
    if (!wkGetBytesFromAXTextMarker(textMarker, &textMarkerData, sizeof(textMarkerData)))
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
    if (!wkGetBytesFromAXTextMarker(textMarker, &textMarkerData, sizeof(textMarkerData)))
        return nullptr;
    return cache->accessibilityObjectForTextMarkerData(textMarkerData);
}

- (id)textMarkerRangeFromRange:(const RefPtr<Range>)range
{
    return textMarkerRangeFromRange(m_object->axObjectCache(), range);
}

static id textMarkerRangeFromRange(AXObjectCache *cache, const RefPtr<Range> range)
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
    
    return CFBridgingRelease(wkCreateAXTextMarker(&textMarkerData, sizeof(textMarkerData)));
}

static id nextTextMarkerForCharacterOffset(AXObjectCache* cache, CharacterOffset& characterOffset)
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForNextCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return CFBridgingRelease(wkCreateAXTextMarker(&textMarkerData, sizeof(textMarkerData)));
}

static id previousTextMarkerForCharacterOffset(AXObjectCache* cache, CharacterOffset& characterOffset)
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForPreviousCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return CFBridgingRelease(wkCreateAXTextMarker(&textMarkerData, sizeof(textMarkerData)));
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
    
    return CFBridgingRelease(wkCreateAXTextMarker(&textMarkerData, sizeof(textMarkerData)));
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
    if (!wkGetBytesFromAXTextMarker(textMarker, &textMarkerData, sizeof(textMarkerData)))
        return CharacterOffset();
    
    return cache->characterOffsetForTextMarkerData(textMarkerData);
}

- (CharacterOffset)characterOffsetForTextMarker:(id)textMarker
{
    return characterOffsetForTextMarker(m_object->axObjectCache(), textMarker);
}

static id textMarkerForVisiblePosition(AXObjectCache* cache, const VisiblePosition& visiblePos)
{
    ASSERT(cache);
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForVisiblePosition(textMarkerData, visiblePos);
    if (!textMarkerData.axID)
        return nil;
    
    return CFBridgingRelease(wkCreateAXTextMarker(&textMarkerData, sizeof(textMarkerData)));
}

- (id)textMarkerForVisiblePosition:(const VisiblePosition &)visiblePos
{
    return textMarkerForVisiblePosition(m_object->axObjectCache(), visiblePos);
}

static VisiblePosition visiblePositionForTextMarker(AXObjectCache* cache, CFTypeRef textMarker)
{
    ASSERT(cache);
    
    if (!textMarker)
        return VisiblePosition();
    TextMarkerData textMarkerData;
    if (!wkGetBytesFromAXTextMarker(textMarker, &textMarkerData, sizeof(textMarkerData)))
        return VisiblePosition();
    
    return cache->visiblePositionForTextMarkerData(textMarkerData);
}

- (VisiblePosition)visiblePositionForTextMarker:(id)textMarker
{
    return visiblePositionForTextMarker(m_object->axObjectCache(), textMarker);
}

static VisiblePosition visiblePositionForStartOfTextMarkerRange(AXObjectCache *cache, id textMarkerRange)
{
    return visiblePositionForTextMarker(cache, AXTextMarkerRangeStart(textMarkerRange));
}

static VisiblePosition visiblePositionForEndOfTextMarkerRange(AXObjectCache *cache, id textMarkerRange)
{
    return visiblePositionForTextMarker(cache, AXTextMarkerRangeEnd(textMarkerRange));
}

static id textMarkerRangeFromMarkers(id textMarker1, id textMarker2)
{
    if (!textMarker1 || !textMarker2)
        return nil;
    
    return AXTextMarkerRange(textMarker1, textMarker2);
}

// When modifying attributed strings, the range can come from a source which may provide faulty information (e.g. the spell checker).
// To protect against such cases the range should be validated before adding or removing attributes.
static BOOL AXAttributedStringRangeIsValid(NSAttributedString* attrString, NSRange range)
{
    return (range.location < [attrString length] && NSMaxRange(range) <= [attrString length]);
}

static void AXAttributeStringSetFont(NSMutableAttributedString* attrString, NSString* attribute, NSFont* font, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;
    
    if (font) {
        NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
                              [font fontName]                             , NSAccessibilityFontNameKey,
                              [font familyName]                           , NSAccessibilityFontFamilyKey,
                              [font displayName]                          , NSAccessibilityVisibleNameKey,
                              [NSNumber numberWithFloat:[font pointSize]] , NSAccessibilityFontSizeKey,
                              nil];
        
        [attrString addAttribute:attribute value:dict range:range];
    } else
        [attrString removeAttribute:attribute range:range];
    
}

static CGColorRef CreateCGColorIfDifferent(NSColor* nsColor, CGColorRef existingColor)
{
    // get color information assuming NSDeviceRGBColorSpace
    NSColor* rgbColor = [nsColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    if (rgbColor == nil)
        rgbColor = [NSColor blackColor];
    CGFloat components[4];
    [rgbColor getRed:&components[0] green:&components[1] blue:&components[2] alpha:&components[3]];
    
    // create a new CGColorRef to return
    CGColorSpaceRef cgColorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef cgColor = CGColorCreate(cgColorSpace, components);
    CGColorSpaceRelease(cgColorSpace);
    
    // check for match with existing color
    if (existingColor && CGColorEqualToColor(cgColor, existingColor)) {
        CGColorRelease(cgColor);
        cgColor = nullptr;
    }
    
    return cgColor;
}

static void AXAttributeStringSetColor(NSMutableAttributedString* attrString, NSString* attribute, NSColor* color, NSRange range)
{
    if (!AXAttributedStringRangeIsValid(attrString, range))
        return;
    
    if (color) {
        CGColorRef existingColor = (CGColorRef) [attrString attribute:attribute atIndex:range.location effectiveRange:nil];
        CGColorRef cgColor = CreateCGColorIfDifferent(color, existingColor);
        if (cgColor) {
            [attrString addAttribute:attribute value:(id)cgColor range:range];
            CGColorRelease(cgColor);
        }
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
    AXAttributeStringSetFont(attrString, NSAccessibilityFontTextAttribute, style.fontCascade().primaryFont().getNSFont(), range);
    
    // set basic colors
    AXAttributeStringSetColor(attrString, NSAccessibilityForegroundColorTextAttribute, nsColor(style.visitedDependentColor(CSSPropertyColor)), range);
    AXAttributeStringSetColor(attrString, NSAccessibilityBackgroundColorTextAttribute, nsColor(style.visitedDependentColor(CSSPropertyBackgroundColor)), range);
    
    // set super/sub scripting
    EVerticalAlign alignment = style.verticalAlign();
    if (alignment == SUB)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:(-1)], range);
    else if (alignment == SUPER)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:1], range);
    else
        [attrString removeAttribute:NSAccessibilitySuperscriptTextAttribute range:range];
    
    // set shadow
    if (style.textShadow())
        AXAttributeStringSetNumber(attrString, NSAccessibilityShadowTextAttribute, [NSNumber numberWithBool:YES], range);
    else
        [attrString removeAttribute:NSAccessibilityShadowTextAttribute range:range];
    
    // set underline and strikethrough
    int decor = style.textDecorationsInEffect();
    if ((decor & TextDecorationUnderline) == 0) {
        [attrString removeAttribute:NSAccessibilityUnderlineTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityUnderlineColorTextAttribute range:range];
    }
    
    if ((decor & TextDecorationLineThrough) == 0) {
        [attrString removeAttribute:NSAccessibilityStrikethroughTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityStrikethroughColorTextAttribute range:range];
    }
    
    if ((decor & (TextDecorationUnderline | TextDecorationLineThrough)) != 0) {
        // FIXME: Should the underline style be reported here?
        Color underlineColor, overlineColor, linethroughColor;
        TextDecorationStyle underlineStyle, overlineStyle, linethroughStyle;
        renderer->getTextDecorationColorsAndStyles(decor, underlineColor, overlineColor, linethroughColor, underlineStyle, overlineStyle, linethroughStyle);
        
        if ((decor & TextDecorationUnderline) != 0) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityUnderlineTextAttribute, [NSNumber numberWithBool:YES], range);
            AXAttributeStringSetColor(attrString, NSAccessibilityUnderlineColorTextAttribute, nsColor(underlineColor), range);
        }
        
        if ((decor & TextDecorationLineThrough) != 0) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityStrikethroughTextAttribute, [NSNumber numberWithBool:YES], range);
            AXAttributeStringSetColor(attrString, NSAccessibilityStrikethroughColorTextAttribute, nsColor(linethroughColor), range);
        }
    }
    
    // Indicate background highlighting.
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (node->hasTagName(markTag))
            AXAttributeStringSetNumber(attrString, @"AXHighlight", [NSNumber numberWithBool:YES], range);
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
    if (unifiedTextCheckerEnabled(node->document().frame())) {
        // Check the spelling directly since document->markersForNode() does not store the misspelled marking when the cursor is in a word.
        TextCheckerClient* checker = node->document().frame()->editor().textChecker();
        
        // checkTextOfParagraph is the only spelling/grammar checker implemented in WK1 and WK2
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(*checker, text, TextCheckingTypeSpelling, results, node->document().frame()->selection().selection());
        
        size_t size = results.size();
        NSNumber* trueValue = [NSNumber numberWithBool:YES];
        for (unsigned i = 0; i < size; i++) {
            const TextCheckingResult& result = results[i];
            AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, trueValue, NSMakeRange(result.location + range.location, result.length));
#if PLATFORM(MAC)
            AXAttributeStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, trueValue, NSMakeRange(result.location + range.location, result.length));
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
        AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, [NSNumber numberWithBool:YES], spellRange);
#if PLATFORM(MAC)
        AXAttributeStringSetNumber(attrString, NSAccessibilityMarkedMisspelledTextAttribute, [NSNumber numberWithBool:YES], spellRange);
#endif

        currentPosition += misspellingLocation + misspellingLength;
    }
}

static void AXAttributeStringSetexpandedTextValue(NSMutableAttributedString *attrString, RenderObject* renderer, NSRange range)
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
        
        AXUIElementRef axElement = wkCreateAXUIElementRef(object->wrapper());
        if (axElement) {
            [attrString addAttribute:attribute value:(id)axElement range:range];
            CFRelease(axElement);
        }
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributedStringAppendText(NSMutableAttributedString* attrString, Node* node, StringView text)
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
#if PLATFORM(MAC)
    [attrString removeAttribute:NSAccessibilityMarkedMisspelledTextAttribute range:attrStringRange];
#endif
    [attrString removeAttribute:NSAccessibilityMisspelledTextAttribute range:attrStringRange];
    
    // set new attributes
    AXAttributeStringSetStyle(attrString, renderer, attrStringRange);
    AXAttributeStringSetHeadingLevel(attrString, renderer, attrStringRange);
    AXAttributeStringSetBlockquoteLevel(attrString, renderer, attrStringRange);
    AXAttributeStringSetexpandedTextValue(attrString, renderer, attrStringRange);
    AXAttributeStringSetElement(attrString, NSAccessibilityLinkTextAttribute, AccessibilityObject::anchorElementForNode(node), attrStringRange);
    
    // do spelling last because it tends to break up the range
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

- (NSAttributedString*)doAXAttributedStringForTextMarkerRange:(id)textMarkerRange
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
                AXAttributedStringAppendText(attrString, &node, listMarkerText);
            AXAttributedStringAppendText(attrString, &node, it.text());
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

static id textMarkerRangeFromVisiblePositions(AXObjectCache *cache, const VisiblePosition& startPosition, const VisiblePosition& endPosition)
{
    id startTextMarker = textMarkerForVisiblePosition(cache, startPosition);
    id endTextMarker = textMarkerForVisiblePosition(cache, endPosition);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

- (id)textMarkerRangeFromVisiblePositions:(const VisiblePosition&)startPosition endPosition:(const VisiblePosition&)endPosition
{
    return textMarkerRangeFromVisiblePositions(m_object->axObjectCache(), startPosition, endPosition);
}

- (NSArray*)accessibilityActionNames
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
    
    if (m_object->supportsExpanded())
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
    
    if (m_object->supportsARIALiveRegion()) {
        [additional addObject:NSAccessibilityARIALiveAttribute];
        [additional addObject:NSAccessibilityARIARelevantAttribute];
    }
    
    if (m_object->supportsARIASetSize())
        [additional addObject:NSAccessibilityARIASetSizeAttribute];
    if (m_object->supportsARIAPosInSet())
        [additional addObject:NSAccessibilityARIAPosInSetAttribute];
    
    if (m_object->sortDirection() != SortDirectionNone)
        [additional addObject:NSAccessibilitySortDirectionAttribute];
    
    // If an object is a child of a live region, then add these
    if (m_object->isInsideARIALiveRegion())
        [additional addObject:NSAccessibilityARIAAtomicAttribute];
    // All objects should expose the ARIA busy attribute (ARIA 1.1 with ISSUE-538).
    [additional addObject:NSAccessibilityElementBusyAttribute];
    
    // Popup buttons on the Mac expose the value attribute.
    if (m_object->isPopUpButton()) {
        [additional addObject:NSAccessibilityValueAttribute];
    }

    if (m_object->supportsRequiredAttribute()) {
        [additional addObject:NSAccessibilityRequiredAttribute];
    }
    
    if (m_object->ariaHasPopup())
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

- (NSArray*)accessibilityAttributeNames
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
        anchorAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (webAreaAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        // WebAreas should not expose AXSubrole.
        [tempArray removeObject:NSAccessibilitySubroleAttribute];
        [tempArray addObject:@"AXLinkUIElements"];
        [tempArray addObject:@"AXLoaded"];
        [tempArray addObject:@"AXLayoutCount"];
        [tempArray addObject:NSAccessibilityLoadingProgressAttribute];
        [tempArray addObject:NSAccessibilityURLAttribute];
        [tempArray addObject:NSAccessibilityCaretBrowsingEnabledAttribute];
        [tempArray addObject:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute];
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
    
    else if (m_object->isLink() || m_object->isImage())
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
    if (m_object->ariaCurrentState() != ARIACurrentFalse)
        [objectAttributes arrayByAddingObjectsFromArray:@[ NSAccessibilityARIACurrentAttribute ]];
    
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return [(widget->platformWidget()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
#pragma clang diagnostic pop
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

static NSMutableArray* convertToNSArray(const AccessibilityObject::AccessibilityChildrenVector& vector)
{
    NSMutableArray* array = [NSMutableArray arrayWithCapacity:vector.size()];
    for (const auto& child : vector) {
        WebAccessibilityObjectWrapper* wrapper = child->wrapper();
        ASSERT(wrapper);
        if (wrapper) {
            // we want to return the attachment view instead of the object representing the attachment.
            // otherwise, we get palindrome errors in the AX hierarchy
            if (child->isAttachment() && [wrapper attachmentView])
                [array addObject:[wrapper attachmentView]];
            else
                [array addObject:wrapper];
        }
    }
    return array;
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

- (CGPoint)convertPointToScreenSpace:(FloatPoint &)point
{
    FrameView* frameView = m_object->documentFrameView();
    
    // WebKit1 code path... platformWidget() exists.
    if (frameView && frameView->platformWidget()) {
        NSPoint nsPoint = (NSPoint)point;
        NSView* view = frameView->documentView();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        nsPoint = [[view window] convertBaseToScreen:[view convertPoint:nsPoint toView:nil]];
#pragma clang diagnostic pop
        return CGPointMake(nsPoint.x, nsPoint.y);
    } else {
        
        // Find the appropriate scroll view to use to convert the contents to the window.
        ScrollView* scrollView = nullptr;
        AccessibilityObject* parent = nullptr;
        for (parent = m_object->parentObject(); parent; parent = parent->parentObject()) {
            if (is<AccessibilityScrollView>(*parent)) {
                scrollView = downcast<AccessibilityScrollView>(*parent).scrollView();
                break;
            }
        }
        
        IntPoint intPoint = flooredIntPoint(point);
        if (scrollView)
            intPoint = scrollView->contentsToRootView(intPoint);
        
        Page* page = m_object->page();
        
        // If we have an empty chrome client (like SVG) then we should use the page
        // of the scroll view parent to help us get to the screen rect.
        if (parent && page && page->chrome().client().isEmptyChromeClient())
            page = parent->page();
        
        if (page) {
            IntRect rect = IntRect(intPoint, IntSize(0, 0));            
            intPoint = page->chrome().rootViewToScreen(rect).location();
        }
        
        return intPoint;
    }
}

static void WebTransformCGPathToNSBezierPath(void *info, const CGPathElement *element)
{
    NSBezierPath *bezierPath = (NSBezierPath *)info;
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
    CGPathApply(path, bezierPath, WebTransformCGPathToNSBezierPath);
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

- (NSValue *)position
{
    IntRect rect = snappedIntRect(m_object->elementRect());
    
    // The Cocoa accessibility API wants the lower-left corner.
    FloatPoint floatPoint = FloatPoint(rect.x(), rect.maxY());

    CGPoint cgPoint = [self convertPointToScreenSpace:floatPoint];
    
    return [NSValue valueWithPoint:NSMakePoint(cgPoint.x, cgPoint.y)];
}

typedef HashMap<int, NSString*> AccessibilityRoleMap;

static const AccessibilityRoleMap& createAccessibilityRoleMap()
{
    struct RoleEntry {
        AccessibilityRole value;
        NSString* string;
    };
    
    static const RoleEntry roles[] = {
        { UnknownRole, NSAccessibilityUnknownRole },
        { ButtonRole, NSAccessibilityButtonRole },
        { RadioButtonRole, NSAccessibilityRadioButtonRole },
        { CheckBoxRole, NSAccessibilityCheckBoxRole },
        { SliderRole, NSAccessibilitySliderRole },
        { TabGroupRole, NSAccessibilityTabGroupRole },
        { TextFieldRole, NSAccessibilityTextFieldRole },
        { StaticTextRole, NSAccessibilityStaticTextRole },
        { TextAreaRole, NSAccessibilityTextAreaRole },
        { ScrollAreaRole, NSAccessibilityScrollAreaRole },
        { PopUpButtonRole, NSAccessibilityPopUpButtonRole },
        { MenuButtonRole, NSAccessibilityMenuButtonRole },
        { TableRole, NSAccessibilityTableRole },
        { ApplicationRole, NSAccessibilityApplicationRole },
        { GroupRole, NSAccessibilityGroupRole },
        { RadioGroupRole, NSAccessibilityRadioGroupRole },
        { ListRole, NSAccessibilityListRole },
        { DirectoryRole, NSAccessibilityListRole },
        { ScrollBarRole, NSAccessibilityScrollBarRole },
        { ValueIndicatorRole, NSAccessibilityValueIndicatorRole },
        { ImageRole, NSAccessibilityImageRole },
        { MenuBarRole, NSAccessibilityMenuBarRole },
        { MenuRole, NSAccessibilityMenuRole },
        { MenuItemRole, NSAccessibilityMenuItemRole },
        { MenuItemCheckboxRole, NSAccessibilityMenuItemRole },
        { MenuItemRadioRole, NSAccessibilityMenuItemRole },
        { ColumnRole, NSAccessibilityColumnRole },
        { RowRole, NSAccessibilityRowRole },
        { ToolbarRole, NSAccessibilityToolbarRole },
        { BusyIndicatorRole, NSAccessibilityBusyIndicatorRole },
        { ProgressIndicatorRole, NSAccessibilityProgressIndicatorRole },
        { WindowRole, NSAccessibilityWindowRole },
        { DrawerRole, NSAccessibilityDrawerRole },
        { SystemWideRole, NSAccessibilitySystemWideRole },
        { OutlineRole, NSAccessibilityOutlineRole },
        { IncrementorRole, NSAccessibilityIncrementorRole },
        { BrowserRole, NSAccessibilityBrowserRole },
        { ComboBoxRole, NSAccessibilityComboBoxRole },
        { SplitGroupRole, NSAccessibilitySplitGroupRole },
        { SplitterRole, NSAccessibilitySplitterRole },
        { ColorWellRole, NSAccessibilityColorWellRole },
        { GrowAreaRole, NSAccessibilityGrowAreaRole },
        { SheetRole, NSAccessibilitySheetRole },
        { HelpTagRole, NSAccessibilityHelpTagRole },
        { MatteRole, NSAccessibilityMatteRole },
        { RulerRole, NSAccessibilityRulerRole },
        { RulerMarkerRole, NSAccessibilityRulerMarkerRole },
        { LinkRole, NSAccessibilityLinkRole },
        { DisclosureTriangleRole, NSAccessibilityDisclosureTriangleRole },
        { GridRole, NSAccessibilityTableRole },
        { WebCoreLinkRole, NSAccessibilityLinkRole },
        { ImageMapLinkRole, NSAccessibilityLinkRole },
        { ImageMapRole, @"AXImageMap" },
        { ListMarkerRole, @"AXListMarker" },
        { WebAreaRole, @"AXWebArea" },
        { HeadingRole, @"AXHeading" },
        { ListBoxRole, NSAccessibilityListRole },
        { ListBoxOptionRole, NSAccessibilityStaticTextRole },
        { CellRole, NSAccessibilityCellRole },
        { GridCellRole, NSAccessibilityCellRole },
        { TableHeaderContainerRole, NSAccessibilityGroupRole },
        { ColumnHeaderRole, NSAccessibilityCellRole },
        { RowHeaderRole, NSAccessibilityCellRole },
        { DefinitionRole, NSAccessibilityGroupRole },
        { DescriptionListDetailRole, NSAccessibilityGroupRole },
        { DescriptionListTermRole, NSAccessibilityGroupRole },
        { DescriptionListRole, NSAccessibilityListRole },
        { SliderThumbRole, NSAccessibilityValueIndicatorRole },
        { LandmarkApplicationRole, NSAccessibilityGroupRole },
        { LandmarkBannerRole, NSAccessibilityGroupRole },
        { LandmarkComplementaryRole, NSAccessibilityGroupRole },
        { LandmarkContentInfoRole, NSAccessibilityGroupRole },
        { LandmarkMainRole, NSAccessibilityGroupRole },
        { LandmarkNavigationRole, NSAccessibilityGroupRole },
        { LandmarkSearchRole, NSAccessibilityGroupRole },
        { ApplicationAlertRole, NSAccessibilityGroupRole },
        { ApplicationAlertDialogRole, NSAccessibilityGroupRole },
        { ApplicationDialogRole, NSAccessibilityGroupRole },
        { ApplicationLogRole, NSAccessibilityGroupRole },
        { ApplicationMarqueeRole, NSAccessibilityGroupRole },
        { ApplicationStatusRole, NSAccessibilityGroupRole },
        { ApplicationTimerRole, NSAccessibilityGroupRole },
        { DocumentRole, NSAccessibilityGroupRole },
        { DocumentArticleRole, NSAccessibilityGroupRole },
        { DocumentMathRole, NSAccessibilityGroupRole },
        { DocumentNoteRole, NSAccessibilityGroupRole },
        { DocumentRegionRole, NSAccessibilityGroupRole },
        { UserInterfaceTooltipRole, NSAccessibilityGroupRole },
        { TabRole, NSAccessibilityRadioButtonRole },
        { TabListRole, NSAccessibilityTabGroupRole },
        { TabPanelRole, NSAccessibilityGroupRole },
        { TreeRole, NSAccessibilityOutlineRole },
        { TreeItemRole, NSAccessibilityRowRole },
        { ListItemRole, NSAccessibilityGroupRole },
        { ParagraphRole, NSAccessibilityGroupRole },
        { LabelRole, NSAccessibilityGroupRole },
        { DivRole, NSAccessibilityGroupRole },
        { FormRole, NSAccessibilityGroupRole },
        { SpinButtonRole, NSAccessibilityIncrementorRole },
        { FooterRole, NSAccessibilityGroupRole },
        { ToggleButtonRole, NSAccessibilityCheckBoxRole },
        { CanvasRole, NSAccessibilityImageRole },
        { SVGRootRole, NSAccessibilityGroupRole },
        { LegendRole, NSAccessibilityGroupRole },
        { MathElementRole, NSAccessibilityGroupRole },
        { AudioRole, NSAccessibilityGroupRole },
        { VideoRole, NSAccessibilityGroupRole },
        { HorizontalRuleRole, NSAccessibilitySplitterRole },
        { BlockquoteRole, NSAccessibilityGroupRole },
        { SwitchRole, NSAccessibilityCheckBoxRole },
        { SearchFieldRole, NSAccessibilityTextFieldRole },
        { PreRole, NSAccessibilityGroupRole },
        { RubyBaseRole, NSAccessibilityGroupRole },
        { RubyBlockRole, NSAccessibilityGroupRole },
        { RubyInlineRole, NSAccessibilityGroupRole },
        { RubyRunRole, NSAccessibilityGroupRole },
        { RubyTextRole, NSAccessibilityGroupRole },
        { DetailsRole, NSAccessibilityGroupRole },
        { SummaryRole, NSAccessibilityGroupRole },
        { SVGTextPathRole, NSAccessibilityGroupRole },
        { SVGTextRole, NSAccessibilityGroupRole },
        { SVGTSpanRole, NSAccessibilityGroupRole },
    };
    AccessibilityRoleMap& roleMap = *new AccessibilityRoleMap;
    
    const unsigned numRoles = sizeof(roles) / sizeof(roles[0]);
    for (unsigned i = 0; i < numRoles; ++i)
        roleMap.set(roles[i].value, roles[i].string);
    return roleMap;
}

static NSString* roleValueToNSString(AccessibilityRole value)
{
    ASSERT(value);
    static const AccessibilityRoleMap& roleMap = createAccessibilityRoleMap();
    return roleMap.get(value);
}

- (NSString*)role
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];
#pragma clang diagnostic pop
    AccessibilityRole role = m_object->roleValue();
    if (role == CanvasRole && m_object->canvasHasFallbackContent())
        role = GroupRole;
    NSString* string = roleValueToNSString(role);
    if (string != nil)
        return string;
    return NSAccessibilityUnknownRole;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (NSString*)subrole
{
    if (m_object->isPasswordField())
        return NSAccessibilitySecureTextFieldSubrole;
    if (m_object->isSearchField())
        return NSAccessibilitySearchFieldSubrole;
    
    if (m_object->isAttachment()) {
        NSView* attachView = [self attachmentView];
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute])
            return [attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
    }
    
    AccessibilityRole role = m_object->roleValue();
    if (role == HorizontalRuleRole)
        return NSAccessibilityContentSeparatorSubrole;
    if (role == ToggleButtonRole)
        return NSAccessibilityToggleSubrole;
    
    if (is<AccessibilitySpinButtonPart>(*m_object)) {
        if (downcast<AccessibilitySpinButtonPart>(*m_object).isIncrementor())
            return NSAccessibilityIncrementArrowSubrole;
        
        return NSAccessibilityDecrementArrowSubrole;
    }
    
    if (m_object->isFileUploadButton())
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
        case LandmarkApplicationRole:
            return @"AXLandmarkApplication";
        case LandmarkBannerRole:
            return @"AXLandmarkBanner";
        case LandmarkComplementaryRole:
            return @"AXLandmarkComplementary";
            // Footer roles should appear as content info types.
        case FooterRole:
        case LandmarkContentInfoRole:
            return @"AXLandmarkContentInfo";
        case LandmarkMainRole:
            return @"AXLandmarkMain";
        case LandmarkNavigationRole:
            return @"AXLandmarkNavigation";
        case LandmarkSearchRole:
            return @"AXLandmarkSearch";
        case ApplicationAlertRole:
            return @"AXApplicationAlert";
        case ApplicationAlertDialogRole:
            return @"AXApplicationAlertDialog";
        case ApplicationDialogRole:
            return @"AXApplicationDialog";
        case ApplicationLogRole:
            return @"AXApplicationLog";
        case ApplicationMarqueeRole:
            return @"AXApplicationMarquee";
        case ApplicationStatusRole:
            return @"AXApplicationStatus";
        case ApplicationTimerRole:
            return @"AXApplicationTimer";
        case DocumentRole:
            return @"AXDocument";
        case DocumentArticleRole:
            return @"AXDocumentArticle";
        case DocumentMathRole:
            return @"AXDocumentMath";
        case DocumentNoteRole:
            return @"AXDocumentNote";
        case DocumentRegionRole:
            return @"AXDocumentRegion";
        case UserInterfaceTooltipRole:
            return @"AXUserInterfaceTooltip";
        case TabPanelRole:
            return @"AXTabPanel";
        case DefinitionRole:
            return @"AXDefinition";
        case DescriptionListTermRole:
            return @"AXTerm";
        case DescriptionListDetailRole:
            return @"AXDescription";
            // Default doesn't return anything, so roles defined below can be chosen.
        default:
            break;
    }
    
    if (role == MathElementRole) {
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
    
    if (role == VideoRole)
        return @"AXVideo";
    if (role == AudioRole)
        return @"AXAudio";
    if (role == DetailsRole)
        return @"AXDetails";
    if (role == SummaryRole)
        return @"AXSummary";
    
    if (m_object->isMediaTimeline())
        return NSAccessibilityTimelineSubrole;

    if (m_object->isSwitch())
        return NSAccessibilitySwitchSubrole;

    if (role == GroupRole) {
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
        }
    }
    
    // Ruby subroles
    switch (role) {
    case RubyBaseRole:
        return NSAccessibilityRubyBaseSubrole;
    case RubyBlockRole:
        return NSAccessibilityRubyBlockSubrole;
    case RubyInlineRole:
        return NSAccessibilityRubyInlineSubrole;
    case RubyRunRole:
        return NSAccessibilityRubyRunSubrole;
    case RubyTextRole:
        return NSAccessibilityRubyTextSubrole;
    default:
        break;
    }
    
    return nil;
}
#pragma clang diagnostic pop

- (NSString*)roleDescription
{
    if (!m_object)
        return nil;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // attachments have the AXImage role, but a different subrole
    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
#pragma clang diagnostic pop

    const AtomicString& overrideRoleDescription = m_object->roleDescription();
    if (!overrideRoleDescription.isNull())
        return overrideRoleDescription;
    
    NSString* axRole = [self role];
    
    if ([axRole isEqualToString:NSAccessibilityGroupRole]) {
        
        NSString *ariaLandmarkRoleDescription = [self ariaLandmarkRoleDescription];
        if (ariaLandmarkRoleDescription)
            return ariaLandmarkRoleDescription;
        
        switch (m_object->roleValue()) {
        case AudioRole:
            return localizedMediaControlElementString("AudioElement");
        case DefinitionRole:
            return AXDefinitionText();
        case DescriptionListTermRole:
            return AXDescriptionListTermText();
        case DescriptionListDetailRole:
            return AXDescriptionListDetailText();
        case FooterRole:
            return AXFooterRoleDescriptionText();
        case VideoRole:
            return localizedMediaControlElementString("VideoElement");
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
    
    if (m_object->isFileUploadButton())
        return AXFileUploadButtonText();
    
    // Only returning for DL (not UL or OL) because description changed with HTML5 from 'definition list' to
    // superset 'description list' and does not return the same values in AX API on some OS versions. 
    if (is<AccessibilityList>(*m_object)) {
        if (downcast<AccessibilityList>(*m_object).isDescriptionList())
            return AXDescriptionListText();
    }
    
    if (m_object->roleValue() == HorizontalRuleRole)
        return AXHorizontalRuleDescriptionText();
    
    // AppKit also returns AXTab for the role description for a tab item.
    if (m_object->isTabItem())
        return NSAccessibilityRoleDescription(@"AXTab", nil);
    
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

// FIXME: split up this function in a better way.
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if (![self updateObjectBackingStore])
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
        if (m_object->isTreeItem()) {
            AccessibilityObject* parent = m_object->parentObjectUnignored();
            while (parent) {
                if (parent->isTree())
                    return parent->wrapper();
                parent = parent->parentObjectUnignored();
            }
        }
        
        AccessibilityObject* parent = m_object->parentObjectUnignored();
        if (!parent)
            return nil;
        
        // In WebKit1, the scroll view is provided by the system (the attachment view), so the parent
        // should be reported directly as such.
        if (m_object->isWebArea() && parent->isAttachment())
            return [parent->wrapper() attachmentView];
        
        return parent->wrapper();
    }
    
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        if (m_object->children().isEmpty()) {
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
            return convertToNSArray(contentCopy);
        }
        
        return convertToNSArray(m_object->children());
    }
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (m_object->isListBox()) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }
        return nil;
    }
    
    if ([attributeName isEqualToString: NSAccessibilityVisibleChildrenAttribute]) {
        if (m_object->isListBox()) {
            AccessibilityObject::AccessibilityChildrenVector visibleChildrenCopy;
            m_object->visibleChildren(visibleChildrenCopy);
            return convertToNSArray(visibleChildrenCopy);
        }
        else if (m_object->isList())
            return [self accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
        
        return nil;
    }
    
    
    if (m_object->isWebArea()) {
        if ([attributeName isEqualToString:@"AXLinkUIElements"]) {
            AccessibilityObject::AccessibilityChildrenVector links;
            downcast<AccessibilityRenderObject>(*m_object).getDocumentLinks(links);
            return convertToNSArray(links);
        }
        if ([attributeName isEqualToString:@"AXLoaded"])
            return [NSNumber numberWithBool:m_object->isLoaded()];
        if ([attributeName isEqualToString:@"AXLayoutCount"])
            return [NSNumber numberWithInt:m_object->layoutCount()];
        if ([attributeName isEqualToString:NSAccessibilityLoadingProgressAttribute])
            return [NSNumber numberWithDouble:m_object->estimatedLoadingProgress()];
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
            
            AccessibilityObject* focusedObject = m_object->focusedUIElement();
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
        if (m_object->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }
        
        // Meter elements should communicate their content via AXValueDescription.
        if (m_object->isMeter())
            return [NSString string];
        
        return [self baseAccessibilityTitle];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute]) {
        if (m_object->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return [self baseAccessibilityDescription];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (m_object->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityValueAttribute];
        }
        if (m_object->supportsRangeValue())
            return [NSNumber numberWithFloat:m_object->valueForRange()];
        if (m_object->roleValue() == SliderThumbRole)
            return [NSNumber numberWithFloat:m_object->parentObject()->valueForRange()];
        if (m_object->isHeading())
            return [NSNumber numberWithInt:m_object->headingLevel()];
        
        if (m_object->isCheckboxOrRadio() || m_object->isMenuItem() || m_object->isSwitch() || m_object->isToggleButton()) {
            switch (m_object->checkboxOrRadioValue()) {
                case ButtonStateOff:
                    return [NSNumber numberWithInt:0];
                case ButtonStateOn:
                    return [NSNumber numberWithInt:1];
                case ButtonStateMixed:
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
        if (m_object->ariaRoleAttribute() == ProgressIndicatorRole && !m_object->hasAttribute(aria_valuenowAttr))
            return @0;
        return [NSNumber numberWithFloat:m_object->minValueForRange()];
    }
    
    if ([attributeName isEqualToString: NSAccessibilityMaxValueAttribute]) {
        // Indeterminate progress indicator should return 0.
        if (m_object->ariaRoleAttribute() == ProgressIndicatorRole && !m_object->hasAttribute(aria_valuenowAttr))
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
    
    if ([attributeName isEqualToString:NSAccessibilityTabsAttribute]) {
        if (m_object->isTabList()) {
            AccessibilityObject::AccessibilityChildrenVector tabsChildren;
            m_object->tabChildren(tabsChildren);
            return convertToNSArray(tabsChildren);
        }
    }
    
    if ([attributeName isEqualToString:NSAccessibilityContentsAttribute]) {
        // The contents of a tab list are all the children except the tabs.
        if (m_object->isTabList()) {
            const auto& children = m_object->children();
            AccessibilityObject::AccessibilityChildrenVector tabsChildren;
            m_object->tabChildren(tabsChildren);
            
            AccessibilityObject::AccessibilityChildrenVector contents;
            unsigned childrenSize = children.size();
            for (unsigned k = 0; k < childrenSize; ++k) {
                if (tabsChildren.find(children[k]) == WTF::notFound)
                    contents.append(children[k]);
            }
            return convertToNSArray(contents);
        } else if (m_object->isScrollView()) {
            // A scrollView's contents are everything except the scroll bars.
            AccessibilityObject::AccessibilityChildrenVector contents;
            for (const auto& child : m_object->children()) {
                if (!child->isScrollbar())
                    contents.append(child);
            }
            return convertToNSArray(contents);
        }
    }
    
    if (is<AccessibilityTable>(*m_object) && downcast<AccessibilityTable>(*m_object).isExposableThroughAccessibility()) {
        auto& table = downcast<AccessibilityTable>(*m_object);
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute])
            return convertToNSArray(table.rows());
        
        if ([attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector visibleRows;
            table.visibleRows(visibleRows);
            return convertToNSArray(visibleRows);
        }
        
        // TODO: distinguish between visible and non-visible columns
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute] ||
            [attributeName isEqualToString:NSAccessibilityVisibleColumnsAttribute]) {
            return convertToNSArray(table.columns());
        }
        
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }
        
        // HTML tables don't support these
        if ([attributeName isEqualToString:NSAccessibilitySelectedColumnsAttribute] ||
            [attributeName isEqualToString:NSAccessibilitySelectedCellsAttribute])
            return nil;
        
        if ([attributeName isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector columnHeaders;
            table.columnHeaders(columnHeaders);
            return convertToNSArray(columnHeaders);
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
            return convertToNSArray(rowHeaders);
        }
        
        if ([attributeName isEqualToString:NSAccessibilityVisibleCellsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector cells;
            table.cells(cells);
            return convertToNSArray(cells);
        }
        
        if ([attributeName isEqualToString:NSAccessibilityColumnCountAttribute])
            return @(table.columnCount());
        
        if ([attributeName isEqualToString:NSAccessibilityRowCountAttribute])
            return @(table.rowCount());
        
        if ([attributeName isEqualToString:NSAccessibilityARIAColumnCountAttribute])
            return @(table.ariaColumnCount());
        
        if ([attributeName isEqualToString:NSAccessibilityARIARowCountAttribute])
            return @(table.ariaRowCount());
    }
    
    if (is<AccessibilityTableColumn>(*m_object)) {
        auto& column = downcast<AccessibilityTableColumn>(*m_object);
        if ([attributeName isEqualToString:NSAccessibilityIndexAttribute])
            return [NSNumber numberWithInt:column.columnIndex()];
        
        // rows attribute for a column is the list of all the elements in that column at each row
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute] ||
            [attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute]) {
            return convertToNSArray(column.children());
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
            return convertToNSArray(columnHeaders);
        }
        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowHeaders;
            cell.rowHeaders(rowHeaders);
            return convertToNSArray(rowHeaders);
        }
        if ([attributeName isEqualToString:NSAccessibilityARIAColumnIndexAttribute])
            return @(cell.ariaColumnIndex());
        
        if ([attributeName isEqualToString:NSAccessibilityARIARowIndexAttribute])
            return @(cell.ariaRowIndex());
    }
    
    if (m_object->isTree()) {
        if ([attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            m_object->ariaTreeRows(rowsCopy);
            return convertToNSArray(rowsCopy);
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
            return convertToNSArray(rowsCopy);
        } else if (is<AccessibilityARIAGridRow>(*m_object)) {
            AccessibilityObject::AccessibilityChildrenVector rowsCopy;
            downcast<AccessibilityARIAGridRow>(*m_object).disclosedRows(rowsCopy);
            return convertToNSArray(rowsCopy);
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
    
    if ((m_object->isListBox() || m_object->isList()) && [attributeName isEqualToString:NSAccessibilityOrientationAttribute])
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
        return [NSNumber numberWithInt:m_object->blockquoteLevel()];
    if ([attributeName isEqualToString:@"AXTableLevel"])
        return [NSNumber numberWithInt:m_object->tableLevel()];
    
    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector linkedUIElements;
        m_object->linkedUIElements(linkedUIElements);
        return convertToNSArray(linkedUIElements);
    }
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return [NSNumber numberWithBool:m_object->isSelected()];
    
    if ([attributeName isEqualToString: NSAccessibilityARIACurrentAttribute]) {
        switch (m_object->ariaCurrentState()) {
        case ARIACurrentFalse:
            return @"false";
        case ARIACurrentPage:
            return @"page";
        case ARIACurrentStep:
            return @"step";
        case ARIACurrentLocation:
            return @"location";
        case ARIACurrentTime:
            return @"time";
        case ARIACurrentDate:
            return @"date";
        default:
        case ARIACurrentTrue:
            return @"true";
        }
    }
    
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
            return [self baseAccessibilityTitle];
        
        return m_object->valueDescription();
    }
    
    if ([attributeName isEqualToString:NSAccessibilityOrientationAttribute]) {
        AccessibilityOrientation elementOrientation = m_object->orientation();
        if (elementOrientation == AccessibilityOrientationVertical)
            return NSAccessibilityVerticalOrientationValue;
        if (elementOrientation == AccessibilityOrientationHorizontal)
            return NSAccessibilityHorizontalOrientationValue;
        if (elementOrientation == AccessibilityOrientationUndefined)
            return NSAccessibilityUnknownOrientationValue;
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityHorizontalScrollBarAttribute]) {
        AccessibilityObject* scrollBar = m_object->scrollBar(AccessibilityOrientationHorizontal);
        if (scrollBar)
            return scrollBar->wrapper();
        return nil;
    }
    if ([attributeName isEqualToString:NSAccessibilityVerticalScrollBarAttribute]) {
        AccessibilityObject* scrollBar = m_object->scrollBar(AccessibilityOrientationVertical);
        if (scrollBar)
            return scrollBar->wrapper();
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilitySortDirectionAttribute]) {
        switch (m_object->sortDirection()) {
            case SortDirectionAscending:
                return NSAccessibilityAscendingSortDirectionValue;
            case SortDirectionDescending:
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
        return convertToNSArray(ariaOwns);
    }
    
    if ([attributeName isEqualToString:NSAccessibilityARIAPosInSetAttribute])
        return [NSNumber numberWithInt:m_object->ariaPosInSet()];
    if ([attributeName isEqualToString:NSAccessibilityARIASetSizeAttribute])
        return [NSNumber numberWithInt:m_object->ariaSetSize()];
    
    if ([attributeName isEqualToString:NSAccessibilityGrabbedAttribute])
        return [NSNumber numberWithBool:m_object->isARIAGrabbed()];
    
    if ([attributeName isEqualToString:NSAccessibilityDropEffectsAttribute]) {
        Vector<String> dropEffects;
        m_object->determineARIADropEffects(dropEffects);
        return convertStringsToNSArray(dropEffects);
    }
    
    if ([attributeName isEqualToString:NSAccessibilityPlaceholderValueAttribute])
        return m_object->placeholderValue();

    if ([attributeName isEqualToString:NSAccessibilityValueAutofillAvailableAttribute])
        return @(m_object->isValueAutofillAvailable());
    
    if ([attributeName isEqualToString:NSAccessibilityValueAutofilledAttribute])
        return @(m_object->isValueAutofilled());

    if ([attributeName isEqualToString:NSAccessibilityHasPopupAttribute])
        return [NSNumber numberWithBool:m_object->ariaHasPopup()];
    
    // ARIA Live region attributes.
    if ([attributeName isEqualToString:NSAccessibilityARIALiveAttribute])
        return m_object->ariaLiveRegionStatus();
    if ([attributeName isEqualToString:NSAccessibilityARIARelevantAttribute])
        return m_object->ariaLiveRegionRelevant();
    if ([attributeName isEqualToString:NSAccessibilityARIAAtomicAttribute])
        return [NSNumber numberWithBool:m_object->ariaLiveRegionAtomic()];
    if ([attributeName isEqualToString:NSAccessibilityElementBusyAttribute])
        return [NSNumber numberWithBool:m_object->ariaLiveRegionBusy()];
    
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
    
    // this is used only by DumpRenderTree for testing
    if ([attributeName isEqualToString:@"AXClickPoint"])
        return [NSValue valueWithPoint:m_object->clickPoint()];
    
    // This is used by DRT to verify CSS3 speech works.
    if ([attributeName isEqualToString:@"AXDRTSpeechAttribute"]) {
        ESpeak speakProperty = m_object->speakProperty();
        switch (speakProperty) {
            case SpeakNone:
                return @"none";
            case SpeakSpellOut:
                return @"spell-out";
            case SpeakDigits:
                return @"digits";
            case SpeakLiteralPunctuation:
                return @"literal-punctuation";
            case SpeakNoPunctuation:
                return @"no-punctuation";
            default:
            case SpeakNormal:
                return @"normal";
        }
    }
    
    // Used by DRT to find an accessible node by its element id.
    if ([attributeName isEqualToString:@"AXDRTElementIdAttribute"])
        return m_object->getAttribute(idAttr);
    
    if (m_object->isWebArea() && [attributeName isEqualToString:NSAccessibilityPreventKeyboardDOMEventDispatchAttribute])
        return [NSNumber numberWithBool:m_object->preventKeyboardDOMEventDispatch()];
    
    if (m_object->isWebArea() && [attributeName isEqualToString:NSAccessibilityCaretBrowsingEnabledAttribute])
        return [NSNumber numberWithBool:m_object->caretBrowsingEnabled()];
    
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
        return convertToNSArray(ariaControls);
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
    
    RefPtr<AccessibilityObject> focusedObj = m_object->focusedUIElement();
    
    if (!focusedObj)
        return nil;
    
    return focusedObj->wrapper();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (![self updateObjectBackingStore])
        return nil;
    
    m_object->updateChildrenIfNecessary();
    RefPtr<AccessibilityObject> axObject = m_object->accessibilityHitTest(IntPoint(point));
    if (axObject)
        return NSAccessibilityUnignoredAncestor(axObject->wrapper());
    return NSAccessibilityUnignoredAncestor(self);
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
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
- (BOOL)accessibilityIsIgnored
{
    if (![self updateObjectBackingStore])
        return YES;
    
    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityIsIgnored];
    return m_object->accessibilityIsIgnored();
}

- (NSArray* )accessibilityParameterizedAttributeNames
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (void)accessibilityPerformPressAction
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
    if (![self updateObjectBackingStore])
        return;
    
    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityIncrementAction];
    else
        m_object->increment();
}

- (void)accessibilityPerformDecrementAction
{
    if (![self updateObjectBackingStore])
        return;
    
    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityDecrementAction];
    else
        m_object->decrement();
}

#pragma clang diagnostic pop

- (void)accessibilityPerformShowMenuAction
{
    if (m_object->roleValue() == ComboBoxRole)
        m_object->setIsExpanded(true);
    else {
        // This needs to be performed in an iteration of the run loop that did not start from an AX call.
        // If it's the same run loop iteration, the menu open notification won't be sent
        [self performSelector:@selector(accessibilityShowContextMenu) withObject:nil afterDelay:0.0];
    }
}

- (void)accessibilityShowContextMenu
{
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
    
    page->contextMenuController().showContextMenuAt(&page->mainFrame(), rect.center());
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

- (void)accessibilityPerformAction:(NSString*)action
{
    if (![self updateObjectBackingStore])
        return;
    
    if ([action isEqualToString:NSAccessibilityPressAction])
        [self accessibilityPerformPressAction];
    
    else if ([action isEqualToString:NSAccessibilityShowMenuAction])
        [self accessibilityPerformShowMenuAction];
    
    else if ([action isEqualToString:NSAccessibilityIncrementAction])
        [self accessibilityPerformIncrementAction];
    
    else if ([action isEqualToString:NSAccessibilityDecrementAction])
        [self accessibilityPerformDecrementAction];
    
    else if ([action isEqualToString:NSAccessibilityScrollToVisibleAction])
        [self accessibilityScrollToVisible];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
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
        if (focus && m_object->isWebArea() && !m_object->document()->frame()->selection().isFocusedAndActive()) {
            FrameView* frameView = m_object->documentFrameView();
            Page* page = m_object->page();
            if (page && frameView) {
                ChromeClient& chromeClient = page->chrome().client();
                chromeClient.focus();
                if (frameView->platformWidget())
                    chromeClient.makeFirstResponder(frameView->platformWidget());
                else
                    chromeClient.makeFirstResponder();
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
        if (!array || m_object->roleValue() != ListBoxRole)
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

- (NSString*)accessibilityActionDescription:(NSString*)action
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
    return [self doAXAttributedStringForTextMarkerRange:[self textMarkerRangeFromRange:webRange]];
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

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
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
        return convertToNSArray(results);
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
        CharacterOffset start = cache->characterOffsetForIndex(range.location, m_object);
        CharacterOffset end = cache->characterOffsetForIndex(range.location + range.length, m_object);
        if (start.isNull() || end.isNull())
            return nil;
        RefPtr<Range> range = cache->rangeForUnorderedCharacterOffsets(start, end);
        return m_object->stringForRange(range);
    }
    
    if ([attribute isEqualToString:@"AXAttributedStringForTextMarkerRange"])
        return [self doAXAttributedStringForTextMarkerRange:textMarkerRange];
    
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
    
    const auto& children = m_object->children();
    
    if (children.isEmpty())
        return [[self renderWidgetChildren] indexOfObject:child];
    
    unsigned count = children.size();
    for (unsigned k = 0; k < count; ++k) {
        WebAccessibilityObjectWrapper* wrapper = children[k]->wrapper();
        if (wrapper == child || (children[k]->isAttachment() && [wrapper attachmentView] == child))
            return k;
    }
    
    return NSNotFound;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    if (![self updateObjectBackingStore])
        return 0;
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        // Tree items object returns a different set of children than those that are in children()
        // because an AXOutline (the mac role is becomes) has some odd stipulations.
        if (m_object->isTree() || m_object->isTreeItem())
            return [[self accessibilityAttributeValue:NSAccessibilityChildrenAttribute] count];
        
        const auto& children = m_object->children();
        if (children.isEmpty())
            return [[self renderWidgetChildren] count];
        
        return children.size();
    }
    
    return [super accessibilityArrayAttributeCount:attribute];
}
#pragma clang diagnostic pop

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount
{
    if (![self updateObjectBackingStore])
        return nil;
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        if (m_object->children().isEmpty()) {
            NSArray *children = [self renderWidgetChildren];
            if (!children)
                return nil;
            
            NSUInteger childCount = [children count];
            if (index >= childCount)
                return nil;
            
            NSUInteger arrayLength = std::min(childCount - index, maxCount);
            return [children subarrayWithRange:NSMakeRange(index, arrayLength)];
        } else if (m_object->isTree() || m_object->isTreeItem()) {
            // Tree objects return their rows as their children & tree items return their contents sans rows.
            // We can use the original method in this case.
            return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
        }
        
        const auto& children = m_object->children();
        unsigned childCount = children.size();
        if (index >= childCount)
            return nil;
        
        unsigned available = std::min(childCount - index, maxCount);
        
        NSMutableArray *subarray = [NSMutableArray arrayWithCapacity:available];
        for (unsigned added = 0; added < available; ++index, ++added) {
            WebAccessibilityObjectWrapper* wrapper = children[index]->wrapper();
            if (wrapper) {
                // The attachment view should be returned, otherwise AX palindrome errors occur.
                if (children[index]->isAttachment() && [wrapper attachmentView])
                    [subarray addObject:[wrapper attachmentView]];
                else
                    [subarray addObject:wrapper];
            }
        }
        
        return subarray;
    }
    
    return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
}

@end

#endif // HAVE(ACCESSIBILITY)
