/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SearchInputType.h"

#include "ElementWithPseudoId.h"
#include "HTMLInputElement.h"
#include "ShadowRoot.h"
#include "TextControlInnerElements.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

inline SearchInputType::SearchInputType(HTMLInputElement* element)
    : BaseTextInputType(element)
    , m_resultsButton(0)
    , m_cancelButton(0)
{
}

PassOwnPtr<InputType> SearchInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new SearchInputType(element));
}

const AtomicString& SearchInputType::formControlType() const
{
    return InputTypeNames::search();
}

bool SearchInputType::shouldRespectSpeechAttribute()
{
    return true;
}

bool SearchInputType::isSearchField() const
{
    return true;
}

bool SearchInputType::needsContainer() const
{
    return true;
}

void SearchInputType::createShadowSubtree()
{
    ASSERT(!m_resultsButton);
    ASSERT(!m_cancelButton);

    TextFieldInputType::createShadowSubtree();
    HTMLElement* container = containerElement();
    HTMLElement* textWrapper = innerBlockElement();
    ASSERT(container);
    ASSERT(textWrapper);

    ExceptionCode ec = 0;
    RefPtr<HTMLElement> results = SearchFieldResultsButtonElement::create(element()->document());
    m_resultsButton = results.get();
    container->insertBefore(results.release(), textWrapper, ec);

    RefPtr<HTMLElement> cancel = SearchFieldCancelButtonElement::create(element()->document());
    m_cancelButton = cancel.get();
    container->insertBefore(cancel.release(), textWrapper->nextSibling(), ec);

}

void SearchInputType::destroyShadowSubtree()
{
    TextFieldInputType::destroyShadowSubtree();
    m_resultsButton = 0;
    m_cancelButton = 0;
}


} // namespace WebCore
