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
#include "AccessibilityUIElementWin.h"

#include <WebCore/NotImplemented.h>

namespace WTR {

Ref<AccessibilityUIElementWin> AccessibilityUIElementWin::create(PlatformUIElement element)
{
    return adoptRef(*new AccessibilityUIElementWin(element));
}

Ref<AccessibilityUIElementWin> AccessibilityUIElementWin::create(const AccessibilityUIElementWin& other)
{
    return adoptRef(*new AccessibilityUIElementWin(other));
}

AccessibilityUIElementWin::AccessibilityUIElementWin(PlatformUIElement element)
    : AccessibilityUIElement(element)
    , m_element(element)
{
    notImplemented();
}

AccessibilityUIElementWin::AccessibilityUIElementWin(const AccessibilityUIElementWin& other)
    : AccessibilityUIElement(other)
    , m_element(other.m_element)
{
    notImplemented();
}

AccessibilityUIElementWin::~AccessibilityUIElementWin()
{
    notImplemented();
}

bool AccessibilityUIElementWin::isValid() const
{
    return m_element;
}

bool AccessibilityUIElementWin::isEqual(AccessibilityUIElement*)
{
    notImplemented();
    return false;
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementWin::getChildren() const
{
    notImplemented();
    return { };
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementWin::getChildrenInRange(unsigned, unsigned) const
{
    notImplemented();
    return { };
}

unsigned AccessibilityUIElementWin::childrenCount()
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::elementAtPoint(int, int)
{
    notImplemented();
    return nullptr;
}

unsigned AccessibilityUIElementWin::indexOfChild(AccessibilityUIElement*)
{
    notImplemented();
    return 0;
}


RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::childAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::linkedUIElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::ariaOwnsElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::ariaFlowToElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::ariaControlsElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::disclosedRowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::rowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::selectedChildAtIndex(unsigned) const
{
    notImplemented();
    return nullptr;
}

unsigned AccessibilityUIElementWin::selectedChildrenCount() const
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::selectedRowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::titleUIElement()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::parentElement()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::disclosedByRow()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfLinkedUIElements()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfDocumentLinks()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfChildren()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::allAttributes()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::stringAttributeValue(JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::currentStateValue() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::sortDirection() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::stringDescriptionOfAttributeValue(JSStringRef)
{
    notImplemented();
    return nullptr;
}

double AccessibilityUIElementWin::numberAttributeValue(JSStringRef attribute)
{
    notImplemented();
    return 0;
}

JSValueRef AccessibilityUIElementWin::uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute)
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElementWin::rowHeaders(JSContextRef)
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElementWin::columnHeaders(JSContextRef)
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElementWin::selectedCells(JSContextRef)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::uiElementAttributeValue(JSStringRef attribute) const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::boolAttributeValue(JSStringRef attribute)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isAttributeSettable(JSStringRef attribute)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isAttributeSupported(JSStringRef attribute)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::parameterizedAttributeNames()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::role()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::subrole()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::roleDescription()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::computedRoleString()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::title()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::description()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::orientation() const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::isAtomicLiveRegion() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::liveRegionRelevant() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::liveRegionStatus() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::stringValue()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::language()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::helpText() const
{
    notImplemented();
    return nullptr;
}

double AccessibilityUIElementWin::pageX()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::pageY()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::x()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::y()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::width()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::height()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::clickPointX()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::clickPointY()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::intValue() const
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::minValue()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementWin::maxValue()
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::valueDescription()
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementWin::insertionPointLineNumber()
{
    notImplemented();
    return 0;
}

bool AccessibilityUIElementWin::isPressActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isIncrementActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isDecrementActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isBusy() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isEnabled()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isRequired() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isFocused() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isSelected() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isSelectedOptionActive() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isExpanded() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isChecked() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isIndeterminate() const
{
    notImplemented();
    return false;
}

int AccessibilityUIElementWin::hierarchicalLevel() const
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::speakAs()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::isGrabbed() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::ariaDropEffects() const
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementWin::lineForIndex(int)
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::rangeForLine(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::rangeForPosition(int, int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::boundsForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::stringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributedStringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    notImplemented();
    return false;
}

unsigned AccessibilityUIElementWin::uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool)
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::selectTextWithCriteria(JSContextRef, JSStringRef, JSValueRef, JSStringRef, JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfColumnHeaders()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfRowHeaders()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfColumns()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfRows()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfVisibleCells()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributesOfHeader()
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementWin::rowCount()
{
    notImplemented();
    return 0;
}

int AccessibilityUIElementWin::columnCount()
{
    notImplemented();
    return 0;
}

int AccessibilityUIElementWin::indexInTable()
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::rowIndexRange()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::columnIndexRange()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::cellForColumnAndRow(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::horizontalScrollbar() const
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::verticalScrollbar() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::selectedTextRange()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::intersectionWithSelectionRange()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::setSelectedTextRange(unsigned, unsigned)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::textInputMarkedRange() const
{
    notImplemented();
    return nullptr;
}

void AccessibilityUIElementWin::increment()
{
    notImplemented();
}

void AccessibilityUIElementWin::decrement()
{
    notImplemented();
}

void AccessibilityUIElementWin::showMenu()
{
    notImplemented();
}

void AccessibilityUIElementWin::press()
{
    notImplemented();
}

void AccessibilityUIElementWin::setSelectedChild(AccessibilityUIElement* element) const
{
    notImplemented();
}

void AccessibilityUIElementWin::setSelectedChildAtIndex(unsigned index) const
{
    notImplemented();
}

void AccessibilityUIElementWin::removeSelectionAtIndex(unsigned index) const
{
    notImplemented();
}

void AccessibilityUIElementWin::clearSelectedChildren() const
{
    notImplemented();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::accessibilityValue() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::url()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::addNotificationListener(JSContextRef, JSValueRef functionCallback)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::removeNotificationListener()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isFocusable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isSelectable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isMultiSelectable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isVisible() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isOffScreen() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isCollapsed() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isIgnored() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isSingleLine() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isMultiLine() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::hasPopup() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::popupValue() const
{
    notImplemented();
    return nullptr;
}

void AccessibilityUIElementWin::takeFocus()
{
    notImplemented();
}

void AccessibilityUIElementWin::takeSelection()
{
    notImplemented();
}

void AccessibilityUIElementWin::addSelection()
{
    notImplemented();
}

void AccessibilityUIElementWin::removeSelection()
{
    notImplemented();
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementWin::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementWin::textMarkerRangeForElement(AccessibilityUIElement*)
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementWin::textMarkerRangeLength(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::previousTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::nextTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::stringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementWin::textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::endTextMarkerForBounds(int, int, int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::startTextMarkerForBounds(int, int, int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::textMarkerForPoint(int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementWin::accessibilityElementForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*)
{
    notImplemented();
    return false;
}

int AccessibilityUIElementWin::indexForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return 0;
}

bool AccessibilityUIElementWin::isTextMarkerValid(AccessibilityTextMarker*)
{
    notImplemented();
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::textMarkerForIndex(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::startTextMarker()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementWin::endTextMarker()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return false;
}

void AccessibilityUIElementWin::scrollToMakeVisible()
{
    notImplemented();
}

void AccessibilityUIElementWin::scrollToGlobalPoint(int, int)
{
    notImplemented();
}

void AccessibilityUIElementWin::scrollToMakeVisibleWithSubFocus(int, int, int, int)
{
    notImplemented();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::supportedActions() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::pathDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::mathPostscriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::mathPrescriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::classList() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::characterAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::wordAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::lineAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::sentenceAtOffset(int)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::replaceTextInRange(JSStringRef, int, int)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::insertText(JSStringRef)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementWin::domIdentifier() const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementWin::isInsertion() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementWin::isDeletion() const
{
    notImplemented();
    return false;
}


bool AccessibilityUIElementWin::isFirstItemInSuggestion() const
{
    notImplemented();
    return false;
}


bool AccessibilityUIElementWin::isLastItemInSuggestion() const
{
    notImplemented();
    return false;
}

} // namespace WTR
