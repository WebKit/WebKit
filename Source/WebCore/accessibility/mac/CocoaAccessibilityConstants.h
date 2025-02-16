/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

//
// Roles
//

#define NSAccessibilityHeadingRole @"AXHeading"
#define NSAccessibilityDateTimeAreaRole @"AXDateTimeArea"
#define NSAccessibilityImageMapRole @"AXImageMap"
#define NSAccessibilityIncrementorArrowRole @"AXIncrementorArrow"
#define NSAccessibilityListMarkerRole @"AXListMarker"
#define NSAccessibilityWebAreaRole @"AXWebArea"

//
// Subroles
//

#define NSAccessibilityApplicationAlertSubrole @"AXApplicationAlert"
#define NSAccessibilityApplicationAlertDialogSubrole @"AXApplicationAlertDialog"
#define NSAccessibilityApplicationDialogSubrole @"AXApplicationDialog"
#define NSAccessibilityApplicationGroupSubrole @"AXApplicationGroup"
#define NSAccessibilityApplicationLogSubrole @"AXApplicationLog"
#define NSAccessibilityApplicationMarqueeSubrole @"AXApplicationMarquee"
#define NSAccessibilityApplicationStatusSubrole @"AXApplicationStatus"
#define NSAccessibilityApplicationTimerSubrole @"AXApplicationTimer"
#define NSAccessibilityAudioSubrole @"AXAudio"
#define NSAccessibilityCiteStyleGroupSubrole @"AXCiteStyleGroup"
#define NSAccessibilityCodeStyleGroupSubrole @"AXCodeStyleGroup"
#define NSAccessibilityContentSeparatorSubrole @"AXContentSeparator"
#define NSAccessibilityDefinitionSubrole @"AXDefinition"
#define NSAccessibilityDeleteStyleGroupSubrole @"AXDeleteStyleGroup"
#define NSAccessibilityDetailsSubrole @"AXDetails"
#define NSAccessibilityDocumentArticleSubrole @"AXDocumentArticle"
#define NSAccessibilityDocumentMathSubrole @"AXDocumentMath"
#define NSAccessibilityDocumentNoteSubrole @"AXDocumentNote"
#define NSAccessibilityEmptyGroupSubrole @"AXEmptyGroup"
#define NSAccessibilityFieldsetSubrole @"AXFieldset"
#define NSAccessibilityFileUploadButtonSubrole @"AXFileUploadButton"
#define NSAccessibilityFooterSubrole @"AXFooter"
#define NSAccessibilityInsertStyleGroupSubrole @"AXInsertStyleGroup"
#define NSAccessibilityKeyboardInputStyleGroupSubrole @"AXKeyboardInputStyleGroup"
#define NSAccessibilityLandmarkBannerSubrole @"AXLandmarkBanner"
#define NSAccessibilityLandmarkComplementarySubrole @"AXLandmarkComplementary"
#define NSAccessibilityLandmarkContentInfoSubrole @"AXLandmarkContentInfo"
#define NSAccessibilityLandmarkMainSubrole @"AXLandmarkMain"
#define NSAccessibilityLandmarkNavigationSubrole @"AXLandmarkNavigation"
#define NSAccessibilityLandmarkRegionSubrole @"AXLandmarkRegion"
#define NSAccessibilityLandmarkSearchSubrole @"AXLandmarkSearch"
#define NSAccessibilityMathFenceOperatorSubrole @"AXMathFenceOperator"
#define NSAccessibilityMathFencedSubrole @"AXMathFenced"
#define NSAccessibilityMathFractionSubrole @"AXMathFraction"
#define NSAccessibilityMathIdentifierSubrole @"AXMathIdentifier"
#define NSAccessibilityMathMultiscriptSubrole @"AXMathMultiscript"
#define NSAccessibilityMathNumberSubrole @"AXMathNumber"
#define NSAccessibilityMathOperatorSubrole @"AXMathOperator"
#define NSAccessibilityMathRootSubrole @"AXMathRoot"
#define NSAccessibilityMathRowSubrole @"AXMathRow"
#define NSAccessibilityMathSeparatorOperatorSubrole @"AXMathSeparatorOperator"
#define NSAccessibilityMathSquareRootSubrole @"AXMathSquareRoot"
#define NSAccessibilityMathSubscriptSuperscriptSubrole @"AXMathSubscriptSuperscript"
#define NSAccessibilityMathTableSubrole @"AXMathTable"
#define NSAccessibilityMathTableCellSubrole @"AXMathTableCell"
#define NSAccessibilityMathTableRowSubrole @"AXMathTableRow"
#define NSAccessibilityMathTextSubrole @"AXMathText"
#define NSAccessibilityMathUnderOverSubrole @"AXMathUnderOver"
#define NSAccessibilityMeterSubrole @"AXMeter"
#define NSAccessibilityModelSubrole @"AXModel"
#define NSAccessibilityPreformattedStyleGroupSubrole @"AXPreformattedStyleGroup"
#define NSAccessibilityRubyInlineSubrole @"AXRubyInline"
#define NSAccessibilityRubyTextSubrole @"AXRubyText"
#define NSAccessibilitySampleStyleGroupSubrole @"AXSampleStyleGroup"
#define NSAccessibilitySubscriptStyleGroupSubrole @"AXSubscriptStyleGroup"
#define NSAccessibilitySuggestionSubrole @"AXSuggestion"
#define NSAccessibilitySummarySubrole @"AXSummary"
#define NSAccessibilitySuperscriptStyleGroupSubrole @"AXSuperscriptStyleGroup"
#define NSAccessibilityTabPanelSubrole @"AXTabPanel"
#define NSAccessibilityTermSubrole @"AXTerm"
#define NSAccessibilityTimeGroupSubrole @"AXTimeGroup"
#define NSAccessibilityUserInterfaceTooltipSubrole @"AXUserInterfaceTooltip"
#define NSAccessibilityVariableStyleGroupSubrole @"AXVariableStyleGroup"
#define NSAccessibilityVideoSubrole @"AXVideo"
#define NSAccessibilityWebApplicationSubrole @"AXWebApplication"

//
// Attributes
//

#define NSAccessibilityARIAAtomicAttribute @"AXARIAAtomic"
#define NSAccessibilityARIAColumnCountAttribute @"AXARIAColumnCount"
#define NSAccessibilityARIAColumnIndexAttribute @"AXARIAColumnIndex"
#define NSAccessibilityARIACurrentAttribute @"AXARIACurrent"
#define NSAccessibilityARIALiveAttribute @"AXARIALive"
#define NSAccessibilityARIAPosInSetAttribute @"AXARIAPosInSet"
#define NSAccessibilityARIAPressedIsPresentAttribute @"AXARIAPressedIsPresent"
#define NSAccessibilityARIARelevantAttribute @"AXARIARelevant"
#define NSAccessibilityARIARoleAttribute @"AXARIARole"
#define NSAccessibilityARIARowCountAttribute @"AXARIARowCount"
#define NSAccessibilityARIARowIndexAttribute @"AXARIARowIndex"
#define NSAccessibilityARIASetSizeAttribute @"AXARIASetSize"
#define NSAccessibilityAccessKeyAttribute @"AXAccessKey"
#define NSAccessibilityActiveElementAttribute @"AXActiveElement"
#define NSAccessibilityAssociatedPluginParentAttribute @"_AXAssociatedPluginParent"
#define NSAccessibilityAutocompleteValueAttribute @"AXAutocompleteValue"
#define NSAccessibilityAutoInteractableAttribute @"AXAutoInteractable"
#define NSAccessibilityBlockQuoteLevelAttribute @"AXBlockQuoteLevel"
#define NSAccessibilityBrailleLabelAttribute @"AXBrailleLabel"
#define NSAccessibilityBrailleRoleDescriptionAttribute @"AXBrailleRoleDescription"
#define NSAccessibilityCaretBrowsingEnabledAttribute @"AXCaretBrowsingEnabled"
#define NSAccessibilityChildrenInNavigationOrderAttribute @"AXChildrenInNavigationOrder"
#define NSAccessibilityClickPointAttribute @"AXClickPoint"
#define NSAccessibilityControllerForAttribute @"AXControllerFor"
#define NSAccessibilityControllersAttribute @"AXControllers"
#define NSAccessibilityDOMClassListAttribute @"AXDOMClassList"
#define NSAccessibilityDOMIdentifierAttribute @"AXDOMIdentifier"
#define NSAccessibilityDRTSpeechAttributeAttribute @"AXDRTSpeechAttribute"
#define NSAccessibilityDateTimeComponentsAttribute @"AXDateTimeComponents"
#define NSAccessibilityDateTimeComponentsTypeAttribute @"AXDateTimeComponentsType"
#define NSAccessibilityDatetimeValueAttribute @"AXDateTimeValue"
#define NSAccessibilityDescribedByAttribute @"AXDescribedBy"
#define NSAccessibilityDescriptionForAttribute @"AXDescriptionFor"
#define NSAccessibilityDetailsElementsAttribute @"AXDetailsElements"
#define NSAccessibilityDetailsForAttribute @"AXDetailsFor"
#define NSAccessibilityDropEffectsAttribute @"AXDropEffects"
#define NSAccessibilityEditableAncestorAttribute @"AXEditableAncestor"
#define NSAccessibilityElementBusyAttribute @"AXElementBusy"
#define NSAccessibilityEmbeddedImageDescriptionAttribute @"AXEmbeddedImageDescription"
#define NSAccessibilityEndTextMarkerAttribute @"AXEndTextMarker"
#define NSAccessibilityEndTextMarkerForTextMarkerRangeAttribute @"_AXEndTextMarkerForTextMarkerRange"
#define NSAccessibilityErrorMessageElementsAttribute @"AXErrorMessageElements"
#define NSAccessibilityErrorMessageForAttribute @"AXErrorMessageFor"
#define NSAccessibilityExpandedTextValueAttribute @"AXExpandedTextValue"
#define NSAccessibilityFlowFromAttribute @"AXFlowFrom"
#define NSAccessibilityFlowToAttribute @"AXFlowTo"
#define NSAccessibilityFocusableAncestorAttribute @"AXFocusableAncestor"
#define NSAccessibilityGrabbedAttribute @"AXGrabbed"
#define NSAccessibilityHasDocumentRoleAncestorAttribute @"AXHasDocumentRoleAncestor"
#define NSAccessibilityHasPopupAttribute @"AXHasPopup"
#define NSAccessibilityHasWebApplicationAncestorAttribute @"AXHasWebApplicationAncestor"
#define NSAccessibilityHeadingLevelAttribute @"AXHeadingLevel"
#define NSAccessibilityHighestEditableAncestorAttribute @"AXHighestEditableAncestor"
#define NSAccessibilityImageOverlayElementsAttribute @"AXImageOverlayElements"
#define NSAccessibilityInfoStringForTestingAttribute @"AXInfoStringForTesting"
#define NSAccessibilityInlineTextAttribute @"AXInlineText"
#define NSAccessibilityInvalidAttribute @"AXInvalid"
#define NSAccessibilityIsInCellAttribute @"AXIsInCell"
#define NSAccessibilityIsInDescriptionListDetailAttribute @"AXIsInDescriptionListDetail"
#define NSAccessibilityIsInDescriptionListTermAttribute @"AXIsInDescriptionListTerm"
#define NSAccessibilityIsInTableAttribute @"_AXIsInTable"
#define NSAccessibilityIsIndeterminateAttribute @"AXIsIndeterminate"
#define NSAccessibilityIsMultiSelectableAttribute @"AXIsMultiSelectable"
#define NSAccessibilityIsOnScreenAttribute @"AXIsOnScreen"
#define NSAccessibilityIsRemoteFrameAttribute @"AXIsRemoteFrame"
#define NSAccessibilityKeyShortcutsAttribute @"AXKeyShortcutsValue"
#define NSAccessibilityLabelForAttribute @"AXLabelFor"
#define NSAccessibilityLabelledByAttribute @"AXLabelledBy"
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#define NSAccessibilityLayoutCountAttribute @"AXLayoutCount"
#define NSAccessibilityLineRectsAndTextAttribute @"AXLineRectsAndText"
#define NSAccessibilityLinkRelationshipTypeAttribute @"AXLinkRelationshipType"
#define NSAccessibilityLinkUIElementsAttribute @"AXLinkUIElements"
#define NSAccessibilityLoadedAttribute @"AXLoaded"
#define NSAccessibilityLoadingProgressAttribute @"AXLoadingProgress"
#define NSAccessibilityOwnersAttribute @"AXOwners"
#define NSAccessibilityOwnsAttribute @"AXOwns"
#define NSAccessibilityPathAttribute @"AXPath"
#define NSAccessibilityPopupValueAttribute @"AXPopupValue"
#define NSAccessibilityPreventKeyboardDOMEventDispatchAttribute @"AXPreventKeyboardDOMEventDispatch"
#define NSAccessibilityPrimaryScreenHeightAttribute @"_AXPrimaryScreenHeight"
#define NSAccessibilityRelativeFrameAttribute @"AXRelativeFrame"
#define NSAccessibilitySelectedCellsAttribute @"AXSelectedCells"
#define NSAccessibilitySelectedTextMarkerRangeAttribute @"AXSelectedTextMarkerRange"
#define NSAccessibilityStartTextMarkerAttribute @"AXStartTextMarker"
#define NSAccessibilityStartTextMarkerForTextMarkerRangeAttribute @"_AXStartTextMarkerForTextMarkerRange"
#define NSAccessibilityStringValueAttribute @"AXStringValue"
#define NSAccessibilityTableLevelAttribute @"AXTableLevel"
#define NSAccessibilityTextCompletionAttribute @"AXTextCompletion"
#define NSAccessibilityTextInputMarkedRangeAttribute @"AXTextInputMarkedRange"
#define NSAccessibilityTextInputMarkedTextMarkerRangeAttribute @"AXTextInputMarkedTextMarkerRange"
#define NSAccessibilityTextMarkerDebugDescriptionAttribute @"AXTextMarkerDebugDescription"
#define NSAccessibilityTextMarkerNodeDebugDescriptionAttribute @"AXTextMarkerNodeDebugDescription"
#define NSAccessibilityTextMarkerNodeTreeDebugDescriptionAttribute @"AXTextMarkerNodeTreeDebugDescription"
#define NSAccessibilityTextMarkerRangeDebugDescriptionAttribute @"AXTextMarkerRangeDebugDescription"
#define NSAccessibilityTextMarkerRangeForNSRangeAttribute @"_AXTextMarkerRangeForNSRange"
#define NSAccessibilityValueAutofillAvailableAttribute @"AXValueAutofillAvailable"
#define NSAccessibilityValueAutofillTypeAttribute @"AXValueAutofillType"
#define NSAccessibilityVisitedAttribute @"AXVisited"

//
// Parameterized Attributes
//

#define NSAccessibilityAttributedStringForTextMarkerRangeAttribute @"AXAttributedStringForTextMarkerRange"
#define NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsAttribute @"AXAttributedStringForTextMarkerRangeWithOptions"
#define NSAccessibilityBoundsForTextMarkerRangeAttribute @"AXBoundsForTextMarkerRange"
#define NSAccessibilityConvertRelativeFrameParameterizedAttribute @"AXConvertRelativeFrame"
#define NSAccessibilityEndTextMarkerForBoundsAttribute @"AXEndTextMarkerForBounds"
#define NSAccessibilityIndexForTextMarkerAttribute @"AXIndexForTextMarker"
#define NSAccessibilityLeftLineTextMarkerRangeForTextMarkerAttribute @"AXLeftLineTextMarkerRangeForTextMarker"
#define NSAccessibilityLeftWordTextMarkerRangeForTextMarkerAttribute @"AXLeftWordTextMarkerRangeForTextMarker"
#define NSAccessibilityLengthForTextMarkerRangeAttribute @"AXLengthForTextMarkerRange"
#define NSAccessibilityLineForTextMarkerAttribute @"AXLineForTextMarker"
#define NSAccessibilityLineTextMarkerRangeForTextMarkerAttribute @"AXLineTextMarkerRangeForTextMarker"
#define NSAccessibilityMisspellingTextMarkerRangeAttribute @"AXMisspellingTextMarkerRange"
#define NSAccessibilityNextLineEndTextMarkerForTextMarkerAttribute @"AXNextLineEndTextMarkerForTextMarker"
#define NSAccessibilityNextParagraphEndTextMarkerForTextMarkerAttribute @"AXNextParagraphEndTextMarkerForTextMarker"
#define NSAccessibilityNextSentenceEndTextMarkerForTextMarkerAttribute @"AXNextSentenceEndTextMarkerForTextMarker"
#define NSAccessibilityNextTextMarkerForTextMarkerAttribute @"AXNextTextMarkerForTextMarker"
#define NSAccessibilityNextWordEndTextMarkerForTextMarkerAttribute @"AXNextWordEndTextMarkerForTextMarker"
#define NSAccessibilityParagraphTextMarkerRangeForTextMarkerAttribute @"AXParagraphTextMarkerRangeForTextMarker"
#define NSAccessibilityPreviousLineStartTextMarkerForTextMarkerAttribute @"AXPreviousLineStartTextMarkerForTextMarker"
#define NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerAttribute @"AXPreviousParagraphStartTextMarkerForTextMarker"
#define NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerAttribute @"AXPreviousSentenceStartTextMarkerForTextMarker"
#define NSAccessibilityPreviousTextMarkerForTextMarkerAttribute @"AXPreviousTextMarkerForTextMarker"
#define NSAccessibilityPreviousWordStartTextMarkerForTextMarkerAttribute @"AXPreviousWordStartTextMarkerForTextMarker"
#define NSAccessibilityRangesForSearchPredicateParameterizedAttribute @"AXRangesForSearchPredicate"
#define NSAccessibilityRightLineTextMarkerRangeForTextMarkerAttribute @"AXRightLineTextMarkerRangeForTextMarker"
#define NSAccessibilityRightWordTextMarkerRangeForTextMarkerAttribute @"AXRightWordTextMarkerRangeForTextMarker"
#define NSAccessibilitySentenceTextMarkerRangeForTextMarkerAttribute @"AXSentenceTextMarkerRangeForTextMarker"
#define NSAccessibilityStartTextMarkerForBoundsAttribute @"AXStartTextMarkerForBounds"
#define NSAccessibilityStringForTextMarkerRangeAttribute @"AXStringForTextMarkerRange"
#define NSAccessibilityStyleTextMarkerRangeForTextMarkerAttribute @"AXStyleTextMarkerRangeForTextMarker"
#define NSAccessibilityTextMarkerForIndexAttribute @"AXTextMarkerForIndex"
#define NSAccessibilityTextMarkerForPositionAttribute @"AXTextMarkerForPosition" // FIXME: should be AXTextMarkerForPoint.
#define NSAccessibilityTextMarkerIsNullParameterizedAttribute @"AXTextMarkerIsNull"
#define NSAccessibilityTextMarkerIsValidAttribute @"AXTextMarkerIsValid"
#define NSAccessibilityTextMarkerRangeForLineAttribute @"AXTextMarkerRangeForLine"
#define NSAccessibilityTextMarkerRangeForTextMarkersAttribute @"AXTextMarkerRangeForTextMarkers"
#define NSAccessibilityTextMarkerRangeForUIElementAttribute @"AXTextMarkerRangeForUIElement"
#define NSAccessibilityTextMarkerRangeForUnorderedTextMarkersAttribute @"AXTextMarkerRangeForUnorderedTextMarkers"
#define NSAccessibilityTextMarkerRangeIsValidParameterizedAttribute @"AXTextMarkerRangeIsValid"
#define NSAccessibilityUIElementForTextMarkerAttribute @"AXUIElementForTextMarker"
#define NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute @"AXUIElementsForSearchPredicate"

//
// Actions
//

#define NSAccessibilityDismissAction @"AXDismissAction"
#define NSAccessibilityScrollToVisibleAction @"AXScrollToVisible"
#define NSAccessibilitySyncDecrementAction @"AXSyncDecrementAction"
#define NSAccessibilitySyncIncrementAction @"AXSyncIncrementAction"
#define NSAccessibilitySyncPressAction @"AXSyncPressAction"

//
// Attributed string attribute names
//

#define NSAccessibilityDidSpellCheckAttribute @"AXDidSpellCheck"
#define NSAccessibilityFontBoldAttribute @"AXFontBold"
#define NSAccessibilityFontItalicAttribute @"AXFontItalic"
#define NSAccessibilityHighlightAttribute @"AXHighlight"
#define NSAccessibilityIsSuggestedInsertionAttribute @"AXIsSuggestedInsertion"
#define NSAccessibilityIsSuggestedDeletionAttribute @"AXIsSuggestedDeletion"
#define NSAccessibilityIsSuggestionAttribute @"AXIsSuggestion"

//
// Notifications
//

#define NSAccessibilityActiveElementChangedNotification @"AXActiveElementChanged"
#define NSAccessibilityAutocorrectionOccurredNotification @"AXAutocorrectionOccurred"
#define NSAccessibilityCurrentStateChangedNotification @"AXCurrentStateChanged"
#define NSAccessibilityDRTNotificationNotification @"AXDRTNotification"
#define NSAccessibilityDraggingDestinationDragAcceptedNotification CFSTR("AXDraggingDestinationDragAccepted")
#define NSAccessibilityDraggingDestinationDragNotAcceptedNotification CFSTR("AXDraggingDestinationDragNotAccepted")
#define NSAccessibilityDraggingDestinationDropAllowedNotification CFSTR("AXDraggingDestinationDropAllowed")
#define NSAccessibilityDraggingDestinationDropNotAllowedNotification CFSTR("AXDraggingDestinationDropNotAllowed")
#define NSAccessibilityDraggingSourceDragBeganNotification CFSTR("AXDraggingSourceDragBegan")
#define NSAccessibilityDraggingSourceDragEndedNotification CFSTR("AXDraggingSourceDragEnded")
#define NSAccessibilityElementBusyChangedNotification @"AXElementBusyChanged"
#define NSAccessibilityExpandedChangedNotification @"AXExpandedChanged"
#define NSAccessibilityFocusChangedNotification @"AXFocusChanged"
#define NSAccessibilityImageOverlayChangedNotification @"AXImageOverlayChanged"
#define NSAccessibilityInvalidStatusChangedNotification @"AXInvalidStatusChanged"
#define NSAccessibilityLayoutCompleteNotification @"AXLayoutComplete"
#define NSAccessibilityLiveRegionChangedNotification @"AXLiveRegionChanged"
#define NSAccessibilityLiveRegionCreatedNotification @"AXLiveRegionCreated"
#define NSAccessibilityLoadCompleteNotification @"AXLoadComplete"
#define NSAccessibilityPressDidFailNotification @"AXPressDidFail"
#define NSAccessibilityPressDidSucceedNotification @"AXPressDidSucceed"
#define NSAccessibilitySortDirectionChangedNotification @"AXSortDirectionChanged"
#define NSAccessibilityTextInputMarkingSessionBeganNotification @"AXTextInputMarkingSessionBegan"
#define NSAccessibilityTextInputMarkingSessionEndedNotification @"AXTextInputMarkingSessionEnded"

//
// Additional attributes in text change notifications
//

#define NSAccessibilityTextStateChangeTypeKey @"AXTextStateChangeType"
#define NSAccessibilityTextStateSyncKey @"AXTextStateSync"
#define NSAccessibilityTextSelectionDirection @"AXTextSelectionDirection"
#define NSAccessibilityTextSelectionGranularity @"AXTextSelectionGranularity"
#define NSAccessibilityTextSelectionChangedFocus @"AXTextSelectionChangedFocus"
#define NSAccessibilityTextEditType @"AXTextEditType"
#define NSAccessibilityTextChangeValues @"AXTextChangeValues"
#define NSAccessibilityTextChangeValue @"AXTextChangeValue"
#define NSAccessibilityTextChangeValueLength @"AXTextChangeValueLength"
#define NSAccessibilityTextChangeValueStartMarker @"AXTextChangeValueStartMarker"
#define NSAccessibilityTextChangeElement @"AXTextChangeElement"

//
// For use with NSAccessibilityMisspellingTextMarkerRangeAttribute
//

#define NSAccessibilityStartTextMarkerRangeParam @"AXStartTextMarkerRange"
#define NSAccessibilitySpellCheckParam @"AXSpellCheck"

//
// For use with NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsAttribute
//

#define NSAccessibilityTextMarkerRangeParam @"AXTextMarkerRange"

//
// For use with search predicate parameterized attributes, such as:
//   NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute
//   NSAccessibilityRangesForSearchPredicateParameterizedAttribute
//

#define NSAccessibilitySearchCurrentElementKey @"AXStartElement"
#define NSAccessibilitySearchCurrentRangeKey @"AXStartRange"
#define NSAccessibilitySearchDirectionKey @"AXDirection"
#define NSAccessibilityImmediateDescendantsOnlyKey @"AXImmediateDescendantsOnly"
#define NSAccessibilitySearchResultsLimitKey @"AXResultsLimit"
#define NSAccessibilitySearchTextKey @"AXSearchText"
#define NSAccessibilityVisibleOnlyKey @"AXVisibleOnly"
#define NSAccessibilitySearchIdentifiersKey @"AXSearchKey"

// Values for NSAccessibilitySearchDirectionKey
#define NSAccessibilitySearchDirectionNext @"AXDirectionNext"

//
// Used for the return value from NSAccessibilityRangesForSearchPredicateParameterizedAttribute
//

#define NSAccessibilitySearchResultElementKey @"AXSearchResultElement"
#define NSAccessibilitySearchResultRangeKey @"AXSearchResultRange"

//
// For use with NSAccessibilitySelectTextWithCriteriaParameterizedAttribute
//

#define NSAccessibilityIntersectionWithSelectionRangeAttribute @"AXIntersectionWithSelectionRange"
#define NSAccessibilitySelectTextActivity @"AXSelectTextActivity"
#define NSAccessibilitySelectTextActivityFindAndCapitalize @"AXSelectTextActivityFindAndCapitalize"
#define NSAccessibilitySelectTextActivityFindAndLowercase @"AXSelectTextActivityFindAndLowercase"
#define NSAccessibilitySelectTextActivityFindAndReplace @"AXSelectTextActivityFindAndReplace"
#define NSAccessibilitySelectTextActivityFindAndSelect @"AXSelectTextActivityFindAndSelect"
#define NSAccessibilitySelectTextActivityFindAndUppercase @"AXSelectTextActivityFindAndUppercase"
#define NSAccessibilitySelectTextAmbiguityResolution @"AXSelectTextAmbiguityResolution"
#define NSAccessibilitySelectTextAmbiguityResolutionClosestAfterSelection @"AXSelectTextAmbiguityResolutionClosestAfterSelection"
#define NSAccessibilitySelectTextAmbiguityResolutionClosestBeforeSelection @"AXSelectTextAmbiguityResolutionClosestBeforeSelection"
#define NSAccessibilitySelectTextAmbiguityResolutionClosestToSelection @"AXSelectTextAmbiguityResolutionClosestToSelection"
#define NSAccessibilitySelectTextReplacementString @"AXSelectTextReplacementString"
#define NSAccessibilitySelectTextSearchStrings @"AXSelectTextSearchStrings"
#define NSAccessibilitySelectTextWithCriteriaParameterizedAttribute @"AXSelectTextWithCriteria"

//
// Text search
//

/* Performs a text search with the given parameters.
 Returns an NSArray of text marker ranges of the search hits.
 */
#define NSAccessibilitySearchTextWithCriteriaParameterizedAttribute @"AXSearchTextWithCriteria"

// NSArray of strings to search for.
#define NSAccessibilitySearchTextSearchStrings @"AXSearchTextSearchStrings"

// NSString specifying the start point of the search: selection, begin or end.
#define NSAccessibilitySearchTextStartFrom @"AXSearchTextStartFrom"

// Values for SearchTextStartFrom.
#define NSAccessibilitySearchTextStartFromBegin @"AXSearchTextStartFromBegin"
#define NSAccessibilitySearchTextStartFromSelection @"AXSearchTextStartFromSelection"
#define NSAccessibilitySearchTextStartFromEnd @"AXSearchTextStartFromEnd"

// NSString specifying the direction of the search: forward, backward, closest, all.
#define NSAccessibilitySearchTextDirection @"AXSearchTextDirection"

// Values for SearchTextDirection.
#define NSAccessibilitySearchTextDirectionForward @"AXSearchTextDirectionForward"
#define NSAccessibilitySearchTextDirectionBackward @"AXSearchTextDirectionBackward"
#define NSAccessibilitySearchTextDirectionClosest @"AXSearchTextDirectionClosest"
#define NSAccessibilitySearchTextDirectionAll @"AXSearchTextDirectionAll"

//
// Text operations
//

// Performs an operation on the given text.
#define NSAccessibilityTextOperationParameterizedAttribute @"AXTextOperation"

// Text on which to perform operation.
#define NSAccessibilityTextOperationMarkerRanges @"AXTextOperationMarkerRanges"

// The type of operation to be performed: select, replace, capitalize....
#define NSAccessibilityTextOperationType @"AXTextOperationType"

// Values for TextOperationType.
#define NSAccessibilityTextOperationSelect @"TextOperationSelect"
#define NSAccessibilityTextOperationReplace @"TextOperationReplace"
#define NSAccessibilityTextOperationReplacePreserveCase @"TextOperationReplacePreserveCase"
#define NSAccessibilityTextOperationCapitalize @"Capitalize"
#define NSAccessibilityTextOperationLowercase @"Lowercase"
#define NSAccessibilityTextOperationUppercase @"Uppercase"

// Replacement text for operation replace.
#define NSAccessibilityTextOperationReplacementString @"AXTextOperationReplacementString"

// Array of replacement text for operation replace. The array should contain
// the same number of items as the number of text operation ranges.
#define NSAccessibilityTextOperationIndividualReplacementStrings @"AXTextOperationIndividualReplacementStrings"

// Boolean specifying whether a smart replacement should be performed.
#define NSAccessibilityTextOperationSmartReplace @"AXTextOperationSmartReplace"

//
// Math-specific attributes
//

#define NSAccessibilityMathBaseAttribute @"AXMathBase"
#define NSAccessibilityMathFencedCloseAttribute @"AXMathFencedClose"
#define NSAccessibilityMathFencedOpenAttribute @"AXMathFencedOpen"
#define NSAccessibilityMathFractionDenominatorAttribute @"AXMathFractionDenominator"
#define NSAccessibilityMathFractionNumeratorAttribute @"AXMathFractionNumerator"
#define NSAccessibilityMathLineThicknessAttribute @"AXMathLineThickness"
#define NSAccessibilityMathOverAttribute @"AXMathOver"
#define NSAccessibilityMathPostscriptsAttribute @"AXMathPostscripts"
#define NSAccessibilityMathPrescriptsAttribute @"AXMathPrescripts"
#define NSAccessibilityMathRootIndexAttribute @"AXMathRootIndex"
#define NSAccessibilityMathRootRadicandAttribute @"AXMathRootRadicand"
#define NSAccessibilityMathSubscriptAttribute @"AXMathSubscript"
#define NSAccessibilityMathSuperscriptAttribute @"AXMathSuperscript"
#define NSAccessibilityMathUnderAttribute @"AXMathUnder"
