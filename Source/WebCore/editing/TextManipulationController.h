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
#include <wtf/EnumTraits.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Optional.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Element;
class VisiblePosition;

class TextManipulationController : public CanMakeWeakPtr<TextManipulationController> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextManipulationController(Document&);

    enum TokenIdentifierType { };
    using TokenIdentifier = ObjectIdentifier<TokenIdentifierType>;

    struct ManipulationToken {
        TokenIdentifier identifier;
        String content;
        bool isExcluded { false };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<ManipulationToken> decode(Decoder&);
    };

    enum ItemIdentifierType { };
    using ItemIdentifier = ObjectIdentifier<ItemIdentifierType>;

    struct ExclusionRule {
        enum class Type : uint8_t { Exclude, Include };

        struct ElementRule {
            AtomString localName;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static Optional<ElementRule> decode(Decoder&);
        };

        struct AttributeRule {
            AtomString name;
            String value;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static Optional<AttributeRule> decode(Decoder&);
        };

        struct ClassRule {
            AtomString className;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static Optional<ClassRule> decode(Decoder&);
        };

        Type type;
        WTF::Variant<ElementRule, AttributeRule, ClassRule> rule;

        bool match(const Element&) const;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<ExclusionRule> decode(Decoder&);
    };

    using ManipulationItemCallback = WTF::Function<void(Document&, ItemIdentifier, const Vector<ManipulationToken>&)>;
    WEBCORE_EXPORT void startObservingParagraphs(ManipulationItemCallback&&, Vector<ExclusionRule>&& = { });

    void didCreateRendererForElement(Element&);

    enum class ManipulationResult : uint8_t {
        Success,
        ContentChanged,
        InvalidItem,
        InvalidToken,
        ExclusionViolation,
    };
    WEBCORE_EXPORT ManipulationResult completeManipulation(ItemIdentifier, const Vector<ManipulationToken>&);

private:
    void observeParagraphs(VisiblePosition& start, VisiblePosition& end);
    void scheduleObservartionUpdate();

    struct ManipulationItem {
        Position start;
        Position end;
        Vector<ManipulationToken> tokens;
    };

    void addItem(const Position& startOfParagraph, const Position& endOfParagraph, Vector<ManipulationToken>&&);
    ManipulationResult replace(const ManipulationItem&, const Vector<ManipulationToken>&);

    WeakPtr<Document> m_document;
    WeakHashSet<Element> m_mutatedElements;
    WeakHashSet<Element> m_recentlyInsertedElements;
    ManipulationItemCallback m_callback;
    Vector<ExclusionRule> m_exclusionRules;
    HashMap<ItemIdentifier, ManipulationItem> m_items;
    ItemIdentifier m_itemIdentifier;
    TokenIdentifier m_tokenIdentifier;
};

template<class Encoder>
void TextManipulationController::ManipulationToken::encode(Encoder& encoder) const
{
    encoder << identifier << content << isExcluded;
}

template<class Decoder>
Optional<TextManipulationController::ManipulationToken> TextManipulationController::ManipulationToken::decode(Decoder& decoder)
{
    ManipulationToken result;
    if (!decoder.decode(result.identifier))
        return WTF::nullopt;
    if (!decoder.decode(result.content))
        return WTF::nullopt;
    if (!decoder.decode(result.isExcluded))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::encode(Encoder& encoder) const
{
    encoder << type << rule;
}

template<class Decoder>
Optional<TextManipulationController::ExclusionRule> TextManipulationController::ExclusionRule::decode(Decoder& decoder)
{
    ExclusionRule result;
    if (!decoder.decode(result.type))
        return WTF::nullopt;
    if (!decoder.decode(result.rule))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::ElementRule::encode(Encoder& encoder) const
{
    encoder << localName;
}

template<class Decoder>
Optional<TextManipulationController::ExclusionRule::ElementRule> TextManipulationController::ExclusionRule::ElementRule::decode(Decoder& decoder)
{
    ElementRule result;
    if (!decoder.decode(result.localName))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::AttributeRule::encode(Encoder& encoder) const
{
    encoder << name << value;
}

template<class Decoder>
Optional<TextManipulationController::ExclusionRule::AttributeRule> TextManipulationController::ExclusionRule::AttributeRule::decode(Decoder& decoder)
{
    AttributeRule result;
    if (!decoder.decode(result.name))
        return WTF::nullopt;
    if (!decoder.decode(result.value))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::ClassRule::encode(Encoder& encoder) const
{
    encoder << className;
}

template<class Decoder>
Optional<TextManipulationController::ExclusionRule::ClassRule> TextManipulationController::ExclusionRule::ClassRule::decode(Decoder& decoder)
{
    ClassRule result;
    if (!decoder.decode(result.className))
        return WTF::nullopt;
    return result;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::TextManipulationController::ExclusionRule::Type> {
    using ExclusionRule = WebCore::TextManipulationController::ExclusionRule;
    using values = EnumValues<
        ExclusionRule::Type,
        ExclusionRule::Type::Include,
        ExclusionRule::Type::Exclude
    >;
};

template<> struct EnumTraits<WebCore::TextManipulationController::ManipulationResult> {
    using ManipulationResult = WebCore::TextManipulationController::ManipulationResult;
    using values = EnumValues<
        ManipulationResult,
        ManipulationResult::Success,
        ManipulationResult::ContentChanged,
        ManipulationResult::InvalidItem,
        ManipulationResult::InvalidToken,
        ManipulationResult::ExclusionViolation
    >;
};

} // namespace WTF
