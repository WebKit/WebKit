/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include <JavaScriptCore/JSRetainPtr.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#ifdef __OBJC__
typedef id PlatformUIElement;
#else
typedef struct objc_object* PlatformUIElement;
#endif
#elif PLATFORM(WIN)
#undef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h

#include <WebCore/COMPtr.h>
#include <oleacc.h>

typedef COMPtr<IAccessible> PlatformUIElement;
#elif HAVE(ACCESSIBILITY) && PLATFORM(GTK)
#include "AccessibilityNotificationHandlerAtk.h"
#include <atk/atk.h>
typedef AtkObject* PlatformUIElement;
#else
typedef void* PlatformUIElement;
#endif

#if PLATFORM(COCOA)
#ifdef __OBJC__
typedef id NotificationHandler;
#else
typedef struct objc_object* NotificationHandler;
#endif
#endif

class AccessibilityUIElement {
public:
    AccessibilityUIElement(PlatformUIElement);
    AccessibilityUIElement(const AccessibilityUIElement&);
    ~AccessibilityUIElement();

    PlatformUIElement platformUIElement() const { return m_element; }

    static JSObjectRef makeJSAccessibilityUIElement(JSContextRef, const AccessibilityUIElement&);

    bool isEqual(AccessibilityUIElement* otherElement);

    void getLinkedUIElements(Vector<AccessibilityUIElement>&);
    void getDocumentLinks(Vector<AccessibilityUIElement>&);
    void getChildren(Vector<AccessibilityUIElement>&);
    void getChildrenWithRange(Vector<AccessibilityUIElement>&, unsigned location, unsigned length);
    
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
    JSRetainPtr<JSStringRef> allAttributes();
    JSRetainPtr<JSStringRef> attributesOfLinkedUIElements();
    AccessibilityUIElement linkedUIElementAtIndex(unsigned);
    
    JSRetainPtr<JSStringRef> attributesOfDocumentLinks();
    JSRetainPtr<JSStringRef> attributesOfChildren();
    JSRetainPtr<JSStringRef> parameterizedAttributeNames();
    void increment();
    void decrement();
    void showMenu();
    void press();

    // Attributes - platform-independent implementations
    JSRetainPtr<JSStringRef> stringAttributeValue(JSStringRef attribute);
    double numberAttributeValue(JSStringRef attribute);
    void uiElementArrayAttributeValue(JSStringRef attribute, Vector<AccessibilityUIElement>& elements) const;
    AccessibilityUIElement uiElementAttributeValue(JSStringRef attribute) const;    
    bool boolAttributeValue(JSStringRef attribute);
    void setBoolAttributeValue(JSStringRef attribute, bool value);
    bool isAttributeSupported(JSStringRef attribute);
    bool isAttributeSettable(JSStringRef attribute);
    bool isPressActionSupported();
    bool isIncrementActionSupported();
    bool isDecrementActionSupported();
    JSRetainPtr<JSStringRef> role();
    JSRetainPtr<JSStringRef> subrole();
    JSRetainPtr<JSStringRef> roleDescription();
    JSRetainPtr<JSStringRef> computedRoleString();
    JSRetainPtr<JSStringRef> title();
    JSRetainPtr<JSStringRef> description();
    JSRetainPtr<JSStringRef> language();
    JSRetainPtr<JSStringRef> stringValue();
    JSRetainPtr<JSStringRef> accessibilityValue() const;
    void setValue(JSStringRef);
    JSRetainPtr<JSStringRef> helpText() const;
    JSRetainPtr<JSStringRef> orientation() const;
    double x();
    double y();
    double width();
    double height();
    double intValue() const;
    double minValue();
    double maxValue();
    JSRetainPtr<JSStringRef> pathDescription() const;
    JSRetainPtr<JSStringRef> valueDescription();
    int insertionPointLineNumber();
    JSRetainPtr<JSStringRef> selectedTextRange();
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
    bool isOffScreen() const;
    bool isCollapsed() const;
    bool isIgnored() const;
    bool isSingleLine() const;
    bool isMultiLine() const;
    bool isIndeterminate() const;
    bool hasPopup() const;
    JSRetainPtr<JSStringRef> popupValue() const;
    int hierarchicalLevel() const;
    double clickPointX();
    double clickPointY();
    JSRetainPtr<JSStringRef> documentEncoding();
    JSRetainPtr<JSStringRef> documentURI();
    JSRetainPtr<JSStringRef> url();
    JSRetainPtr<JSStringRef> classList() const;

    // CSS3-speech properties.
    JSRetainPtr<JSStringRef> speakAs();
    
    // Table-specific attributes
    JSRetainPtr<JSStringRef> attributesOfColumnHeaders();
    JSRetainPtr<JSStringRef> attributesOfRowHeaders();
    JSRetainPtr<JSStringRef> attributesOfColumns();
    JSRetainPtr<JSStringRef> attributesOfRows();
    JSRetainPtr<JSStringRef> attributesOfVisibleCells();
    JSRetainPtr<JSStringRef> attributesOfHeader();
    int indexInTable();
    JSRetainPtr<JSStringRef> rowIndexRange();
    JSRetainPtr<JSStringRef> columnIndexRange();
    int rowCount();
    int columnCount();
    void rowHeaders(Vector<AccessibilityUIElement>& elements) const;
    void columnHeaders(Vector<AccessibilityUIElement>& elements) const;
    
    // Tree/Outline specific attributes
    AccessibilityUIElement selectedRowAtIndex(unsigned);
    AccessibilityUIElement disclosedByRow();
    AccessibilityUIElement disclosedRowAtIndex(unsigned);
    AccessibilityUIElement rowAtIndex(unsigned);

    // ARIA specific
    AccessibilityUIElement ariaOwnsElementAtIndex(unsigned);
    AccessibilityUIElement ariaFlowToElementAtIndex(unsigned);
    AccessibilityUIElement ariaControlsElementAtIndex(unsigned);

    // ARIA Drag and Drop
    bool ariaIsGrabbed() const;
    // A space concatentated string of all the drop effects.
    JSRetainPtr<JSStringRef> ariaDropEffects() const;
    
    // Parameterized attributes
    int lineForIndex(int);
    JSRetainPtr<JSStringRef> rangeForLine(int);
    JSRetainPtr<JSStringRef> rangeForPosition(int x, int y);
    JSRetainPtr<JSStringRef> boundsForRange(unsigned location, unsigned length);
    void setSelectedTextRange(unsigned location, unsigned length);
    JSRetainPtr<JSStringRef> stringForRange(unsigned location, unsigned length);
    JSRetainPtr<JSStringRef> attributedStringForRange(unsigned location, unsigned length);
    bool attributedStringRangeIsMisspelled(unsigned location, unsigned length);
    unsigned uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    AccessibilityUIElement uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);
    JSRetainPtr<JSStringRef> selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity);
#if PLATFORM(MAC)
    JSValueRef searchTextWithCriteria(JSContextRef, JSValueRef searchStrings, JSStringRef StartFrom, JSStringRef direction);
#endif

#if PLATFORM(IOS_FAMILY)
    void elementsForRange(unsigned location, unsigned length, Vector<AccessibilityUIElement>& elements);
    JSRetainPtr<JSStringRef> stringForSelection();
    void increaseTextSelection();
    void decreaseTextSelection();
    AccessibilityUIElement linkedElement();
    
    bool scrollPageUp();
    bool scrollPageDown();
    bool scrollPageLeft();
    bool scrollPageRight();
    
    bool hasContainedByFieldsetTrait();
    AccessibilityUIElement fieldsetAncestorElement();
    JSRetainPtr<JSStringRef> attributedStringForElement();
#endif

#if PLATFORM(GTK)
    // Text-specific
    JSRetainPtr<JSStringRef> characterAtOffset(int offset);
    JSRetainPtr<JSStringRef> wordAtOffset(int offset);
    JSRetainPtr<JSStringRef> lineAtOffset(int offset);
    JSRetainPtr<JSStringRef> sentenceAtOffset(int offset);
#endif

    // Table-specific
    AccessibilityUIElement cellForColumnAndRow(unsigned column, unsigned row);

    // Scrollarea-specific
    AccessibilityUIElement horizontalScrollbar() const;
    AccessibilityUIElement verticalScrollbar() const;

    // Text markers.
    AccessibilityTextMarkerRange lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*);
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
    bool setSelectedVisibleTextRange(AccessibilityTextMarkerRange*);
    bool replaceTextInRange(JSStringRef, int position, int length);
    bool insertText(JSStringRef);

    JSRetainPtr<JSStringRef> stringForTextMarkerRange(AccessibilityTextMarkerRange*);
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*);
    JSRetainPtr<JSStringRef> attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool includeSpellCheck);
    int textMarkerRangeLength(AccessibilityTextMarkerRange*);
    bool attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*);
    int indexForTextMarker(AccessibilityTextMarker*);
    bool isTextMarkerValid(AccessibilityTextMarker*);
    AccessibilityTextMarker textMarkerForIndex(int);
    
    void scrollToMakeVisible();
    void scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height);
    void scrollToGlobalPoint(int x, int y);

    // Notifications
    // Function callback should take one argument, the name of the notification.
    bool addNotificationListener(JSObjectRef functionCallback);
    // Make sure you call remove, because you can't rely on objects being deallocated in a timely fashion.
    void removeNotificationListener();
    
#if PLATFORM(IOS_FAMILY)
    JSRetainPtr<JSStringRef> traits();
    JSRetainPtr<JSStringRef> identifier();
    int elementTextPosition();
    int elementTextLength();
    AccessibilityUIElement headerElementAtIndex(unsigned);
    // This will simulate the accessibilityDidBecomeFocused API in UIKit.
    void assistiveTechnologySimulatedFocus();
    
    bool isTextArea() const;
    bool isSearchField() const;
    
    AccessibilityTextMarkerRange textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*);
    
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
    // Returns an ordered list of supported actions for an element.
    JSRetainPtr<JSStringRef> supportedActions();
    
    // A general description of the elements making up multiscript pre/post objects.
    JSRetainPtr<JSStringRef> mathPostscriptsDescription() const;
    JSRetainPtr<JSStringRef> mathPrescriptsDescription() const;
#endif
    
private:
    static JSClassRef getJSClass();
    PlatformUIElement m_element;
    
#if PLATFORM(COCOA)
    // A retained, platform specific object used to help manage notifications for this object.
    NotificationHandler m_notificationHandler;
#endif

#if HAVE(ACCESSIBILITY) && PLATFORM(GTK)
    RefPtr<AccessibilityNotificationHandler> m_notificationHandler;
#endif
};
