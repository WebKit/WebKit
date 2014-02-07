/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AccessibilityNotificationHandler.h"
#import "AccessibilityUIElement.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"
#import <UIKit/UIKit.h>

namespace WTR {

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
    // FIXME: ap@webkit.org says ObjC objects need to be CFRetained/CFRelease to be GC-compliant on the mac.
    [m_element retain];
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : JSWrappable()
    , m_element(other.m_element)
{
    [m_element retain];
}

AccessibilityUIElement::~AccessibilityUIElement()
{
    [m_element release];
}

bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    if (!otherElement)
        return false;
    return platformUIElement() == otherElement->platformUIElement();
}

void AccessibilityUIElement::getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
}

void AccessibilityUIElement::getDocumentLinks(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
    NSInteger childCount = [m_element accessibilityElementCount];
    for (NSInteger k = 0; k < childCount; ++k)
        elementVector.append(AccessibilityUIElement::create([m_element accessibilityElementAtIndex:k]));
}

void AccessibilityUIElement::getChildrenWithRange(Vector<RefPtr<AccessibilityUIElement> >& elementVector, unsigned location, unsigned length)
{
    NSUInteger childCount = [m_element accessibilityElementCount];
    for (NSUInteger k = location; k < childCount && k < (location+length); ++k)
        elementVector.append(AccessibilityUIElement::create([m_element accessibilityElementAtIndex:k]));
}

int AccessibilityUIElement::childrenCount()
{
    Vector<RefPtr<AccessibilityUIElement>> children;
    getChildren(children);

    return children.size();
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int x, int y)
{
    return 0;
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned index)
{
    Vector<RefPtr<AccessibilityUIElement> > children;
    getChildrenWithRange(children, index, 1);

    if (children.size() == 1)
        return children[0];
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    return 0;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    return nil;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    return JSStringCreateWithCharacters(0, 0);
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    return 0;
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute) const
{
    return 0;
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    return nullptr;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    return 0;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    return JSStringCreateWithCharacters(0, 0);
}

double AccessibilityUIElement::x()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.origin.x;
}

double AccessibilityUIElement::y()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.origin.y;
}

double AccessibilityUIElement::width()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.size.width;
}

double AccessibilityUIElement::height()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.size.height;
}

double AccessibilityUIElement::clickPointX()
{
    return 0;
}

double AccessibilityUIElement::clickPointY()
{
    return 0;
}

double AccessibilityUIElement::intValue() const
{
    return 0;
}

double AccessibilityUIElement::minValue()
{
    return 0;
}

double AccessibilityUIElement::maxValue()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    return (traits & UIAccessibilityTraitSelected);
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isExpanded() const
{
    return false;
}

bool AccessibilityUIElement::isChecked() const
{
    return false;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speak()
{
    return nullptr;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    return JSStringCreateWithCharacters(0, 0);
}

// parameterized attributes
int AccessibilityUIElement::lineForIndex(int index)
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int x, int y)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned location, unsigned length)
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::rowCount()
{
    return -1;
}

int AccessibilityUIElement::columnCount()
{
    return -1;
}

int AccessibilityUIElement::indexInTable()
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    return JSStringCreateWithCharacters(0, 0);
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    return 0;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    return JSStringCreateWithCharacters(0, 0);
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
}

void AccessibilityUIElement::increment()
{
    [m_element accessibilityIncrement];
}

void AccessibilityUIElement::decrement()
{
    [m_element accessibilityDecrement];
}

void AccessibilityUIElement::showMenu()
{
}

void AccessibilityUIElement::press()
{
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::addNotificationListener(JSValueRef functionCallback)
{
    return false;
}

bool AccessibilityUIElement::removeNotificationListener()
{
    return false;
}

bool AccessibilityUIElement::isFocusable() const
{
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    return false;
}

void AccessibilityUIElement::takeFocus()
{
    // FIXME: implement
}

void AccessibilityUIElement::takeSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::addSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::removeSelection()
{
    // FIXME: implement
}

// Text markers
PassRefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    return 0;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    return 0;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker* marker)
{
    return -1;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    return false;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    return 0;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    return nullptr;
}

PassRefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    return 0;
}

} // namespace WTR

