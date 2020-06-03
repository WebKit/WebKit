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

#include "AccessibilityObject.h"
#include "CharacterData.h"
#include "Editing.h"
#include "ElementAncestorIterator.h"
#include "EventLoop.h"
#include "HTMLBRElement.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
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
        for (auto& element : lineageOfType<Element>(*startingElement)) {
            if (auto typeOrNullopt = typeForElement(element)) {
                type = *typeOrNullopt;
                matchingElement = &element;
                break;
            }
        }

        for (auto& element : lineageOfType<Element>(*startingElement)) {
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

bool TextManipulationController::isInManipulatedElement(Element& element)
{
    if (!m_manipulatedElements.capacity())
        return false; // Fast path for startObservingParagraphs.
    for (auto& ancestorOrSelf : lineageOfType<Element>(element)) {
        if (m_manipulatedElements.contains(ancestorOrSelf))
            return true;
    }
    return false;
}

void TextManipulationController::startObservingParagraphs(ManipulationItemCallback&& callback, Vector<ExclusionRule>&& exclusionRules)
{
    auto document = makeRefPtr(m_document.get());
    if (!document)
        return;

    m_callback = WTFMove(callback);
    m_exclusionRules = WTFMove(exclusionRules);

    observeParagraphs(firstPositionInNode(m_document.get()), lastPositionInNode(m_document.get()));
    flushPendingItemsForCallback();
}

class ParagraphContentIterator {
public:
    ParagraphContentIterator(const Position& start, const Position& end)
        : m_iterator({ *makeBoundaryPoint(start), *makeBoundaryPoint(end) }, TextIteratorIgnoresStyleVisibility)
        , m_iteratorNode(m_iterator.atEnd() ? nullptr : createLiveRange(m_iterator.range())->firstNode())
        , m_node(start.firstNode())
        , m_pastEndNode(end.firstNode())
    {
        if (shouldAdvanceIteratorPastCurrentNode())
            advanceIteratorNodeAndUpdateText();
    }

    void advance()
    {
        m_text = WTF::nullopt;
        advanceNode();

        if (shouldAdvanceIteratorPastCurrentNode())
            advanceIteratorNodeAndUpdateText();
    }

    struct CurrentContent {
        RefPtr<Node> node;
        StringView text;
        bool isTextContent { false };
        bool isReplacedContent { false };
    };

    CurrentContent currentContent()
    {
        CurrentContent content = { m_node.copyRef(), m_text ? m_text.value() : StringView { }, !!m_text };
        if (content.node) {
            if (auto* renderer = content.node->renderer()) {
                if (renderer->isRenderReplaced()) {
                    content.isTextContent = false;
                    content.isReplacedContent = true;
                }
            }
        }
        return content;
    }

    bool atEnd() const { return !m_text && m_iterator.atEnd() && m_node == m_pastEndNode; }

private:
    bool shouldAdvanceIteratorPastCurrentNode() const { return !m_iterator.atEnd() && m_iteratorNode == m_node; }

    void advanceNode()
    {
        if (m_node == m_pastEndNode)
            return;

        m_node = NodeTraversal::next(*m_node);
        if (!m_node)
            m_node = m_pastEndNode;
    }

    void advanceIteratorNodeAndUpdateText()
    {
        ASSERT(shouldAdvanceIteratorPastCurrentNode());

        StringBuilder stringBuilder;
        while (shouldAdvanceIteratorPastCurrentNode()) {
            stringBuilder.append(m_iterator.text());
            m_iterator.advance();
            m_iteratorNode = m_iterator.atEnd() ? nullptr : createLiveRange(m_iterator.range())->firstNode();
        }
        m_text = { stringBuilder.toString() };
    }

    TextIterator m_iterator;
    RefPtr<Node> m_iteratorNode;
    RefPtr<Node> m_node;
    RefPtr<Node> m_pastEndNode;
    Optional<String> m_text;
};

static bool isAttributeForTextManipulation(const Element& element, const QualifiedName& nameToCheck)
{
    using namespace HTMLNames;

    if (nameToCheck == valueAttr)
        return is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isTextButton();

    static const QualifiedName* const attributeNames[] = {
        &titleAttr.get(),
        &altAttr.get(),
        &placeholderAttr.get(),
        &aria_labelAttr.get(),
        &aria_placeholderAttr.get(),
        &aria_roledescriptionAttr.get(),
        &aria_valuetextAttr.get(),
    };
    for (auto& entry : attributeNames) {
        if (*entry == nameToCheck)
            return true;
    }
    return false;
}

static bool canPerformTextManipulationByReplacingEntireTextContent(const Element& element)
{
    return element.hasTagName(HTMLNames::titleTag) || element.hasTagName(HTMLNames::optionTag);
}

static Optional<TextManipulationController::ManipulationTokenInfo> tokenInfo(Node* node)
{
    if (!node)
        return WTF::nullopt;

    TextManipulationController::ManipulationTokenInfo result;
    result.documentURL = node->document().url();
    if (auto element = is<Element>(node) ? makeRefPtr(downcast<Element>(*node)) : makeRefPtr(node->parentElement())) {
        result.tagName = element->tagName();
        if (element->hasAttributeWithoutSynchronization(HTMLNames::roleAttr))
            result.roleAttribute = element->attributeWithoutSynchronization(HTMLNames::roleAttr);
    }
    return result;
}

static bool isEnclosingItemBoundaryElement(const Element& element)
{
    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    auto role = [](const Element& element) -> AccessibilityRole {
        return AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr));
    };

    if (element.hasTagName(HTMLNames::buttonTag) || role(element) == AccessibilityRole::Button)
        return true;

    if (element.hasTagName(HTMLNames::liTag) || element.hasTagName(HTMLNames::aTag)) {
        auto displayType = renderer->style().display();
        if (displayType == DisplayType::Block || displayType == DisplayType::InlineBlock)
            return true;

        for (auto parent = makeRefPtr(element.parentElement()); parent; parent = parent->parentElement()) {
            if (parent->hasTagName(HTMLNames::navTag) || role(*parent) == AccessibilityRole::LandmarkNavigation)
                return true;
        }
    }

    return false;
}

TextManipulationController::ManipulationTokens TextManipulationController::parse(StringView text, Node* textNode)
{
    Vector<ManipulationToken> tokens;
    ExclusionRuleMatcher exclusionRuleMatcher(m_exclusionRules);
    size_t positionOfLastNonHTMLSpace = WTF::notFound;
    size_t startPositionOfCurrentToken = 0;
    bool containsOnlyHTMLSpace = true;
    bool containsLineBreak = false;
    bool firstTokenContainsLineBreak = false;
    bool lastTokenContainsLineBreak = false;

    size_t index = 0;
    for (; index < text.length(); ++index) {
        auto character = text[index];
        if (isNotHTMLSpace(character)) {
            containsOnlyHTMLSpace = false;
            positionOfLastNonHTMLSpace = index;
        } else if (isHTMLLineBreak(character)) {
            containsLineBreak = true;
            if (positionOfLastNonHTMLSpace != WTF::notFound && startPositionOfCurrentToken <= positionOfLastNonHTMLSpace) {
                auto tokenString = text.substring(startPositionOfCurrentToken, positionOfLastNonHTMLSpace + 1 - startPositionOfCurrentToken).toString();
                tokens.append(ManipulationToken { m_tokenIdentifier.generate(), tokenString, tokenInfo(textNode), exclusionRuleMatcher.isExcluded(textNode) });
                startPositionOfCurrentToken = positionOfLastNonHTMLSpace + 1;
            }

            while (index < text.length() && !isNotHTMLSpace(text[index]))
                ++index;

            --index;

            auto lineBreakTokenString = text.substring(startPositionOfCurrentToken, index + 1 - startPositionOfCurrentToken).toString();
            if (tokens.isEmpty())
                firstTokenContainsLineBreak = true;
            tokens.append(ManipulationToken { m_tokenIdentifier.generate(), lineBreakTokenString, tokenInfo(textNode), true });
            startPositionOfCurrentToken = index + 1;
            lastTokenContainsLineBreak = true;

            continue;
        }
    }

    if (startPositionOfCurrentToken < text.length()) {
        auto tokenString = text.substring(startPositionOfCurrentToken, index + 1 - startPositionOfCurrentToken).toString();
        tokens.append(ManipulationToken { m_tokenIdentifier.generate(), tokenString, tokenInfo(textNode), exclusionRuleMatcher.isExcluded(textNode) });
        lastTokenContainsLineBreak = false;
    }

    return { WTFMove(tokens), containsOnlyHTMLSpace, containsLineBreak, firstTokenContainsLineBreak, lastTokenContainsLineBreak };
}

void TextManipulationController::observeParagraphs(const Position& start, const Position& end)
{
    if (start.isNull() || end.isNull())
        return;

    auto document = makeRefPtr(start.document());
    ASSERT(document);
    ParagraphContentIterator iterator { start, end };
    // TextIterator's constructor may have updated the layout and executed arbitrary scripts.
    if (document != start.document() || document != end.document())
        return;

    Vector<ManipulationToken> tokensInCurrentParagraph;
    Position startOfCurrentParagraph;
    Position endOfCurrentParagraph;
    RefPtr<Element> enclosingItemBoundaryElement;

    for (; !iterator.atEnd(); iterator.advance()) {
        auto content = iterator.currentContent();
        auto* contentNode = content.node.get();
        ASSERT(contentNode);

        if (enclosingItemBoundaryElement && !enclosingItemBoundaryElement->contains(contentNode)) {
            if (!tokensInCurrentParagraph.isEmpty())
                addItem(ManipulationItemData { startOfCurrentParagraph, endOfCurrentParagraph, nullptr, nullQName(), std::exchange(tokensInCurrentParagraph, { }) });
            enclosingItemBoundaryElement = nullptr;
        }

        if (RefPtr<Element> currentElementAncestor = is<Element>(*contentNode) ? downcast<Element>(contentNode) : contentNode->parentOrShadowHostElement()) {
            // We can exit early here because scheduleObservationUpdate calls this function on each paragraph separately.
            if (isInManipulatedElement(*currentElementAncestor))
                return;
        }

        if (is<Element>(*contentNode)) {
            auto& currentElement = downcast<Element>(*contentNode);
            if (!content.isTextContent && canPerformTextManipulationByReplacingEntireTextContent(currentElement))
                addItem(ManipulationItemData { Position(), Position(), makeWeakPtr(currentElement), nullQName(), { ManipulationToken { m_tokenIdentifier.generate(), currentElement.textContent(), tokenInfo(&currentElement) } } });

            if (currentElement.hasAttributes()) {
                for (auto& attribute : currentElement.attributesIterator()) {
                    if (isAttributeForTextManipulation(currentElement, attribute.name()))
                        addItem(ManipulationItemData { Position(), Position(), makeWeakPtr(currentElement), attribute.name(), { ManipulationToken { m_tokenIdentifier.generate(), attribute.value(), tokenInfo(&currentElement) } } });
                }
            }
            if (!enclosingItemBoundaryElement && isEnclosingItemBoundaryElement(currentElement))
                enclosingItemBoundaryElement = &currentElement;
        }

        if (content.isReplacedContent) {
            if (!tokensInCurrentParagraph.isEmpty()) {
                tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), "[]", tokenInfo(content.node.get()), true });
                endOfCurrentParagraph = positionAfterNode(contentNode);
            }
            continue;
        }

        if (!content.isTextContent)
            continue;

        auto tokensInCurrentNode = parse(content.text, contentNode);
        if (!tokensInCurrentParagraph.isEmpty() && tokensInCurrentNode.firstTokenContainsLineBreak)
            addItem(ManipulationItemData { startOfCurrentParagraph, endOfCurrentParagraph, nullptr, nullQName(), std::exchange(tokensInCurrentParagraph, { }) });

        if (tokensInCurrentParagraph.isEmpty()) {
            if (tokensInCurrentNode.containsOnlyHTMLSpace)
                continue;
            startOfCurrentParagraph = firstPositionInOrBeforeNode(contentNode);
        }

        tokensInCurrentParagraph.appendVector(tokensInCurrentNode.tokens);
        endOfCurrentParagraph = positionAfterNode(contentNode);

        if (!tokensInCurrentParagraph.isEmpty() && tokensInCurrentNode.lastTokenContainsLineBreak) {
            ASSERT(!tokensInCurrentParagraph.isEmpty());
            addItem(ManipulationItemData { startOfCurrentParagraph, endOfCurrentParagraph, nullptr, nullQName(), std::exchange(tokensInCurrentParagraph, { }) });
        }
    }

    if (!tokensInCurrentParagraph.isEmpty())
        addItem(ManipulationItemData { startOfCurrentParagraph, endOfCurrentParagraph, nullptr, nullQName(), WTFMove(tokensInCurrentParagraph) });
}

void TextManipulationController::didCreateRendererForElement(Element& element)
{
    if (isInManipulatedElement(element))
        return;

    if (m_elementsWithNewRenderer.computesEmpty())
        scheduleObservationUpdate();

    if (is<PseudoElement>(element)) {
        if (auto* host = downcast<PseudoElement>(element).hostElement())
            m_elementsWithNewRenderer.add(*host);
    } else
        m_elementsWithNewRenderer.add(element);
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

void TextManipulationController::scheduleObservationUpdate()
{
    if (!m_document)
        return;

    m_document->eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakThis = makeWeakPtr(*this)] {
        auto* controller = weakThis.get();
        if (!controller)
            return;

        HashSet<Ref<Element>> mutatedElements;
        for (auto& weakElement : controller->m_elementsWithNewRenderer)
            mutatedElements.add(weakElement);
        controller->m_elementsWithNewRenderer.clear();

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

            if (start.isNull() || end.isNull())
                continue;

            auto key = makeHashablePositionRange(start, end);
            if (!paragraphSets.add(key).isNewEntry)
                continue;

            auto* controller = weakThis.get();
            if (!controller)
                return; // Finding the start/end of paragraph may have updated layout & executed arbitrary scripts.

            controller->observeParagraphs(start.deepEquivalent(), end.deepEquivalent());
        }
        controller->flushPendingItemsForCallback();
    });
}

void TextManipulationController::addItem(ManipulationItemData&& itemData)
{
    const unsigned itemCallbackBatchingSize = 128;

    ASSERT(m_document);
    ASSERT(!itemData.tokens.isEmpty());
    auto newID = m_itemIdentifier.generate();
    m_pendingItemsForCallback.append(ManipulationItem {
        newID,
        itemData.tokens.map([](auto& token) { return token; })
    });
    m_items.add(newID, WTFMove(itemData));

    if (m_pendingItemsForCallback.size() >= itemCallbackBatchingSize)
        flushPendingItemsForCallback();
}

void TextManipulationController::flushPendingItemsForCallback()
{
    m_callback(*m_document, m_pendingItemsForCallback);
    m_pendingItemsForCallback.clear();
}

auto TextManipulationController::completeManipulation(const Vector<WebCore::TextManipulationController::ManipulationItem>& completionItems) -> Vector<ManipulationFailure>
{
    Vector<ManipulationFailure> failures;
    for (unsigned i = 0; i < completionItems.size(); ++i) {
        auto& itemToComplete = completionItems[i];
        auto identifier = itemToComplete.identifier;
        if (!identifier) {
            failures.append(ManipulationFailure { identifier, i, ManipulationFailureType::InvalidItem });
            continue;
        }

        auto itemDataIterator = m_items.find(identifier);
        if (itemDataIterator == m_items.end()) {
            failures.append(ManipulationFailure { identifier, i, ManipulationFailureType::InvalidItem });
            continue;
        }

        ManipulationItemData itemData;
        std::exchange(itemData, itemDataIterator->value);
        m_items.remove(itemDataIterator);

        auto failureOrNullopt = replace(itemData, itemToComplete.tokens);
        if (failureOrNullopt)
            failures.append(ManipulationFailure { identifier, i, *failureOrNullopt });
    }
    return failures;
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

Vector<Ref<Node>> TextManipulationController::getPath(Node* ancestor, Node* node)
{
    Vector<Ref<Node>> path;
    RefPtr<ContainerNode> containerNode = is<ContainerNode>(*node) ? &downcast<ContainerNode>(*node) : node->parentNode();
    for (; containerNode && containerNode != ancestor; containerNode = containerNode->parentNode())
        path.append(*containerNode);
    path.reverse();
    return path;
}

void TextManipulationController::updateInsertions(Vector<NodeEntry>& lastTopDownPath, const Vector<Ref<Node>>& currentTopDownPath, Node* currentNode, HashSet<Ref<Node>>& insertedNodes, Vector<NodeInsertion>& insertions)
{
    size_t i =0;
    while (i < lastTopDownPath.size() && i < currentTopDownPath.size() && lastTopDownPath[i].first.ptr() == currentTopDownPath[i].ptr())
        ++i;

    if (i != lastTopDownPath.size() || i != currentTopDownPath.size()) {
        if (i < lastTopDownPath.size())
            lastTopDownPath.shrink(i);

        for (;i < currentTopDownPath.size(); ++i) {
            Ref<Node> node = currentTopDownPath[i];
            if (!insertedNodes.add(node.copyRef()).isNewEntry) {
                auto clonedNode = node->cloneNodeInternal(node->document(), Node::CloningOperation::OnlySelf);
                if (auto* data = node->eventTargetData())
                    data->eventListenerMap.copyEventListenersNotCreatedFromMarkupToTarget(clonedNode.ptr());
                node = WTFMove(clonedNode);
            }
            insertions.append(NodeInsertion { lastTopDownPath.size() ? lastTopDownPath.last().second.ptr() : nullptr, node.copyRef() });
            lastTopDownPath.append({ currentTopDownPath[i].copyRef(), WTFMove(node) });
        }
    }

    if (currentNode)
        insertions.append(NodeInsertion { lastTopDownPath.size() ? lastTopDownPath.last().second.ptr() : nullptr, *currentNode });
}

auto TextManipulationController::replace(const ManipulationItemData& item, const Vector<ManipulationToken>& replacementTokens) -> Optional<ManipulationFailureType>
{
    if (item.start.isOrphan() || item.end.isOrphan())
        return ManipulationFailureType::ContentChanged;

    if (item.start.isNull() || item.end.isNull()) {
        RELEASE_ASSERT(item.tokens.size() == 1);
        auto element = makeRefPtr(item.element.get());
        if (!element)
            return ManipulationFailureType::ContentChanged;
        if (replacementTokens.size() > 1 && !canPerformTextManipulationByReplacingEntireTextContent(*element) && item.attributeName == nullQName())
            return ManipulationFailureType::InvalidToken;
        auto expectedTokenIdentifier = item.tokens[0].identifier;
        StringBuilder newValue;
        for (size_t i = 0; i < replacementTokens.size(); ++i) {
            if (replacementTokens[i].identifier != expectedTokenIdentifier)
                return ManipulationFailureType::InvalidToken;
            if (i)
                newValue.append(' ');
            newValue.append(replacementTokens[i].content);
        }
        if (item.attributeName == nullQName())
            element->setTextContent(newValue.toString());
        else
            element->setAttribute(item.attributeName, newValue.toString());
        return WTF::nullopt;
    }

    size_t currentTokenIndex = 0;
    HashMap<TokenIdentifier, TokenExchangeData> tokenExchangeMap;
    RefPtr<Node> commonAncestor;
    RefPtr<Node> firstContentNode;
    RefPtr<Node> lastChildOfCommonAncestorInRange;
    HashSet<Ref<Node>> nodesToRemove;
    ParagraphContentIterator iterator { item.start, item.end };
    for (; !iterator.atEnd(); iterator.advance()) {
        auto content = iterator.currentContent();
        ASSERT(content.node);

        lastChildOfCommonAncestorInRange = content.node;
        nodesToRemove.add(*content.node);

        if (!content.isReplacedContent && !content.isTextContent)
            continue;

        Vector<ManipulationToken> tokensInCurrentNode;
        if (content.isReplacedContent)
            tokensInCurrentNode.append(item.tokens[currentTokenIndex]);
        else
            tokensInCurrentNode = parse(content.text, content.node.get()).tokens;

        bool isNodeIncluded = WTF::anyOf(tokensInCurrentNode, [] (auto& token) {
            return !token.isExcluded;
        });
        for (auto& token : tokensInCurrentNode) {
            if (currentTokenIndex > item.tokens.size())
                return ManipulationFailureType::ContentChanged;

            auto& currentToken = item.tokens[currentTokenIndex++];
            if (!content.isReplacedContent && currentToken.content != token.content)
                return ManipulationFailureType::ContentChanged;

            tokenExchangeMap.set(currentToken.identifier, TokenExchangeData { content.node.copyRef(), currentToken.content, !isNodeIncluded });
        }

        if (!firstContentNode)
            firstContentNode = content.node;

        auto parentNode = content.node->parentNode();
        if (!commonAncestor)
            commonAncestor = parentNode;
        else if (!parentNode->isDescendantOf(commonAncestor.get())) {
            commonAncestor = commonInclusiveAncestor(*commonAncestor, *parentNode);
            ASSERT(commonAncestor);
        }
    }

    while (lastChildOfCommonAncestorInRange && lastChildOfCommonAncestorInRange->parentNode() != commonAncestor)
        lastChildOfCommonAncestorInRange = lastChildOfCommonAncestorInRange->parentNode();

    for (auto node = commonAncestor; node; node = node->parentNode())
        nodesToRemove.remove(*node);

    HashSet<Ref<Node>> reusedOriginalNodes;
    Vector<NodeEntry> lastTopDownPath;
    Vector<NodeInsertion> insertions;
    for (size_t index = 0; index < replacementTokens.size(); ++index) {
        auto& replacementToken = replacementTokens[index];
        auto it = tokenExchangeMap.find(replacementToken.identifier);
        if (it == tokenExchangeMap.end())
            return ManipulationFailureType::InvalidToken;

        auto& exchangeData = it->value;
        auto* originalNode = exchangeData.node.get();
        ASSERT(originalNode);
        auto replacementText = replacementToken.content;

        RefPtr<Node> replacementNode;
        if (exchangeData.isExcluded) {
            if (exchangeData.isConsumed)
                return ManipulationFailureType::ExclusionViolation;
            exchangeData.isConsumed = true;

            if (!replacementToken.content.isNull() && replacementToken.content != exchangeData.originalContent)
                return ManipulationFailureType::ExclusionViolation;

            replacementNode = originalNode;
            for (RefPtr<Node> descendentNode = NodeTraversal::next(*originalNode, originalNode); descendentNode; descendentNode = NodeTraversal::next(*descendentNode, originalNode))
                nodesToRemove.remove(*descendentNode);
        } else
            replacementNode = Text::create(commonAncestor->document(), replacementText);

        auto topDownPath = getPath(commonAncestor.get(), originalNode);
        updateInsertions(lastTopDownPath, topDownPath, replacementNode.get(), reusedOriginalNodes, insertions);
    }

    auto end = lastChildOfCommonAncestorInRange ? positionInParentAfterNode(lastChildOfCommonAncestorInRange.get()) : positionAfterNode(commonAncestor.get());
    RefPtr<Node> node = item.end.firstNode();
    RefPtr<Node> endNode = end.firstNode();
    if (node && node != endNode) {
        auto topDownPath = getPath(commonAncestor.get(), node->parentNode());
        updateInsertions(lastTopDownPath, topDownPath, nullptr, reusedOriginalNodes, insertions);
    }
    while (node != endNode) {
        Ref<Node> parentNode = *node->parentNode();
        while (!lastTopDownPath.isEmpty() && lastTopDownPath.last().first.ptr() != parentNode.ptr())
            lastTopDownPath.removeLast();

        insertions.append(NodeInsertion { lastTopDownPath.size() ? lastTopDownPath.last().second.ptr() : nullptr, *node });
        lastTopDownPath.append({ *node, *node });
        node = NodeTraversal::next(*node);
    }

    Position insertionPoint = positionBeforeNode(firstContentNode.get()).parentAnchoredEquivalent();
    while (insertionPoint.containerNode() != commonAncestor)
        insertionPoint = positionInParentBeforeNode(insertionPoint.containerNode());

    for (auto& node : nodesToRemove)
        node->remove();

    for (auto& insertion : insertions) {
        if (!insertion.parentIfDifferentFromCommonAncestor) {
            insertionPoint.containerNode()->insertBefore(insertion.child, insertionPoint.computeNodeAfterPosition());
            insertionPoint = positionInParentAfterNode(insertion.child.ptr());
        } else
            insertion.parentIfDifferentFromCommonAncestor->appendChild(insertion.child);
        if (is<Element>(insertion.child.get()))
            m_manipulatedElements.add(downcast<Element>(insertion.child.get()));
    }

    return WTF::nullopt;
}

} // namespace WebCore
