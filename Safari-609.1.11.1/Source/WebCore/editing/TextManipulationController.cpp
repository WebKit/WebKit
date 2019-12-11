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

#include "config.h"
#include "TextManipulationController.h"

#include "CharacterData.h"
#include "Editing.h"
#include "ElementAncestorIterator.h"
#include "ScriptDisallowedScope.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

inline bool TextManipulationController::ExclusionRule::match(const Element& element) const
{
    return switchOn(rule, [&element] (ElementRule rule) {
        return rule.localName == element.localName();
    }, [&element] (AttributeRule rule) {
        return equalIgnoringASCIICase(element.getAttribute(rule.name), rule.value);
    });
}

class ExclusionRuleMatcher {
public:
    using ExclusionRule = TextManipulationController::ExclusionRule;
    using Type = TextManipulationController::ExclusionRule::Type;

    ExclusionRuleMatcher(const Vector<ExclusionRule>& rules)
        : m_rules(rules)
    { }

    bool isExcluded(Node* node)
    {
        if (!node)
            return false;

        RefPtr<Element> startingElement = is<Element>(*node) ? downcast<Element>(node) : node->parentElement();
        if (!startingElement)
            return false;

        Type type = Type::Include;
        RefPtr<Element> matchingElement;
        for (auto& element : elementLineage(startingElement.get())) {
            if (auto typeOrNullopt = typeForElement(element)) {
                type = *typeOrNullopt;
                matchingElement = &element;
                break;
            }
        }

        for (auto& element : elementLineage(startingElement.get())) {
            m_cache.set(element, type);
            if (&element == matchingElement)
                break;
        }

        return type == Type::Exclude;
    }

    Optional<Type> typeForElement(Element& element)
    {
        auto it = m_cache.find(element);
        if (it != m_cache.end())
            return it->value;

        for (auto& rule : m_rules) {
            if (rule.match(element))
                return rule.type;
        }

        return WTF::nullopt;
    }

private:
    const Vector<ExclusionRule>& m_rules;
    HashMap<Ref<Element>, ExclusionRule::Type> m_cache;
};

TextManipulationController::TextManipulationController(Document& document)
    : m_document(makeWeakPtr(document))
{
}

void TextManipulationController::startObservingParagraphs(ManipulationItemCallback&& callback, Vector<ExclusionRule>&& exclusionRules)
{
    auto document = makeRefPtr(m_document.get());
    if (!document)
        return;

    m_callback = WTFMove(callback);
    m_exclusionRules = WTFMove(exclusionRules);

    VisiblePosition start = firstPositionInNode(m_document.get());
    VisiblePosition end = lastPositionInNode(m_document.get());
    TextIterator iterator { start.deepEquivalent(), end.deepEquivalent() };
    if (!document)
        return; // VisiblePosition or TextIterator's constructor may have updated the layout and executed arbitrary scripts.

    ExclusionRuleMatcher exclusionRuleMatcher(m_exclusionRules);
    Vector<ManipulationToken> tokensInCurrentParagraph;
    Position startOfCurrentParagraph = start.deepEquivalent();
    while (!iterator.atEnd()) {
        StringView currentText = iterator.text();

        if (startOfCurrentParagraph.isNull())
            startOfCurrentParagraph = iterator.range()->startPosition();

        size_t endOfLastNewLine = 0;
        size_t offsetOfNextNewLine = 0;
        while ((offsetOfNextNewLine = currentText.find('\n', endOfLastNewLine)) != notFound) {
            if (endOfLastNewLine < offsetOfNextNewLine) {
                auto stringUntilEndOfLine = currentText.substring(endOfLastNewLine, offsetOfNextNewLine - endOfLastNewLine).toString();
                tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), stringUntilEndOfLine, exclusionRuleMatcher.isExcluded(iterator.node()) });
            }

            auto lastRange = iterator.range();
            if (offsetOfNextNewLine < currentText.length()) {
                lastRange->setStart(firstPositionInOrBeforeNode(iterator.node())); // Move the start to the beginning of the current node.
                TextIterator::subrange(lastRange, 0, offsetOfNextNewLine);
            }
            Position endOfCurrentParagraph = lastRange->endPosition();

            if (!tokensInCurrentParagraph.isEmpty())
                addItem(startOfCurrentParagraph, endOfCurrentParagraph, WTFMove(tokensInCurrentParagraph));
            startOfCurrentParagraph.clear();
            endOfLastNewLine = offsetOfNextNewLine + 1;
        }

        auto remainingText = currentText.substring(endOfLastNewLine);
        if (remainingText.length())
            tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), remainingText.toString(), exclusionRuleMatcher.isExcluded(iterator.node()) });

        iterator.advance();
    }

    if (!tokensInCurrentParagraph.isEmpty())
        addItem(startOfCurrentParagraph, end.deepEquivalent(), WTFMove(tokensInCurrentParagraph));
}

void TextManipulationController::addItem(const Position& startOfParagraph, const Position& endOfParagraph, Vector<ManipulationToken>&& tokens)
{
    ASSERT(m_document);
    auto result = m_items.add(m_itemIdentifier.generate(), ManipulationItem { startOfParagraph, endOfParagraph, WTFMove(tokens) });
    m_callback(*m_document, result.iterator->key, result.iterator->value.tokens);
}

auto TextManipulationController::completeManipulation(ItemIdentifier itemIdentifier, const Vector<ManipulationToken>& replacementTokens) -> ManipulationResult
{
    if (!itemIdentifier)
        return ManipulationResult::InvalidItem;

    auto itemIterator = m_items.find(itemIdentifier);
    if (itemIterator == m_items.end())
        return ManipulationResult::InvalidItem;

    auto didReplace = replace(itemIterator->value, replacementTokens);

    m_items.remove(itemIterator);

    return didReplace;
}

struct DOMChange {
    Ref<CharacterData> node;
    String newData;
};

auto TextManipulationController::replace(const ManipulationItem& item, const Vector<ManipulationToken>& replacementTokens) -> ManipulationResult
{
    TextIterator iterator { item.start, item.end };
    size_t currentTokenIndex = 0;
    HashMap<TokenIdentifier, std::pair<RefPtr<Node>, const ManipulationToken*>> tokenToNodeTokenPair;

    while (!iterator.atEnd()) {
        auto string = iterator.text().toString();
        if (currentTokenIndex >= item.tokens.size())
            return ManipulationResult::ContentChanged;
        auto& currentToken = item.tokens[currentTokenIndex];
        if (iterator.text() != currentToken.content)
            return ManipulationResult::ContentChanged;
        tokenToNodeTokenPair.set(currentToken.identifier, std::pair<RefPtr<Node>, const ManipulationToken*> { iterator.node(), &currentToken });
        iterator.advance();
        ++currentTokenIndex;
    }

    // FIXME: This doesn't preseve the order of the replacement at all.
    Vector<DOMChange> changes;
    for (auto& newToken : replacementTokens) {
        auto it = tokenToNodeTokenPair.find(newToken.identifier);
        if (it == tokenToNodeTokenPair.end())
            return ManipulationResult::InvalidToken;
        auto& oldToken = *it->value.second;
        if (oldToken.isExcluded)
            return ManipulationResult::ExclusionViolation;
        auto* node = it->value.first.get();
        if (!node || !is<CharacterData>(*node))
            continue;
        changes.append({ downcast<CharacterData>(*node), newToken.content });
    }

    for (auto& change : changes)
        change.node->setData(change.newData);

    return ManipulationResult::Success;
}

} // namespace WebCore
