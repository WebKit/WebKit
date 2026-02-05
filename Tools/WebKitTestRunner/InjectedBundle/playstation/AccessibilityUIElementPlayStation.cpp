/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "AccessibilityUIElementPlayStation.h"

#include <WebCore/NotImplemented.h>

namespace WTR {

Ref<AccessibilityUIElementPlayStation> AccessibilityUIElementPlayStation::create(PlatformUIElement element)
{
    return adoptRef(*new AccessibilityUIElementPlayStation(element));
}

Ref<AccessibilityUIElementPlayStation> AccessibilityUIElementPlayStation::create(const AccessibilityUIElementPlayStation& other)
{
    return adoptRef(*new AccessibilityUIElementPlayStation(other));
}

AccessibilityUIElementPlayStation::AccessibilityUIElementPlayStation(PlatformUIElement element)
    : AccessibilityUIElement(element)
    , m_element(element)
{
    notImplemented();
}

AccessibilityUIElementPlayStation::AccessibilityUIElementPlayStation(const AccessibilityUIElementPlayStation& other)
    : AccessibilityUIElement(other)
    , m_element(other.m_element)
{
    notImplemented();
}

AccessibilityUIElementPlayStation::~AccessibilityUIElementPlayStation()
{
    notImplemented();
}

bool AccessibilityUIElementPlayStation::isEqual(AccessibilityUIElement*)
{
    notImplemented();
    return false;
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementPlayStation::getChildren() const
{
    notImplemented();
    return { };
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementPlayStation::getChildrenInRange(unsigned, unsigned) const
{
    notImplemented();
    return { };
}

unsigned AccessibilityUIElementPlayStation::childrenCount()
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::elementAtPoint(int, int)
{
    notImplemented();
    return nullptr;
}

unsigned AccessibilityUIElementPlayStation::indexOfChild(AccessibilityUIElement*)
{
    notImplemented();
    return 0;
}


RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::childAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::linkedUIElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::ariaOwnsElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::ariaFlowToElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::ariaControlsElementAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::disclosedRowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::rowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::selectedChildAtIndex(unsigned) const
{
    notImplemented();
    return nullptr;
}

unsigned AccessibilityUIElementPlayStation::selectedChildrenCount() const
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::selectedRowAtIndex(unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::titleUIElement()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::parentElement()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::disclosedByRow()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfLinkedUIElements()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfDocumentLinks()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfChildren()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::allAttributes()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::stringAttributeValue(JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::currentStateValue() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::sortDirection() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::stringDescriptionOfAttributeValue(JSStringRef)
{
    notImplemented();
    return nullptr;
}

double AccessibilityUIElementPlayStation::numberAttributeValue(JSStringRef attribute)
{
    notImplemented();
    return 0;
}

JSValueRef AccessibilityUIElementPlayStation::uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute)
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElementPlayStation::rowHeaders(JSContextRef)
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElementPlayStation::columnHeaders(JSContextRef)
{
    notImplemented();
    return nullptr;
}

JSValueRef AccessibilityUIElementPlayStation::selectedCells(JSContextRef)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::uiElementAttributeValue(JSStringRef attribute) const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::boolAttributeValue(JSStringRef attribute)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isAttributeSettable(JSStringRef attribute)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isAttributeSupported(JSStringRef attribute)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::parameterizedAttributeNames()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::role()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::subrole()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::roleDescription()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::computedRoleString()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::title()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::description()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::orientation() const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::isAtomicLiveRegion() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::liveRegionRelevant() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::liveRegionStatus() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::stringValue()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::language()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::helpText() const
{
    notImplemented();
    return nullptr;
}

double AccessibilityUIElementPlayStation::pageX()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::pageY()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::x()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::y()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::width()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::height()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::clickPointX()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::clickPointY()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::intValue() const
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::minValue()
{
    notImplemented();
    return 0;
}

double AccessibilityUIElementPlayStation::maxValue()
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::valueDescription()
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementPlayStation::insertionPointLineNumber()
{
    notImplemented();
    return 0;
}

bool AccessibilityUIElementPlayStation::isPressActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isIncrementActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isDecrementActionSupported()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isBusy() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isEnabled()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isRequired() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isFocused() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isSelected() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isSelectedOptionActive() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isExpanded() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isChecked() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isIndeterminate() const
{
    notImplemented();
    return false;
}

int AccessibilityUIElementPlayStation::hierarchicalLevel() const
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::speakAs()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::isGrabbed() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::ariaDropEffects() const
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementPlayStation::lineForIndex(int)
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::rangeForLine(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::rangeForPosition(int, int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::boundsForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::stringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributedStringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    notImplemented();
    return false;
}

unsigned AccessibilityUIElementPlayStation::uiElementCountForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool)
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::uiElementForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::selectTextWithCriteria(JSContextRef, JSStringRef, JSValueRef, JSStringRef, JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfColumnHeaders()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfRowHeaders()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfColumns()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfRows()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfVisibleCells()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributesOfHeader()
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementPlayStation::rowCount()
{
    notImplemented();
    return 0;
}

int AccessibilityUIElementPlayStation::columnCount()
{
    notImplemented();
    return 0;
}

int AccessibilityUIElementPlayStation::indexInTable()
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::rowIndexRange()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::columnIndexRange()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::cellForColumnAndRow(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::horizontalScrollbar() const
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::verticalScrollbar() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::selectedTextRange()
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::intersectionWithSelectionRange()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::setSelectedTextRange(unsigned, unsigned)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::textInputMarkedRange() const
{
    notImplemented();
    return nullptr;
}

void AccessibilityUIElementPlayStation::increment()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::decrement()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::showMenu()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::press()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::setSelectedChild(AccessibilityUIElement* element) const
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::setSelectedChildAtIndex(unsigned index) const
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::removeSelectionAtIndex(unsigned index) const
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::clearSelectedChildren() const
{
    notImplemented();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::accessibilityValue() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::url()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::addNotificationListener(JSContextRef, JSValueRef functionCallback)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::removeNotificationListener()
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isFocusable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isSelectable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isMultiSelectable() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isVisible() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isOffScreen() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isCollapsed() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isIgnored() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isSingleLine() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isMultiLine() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::hasPopup() const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::popupValue() const
{
    notImplemented();
    return nullptr;
}

void AccessibilityUIElementPlayStation::takeFocus()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::takeSelection()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::addSelection()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::removeSelection()
{
    notImplemented();
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementPlayStation::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementPlayStation::textMarkerRangeForElement(AccessibilityUIElement*)
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementPlayStation::textMarkerRangeLength(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::previousTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::nextTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::stringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementPlayStation::textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::endTextMarkerForBounds(int, int, int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::startTextMarkerForBounds(int, int, int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::textMarkerForPoint(int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::accessibilityElementForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*)
{
    notImplemented();
    return false;
}

int AccessibilityUIElementPlayStation::indexForTextMarker(AccessibilityTextMarker*)
{
    notImplemented();
    return 0;
}

bool AccessibilityUIElementPlayStation::isTextMarkerValid(AccessibilityTextMarker*)
{
    notImplemented();
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::textMarkerForIndex(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::startTextMarker()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementPlayStation::endTextMarker()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return false;
}

void AccessibilityUIElementPlayStation::scrollToMakeVisible()
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::scrollToGlobalPoint(int, int)
{
    notImplemented();
}

void AccessibilityUIElementPlayStation::scrollToMakeVisibleWithSubFocus(int, int, int, int)
{
    notImplemented();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::supportedActions() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::pathDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::mathPostscriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::mathPrescriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::classList() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::characterAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::wordAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::lineAtOffset(int)
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::sentenceAtOffset(int)
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::replaceTextInRange(JSStringRef, int, int)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::insertText(JSStringRef)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementPlayStation::domIdentifier() const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::isInsertion() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isDeletion() const
{
    notImplemented();
    return false;
}


bool AccessibilityUIElementPlayStation::isFirstItemInSuggestion() const
{
    notImplemented();
    return false;
}


bool AccessibilityUIElementPlayStation::isLastItemInSuggestion() const
{
    notImplemented();
    return false;
}

bool AccessibilityUIElementPlayStation::isInNonNativeTextControl() const
{
    notImplemented();
    return false;
}

} // namespace WTR
