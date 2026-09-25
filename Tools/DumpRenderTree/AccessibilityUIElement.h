/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRefPtr.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
OBJC_CLASS NSString;
#endif

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#elif PLATFORM(WIN)
#undef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#include <WebCore/COMPtr.h>
#include <oleacc.h>
typedef COMPtr<IAccessible> PlatformUIElement;
#else
typedef void* PlatformUIElement;
#endif

class AccessibilityUIElement {
public:
#if PLATFORM(COCOA)
    AccessibilityUIElement(id);
#endif
#if !PLATFORM(COCOA)
    AccessibilityUIElement(PlatformUIElement);
#endif

#if PLATFORM(COCOA)
#ifdef __OBJC__
    id platformUIElement() const { return m_element.get(); }
#endif
#endif

#if !PLATFORM(COCOA)
    PlatformUIElement platformUIElement() const { return m_element; }
#endif

    bool hasPlatformUIElement() const { return !!m_element; }

    static JSObjectRef makeJSAccessibilityUIElement(JSContextRef, const AccessibilityUIElement&);

    bool isEqual(AccessibilityUIElement* otherElement);

    void getLinkedUIElements(Vector<AccessibilityUIElement>&);
    void getDocumentLinks(Vector<AccessibilityUIElement>&);
    void getChildren(Vector<AccessibilityUIElement>&);
    void getChildrenWithRange(Vector<AccessibilityUIElement>&, unsigned location, unsigned length);

    bool isInDescriptionListDetail() const;
    bool isInDescriptionListTerm() const;
    bool isInCell() const;

    AccessibilityUIElement elementAtPoint(int x, int y);
    AccessibilityUIElement getChildAtIndex(unsigned);
    unsigned indexOfChild(AccessibilityUIElement*);
    int childrenCount();
    AccessibilityUIElement titleUIElement();
    AccessibilityUIElement parentElement();

    void takeFocus();
    void takeSelection();
    void addSelection();
    void removeSelection();

    // Methods - platform-independent implementations
    RefPtr<OpaqueJSString> allAttributes();
    RefPtr<OpaqueJSString> attributesOfLinkedUIElements();
    AccessibilityUIElement linkedUIElementAtIndex(unsigned);

    RefPtr<OpaqueJSString> attributesOfDocumentLinks();
    RefPtr<OpaqueJSString> attributesOfChildren();
    RefPtr<OpaqueJSString> parameterizedAttributeNames();
    void increment();
    void decrement();
    void showMenu();
    void press();
    void dismiss();

    // Attributes - platform-independent implementations
    RefPtr<OpaqueJSString> stringAttributeValue(JSStringRef attribute);
    double numberAttributeValue(JSStringRef attribute);
    void uiElementArrayAttributeValue(JSStringRef attribute, Vector<AccessibilityUIElement>& elements) const;
    AccessibilityUIElement uiElementAttributeValue(JSStringRef attribute) const;
    bool boolAttributeValue(JSStringRef attribute);
#if PLATFORM(MAC)
    bool boolAttributeValue(NSString *attribute) const;
    RefPtr<OpaqueJSString> stringAttributeValue(NSString *attribute) const;
    double numberAttributeValue(NSString *attribute) const;
#endif
    void setBoolAttributeValue(JSStringRef attribute, bool value);
    bool isAttributeSupported(JSStringRef attribute);
    bool isAttributeSettable(JSStringRef attribute);
    bool isPressActionSupported();
    bool isIncrementActionSupported();
    bool isDecrementActionSupported();
    RefPtr<OpaqueJSString> role();
    RefPtr<OpaqueJSString> subrole();
    RefPtr<OpaqueJSString> roleDescription();
    RefPtr<OpaqueJSString> computedRoleString();
    RefPtr<OpaqueJSString> title();
    RefPtr<OpaqueJSString> description();
    RefPtr<OpaqueJSString> language();
    RefPtr<OpaqueJSString> stringValue();
    RefPtr<OpaqueJSString> accessibilityValue() const;
    void setValue(JSStringRef);
    RefPtr<OpaqueJSString> helpText() const;
    RefPtr<OpaqueJSString> liveRegionRelevant() const;
    RefPtr<OpaqueJSString> liveRegionStatus() const;
    RefPtr<OpaqueJSString> orientation() const;
    double pageX();
    double pageY();
    double x();
    double y();
    double width();
    double height();
    double intValue() const;
    double minValue();
    double maxValue();
    RefPtr<OpaqueJSString> pathDescription() const;
    RefPtr<OpaqueJSString> valueDescription();
    int insertionPointLineNumber();
    RefPtr<OpaqueJSString> selectedTextRange();
    RefPtr<OpaqueJSString> textInputMarkedRange() const;
    bool isAtomicLiveRegion() const;
    bool isBusy() const;
    bool isEnabled();
    bool isRequired() const;

    bool isFocused() const;
    bool isFocusable() const;
    bool isSelected() const;
    bool isSelectable() const;
    bool isMultiSelectable() const;
    bool isSelectedOptionActive() const;
    void setSelectedChild(AccessibilityUIElement*) const;
    unsigned selectedChildrenCount() const;
    AccessibilityUIElement selectedChildAtIndex(unsigned) const;
    void setSelectedChildAtIndex(unsigned) const;
    void removeSelectionAtIndex(unsigned) const;
    void clearSelectedChildren() const;

    bool isExpanded() const;
    bool isChecked() const;
    bool isVisible() const;
    bool isOnScreen() const;
    bool isOffScreen() const;
    bool isCollapsed() const;
    bool isIgnored() const;
    bool isSingleLine() const;
    bool isMultiLine() const;
    bool isIndeterminate() const;
    bool hasPopup() const;
    RefPtr<OpaqueJSString> popupValue() const;
    int hierarchicalLevel() const;
    double clickPointX();
    double clickPointY();
    RefPtr<OpaqueJSString> url();
    RefPtr<OpaqueJSString> classList() const;
    RefPtr<OpaqueJSString> domIdentifier() const;

    // CSS3-speech properties.
    RefPtr<OpaqueJSString> speakAs();

    // Table-specific attributes
    RefPtr<OpaqueJSString> attributesOfColumnHeaders();
    RefPtr<OpaqueJSString> attributesOfRowHeaders();
    RefPtr<OpaqueJSString> attributesOfColumns();
    RefPtr<OpaqueJSString> attributesOfRows();
    RefPtr<OpaqueJSString> attributesOfVisibleCells();
    RefPtr<OpaqueJSString> attributesOfHeader();
    int indexInTable();
    RefPtr<OpaqueJSString> rowIndexRange();
    RefPtr<OpaqueJSString> columnIndexRange();
    int rowCount();
    int columnCount();
    void rowHeaders(Vector<AccessibilityUIElement>& elements) const;
    void columnHeaders(Vector<AccessibilityUIElement>& elements) const;
    JSValueRef selectedCells(JSContextRef) const;

    // Tree/Outline specific attributes
    AccessibilityUIElement selectedRowAtIndex(unsigned);
    AccessibilityUIElement disclosedByRow();
    AccessibilityUIElement disclosedRowAtIndex(unsigned);
    AccessibilityUIElement rowAtIndex(unsigned);

    // ARIA specific
    AccessibilityUIElement ariaOwnsElementAtIndex(unsigned);
    AccessibilityUIElement ariaFlowToElementAtIndex(unsigned);
    AccessibilityUIElement ariaControlsElementAtIndex(unsigned);

#if PLATFORM(COCOA)
    RefPtr<OpaqueJSString> customContent() const;
#endif

    // Drag and drop
    bool isGrabbed() const;
    // A space concatentated string of all the drop effects.
    RefPtr<OpaqueJSString> ariaDropEffects() const;

    // Parameterized attributes
    int lineForIndex(int);
    RefPtr<OpaqueJSString> rangeForLine(int);
    RefPtr<OpaqueJSString> rangeForPosition(int x, int y);
    RefPtr<OpaqueJSString> boundsForRange(unsigned location, unsigned length);
    void setSelectedTextRange(unsigned location, unsigned length);
    RefPtr<OpaqueJSString> stringForRange(unsigned location, unsigned length);
    RefPtr<OpaqueJSString> attributedStringForRange(unsigned location, unsigned length);
    bool attributedStringRangeIsMisspelled(unsigned location, unsigned length);
    unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    AccessibilityUIElement uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    RefPtr<OpaqueJSString> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity);
#if PLATFORM(MAC)
    JSValueRef searchTextWithCriteria(JSContextRef, JSValueRef searchStrings, JSStringRef StartFrom, JSStringRef direction);
#endif

#if PLATFORM(IOS_FAMILY)
    RefPtr<OpaqueJSString> stringForSelection();
    void increaseTextSelection();
    void decreaseTextSelection();
    AccessibilityUIElement linkedElement();

    bool scrollPageUp();
    bool scrollPageDown();
    bool scrollPageLeft();
    bool scrollPageRight();

    bool hasTextEntryTrait();
    AccessibilityUIElement fieldsetAncestorElement();
    RefPtr<OpaqueJSString> attributedStringForElement();

    bool isDeletion();
    bool isInsertion();
    bool isFirstItemInSuggestion();
    bool isLastItemInSuggestion();
#endif

    // Table-specific
    AccessibilityUIElement cellForColumnAndRow(unsigned column, unsigned row);

    // Scrollarea-specific
    AccessibilityUIElement horizontalScrollbar() const;
    AccessibilityUIElement verticalScrollbar() const;

    // Text markers.
    AccessibilityTextMarkerRange lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarkerRange misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward);
    AccessibilityTextMarkerRange textMarkerRangeForElement(AccessibilityUIElement*);
    AccessibilityTextMarkerRange textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker);
    AccessibilityTextMarker startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*);
    AccessibilityTextMarker endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*);
    AccessibilityTextMarker endTextMarkerForBounds(int x, int y, int width, int height);
    AccessibilityTextMarker startTextMarkerForBounds(int x, int y, int width, int height);
    AccessibilityTextMarker textMarkerForPoint(int x, int y);
    AccessibilityTextMarker previousTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker nextTextMarker(AccessibilityTextMarker*);
    AccessibilityUIElement accessibilityElementForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker startTextMarker();
    AccessibilityTextMarker endTextMarker();
    AccessibilityTextMarkerRange leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarkerRange rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarkerRange paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarkerRange sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarker nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*);
    AccessibilityTextMarkerRange selectedTextMarkerRange();
    void resetSelectedTextMarkerRange();
    bool setSelectedTextMarkerRange(AccessibilityTextMarkerRange*);
    bool replaceTextInRange(JSStringRef, int position, int length);
    bool insertText(JSStringRef);
    AccessibilityTextMarkerRange textInputMarkedTextMarkerRange() const;

    RefPtr<OpaqueJSString> stringForTextMarkerRange(AccessibilityTextMarkerRange*);
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*);
    RefPtr<OpaqueJSString> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool includeSpellCheck);
    int textMarkerRangeLength(AccessibilityTextMarkerRange*);
    bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*);
    int indexForTextMarker(AccessibilityTextMarker*);
    bool isTextMarkerValid(AccessibilityTextMarker*);
    bool isTextMarkerNull(AccessibilityTextMarker*);
    AccessibilityTextMarker textMarkerForIndex(int);
    RefPtr<OpaqueJSString> textMarkerDebugDescription(AccessibilityTextMarker*);
    RefPtr<OpaqueJSString> textMarkerRangeDebugDescription(AccessibilityTextMarkerRange*);
    RefPtr<OpaqueJSString> textMarkerDescription(AccessibilityTextMarker*);
    RefPtr<OpaqueJSString> textMarkerRangeDescription(AccessibilityTextMarkerRange*);

    void scrollToMakeVisible();
    void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height);
    void scrollToGlobalPoint(int x, int y);

    // Notifications
    // Function callback should take one argument, the name of the notification.
    bool addNotificationListener(JSObjectRef functionCallback);
    // Make sure you call remove, because you can't rely on objects being deallocated in a timely fashion.
    void removeNotificationListener();

#if PLATFORM(IOS_FAMILY)
    RefPtr<OpaqueJSString> traits();
    RefPtr<OpaqueJSString> identifier();
    int elementTextPosition();
    int elementTextLength();
    AccessibilityUIElement headerElementAtIndex(unsigned);
    // This will simulate the accessibilityDidBecomeFocused API in UIKit.
    void assistiveTechnologySimulatedFocus();

    bool isTextArea() const;
    bool isSearchField() const;

    bool isMarkAnnotation() const;

    AccessibilityTextMarkerRange textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*);
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(COCOA)
    RefPtr<OpaqueJSString> embeddedImageDescription() const;
#endif

#if PLATFORM(MAC)
    // Returns an ordered list of supported actions for an element.
    RefPtr<OpaqueJSString> supportedActions();

    // A general description of the elements making up multiscript pre/post objects.
    RefPtr<OpaqueJSString> mathPostscriptsDescription() const;
    RefPtr<OpaqueJSString> mathPrescriptsDescription() const;
#endif

    static JSClassRef getJSClass();

private:

#if !PLATFORM(COCOA)
    PlatformUIElement m_element;
#endif

#if PLATFORM(COCOA)
    RetainPtr<id> m_element;
    RetainPtr<id> m_notificationHandler;
#endif
};
