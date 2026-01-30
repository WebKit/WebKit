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

#include "config.h"
#include "AccessibilityUIElement.h"

#include "JSAccessibilityUIElement.h"

#if PLATFORM(MAC)
#include "mac/AccessibilityUIElementClientMac.h"
#include "mac/AccessibilityUIElementMac.h"
#elif PLATFORM(IOS_FAMILY)
#include "ios/AccessibilityUIElementIOS.h"
#elif USE(ATSPI)
#include "atspi/AccessibilityUIElementAtspi.h"
#elif PLATFORM(WIN)
#include "win/AccessibilityUIElementWin.h"
#elif PLATFORM(PLAYSTATION)
#include "playstation/AccessibilityUIElementPlayStation.h"
#endif

namespace WTR {

// Static controller reference used by subclasses
RefPtr<AccessibilityController> AccessibilityUIElement::s_controller;

// Factory method - dispatches to platform-specific create()
Ref<AccessibilityUIElement> AccessibilityUIElement::create(PlatformUIElement uiElement)
{
    RELEASE_ASSERT(uiElement);
#if PLATFORM(MAC)
    return AccessibilityUIElementMac::create(uiElement);
#elif PLATFORM(IOS_FAMILY)
    return AccessibilityUIElementIOS::create(uiElement);
#elif USE(ATSPI)
    return AccessibilityUIElementAtspi::create(uiElement);
#elif PLATFORM(WIN)
    return AccessibilityUIElementWin::create(uiElement);
#elif PLATFORM(PLAYSTATION)
    return AccessibilityUIElementPlayStation::create(uiElement);
#else
    return adoptRef(*new AccessibilityUIElement(uiElement));
#endif
}

Ref<AccessibilityUIElement> AccessibilityUIElement::create(const AccessibilityUIElement& uiElement)
{
#if PLATFORM(MAC)
    return AccessibilityUIElementMac::create(static_cast<const AccessibilityUIElementMac&>(uiElement));
#elif PLATFORM(IOS_FAMILY)
    return AccessibilityUIElementIOS::create(static_cast<const AccessibilityUIElementIOS&>(uiElement));
#elif USE(ATSPI)
    return AccessibilityUIElementAtspi::create(static_cast<const AccessibilityUIElementAtspi&>(uiElement));
#elif PLATFORM(WIN)
    return AccessibilityUIElementWin::create(static_cast<const AccessibilityUIElementWin&>(uiElement));
#elif PLATFORM(PLAYSTATION)
    return AccessibilityUIElementPlayStation::create(static_cast<const AccessibilityUIElementPlayStation&>(uiElement));
#else
    return adoptRef(*new AccessibilityUIElement(uiElement));
#endif
}

JSClassRef AccessibilityUIElement::wrapperClass()
{
    return JSAccessibilityUIElement::accessibilityUIElementClass();
}

// Base class constructors are empty - all platforms store m_element in their subclasses
AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement)
{
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement&)
{
}

AccessibilityUIElement::~AccessibilityUIElement() = default;

// Stub implementations for methods that are virtual and may be overridden by platform-specific subclasses
RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::activeElement() const
{
    return nullptr;
}

bool AccessibilityUIElement::addNotificationListener(OpaqueJSContext const*, OpaqueJSValue const*)
{
    return false;
}

void AccessibilityUIElement::addSelection()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDescribedByElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDetailsElementAtIndex(unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaErrorMessageElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaLabelledByElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned)
{
    return nullptr;
}

void AccessibilityUIElement::assistiveTechnologySimulatedFocus()
{
    return;
}

void AccessibilityUIElement::asyncDecrement()
{
}

void AccessibilityUIElement::asyncIncrement()
{
}

void AccessibilityUIElement::attributeValueAsync(OpaqueJSContext const*, OpaqueJSString*, OpaqueJSValue const*)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForElement()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned, unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(WTR::AccessibilityTextMarkerRange*)
{
    return nullptr;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(OpaqueJSString*, WTR::AccessibilityTextMarkerRange*)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithDidSpellCheck(WTR::AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(WTR::AccessibilityTextMarkerRange*, bool)
{
    return nullptr;
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    return nullptr;
}

bool AccessibilityUIElement::boolAttributeValue(OpaqueJSString*)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned, unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRangeWithPagePosition(unsigned, unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::brailleLabel() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::brailleRoleDescription() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned, unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::characterAtOffset(int)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndexWithRemoteElement(unsigned)
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::children(OpaqueJSContext const*)
{
    return nullptr;
}

unsigned AccessibilityUIElement::childrenCount()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    return nullptr;
}

void AccessibilityUIElement::clearSelectedChildren() const
{
}

double AccessibilityUIElement::clickPointX()
{
    return 0;
}

double AccessibilityUIElement::clickPointY()
{
    return 0;
}

int AccessibilityUIElement::columnCount()
{
    return 0;
}

JSValueRef AccessibilityUIElement::columnHeaders(OpaqueJSContext const*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::columns(OpaqueJSContext const*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::controllerElementAtIndex(unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::currentStateValue() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::customContent() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::dateTimeValue() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::dateValue()
{
    return nullptr;
}

void AccessibilityUIElement::decreaseTextSelection()
{
}

void AccessibilityUIElement::decrement()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::descriptionForElementAtIndex(unsigned)
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::detailsElements(OpaqueJSContext const*)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::detailsForElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned)
{
    return nullptr;
}

bool AccessibilityUIElement::dismiss()
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::domIdentifier() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::editableAncestor()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int, int)
{
    return nullptr;
}

void AccessibilityUIElement::elementAtPointResolvingRemoteFrame(OpaqueJSContext const*, int, int, OpaqueJSValue const*)
{
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPointWithRemoteElement(int, int)
{
    return nullptr;
}

int AccessibilityUIElement::elementTextLength()
{
    return 0;
}

int AccessibilityUIElement::elementTextPosition()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::embeddedImageDescription() const
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int, int, int, int)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(WTR::AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::errorMessageElements(OpaqueJSContext const*)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::errorMessageForElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::fieldsetAncestorElement()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::flowFromElementAtIndex(unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::focusableAncestor()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::focusedElement() const
{
    return nullptr;
}

bool AccessibilityUIElement::hasMenuItemTrait()
{
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    return false;
}

bool AccessibilityUIElement::hasTabBarTrait()
{
    return false;
}

bool AccessibilityUIElement::hasTextEntryTrait()
{
    return false;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::headerElementAtIndex(unsigned)
{
    return nullptr;
}

double AccessibilityUIElement::height()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    return nullptr;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::highestEditableAncestor()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::identifier()
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::imageOverlayElements(OpaqueJSContext const*)
{
    return nullptr;
}

void AccessibilityUIElement::increaseTextSelection()
{
}

void AccessibilityUIElement::increment()
{
}

int AccessibilityUIElement::indexForTextMarker(WTR::AccessibilityTextMarker*)
{
    return 0;
}

int AccessibilityUIElement::indexInTable()
{
    return 0;
}

unsigned AccessibilityUIElement::indexOfChild(WTR::AccessibilityUIElement*)
{
    return 0;
}

bool AccessibilityUIElement::insertText(OpaqueJSString*)
{
    return false;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return 0;
}

double AccessibilityUIElement::intValue() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::intersectionWithSelectionRange()
{
    return nullptr;
}

bool AccessibilityUIElement::isAtomicLiveRegion() const
{
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(OpaqueJSString*)
{
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(OpaqueJSString*)
{
    return false;
}

bool AccessibilityUIElement::isBusy() const
{
    return false;
}

bool AccessibilityUIElement::isChecked() const
{
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isDeletion() const
{
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    return false;
}

bool AccessibilityUIElement::isEqual(WTR::AccessibilityUIElement*)
{
    return false;
}

bool AccessibilityUIElement::isExpanded() const
{
    return false;
}

bool AccessibilityUIElement::isFirstItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElement::isFocusable() const
{
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    return false;
}

bool AccessibilityUIElement::isGrabbed() const
{
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    return false;
}

bool AccessibilityUIElement::isInCell() const
{
    return false;
}

bool AccessibilityUIElement::isInDescriptionListDetail() const
{
    return false;
}

bool AccessibilityUIElement::isInDescriptionListTerm() const
{
    return false;
}

bool AccessibilityUIElement::isInLandmark() const
{
    return false;
}

bool AccessibilityUIElement::isInList() const
{
    return false;
}

bool AccessibilityUIElement::isInTable() const
{
    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    return false;
}

bool AccessibilityUIElement::isInsertion() const
{
    return false;
}

bool AccessibilityUIElement::isLastItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElement::isMarkAnnotation() const
{
    return false;
}

bool AccessibilityUIElement::isMultiLine() const
{
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    return false;
}

bool AccessibilityUIElement::isOnScreen() const
{
    return false;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isRemoteFrame() const
{
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    return false;
}

bool AccessibilityUIElement::isSearchField() const
{
    return false;
}

bool AccessibilityUIElement::isSelectable() const
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

bool AccessibilityUIElement::isSingleLine() const
{
    return false;
}

bool AccessibilityUIElement::isSwitch() const
{
    return false;
}

bool AccessibilityUIElement::isTextArea() const
{
    return false;
}

bool AccessibilityUIElement::isTextMarkerNull(WTR::AccessibilityTextMarker*)
{
    return false;
}

bool AccessibilityUIElement::isTextMarkerRangeValid(WTR::AccessibilityTextMarkerRange*)
{
    return false;
}

bool AccessibilityUIElement::isTextMarkerValid(WTR::AccessibilityTextMarker*)
{
    return false;
}

bool AccessibilityUIElement::isValid() const
{
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    return false;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::labelForElementAtIndex(unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::leftLineTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineAtOffset(int)
{
    return nullptr;
}

int AccessibilityUIElement::lineForIndex(int)
{
    return 0;
}

int AccessibilityUIElement::lineIndexForTextMarker(WTR::AccessibilityTextMarker*) const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineRectsAndText() const
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedElement()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionRelevant() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionStatus() const
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

JSValueRef AccessibilityUIElement::mathRootRadicand(OpaqueJSContext const*)
{
    return nullptr;
}

double AccessibilityUIElement::maxValue()
{
    return 0;
}

double AccessibilityUIElement::minValue()
{
    return 0;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::misspellingTextMarkerRange(WTR::AccessibilityTextMarkerRange*, bool)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextLineEndTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

double AccessibilityUIElement::numberAttributeValue(OpaqueJSString*)
{
    return 0;
}

unsigned AccessibilityUIElement::numberOfCharacters() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ownerElementAtIndex(unsigned)
{
    return nullptr;
}

double AccessibilityUIElement::pageX()
{
    return 0;
}

double AccessibilityUIElement::pageY()
{
    return 0;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::performTextOperation(OpaqueJSContext const*, OpaqueJSString*, OpaqueJSValue const*, OpaqueJSValue const*, bool)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    return nullptr;
}

void AccessibilityUIElement::press()
{
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousLineStartTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int, int)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rectsForTextMarkerRange(WTR::AccessibilityTextMarkerRange*, OpaqueJSString*)
{
    return nullptr;
}

bool AccessibilityUIElement::removeNotificationListener()
{
    return false;
}

void AccessibilityUIElement::removeSelection()
{
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned) const
{
}

bool AccessibilityUIElement::replaceTextInRange(OpaqueJSString*, int, int)
{
    return false;
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::rightLineTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned)
{
    return nullptr;
}

int AccessibilityUIElement::rowCount()
{
    return 0;
}

JSValueRef AccessibilityUIElement::rowHeaders(OpaqueJSContext const*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    return nullptr;
}

bool AccessibilityUIElement::scrollPageDown()
{
    return false;
}

bool AccessibilityUIElement::scrollPageLeft()
{
    return false;
}

bool AccessibilityUIElement::scrollPageRight()
{
    return false;
}

bool AccessibilityUIElement::scrollPageUp()
{
    return false;
}

void AccessibilityUIElement::scrollToGlobalPoint(int, int)
{
}

void AccessibilityUIElement::scrollToMakeVisible()
{
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int, int, int, int)
{
}

JSValueRef AccessibilityUIElement::searchTextWithCriteria(OpaqueJSContext const*, OpaqueJSValue const*, OpaqueJSString*, OpaqueJSString*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(OpaqueJSContext const*, OpaqueJSString*, OpaqueJSValue const*, OpaqueJSString*, OpaqueJSString*)
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::selectedCells(OpaqueJSContext const*)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned) const
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::selectedChildren(OpaqueJSContext const*)
{
    return nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedText()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::selectedTextMarkerRange()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sentenceAtOffset(int)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

void AccessibilityUIElement::setBoolAttributeValue(OpaqueJSString*, bool)
{
}

void AccessibilityUIElement::setSelectedChild(WTR::AccessibilityUIElement*) const
{
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned) const
{
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(WTR::AccessibilityTextMarkerRange*)
{
    return false;
}

bool AccessibilityUIElement::setSelectedTextRange(unsigned, unsigned)
{
    return false;
}

void AccessibilityUIElement::setValue(OpaqueJSString*)
{
}

void AccessibilityUIElement::showMenu()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sortDirection() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int, int, int, int)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(WTR::AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(OpaqueJSString*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringDescriptionOfAttributeValue(OpaqueJSString*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned, unsigned)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForSelection()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(WTR::AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::styleTextMarkerRangeForTextMarker(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    return nullptr;
}

bool AccessibilityUIElement::supportsExpanded() const
{
    return false;
}

void AccessibilityUIElement::syncPress()
{
}

void AccessibilityUIElement::takeFocus()
{
}

void AccessibilityUIElement::takeSelection()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::textInputMarkedRange() const
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textInputMarkedTextMarkerRange() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::textMarkerDebugDescription(WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int, int)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::textMarkerRangeDebugDescription(WTR::AccessibilityTextMarkerRange*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(WTR::AccessibilityUIElement*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForLine(long)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(WTR::AccessibilityTextMarker*, WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForRange(unsigned, unsigned)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForSearchPredicate(OpaqueJSContext const*, WTR::AccessibilityTextMarkerRange*, bool, OpaqueJSValue const*, OpaqueJSString*, bool, bool)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForUnorderedMarkers(WTR::AccessibilityTextMarker*, WTR::AccessibilityTextMarker*)
{
    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(WTR::AccessibilityTextMarkerRange*)
{
    return 0;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::traits()
{
    return nullptr;
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(OpaqueJSContext const*, OpaqueJSString*)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(OpaqueJSString*) const
{
    return nullptr;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(OpaqueJSContext const*, WTR::AccessibilityUIElement*, bool, OpaqueJSValue const*, OpaqueJSString*, bool, bool)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(OpaqueJSContext const*, WTR::AccessibilityUIElement*, bool, OpaqueJSValue const*, OpaqueJSString*, bool, bool)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    return nullptr;
}

double AccessibilityUIElement::width()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::wordAtOffset(int)
{
    return nullptr;
}

double AccessibilityUIElement::x()
{
    return 0;
}

double AccessibilityUIElement::y()
{
    return 0;
}

} // namespace WTR
