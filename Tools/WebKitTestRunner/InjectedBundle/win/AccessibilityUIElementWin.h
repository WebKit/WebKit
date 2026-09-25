/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#pragma once

#if PLATFORM(WIN)

#include "AccessibilityUIElement.h"

namespace WTR {

// Windows implementation of AccessibilityUIElement
class AccessibilityUIElementWin final : public AccessibilityUIElement {
public:
    static Ref<AccessibilityUIElementWin> create(PlatformUIElement);
    static Ref<AccessibilityUIElementWin> create(const AccessibilityUIElementWin&);

    virtual ~AccessibilityUIElementWin();

    PlatformUIElement platformUIElement() override { return m_element; }

    bool isEqual(AccessibilityUIElement* otherElement) override;
    RefPtr<OpaqueJSString> domIdentifier() const override;

    RefPtr<AccessibilityUIElement> elementAtPoint(int x, int y) override;
    unsigned indexOfChild(AccessibilityUIElement*) override;
    RefPtr<AccessibilityUIElement> childAtIndex(unsigned) override;
    unsigned childrenCount() override;
    RefPtr<AccessibilityUIElement> titleUIElement() override;
    RefPtr<AccessibilityUIElement> parentElement() override;

    void takeFocus() override;
    void takeSelection() override;
    void addSelection() override;
    void removeSelection() override;

    RefPtr<OpaqueJSString> allAttributes() override;
    RefPtr<OpaqueJSString> attributesOfLinkedUIElements() override;
    RefPtr<AccessibilityUIElement> linkedUIElementAtIndex(unsigned) override;

    RefPtr<OpaqueJSString> attributesOfDocumentLinks() override;
    RefPtr<OpaqueJSString> attributesOfChildren() override;
    RefPtr<OpaqueJSString> parameterizedAttributeNames() override;
    void increment() override;
    void decrement() override;
    void showMenu() override;
    void press() override;

    RefPtr<OpaqueJSString> stringDescriptionOfAttributeValue(JSStringRef attribute) override;
    RefPtr<OpaqueJSString> stringAttributeValue(JSStringRef attribute) override;
    double numberAttributeValue(JSStringRef attribute) override;
    JSValueRef uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute) override;
    RefPtr<AccessibilityUIElement> uiElementAttributeValue(JSStringRef attribute) const override;
    bool boolAttributeValue(JSStringRef attribute) override;
    bool isAttributeSupported(JSStringRef attribute) override;
    bool isAttributeSettable(JSStringRef attribute) override;
    bool isPressActionSupported() override;
    bool isIncrementActionSupported() override;
    bool isDecrementActionSupported() override;
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
    RefPtr<OpaqueJSString> liveRegionRelevant() const override;
    RefPtr<OpaqueJSString> liveRegionStatus() const override;
    double x() override;
    double y() override;
    double width() override;
    double height() override;
    double pageX() override;
    double pageY() override;
    double clickPointX() override;
    double clickPointY() override;

    double intValue() const override;
    double minValue() override;
    double maxValue() override;
    RefPtr<OpaqueJSString> valueDescription() override;
    int insertionPointLineNumber() override;
    RefPtr<OpaqueJSString> selectedTextRange() override;
    RefPtr<AccessibilityTextMarkerRange> intersectionWithSelectionRange() override;
    RefPtr<OpaqueJSString> textInputMarkedRange() const override;
    bool isAtomicLiveRegion() const override;
    bool isBusy() const override;
    bool isEnabled() override;
    bool isRequired() const override;

    bool isFocused() const override;
    bool isFocusable() const override;
    bool isSelected() const override;
    bool isSelectedOptionActive() const override;
    bool isSelectable() const override;
    bool isMultiSelectable() const override;
    void setSelectedChild(AccessibilityUIElement*) const override;
    void setSelectedChildAtIndex(unsigned) const override;
    void removeSelectionAtIndex(unsigned) const override;
    void clearSelectedChildren() const override;
    unsigned selectedChildrenCount() const override;
    RefPtr<AccessibilityUIElement> selectedChildAtIndex(unsigned) const override;

    bool isValid() const override;
    bool isExpanded() const override;
    bool isChecked() const override;
    RefPtr<OpaqueJSString> currentStateValue() const override;
    RefPtr<OpaqueJSString> sortDirection() const override;
    bool isIndeterminate() const override;
    bool isVisible() const override;
    bool isOffScreen() const override;
    bool isCollapsed() const override;
    bool isIgnored() const override;
    bool isSingleLine() const override;
    bool isMultiLine() const override;
    bool hasPopup() const override;
    RefPtr<OpaqueJSString> popupValue() const override;
    int hierarchicalLevel() const override;
    RefPtr<OpaqueJSString> url() override;
    RefPtr<OpaqueJSString> classList() const override;

    RefPtr<OpaqueJSString> speakAs() override;

    RefPtr<OpaqueJSString> attributesOfColumnHeaders() override;
    RefPtr<OpaqueJSString> attributesOfRowHeaders() override;
    RefPtr<OpaqueJSString> attributesOfColumns() override;
    RefPtr<OpaqueJSString> attributesOfRows() override;
    RefPtr<OpaqueJSString> attributesOfVisibleCells() override;
    RefPtr<OpaqueJSString> attributesOfHeader() override;
    int indexInTable() override;
    RefPtr<OpaqueJSString> rowIndexRange() override;
    RefPtr<OpaqueJSString> columnIndexRange() override;
    int rowCount() override;
    int columnCount() override;
    JSValueRef rowHeaders(JSContextRef) override;
    JSValueRef columnHeaders(JSContextRef) override;
    JSValueRef selectedCells(JSContextRef) override;

    RefPtr<AccessibilityUIElement> selectedRowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> disclosedByRow() override;
    RefPtr<AccessibilityUIElement> disclosedRowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> rowAtIndex(unsigned) override;

    RefPtr<AccessibilityUIElement> ariaActionsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaControlsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaFlowToElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaOwnsElementAtIndex(unsigned) override;

    bool isGrabbed() const override;
    RefPtr<OpaqueJSString> ariaDropEffects() const override;

    int lineForIndex(int) override;
    RefPtr<OpaqueJSString> rangeForLine(int) override;
    RefPtr<OpaqueJSString> rangeForPosition(int x, int y) override;
    RefPtr<OpaqueJSString> boundsForRange(unsigned location, unsigned length) override;
    bool setSelectedTextRange(unsigned location, unsigned length) override;
    RefPtr<OpaqueJSString> stringForRange(unsigned location, unsigned length) override;
    RefPtr<OpaqueJSString> attributedStringForRange(unsigned location, unsigned length) override;
    bool attributedStringRangeIsMisspelled(unsigned location, unsigned length) override;
    unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    RefPtr<AccessibilityUIElement> uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    JSValueRef uiElementsForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly, unsigned resultsLimit) override;
    RefPtr<OpaqueJSString> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity) override;

    RefPtr<AccessibilityUIElement> cellForColumnAndRow(unsigned column, unsigned row) override;

    RefPtr<AccessibilityUIElement> horizontalScrollbar() const override;
    RefPtr<AccessibilityUIElement> verticalScrollbar() const override;

    void scrollToMakeVisible() override;
    void scrollToGlobalPoint(int x, int y) override;
    void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height) override;

    RefPtr<AccessibilityTextMarker> textMarkerForPoint(int x, int y) override;
    RefPtr<AccessibilityTextMarker> textMarkerForIndex(int) override;
    RefPtr<AccessibilityTextMarker> startTextMarker() override;
    RefPtr<AccessibilityTextMarker> endTextMarker() override;
    bool setSelectedTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForElement(AccessibilityUIElement*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> endTextMarkerForBounds(int x, int y, int width, int height) override;
    RefPtr<AccessibilityTextMarker> startTextMarkerForBounds(int x, int y, int width, int height) override;
    RefPtr<AccessibilityTextMarker> previousTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextTextMarker(AccessibilityTextMarker*) override;
    RefPtr<OpaqueJSString> stringForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<OpaqueJSString> rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef) override;
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool) override;
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*) override;
    int textMarkerRangeLength(AccessibilityTextMarkerRange*) override;
    bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*) override;
    int indexForTextMarker(AccessibilityTextMarker*) override;
    bool isTextMarkerValid(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityUIElement> accessibilityElementForTextMarker(AccessibilityTextMarker*) override;

    RefPtr<OpaqueJSString> supportedActions() const override;
    RefPtr<OpaqueJSString> mathPostscriptsDescription() const override;
    RefPtr<OpaqueJSString> mathPrescriptsDescription() const override;

    RefPtr<OpaqueJSString> pathDescription() const override;
    RefPtr<OpaqueJSString> pathAsBounds() const override;

    bool addNotificationListener(JSContextRef, JSValueRef functionCallback) override;
    bool removeNotificationListener() override;

    bool isInsertion() const override;
    bool isDeletion() const override;
    bool isFirstItemInSuggestion() const override;
    bool isLastItemInSuggestion() const override;

    bool replaceTextInRange(JSStringRef, int position, int length) override;
    bool insertText(JSStringRef) override;

    RefPtr<OpaqueJSString> characterAtOffset(int offset) override;
    RefPtr<OpaqueJSString> wordAtOffset(int offset) override;
    RefPtr<OpaqueJSString> lineAtOffset(int offset) override;
    RefPtr<OpaqueJSString> sentenceAtOffset(int offset) override;

private:
    AccessibilityUIElementWin(PlatformUIElement);
    AccessibilityUIElementWin(const AccessibilityUIElementWin&);

    Vector<RefPtr<AccessibilityUIElement>> getChildren() const;
    Vector<RefPtr<AccessibilityUIElement>> getChildrenInRange(unsigned location, unsigned length) const;

    PlatformUIElement m_element;
};

} // namespace WTR

#endif // PLATFORM(WIN)
