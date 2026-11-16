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

RefPtr<AccessibilityUIElement> AccessibilityUIElementPlayStation::ariaActionsElementAtIndex(unsigned)
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfLinkedUIElements()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfDocumentLinks()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfChildren()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::allAttributes()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::stringAttributeValue(JSStringRef)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::currentStateValue() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::sortDirection() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::stringDescriptionOfAttributeValue(JSStringRef)
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::parameterizedAttributeNames()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::role()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::subrole()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::roleDescription()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::computedRoleString()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::title()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::description()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::orientation() const
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::isAtomicLiveRegion() const
{
    notImplemented();
    return false;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::liveRegionRelevant() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::liveRegionStatus() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::stringValue()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::language()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::helpText() const
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::valueDescription()
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::speakAs()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::isGrabbed() const
{
    notImplemented();
    return false;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::ariaDropEffects() const
{
    notImplemented();
    return nullptr;
}

int AccessibilityUIElementPlayStation::lineForIndex(int)
{
    notImplemented();
    return 0;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::rangeForLine(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::rangeForPosition(int, int)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::boundsForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::stringForRange(unsigned, unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributedStringForRange(unsigned, unsigned)
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

JSValueRef AccessibilityUIElementPlayStation::uiElementsForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool, unsigned)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::selectTextWithCriteria(JSContextRef, JSStringRef, JSValueRef, JSStringRef, JSStringRef)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfColumnHeaders()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfRowHeaders()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfColumns()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfRows()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfVisibleCells()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributesOfHeader()
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::rowIndexRange()
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::columnIndexRange()
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::selectedTextRange()
{
    notImplemented();
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementPlayStation::intersectionWithSelectionRange()
{
    notImplemented();
    return nullptr;
}

bool AccessibilityUIElementPlayStation::setSelectedTextRange(unsigned, unsigned)
{
    notImplemented();
    return false;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::textInputMarkedRange() const
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::accessibilityValue() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::url()
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::popupValue() const
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::stringForTextMarkerRange(AccessibilityTextMarkerRange*)
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::supportedActions() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::pathDescription() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::pathAsBounds() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::mathPostscriptsDescription() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::mathPrescriptsDescription() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::classList() const
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::characterAtOffset(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::wordAtOffset(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::lineAtOffset(int)
{
    notImplemented();
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::sentenceAtOffset(int)
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

RefPtr<OpaqueJSString> AccessibilityUIElementPlayStation::domIdentifier() const
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
