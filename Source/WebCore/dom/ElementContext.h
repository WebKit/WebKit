/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "DocumentIdentifier.h"
#include "ElementIdentifier.h"
#include "FloatRect.h"
#include "PageIdentifier.h"

namespace WebCore {

struct ElementContext {
    FloatRect boundingRect;

    PageIdentifier webPageIdentifier;
    DocumentIdentifier documentIdentifier;
    ElementIdentifier elementIdentifier;

    ~ElementContext() = default;

    bool isSameElement(const ElementContext& other) const
    {
        return webPageIdentifier == other.webPageIdentifier && documentIdentifier == other.documentIdentifier && elementIdentifier == other.elementIdentifier;
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ElementContext> decode(Decoder&);
};

inline bool operator==(const ElementContext& a, const ElementContext& b)
{
    return a.boundingRect == b.boundingRect && a.isSameElement(b);
}

template<class Encoder>
void ElementContext::encode(Encoder& encoder) const
{
    encoder << boundingRect;
    encoder << webPageIdentifier;
    encoder << documentIdentifier;
    encoder << elementIdentifier;
}

template<class Decoder>
Optional<ElementContext> ElementContext::decode(Decoder& decoder)
{
    ElementContext context;

    if (!decoder.decode(context.boundingRect))
        return WTF::nullopt;

    auto pageIdentifier = PageIdentifier::decode(decoder);
    if (!pageIdentifier)
        return WTF::nullopt;
    context.webPageIdentifier = *pageIdentifier;

    auto documentIdentifier = DocumentIdentifier::decode(decoder);
    if (!documentIdentifier)
        return WTF::nullopt;
    context.documentIdentifier = *documentIdentifier;

    auto elementIdentifier = ElementIdentifier::decode(decoder);
    if (!elementIdentifier)
        return WTF::nullopt;
    context.elementIdentifier = *elementIdentifier;

    return context;
}

}
