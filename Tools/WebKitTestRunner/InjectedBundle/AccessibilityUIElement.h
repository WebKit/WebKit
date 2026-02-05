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

#include "AccessibilityTextMarker.h"
#include "AccessibilityTextMarkerRange.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSWrappable.h"

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSArray;
OBJC_CLASS NSString;
#include <wtf/RetainPtr.h>
#include <wtf/WeakObjCPtr.h>
using PlatformUIElement = id;
#elif USE(ATSPI)
namespace WebCore {
class AccessibilityObjectAtspi;
}
typedef WebCore::AccessibilityObjectAtspi* PlatformUIElement;
#else
typedef void* PlatformUIElement;
#endif

namespace WTR {

class AccessibilityController;

class AccessibilityUIElement : public JSWrappable {
#if PLATFORM(COCOA)
    // Helper functions that dispatch the corresponding AccessibilityObjectWrapper method to the AX secondary thread when appropriate.
    friend RetainPtr<NSArray> supportedAttributes(id);
    friend void setAttributeValue(id, NSString *, id, bool synchronous);
#endif

public:
    static Ref<AccessibilityUIElement> create(PlatformUIElement);
    static Ref<AccessibilityUIElement> create(const AccessibilityUIElement&);

    virtual ~AccessibilityUIElement();

    virtual PlatformUIElement platformUIElement() = 0;

    virtual JSClassRef wrapperClass();

    static JSObjectRef makeJSAccessibilityUIElement(JSContextRef, const AccessibilityUIElement&);

    virtual bool isEqual(AccessibilityUIElement* otherElement);
    virtual JSRetainPtr<JSStringRef> domIdentifier() const;

    virtual RefPtr<AccessibilityUIElement> elementAtPoint(int x, int y);
    virtual RefPtr<AccessibilityUIElement> elementAtPointWithRemoteElement(int x, int y);
    virtual void elementAtPointResolvingRemoteFrame(JSContextRef, int x, int y, JSValueRef callback);

    virtual JSValueRef children(JSContextRef);
    virtual RefPtr<AccessibilityUIElement> childAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> childAtIndexWithRemoteElement(unsigned);
    virtual unsigned indexOfChild(AccessibilityUIElement*);
    virtual unsigned childrenCount();
    virtual RefPtr<AccessibilityUIElement> titleUIElement();
    virtual RefPtr<AccessibilityUIElement> parentElement();

    virtual void takeFocus();
    virtual void takeSelection();
    virtual void addSelection();
    virtual void removeSelection();

    // Methods - platform-independent implementations
    virtual JSRetainPtr<JSStringRef> allAttributes();
    virtual JSRetainPtr<JSStringRef> attributesOfLinkedUIElements();
    virtual RefPtr<AccessibilityUIElement> linkedUIElementAtIndex(unsigned);

    virtual JSRetainPtr<JSStringRef> attributesOfDocumentLinks();
    virtual JSRetainPtr<JSStringRef> attributesOfChildren();
    virtual JSRetainPtr<JSStringRef> parameterizedAttributeNames();
    virtual void increment();
    virtual void decrement();
    virtual void showMenu();
    virtual void press();
    virtual bool dismiss();
    virtual void syncPress();
    virtual void asyncIncrement();
    virtual void asyncDecrement();
    virtual RefPtr<AccessibilityUIElement> focusableAncestor();
    virtual RefPtr<AccessibilityUIElement> editableAncestor();
    virtual RefPtr<AccessibilityUIElement> highestEditableAncestor();
    virtual JSRetainPtr<JSStringRef> selectedText();

    virtual JSRetainPtr<JSStringRef> dateTimeValue() const;

    // Attributes - platform-independent implementations
    virtual JSRetainPtr<JSStringRef> stringDescriptionOfAttributeValue(JSStringRef attribute);
    virtual JSRetainPtr<JSStringRef> stringAttributeValue(JSStringRef attribute);
    virtual double numberAttributeValue(JSStringRef attribute);
    virtual JSValueRef uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute);
    virtual RefPtr<AccessibilityUIElement> uiElementAttributeValue(JSStringRef attribute) const;
    virtual bool boolAttributeValue(JSStringRef attribute);
    virtual void attributeValueAsync(JSContextRef, JSStringRef attribute, JSValueRef callback);
    virtual void setBoolAttributeValue(JSStringRef attribute, bool value);
    virtual bool isAttributeSupported(JSStringRef attribute);
    virtual bool isAttributeSettable(JSStringRef attribute);
    virtual bool isPressActionSupported();
    virtual bool isIncrementActionSupported();
    virtual bool isDecrementActionSupported();
    virtual void setValue(JSStringRef);
    virtual JSRetainPtr<JSStringRef> role();
    virtual JSRetainPtr<JSStringRef> subrole();
    virtual JSRetainPtr<JSStringRef> roleDescription();
    virtual JSRetainPtr<JSStringRef> computedRoleString();
    virtual JSRetainPtr<JSStringRef> title();
    virtual JSRetainPtr<JSStringRef> description();
    virtual JSRetainPtr<JSStringRef> language();
    virtual JSRetainPtr<JSStringRef> stringValue();
    virtual JSRetainPtr<JSStringRef> dateValue();
    virtual JSRetainPtr<JSStringRef> accessibilityValue() const;
    virtual JSRetainPtr<JSStringRef> helpText() const;
    virtual JSRetainPtr<JSStringRef> orientation() const;
    virtual JSRetainPtr<JSStringRef> liveRegionRelevant() const;
    virtual JSRetainPtr<JSStringRef> liveRegionStatus() const;
    virtual double pageX();
    virtual double pageY();
    virtual double x();
    virtual double y();
    virtual double width();
    virtual double height();
    virtual JSRetainPtr<JSStringRef> lineRectsAndText() const;
    virtual JSRetainPtr<JSStringRef> brailleLabel() const;
    virtual JSRetainPtr<JSStringRef> brailleRoleDescription() const;

    virtual double intValue() const;
    virtual double minValue();
    virtual double maxValue();
    virtual JSRetainPtr<JSStringRef> valueDescription();
    virtual unsigned numberOfCharacters() const;
    virtual int insertionPointLineNumber();
    virtual JSRetainPtr<JSStringRef> selectedTextRange();
    virtual JSRetainPtr<JSStringRef> intersectionWithSelectionRange();
    virtual JSRetainPtr<JSStringRef> textInputMarkedRange() const;
    virtual bool isAtomicLiveRegion() const;
    virtual bool isBusy() const;
    virtual bool isEnabled();
    virtual bool isRequired() const;

    virtual RefPtr<AccessibilityUIElement> focusedElement() const;
    virtual bool isFocused() const;
    virtual bool isFocusable() const;
    virtual bool isSelected() const;
    virtual bool isSelectedOptionActive() const;
    virtual bool isSelectable() const;
    virtual bool isMultiSelectable() const;
    virtual void setSelectedChild(AccessibilityUIElement*) const;
    virtual void setSelectedChildAtIndex(unsigned) const;
    virtual void removeSelectionAtIndex(unsigned) const;
    virtual void clearSelectedChildren() const;
    virtual RefPtr<AccessibilityUIElement> activeElement() const;
    virtual JSValueRef selectedChildren(JSContextRef);
    virtual unsigned selectedChildrenCount() const;
    virtual RefPtr<AccessibilityUIElement> selectedChildAtIndex(unsigned) const;

    virtual bool isValid() const;
    virtual bool isExpanded() const;
    virtual bool supportsExpanded() const;
    virtual bool isChecked() const;
    virtual JSRetainPtr<JSStringRef> currentStateValue() const;
    virtual JSRetainPtr<JSStringRef> sortDirection() const;
    virtual bool isIndeterminate() const;
    virtual bool isVisible() const;
    virtual bool isOnScreen() const;
    virtual bool isOffScreen() const;
    virtual bool isCollapsed() const;
    virtual bool isIgnored() const;
    virtual bool isSingleLine() const;
    virtual bool isMultiLine() const;
    virtual bool hasPopup() const;
    virtual JSRetainPtr<JSStringRef> popupValue() const;
    virtual int hierarchicalLevel() const;
    virtual double clickPointX();
    virtual double clickPointY();
    virtual JSRetainPtr<JSStringRef> url();
    virtual JSRetainPtr<JSStringRef> classList() const;
    virtual JSRetainPtr<JSStringRef> embeddedImageDescription() const;
    virtual JSValueRef imageOverlayElements(JSContextRef);

    // CSS3-speech properties.
    virtual JSRetainPtr<JSStringRef> speakAs();

    // Table-specific attributes
    virtual JSRetainPtr<JSStringRef> attributesOfColumnHeaders();
    virtual JSRetainPtr<JSStringRef> attributesOfRowHeaders();
    virtual JSRetainPtr<JSStringRef> attributesOfColumns();
    virtual JSValueRef columns(JSContextRef);
    virtual JSRetainPtr<JSStringRef> attributesOfRows();
    virtual JSRetainPtr<JSStringRef> attributesOfVisibleCells();
    virtual JSRetainPtr<JSStringRef> attributesOfHeader();
    virtual bool isInCell() const;
    virtual bool isInTable() const;
    virtual bool isInList() const;
    virtual bool isInLandmark() const;
    virtual int indexInTable();
    virtual JSRetainPtr<JSStringRef> rowIndexRange();
    virtual JSRetainPtr<JSStringRef> columnIndexRange();
    virtual int rowCount();
    virtual int columnCount();
    virtual JSValueRef rowHeaders(JSContextRef);
    virtual JSValueRef columnHeaders(JSContextRef);
    virtual JSRetainPtr<JSStringRef> customContent() const;
    virtual JSValueRef selectedCells(JSContextRef);

    // Tree/Outline specific attributes
    virtual RefPtr<AccessibilityUIElement> selectedRowAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> disclosedByRow();
    virtual RefPtr<AccessibilityUIElement> disclosedRowAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> rowAtIndex(unsigned);

    // Relationships.
    // FIXME: replace all ***AtIndex methods with ones that return an array and make the naming consistent.
    virtual RefPtr<AccessibilityUIElement> controllerElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> ariaControlsElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> ariaDescribedByElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> descriptionForElementAtIndex(unsigned);
    virtual JSValueRef detailsElements(JSContextRef);
    virtual RefPtr<AccessibilityUIElement> ariaDetailsElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> detailsForElementAtIndex(unsigned);
    virtual JSValueRef errorMessageElements(JSContextRef);
    virtual RefPtr<AccessibilityUIElement> ariaErrorMessageElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> errorMessageForElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> flowFromElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> ariaFlowToElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> ariaLabelledByElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> labelForElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> ownerElementAtIndex(unsigned);
    virtual RefPtr<AccessibilityUIElement> ariaOwnsElementAtIndex(unsigned);

    // Drag and drop
    virtual bool isGrabbed() const;
    // A space concatentated string of all the drop effects.
    virtual JSRetainPtr<JSStringRef> ariaDropEffects() const;

    // Parameterized attributes
    virtual int lineForIndex(int);
    virtual JSRetainPtr<JSStringRef> rangeForLine(int);
    virtual JSRetainPtr<JSStringRef> rangeForPosition(int x, int y);
    virtual JSRetainPtr<JSStringRef> boundsForRange(unsigned location, unsigned length);
    virtual JSRetainPtr<JSStringRef> boundsForRangeWithPagePosition(unsigned location, unsigned length);
    virtual bool setSelectedTextRange(unsigned location, unsigned length);
    virtual JSRetainPtr<JSStringRef> stringForRange(unsigned location, unsigned length);
    virtual JSRetainPtr<JSStringRef> attributedStringForRange(unsigned location, unsigned length);
    virtual JSRetainPtr<JSStringRef> attributedStringForElement();

    virtual bool attributedStringRangeIsMisspelled(unsigned location, unsigned length);
    virtual unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    virtual RefPtr<AccessibilityUIElement> uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    virtual JSRetainPtr<JSStringRef> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity);
    virtual JSValueRef searchTextWithCriteria(JSContextRef, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction);
    virtual JSValueRef performTextOperation(JSContextRef, JSStringRef operationType, JSValueRef markerRanges, JSValueRef replacementStrings, bool shouldSmartReplace);

    // Text-specific
    virtual JSRetainPtr<JSStringRef> characterAtOffset(int offset);
    virtual JSRetainPtr<JSStringRef> wordAtOffset(int offset);
    virtual JSRetainPtr<JSStringRef> lineAtOffset(int offset);
    virtual JSRetainPtr<JSStringRef> sentenceAtOffset(int offset);

    // Table-specific
    virtual RefPtr<AccessibilityUIElement> cellForColumnAndRow(unsigned column, unsigned row);

    // Scrollarea-specific
    virtual RefPtr<AccessibilityUIElement> horizontalScrollbar() const;
    virtual RefPtr<AccessibilityUIElement> verticalScrollbar() const;

    virtual void scrollToMakeVisible();
    virtual void scrollToGlobalPoint(int x, int y);
    virtual void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height);

    // Text markers.
    virtual RefPtr<AccessibilityTextMarkerRange> lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> rightLineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> leftLineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> previousLineStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> nextLineEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual int lineIndexForTextMarker(AccessibilityTextMarker*) const;
    virtual RefPtr<AccessibilityTextMarkerRange> styleTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForSearchPredicate(JSContextRef, AccessibilityTextMarkerRange* startRange, bool forward, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    virtual RefPtr<AccessibilityTextMarkerRange> misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForElement(AccessibilityUIElement*);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForUnorderedMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForRange(unsigned location, unsigned length);
    virtual RefPtr<AccessibilityTextMarkerRange> selectedTextMarkerRange();
    virtual void resetSelectedTextMarkerRange();
    virtual bool replaceTextInRange(JSStringRef, int position, int length);
    virtual bool insertText(JSStringRef);
    virtual RefPtr<AccessibilityTextMarkerRange> textInputMarkedTextMarkerRange() const;
    virtual RefPtr<AccessibilityTextMarker> startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*);
    virtual RefPtr<AccessibilityTextMarker> endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*);
    virtual RefPtr<AccessibilityTextMarker> endTextMarkerForBounds(int x, int y, int width, int height);
    virtual RefPtr<AccessibilityTextMarker> startTextMarkerForBounds(int x, int y, int width, int height);
    virtual RefPtr<AccessibilityTextMarker> textMarkerForPoint(int x, int y);
    virtual RefPtr<AccessibilityTextMarker> previousTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> nextTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityUIElement> accessibilityElementForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForLine(long);
    virtual JSRetainPtr<JSStringRef> stringForTextMarkerRange(AccessibilityTextMarkerRange*);
    virtual JSRetainPtr<JSStringRef> rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef);
    virtual JSRetainPtr<JSStringRef> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*);
    virtual JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*);
    virtual JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool);
    virtual int textMarkerRangeLength(AccessibilityTextMarkerRange*);
    virtual bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*);
    virtual int indexForTextMarker(AccessibilityTextMarker*);
    virtual bool isTextMarkerValid(AccessibilityTextMarker*);
    virtual bool isTextMarkerRangeValid(AccessibilityTextMarkerRange*);
    virtual bool isTextMarkerNull(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> textMarkerForIndex(int);
    virtual RefPtr<AccessibilityTextMarker> startTextMarker();
    virtual RefPtr<AccessibilityTextMarker> endTextMarker();
    virtual bool setSelectedTextMarkerRange(AccessibilityTextMarkerRange*);
    virtual RefPtr<AccessibilityTextMarkerRange> leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarker> previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    virtual RefPtr<AccessibilityTextMarkerRange> textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*);
    virtual JSRetainPtr<JSStringRef> textMarkerDebugDescription(AccessibilityTextMarker*);
    virtual JSRetainPtr<JSStringRef> textMarkerRangeDebugDescription(AccessibilityTextMarkerRange*);

    // Returns an ordered list of supported actions for an element.
    virtual JSRetainPtr<JSStringRef> supportedActions() const;
    virtual JSRetainPtr<JSStringRef> mathPostscriptsDescription() const;
    virtual JSRetainPtr<JSStringRef> mathPrescriptsDescription() const;
    virtual JSValueRef mathRootRadicand(JSContextRef);

    virtual JSRetainPtr<JSStringRef> pathDescription() const;

    // Notifications
    // Function callback should take one argument, the name of the notification.
    virtual bool addNotificationListener(JSContextRef, JSValueRef functionCallback);
    // Make sure you call remove, because you can't rely on objects being deallocated in a timely fashion.
    virtual bool removeNotificationListener();

    virtual JSRetainPtr<JSStringRef> identifier();
    virtual JSRetainPtr<JSStringRef> traits();
    virtual int elementTextPosition();
    virtual int elementTextLength();
    virtual JSRetainPtr<JSStringRef> stringForSelection();
    virtual void increaseTextSelection();
    virtual void decreaseTextSelection();
    virtual RefPtr<AccessibilityUIElement> linkedElement();
    virtual RefPtr<AccessibilityUIElement> headerElementAtIndex(unsigned index);
    virtual void assistiveTechnologySimulatedFocus();
    virtual bool isSearchField() const;
    virtual bool isSwitch() const;
    virtual bool isTextArea() const;

    virtual bool scrollPageUp();
    virtual bool scrollPageDown();
    virtual bool scrollPageLeft();
    virtual bool scrollPageRight();

    virtual bool isInDescriptionListDetail() const;
    virtual bool isInDescriptionListTerm() const;

    virtual bool hasTextEntryTrait();
    virtual bool hasTabBarTrait();
    virtual bool hasMenuItemTrait();
    virtual RefPtr<AccessibilityUIElement> fieldsetAncestorElement();

    virtual bool isInsertion() const;
    virtual bool isDeletion() const;
    virtual bool isFirstItemInSuggestion() const;
    virtual bool isLastItemInSuggestion() const;
    virtual bool isRemoteFrame() const;

    virtual bool isMarkAnnotation() const;
protected:
    AccessibilityUIElement(PlatformUIElement);
    AccessibilityUIElement(const AccessibilityUIElement&);

    static RefPtr<AccessibilityController> s_controller;
};

#ifdef __OBJC__
inline std::optional<RefPtr<AccessibilityUIElement>> makeVectorElement(const RefPtr<AccessibilityUIElement>*, id element) { return { { AccessibilityUIElement::create(element) } }; }

JSObjectRef makeJSArray(JSContextRef, NSArray *);
#endif

template<typename T>
JSObjectRef makeJSArray(JSContextRef context, const Vector<T>& elements)
{
    auto array = JSObjectMakeArray(context, 0, nullptr, nullptr);
    size_t size = elements.size();
    for (size_t i = 0; i < size; ++i)
        JSObjectSetPropertyAtIndex(context, array, i, JSObjectMake(context, elements[i]->wrapperClass(), elements[i].get()), nullptr);

    return array;
}

} // namespace WTR
