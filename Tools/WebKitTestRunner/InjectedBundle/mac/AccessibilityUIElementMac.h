/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#if PLATFORM(MAC)

#include "AccessibilityUIElement.h"
#include <wtf/RetainPtr.h>
#include <wtf/WeakObjCPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSString;

namespace WTR {

// Mac implementation of AccessibilityUIElement using in-process accessibility wrappers
class AccessibilityUIElementMac final : public AccessibilityUIElement {
    // Helper functions that dispatch to AX secondary thread
    friend RetainPtr<NSArray> supportedAttributes(id);
    friend void setAttributeValue(id, NSString *, id, bool synchronous);

public:
    static Ref<AccessibilityUIElementMac> create(PlatformUIElement);
    static Ref<AccessibilityUIElementMac> create(const AccessibilityUIElementMac&);

    virtual ~AccessibilityUIElementMac();

    PlatformUIElement platformUIElement() override { return m_element.getAutoreleased(); }

    bool isEqual(AccessibilityUIElement* otherElement) override;
    JSRetainPtr<JSStringRef> domIdentifier() const override;

    RefPtr<AccessibilityUIElement> elementAtPoint(int x, int y) override;
    RefPtr<AccessibilityUIElement> elementAtPointWithRemoteElement(int x, int y) override;
    void elementAtPointResolvingRemoteFrame(JSContextRef, int x, int y, JSValueRef callback) override;

    JSValueRef children(JSContextRef) override;
    unsigned childrenCount() override;
    RefPtr<AccessibilityUIElement> childAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> childAtIndexWithRemoteElement(unsigned) override;
    unsigned indexOfChild(AccessibilityUIElement*) override;
    RefPtr<AccessibilityUIElement> titleUIElement() override;
    RefPtr<AccessibilityUIElement> parentElement() override;

    void takeFocus() override;
    void takeSelection() override;
    void addSelection() override;
    void removeSelection() override;

    JSRetainPtr<JSStringRef> allAttributes() override;
    JSRetainPtr<JSStringRef> attributesOfLinkedUIElements() override;
    RefPtr<AccessibilityUIElement> linkedUIElementAtIndex(unsigned) override;

    JSRetainPtr<JSStringRef> attributesOfDocumentLinks() override;
    JSRetainPtr<JSStringRef> attributesOfChildren() override;
    JSRetainPtr<JSStringRef> parameterizedAttributeNames() override;
    void increment() override;
    void decrement() override;
    void showMenu() override;
    void press() override;
    bool dismiss() override;
    void syncPress() override;
    void asyncIncrement() override;
    void asyncDecrement() override;
    RefPtr<AccessibilityUIElement> focusableAncestor() override;
    RefPtr<AccessibilityUIElement> editableAncestor() override;
    RefPtr<AccessibilityUIElement> highestEditableAncestor() override;
    JSRetainPtr<JSStringRef> selectedText() override;

    JSRetainPtr<JSStringRef> dateTimeValue() const override;

    JSRetainPtr<JSStringRef> stringDescriptionOfAttributeValue(JSStringRef attribute) override;
    JSRetainPtr<JSStringRef> stringAttributeValue(JSStringRef attribute) override;
    double numberAttributeValue(JSStringRef attribute) override;
    JSValueRef uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute) override;
    RefPtr<AccessibilityUIElement> uiElementAttributeValue(JSStringRef attribute) const override;
    bool boolAttributeValue(JSStringRef attribute) override;
    RetainPtr<id> attributeValue(NSString *) const;
    void attributeValueAsync(JSContextRef, JSStringRef attribute, JSValueRef callback) override;
    void setBoolAttributeValue(JSStringRef attribute, bool value) override;
    bool isAttributeSupported(JSStringRef attribute) override;
    bool isAttributeSettable(JSStringRef attribute) override;
    bool isPressActionSupported() override;
    bool isIncrementActionSupported() override;
    bool isDecrementActionSupported() override;
    void setValue(JSStringRef) override;
    JSRetainPtr<JSStringRef> role() override;
    JSRetainPtr<JSStringRef> subrole() override;
    JSRetainPtr<JSStringRef> roleDescription() override;
    JSRetainPtr<JSStringRef> computedRoleString() override;
    JSRetainPtr<JSStringRef> title() override;
    JSRetainPtr<JSStringRef> description() override;
    JSRetainPtr<JSStringRef> language() override;
    JSRetainPtr<JSStringRef> stringValue() override;
    JSRetainPtr<JSStringRef> dateValue() override;
    JSRetainPtr<JSStringRef> accessibilityValue() const override;
    JSRetainPtr<JSStringRef> helpText() const override;
    JSRetainPtr<JSStringRef> orientation() const override;
    JSRetainPtr<JSStringRef> liveRegionRelevant() const override;
    JSRetainPtr<JSStringRef> liveRegionStatus() const override;
    double pageX() override;
    double pageY() override;
    double x() override;
    double y() override;
    double width() override;
    double height() override;
    JSRetainPtr<JSStringRef> lineRectsAndText() const override;
    JSRetainPtr<JSStringRef> brailleLabel() const override;
    JSRetainPtr<JSStringRef> brailleRoleDescription() const override;

    double intValue() const override;
    double minValue() override;
    double maxValue() override;
    JSRetainPtr<JSStringRef> valueDescription() override;
    unsigned numberOfCharacters() const override;
    int insertionPointLineNumber() override;
    JSRetainPtr<JSStringRef> selectedTextRange() override;
    JSRetainPtr<JSStringRef> intersectionWithSelectionRange() override;
    JSRetainPtr<JSStringRef> textInputMarkedRange() const override;
    bool isAtomicLiveRegion() const override;
    bool isBusy() const override;
    bool isEnabled() override;
    bool isRequired() const override;

    RefPtr<AccessibilityUIElement> focusedElement() const override;
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
    RefPtr<AccessibilityUIElement> activeElement() const override;
    JSValueRef selectedChildren(JSContextRef) override;
    unsigned selectedChildrenCount() const override;
    RefPtr<AccessibilityUIElement> selectedChildAtIndex(unsigned) const override;

    bool isValid() const override;
    bool isExpanded() const override;
    bool isChecked() const override;
    JSRetainPtr<JSStringRef> currentStateValue() const override;
    JSRetainPtr<JSStringRef> sortDirection() const override;
    bool isIndeterminate() const override;
    bool isVisible() const override;
    bool isOnScreen() const override;
    bool isOffScreen() const override;
    bool isCollapsed() const override;
    bool isIgnored() const override;
    bool isSingleLine() const override;
    bool isMultiLine() const override;
    bool hasPopup() const override;
    JSRetainPtr<JSStringRef> popupValue() const override;
    int hierarchicalLevel() const override;
    double clickPointX() override;
    double clickPointY() override;
    JSRetainPtr<JSStringRef> url() override;
    JSRetainPtr<JSStringRef> classList() const override;
    JSRetainPtr<JSStringRef> embeddedImageDescription() const override;
    JSValueRef imageOverlayElements(JSContextRef) override;

    JSRetainPtr<JSStringRef> speakAs() override;

    JSRetainPtr<JSStringRef> attributesOfColumnHeaders() override;
    JSRetainPtr<JSStringRef> attributesOfRowHeaders() override;
    JSRetainPtr<JSStringRef> attributesOfColumns() override;
    JSValueRef columns(JSContextRef) override;
    JSRetainPtr<JSStringRef> attributesOfRows() override;
    JSRetainPtr<JSStringRef> attributesOfVisibleCells() override;
    JSRetainPtr<JSStringRef> attributesOfHeader() override;
    bool isInCell() const override;
    bool isInTable() const override;
    int indexInTable() override;
    JSRetainPtr<JSStringRef> rowIndexRange() override;
    JSRetainPtr<JSStringRef> columnIndexRange() override;
    int rowCount() override;
    int columnCount() override;
    JSValueRef rowHeaders(JSContextRef) override;
    JSValueRef columnHeaders(JSContextRef) override;
    JSRetainPtr<JSStringRef> customContent() const override;
    JSValueRef selectedCells(JSContextRef) override;

    RefPtr<AccessibilityUIElement> selectedRowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> disclosedByRow() override;
    RefPtr<AccessibilityUIElement> disclosedRowAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> rowAtIndex(unsigned) override;

    RefPtr<AccessibilityUIElement> controllerElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaControlsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaDescribedByElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> descriptionForElementAtIndex(unsigned) override;
    JSValueRef detailsElements(JSContextRef) override;
    RefPtr<AccessibilityUIElement> ariaDetailsElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> detailsForElementAtIndex(unsigned) override;
    JSValueRef errorMessageElements(JSContextRef) override;
    RefPtr<AccessibilityUIElement> ariaErrorMessageElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> errorMessageForElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> flowFromElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaFlowToElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaLabelledByElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> labelForElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ownerElementAtIndex(unsigned) override;
    RefPtr<AccessibilityUIElement> ariaOwnsElementAtIndex(unsigned) override;

    bool isGrabbed() const override;
    JSRetainPtr<JSStringRef> ariaDropEffects() const override;

    int lineForIndex(int) override;
    JSRetainPtr<JSStringRef> rangeForLine(int) override;
    JSRetainPtr<JSStringRef> rangeForPosition(int x, int y) override;
    JSRetainPtr<JSStringRef> boundsForRange(unsigned location, unsigned length) override;
    JSRetainPtr<JSStringRef> boundsForRangeWithPagePosition(unsigned location, unsigned length) override;
    bool setSelectedTextRange(unsigned location, unsigned length) override;
    JSRetainPtr<JSStringRef> stringForRange(unsigned location, unsigned length) override;
    JSRetainPtr<JSStringRef> attributedStringForRange(unsigned location, unsigned length) override;

    bool attributedStringRangeIsMisspelled(unsigned location, unsigned length) override;
    unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    RefPtr<AccessibilityUIElement> uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    JSRetainPtr<JSStringRef> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity) override;
    JSValueRef searchTextWithCriteria(JSContextRef, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction) override;
    JSValueRef performTextOperation(JSContextRef, JSStringRef operationType, JSValueRef markerRanges, JSValueRef replacementStrings, bool shouldSmartReplace) override;

    RefPtr<AccessibilityUIElement> cellForColumnAndRow(unsigned column, unsigned row) override;

    RefPtr<AccessibilityUIElement> horizontalScrollbar() const override;
    RefPtr<AccessibilityUIElement> verticalScrollbar() const override;

    void scrollToMakeVisible() override;
    void scrollToGlobalPoint(int x, int y) override;
    void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height) override;

    RefPtr<AccessibilityTextMarkerRange> lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> rightLineTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> leftLineTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousLineStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextLineEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    int lineIndexForTextMarker(AccessibilityTextMarker*) const override;
    RefPtr<AccessibilityTextMarkerRange> styleTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForSearchPredicate(JSContextRef, AccessibilityTextMarkerRange* startRange, bool forward, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly) override;
    RefPtr<AccessibilityTextMarkerRange> misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForElement(AccessibilityUIElement*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForUnorderedMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForRange(unsigned location, unsigned length) override;
    RefPtr<AccessibilityTextMarkerRange> selectedTextMarkerRange() override;
    void resetSelectedTextMarkerRange() override;
    bool replaceTextInRange(JSStringRef, int position, int length) override;
    bool insertText(JSStringRef) override;
    RefPtr<AccessibilityTextMarkerRange> textInputMarkedTextMarkerRange() const override;
    RefPtr<AccessibilityTextMarker> startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarker> endTextMarkerForBounds(int x, int y, int width, int height) override;
    RefPtr<AccessibilityTextMarker> startTextMarkerForBounds(int x, int y, int width, int height) override;
    RefPtr<AccessibilityTextMarker> textMarkerForPoint(int x, int y) override;
    RefPtr<AccessibilityTextMarker> previousTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityUIElement> accessibilityElementForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForLine(long) override;
    JSRetainPtr<JSStringRef> stringForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    JSRetainPtr<JSStringRef> rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef) override;
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*) override;
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*) override;
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool) override;
    int textMarkerRangeLength(AccessibilityTextMarkerRange*) override;
    bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*) override;
    int indexForTextMarker(AccessibilityTextMarker*) override;
    bool isTextMarkerValid(AccessibilityTextMarker*) override;
    bool isTextMarkerRangeValid(AccessibilityTextMarkerRange*) override;
    bool isTextMarkerNull(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> textMarkerForIndex(int) override;
    RefPtr<AccessibilityTextMarker> startTextMarker() override;
    RefPtr<AccessibilityTextMarker> endTextMarker() override;
    bool setSelectedTextMarkerRange(AccessibilityTextMarkerRange*) override;
    RefPtr<AccessibilityTextMarkerRange> leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarkerRange> sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    RefPtr<AccessibilityTextMarker> previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*) override;
    JSRetainPtr<JSStringRef> textMarkerDebugDescription(AccessibilityTextMarker*) override;
    JSRetainPtr<JSStringRef> textMarkerRangeDebugDescription(AccessibilityTextMarkerRange*) override;

    JSRetainPtr<JSStringRef> supportedActions() const override;
    JSRetainPtr<JSStringRef> mathPostscriptsDescription() const override;
    JSRetainPtr<JSStringRef> mathPrescriptsDescription() const override;
    JSValueRef mathRootRadicand(JSContextRef) override;

    JSRetainPtr<JSStringRef> pathDescription() const override;

    bool addNotificationListener(JSContextRef, JSValueRef functionCallback) override;
    bool removeNotificationListener() override;

    bool isInDescriptionListDetail() const override;
    bool isInDescriptionListTerm() const override;

    bool isInsertion() const override;
    bool isDeletion() const override;
    bool isFirstItemInSuggestion() const override;
    bool isLastItemInSuggestion() const override;
    bool isRemoteFrame() const override;

private:
    AccessibilityUIElementMac(PlatformUIElement);
    AccessibilityUIElementMac(const AccessibilityUIElementMac&);

    NSArray *actionNames() const;
    bool performAction(NSString *) const;
    RetainPtr<id> attributeValueForParameter(NSString *, id) const;
    unsigned arrayAttributeCount(NSString *) const;
    RetainPtr<NSString> descriptionOfValue(id valueObject) const;
    bool boolAttributeValueNS(NSString *attribute) const;
    JSRetainPtr<JSStringRef> stringAttributeValueNS(NSString *attribute) const;
    double numberAttributeValueNS(NSString *attribute) const;
    bool isAttributeSettableNS(NSString *) const;

    WeakObjCPtr<id> m_element;
    RetainPtr<id> m_notificationHandler;

    void getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement> >&);
    void getDocumentLinks(Vector<RefPtr<AccessibilityUIElement> >&);
    RefPtr<AccessibilityUIElement> elementForAttribute(NSString*) const;
    RefPtr<AccessibilityUIElement> elementForAttributeAtIndex(NSString*, unsigned) const;

    void getUIElementsWithAttribute(JSStringRef, Vector<RefPtr<AccessibilityUIElement> >&) const;

    Vector<RefPtr<AccessibilityUIElement>> getChildren() const;
    Vector<RefPtr<AccessibilityUIElement>> getChildrenInRange(unsigned location, unsigned length) const;
};

} // namespace WTR

#endif // PLATFORM(MAC)
