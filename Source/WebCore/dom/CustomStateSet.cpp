/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CustomStateSet.h"

#include "CSSParser.h"
#include "Element.h"
#include "PseudoClassChangeInvalidation.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CustomStateSet);

bool CustomStateSet::addToSetLike(const AtomString& state)
{
    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (RefPtr element = m_element.get())
        styleInvalidation.emplace(*element, CSSSelector::PseudoClass::State, Style::PseudoClassChangeInvalidation::AnyValue);

    return m_states.add(AtomString(state)).isNewEntry;
}

bool CustomStateSet::removeFromSetLike(const AtomString& state)
{
    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (RefPtr element = m_element.get())
        styleInvalidation.emplace(*element, CSSSelector::PseudoClass::State, Style::PseudoClassChangeInvalidation::AnyValue);

    return m_states.remove(AtomString(state));
}

void CustomStateSet::clearFromSetLike()
{
    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (RefPtr element = m_element.get())
        styleInvalidation.emplace(*element, CSSSelector::PseudoClass::State, Style::PseudoClassChangeInvalidation::AnyValue);

    m_states.clear();
}

bool CustomStateSet::has(const AtomString& state) const
{
    return m_states.contains(state);
}

} // namespace WebCore
