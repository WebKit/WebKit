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

#include "Position.h"
#include <wtf/CompletionHandler.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Optional.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;

class TextManipulationController : public CanMakeWeakPtr<TextManipulationController> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextManipulationController(Document&);

    enum TokenIdentifierType { };
    using TokenIdentifier = ObjectIdentifier<TokenIdentifierType>;

    struct ManipulationToken {
        TokenIdentifier identifier;
        String content;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<ManipulationToken> decode(Decoder&);
    };

    enum ItemIdentifierType { };
    using ItemIdentifier = ObjectIdentifier<ItemIdentifierType>;

    using ManipulationItemCallback = WTF::Function<void(Document&, ItemIdentifier, const Vector<ManipulationToken>&)>;
    WEBCORE_EXPORT void startObservingParagraphs(ManipulationItemCallback&&);

    WEBCORE_EXPORT bool completeManipulation(ItemIdentifier, const Vector<ManipulationToken>&);

private:
    struct ManipulationItem {
        Position start;
        Position end;
        Vector<ManipulationToken> tokens;
    };

    void addItem(const Position& startOfParagraph, const Position& endOfParagraph, Vector<ManipulationToken>&&);
    bool replace(const ManipulationItem&, const Vector<ManipulationToken>&);

    WeakPtr<Document> m_document;
    ManipulationItemCallback m_callback;
    HashMap<ItemIdentifier, ManipulationItem> m_items;
    ItemIdentifier m_itemIdentifier;
    TokenIdentifier m_tokenIdentifier;
};

template<class Encoder>
void TextManipulationController::ManipulationToken::encode(Encoder& encoder) const
{
    encoder << identifier << content;
}

template<class Decoder>
Optional<TextManipulationController::ManipulationToken> TextManipulationController::ManipulationToken::decode(Decoder& decoder)
{
    ManipulationToken result;
    if (!decoder.decode(result.identifier))
        return WTF::nullopt;
    if (!decoder.decode(result.content))
        return WTF::nullopt;
    return result;
}

} // namespace WebCore
