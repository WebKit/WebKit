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
#include "EventLoop.h"
#include "NodeTraversal.h"
#include "PseudoElement.h"
#include "Range.h"
#include "ScriptDisallowedScope.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

inline bool TextManipulationController::ExclusionRule::match(const Element& element) const
{
    return switchOn(rule, [&element] (ElementRule rule) {
        return rule.localName == element.localName();
    }, [&element] (AttributeRule rule) {
        return equalIgnoringASCIICase(element.getAttribute(rule.name), rule.value);
    }, [&element] (ClassRule rule) {
        return element.hasClass() && element.classNames().contains(rule.className);
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

    observeParagraphs(start, end);
}

void TextManipulationController::observeParagraphs(VisiblePosition& start, VisiblePosition& end)
{
    auto document = makeRefPtr(start.deepEquivalent().document());
    ASSERT(document);
    TextIterator iterator { start.deepEquivalent(), end.deepEquivalent() };
    if (document != start.deepEquivalent().document() || document != end.deepEquivalent().document())
        return; // TextIterator's constructor may have updated the layout and executed arbitrary scripts.

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

void TextManipulationController::didCreateRendererForElement(Element& element)
{
    if (m_recentlyInsertedElements.contains(element))
        return;

    if (m_mutatedElements.computesEmpty())
        scheduleObservartionUpdate();

    if (is<PseudoElement>(element)) {
        if (auto* host = downcast<PseudoElement>(element).hostElement())
            m_mutatedElements.add(*host);
    } else
        m_mutatedElements.add(element);
}

using PositionTuple = std::tuple<RefPtr<Node>, unsigned, unsigned>;
static const PositionTuple makePositionTuple(const Position& position)
{
    return { position.anchorNode(), static_cast<unsigned>(position.anchorType()), position.anchorType() == Position::PositionIsOffsetInAnchor ? position.offsetInContainerNode() : 0 }; 
}

static const std::pair<PositionTuple, PositionTuple> makeHashablePositionRange(const VisiblePosition& start, const VisiblePosition& end)
{
    return { makePositionTuple(start.deepEquivalent()), makePositionTuple(end.deepEquivalent()) };
}

void TextManipulationController::scheduleObservartionUpdate()
{
    if (!m_document)
        return;

    m_document->eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakThis = makeWeakPtr(*this)] {
        auto* controller = weakThis.get();
        if (!controller)
            return;

        HashSet<Ref<Element>> mutatedElements;
        for (auto& weakElement : controller->m_mutatedElements)
            mutatedElements.add(weakElement);
        controller->m_mutatedElements.clear();

        HashSet<Ref<Element>> filteredElements;
        for (auto& element : mutatedElements) {
            auto* parentElement = element->parentElement();
            if (!parentElement || !mutatedElements.contains(parentElement))
                filteredElements.add(element.copyRef());
        }
        mutatedElements.clear();

        HashSet<std::pair<PositionTuple, PositionTuple>> paragraphSets;
        for (auto& element : filteredElements) {
            auto start = startOfParagraph(firstPositionInOrBeforeNode(element.ptr()));
            auto end = endOfParagraph(lastPositionInOrAfterNode(element.ptr()));

            auto key = makeHashablePositionRange(start, end);
            if (!paragraphSets.add(key).isNewEntry)
                continue;

            auto* controller = weakThis.get();
            if (!controller)
                return; // Finding the start/end of paragraph may have updated layout & executed arbitrary scripts.

            controller->observeParagraphs(start, end);
        }
    });
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

    ManipulationItem item;
    std::exchange(item, itemIterator->value);
    m_items.remove(itemIterator);

    return replace(item, replacementTokens);
}

struct TokenExchangeData {
    RefPtr<Node> node;
    String originalContent;
    bool isExcluded { false };
    bool isConsumed { false };
};

struct ReplacementData {
    Ref<Node> originalNode;
    String newData;
};

struct NodeInsertion {
    RefPtr<Node> parentIfDifferentFromCommonAncestor;
    Ref<Node> child;
};

auto TextManipulationController::replace(const ManipulationItem& item, const Vector<ManipulationToken>& replacementTokens) -> ManipulationResult
{
    if (item.start.isOrphan() || item.end.isOrphan())
        return ManipulationResult::ContentChanged;

    TextIterator iterator { item.start, item.end };
    size_t currentTokenIndex = 0;
    HashMap<TokenIdentifier, TokenExchangeData> tokenExchangeMap;

    RefPtr<Node> commonAncestor;
    while (!iterator.atEnd()) {
        auto string = iterator.text().toString();
        if (currentTokenIndex >= item.tokens.size())
            return ManipulationResult::ContentChanged;
        auto& currentToken = item.tokens[currentTokenIndex];
        if (iterator.text() != currentToken.content)
            return ManipulationResult::ContentChanged;

        auto currentNode = makeRefPtr(iterator.node());
        tokenExchangeMap.set(currentToken.identifier, TokenExchangeData { currentNode.copyRef(), currentToken.content, currentToken.isExcluded });

        if (currentNode) {
            // FIXME: Take care of when currentNode is nullptr.
            if (!commonAncestor)
                commonAncestor = currentNode;
            else if (!currentNode->isDescendantOf(commonAncestor.get())) {
                commonAncestor = Range::commonAncestorContainer(commonAncestor.get(), currentNode.get());
                ASSERT(commonAncestor);
            }
        }

        iterator.advance();
        ++currentTokenIndex;
    }
    ASSERT(commonAncestor);

    RefPtr<Node> nodeAfterStart = item.start.computeNodeAfterPosition();
    if (!nodeAfterStart)
        nodeAfterStart = item.start.containerNode();

    RefPtr<Node> nodeAfterEnd = item.end.computeNodeAfterPosition();
    if (!nodeAfterEnd)
        nodeAfterEnd = NodeTraversal::nextSkippingChildren(*item.end.containerNode());

    HashSet<Ref<Node>> nodesToRemove;
    for (RefPtr<Node> currentNode = nodeAfterStart; currentNode && currentNode != nodeAfterEnd; currentNode = NodeTraversal::next(*currentNode)) {
        if (commonAncestor == currentNode)
            commonAncestor = currentNode->parentNode();
        nodesToRemove.add(*currentNode);
    }

    Vector<Ref<Node>> currentElementStack;
    HashSet<Ref<Node>> reusedOriginalNodes;
    Vector<NodeInsertion> insertions;
    for (auto& newToken : replacementTokens) {
        auto it = tokenExchangeMap.find(newToken.identifier);
        if (it == tokenExchangeMap.end())
            return ManipulationResult::InvalidToken;

        auto& exchangeData = it->value;

        RefPtr<Node> contentNode;
        if (exchangeData.isExcluded) {
            if (exchangeData.isConsumed)
                return ManipulationResult::ExclusionViolation;
            exchangeData.isConsumed = true;
            if (!newToken.content.isNull() && newToken.content != exchangeData.originalContent)
                return ManipulationResult::ExclusionViolation;
            contentNode = Text::create(commonAncestor->document(), exchangeData.originalContent);
        } else
            contentNode = Text::create(commonAncestor->document(), newToken.content);

        auto& originalNode = exchangeData.node ? *exchangeData.node : *commonAncestor;
        RefPtr<ContainerNode> currentNode = is<ContainerNode>(originalNode) ? &downcast<ContainerNode>(originalNode) : originalNode.parentNode();

        Vector<Ref<Node>> currentAncestors;
        for (; currentNode && currentNode != commonAncestor; currentNode = currentNode->parentNode())
            currentAncestors.append(*currentNode);
        currentAncestors.reverse();

        size_t i =0;
        while (i < currentElementStack.size() && i < currentAncestors.size() && currentElementStack[i].ptr() == currentAncestors[i].ptr())
            ++i;

        if (i == currentElementStack.size() && i == currentAncestors.size())
            insertions.append(NodeInsertion { currentElementStack.size() ? currentElementStack.last().ptr() : nullptr, contentNode.releaseNonNull() });
        else {
            if (i < currentElementStack.size())
                currentElementStack.shrink(i);
            for (;i < currentAncestors.size(); ++i) {
                Ref<Node> currentNode = currentAncestors[i].copyRef();
                if (!reusedOriginalNodes.add(currentNode.copyRef()).isNewEntry) {
                    auto clonedNode = currentNode->cloneNodeInternal(currentNode->document(), Node::CloningOperation::OnlySelf);
                    if (auto* data = currentNode->eventTargetData())
                        data->eventListenerMap.copyEventListenersNotCreatedFromMarkupToTarget(clonedNode.ptr());
                    currentNode = WTFMove(clonedNode);
                }

                insertions.append(NodeInsertion { currentElementStack.size() ? currentElementStack.last().ptr() : nullptr, currentNode.copyRef() });
                currentElementStack.append(WTFMove(currentNode));
            }
            insertions.append(NodeInsertion { currentElementStack.size() ? currentElementStack.last().ptr() : nullptr, contentNode.releaseNonNull() });
        }
    }

    Position insertionPoint = item.start;
    while (insertionPoint.containerNode() != commonAncestor)
        insertionPoint = positionInParentBeforeNode(insertionPoint.containerNode());
    ASSERT(!insertionPoint.isNull());

    for (auto& node : nodesToRemove)
        node->remove();

    for (auto& insertion : insertions) {
        if (!insertion.parentIfDifferentFromCommonAncestor)
            insertionPoint.containerNode()->insertBefore(insertion.child, insertionPoint.computeNodeBeforePosition());
        else
            insertion.parentIfDifferentFromCommonAncestor->appendChild(insertion.child);
        if (is<Element>(insertion.child.get()))
            m_recentlyInsertedElements.add(downcast<Element>(insertion.child.get()));
    }
    m_document->eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakThis = makeWeakPtr(*this)] {
        if (auto strongThis = weakThis.get())
            strongThis->m_recentlyInsertedElements.clear();
    });

    return ManipulationResult::Success;
}

} // namespace WebCore
