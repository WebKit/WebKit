/*
 * Copyright (C) 2021 Igalia S.L.
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

#include "config.h"
#include "AccessibilityUIElement.h"

#if HAVE(ACCESSIBILITY) && USE(ATSPI)
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/AccessibilityObjectAtspi.h>
#include <WebKit/WKBundleFrame.h>

namespace WTR {

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : JSWrappable()
    , m_element(other.m_element)
{
}

AccessibilityUIElement::~AccessibilityUIElement()
{
}

bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    return m_element.get() == otherElement->platformUIElement();
}

bool AccessibilityUIElement::isIsolatedObject() const
{
    return true;
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement> >& children)
{
}

void AccessibilityUIElement::getChildrenWithRange(Vector<RefPtr<AccessibilityUIElement> >& children, unsigned location, unsigned length)
{
}

int AccessibilityUIElement::childrenCount()
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int x, int y)
{
    return nullptr;
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    return nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    return nullptr;
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringDescriptionOfAttributeValue(JSStringRef attribute)
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
    return nullptr;
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    return nullptr;
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
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
    return 0;
}

double AccessibilityUIElement::y()
{
    return 0;
}

double AccessibilityUIElement::width()
{
    return 0;
}

double AccessibilityUIElement::height()
{
    return 0;
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
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
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

bool AccessibilityUIElement::isIndeterminate() const
{
    return false;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::lineForIndex(int index)
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int x, int y)
{
    return JSStringCreateWithCharacters(0, 0);
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

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
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
    return 0;
}

int AccessibilityUIElement::columnCount()
{
    return 0;
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

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    return false;
}

void AccessibilityUIElement::increment()
{
}

void AccessibilityUIElement::decrement()
{
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

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
}

void AccessibilityUIElement::clearSelectedChildren() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
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
    return true;
}

bool AccessibilityUIElement::isFocusable() const
{
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    return false;
}

bool AccessibilityUIElement::isSingleLine() const
{
    return false;
}

bool AccessibilityUIElement::isMultiLine() const
{
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    return false;
}

void AccessibilityUIElement::takeFocus()
{
}

void AccessibilityUIElement::takeSelection()
{
}

void AccessibilityUIElement::addSelection()
{
}

void AccessibilityUIElement::removeSelection()
{
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rectsForTextMarkerRange(AccessibilityTextMarkerRange* markerRange, JSStringRef searchText)
{
    return JSStringCreateWithCharacters(0, 0);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    return nullptr;
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

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return false;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::characterAtOffset(int offset)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::wordAtOffset(int offset)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineAtOffset(int offset)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sentenceAtOffset(int offset)
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef, int, int)
{
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    return nullptr;
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY) && USE(ATSPI)
