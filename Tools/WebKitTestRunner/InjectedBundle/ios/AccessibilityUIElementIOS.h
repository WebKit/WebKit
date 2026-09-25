/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(IOS_FAMILY)

#include "AccessibilityUIElement.h"
#include <wtf/RetainPtr.h>
#include <wtf/WeakObjCPtr.h>

namespace WTR {

class AccessibilityUIElementIOS final : public AccessibilityUIElement {
public:
    static Ref<AccessibilityUIElementIOS> create(PlatformUIElement);
    static Ref<AccessibilityUIElementIOS> create(const AccessibilityUIElementIOS&);

    ~AccessibilityUIElementIOS();

    PlatformUIElement platformUIElement() override { return m_element.getAutoreleased(); }

    // AccessibilityUIElement overrides
    bool isValid() const override;
    bool isEqual(AccessibilityUIElement* otherElement) override;

    RefPtr<OpaqueJSString> domIdentifier() const override;
    RefPtr<AccessibilityUIElement> headerElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> linkedElement() override;
    void getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement>>&);
    void getDocumentLinks(Vector<RefPtr<AccessibilityUIElement>>&);
    JSValueRef children(JSContextRef) override;
    unsigned childrenCount() override;
    RefPtr<AccessibilityUIElement> childAtIndex(unsigned) override;
    Vector<RefPtr<AccessibilityUIElement>> getChildren() const;
    Vector<RefPtr<AccessibilityUIElement>> getChildrenInRange(unsigned location, unsigned length) const;
    RefPtr<AccessibilityUIElement> elementAtPoint(int x, int y) override;
    unsigned indexOfChild(AccessibilityUIElement*) override;
    RefPtr<AccessibilityUIElement> linkedUIElementAtIndex(unsigned) override;
    JSValueRef detailsElements(JSContextRef) override;
    JSValueRef errorMessageElements(JSContextRef) override;
    RefPtr<AccessibilityUIElement> ariaOwnsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaFlowToElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaActionsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaControlsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaDetailsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaErrorMessageElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> disclosedRowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> rowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> selectedChildAtIndex(unsigned) const override;
    unsigned selectedChildrenCount() const override;

    RefPtr<OpaqueJSString> allAttributes() override;
    RefPtr<OpaqueJSString> attributesOfLinkedUIElements() override;
    RefPtr<AccessibilityUIElement> titleUIElement() override;
    RefPtr<AccessibilityUIElement> parentElement() override;

    RefPtr<OpaqueJSString> attributesOfChildren() override;
    RefPtr<OpaqueJSString> parameterizedAttributeNames() override;

    RefPtr<OpaqueJSString> role() override;
    RefPtr<OpaqueJSString> subrole() override;
    RefPtr<OpaqueJSString> roleDescription() override;
    RefPtr<OpaqueJSString> computedRoleString() override;
    RefPtr<OpaqueJSString> title() override;
    RefPtr<OpaqueJSString> description() override;
    RefPtr<OpaqueJSString> language() override;
    RefPtr<OpaqueJSString> stringValue() override;
    RefPtr<OpaqueJSString> accessibilityValue() const override;
    RefPtr<OpaqueJSString> helpText() const override;
    RefPtr<OpaqueJSString> orientation() const override;
    double x() override;
    double y() override;
    double width() override;
    double height() override;
    double intValue() const override;
    double minValue() override;
    double maxValue() override;
    RefPtr<OpaqueJSString> valueDescription() override;
    int insertionPointLineNumber() override;
    RefPtr<OpaqueJSString> selectedTextRange() override;
    bool isEnabled() override;
    bool isRequired() const override;

    bool isFocused() const override;
    bool isFocusable() const override;
    bool isSelected() const override;
    bool isSelectable() const override;
    bool isMultiSelectable() const override;
    void setSelectedChild(AccessibilityUIElement*) const override;
    void setSelectedChildAtIndex(unsigned) const override;
    void removeSelectionAtIndex(unsigned) const override;
    void clearSelectedChildren() const override;

    bool isExpanded() const override;
    bool isChecked() const override;
    bool isIndeterminate() const override;
    bool isVisible() const override;
    bool isOffScreen() const override;
    bool isCollapsed() const override;
    bool isIgnored() const override;
    bool isRemotePlatformElement() const override;
    bool isSingleLine() const override;
    bool isMultiLine() const override;
    bool hasPopup() const override;
    RefPtr<OpaqueJSString> popupValue() const override;
    int hierarchicalLevel() const override;
    double clickPointX() override;
    double clickPointY() override;
    RefPtr<OpaqueJSString> documentEncoding();
    RefPtr<OpaqueJSString> documentURI();
    RefPtr<OpaqueJSString> url() override;

    RefPtr<OpaqueJSString> classList() const override;

    // Additional platform-specific methods
    RefPtr<OpaqueJSString> identifier() override;
    RefPtr<OpaqueJSString> traits() override;
    int elementTextPosition() override;
    int elementTextLength() override;
    RefPtr<OpaqueJSString> stringForSelection() override;
    void increaseTextSelection() override;
    void decreaseTextSelection() override;
    RefPtr<AccessibilityUIElement> fieldsetAncestorElement() override;

    bool scrollPageUp() override;
    bool scrollPageDown() override;
    bool scrollPageLeft() override;
    bool scrollPageRight() override;

    bool hasTextEntryTrait() override;
    bool hasTabBarTrait() override;
    bool hasMenuItemTrait() override;
    bool hasPopupButtonTrait() override;
    bool hasButtonTrait() override;

    bool isSearchField() const override;
    bool isSwitch() const override;
    bool isTextArea() const override;

    void assistiveTechnologySimulatedFocus() override;

    RefPtr<OpaqueJSString> customContent() const override;
    RefPtr<OpaqueJSString> brailleLabel() const override;
    RefPtr<OpaqueJSString> brailleRoleDescription() const override;

    RefPtr<OpaqueJSString> embeddedImageDescription() const override;
    RefPtr<OpaqueJSString> imageDataSize() const override;
    RefPtr<OpaqueJSString> imageDataForParameters(int resizeWidth, int resizeHeight) const override;
    RefPtr<OpaqueJSString> imageDataForParametersWithFormat(int resizeWidth, int resizeHeight, JSStringRef format) const override;
    RefPtr<OpaqueJSString> imageDataForSubrect(int resizeWidth, int resizeHeight, int left, int top, int width, int height) const override;
    JSValueRef imageOverlayElements(JSContextRef) override;

    bool hasDocumentRoleAncestor() const;
    bool hasWebApplicationAncestor() const;
    bool isInDescriptionListDetail() const override;
    bool isInDescriptionListTerm() const override;
    bool isInCell() const override;
    bool isInTable() const override;
    bool isInLandmark() const override;
    bool isInList() const override;

    JSValueRef selectedChildren(JSContextRef) override;

    // Attribute methods
    RefPtr<OpaqueJSString> stringDescriptionOfAttributeValue(JSStringRef attribute) override;
    RefPtr<OpaqueJSString> stringAttributeValue(JSStringRef attribute) override;
    double numberAttributeValue(JSStringRef attribute) override;
    JSValueRef uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute) override;
    RefPtr<AccessibilityUIElement> uiElementAttributeValue(JSStringRef attribute) const override;
    bool boolAttributeValue(JSStringRef attribute) override;
    bool isAttributeSettable(JSStringRef attribute) override;
    bool isAttributeSupported(JSStringRef attribute) override;

    // Table methods
    JSValueRef rowHeaders(JSContextRef) override;
    JSValueRef columnHeaders(JSContextRef) override;
    JSValueRef selectedCells(JSContextRef) override;
    RefPtr<AccessibilityUIElement> selectedRowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> disclosedByRow() override;

    // Other methods
    RefPtr<OpaqueJSString> attributesOfDocumentLinks() override;
    RefPtr<OpaqueJSString> liveRegionRelevant() const override;
    RefPtr<OpaqueJSString> liveRegionStatus() const override;
    double pageX() override;
    double pageY() override;

    // Action support
    bool isPressActionSupported() override;
    bool isIncrementActionSupported() override;
    bool isDecrementActionSupported() override;

    // State methods
    bool isAtomicLiveRegion() const override;
    bool isBusy() const override;
    bool isSelectedOptionActive() const override;
    bool supportsExpanded() const override;
    bool isGrabbed() const override;

    // Focus
    RefPtr<AccessibilityUIElement> focusedElement() const override;

    // ARIA
    RefPtr<OpaqueJSString> currentStateValue() const override;
    RefPtr<OpaqueJSString> sortDirection() const override;
    RefPtr<OpaqueJSString> speakAs() override;
    RefPtr<OpaqueJSString> ariaDropEffects() const override;

    // Text/Range methods
    RefPtr<OpaqueJSString> lineRectsAndText() const override;
    int lineForIndex(int) override;
    RefPtr<OpaqueJSString> rangeForLine(int) override;
    RefPtr<OpaqueJSString> rangeForPosition(int x, int y) override;
    RefPtr<OpaqueJSString> boundsForRange(unsigned location, unsigned length) override;
    RefPtr<OpaqueJSString> stringForRange(unsigned location, unsigned length) override;
    RefPtr<OpaqueJSString> attributedStringForRange(unsigned location, unsigned length) override;
    RefPtr<OpaqueJSString> attributedStringForElement() override;
    bool attributedStringRangeIsMisspelled(unsigned location, unsigned length) override;

    // Search methods
    unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    RefPtr<AccessibilityUIElement> uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    JSValueRef uiElementsForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly, unsigned resultsLimit) override;
    RefPtr<OpaqueJSString> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity) override;

    // Table attribute methods
    RefPtr<OpaqueJSString> attributesOfColumnHeaders() override;
    RefPtr<OpaqueJSString> attributesOfRowHeaders() override;
    RefPtr<OpaqueJSString> attributesOfColumns() override;
    RefPtr<OpaqueJSString> attributesOfRows() override;
    RefPtr<OpaqueJSString> attributesOfVisibleCells() override;
    RefPtr<OpaqueJSString> attributesOfHeader() override;

    // Table cell methods
    int rowCount() override;
    int columnCount() override;
    int indexInTable() override;
    RefPtr<OpaqueJSString> rowIndexRange() override;
    RefPtr<OpaqueJSString> columnIndexRange() override;
    RefPtr<AccessibilityUIElement> cellForColumnAndRow(unsigned col, unsigned row) override;

    // Scrollbar methods
    RefPtr<AccessibilityUIElement> horizontalScrollbar() const override;
    RefPtr<AccessibilityUIElement> verticalScrollbar() const override;

    // Scrolling methods
    void scrollToMakeVisible() override;
    void scrollToGlobalPoint(int x, int y) override;
    void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height) override;

    // Selection methods
    RefPtr<AccessibilityTextMarkerRange> intersectionWithSelectionRange() override;
    bool setSelectedTextMarkerRange(AccessibilityTextMarkerRange*) override;
    bool setSelectedTextRange(unsigned location, unsigned length) override;
    RefPtr<OpaqueJSString> textInputMarkedRange() const override;

    // Action methods
    void increment() override;
    void decrement() override;
    void showMenu() override;
    void press() override;
    bool dismiss() override;
    bool invokeCustomActionAtIndex(unsigned) override;

    // Focus and selection actions
    void takeFocus() override;
    void takeSelection() override;
    void addSelection() override;
    void removeSelection() override;

    // Notification methods
    bool addNotificationListener(JSContextRef, JSValueRef functionCallback) override;
    bool removeNotificationListener() override;

    // Date/time
    RefPtr<OpaqueJSString> dateTimeValue() const override;

    // Math
    RefPtr<OpaqueJSString> mathPostscriptsDescription() const override;
    RefPtr<OpaqueJSString> mathPrescriptsDescription() const override;

    // Path
    RefPtr<OpaqueJSString> pathDescription() const override;
    RefPtr<OpaqueJSString> pathAsBounds() const override;

    // Supported actions
    RefPtr<OpaqueJSString> supportedActions() const override;

    // Insertion/deletion
    bool isInsertion() const override;
    bool isDeletion() const override;
    bool isFirstItemInSuggestion() const override;
    bool isLastItemInSuggestion() const override;
    bool isMarkAnnotation() const override;
    bool isFrameGeometryInitialized() const override;

    // Text input
    bool insertText(JSStringRef) override;
    bool replaceTextInRange(JSStringRef, int position, int length) override;

    // Text marker methods
    RefPtr<AccessibilityTextMarker> textMarkerForPoint(int x, int y) override;
    RefPtr<AccessibilityTextMarker> textMarkerForIndex(int) override;
    RefPtr<AccessibilityTextMarker> startTextMarker() override;
    RefPtr<AccessibilityTextMarker> endTextMarker() override;
    bool isTextMarkerValid(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextTextMarker(AccessibilityTextMarker*) override;
    RefPtr<OpaqueJSString> stringForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    int textMarkerRangeLength(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> startTextMarkerForBounds(int x, int y, int width, int height) override;
    RefPtr<AccessibilityTextMarker> endTextMarkerForBounds(int x, int y, int width, int height) override;
    RefPtr<AccessibilityUIElement> accessibilityElementForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForElement(AccessibilityUIElement*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> intersectTextMarkerRanges(AccessibilityTextMarkerRange*, AccessibilityTextMarkerRange*) override;
    int indexForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<OpaqueJSString> rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef) override;
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*) override;
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool) override;
    bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarkerRange> lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForSearchPredicate(JSContextRef, AccessibilityTextMarkerRange* startRange, bool forward, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    RefPtr<AccessibilityTextMarkerRange> misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*) override;

private:
    AccessibilityUIElementIOS(PlatformUIElement);
    AccessibilityUIElementIOS(const AccessibilityUIElementIOS&);

    WeakObjCPtr<id> m_element;
    RetainPtr<id> m_notificationHandler;
};

} // namespace WTR

#endif // PLATFORM(IOS_FAMILY)
