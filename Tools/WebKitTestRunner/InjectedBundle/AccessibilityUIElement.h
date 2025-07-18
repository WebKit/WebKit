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
#if USE(ATSPI)
class AccessibilityNotificationHandler;
#endif

class AccessibilityUIElement : public JSWrappable {
#if PLATFORM(COCOA)
    // Helper functions that dispatch the corresponding AccessibilityObjectWrapper method to the AX secondary thread when appropriate.
    friend RetainPtr<NSArray> supportedAttributes(id);
    friend void setAttributeValue(id, NSString *, id, bool synchronous);
#endif

public:
    static Ref<AccessibilityUIElement> create(PlatformUIElement);
    static Ref<AccessibilityUIElement> create(const AccessibilityUIElement&);

    ~AccessibilityUIElement();

#if PLATFORM(COCOA)
    id platformUIElement() { return m_element.getAutoreleased(); }
#elif USE(ATSPI)
    PlatformUIElement platformUIElement() { return m_element.get(); }
#else
    PlatformUIElement platformUIElement() { return m_element; }
#endif

    virtual JSClassRef wrapperClass();

    static JSObjectRef makeJSAccessibilityUIElement(JSContextRef, const AccessibilityUIElement&);

    bool isEqual(AccessibilityUIElement* otherElement);
    JSRetainPtr<JSStringRef> domIdentifier() const;

    RefPtr<AccessibilityUIElement> elementAtPoint(int x, int y);
    RefPtr<AccessibilityUIElement> elementAtPointWithRemoteElement(int x, int y);
    void elementAtPointResolvingRemoteFrame(JSContextRef, int x, int y, JSValueRef callback);

    JSValueRef children(JSContextRef);
    RefPtr<AccessibilityUIElement> childAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> childAtIndexWithRemoteElement(unsigned);
    unsigned indexOfChild(AccessibilityUIElement*);
    unsigned childrenCount();
    RefPtr<AccessibilityUIElement> titleUIElement();
    RefPtr<AccessibilityUIElement> parentElement();

    void takeFocus();
    void takeSelection();
    void addSelection();
    void removeSelection();

    // Methods - platform-independent implementations
    JSRetainPtr<JSStringRef> allAttributes();
    JSRetainPtr<JSStringRef> attributesOfLinkedUIElements();
    RefPtr<AccessibilityUIElement> linkedUIElementAtIndex(unsigned);
    
    JSRetainPtr<JSStringRef> attributesOfDocumentLinks();
    JSRetainPtr<JSStringRef> attributesOfChildren();
    JSRetainPtr<JSStringRef> parameterizedAttributeNames();
    void increment();
    void decrement();
    void showMenu();
    void press();
    bool dismiss();
#if PLATFORM(MAC)
    void syncPress();
    void asyncIncrement();
    void asyncDecrement();
    RefPtr<AccessibilityUIElement> focusableAncestor();
    RefPtr<AccessibilityUIElement> editableAncestor();
    RefPtr<AccessibilityUIElement> highestEditableAncestor();
    JSRetainPtr<JSStringRef> selectedText();
#else
    void syncPress() { press(); }
    void asyncIncrement() { }
    void asyncDecrement() { };
    RefPtr<AccessibilityUIElement> focusableAncestor() { return nullptr; }
    RefPtr<AccessibilityUIElement> editableAncestor() { return nullptr; }
    RefPtr<AccessibilityUIElement> highestEditableAncestor() { return nullptr; }
    JSRetainPtr<JSStringRef> selectedText() { return nullptr; }
#endif // PLATFORM(MAC)

#if PLATFORM(COCOA)
    JSRetainPtr<JSStringRef> dateTimeValue() const;
#else
    JSRetainPtr<JSStringRef> dateTimeValue() const { return nullptr; }
#endif // PLATFORM(COCOA)

    // Attributes - platform-independent implementations
    JSRetainPtr<JSStringRef> stringDescriptionOfAttributeValue(JSStringRef attribute);
    JSRetainPtr<JSStringRef> stringAttributeValue(JSStringRef attribute);
    double numberAttributeValue(JSStringRef attribute);
    JSValueRef uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute);
    RefPtr<AccessibilityUIElement> uiElementAttributeValue(JSStringRef attribute) const;
    bool boolAttributeValue(JSStringRef attribute);
#if PLATFORM(MAC)
    RetainPtr<id> attributeValue(NSString *) const;
    void attributeValueAsync(JSContextRef, JSStringRef attribute, JSValueRef callback);
    NSArray *actionNames() const;
    bool performAction(NSString *) const;
#else
    void attributeValueAsync(JSContextRef, JSStringRef attribute, JSValueRef callback) { }
#endif
    void setBoolAttributeValue(JSStringRef attribute, bool value);
    bool isAttributeSupported(JSStringRef attribute);
    bool isAttributeSettable(JSStringRef attribute);
    bool isPressActionSupported();
    bool isIncrementActionSupported();
    bool isDecrementActionSupported();
    void setValue(JSStringRef);
    JSRetainPtr<JSStringRef> role();
    JSRetainPtr<JSStringRef> subrole();
    JSRetainPtr<JSStringRef> roleDescription();
    JSRetainPtr<JSStringRef> computedRoleString();
    JSRetainPtr<JSStringRef> title();
    JSRetainPtr<JSStringRef> description();
    JSRetainPtr<JSStringRef> language();
    JSRetainPtr<JSStringRef> stringValue();
    JSRetainPtr<JSStringRef> dateValue();
    JSRetainPtr<JSStringRef> accessibilityValue() const;
    JSRetainPtr<JSStringRef> helpText() const;
    JSRetainPtr<JSStringRef> orientation() const;
    JSRetainPtr<JSStringRef> liveRegionRelevant() const;
    JSRetainPtr<JSStringRef> liveRegionStatus() const;
    double pageX();
    double pageY();
    double x();
    double y();
    double width();
    double height();
    JSRetainPtr<JSStringRef> lineRectsAndText() const;
    JSRetainPtr<JSStringRef> brailleLabel() const;
    JSRetainPtr<JSStringRef> brailleRoleDescription() const;

    double intValue() const;
    double minValue();
    double maxValue();
    JSRetainPtr<JSStringRef> valueDescription();
    unsigned numberOfCharacters() const;
    int insertionPointLineNumber();
    JSRetainPtr<JSStringRef> selectedTextRange();
    JSRetainPtr<JSStringRef> intersectionWithSelectionRange();
    JSRetainPtr<JSStringRef> textInputMarkedRange() const;
    bool isAtomicLiveRegion() const;
    bool isBusy() const;
    bool isEnabled();
    bool isRequired() const;

    RefPtr<AccessibilityUIElement> focusedElement() const;
    bool isFocused() const;
    bool isFocusable() const;
    bool isSelected() const;
    bool isSelectedOptionActive() const;
    bool isSelectable() const;
    bool isMultiSelectable() const;
    void setSelectedChild(AccessibilityUIElement*) const;
    void setSelectedChildAtIndex(unsigned) const;
    void removeSelectionAtIndex(unsigned) const;
    void clearSelectedChildren() const;
    RefPtr<AccessibilityUIElement> activeElement() const;
    JSValueRef selectedChildren(JSContextRef);
    unsigned selectedChildrenCount() const;
    RefPtr<AccessibilityUIElement> selectedChildAtIndex(unsigned) const;

    bool isValid() const;
    bool isExpanded() const;
    bool supportsExpanded() const;
    bool isChecked() const;
    JSRetainPtr<JSStringRef> currentStateValue() const;
    JSRetainPtr<JSStringRef> sortDirection() const;
    bool isIndeterminate() const;
    bool isVisible() const;
    bool isOnScreen() const;
    bool isOffScreen() const;
    bool isCollapsed() const;
    bool isIgnored() const;
    bool isSingleLine() const;
    bool isMultiLine() const;
    bool hasPopup() const;
    JSRetainPtr<JSStringRef> popupValue() const;
    int hierarchicalLevel() const;
    double clickPointX();
    double clickPointY();
    JSRetainPtr<JSStringRef> url();
    JSRetainPtr<JSStringRef> classList() const;
    JSRetainPtr<JSStringRef> embeddedImageDescription() const;
    JSValueRef imageOverlayElements(JSContextRef);

    // CSS3-speech properties.
    JSRetainPtr<JSStringRef> speakAs();
    
    // Table-specific attributes
    JSRetainPtr<JSStringRef> attributesOfColumnHeaders();
    JSRetainPtr<JSStringRef> attributesOfRowHeaders();
    JSRetainPtr<JSStringRef> attributesOfColumns();
    JSValueRef columns(JSContextRef);
    JSRetainPtr<JSStringRef> attributesOfRows();
    JSRetainPtr<JSStringRef> attributesOfVisibleCells();
    JSRetainPtr<JSStringRef> attributesOfHeader();
    bool isInCell() const;
    bool isInTable() const;
    bool isInList() const;
    bool isInLandmark() const;
    int indexInTable();
    JSRetainPtr<JSStringRef> rowIndexRange();
    JSRetainPtr<JSStringRef> columnIndexRange();
    int rowCount();
    int columnCount();
    JSValueRef rowHeaders(JSContextRef);
    JSValueRef columnHeaders(JSContextRef);
    JSRetainPtr<JSStringRef> customContent() const;
    JSValueRef selectedCells(JSContextRef);

    // Tree/Outline specific attributes
    RefPtr<AccessibilityUIElement> selectedRowAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> disclosedByRow();
    RefPtr<AccessibilityUIElement> disclosedRowAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> rowAtIndex(unsigned);

    // Relationships.
    // FIXME: replace all ***AtIndex methods with ones that return an array and make the naming consistent.
    RefPtr<AccessibilityUIElement> controllerElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> ariaControlsElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> ariaDescribedByElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> descriptionForElementAtIndex(unsigned);
    JSValueRef detailsElements(JSContextRef);
    RefPtr<AccessibilityUIElement> ariaDetailsElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> detailsForElementAtIndex(unsigned);
    JSValueRef errorMessageElements(JSContextRef);
    RefPtr<AccessibilityUIElement> ariaErrorMessageElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> errorMessageForElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> flowFromElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> ariaFlowToElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> ariaLabelledByElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> labelForElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> ownerElementAtIndex(unsigned);
    RefPtr<AccessibilityUIElement> ariaOwnsElementAtIndex(unsigned);

    // Drag and drop
    bool isGrabbed() const;
    // A space concatentated string of all the drop effects.
    JSRetainPtr<JSStringRef> ariaDropEffects() const;
    
    // Parameterized attributes
    int lineForIndex(int);
    JSRetainPtr<JSStringRef> rangeForLine(int);
    JSRetainPtr<JSStringRef> rangeForPosition(int x, int y);
    JSRetainPtr<JSStringRef> boundsForRange(unsigned location, unsigned length);
#if PLATFORM(MAC)
    JSRetainPtr<JSStringRef> boundsForRangeWithPagePosition(unsigned location, unsigned length);
#else
    JSRetainPtr<JSStringRef> boundsForRangeWithPagePosition(unsigned location, unsigned length) { return createJSString(); };
#endif
    bool setSelectedTextRange(unsigned location, unsigned length);
    JSRetainPtr<JSStringRef> stringForRange(unsigned location, unsigned length);
    JSRetainPtr<JSStringRef> attributedStringForRange(unsigned location, unsigned length);
    JSRetainPtr<JSStringRef> attributedStringForElement();

    bool attributedStringRangeIsMisspelled(unsigned location, unsigned length);
    unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    RefPtr<AccessibilityUIElement> uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    JSRetainPtr<JSStringRef> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity);
    JSValueRef searchTextWithCriteria(JSContextRef, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction);
    JSValueRef performTextOperation(JSContextRef, JSStringRef operationType, JSValueRef markerRanges, JSValueRef replacementStrings, bool shouldSmartReplace);

    // Text-specific
    JSRetainPtr<JSStringRef> characterAtOffset(int offset);
    JSRetainPtr<JSStringRef> wordAtOffset(int offset);
    JSRetainPtr<JSStringRef> lineAtOffset(int offset);
    JSRetainPtr<JSStringRef> sentenceAtOffset(int offset);
    
    // Table-specific
    RefPtr<AccessibilityUIElement> cellForColumnAndRow(unsigned column, unsigned row);

    // Scrollarea-specific
    RefPtr<AccessibilityUIElement> horizontalScrollbar() const;
    RefPtr<AccessibilityUIElement> verticalScrollbar() const;

    void scrollToMakeVisible();
    void scrollToGlobalPoint(int x, int y);
    void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height);

    // Text markers.
    RefPtr<AccessibilityTextMarkerRange> lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> rightLineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> leftLineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> previousLineStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> nextLineEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    int lineIndexForTextMarker(AccessibilityTextMarker*) const;
    RefPtr<AccessibilityTextMarkerRange> styleTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForSearchPredicate(JSContextRef, AccessibilityTextMarkerRange* startRange, bool forward, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    RefPtr<AccessibilityTextMarkerRange> misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForElement(AccessibilityUIElement*);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForUnorderedMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForRange(unsigned location, unsigned length);
    RefPtr<AccessibilityTextMarkerRange> selectedTextMarkerRange();
    void resetSelectedTextMarkerRange();
    bool replaceTextInRange(JSStringRef, int position, int length);
    bool insertText(JSStringRef);
    RefPtr<AccessibilityTextMarkerRange> textInputMarkedTextMarkerRange() const;
    RefPtr<AccessibilityTextMarker> startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*);
    RefPtr<AccessibilityTextMarker> endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*);
    RefPtr<AccessibilityTextMarker> endTextMarkerForBounds(int x, int y, int width, int height);
    RefPtr<AccessibilityTextMarker> startTextMarkerForBounds(int x, int y, int width, int height);
    RefPtr<AccessibilityTextMarker> textMarkerForPoint(int x, int y);
    RefPtr<AccessibilityTextMarker> previousTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> nextTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityUIElement> accessibilityElementForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeForLine(long);
    JSRetainPtr<JSStringRef> stringForTextMarkerRange(AccessibilityTextMarkerRange*);
    JSRetainPtr<JSStringRef> rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef);
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*);
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*);
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool);
    int textMarkerRangeLength(AccessibilityTextMarkerRange*);
    bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*);
    int indexForTextMarker(AccessibilityTextMarker*);
    bool isTextMarkerValid(AccessibilityTextMarker*);
    bool isTextMarkerRangeValid(AccessibilityTextMarkerRange*);
    bool isTextMarkerNull(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> textMarkerForIndex(int);
    RefPtr<AccessibilityTextMarker> startTextMarker();
    RefPtr<AccessibilityTextMarker> endTextMarker();
    bool setSelectedTextMarkerRange(AccessibilityTextMarkerRange*);
    RefPtr<AccessibilityTextMarkerRange> leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarker> previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    RefPtr<AccessibilityTextMarkerRange> textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*);
    JSRetainPtr<JSStringRef> textMarkerDebugDescription(AccessibilityTextMarker*);
    JSRetainPtr<JSStringRef> textMarkerRangeDebugDescription(AccessibilityTextMarkerRange*);

    // Returns an ordered list of supported actions for an element.
    JSRetainPtr<JSStringRef> supportedActions() const;
    JSRetainPtr<JSStringRef> mathPostscriptsDescription() const;
    JSRetainPtr<JSStringRef> mathPrescriptsDescription() const;
    JSValueRef mathRootRadicand(JSContextRef);

    JSRetainPtr<JSStringRef> pathDescription() const;
    
    // Notifications
    // Function callback should take one argument, the name of the notification.
    bool addNotificationListener(JSContextRef, JSValueRef functionCallback);
    // Make sure you call remove, because you can't rely on objects being deallocated in a timely fashion.
    bool removeNotificationListener();
    
    JSRetainPtr<JSStringRef> identifier();
    JSRetainPtr<JSStringRef> traits();
    int elementTextPosition();
    int elementTextLength();
    JSRetainPtr<JSStringRef> stringForSelection();
    void increaseTextSelection();
    void decreaseTextSelection();
    RefPtr<AccessibilityUIElement> linkedElement();
    RefPtr<AccessibilityUIElement> headerElementAtIndex(unsigned index);
    void assistiveTechnologySimulatedFocus();
    bool isSearchField() const;
    bool isSwitch() const;
    bool isTextArea() const;

    bool scrollPageUp();
    bool scrollPageDown();
    bool scrollPageLeft();
    bool scrollPageRight();
    
    bool isInDescriptionListDetail() const;
    bool isInDescriptionListTerm() const;

    bool hasTextEntryTrait();
    bool hasTabBarTrait();
    bool hasMenuItemTrait();
    RefPtr<AccessibilityUIElement> fieldsetAncestorElement();

    bool isInsertion() const;
    bool isDeletion() const;
    bool isFirstItemInSuggestion() const;
    bool isLastItemInSuggestion() const;
    bool isRemoteFrame() const;
    
    bool isMarkAnnotation() const;
private:
    AccessibilityUIElement(PlatformUIElement);
    AccessibilityUIElement(const AccessibilityUIElement&);

#if PLATFORM(MAC)
    RetainPtr<id> attributeValueForParameter(NSString *, id) const;
    unsigned arrayAttributeCount(NSString *) const;
    RetainPtr<NSString> descriptionOfValue(id valueObject) const;
    bool boolAttributeValueNS(NSString *attribute) const;
    JSRetainPtr<JSStringRef> stringAttributeValueNS(NSString *attribute) const;
    double numberAttributeValueNS(NSString *attribute) const;
    bool isAttributeSettableNS(NSString *) const;
#endif

#if !PLATFORM(COCOA) && !USE(ATSPI)
    PlatformUIElement m_element;
#endif

    // A retained, platform specific object used to help manage notifications for this object.
#if PLATFORM(COCOA)
    WeakObjCPtr<id> m_element;
    RetainPtr<id> m_notificationHandler;
    static RefPtr<AccessibilityController> s_controller;

    void getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement> >&);
    void getDocumentLinks(Vector<RefPtr<AccessibilityUIElement> >&);
    RefPtr<AccessibilityUIElement> elementForAttribute(NSString*) const;
    RefPtr<AccessibilityUIElement> elementForAttributeAtIndex(NSString*, unsigned) const;

    void getUIElementsWithAttribute(JSStringRef, Vector<RefPtr<AccessibilityUIElement> >&) const;
#endif

    Vector<RefPtr<AccessibilityUIElement>> getChildren() const;
    Vector<RefPtr<AccessibilityUIElement>> getChildrenInRange(unsigned location, unsigned length) const;

#if USE(ATSPI)
    static RefPtr<AccessibilityController> s_controller;
    RefPtr<WebCore::AccessibilityObjectAtspi> m_element;
    std::unique_ptr<AccessibilityNotificationHandler> m_notificationHandler;
#endif
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
