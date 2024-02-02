/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKAXTypes_h
#define WKAXTypes_h

#include <WebKit/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

enum AXNotification {
    // must be same as WebCore::AXObjectCache::AXNotification
    kWKAXAccessKeyChanged,
    kWKAXActiveDescendantChanged,
    kWKAXAnnouncementRequested,
    kWKAXAutocorrectionOccured,
    kWKAXAutofillTypeChanged,
    kWKAXCellSlotsChanged,
    kWKAXCheckedStateChanged,
    kWKAXChildrenChanged,
    kWKAXColumnCountChanged,
    kWKAXColumnIndexChanged,
    kWKAXColumnSpanChanged,
    kWKAXContentEditableAttributeChanged,
    kWKAXControlledObjectsChanged,
    kWKAXCurrentStateChanged,
    kWKAXDescribedByChanged,
    kWKAXDisabledStateChanged,
    kWKAXDropEffectChanged,
    kWKAXExtendedDescriptionChanged,
    kWKAXFlowToChanged,
    kWKAXFocusableStateChanged,
    kWKAXFocusedUIElementChanged,
    kWKAXFrameLoadComplete,
    kWKAXGrabbedStateChanged,
    kWKAXHasPopupChanged,
    kWKAXIdAttributeChanged,
    kWKAXImageOverlayChanged,
    kWKAXIsAtomicChanged,
    kWKAXKeyShortcutsChanged,
    kWKAXLanguageChanged,
    kWKAXLayoutComplete,
    kWKAXLevelChanged,
    kWKAXLoadComplete,
    kWKAXNameChanged,
    kWKAXNewDocumentLoadComplete,
    kWKAXPageScrolled,
    kWKAXPlaceholderChanged,
    kWKAXPopoverTargetChanged,
    kWKAXPositionInSetChanged,
    kWKAXRoleChanged,
    kWKAXRoleDescriptionChanged,
    kWKAXRowIndexChanged,
    kWKAXRowSpanChanged,
    kWKAXCellScopeChanged,
    kWKAXSelectedChildrenChanged,
    kWKAXSelectedCellsChanged,
    kWKAXSelectedStateChanged,
    kWKAXSelectedTextChanged,
    kWKAXSetSizeChanged,
    kWKAXTableHeadersChanged,
    kWKAXTextCompositionBegan,
    kWKAXTextCompositionEnded,
    kWKAXURLChanged,
    kWKAXValueChanged,
    kWKAXVisibilityChanged,
    kWKAXScrolledToAnchor,
    kWKAXLabelChanged,
    kWKAXLiveRegionCreated,
    kWKAXLiveRegionChanged,
    kWKAXLiveRegionRelevantChanged,
    kWKAXLiveRegionStatusChanged,
    kWKAXMaximumValueChanged,
    kWKAXMenuListItemSelected,
    kWKAXMenuListValueChanged,
    kWKAXMenuClosed,
    kWKAXMenuOpened,
    kWKAXMinimumValueChanged,
    kWKAXMultiSelectableStateChanged,
    kWKAXOrientationChanged,
    kWKAXRowCountChanged,
    kWKAXRowCollapsed,
    kWKAXRowExpanded,
    kWKAXExpandedChanged,
    kWKAXInvalidStatusChanged,
    kWKAXPressDidSucceed,
    kWKAXPressDidFail,
    kWKAXPressedStateChanged,
    kWKAXReadOnlyStatusChanged,
    kWKAXRequiredStatusChanged,
    kWKAXSortDirectionChanged,
    kWKAXTextChanged,
    kWKAXTextCompositionChanged,
    kWKAXTextSecurityChanged,
    kWKAXElementBusyChanged,
    kWKAXDraggingStarted,
    kWKAXDraggingEnded,
    kWKAXDraggingEnteredDropZone,
    kWKAXDraggingDropped,
    kWKAXDraggingExitedDropZone
};
typedef uint32_t WKAXNotification;

enum {
    // must be same as WebCore::AXObjectCache::AXTextChange
    kWKAXTextInserted,
    kWKAXTextDeleted,
    kWKAXTextAttributesChanged
};
typedef uint32_t WKAXTextChange;

enum {
    // must be same as WebCore::AXObjectCache::AXLoadingEvent
    kWKAXLoadingStarted,
    kWKAXLoadingReloaded,
    kWKAXLoadingFailed,
    kWKAXLoadingFinished,
};
typedef uint32_t WKAXLoadingEvent;

enum {
    // must be same as WebCore::AccessibilityRole
    kWKAnnotation = 1,
    kWKApplication,
    kWKApplicationAlert,
    kWKApplicationAlertDialog,
    kWKApplicationDialog,
    kWKApplicationGroup,
    kWKApplicationLog,
    kWKApplicationMarquee,
    kWKApplicationStatus,
    kWKApplicationTextGroup,
    kWKApplicationTimer,
    kWKAudio,
    kWKBlockquote,
    kWKBrowser,
    kWKBusyIndicator,
    kWKButton,
    kWKCanvas,
    kWKCaption,
    kWKCell,
    kWKCheckBox,
    kWKCode,
    kWKColorWell,
    kWKColumn,
    kWKColumnHeader,
    kWKComboBox,
    kWKDefinition,
    kWKDeletion,
    kWKDescriptionList,
    kWKDescriptionListTerm,
    kWKDescriptionListDetail,
    kWKDetails,
    kWKDirectory,
    kWKDisclosureTriangle,
    kWKDiv,
    kWKDocument,
    kWKDocumentArticle,
    kWKDocumentMath,
    kWKDocumentNote,
    kWKDrawer,
    kWKEditableText,
    kWKFeed,
    kWKFigure,
    kWKFooter,
    kWKFootnote,
    kWKForm,
    kWKGraphicsDocument,
    kWKGraphicsObject,
    kWKGraphicsSymbol,
    kWKGrid,
    kWKGridCell,
    kWKGroup,
    kWKGrowArea,
    kWKHeading,
    kWKHelpTag,
    kWKHorizontalRule,
    kWKIgnored,
    kWKInline,
    kWKImage,
    kWKImageMap,
    kWKImageMapLink,
    kWKIncrementor,
    kWKInsertion,
    kWKLabel,
    kWKLandmarkBanner,
    kWKLandmarkComplementary,
    kWKLandmarkContentInfo,
    kWKLandmarkDocRegion,
    kWKLandmarkMain,
    kWKLandmarkNavigation,
    kWKLandmarkRegion,
    kWKLandmarkSearch,
    kWKLegend,
    kWKLineBreak,
    kWKLink,
    kWKList,
    kWKListBox,
    kWKListBoxOption,
    kWKListItem,
    kWKListMarker,
    kWKMark,
    kWKMathElement,
    kWKMatte,
    kWKMenu,
    kWKMenuBar,
    kWKMenuButton,
    kWKMenuItem,
    kWKMenuItemCheckbox,
    kWKMenuItemRadio,
    kWKMenuListPopup,
    kWKMenuListOption,
    kWKMeter,
    kWKModel,
    kWKOutline,
    kWKParagraph,
    kWKPopUpButton,
    kWKPre,
    kWKPresentational,
    kWKProgressIndicator,
    kWKRadioButton,
    kWKRadioGroup,
    kWKRowHeader,
    kWKRow,
    kWKRowGroup,
    kWKRubyBase,
    kWKRubyBlock,
    kWKRubyInline,
    kWKRubyRun,
    kWKRubyText,
    kWKRuler,
    kWKRulerMarker,
    kWKScrollArea,
    kWKScrollBar,
    kWKSearchField,
    kWKSheet,
    kWKSlider,
    kWKSliderThumb,
    kWKSpinButton,
    kWKSpinButtonPart,
    kWKSplitGroup,
    kWKSplitter,
    kWKStaticText,
    kWKSubscript,
    kWKSuggestion,
    kWKSummary,
    kWKSuperscript,
    kWKSwitch,
    kWKSystemWide,
    kWKSVGRoot,
    kWKSVGText,
    kWKSVGTSpan,
    kWKSVGTextPath,
    kWKTabGroup,
    kWKTabList,
    kWKTabPanel,
    kWKTab,
    kWKTable,
    kWKTableHeaderContainer,
    kWKTextArea,
    kWKTextGroup,
    kWKTerm,
    kWKTime,
    kWKTree,
    kWKTreeGrid,
    kWKTreeItem,
    kWKTextField,
    kWKToggleButton,
    kWKToolbar,
    kWKUnknown,
    kWKUserInterfaceTooltip,
    kWKValueIndicator,
    kWKVideo,
    kWKWebApplication,
    kWKWebArea,
    kWKWebCoreLink,
    kWKWindow,
};
typedef uint32_t WKAXRole;

enum {
    // must be same as WebCore::AccessibilityButtonState
    kWKButtonStateOff = 0,
    kWKButtonStateOn,
    kWKButtonStateMixed,
};
typedef uint32_t WKAXButtonState;

#ifdef __cplusplus
}
#endif

#endif // WKAXTypes_h
