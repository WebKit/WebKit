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

#include "HTMLInputElement.h"
#include "HTMLOptionElement.h"
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
    if (element->isFormControlElement() || isHTMLOptionElement(element) || isHTMLOptGroupElement(element))
        return !element->isDisabledFormControl();
    return false;
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
    return element->matchesReadOnlyPseudoClass();
}

ALWAYS_INLINE bool matchesReadWritePseudoClass(const Element* element)
{
    return element->matchesReadWritePseudoClass();
}

ALWAYS_INLINE bool shouldAppearIndeterminate(const Element* element)
{
    return element->shouldAppearIndeterminate();
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
