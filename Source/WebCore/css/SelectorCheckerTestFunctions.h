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

#ifndef SelectorCheckerTestFunctions_h
#define SelectorCheckerTestFunctions_h

#include "FocusController.h"
#include "HTMLAnchorElement.h"
#include "HTMLAreaElement.h"
#include "HTMLInputElement.h"
#include "HTMLOptionElement.h"
#include "RenderScrollbar.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include <wtf/Compiler.h>

#if ENABLE(VIDEO_TRACK)
#include "WebVTTElement.h"
#endif

namespace WebCore {

ALWAYS_INLINE bool isAutofilled(const Element* element)
{
    if (element->isFormControlElement()) {
        if (const HTMLInputElement* inputElement = element->toInputElement())
            return inputElement->isAutofilled();
    }
    return false;
}

ALWAYS_INLINE bool isDefaultButtonForForm(const Element* element)
{
    return element->isDefaultButtonForForm();
}

ALWAYS_INLINE bool isDisabled(const Element* element)
{
    if (element->isFormControlElement() || isHTMLOptionElement(element) || isHTMLOptGroupElement(element))
        return element->isDisabledFormControl();
    return false;
}

ALWAYS_INLINE bool isEnabled(const Element* element)
{
    bool isEnabled = false;
    if (element->isFormControlElement() || isHTMLOptionElement(element) || isHTMLOptGroupElement(element))
        isEnabled = !element->isDisabledFormControl();
    else if (element->isLink())
        isEnabled = isHTMLAnchorElement(element) || isHTMLAreaElement(element);
    return isEnabled;
}

ALWAYS_INLINE bool isMediaDocument(Element* element)
{
    return element->document().isMediaDocument();
}

ALWAYS_INLINE bool isChecked(Element* element)
{
    // Even though WinIE allows checked and indeterminate to co-exist, the CSS selector spec says that
    // you can't be both checked and indeterminate. We will behave like WinIE behind the scenes and just
    // obey the CSS spec here in the test for matching the pseudo.
    const HTMLInputElement* inputElement = element->toInputElement();
    if (inputElement && inputElement->shouldAppearChecked() && !inputElement->shouldAppearIndeterminate())
        return true;
    if (isHTMLOptionElement(element) && toHTMLOptionElement(element)->selected())
        return true;
    return false;
}

ALWAYS_INLINE bool isInRange(Element* element)
{
    element->document().setContainsValidityStyleRules();
    return element->isInRange();
}

ALWAYS_INLINE bool isOutOfRange(Element* element)
{
    element->document().setContainsValidityStyleRules();
    return element->isOutOfRange();
}

ALWAYS_INLINE bool isInvalid(const Element* element)
{
    element->document().setContainsValidityStyleRules();
    return element->willValidate() && !element->isValidFormControlElement();
}

ALWAYS_INLINE bool isOptionalFormControl(const Element* element)
{
    return element->isOptionalFormControl();
}

ALWAYS_INLINE bool isRequiredFormControl(const Element* element)
{
    return element->isRequiredFormControl();
}

ALWAYS_INLINE bool isValid(const Element* element)
{
    element->document().setContainsValidityStyleRules();
    return element->willValidate() && element->isValidFormControlElement();
}

ALWAYS_INLINE bool isWindowInactive(const Element* element)
{
    return !element->document().page()->focusController().isActive();
}
    
inline bool matchesLangPseudoClass(const Element* element, AtomicStringImpl* filter)
{
    AtomicString value;
#if ENABLE(VIDEO_TRACK)
    if (element->isWebVTTElement())
        value = toWebVTTElement(element)->language();
    else
#endif
        value = element->computeInheritedLanguage();

    if (value.isNull())
        return false;

    if (value.impl() == filter)
        return true;

    if (value.impl()->startsWith(filter, false)) {
        if (value.length() == filter->length())
            return true;
        return value[filter->length()] == '-';
    }
    return false;
}

ALWAYS_INLINE bool matchesReadOnlyPseudoClass(const Element* element)
{
    return !element->matchesReadWritePseudoClass();
}

ALWAYS_INLINE bool matchesReadWritePseudoClass(const Element* element)
{
    return element->matchesReadWritePseudoClass();
}

ALWAYS_INLINE bool shouldAppearIndeterminate(const Element* element)
{
    return element->shouldAppearIndeterminate();
}

ALWAYS_INLINE bool scrollbarMatchesEnabledPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbar && context.scrollbar->enabled();
}

ALWAYS_INLINE bool scrollbarMatchesDisabledPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbar && !context.scrollbar->enabled();
}

ALWAYS_INLINE bool scrollbarMatchesHoverPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbar)
        return false;
    ScrollbarPart hoveredPart = context.scrollbar->hoveredPart();
    if (context.scrollbarPart == ScrollbarBGPart)
        return hoveredPart != NoPart;
    if (context.scrollbarPart == TrackBGPart)
        return hoveredPart == BackTrackPart || hoveredPart == ForwardTrackPart || hoveredPart == ThumbPart;
    return context.scrollbarPart == hoveredPart;
}

ALWAYS_INLINE bool scrollbarMatchesActivePseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbar)
        return false;
    ScrollbarPart pressedPart = context.scrollbar->pressedPart();
    if (context.scrollbarPart == ScrollbarBGPart)
        return pressedPart != NoPart;
    if (context.scrollbarPart == TrackBGPart)
        return pressedPart == BackTrackPart || pressedPart == ForwardTrackPart || pressedPart == ThumbPart;
    return context.scrollbarPart == pressedPart;
}

ALWAYS_INLINE bool scrollbarMatchesHorizontalPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbar && context.scrollbar->orientation() == HorizontalScrollbar;
}

ALWAYS_INLINE bool scrollbarMatchesVerticalPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbar && context.scrollbar->orientation() == VerticalScrollbar;
}

ALWAYS_INLINE bool scrollbarMatchesDecrementPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarPart == BackButtonStartPart || context.scrollbarPart == BackButtonEndPart || context.scrollbarPart == BackTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesIncrementPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarPart == ForwardButtonStartPart || context.scrollbarPart == ForwardButtonEndPart || context.scrollbarPart == ForwardTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesStartPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarPart == BackButtonStartPart || context.scrollbarPart == ForwardButtonStartPart || context.scrollbarPart == BackTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesEndPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarPart == BackButtonEndPart || context.scrollbarPart == ForwardButtonEndPart || context.scrollbarPart == ForwardTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesDoubleButtonPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbar)
        return false;
    ScrollbarButtonsPlacement buttonsPlacement = context.scrollbar->theme()->buttonsPlacement();
    if (context.scrollbarPart == BackButtonStartPart || context.scrollbarPart == ForwardButtonStartPart || context.scrollbarPart == BackTrackPart)
        return buttonsPlacement == ScrollbarButtonsDoubleStart || buttonsPlacement == ScrollbarButtonsDoubleBoth;
    if (context.scrollbarPart == BackButtonEndPart || context.scrollbarPart == ForwardButtonEndPart || context.scrollbarPart == ForwardTrackPart)
        return buttonsPlacement == ScrollbarButtonsDoubleEnd || buttonsPlacement == ScrollbarButtonsDoubleBoth;
    return false;
}

ALWAYS_INLINE bool scrollbarMatchesSingleButtonPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbar)
        return false;
    ScrollbarButtonsPlacement buttonsPlacement = context.scrollbar->theme()->buttonsPlacement();
    if (context.scrollbarPart == BackButtonStartPart || context.scrollbarPart == ForwardButtonEndPart || context.scrollbarPart == BackTrackPart || context.scrollbarPart == ForwardTrackPart)
        return buttonsPlacement == ScrollbarButtonsSingle;
    return false;
}

ALWAYS_INLINE bool scrollbarMatchesNoButtonPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbar)
        return false;
    ScrollbarButtonsPlacement buttonsPlacement = context.scrollbar->theme()->buttonsPlacement();
    if (context.scrollbarPart == BackTrackPart)
        return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleEnd;
    if (context.scrollbarPart == ForwardTrackPart)
        return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleStart;
    return false;
}

ALWAYS_INLINE bool scrollbarMatchesCornerPresentPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbar && context.scrollbar->scrollableArea()->isScrollCornerVisible();
}

#if ENABLE(FULLSCREEN_API)
ALWAYS_INLINE bool matchesFullScreenPseudoClass(const Element* element)
{
    // While a Document is in the fullscreen state, and the document's current fullscreen
    // element is an element in the document, the 'full-screen' pseudoclass applies to
    // that element. Also, an <iframe>, <object> or <embed> element whose child browsing
    // context's Document is in the fullscreen state has the 'full-screen' pseudoclass applied.
    if (element->isFrameElementBase() && element->containsFullScreenElement())
        return true;
    if (!element->document().webkitIsFullScreen())
        return false;
    return element == element->document().webkitCurrentFullScreenElement();
}

ALWAYS_INLINE bool matchesFullScreenAnimatingFullScreenTransitionPseudoClass(const Element* element)
{
    if (element != element->document().webkitCurrentFullScreenElement())
        return false;
    return element->document().isAnimatingFullScreen();
}

ALWAYS_INLINE bool matchesFullScreenAncestorPseudoClass(const Element* element)
{
    return element->containsFullScreenElement();
}

ALWAYS_INLINE bool matchesFullScreenDocumentPseudoClass(const Element* element)
{
    // While a Document is in the fullscreen state, the 'full-screen-document' pseudoclass applies
    // to all elements of that Document.
    if (!element->document().webkitIsFullScreen())
        return false;
    return true;
}
#endif

#if ENABLE(VIDEO_TRACK)
ALWAYS_INLINE bool matchesFutureCuePseudoClass(const Element* element)
{
    return (element->isWebVTTElement() && !toWebVTTElement(element)->isPastNode());
}

ALWAYS_INLINE bool matchesPastCuePseudoClass(const Element* element)
{
    return (element->isWebVTTElement() && toWebVTTElement(element)->isPastNode());
}
#endif

} // namespace WebCore

#endif // SelectorCheckerTestFunctions_h
