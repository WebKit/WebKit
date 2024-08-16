/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ViewTransitionTypeSet.h"

#include "Document.h"
#include "PseudoClassChangeInvalidation.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ViewTransitionTypeSet);

ViewTransitionTypeSet::ViewTransitionTypeSet(Document& document, Vector<AtomString>&& initialActiveTypes)
    : m_typeSet()
    , m_document(document)
{
    for (auto initialActiveType : initialActiveTypes)
        m_typeSet.add(initialActiveType);
}

void ViewTransitionTypeSet::initializeSetLike(DOMSetAdapter& setAdapter) const
{
    for (auto activeType : m_typeSet)
        setAdapter.add<IDLDOMString>(activeType);
}

void ViewTransitionTypeSet::clearFromSetLike()
{
    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_document.documentElement()) {
        styleInvalidation.emplace(
            *m_document.documentElement(),
            CSSSelector::PseudoClass::ActiveViewTransitionType,
            Style::PseudoClassChangeInvalidation::AnyValue
        );
    }

    m_typeSet.clear();
}

void ViewTransitionTypeSet::addToSetLike(const AtomString& type)
{
    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_document.documentElement()) {
        styleInvalidation.emplace(
            *m_document.documentElement(),
            CSSSelector::PseudoClass::ActiveViewTransitionType,
            Style::PseudoClassChangeInvalidation::AnyValue
        );
    }

    m_typeSet.add(type);
}

bool ViewTransitionTypeSet::removeFromSetLike(const AtomString& type)
{
    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_document.documentElement()) {
        styleInvalidation.emplace(
            *m_document.documentElement(),
            CSSSelector::PseudoClass::ActiveViewTransitionType,
            Style::PseudoClassChangeInvalidation::AnyValue
        );
    }

    return m_typeSet.remove(type);
}

bool ViewTransitionTypeSet::hasType(const AtomString& type) const
{
    return m_typeSet.contains(type);
}

}
