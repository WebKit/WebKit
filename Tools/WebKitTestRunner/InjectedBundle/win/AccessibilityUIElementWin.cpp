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

#include "config.h"
#include "AccessibilityUIElement.h"

#if HAVE(ACCESSIBILITY)

#include <WebCore/NotImplemented.h>

namespace WTR {

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement)
{
    notImplemented();
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement&)
{
    notImplemented();
}

AccessibilityUIElement::~AccessibilityUIElement()
{
    notImplemented();
}

bool AccessibilityUIElement::isEqual(AccessibilityUIElement*)
{
    notImplemented();
    return false;
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement>>&)
{
    notImplemented();
}

void AccessibilityUIElement::getChildrenWithRange(Vector<RefPtr<AccessibilityUIElement>>&, unsigned, unsigned)
{
    notImplemented();
}

int AccessibilityUIElement::childrenCount()
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int, int)
{
    notImplemented();
    return nullptr;
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement*)
{
    notImplemented();
    return 0;
}


RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned) const
{
    notImplemented();
    return nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef)
{
    notImplemented();
    return nullptr;
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    notImplemented();
    return 0;
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute) const
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    notImplemented();
    return nullptr;
}

double AccessibilityUIElement::x()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::y()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::width()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::height()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::clickPointX()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::clickPointY()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::intValue() const
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::minValue()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElement::maxValue()
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    notImplemented();
    return 0;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isExpanded() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isChecked() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    notImplemented();
    return false;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElement::lineForIndex(int)
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int, int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    notImplemented();
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool)
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef, JSStringRef, JSValueRef, JSStringRef, JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElement::rowCount()
{
    notImplemented();
    return 0;
}

int AccessibilityUIElement::columnCount()
{
    notImplemented();
    return 0;
}

int AccessibilityUIElement::indexInTable()
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextRange(unsigned, unsigned)
{
    notImplemented();
    return false;
}

void AccessibilityUIElement::increment()
{
    notImplemented();
}

void AccessibilityUIElement::decrement()
{
    notImplemented();
}

void AccessibilityUIElement::showMenu()
{
    notImplemented();
}

void AccessibilityUIElement::press()
{
    notImplemented();
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
    notImplemented();
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
    notImplemented();
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
    notImplemented();
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    notImplemented();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::addNotificationListener(JSValueRef functionCallback)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::removeNotificationListener()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isFocusable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isSingleLine() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::isMultiLine() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    notImplemented();
    return false;
}

void AccessibilityUIElement::takeFocus()
{
    notImplemented();
}

void AccessibilityUIElement::takeSelection()
{
    notImplemented();
}

void AccessibilityUIElement::addSelection()
{
    notImplemented();
}

void AccessibilityUIElement::removeSelection()
{
    notImplemented();
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement*)
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int, int, int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int, int, int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*)
{
    notImplemented();
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return 0;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker*)
{
    notImplemented();
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElement::setSelectedVisibleTextRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return false;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
    notImplemented();
}

void AccessibilityUIElement::scrollToGlobalPoint(int, int)
{
    notImplemented();
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int, int, int, int)
{
    notImplemented();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::characterAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::wordAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sentenceAtOffset(int)
{
    notImplemented();
    return nullptr;
}

} // namespace  WTF

#endif // HAVE(ACCESSIBILITY)
