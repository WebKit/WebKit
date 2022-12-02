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
#include "TextManipulationItem.h"

#include "AccessibilityObject.h"
#include "CharacterData.h"
#include "Editing.h"
#include "ElementAncestorIterator.h"
#include "EventLoop.h"
#include "FrameView.h"
#include "HTMLBRElement.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "Logging.h"
#include "NodeRenderStyle.h"
#include "NodeTraversal.h"
#include "PseudoElement.h"
#include "RenderBox.h"
#include "ScriptDisallowedScope.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

inline bool TextManipulationController::ExclusionRule::match(const Element& element) const
{
    return WTF::switchOn(rule, [&element] (ElementRule rule) {
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

    std::optional<Type> typeForElement(Element& element)
    {
        auto it = m_cache.find(element);
        if (it != m_cache.end())
            return it->value;

        for (auto& rule : m_rules) {
            if (rule.match(element))
                return rule.type;
        }

        return std::nullopt;
    }

private:
    const Vector<ExclusionRule>& m_rules;
    HashMap<Ref<Element>, ExclusionRule::Type> m_cache;
};

TextManipulationController::TextManipulationController(Document& document)
    : m_document(document)
{
}

void TextManipulationController::startObservingParagraphs(ManipulationItemCallback&& callback, Vector<ExclusionRule>&& exclusionRules)
{
    RefPtr document { m_document.get() };
    if (!document)
        return;

    m_callback = WTFMove(callback);
    m_exclusionRules = WTFMove(exclusionRules);

    observeParagraphs(firstPositionInNode(m_document.get()), lastPositionInNode(m_document.get()));
    flushPendingItemsForCallback();
}

static bool isInPrivateUseArea(UChar character)
{
    return 0xE000 <= character && character <= 0xF8FF;
}

static bool isTokenDelimiter(UChar character)
{
    return isHTMLLineBreak(character) || isInPrivateUseArea(character);
}

static bool isNotSpace(UChar character)
{
    if (character == noBreakSpace)
        return false;

    return isNotHTMLSpace(character);
}

class ParagraphContentIterator {
public:
    ParagraphContentIterator(const Position& start, const Position& end)
        : m_iterator(*makeSimpleRange(start, end), TextIteratorBehavior::IgnoresStyleVisibility)
        , m_node(start.firstNode())
        , m_pastEndNode(end.firstNode())
    {
        if (shouldAdvanceIteratorPastCurrentNode())
            advanceIteratorNodeAndUpdateText();
    }

    void advance()
    {
        m_text = std::nullopt;
        advanceNode();

        if (shouldAdvanceIteratorPastCurrentNode())
            advanceIteratorNodeAndUpdateText();
    }

    struct CurrentContent {
        RefPtr<Node> node;
        Vector<String> text;
        bool isTextContent { false };
        bool isReplacedContent { false };
    };

    CurrentContent currentContent()
    {
        CurrentContent content = { m_node.copyRef(), m_text ? m_text.value() : Vector<String> { }, !!m_text };
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
    bool shouldAdvanceIteratorPastCurrentNode() const
    {
        if (m_iterator.atEnd())
            return false;

        auto* iteratorNode = m_iterator.node();
        return !iteratorNode || iteratorNode == m_node;
    }

    void advanceNode()
    {
        if (m_node == m_pastEndNode)
            return;

        m_node = NodeTraversal::next(*m_node);
        if (!m_node)
            m_node = m_pastEndNode;
    }

    void appendToText(Vector<String>& text, StringBuilder& stringBuilder)
    {
        if (!stringBuilder.isEmpty()) {
            text.append(stringBuilder.toString());
            stringBuilder.clear();
        }
    }

    void advanceIteratorNodeAndUpdateText()
    {
        ASSERT(shouldAdvanceIteratorPastCurrentNode());

        StringBuilder stringBuilder;
        Vector<String> text;
        while (shouldAdvanceIteratorPastCurrentNode()) {
            auto iteratorText = m_iterator.text();
            if (m_iterator.range().collapsed()) {
                if (iteratorText == "\n"_s) {
                    appendToText(text, stringBuilder);
                    text.append({ });
                }
            } else
                stringBuilder.append(iteratorText);

            m_iterator.advance();
        }
        appendToText(text, stringBuilder);
        m_text = text;
    }

    TextIterator m_iterator;
    RefPtr<Node> m_node;
    RefPtr<Node> m_pastEndNode;
    std::optional<Vector<String>> m_text;
};

static bool shouldExtractValueForTextManipulation(const HTMLInputElement& input)
{
    if (input.isSearchField() || equalIgnoringASCIICase(input.attributeWithoutSynchronization(HTMLNames::typeAttr), InputTypeNames::text()))
        return !input.lastChangeWasUserEdit();

    return input.isTextButton();
}

static bool isAttributeForTextManipulation(const QualifiedName& nameToCheck)
{
    using namespace HTMLNames;
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

static bool areEqualIgnoringLeadingAndTrailingWhitespaces(const String& content, const String& originalContent)
{
    return content.stripWhiteSpace() == originalContent.stripWhiteSpace();
}

static std::optional<TextManipulationTokenInfo> tokenInfo(Node* node)
{
    if (!node)
        return std::nullopt;

    TextManipulationTokenInfo result;
    result.documentURL = node->document().url();
    if (RefPtr element = is<Element>(node) ? downcast<Element>(node) : node->parentElement()) {
        result.tagName = element->tagName();
        if (element->hasAttributeWithoutSynchronization(HTMLNames::roleAttr))
            result.roleAttribute = element->attributeWithoutSynchronization(HTMLNames::roleAttr);
        if (RefPtr frame = node->document().frame(); frame && frame->view() && element->renderer()) {
            // FIXME: This doesn't account for overflow clip.
            auto elementRect = element->renderer()->absoluteAnchorRect();
            auto visibleContentRect = frame->view()->visibleContentRect();
            result.isVisible = visibleContentRect.intersects(enclosingIntRect(elementRect));
        }
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

    auto displayType = renderer->style().display();
    if (element.hasTagName(HTMLNames::liTag) || element.hasTagName(HTMLNames::aTag)) {
        if (displayType == DisplayType::Block || displayType == DisplayType::InlineBlock)
            return true;

        for (RefPtr parent = element.parentElement(); parent; parent = parent->parentElement()) {
            if (parent->hasTagName(HTMLNames::navTag) || role(*parent) == AccessibilityRole::LandmarkNavigation)
                return true;
        }
    }

    if (displayType == DisplayType::TableCell)
        return true;

    if (element.hasTagName(HTMLNames::spanTag) && displayType == DisplayType::InlineBlock)
        return true;

    if (displayType == DisplayType::Block && (element.hasTagName(HTMLNames::h1Tag) || element.hasTagName(HTMLNames::h2Tag) || element.hasTagName(HTMLNames::h3Tag)
        || element.hasTagName(HTMLNames::h4Tag) || element.hasTagName(HTMLNames::h5Tag) || element.hasTagName(HTMLNames::h6Tag)))
        return true;

    return false;
}

TextManipulationController::ManipulationUnit TextManipulationController::createUnit(const Vector<String>& text, Node& textNode)
{
    ManipulationUnit unit = { textNode, { } };
    for (auto& textEntry : text) {
        if (!textEntry.isNull())
            parse(unit, textEntry, textNode);
        else {
            if (unit.tokens.isEmpty())
                unit.firstTokenContainsDelimiter = true;
            unit.lastTokenContainsDelimiter = true;
        }
    }
    return unit;
}

bool TextManipulationController::shouldExcludeNodeBasedOnStyle(const Node& node)
{
    auto* style = node.renderStyle();
    if (!style)
        return false;

    auto& font = style->fontCascade().primaryFont();
    auto familyName = font.platformData().familyName();
    if (familyName.isEmpty())
        return false;

    auto iter = m_cachedFontFamilyExclusionResults.find(familyName);
    if (iter != m_cachedFontFamilyExclusionResults.end())
        return iter->value;

    // FIXME: We should reconsider whether a node should be excluded if the primary font
    // used to render the node changes, since this "icon font" heuristic may return a
    // different result.
    bool result = font.isProbablyOnlyUsedToRenderIcons();
    m_cachedFontFamilyExclusionResults.set(familyName, result);
    return result;
}

void TextManipulationController::parse(ManipulationUnit& unit, const String& text, Node& textNode)
{
    ExclusionRuleMatcher exclusionRuleMatcher(m_exclusionRules);
    bool isNodeExcluded = exclusionRuleMatcher.isExcluded(&textNode) || shouldExcludeNodeBasedOnStyle(textNode);
    size_t positionOfLastNonHTMLSpace = notFound;
    size_t startPositionOfCurrentToken = 0;
    size_t index = 0;
    for (; index < text.length(); ++index) {
        auto character = text[index];
        if (isTokenDelimiter(character)) {
            if (positionOfLastNonHTMLSpace != notFound && startPositionOfCurrentToken <= positionOfLastNonHTMLSpace) {
                auto stringForToken = text.substring(startPositionOfCurrentToken, positionOfLastNonHTMLSpace + 1 - startPositionOfCurrentToken);
                unit.tokens.append(TextManipulationToken { m_tokenIdentifier.generate(), stringForToken, tokenInfo(&textNode), isNodeExcluded });
                startPositionOfCurrentToken = positionOfLastNonHTMLSpace + 1;
            }

            while (index < text.length() && (isHTMLSpace(text[index]) || isInPrivateUseArea(text[index])))
                ++index;

            --index;

            auto stringForToken = text.substring(startPositionOfCurrentToken, index + 1 - startPositionOfCurrentToken);
            if (unit.tokens.isEmpty() && !unit.firstTokenContainsDelimiter)
                unit.firstTokenContainsDelimiter = true;
            unit.tokens.append(TextManipulationToken { m_tokenIdentifier.generate(), stringForToken, tokenInfo(&textNode), true });
            startPositionOfCurrentToken = index + 1;
            unit.lastTokenContainsDelimiter = true;
        } else if (isNotSpace(character)) {
            if (!isNodeExcluded)
                unit.areAllTokensExcluded = false;
            positionOfLastNonHTMLSpace = index;
        }
    }

    if (startPositionOfCurrentToken < text.length()) {
        auto stringForToken = text.substring(startPositionOfCurrentToken, index + 1 - startPositionOfCurrentToken);
        unit.tokens.append(TextManipulationToken { m_tokenIdentifier.generate(), stringForToken, tokenInfo(&textNode), isNodeExcluded });
        unit.lastTokenContainsDelimiter = false;
    }
}

void TextManipulationController::addItemIfPossible(Vector<ManipulationUnit>&& units)
{
    if (units.isEmpty())
        return;

    size_t index = 0;
    size_t end = units.size();
    while (index < units.size() && units[index].areAllTokensExcluded)
        ++index;

    while (end > 0 && units[end - 1].areAllTokensExcluded)
        --end;

    if (index == end)
        return;

    ASSERT(end);
    auto startPosition = firstPositionInOrBeforeNode(units[index].node.ptr());
    auto endPosition = positionAfterNode(units[end - 1].node.ptr());
    Vector<TextManipulationToken> tokens;
    for (; index < end; ++index)
        tokens.appendVector(WTFMove(units[index].tokens));

    addItem(ManipulationItemData { startPosition, endPosition, nullptr, nullQName(), WTFMove(tokens) });
}

void TextManipulationController::observeParagraphs(const Position& start, const Position& end)
{
    if (start.isNull() || end.isNull() || start.isOrphan() || end.isOrphan())
        return;

    RefPtr document { start.document() };
    ASSERT(document);
    // TextIterator's constructor may have updated the layout and executed arbitrary scripts.
    if (document != start.document() || document != end.document())
        return;

    Vector<ManipulationUnit> unitsInCurrentParagraph;
    Vector<Ref<Element>> enclosingItemBoundaryElements;
    ParagraphContentIterator iterator { start, end };
    for (; !iterator.atEnd(); iterator.advance()) {
        auto content = iterator.currentContent();
        auto* contentNode = content.node.get();
        ASSERT(contentNode);

        while (!enclosingItemBoundaryElements.isEmpty() && !enclosingItemBoundaryElements.last()->contains(contentNode)) {
            addItemIfPossible(std::exchange(unitsInCurrentParagraph, { }));
            enclosingItemBoundaryElements.removeLast();
        }

        if (m_manipulatedNodes.contains(*contentNode)) {
            addItemIfPossible(std::exchange(unitsInCurrentParagraph, { }));
            continue;
        }

        if (is<Element>(*contentNode)) {
            auto& currentElement = downcast<Element>(*contentNode);
            if (!content.isTextContent && canPerformTextManipulationByReplacingEntireTextContent(currentElement))
                addItem(ManipulationItemData { Position(), Position(), currentElement, nullQName(), { TextManipulationToken { m_tokenIdentifier.generate(), currentElement.textContent(), tokenInfo(&currentElement) } } });

            if (currentElement.hasAttributes()) {
                for (auto& attribute : currentElement.attributesIterator()) {
                    if (isAttributeForTextManipulation(attribute.name()))
                        addItem(ManipulationItemData { Position(), Position(), currentElement, attribute.name(), { TextManipulationToken { m_tokenIdentifier.generate(), attribute.value(), tokenInfo(&currentElement) } } });
                }
            }

            if (is<HTMLInputElement>(currentElement)) {
                auto& input = downcast<HTMLInputElement>(currentElement);
                if (shouldExtractValueForTextManipulation(input))
                    addItem(ManipulationItemData { { }, { }, currentElement, HTMLNames::valueAttr, { TextManipulationToken { m_tokenIdentifier.generate(), input.value(), tokenInfo(&currentElement) } } });
            }

            if (isEnclosingItemBoundaryElement(currentElement)) {
                addItemIfPossible(std::exchange(unitsInCurrentParagraph, { }));
                enclosingItemBoundaryElements.append(currentElement);
            }
        }

        if (content.isReplacedContent) {
            if (!unitsInCurrentParagraph.isEmpty())
                unitsInCurrentParagraph.append(ManipulationUnit { *contentNode, { TextManipulationToken { m_tokenIdentifier.generate(), "[]"_s, tokenInfo(content.node.get()), true } } });
            continue;
        }

        if (!content.isTextContent)
            continue;

        auto currentUnit = createUnit(content.text, *contentNode);
        if (currentUnit.firstTokenContainsDelimiter)
            addItemIfPossible(std::exchange(unitsInCurrentParagraph, { }));

        if (unitsInCurrentParagraph.isEmpty() && currentUnit.areAllTokensExcluded)
            continue;

        bool currentUnitEndsWithDelimiter = currentUnit.lastTokenContainsDelimiter;
        unitsInCurrentParagraph.append(WTFMove(currentUnit));

        if (currentUnitEndsWithDelimiter)
            addItemIfPossible(std::exchange(unitsInCurrentParagraph, { }));
    }

    addItemIfPossible(std::exchange(unitsInCurrentParagraph, { }));
}

void TextManipulationController::didUpdateContentForNode(Node& node)
{
    if (!m_manipulatedNodes.contains(node))
        return;

    scheduleObservationUpdate();

    m_manipulatedNodesWithNewContent.add(node);
}

void TextManipulationController::didAddOrCreateRendererForNode(Node& node)
{
    if (m_manipulatedNodes.contains(node))
        return;

    scheduleObservationUpdate();

    if (is<PseudoElement>(node)) {
        if (auto* host = downcast<PseudoElement>(node).hostElement())
            m_addedOrNewlyRenderedNodes.add(*host);
    } else
        m_addedOrNewlyRenderedNodes.add(node);
}

void TextManipulationController::scheduleObservationUpdate()
{
    if (m_didScheduleObservationUpdate)
        return;

    if (!m_document)
        return;

    m_didScheduleObservationUpdate = true;

    m_document->eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakThis = WeakPtr { *this }] {
        auto* controller = weakThis.get();
        if (!controller)
            return;

        controller->m_didScheduleObservationUpdate = false;

        HashSet<Ref<Node>> nodesToObserve;
        for (auto& text : controller->m_manipulatedNodesWithNewContent) {
            if (!controller->m_manipulatedNodes.contains(text))
                continue;
            controller->m_manipulatedNodes.remove(text);
            nodesToObserve.add(text);
        }
        controller->m_manipulatedNodesWithNewContent.clear();

        for (auto& node : controller->m_addedOrNewlyRenderedNodes)
            nodesToObserve.add(node);
        controller->m_addedOrNewlyRenderedNodes.clear();

        if (nodesToObserve.isEmpty())
            return;

        RefPtr<Node> commonAncestor;
        for (auto& node : nodesToObserve) {
            if (!node->isConnected())
                continue;

            if (RefPtr host = node->shadowHost(); is<HTMLInputElement>(host) && downcast<HTMLInputElement>(*host).lastChangeWasUserEdit())
                continue;

            if (!commonAncestor)
                commonAncestor = is<ContainerNode>(node) ? node.ptr() : node->parentNode();
            else if (!node->isDescendantOf(commonAncestor.get()))
                commonAncestor = commonInclusiveAncestor<ComposedTree>(*commonAncestor, node.get());
        }

        Position start;
        if (auto* element = downcast<Element>(commonAncestor.get())) {
            // Ensure to include the element in the range.
            if (canPerformTextManipulationByReplacingEntireTextContent(*element))
                start = positionBeforeNode(commonAncestor.get());
        }
        if (start.isNull())
            start = firstPositionInOrBeforeNode(commonAncestor.get());

        auto end = lastPositionInOrAfterNode(commonAncestor.get());
        controller->observeParagraphs(start, end);

        if (controller->m_items.isEmpty() && commonAncestor) {
            controller->m_manipulatedNodes.add(*commonAncestor);
            return;
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
    m_pendingItemsForCallback.append(TextManipulationItem {
        newID,
        itemData.tokens.map([](auto& token) { return token; })
    });
    m_items.add(newID, WTFMove(itemData));

    if (m_pendingItemsForCallback.size() >= itemCallbackBatchingSize)
        flushPendingItemsForCallback();
}

void TextManipulationController::flushPendingItemsForCallback()
{
    if (m_pendingItemsForCallback.isEmpty())
        return;

    m_callback(*m_document, m_pendingItemsForCallback);
    m_pendingItemsForCallback.clear();
}

auto TextManipulationController::completeManipulation(const Vector<WebCore::TextManipulationItem>& completionItems) -> Vector<ManipulationFailure>
{
    Vector<ManipulationFailure> failures;
    HashSet<Ref<Node>> containersWithoutVisualOverflowBeforeReplacement;
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

        auto failureOrNullopt = replace(itemData, itemToComplete.tokens, containersWithoutVisualOverflowBeforeReplacement);
        if (failureOrNullopt)
            failures.append(ManipulationFailure { identifier, i, *failureOrNullopt });
    }

    if (!containersWithoutVisualOverflowBeforeReplacement.isEmpty()) {
        if (m_document)
            m_document->updateLayoutIgnorePendingStylesheets();

        for (auto& container : containersWithoutVisualOverflowBeforeReplacement) {
            if (!is<StyledElement>(container))
                continue;

            auto& element = downcast<StyledElement>(container.get());
            auto* box = element.renderBox();
            if (!box || !box->hasVisualOverflow())
                continue;

            auto& style = box->style();
            if (style.width().isFixed() && style.height().isFixed() && !style.hasOutOfFlowPosition() && !style.hasClip()) {
                element.setInlineStyleProperty(CSSPropertyOverflowX, CSSValueHidden);
                element.setInlineStyleProperty(CSSPropertyOverflowY, CSSValueAuto);
            }
        }
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
    size_t i = 0;
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

auto TextManipulationController::replace(const ManipulationItemData& item, const Vector<TextManipulationToken>& replacementTokens, HashSet<Ref<Node>>& containersWithoutVisualOverflowBeforeReplacement) -> std::optional<ManipulationFailureType>
{
    if (item.start.isOrphan() || item.end.isOrphan())
        return ManipulationFailureType::ContentChanged;

    if (item.start.isNull() || item.end.isNull()) {
        RELEASE_ASSERT(item.tokens.size() == 1);
        RefPtr element = { item.element.get() };
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
        else if (item.attributeName == HTMLNames::valueAttr && is<HTMLInputElement>(*element))
            downcast<HTMLInputElement>(*element).setValue(newValue.toString());
        else
            element->setAttribute(item.attributeName, newValue.toAtomString());

        m_manipulatedNodes.add(*element);
        return std::nullopt;
    }

    size_t currentTokenIndex = 0;
    HashMap<TextManipulationTokenIdentifier, TokenExchangeData> tokenExchangeMap;
    RefPtr<Node> commonAncestor;
    RefPtr<Node> firstContentNode;
    RefPtr<Node> lastChildOfCommonAncestorInRange;
    HashSet<Ref<Node>> nodesToRemove;
    
    for (ParagraphContentIterator iterator { item.start, item.end }; !iterator.atEnd(); iterator.advance()) {
        auto content = iterator.currentContent();
        ASSERT(content.node);

        bool isReplacedOrTextContent = content.isReplacedContent || content.isTextContent;
        if (!isReplacedOrTextContent && is<ContainerNode>(*content.node) && !content.node->hasChildNodes() && content.text.isEmpty())
            continue;

        lastChildOfCommonAncestorInRange = content.node;
        nodesToRemove.add(*content.node);

        if (!isReplacedOrTextContent)
            continue;

        Vector<TextManipulationToken> tokensInCurrentNode;
        if (content.isReplacedContent) {
            if (currentTokenIndex >= item.tokens.size())
                return ManipulationFailureType::ContentChanged;

            tokensInCurrentNode.append(item.tokens[currentTokenIndex]);
        } else
            tokensInCurrentNode = createUnit(content.text, *content.node).tokens;

        bool isNodeIncluded = WTF::anyOf(tokensInCurrentNode, [] (auto& token) {
            return !token.isExcluded;
        });
        for (auto& token : tokensInCurrentNode) {
            if (currentTokenIndex >= item.tokens.size())
                return ManipulationFailureType::ContentChanged;

            auto& currentToken = item.tokens[currentTokenIndex++];
            bool isContentUnchanged = areEqualIgnoringLeadingAndTrailingWhitespaces(currentToken.content, token.content);
            if (!content.isReplacedContent && !isContentUnchanged)
                return ManipulationFailureType::ContentChanged;

            tokenExchangeMap.set(currentToken.identifier, TokenExchangeData { content.node.copyRef(), currentToken.content, !isNodeIncluded });
        }

        if (!firstContentNode)
            firstContentNode = content.node;

        auto parentNode = content.node->parentNode();
        if (!commonAncestor)
            commonAncestor = parentNode;
        else if (!parentNode->isDescendantOf(commonAncestor.get())) {
            commonAncestor = commonInclusiveAncestor<ComposedTree>(*commonAncestor, *parentNode);
            ASSERT(commonAncestor);
        }
    }

    if (!firstContentNode)
        return ManipulationFailureType::ContentChanged;

    while (lastChildOfCommonAncestorInRange && lastChildOfCommonAncestorInRange->parentNode() != commonAncestor)
        lastChildOfCommonAncestorInRange = lastChildOfCommonAncestorInRange->parentNode();

    if (!lastChildOfCommonAncestorInRange) {
        RELEASE_LOG_ERROR(TextManipulation, "%p - TextManipulationController::replace lastChildOfCommonAncestorInRange is null", this);
        return ManipulationFailureType::ContentChanged;
    }

    for (auto node = commonAncestor; node; node = node->parentNode())
        nodesToRemove.remove(*node);

    HashSet<Ref<Node>> reusedOriginalNodes;
    Vector<NodeInsertion> insertions;
    auto startTopDownPath = getPath(commonAncestor.get(), firstContentNode.get());
    while (!startTopDownPath.isEmpty()) {
        auto lastNode = startTopDownPath.last();
        ASSERT(is<ContainerNode>(lastNode));
        if (!downcast<ContainerNode>(lastNode.get()).hasOneChild())
            break;
        nodesToRemove.add(startTopDownPath.takeLast());
    }
    auto lastTopDownPath = startTopDownPath.map([&](auto node) -> NodeEntry {
        reusedOriginalNodes.add(node.copyRef());
        return { node, node };
    });

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
            replacementNode = Text::create(commonAncestor->document(), WTFMove(replacementText));

        auto topDownPath = getPath(commonAncestor.get(), originalNode);
        updateInsertions(lastTopDownPath, topDownPath, replacementNode.get(), reusedOriginalNodes, insertions);
    }

    RefPtr<Node> node = item.end.firstNode();
    if (node && lastChildOfCommonAncestorInRange->contains(node.get())) {
        auto topDownPath = getPath(commonAncestor.get(), node->parentNode());
        updateInsertions(lastTopDownPath, topDownPath, nullptr, reusedOriginalNodes, insertions);
    }
    while (lastChildOfCommonAncestorInRange->contains(node.get())) {
        Ref<Node> parentNode = *node->parentNode();
        while (!lastTopDownPath.isEmpty() && lastTopDownPath.last().first.ptr() != parentNode.ptr())
            lastTopDownPath.removeLast();

        insertions.append(NodeInsertion { lastTopDownPath.size() ? lastTopDownPath.last().second.ptr() : nullptr, *node, IsNodeManipulated::No });
        lastTopDownPath.append({ *node, *node });
        node = NodeTraversal::next(*node);
    }

    RefPtr<Node> insertionPointNode = lastChildOfCommonAncestorInRange->nextSibling();

    for (auto& node : nodesToRemove)
        node->remove();

    for (auto& insertion : insertions) {
        auto parentContainer = insertion.parentIfDifferentFromCommonAncestor;
        if (!parentContainer) {
            parentContainer = commonAncestor;
            parentContainer->insertBefore(insertion.child, insertionPointNode.get());
        } else
            parentContainer->appendChild(insertion.child);

        if (auto* box = parentContainer->renderBox()) {
            if (!box->hasVisualOverflow())
                containersWithoutVisualOverflowBeforeReplacement.add(*parentContainer);
        }

        if (insertion.isChildManipulated == IsNodeManipulated::Yes)
            m_manipulatedNodes.add(insertion.child.get());
    }

    return std::nullopt;
}

void TextManipulationController::removeNode(Node& node)
{
    m_manipulatedNodes.remove(node);
    m_textNodesWithNewRenderer.remove(node);
}

} // namespace WebCore
