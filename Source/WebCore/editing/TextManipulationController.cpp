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
        , m_currentNodeForFindingInvisibleContent(start.firstNode())
        , m_pastEndNode(end.firstNode())
    {
    }

    void advance()
    {
        // FIXME: Add a node callback to TextIterator instead of traversing the node tree twice like this.
        if (m_currentNodeForFindingInvisibleContent != m_iteratorNode && m_currentNodeForFindingInvisibleContent != m_pastEndNode) {
            moveCurrentNodeForward();
            return;
        }

        if (m_iterator.atEnd())
            return;

        auto previousIteratorNode = m_iteratorNode;

        m_iterator.advance();
        m_iteratorNode = m_iterator.atEnd() ? nullptr : createLiveRange(m_iterator.range())->firstNode();
        if (previousIteratorNode != m_iteratorNode)
            moveCurrentNodeForward();
    }

    struct CurrentContent {
        RefPtr<Node> node;
        StringView text;
        bool isTextContent { false };
        bool isReplacedContent { false };
    };

    CurrentContent currentContent()
    {
        CurrentContent content;
        if (m_currentNodeForFindingInvisibleContent && m_currentNodeForFindingInvisibleContent != m_iteratorNode)
            content = { m_currentNodeForFindingInvisibleContent.copyRef(), StringView { } };
        else
            content = { m_iterator.node(), m_iterator.text(), true };
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

    Position startPosition()
    {
        return createLegacyEditingPosition(m_iterator.range().start);
    }

    Position endPosition()
    {
        return createLegacyEditingPosition(m_iterator.range().end);
    }

    bool atEnd() const { return m_iterator.atEnd() && m_currentNodeForFindingInvisibleContent == m_pastEndNode; }

private:
    void moveCurrentNodeForward()
    {
        if (m_currentNodeForFindingInvisibleContent == m_pastEndNode)
            return;

        m_currentNodeForFindingInvisibleContent = NodeTraversal::next(*m_currentNodeForFindingInvisibleContent);
        if (!m_currentNodeForFindingInvisibleContent)
            m_currentNodeForFindingInvisibleContent = m_pastEndNode;
    }

    TextIterator m_iterator;
    RefPtr<Node> m_iteratorNode;
    RefPtr<Node> m_currentNodeForFindingInvisibleContent;
    RefPtr<Node> m_pastEndNode;
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

static bool containsOnlyHTMLSpaces(StringView text)
{
    for (unsigned index = 0; index < text.length(); ++index) {
        if (isNotHTMLSpace(text[index]))
            return false;
    }

    return true;
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

void TextManipulationController::observeParagraphs(const Position& start, const Position& end)
{
    if (start.isNull() || end.isNull())
        return;

    auto document = makeRefPtr(start.document());
    ASSERT(document);
    ParagraphContentIterator iterator { start, end };
    VisiblePosition visibleStart = start;
    VisiblePosition visibleEnd = end;
    if (document != start.document() || document != end.document())
        return; // TextIterator's constructor may have updated the layout and executed arbitrary scripts.

    ExclusionRuleMatcher exclusionRuleMatcher(m_exclusionRules);
    Vector<ManipulationToken> tokensInCurrentParagraph;
    Position startOfCurrentParagraph;
    Position endOfCurrentParagraph;
    RefPtr<Element> enclosingItemBoundaryElement;

    auto isEnclosingItemBoundaryElement = [](const Element& element) {
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
    };

    for (; !iterator.atEnd(); iterator.advance()) {
        auto content = iterator.currentContent();
        if (content.node) {
            if (enclosingItemBoundaryElement && !enclosingItemBoundaryElement->contains(content.node.get())) {
                if (!tokensInCurrentParagraph.isEmpty())
                    addItem(ManipulationItemData { startOfCurrentParagraph, endOfCurrentParagraph, nullptr, nullQName(), std::exchange(tokensInCurrentParagraph, { }) });
                enclosingItemBoundaryElement = nullptr;
            }

            if (RefPtr<Element> currentElementAncestor = is<Element>(*content.node) ? downcast<Element>(content.node.get()) : content.node->parentOrShadowHostElement()) {
                if (isInManipulatedElement(*currentElementAncestor))
                    return; // We can exit early here because scheduleObservartionUpdate calls this function on each paragraph separately.
            }

            if (is<Element>(*content.node)) {
                auto& currentElement = downcast<Element>(*content.node);
                if (!content.isTextContent && canPerformTextManipulationByReplacingEntireTextContent(currentElement)) {
                    addItem(ManipulationItemData { Position(), Position(), makeWeakPtr(currentElement), nullQName(),
                        { ManipulationToken { m_tokenIdentifier.generate(), currentElement.textContent(), tokenInfo(&currentElement) } } });
                }
                if (currentElement.hasAttributes()) {
                    for (auto& attribute : currentElement.attributesIterator()) {
                        if (isAttributeForTextManipulation(currentElement, attribute.name())) {
                            addItem(ManipulationItemData { Position(), Position(), makeWeakPtr(currentElement), attribute.name(),
                                { ManipulationToken { m_tokenIdentifier.generate(), attribute.value(), tokenInfo(&currentElement) } } });
                        }
                    }
                }
                if (!enclosingItemBoundaryElement && isEnclosingItemBoundaryElement(currentElement))
                    enclosingItemBoundaryElement = &currentElement;
            }
        }

        if (content.isReplacedContent) {
            if (tokensInCurrentParagraph.isEmpty())
                continue;

            auto currentEndOfCurrentParagraph = positionAfterNode(content.node.get());
            // This is at the same Node as last token, so it is already included in current range.
            if (!is<Text>(content.node) && currentEndOfCurrentParagraph.equals(endOfCurrentParagraph))
                continue;

            endOfCurrentParagraph = currentEndOfCurrentParagraph;
            tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), "[]", tokenInfo(content.node.get()), true });

            continue;
        }

        if (!content.isTextContent)
            continue;

        size_t startOfCurrentLine = 0;
        size_t offsetOfNextNewLine = 0;
        StringView currentText = content.text;
        while ((offsetOfNextNewLine = currentText.find('\n', startOfCurrentLine)) != notFound) {
            if (is<Text>(content.node) && startOfCurrentLine < offsetOfNextNewLine) {
                auto stringUntilEndOfLine = currentText.substring(startOfCurrentLine, offsetOfNextNewLine - startOfCurrentLine).toString();
                auto& textNode = downcast<Text>(*content.node);
                endOfCurrentParagraph = Position(&textNode, offsetOfNextNewLine);
                if (tokensInCurrentParagraph.isEmpty())
                    startOfCurrentParagraph = Position(&textNode, startOfCurrentLine);

                tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), stringUntilEndOfLine, tokenInfo(&textNode), exclusionRuleMatcher.isExcluded(content.node.get()) });
            }

            if (!tokensInCurrentParagraph.isEmpty()) {
                if (is<HTMLBRElement>(content.node))
                    endOfCurrentParagraph = positionAfterNode(content.node.get());
                addItem(ManipulationItemData { startOfCurrentParagraph, endOfCurrentParagraph, nullptr, nullQName(), std::exchange(tokensInCurrentParagraph, { }) });
            }
            startOfCurrentLine = offsetOfNextNewLine + 1;
        }

        auto remainingText = currentText.substring(startOfCurrentLine);
        if (!containsOnlyHTMLSpaces(remainingText)) {
            if (tokensInCurrentParagraph.isEmpty()) {
                if (startOfCurrentLine && is<Text>(content.node))
                    startOfCurrentParagraph = Position(&downcast<Text>(*content.node), startOfCurrentLine);
                else
                    startOfCurrentParagraph = iterator.startPosition();
            }
            endOfCurrentParagraph = iterator.endPosition();
            tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), remainingText.toString(), tokenInfo(content.node.get()), exclusionRuleMatcher.isExcluded(content.node.get()) });
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

struct NodeInsertion {
    RefPtr<Node> parentIfDifferentFromCommonAncestor;
    Ref<Node> child;
};

auto TextManipulationController::replace(const ManipulationItemData& item, const Vector<ManipulationToken>& replacementTokens) -> Optional<ManipulationFailureType>
{
    if (item.start.isOrphan() || item.end.isOrphan())
        return ManipulationFailureType::ContentChanged;

    size_t currentTokenIndex = 0;
    HashMap<TokenIdentifier, TokenExchangeData> tokenExchangeMap;

    if (item.start.isNull() || item.end.isNull()) {
        RELEASE_ASSERT(item.tokens.size() == 1);
        auto element = makeRefPtr(item.element.get());
        if (!element)
            return ManipulationFailureType::ContentChanged;
        if (replacementTokens.size() > 1 && !canPerformTextManipulationByReplacingEntireTextContent(*element))
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

    RefPtr<Node> commonAncestor;
    RefPtr<Node> firstContentNode;
    ParagraphContentIterator iterator { item.start, item.end };
    HashSet<Ref<Node>> excludedNodes;
    HashSet<Ref<Node>> nodesToRemove;
    RefPtr<Node> nodeToInsertBackAtEnd;
    for (; !iterator.atEnd(); iterator.advance()) {
        auto content = iterator.currentContent();
        
        if (content.node)
            nodesToRemove.add(*content.node);

        if (!content.isReplacedContent && !content.isTextContent)
            continue;

        if (currentTokenIndex >= item.tokens.size()) {
            if (content.node && !nodeToInsertBackAtEnd && content.isTextContent && content.text == "\n") { // br
                nodeToInsertBackAtEnd = content.node;
                continue;
            }
            return ManipulationFailureType::ContentChanged;
        }

        if (content.isTextContent && containsOnlyHTMLSpaces(content.text)) {
            // <br> should not exist in the middle of a paragraph.
            if (is<HTMLBRElement>(content.node))
                return ManipulationFailureType::ContentChanged;
            continue;
        }

        auto& currentToken = item.tokens[currentTokenIndex];
        if (!content.isReplacedContent && content.text != currentToken.content)
            return ManipulationFailureType::ContentChanged;

        tokenExchangeMap.set(currentToken.identifier, TokenExchangeData { content.node.copyRef(), currentToken.content, currentToken.isExcluded });
        ++currentTokenIndex;
        
        if (!firstContentNode)
            firstContentNode = content.node;

        // FIXME: Take care of when currentNode is nullptr.
        if (RefPtr<Node> parent = content.node ? content.node->parentNode() : nullptr) {
            if (!commonAncestor)
                commonAncestor = parent;
            else if (!parent->isDescendantOf(commonAncestor.get())) {
                commonAncestor = commonInclusiveAncestor(*commonAncestor, *parent);
                ASSERT(commonAncestor);
            }
        }
    }

    for (auto node = commonAncestor; node; node = node->parentNode())
        nodesToRemove.remove(*node);

    Vector<Ref<Node>> currentElementStack;
    HashSet<Ref<Node>> reusedOriginalNodes;
    Vector<NodeInsertion> insertions;
    for (auto& newToken : replacementTokens) {
        auto it = tokenExchangeMap.find(newToken.identifier);
        if (it == tokenExchangeMap.end())
            return ManipulationFailureType::InvalidToken;

        auto& exchangeData = it->value;

        RefPtr<Node> contentNode;
        if (exchangeData.isExcluded) {
            if (exchangeData.isConsumed)
                return ManipulationFailureType::ExclusionViolation;
            exchangeData.isConsumed = true;
            if (!newToken.content.isNull() && newToken.content != exchangeData.originalContent)
                return ManipulationFailureType::ExclusionViolation;
            contentNode = exchangeData.node;
            for (RefPtr<Node> currentDescendentNode = NodeTraversal::next(*contentNode, contentNode.get()); currentDescendentNode; currentDescendentNode = NodeTraversal::next(*currentDescendentNode, contentNode.get()))
                nodesToRemove.remove(*currentDescendentNode);
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
    if (nodeToInsertBackAtEnd)
        insertions.append(NodeInsertion { nullptr, nodeToInsertBackAtEnd.releaseNonNull() });

    Position insertionPoint = positionBeforeNode(firstContentNode.get()).parentAnchoredEquivalent();
    while (insertionPoint.containerNode() != commonAncestor)
        insertionPoint = positionInParentBeforeNode(insertionPoint.containerNode());
    ASSERT(!insertionPoint.isNull());
    ASSERT(!insertionPoint.isOrphan());

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
