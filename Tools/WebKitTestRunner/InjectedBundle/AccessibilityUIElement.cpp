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

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityUIElement.h"

#include "JSAccessibilityUIElement.h"

namespace WTR {

Ref<AccessibilityUIElement> AccessibilityUIElement::create(PlatformUIElement uiElement)
{
    return adoptRef(*new AccessibilityUIElement(uiElement));
}
    
Ref<AccessibilityUIElement> AccessibilityUIElement::create(const AccessibilityUIElement& uiElement)
{
    return adoptRef(*new AccessibilityUIElement(uiElement));
}

JSClassRef AccessibilityUIElement::wrapperClass()
{
    return JSAccessibilityUIElement::accessibilityUIElementClass();
}
    
// Implementation

bool AccessibilityUIElement::isValid() const
{
    return m_element;            
}

// iOS specific methods
#if !PLATFORM(IOS_FAMILY)
JSRetainPtr<JSStringRef> AccessibilityUIElement::identifier() { return nullptr; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::traits() { return nullptr; }
int AccessibilityUIElement::elementTextPosition() { return 0; }
int AccessibilityUIElement::elementTextLength() { return 0; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForSelection() { return nullptr; }
JSValueRef AccessibilityUIElement::elementsForRange(unsigned, unsigned) { return nullptr; }
void AccessibilityUIElement::increaseTextSelection() { }
void AccessibilityUIElement::decreaseTextSelection() { }
RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedElement() { return nullptr; }
RefPtr<AccessibilityUIElement> AccessibilityUIElement::headerElementAtIndex(unsigned) { return nullptr; }
void AccessibilityUIElement::assistiveTechnologySimulatedFocus() { return; }
bool AccessibilityUIElement::scrollPageUp() { return false; }
bool AccessibilityUIElement::scrollPageDown() { return false; }
bool AccessibilityUIElement::scrollPageLeft() { return false; }
bool AccessibilityUIElement::scrollPageRight() { return false; }
bool AccessibilityUIElement::hasContainedByFieldsetTrait() { return false; }
RefPtr<AccessibilityUIElement> AccessibilityUIElement::fieldsetAncestorElement() { return nullptr; }
bool AccessibilityUIElement::isSearchField() const { return false; }
bool AccessibilityUIElement::isInDefinitionListDefinition() const { return false; }
bool AccessibilityUIElement::isInDefinitionListTerm() const { return false; }
bool AccessibilityUIElement::isTextArea() const { return false; }
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*) { return nullptr; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForElement() { return nullptr; }
bool AccessibilityUIElement::isInTableCell() const { return false; }
#endif
    
// Unsupported methods on various platforms. As they're implemented on other platforms this list should be modified.
#if PLATFORM(COCOA) || !HAVE(ACCESSIBILITY)
JSRetainPtr<JSStringRef> AccessibilityUIElement::characterAtOffset(int) { return nullptr; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::wordAtOffset(int) { return nullptr; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::lineAtOffset(int) { return nullptr; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::sentenceAtOffset(int) { return nullptr; }
#endif

#if !PLATFORM(MAC) || !HAVE(ACCESSIBILITY)
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::selectedTextMarkerRange() { return nullptr; }
void AccessibilityUIElement::resetSelectedTextMarkerRange() { }
void AccessibilityUIElement::setBoolAttributeValue(JSStringRef, bool) { }
void AccessibilityUIElement::setValue(JSStringRef) { }
JSValueRef AccessibilityUIElement::searchTextWithCriteria(JSContextRef, JSValueRef, JSStringRef, JSStringRef) { return nullptr; }
#endif

#if !PLATFORM(COCOA) || !HAVE(ACCESSIBILITY)
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*) { return nullptr; }
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::misspellingTextMarkerRange(AccessibilityTextMarkerRange*, bool) { return nullptr; }
void AccessibilityUIElement::dismiss() { }
#endif

} // namespace WTR
#endif // ENABLE(ACCESSIBILITY)
