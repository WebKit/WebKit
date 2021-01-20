/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Dhi Aurrahman <diorahman@rockybars.com>
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

#pragma once

#include "FocusController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FullscreenManager.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLOptionElement.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "SelectorChecker.h"
#include "ShadowRoot.h"
#include <wtf/Compiler.h>

#if ENABLE(ATTACHMENT_ELEMENT)
#include "HTMLAttachmentElement.h"
#endif

#if ENABLE(VIDEO)
#include "WebVTTElement.h"
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
#include "HTMLVideoElement.h"
#endif

namespace WebCore {

ALWAYS_INLINE bool isAutofilled(const Element& element)
{
    return is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isAutoFilled();
}

ALWAYS_INLINE bool isAutofilledStrongPassword(const Element& element)
{
    return is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isAutoFilled() && downcast<HTMLInputElement>(element).hasAutoFillStrongPasswordButton();
}

ALWAYS_INLINE bool isAutofilledStrongPasswordViewable(const Element& element)
{
    return is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isAutoFilledAndViewable();
}

ALWAYS_INLINE bool matchesDefaultPseudoClass(const Element& element)
{
    return element.matchesDefaultPseudoClass();
}

// https://html.spec.whatwg.org/multipage/scripting.html#selector-disabled
ALWAYS_INLINE bool matchesDisabledPseudoClass(const Element& element)
{
    return is<HTMLElement>(element) && downcast<HTMLElement>(element).isActuallyDisabled();
}

// https://html.spec.whatwg.org/multipage/scripting.html#selector-enabled
ALWAYS_INLINE bool matchesEnabledPseudoClass(const Element& element)
{
    return is<HTMLElement>(element) && downcast<HTMLElement>(element).canBeActuallyDisabled() && !element.isDisabledFormControl();
}

ALWAYS_INLINE bool isDefinedElement(const Element& element)
{
    return !element.isUndefinedCustomElement();
}

ALWAYS_INLINE bool isMediaDocument(const Element& element)
{
    return element.document().isMediaDocument();
}

ALWAYS_INLINE bool isChecked(const Element& element)
{
    // Even though WinIE allows checked and indeterminate to co-exist, the CSS selector spec says that
    // you can't be both checked and indeterminate. We will behave like WinIE behind the scenes and just
    // obey the CSS spec here in the test for matching the pseudo.
    if (is<HTMLInputElement>(element)) {
        auto& inputElement = downcast<HTMLInputElement>(element);
        return inputElement.shouldAppearChecked() && !inputElement.shouldAppearIndeterminate();
    }
    if (is<HTMLOptionElement>(element))
        return const_cast<HTMLOptionElement&>(downcast<HTMLOptionElement>(element)).selected();

    return false;
}

ALWAYS_INLINE bool isInRange(const Element& element)
{
    return element.isInRange();
}

ALWAYS_INLINE bool isOutOfRange(const Element& element)
{
    return element.isOutOfRange();
}

ALWAYS_INLINE bool isInvalid(const Element& element)
{
    return element.matchesInvalidPseudoClass();
}

ALWAYS_INLINE bool isOptionalFormControl(const Element& element)
{
    return element.isOptionalFormControl();
}

ALWAYS_INLINE bool isRequiredFormControl(const Element& element)
{
    return element.isRequiredFormControl();
}

ALWAYS_INLINE bool isValid(const Element& element)
{
    return element.matchesValidPseudoClass();
}

ALWAYS_INLINE bool isWindowInactive(const Element& element)
{
    auto* page = element.document().page();
    if (!page)
        return false;
    return !page->focusController().isActive();
}

#if ENABLE(ATTACHMENT_ELEMENT)
ALWAYS_INLINE bool hasAttachment(const Element& element)
{
    return is<HTMLImageElement>(element) && downcast<HTMLImageElement>(element).attachmentElement();
}
#endif

ALWAYS_INLINE bool containslanguageSubtagMatchingRange(StringView language, StringView range, unsigned languageLength, unsigned& position)
{
    unsigned languageSubtagsStartIndex = position;
    unsigned languageSubtagsEndIndex = languageLength;
    bool isAsteriskRange = range == "*";
    do {
        if (languageSubtagsStartIndex > 0)
            languageSubtagsStartIndex += 1;
        
        languageSubtagsEndIndex = std::min<unsigned>(language.find('-', languageSubtagsStartIndex), languageLength);

        if (languageSubtagsStartIndex > languageSubtagsEndIndex)
            return false;

        StringView languageSubtag = language.substring(languageSubtagsStartIndex, languageSubtagsEndIndex - languageSubtagsStartIndex);
        bool isEqual = equalIgnoringASCIICase(range, languageSubtag);
        if (!isAsteriskRange) {
            if ((!isEqual && !languageSubtagsStartIndex) || (languageSubtag.length() == 1 && languageSubtagsStartIndex > 0))
                return false;
        }
        languageSubtagsStartIndex = languageSubtagsEndIndex;
        if (isEqual || isAsteriskRange) {
            position = languageSubtagsStartIndex;
            return true;
        }

    } while (languageSubtagsStartIndex < languageLength);
    return false;
}

ALWAYS_INLINE bool matchesLangPseudoClass(const Element& element, const Vector<AtomString>& argumentList)
{
    AtomString language;
#if ENABLE(VIDEO)
    if (is<WebVTTElement>(element))
        language = downcast<WebVTTElement>(element).language();
    else
#endif
        language = element.computeInheritedLanguage();

    if (language.isEmpty())
        return false;

    // Implement basic and extended filterings of given language tags
    // as specified in www.ietf.org/rfc/rfc4647.txt.
    StringView languageStringView = language.string();
    unsigned languageLength = language.length();
    for (const AtomString& range : argumentList) {
        if (range.isEmpty())
            continue;

        if (range == "*")
            return true;

        StringView rangeStringView = range.string();
        if (equalIgnoringASCIICase(languageStringView, rangeStringView) && !languageStringView.contains('-'))
            return true;
        
        unsigned rangeLength = rangeStringView.length();
        unsigned rangeSubtagsStartIndex = 0;
        unsigned rangeSubtagsEndIndex = rangeLength;
        unsigned lastMatchedLanguageSubtagIndex = 0;

        bool matchedRange = true;
        do {
            if (rangeSubtagsStartIndex > 0)
                rangeSubtagsStartIndex += 1;
            if (rangeSubtagsStartIndex > languageLength)
                return false;
            rangeSubtagsEndIndex = std::min<unsigned>(rangeStringView.find('-', rangeSubtagsStartIndex), rangeLength);
            StringView rangeSubtag = rangeStringView.substring(rangeSubtagsStartIndex, rangeSubtagsEndIndex - rangeSubtagsStartIndex);
            if (!containslanguageSubtagMatchingRange(languageStringView, rangeSubtag, languageLength, lastMatchedLanguageSubtagIndex)) {
                matchedRange = false;
                break;
            }
            rangeSubtagsStartIndex = rangeSubtagsEndIndex;
        } while (rangeSubtagsStartIndex < rangeLength);
        if (matchedRange)
            return true;
    }
    return false;
}

ALWAYS_INLINE bool matchesReadOnlyPseudoClass(const Element& element)
{
    return !element.matchesReadWritePseudoClass();
}

ALWAYS_INLINE bool matchesReadWritePseudoClass(const Element& element)
{
    return element.matchesReadWritePseudoClass();
}

ALWAYS_INLINE bool matchesIndeterminatePseudoClass(const Element& element)
{
    return element.matchesIndeterminatePseudoClass();
}

ALWAYS_INLINE bool scrollbarMatchesEnabledPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarState && context.scrollbarState->enabled;
}

ALWAYS_INLINE bool scrollbarMatchesDisabledPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarState && !context.scrollbarState->enabled;
}

ALWAYS_INLINE bool scrollbarMatchesHoverPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    auto hoveredPart = context.scrollbarState->hoveredPart;
    if (scrollbarPart == ScrollbarBGPart)
        return hoveredPart != NoPart;
    if (scrollbarPart == TrackBGPart)
        return hoveredPart == BackTrackPart || hoveredPart == ForwardTrackPart || hoveredPart == ThumbPart;
    return scrollbarPart == hoveredPart;
}

ALWAYS_INLINE bool scrollbarMatchesActivePseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    auto pressedPart = context.scrollbarState->pressedPart;
    if (scrollbarPart == ScrollbarBGPart)
        return pressedPart != NoPart;
    if (scrollbarPart == TrackBGPart)
        return pressedPart == BackTrackPart || pressedPart == ForwardTrackPart || pressedPart == ThumbPart;
    return scrollbarPart == pressedPart;
}

ALWAYS_INLINE bool scrollbarMatchesHorizontalPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarState && context.scrollbarState->orientation == HorizontalScrollbar;
}

ALWAYS_INLINE bool scrollbarMatchesVerticalPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarState && context.scrollbarState->orientation == VerticalScrollbar;
}

ALWAYS_INLINE bool scrollbarMatchesDecrementPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    return scrollbarPart == BackButtonStartPart || scrollbarPart == BackButtonEndPart || scrollbarPart == BackTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesIncrementPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    return scrollbarPart == ForwardButtonStartPart || scrollbarPart == ForwardButtonEndPart || scrollbarPart == ForwardTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesStartPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    return scrollbarPart == BackButtonStartPart || scrollbarPart == ForwardButtonStartPart || scrollbarPart == BackTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesEndPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    return scrollbarPart == BackButtonEndPart || scrollbarPart == ForwardButtonEndPart || scrollbarPart == ForwardTrackPart;
}

ALWAYS_INLINE bool scrollbarMatchesDoubleButtonPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    auto buttonsPlacement = context.scrollbarState->buttonsPlacement;
    if (scrollbarPart == BackButtonStartPart || scrollbarPart == ForwardButtonStartPart || scrollbarPart == BackTrackPart)
        return buttonsPlacement == ScrollbarButtonsDoubleStart || buttonsPlacement == ScrollbarButtonsDoubleBoth;
    if (scrollbarPart == BackButtonEndPart || scrollbarPart == ForwardButtonEndPart || scrollbarPart == ForwardTrackPart)
        return buttonsPlacement == ScrollbarButtonsDoubleEnd || buttonsPlacement == ScrollbarButtonsDoubleBoth;
    return false;
}

ALWAYS_INLINE bool scrollbarMatchesSingleButtonPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    auto buttonsPlacement = context.scrollbarState->buttonsPlacement;
    if (scrollbarPart == BackButtonStartPart || scrollbarPart == ForwardButtonEndPart || scrollbarPart == BackTrackPart || scrollbarPart == ForwardTrackPart)
        return buttonsPlacement == ScrollbarButtonsSingle;
    return false;
}

ALWAYS_INLINE bool scrollbarMatchesNoButtonPseudoClass(const SelectorChecker::CheckingContext& context)
{
    if (!context.scrollbarState)
        return false;
    auto scrollbarPart = context.scrollbarState->scrollbarPart;
    auto buttonsPlacement = context.scrollbarState->buttonsPlacement;
    if (scrollbarPart == BackTrackPart)
        return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleEnd;
    if (scrollbarPart == ForwardTrackPart)
        return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleStart;
    return false;
}

ALWAYS_INLINE bool scrollbarMatchesCornerPresentPseudoClass(const SelectorChecker::CheckingContext& context)
{
    return context.scrollbarState && context.scrollbarState->scrollCornerIsVisible;
}

#if ENABLE(FULLSCREEN_API)

ALWAYS_INLINE bool matchesFullScreenPseudoClass(const Element& element)
{
    // While a Document is in the fullscreen state, and the document's current fullscreen
    // element is an element in the document, the 'full-screen' pseudoclass applies to
    // that element. Also, an <iframe>, <object> or <embed> element whose child browsing
    // context's Document is in the fullscreen state has the 'full-screen' pseudoclass applied.
    if (is<HTMLFrameElementBase>(element) && element.containsFullScreenElement())
        return true;
    if (!element.document().fullscreenManager().isFullscreen())
        return false;
    return &element == element.document().fullscreenManager().currentFullscreenElement();
}

ALWAYS_INLINE bool matchesFullScreenAnimatingFullScreenTransitionPseudoClass(const Element& element)
{
    if (&element != element.document().fullscreenManager().currentFullscreenElement())
        return false;
    return element.document().fullscreenManager().isAnimatingFullscreen();
}

ALWAYS_INLINE bool matchesFullScreenAncestorPseudoClass(const Element& element)
{
    return element.containsFullScreenElement();
}

ALWAYS_INLINE bool matchesFullScreenDocumentPseudoClass(const Element& element)
{
    // While a Document is in the fullscreen state, the 'full-screen-document' pseudoclass applies
    // to all elements of that Document.
    if (!element.document().fullscreenManager().isFullscreen())
        return false;
    return true;
}

ALWAYS_INLINE bool matchesFullScreenControlsHiddenPseudoClass(const Element& element)
{
    if (&element != element.document().fullscreenManager().currentFullscreenElement())
        return false;
    return element.document().fullscreenManager().areFullscreenControlsHidden();
}

#endif

#if ENABLE(PICTURE_IN_PICTURE_API)

ALWAYS_INLINE bool matchesPictureInPicturePseudoClass(const Element& element)
{
    return is<HTMLVideoElement>(element) && element.document().pictureInPictureElement() == &element;
}

#endif

#if ENABLE(VIDEO)

ALWAYS_INLINE bool matchesFutureCuePseudoClass(const Element& element)
{
    return is<WebVTTElement>(element) && !downcast<WebVTTElement>(element).isPastNode();
}

ALWAYS_INLINE bool matchesPastCuePseudoClass(const Element& element)
{
    return is<WebVTTElement>(element) && downcast<WebVTTElement>(element).isPastNode();
}

#endif

ALWAYS_INLINE bool isFrameFocused(const Element& element)
{
    return element.document().frame() && element.document().frame()->selection().isFocusedAndActive();
}

// This needs to match a subset of elements matchesFocusPseudoClass match since direct focus is treated
// as a part of focus pseudo class selectors in ElementRuleCollector::collectMatchingRules.
ALWAYS_INLINE bool matchesDirectFocusPseudoClass(const Element& element)
{
    if (InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassFocus))
        return true;

    return element.focused() && isFrameFocused(element);
}

ALWAYS_INLINE bool doesShadowTreeContainFocusedElement(const Element& element)
{
    auto* shadowRoot = element.shadowRoot();
    return shadowRoot && shadowRoot->containsFocusedElement();
}

ALWAYS_INLINE bool matchesFocusPseudoClass(const Element& element)
{
    if (InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassFocus))
        return true;

    return (element.focused() || doesShadowTreeContainFocusedElement(element)) && isFrameFocused(element);
}

} // namespace WebCore
