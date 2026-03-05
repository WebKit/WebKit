/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "PseudoElementUtilities.h"

#include "CSSSelectorParser.h"
#include "Document.h"
#include "Element.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIteratorInlines.h"

namespace WebCore::Style {

static RefPtr<Element> findElementForUserAgentPart(Element& host, const AtomString& userAgentPartName)
{
    RefPtr shadowRoot = host.userAgentShadowRoot();
    if (!shadowRoot)
        return nullptr;
    for (Ref descendant : descendantsOfType<Element>(*shadowRoot)) {
        if (descendant->userAgentPart() == userAgentPartName)
            return descendant.ptr();
    }
    return nullptr;
}

ResolvedComputedPseudoElement resolveComputedPseudoElement(Element& element, const String& pseudoElement)
{
    auto identifier = CSSSelectorParser::parsePseudoElement(pseudoElement, CSSSelectorParserContext { protect(element.document()) });
    if (!identifier)
        return { nullptr, { } };
    if (identifier->type == PseudoElementType::UserAgentPartFallback) {
        if (RefPtr backingElement = findElementForUserAgentPart(element, identifier->nameOrPart))
            return { WTF::move(backingElement), { } };
    }
    return { &element, *identifier };
}

} // namespace WebCore::Style
