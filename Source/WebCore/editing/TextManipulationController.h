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
#include "QualifiedName.h"
#include "TextManipulationItem.h"
#include <wtf/CompletionHandler.h>
#include <wtf/EnumTraits.h>
#include <wtf/ObjectIdentifier.h>
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

    struct ExclusionRule {
        enum class Type : uint8_t { Exclude, Include };

        struct ElementRule {
            AtomString localName;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<ElementRule> decode(Decoder&);
        };

        struct AttributeRule {
            AtomString name;
            String value;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<AttributeRule> decode(Decoder&);
        };

        struct ClassRule {
            AtomString className;

            template<class Encoder> void encode(Encoder&) const;
            template<class Decoder> static std::optional<ClassRule> decode(Decoder&);
        };

        Type type;
        std::variant<ElementRule, AttributeRule, ClassRule> rule;

        bool match(const Element&) const;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<ExclusionRule> decode(Decoder&);
    };

    using ManipulationItemCallback = Function<void(Document&, const Vector<TextManipulationItem>&)>;
    WEBCORE_EXPORT void startObservingParagraphs(ManipulationItemCallback&&, Vector<ExclusionRule>&& = { });

    void didUpdateContentForNode(Node&);
    void didAddOrCreateRendererForNode(Node&);
    void removeNode(Node&);

    enum class ManipulationFailureType : uint8_t {
        ContentChanged,
        InvalidItem,
        InvalidToken,
        ExclusionViolation,
    };

    struct ManipulationFailure {
        TextManipulationItemIdentifier identifier;
        uint64_t index;
        ManipulationFailureType type;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<ManipulationFailure> decode(Decoder&);
    };

    WEBCORE_EXPORT Vector<ManipulationFailure> completeManipulation(const Vector<TextManipulationItem>&);

private:
    void observeParagraphs(const Position& start, const Position& end);
    void scheduleObservationUpdate();

    struct ManipulationItemData {
        Position start;
        Position end;

        WeakPtr<Element, WeakPtrImplWithEventTargetData> element;
        QualifiedName attributeName { nullQName() };

        Vector<TextManipulationToken> tokens;
    };

    struct ManipulationUnit {
        Ref<Node> node;
        Vector<TextManipulationToken> tokens;
        bool areAllTokensExcluded { true };
        bool firstTokenContainsDelimiter { false };
        bool lastTokenContainsDelimiter { false };
    };
    ManipulationUnit createUnit(const Vector<String>&, Node&);
    void parse(ManipulationUnit&, const String&, Node&);

    bool shouldExcludeNodeBasedOnStyle(const Node&);

    void addItem(ManipulationItemData&&);
    void addItemIfPossible(Vector<ManipulationUnit>&&);
    void flushPendingItemsForCallback();

    enum class IsNodeManipulated : bool { No, Yes };
    struct NodeInsertion {
        RefPtr<Node> parentIfDifferentFromCommonAncestor;
        Ref<Node> child;
        IsNodeManipulated isChildManipulated { IsNodeManipulated::Yes };
    };
    using NodeEntry = std::pair<Ref<Node>, Ref<Node>>;
    Vector<Ref<Node>> getPath(Node*, Node*);
    void updateInsertions(Vector<NodeEntry>&, const Vector<Ref<Node>>&, Node*, HashSet<Ref<Node>>&, Vector<NodeInsertion>&);
    std::optional<ManipulationFailureType> replace(const ManipulationItemData&, const Vector<TextManipulationToken>&, HashSet<Ref<Node>>& containersWithoutVisualOverflowBeforeReplacement);

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_elementsWithNewRenderer;
    WeakHashSet<Node, WeakPtrImplWithEventTargetData> m_textNodesWithNewRenderer;
    WeakHashSet<Node, WeakPtrImplWithEventTargetData> m_manipulatedNodes;
    WeakHashSet<Node, WeakPtrImplWithEventTargetData> m_manipulatedNodesWithNewContent;
    WeakHashSet<Node, WeakPtrImplWithEventTargetData> m_addedOrNewlyRenderedNodes;

    HashMap<String, bool> m_cachedFontFamilyExclusionResults;

    bool m_didScheduleObservationUpdate { false };

    ManipulationItemCallback m_callback;
    Vector<TextManipulationItem> m_pendingItemsForCallback;

    Vector<ExclusionRule> m_exclusionRules;
    HashMap<TextManipulationItemIdentifier, ManipulationItemData> m_items;
    TextManipulationItemIdentifier m_itemIdentifier;
    TextManipulationTokenIdentifier m_tokenIdentifier;
};

template<class Encoder>
void TextManipulationController::ExclusionRule::encode(Encoder& encoder) const
{
    encoder << type << rule;
}

template<class Decoder>
std::optional<TextManipulationController::ExclusionRule> TextManipulationController::ExclusionRule::decode(Decoder& decoder)
{
    ExclusionRule result;
    if (!decoder.decode(result.type))
        return std::nullopt;
    if (!decoder.decode(result.rule))
        return std::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::ElementRule::encode(Encoder& encoder) const
{
    encoder << localName;
}

template<class Decoder>
std::optional<TextManipulationController::ExclusionRule::ElementRule> TextManipulationController::ExclusionRule::ElementRule::decode(Decoder& decoder)
{
    ElementRule result;
    if (!decoder.decode(result.localName))
        return std::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::AttributeRule::encode(Encoder& encoder) const
{
    encoder << name << value;
}

template<class Decoder>
std::optional<TextManipulationController::ExclusionRule::AttributeRule> TextManipulationController::ExclusionRule::AttributeRule::decode(Decoder& decoder)
{
    AttributeRule result;
    if (!decoder.decode(result.name))
        return std::nullopt;
    if (!decoder.decode(result.value))
        return std::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ExclusionRule::ClassRule::encode(Encoder& encoder) const
{
    encoder << className;
}

template<class Decoder>
std::optional<TextManipulationController::ExclusionRule::ClassRule> TextManipulationController::ExclusionRule::ClassRule::decode(Decoder& decoder)
{
    ClassRule result;
    if (!decoder.decode(result.className))
        return std::nullopt;
    return result;
}

template<class Encoder>
void TextManipulationController::ManipulationFailure::encode(Encoder& encoder) const
{
    encoder << identifier << index << type;
}

template<class Decoder>
std::optional<TextManipulationController::ManipulationFailure> TextManipulationController::ManipulationFailure::decode(Decoder& decoder)
{
    ManipulationFailure result;
    if (!decoder.decode(result.identifier))
        return std::nullopt;
    if (!decoder.decode(result.index))
        return std::nullopt;
    if (!decoder.decode(result.type))
        return std::nullopt;
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

template<> struct EnumTraits<WebCore::TextManipulationController::ManipulationFailureType> {
    using ManipulationFailureType = WebCore::TextManipulationController::ManipulationFailureType;
    using values = EnumValues<
        ManipulationFailureType,
        ManipulationFailureType::ContentChanged,
        ManipulationFailureType::InvalidItem,
        ManipulationFailureType::InvalidToken,
        ManipulationFailureType::ExclusionViolation
    >;
};

} // namespace WTF
