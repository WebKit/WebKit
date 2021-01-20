/*
 * Copyright (C) 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ApplyStyleCommand.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "Frame.h"
#include "HTMLFontElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "Text.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include "VisibleUnits.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

static int toIdentifier(RefPtr<CSSValue>&& value)
{
    return is<CSSPrimitiveValue>(value) ? downcast<CSSPrimitiveValue>(*value).valueID() : 0;
}

static String& styleSpanClassString()
{
    static NeverDestroyed<String> styleSpanClassString(AppleStyleSpanClass);
    return styleSpanClassString;
}

bool isLegacyAppleStyleSpan(const Node* node)
{
    if (!is<HTMLSpanElement>(node))
        return false;

    return downcast<HTMLSpanElement>(*node).attributeWithoutSynchronization(classAttr) == styleSpanClassString();
}

static bool hasNoAttributeOrOnlyStyleAttribute(const StyledElement& element, ShouldStyleAttributeBeEmpty shouldStyleAttributeBeEmpty)
{
    if (!element.hasAttributes())
        return true;

    unsigned matchedAttributes = 0;
    if (element.attributeWithoutSynchronization(classAttr) == styleSpanClassString())
        matchedAttributes++;
    if (element.hasAttribute(styleAttr) && (shouldStyleAttributeBeEmpty == AllowNonEmptyStyleAttribute
        || !element.inlineStyle() || element.inlineStyle()->isEmpty()))
        matchedAttributes++;

    ASSERT(matchedAttributes <= element.attributeCount());
    return matchedAttributes == element.attributeCount();
}

bool isStyleSpanOrSpanWithOnlyStyleAttribute(const Element& element)
{
    if (!is<HTMLSpanElement>(element))
        return false;
    return hasNoAttributeOrOnlyStyleAttribute(downcast<HTMLSpanElement>(element), AllowNonEmptyStyleAttribute);
}

static inline bool isSpanWithoutAttributesOrUnstyledStyleSpan(const Element& element)
{
    if (!is<HTMLSpanElement>(element))
        return false;
    return hasNoAttributeOrOnlyStyleAttribute(downcast<HTMLSpanElement>(element), StyleAttributeShouldBeEmpty);
}

bool isEmptyFontTag(const Element* element, ShouldStyleAttributeBeEmpty shouldStyleAttributeBeEmpty)
{
    if (!is<HTMLFontElement>(element))
        return false;

    return hasNoAttributeOrOnlyStyleAttribute(downcast<HTMLFontElement>(*element), shouldStyleAttributeBeEmpty);
}

static Ref<HTMLElement> createFontElement(Document& document)
{
    return createHTMLElement(document, fontTag);
}

Ref<HTMLElement> createStyleSpanElement(Document& document)
{
    return createHTMLElement(document, spanTag);
}

ApplyStyleCommand::ApplyStyleCommand(Document& document, const EditingStyle* style, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document, editingAction)
    , m_style(style->copy())
    , m_propertyLevel(propertyLevel)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(Document& document, const EditingStyle* style, const Position& start, const Position& end, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document, editingAction)
    , m_style(style->copy())
    , m_propertyLevel(propertyLevel)
    , m_start(start)
    , m_end(end)
    , m_useEndingSelection(false)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(Ref<Element>&& element, bool removeOnly, EditAction editingAction)
    : CompositeEditCommand(element->document(), editingAction)
    , m_style(EditingStyle::create())
    , m_propertyLevel(PropertyDefault)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_styledInlineElement(WTFMove(element))
    , m_removeOnly(removeOnly)
{
}

ApplyStyleCommand::ApplyStyleCommand(Document& document, const EditingStyle* style, IsInlineElementToRemoveFunction isInlineElementToRemoveFunction, EditAction editingAction)
    : CompositeEditCommand(document, editingAction)
    , m_style(style->copy())
    , m_propertyLevel(PropertyDefault)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_removeOnly(true)
    , m_isInlineElementToRemoveFunction(isInlineElementToRemoveFunction)
{
}

void ApplyStyleCommand::updateStartEnd(const Position& newStart, const Position& newEnd)
{
    ASSERT(!(newStart > newEnd));

    if (!m_useEndingSelection && (newStart != m_start || newEnd != m_end))
        m_useEndingSelection = true;

    bool wasBaseFirst = startingSelection().isBaseFirst() || !startingSelection().isDirectional();
    setEndingSelection(VisibleSelection(wasBaseFirst ? newStart : newEnd, wasBaseFirst ? newEnd : newStart, VisiblePosition::defaultAffinity, endingSelection().isDirectional()));
    m_start = newStart;
    m_end = newEnd;
}

Position ApplyStyleCommand::startPosition()
{
    if (m_useEndingSelection)
        return endingSelection().start();
    
    return m_start;
}

Position ApplyStyleCommand::endPosition()
{
    if (m_useEndingSelection)
        return endingSelection().end();
    
    return m_end;
}

void ApplyStyleCommand::doApply()
{
    switch (m_propertyLevel) {
    case PropertyDefault: {
        // Apply the block-centric properties of the style.
        auto blockStyle = m_style->extractAndRemoveBlockProperties();
        if (!blockStyle->isEmpty())
            applyBlockStyle(blockStyle);
        // Apply any remaining styles to the inline elements.
        if (!m_style->isEmpty() || m_styledInlineElement || m_isInlineElementToRemoveFunction) {
            applyRelativeFontStyleChange(m_style.get());
            applyInlineStyle(*m_style);
        }
        break;
    }
    case ForceBlockProperties:
        // Force all properties to be applied as block styles.
        applyBlockStyle(*m_style);
        break;
    }
}

void ApplyStyleCommand::applyBlockStyle(EditingStyle& style)
{
    // Update document layout once before removing styles so that we avoid the expense of
    // updating before each and every call to check a computed style.
    document().updateLayoutIgnorePendingStylesheets();

    auto start = startPosition();
    auto end = endPosition();
    if (end < start)
        std::swap(start, end);

    VisiblePosition visibleStart(start);
    VisiblePosition visibleEnd(end);

    if (visibleStart.isNull() || visibleStart.isOrphan() || visibleEnd.isNull() || visibleEnd.isOrphan())
        return;

    // Save and restore the selection endpoints using their indices in the editable root, since
    // addBlockStyleIfNeeded may moveParagraphs, which can remove these endpoints.
    // Calculate start and end indices from the start of the tree that they're in.
    auto scopeRoot = makeRefPtr(highestEditableRoot(visibleStart.deepEquivalent()));
    if (!scopeRoot)
        return;

    auto scope = makeRangeSelectingNodeContents(*scopeRoot);
    auto range = *makeSimpleRange(visibleStart, visibleEnd);
    auto startIndex = characterCount({ scope.start, range.start }, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    auto endIndex = characterCount({ scope.start, range.end }, TextIteratorEmitsCharactersBetweenAllVisiblePositions);

    VisiblePosition paragraphStart(startOfParagraph(visibleStart));
    VisiblePosition nextParagraphStart(endOfParagraph(paragraphStart).next());
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        visibleEnd = visibleEnd.previous(CannotCrossEditingBoundary);
    VisiblePosition beyondEnd(endOfParagraph(visibleEnd).next());
    while (paragraphStart.isNotNull() && paragraphStart != beyondEnd) {
        StyleChange styleChange(&style, paragraphStart.deepEquivalent());
        if (styleChange.cssStyle() || m_removeOnly) {
            RefPtr<Node> block = enclosingBlock(paragraphStart.deepEquivalent().deprecatedNode());
            if (!m_removeOnly) {
                RefPtr<Node> newBlock = moveParagraphContentsToNewBlockIfNecessary(paragraphStart.deepEquivalent());
                if (newBlock)
                    block = newBlock;
            }
            ASSERT(!block || is<HTMLElement>(*block));
            if (is<HTMLElement>(block)) {
                removeCSSStyle(style, downcast<HTMLElement>(*block));
                if (!m_removeOnly)
                    addBlockStyle(styleChange, downcast<HTMLElement>(*block));
            }

            if (nextParagraphStart.isOrphan())
                nextParagraphStart = endOfParagraph(paragraphStart).next();
        }

        paragraphStart = nextParagraphStart;
        nextParagraphStart = endOfParagraph(paragraphStart).next();
    }
    
    auto startPosition = makeDeprecatedLegacyPosition(resolveCharacterLocation(scope, startIndex, TextIteratorEmitsCharactersBetweenAllVisiblePositions));
    auto endPosition = makeDeprecatedLegacyPosition(resolveCharacterLocation(scope, endIndex, TextIteratorEmitsCharactersBetweenAllVisiblePositions));
    updateStartEnd(startPosition, endPosition);
}

static Ref<MutableStyleProperties> copyStyleOrCreateEmpty(const StyleProperties* style)
{
    if (!style)
        return MutableStyleProperties::create();
    return style->mutableCopy();
}

void ApplyStyleCommand::applyRelativeFontStyleChange(EditingStyle* style)
{
    static const float MinimumFontSize = 0.1f;

    if (!style || !style->hasFontSizeDelta())
        return;

    auto start = startPosition();
    auto end = endPosition();
    if (end < start)
        std::swap(start, end);

    // Join up any adjacent text nodes.
    if (is<Text>(start.deprecatedNode())) {
        joinChildTextNodes(start.deprecatedNode()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }
    
    if (start.isNull() || end.isNull())
        return;

    if (is<Text>(*end.deprecatedNode()) && start.deprecatedNode()->parentNode() != end.deprecatedNode()->parentNode()) {
        joinChildTextNodes(end.deprecatedNode()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // Split the start text nodes if needed to apply style.
    if (isValidCaretPositionInTextNode(start)) {
        splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    if (isValidCaretPositionInTextNode(end)) {
        splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // Calculate loop end point.
    // If the end node is before the start node (can only happen if the end node is
    // an ancestor of the start node), we gather nodes up to the next sibling of the end node
    Node* beyondEnd;
    ASSERT(start.deprecatedNode());
    ASSERT(end.deprecatedNode());
    if (start.deprecatedNode()->isDescendantOf(*end.deprecatedNode()))
        beyondEnd = NodeTraversal::nextSkippingChildren(*end.deprecatedNode());
    else
        beyondEnd = NodeTraversal::next(*end.deprecatedNode());
    
    start = start.upstream(); // Move upstream to ensure we do not add redundant spans.
    Node* startNode = start.deprecatedNode();

    // Make sure we're not already at the end or the next NodeTraversal::next() will traverse past it.
    if (startNode == beyondEnd)
        return;

    if (is<Text>(*startNode) && start.deprecatedEditingOffset() >= caretMaxOffset(*startNode)) {
        // Move out of text node if range does not include its characters.
        startNode = NodeTraversal::next(*startNode);
        if (!startNode)
            return;
    }

    // Store away font size before making any changes to the document.
    // This ensures that changes to one node won't effect another.
    HashMap<Node*, float> startingFontSizes;
    for (Node* node = startNode; node != beyondEnd; node = NodeTraversal::next(*node)) {
        ASSERT(node);
        startingFontSizes.set(node, computedFontSize(node));
    }

    // These spans were added by us. If empty after font size changes, they can be removed.
    Vector<Ref<HTMLElement>> unstyledSpans;
    
    Node* lastStyledNode = nullptr;
    for (Node* node = startNode; node != beyondEnd; node = NodeTraversal::next(*node)) {
        ASSERT(node);
        RefPtr<HTMLElement> element;
        if (is<HTMLElement>(*node)) {
            // Only work on fully selected nodes.
            if (!nodeFullySelected(downcast<HTMLElement>(*node), start, end))
                continue;
            element = &downcast<HTMLElement>(*node);
        } else if (is<Text>(*node) && node->renderer() && node->parentNode() != lastStyledNode) {
            // Last styled node was not parent node of this text node, but we wish to style this
            // text node. To make this possible, add a style span to surround this text node.
            auto span = createStyleSpanElement(document());
            surroundNodeRangeWithElement(*node, *node, span.copyRef());
            element = WTFMove(span);
        }  else {
            // Only handle HTML elements and text nodes.
            continue;
        }
        lastStyledNode = node;

        RefPtr<MutableStyleProperties> inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
        float currentFontSize = computedFontSize(node);
        float desiredFontSize = std::max(MinimumFontSize, startingFontSizes.get(node) + style->fontSizeDelta());
        RefPtr<CSSValue> value = inlineStyle->getPropertyCSSValue(CSSPropertyFontSize);
        if (value) {
            element->removeInlineStyleProperty(CSSPropertyFontSize);
            currentFontSize = computedFontSize(node);
        }
        if (currentFontSize != desiredFontSize) {
            inlineStyle->setProperty(CSSPropertyFontSize, CSSValuePool::singleton().createValue(desiredFontSize, CSSUnitType::CSS_PX), false);
            setNodeAttribute(*element, styleAttr, inlineStyle->asText());
        }
        if (inlineStyle->isEmpty()) {
            removeNodeAttribute(*element, styleAttr);
            if (isSpanWithoutAttributesOrUnstyledStyleSpan(*element))
                unstyledSpans.append(element.releaseNonNull());
        }
    }

    for (auto& unstyledSpan : unstyledSpans)
        removeNodePreservingChildren(unstyledSpan);
}

static ContainerNode* dummySpanAncestorForNode(const Node* node)
{
    while (node && (!is<Element>(*node) || !isStyleSpanOrSpanWithOnlyStyleAttribute(downcast<Element>(*node))))
        node = node->parentNode();
    
    return node ? node->parentNode() : nullptr;
}

void ApplyStyleCommand::cleanupUnstyledAppleStyleSpans(ContainerNode* dummySpanAncestor)
{
    if (!dummySpanAncestor)
        return;

    // Dummy spans are created when text node is split, so that style information
    // can be propagated, which can result in more splitting. If a dummy span gets
    // cloned/split, the new node is always a sibling of it. Therefore, we scan
    // all the children of the dummy's parent

    Vector<Element*> toRemove;
    for (auto& child : childrenOfType<Element>(*dummySpanAncestor)) {
        if (isSpanWithoutAttributesOrUnstyledStyleSpan(child))
            toRemove.append(&child);
    }

    for (auto& element : toRemove)
        removeNodePreservingChildren(*element);
}

HTMLElement* ApplyStyleCommand::splitAncestorsWithUnicodeBidi(Node* node, bool before, WritingDirection allowedDirection)
{
    // We are allowed to leave the highest ancestor with unicode-bidi unsplit if it is unicode-bidi: embed and direction: allowedDirection.
    // In that case, we return the unsplit ancestor. Otherwise, we return 0.
    Element* block = enclosingBlock(node);
    if (!block || block == node)
        return 0;

    Node* highestAncestorWithUnicodeBidi = nullptr;
    Node* nextHighestAncestorWithUnicodeBidi = nullptr;
    int highestAncestorUnicodeBidi = 0;
    for (Node* n = node->parentNode(); n != block; n = n->parentNode()) {
        int unicodeBidi = toIdentifier(ComputedStyleExtractor(n).propertyValue(CSSPropertyUnicodeBidi));
        if (unicodeBidi && unicodeBidi != CSSValueNormal) {
            highestAncestorUnicodeBidi = unicodeBidi;
            nextHighestAncestorWithUnicodeBidi = highestAncestorWithUnicodeBidi;
            highestAncestorWithUnicodeBidi = n;
        }
    }

    if (!highestAncestorWithUnicodeBidi)
        return 0;

    HTMLElement* unsplitAncestor = nullptr;

    if (allowedDirection != WritingDirection::Natural && highestAncestorUnicodeBidi != CSSValueBidiOverride && is<HTMLElement>(*highestAncestorWithUnicodeBidi)) {
        auto highestAncestorDirection = EditingStyle::create(highestAncestorWithUnicodeBidi, EditingStyle::AllProperties)->textDirection();
        if (highestAncestorDirection && *highestAncestorDirection == allowedDirection) {
            if (!nextHighestAncestorWithUnicodeBidi)
                return downcast<HTMLElement>(highestAncestorWithUnicodeBidi);

            unsplitAncestor = downcast<HTMLElement>(highestAncestorWithUnicodeBidi);
            highestAncestorWithUnicodeBidi = nextHighestAncestorWithUnicodeBidi;
        }
    }

    // Split every ancestor through highest ancestor with embedding.
    RefPtr<Node> currentNode = node;
    while (currentNode) {
        RefPtr<Element> parent = downcast<Element>(currentNode->parentNode());
        if (before ? currentNode->previousSibling() : currentNode->nextSibling())
            splitElement(*parent, before ? *currentNode : *currentNode->nextSibling());
        if (parent == highestAncestorWithUnicodeBidi)
            break;
        currentNode = parent;
    }
    return unsplitAncestor;
}

void ApplyStyleCommand::removeEmbeddingUpToEnclosingBlock(Node* node, Node* unsplitAncestor)
{
    Element* block = enclosingBlock(node);
    if (!block || block == node)
        return;

    Node* parent = nullptr;
    for (Node* ancestor = node->parentNode(); ancestor != block && ancestor != unsplitAncestor; ancestor = parent) {
        parent = ancestor->parentNode();
        if (!is<StyledElement>(*ancestor))
            continue;

        StyledElement& element = downcast<StyledElement>(*ancestor);
        int unicodeBidi = toIdentifier(ComputedStyleExtractor(&element).propertyValue(CSSPropertyUnicodeBidi));
        if (!unicodeBidi || unicodeBidi == CSSValueNormal)
            continue;

        // FIXME: This code should really consider the mapped attribute 'dir', the inline style declaration,
        // and all matching style rules in order to determine how to best set the unicode-bidi property to 'normal'.
        // For now, it assumes that if the 'dir' attribute is present, then removing it will suffice, and
        // otherwise it sets the property in the inline style declaration.
        if (element.hasAttributeWithoutSynchronization(dirAttr)) {
            // FIXME: If this is a BDO element, we should probably just remove it if it has no
            // other attributes, like we (should) do with B and I elements.
            removeNodeAttribute(element, dirAttr);
        } else {
            RefPtr<MutableStyleProperties> inlineStyle = copyStyleOrCreateEmpty(element.inlineStyle());
            inlineStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
            inlineStyle->removeProperty(CSSPropertyDirection);
            setNodeAttribute(element, styleAttr, inlineStyle->asText());
            if (isSpanWithoutAttributesOrUnstyledStyleSpan(element))
                removeNodePreservingChildren(element);
        }
    }
}

static Node* highestEmbeddingAncestor(Node* startNode, Node* enclosingNode)
{
    for (Node* n = startNode; n && n != enclosingNode; n = n->parentNode()) {
        if (n->isHTMLElement() && toIdentifier(ComputedStyleExtractor(n).propertyValue(CSSPropertyUnicodeBidi)) == CSSValueEmbed)
            return n;
    }

    return 0;
}

void ApplyStyleCommand::applyInlineStyle(EditingStyle& style)
{
    RefPtr<ContainerNode> startDummySpanAncestor;
    RefPtr<ContainerNode> endDummySpanAncestor;

    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateLayoutIgnorePendingStylesheets();

    // adjust to the positions we want to use for applying style
    auto start = startPosition();
    auto end = endPosition();
    if (end < start)
        std::swap(start, end);

    // split the start node and containing element if the selection starts inside of it
    bool splitStart = isValidCaretPositionInTextNode(start);
    if (splitStart) {
        if (shouldSplitTextElement(start.deprecatedNode()->parentElement(), style))
            splitTextElementAtStart(start, end);
        else
            splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
        startDummySpanAncestor = dummySpanAncestorForNode(start.deprecatedNode());
    }

    // split the end node and containing element if the selection ends inside of it
    bool splitEnd = isValidCaretPositionInTextNode(end);
    if (splitEnd) {
        if (shouldSplitTextElement(end.deprecatedNode()->parentElement(), style))
            splitTextElementAtEnd(start, end);
        else
            splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
        endDummySpanAncestor = dummySpanAncestorForNode(end.deprecatedNode());
    }

    if (start.isNull() || end.isNull())
        return;

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    Position removeStart = start.upstream();
    auto textDirection = style.textDirection();
    RefPtr<EditingStyle> styleWithoutEmbedding;
    RefPtr<EditingStyle> embeddingStyle;
    if (textDirection.hasValue()) {
        // Leave alone an ancestor that provides the desired single level embedding, if there is one.
        auto* startUnsplitAncestor = splitAncestorsWithUnicodeBidi(start.deprecatedNode(), true, *textDirection);
        auto* endUnsplitAncestor = splitAncestorsWithUnicodeBidi(end.deprecatedNode(), false, *textDirection);
        removeEmbeddingUpToEnclosingBlock(start.deprecatedNode(), startUnsplitAncestor);
        removeEmbeddingUpToEnclosingBlock(end.deprecatedNode(), endUnsplitAncestor);

        // Avoid removing the dir attribute and the unicode-bidi and direction properties from the unsplit ancestors.
        Position embeddingRemoveStart = removeStart;
        if (startUnsplitAncestor && nodeFullySelected(*startUnsplitAncestor, removeStart, end))
            embeddingRemoveStart = positionInParentAfterNode(startUnsplitAncestor);

        Position embeddingRemoveEnd = end;
        if (endUnsplitAncestor && nodeFullySelected(*endUnsplitAncestor, removeStart, end))
            embeddingRemoveEnd = positionInParentBeforeNode(endUnsplitAncestor).downstream();

        if (embeddingRemoveEnd != removeStart || embeddingRemoveEnd != end) {
            styleWithoutEmbedding = style.copy();
            embeddingStyle = styleWithoutEmbedding->extractAndRemoveTextDirection();

            if (embeddingRemoveStart <= embeddingRemoveEnd)
                removeInlineStyle(*embeddingStyle, embeddingRemoveStart, embeddingRemoveEnd);
        }
    }

    removeInlineStyle(styleWithoutEmbedding ? *styleWithoutEmbedding : style, removeStart, end);
    start = startPosition();
    end = endPosition();
    if (start.isNull() || start.isOrphan() || end.isNull() || end.isOrphan())
        return;

    if (splitStart && mergeStartWithPreviousIfIdentical(start, end)) {
        start = startPosition();
        end = endPosition();
    }

    if (splitEnd) {
        mergeEndWithNextIfIdentical(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateLayoutIgnorePendingStylesheets();

    RefPtr<EditingStyle> styleToApply = &style;
    if (textDirection.hasValue()) {
        // Avoid applying the unicode-bidi and direction properties beneath ancestors that already have them.
        Node* embeddingStartNode = highestEmbeddingAncestor(start.deprecatedNode(), enclosingBlock(start.deprecatedNode()));
        Node* embeddingEndNode = highestEmbeddingAncestor(end.deprecatedNode(), enclosingBlock(end.deprecatedNode()));

        if (embeddingStartNode || embeddingEndNode) {
            Position embeddingApplyStart = embeddingStartNode ? positionInParentAfterNode(embeddingStartNode) : start;
            Position embeddingApplyEnd = embeddingEndNode ? positionInParentBeforeNode(embeddingEndNode) : end;
            ASSERT(embeddingApplyStart.isNotNull() && embeddingApplyEnd.isNotNull());

            if (!embeddingStyle) {
                styleWithoutEmbedding = style.copy();
                embeddingStyle = styleWithoutEmbedding->extractAndRemoveTextDirection();
            }
            fixRangeAndApplyInlineStyle(*embeddingStyle, embeddingApplyStart, embeddingApplyEnd);

            styleToApply = styleWithoutEmbedding;
        }
    }

    fixRangeAndApplyInlineStyle(*styleToApply, start, end);

    // Remove dummy style spans created by splitting text elements.
    cleanupUnstyledAppleStyleSpans(startDummySpanAncestor.get());
    if (endDummySpanAncestor != startDummySpanAncestor)
        cleanupUnstyledAppleStyleSpans(endDummySpanAncestor.get());
}

void ApplyStyleCommand::fixRangeAndApplyInlineStyle(EditingStyle& style, const Position& start, const Position& end)
{
    Node* startNode = start.deprecatedNode();

    if (start.deprecatedEditingOffset() >= caretMaxOffset(*startNode)) {
        startNode = NodeTraversal::next(*startNode);
        if (!startNode || end < firstPositionInOrBeforeNode(startNode))
            return;
    }

    Node* pastEndNode = end.deprecatedNode();
    if (end.deprecatedEditingOffset() >= caretMaxOffset(*pastEndNode))
        pastEndNode = NodeTraversal::nextSkippingChildren(*pastEndNode);

    // FIXME: Callers should perform this operation on a Range that includes the br
    // if they want style applied to the empty line.
    // FIXME: Should this be using startNode instead of start.deprecatedNode()?
    if (start == end && start.deprecatedNode()->hasTagName(brTag))
        pastEndNode = NodeTraversal::next(*start.deprecatedNode());

    // Start from the highest fully selected ancestor so that we can modify the fully selected node.
    // e.g. When applying font-size: large on <font color="blue">hello</font>, we need to include the font element in our run
    // to generate <font color="blue" size="4">hello</font> instead of <font color="blue"><font size="4">hello</font></font>
    auto range = *makeSimpleRange(start, end);
    auto* editableRoot = startNode->rootEditableElement();
    if (startNode != editableRoot) {
        while (editableRoot && startNode->parentNode() != editableRoot && isNodeVisiblyContainedWithin(*startNode->parentNode(), range))
            startNode = startNode->parentNode();
    }

    applyInlineStyleToNodeRange(style, *startNode, pastEndNode);
}

static bool containsNonEditableRegion(Node& node)
{
    if (!node.hasEditableStyle())
        return true;

    Node* sibling = NodeTraversal::nextSkippingChildren(node);
    for (Node* descendant = node.firstChild(); descendant && descendant != sibling; descendant = NodeTraversal::next(*descendant)) {
        if (!descendant->hasEditableStyle())
            return true;
    }

    return false;
}

struct InlineRunToApplyStyle {
    InlineRunToApplyStyle(Node* start, Node* end, Node* pastEndNode)
        : start(start)
        , end(end)
        , pastEndNode(pastEndNode)
    {
        ASSERT(start->parentNode() == end->parentNode());
    }

    bool startAndEndAreStillInDocument()
    {
        return start && end && start->isConnected() && end->isConnected();
    }

    RefPtr<Node> start;
    RefPtr<Node> end;
    RefPtr<Node> pastEndNode;
    Position positionForStyleComputation;
    RefPtr<Node> dummyElement;
    StyleChange change;
};

void ApplyStyleCommand::applyInlineStyleToNodeRange(EditingStyle& style, Node& startNode, Node* pastEndNode)
{
    if (m_removeOnly)
        return;

    document().updateLayoutIgnorePendingStylesheets();

    Vector<InlineRunToApplyStyle> runs;
    RefPtr<Node> node = &startNode;
    for (RefPtr<Node> next; node && node != pastEndNode; node = next) {
        next = NodeTraversal::next(*node);

        if (!node->renderer() || !node->hasEditableStyle())
            continue;
        
        if (!node->hasRichlyEditableStyle() && is<HTMLElement>(*node)) {
            // This is a plaintext-only region. Only proceed if it's fully selected.
            // pastEndNode is the node after the last fully selected node, so if it's inside node then
            // node isn't fully selected.
            if (pastEndNode && pastEndNode->isDescendantOf(*node))
                break;
            // Add to this element's inline style and skip over its contents.
            HTMLElement& element = downcast<HTMLElement>(*node);
            RefPtr<MutableStyleProperties> inlineStyle = copyStyleOrCreateEmpty(element.inlineStyle());
            if (MutableStyleProperties* otherStyle = style.style())
                inlineStyle->mergeAndOverrideOnConflict(*otherStyle);
            setNodeAttribute(element, styleAttr, inlineStyle->asText());
            next = NodeTraversal::nextSkippingChildren(*node);
            continue;
        }
        
        if (isBlock(node.get()))
            continue;
        
        if (node->hasChildNodes()) {
            if (node->contains(pastEndNode) || containsNonEditableRegion(*node) || !node->parentNode()->hasEditableStyle())
                continue;
            if (editingIgnoresContent(*node)) {
                next = NodeTraversal::nextSkippingChildren(*node);
                continue;
            }
        }

        Node* runStart = node.get();
        Node* runEnd = node.get();
        Node* sibling = node->nextSibling();
        while (sibling && sibling != pastEndNode && !sibling->contains(pastEndNode) && (!isBlock(sibling) || sibling->hasTagName(brTag)) && !containsNonEditableRegion(*sibling)) {
            runEnd = sibling;
            sibling = runEnd->nextSibling();
        }
        next = NodeTraversal::nextSkippingChildren(*runEnd);

        Node* pastEndNode = NodeTraversal::nextSkippingChildren(*runEnd);
        if (!shouldApplyInlineStyleToRun(style, runStart, pastEndNode))
            continue;

        runs.append(InlineRunToApplyStyle(runStart, runEnd, pastEndNode));
    }

    for (auto& run : runs) {
        removeConflictingInlineStyleFromRun(style, run.start, run.end, run.pastEndNode.get());
        if (run.startAndEndAreStillInDocument())
            run.positionForStyleComputation = positionToComputeInlineStyleChange(*run.start, run.dummyElement);
    }

    document().updateLayoutIgnorePendingStylesheets();

    for (auto& run : runs)
        run.change = StyleChange(&style, run.positionForStyleComputation);

    for (auto& run : runs) {
        if (run.dummyElement)
            removeNode(*run.dummyElement);
        if (run.startAndEndAreStillInDocument())
            applyInlineStyleChange(run.start.releaseNonNull(), run.end.releaseNonNull(), run.change, AddStyledElement);
    }
}

bool ApplyStyleCommand::isStyledInlineElementToRemove(Element* element) const
{
    return (m_styledInlineElement && element->hasTagName(m_styledInlineElement->tagQName()))
        || (m_isInlineElementToRemoveFunction && m_isInlineElementToRemoveFunction(element));
}

bool ApplyStyleCommand::shouldApplyInlineStyleToRun(EditingStyle& style, Node* runStart, Node* pastEndNode)
{
    ASSERT(runStart);

    for (Node* node = runStart; node && node != pastEndNode; node = NodeTraversal::next(*node)) {
        if (node->hasChildNodes())
            continue;
        // We don't consider m_isInlineElementToRemoveFunction here because we never apply style when m_isInlineElementToRemoveFunction is specified
        if (!style.styleIsPresentInComputedStyleOfNode(*node))
            return true;
        if (m_styledInlineElement && !enclosingElementWithTag(positionBeforeNode(node), m_styledInlineElement->tagQName()))
            return true;
    }
    return false;
}

void ApplyStyleCommand::removeConflictingInlineStyleFromRun(EditingStyle& style, RefPtr<Node>& runStart, RefPtr<Node>& runEnd, Node* pastEndNode)
{
    ASSERT(runStart && runEnd);
    RefPtr<Node> next = runStart;
    for (RefPtr<Node> node = next; node && node->isConnected() && node != pastEndNode; node = next) {
        if (editingIgnoresContent(*node)) {
            ASSERT(!node->contains(pastEndNode));
            next = NodeTraversal::nextSkippingChildren(*node);
        } else
            next = NodeTraversal::next(*node);

        if (!is<HTMLElement>(*node))
            continue;

        RefPtr<Node> previousSibling = node->previousSibling();
        RefPtr<Node> nextSibling = node->nextSibling();
        RefPtr<ContainerNode> parent = node->parentNode();
        removeInlineStyleFromElement(style, downcast<HTMLElement>(*node), RemoveAlways);
        if (!node->isConnected()) {
            // FIXME: We might need to update the start and the end of current selection here but need a test.
            if (runStart == node)
                runStart = previousSibling ? previousSibling->nextSibling() : parent->firstChild();
            if (runEnd == node)
                runEnd = nextSibling ? nextSibling->previousSibling() : parent->lastChild();
        }
    }
}

bool ApplyStyleCommand::removeInlineStyleFromElement(EditingStyle& style, HTMLElement& element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    if (!element.parentNode() || !isEditableNode(*element.parentNode()))
        return false;

    if (isStyledInlineElementToRemove(&element)) {
        if (mode == RemoveNone)
            return true;
        if (extractedStyle)
            extractedStyle->mergeInlineStyleOfElement(element, EditingStyle::OverrideValues);
        removeNodePreservingChildren(element);
        return true;
    }

    bool removed = false;
    if (removeImplicitlyStyledElement(style, element, mode, extractedStyle))
        removed = true;

    if (!element.isConnected())
        return removed;

    // If the node was converted to a span, the span may still contain relevant
    // styles which must be removed (e.g. <b style='font-weight: bold'>)
    if (removeCSSStyle(style, element, mode, extractedStyle))
        removed = true;

    return removed;
}
    
void ApplyStyleCommand::replaceWithSpanOrRemoveIfWithoutAttributes(HTMLElement& element)
{
    if (hasNoAttributeOrOnlyStyleAttribute(element, StyleAttributeShouldBeEmpty))
        removeNodePreservingChildren(element);
    else {
        HTMLElement* newSpanElement = replaceElementWithSpanPreservingChildrenAndAttributes(element);
        ASSERT_UNUSED(newSpanElement, newSpanElement && newSpanElement->isConnected());
    }
}

bool ApplyStyleCommand::removeImplicitlyStyledElement(EditingStyle& style, HTMLElement& element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    if (mode == RemoveNone) {
        ASSERT(!extractedStyle);
        return style.conflictsWithImplicitStyleOfElement(element) || style.conflictsWithImplicitStyleOfAttributes(element);
    }

    ASSERT(mode == RemoveIfNeeded || mode == RemoveAlways);
    if (style.conflictsWithImplicitStyleOfElement(element, extractedStyle, mode == RemoveAlways ? EditingStyle::ExtractMatchingStyle : EditingStyle::DoNotExtractMatchingStyle)) {
        replaceWithSpanOrRemoveIfWithoutAttributes(element);
        return true;
    }

    // unicode-bidi and direction are pushed down separately so don't push down with other styles
    Vector<QualifiedName> attributes;
    if (!style.extractConflictingImplicitStyleOfAttributes(element, extractedStyle ? EditingStyle::PreserveWritingDirection : EditingStyle::DoNotPreserveWritingDirection, extractedStyle, attributes, mode == RemoveAlways ? EditingStyle::ExtractMatchingStyle : EditingStyle::DoNotExtractMatchingStyle))
        return false;

    for (auto& attribute : attributes)
        removeNodeAttribute(element, attribute);

    if (isEmptyFontTag(&element) || isSpanWithoutAttributesOrUnstyledStyleSpan(element))
        removeNodePreservingChildren(element);

    return true;
}

bool ApplyStyleCommand::removeCSSStyle(EditingStyle& style, HTMLElement& element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    if (mode == RemoveNone)
        return style.conflictsWithInlineStyleOfElement(element);

    RefPtr<MutableStyleProperties> newInlineStyle;
    if (!style.conflictsWithInlineStyleOfElement(element, newInlineStyle, extractedStyle))
        return false;

    if (newInlineStyle->isEmpty())
        removeNodeAttribute(element, styleAttr);
    else
        setNodeAttribute(element, styleAttr, newInlineStyle->asText());

    if (isSpanWithoutAttributesOrUnstyledStyleSpan(element))
        removeNodePreservingChildren(element);

    return true;
}

HTMLElement* ApplyStyleCommand::highestAncestorWithConflictingInlineStyle(EditingStyle& style, Node* node)
{
    if (!node)
        return nullptr;

    HTMLElement* result = nullptr;
    Node* unsplittableElement = unsplittableElementForPosition(firstPositionInOrBeforeNode(node));

    for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
        if (is<HTMLElement>(*ancestor) && shouldRemoveInlineStyleFromElement(style, downcast<HTMLElement>(*ancestor)))
            result = downcast<HTMLElement>(ancestor);
        // Should stop at the editable root (cannot cross editing boundary) and
        // also stop at the unsplittable element to be consistent with other UAs
        if (ancestor == unsplittableElement)
            break;
    }

    return result;
}

void ApplyStyleCommand::applyInlineStyleToPushDown(Node& node, EditingStyle* style)
{
    node.document().updateStyleIfNeeded();

    if (!style || style->isEmpty() || !node.renderer() || is<HTMLIFrameElement>(node))
        return;

    RefPtr<EditingStyle> newInlineStyle = style;
    if (is<HTMLElement>(node) && downcast<HTMLElement>(node).inlineStyle()) {
        newInlineStyle = style->copy();
        newInlineStyle->mergeInlineStyleOfElement(downcast<HTMLElement>(node), EditingStyle::OverrideValues);
    }

    // Since addInlineStyleIfNeeded can't add styles to block-flow render objects, add style attribute instead.
    // FIXME: applyInlineStyleToRange should be used here instead.
    if ((node.renderer()->isRenderBlockFlow() || node.hasChildNodes()) && is<HTMLElement>(node)) {
        setNodeAttribute(downcast<HTMLElement>(node), styleAttr, newInlineStyle->style()->asText());
        return;
    }

    if (node.renderer()->isText() && static_cast<RenderText*>(node.renderer())->isAllCollapsibleWhitespace())
        return;
    if (node.renderer()->isBR() && !node.renderer()->style().preserveNewline())
        return;

    // We can't wrap node with the styled element here because new styled element will never be removed if we did.
    // If we modified the child pointer in pushDownInlineStyleAroundNode to point to new style element
    // then we fall into an infinite loop where we keep removing and adding styled element wrapping node.
    addInlineStyleIfNeeded(newInlineStyle.get(), node, node, DoNotAddStyledElement);
}

void ApplyStyleCommand::pushDownInlineStyleAroundNode(EditingStyle& style, Node* targetNode)
{
    HTMLElement* highestAncestor = highestAncestorWithConflictingInlineStyle(style, targetNode);
    if (!highestAncestor)
        return;

    // The outer loop is traversing the tree vertically from highestAncestor to targetNode
    RefPtr<Node> current = highestAncestor;
    // Along the way, styled elements that contain targetNode are removed and accumulated into elementsToPushDown.
    // Each child of the removed element, exclusing ancestors of targetNode, is then wrapped by clones of elements in elementsToPushDown.
    Vector<Ref<Element>> elementsToPushDown;
    while (current && current != targetNode && current->contains(targetNode)) {
        auto currentChildren = collectChildNodes(*current);

        RefPtr<StyledElement> styledElement;
        if (is<StyledElement>(*current) && isStyledInlineElementToRemove(downcast<Element>(current.get()))) {
            styledElement = downcast<StyledElement>(current.get());
            elementsToPushDown.append(*styledElement);
        }

        auto styleToPushDown = EditingStyle::create();
        if (is<HTMLElement>(*current))
            removeInlineStyleFromElement(style, downcast<HTMLElement>(*current), RemoveIfNeeded, styleToPushDown.ptr());

        // The inner loop will go through children on each level
        // FIXME: we should aggregate inline child elements together so that we don't wrap each child separately.
        for (Ref<Node>& childRef : currentChildren) {
            Node& child = childRef;
            if (!child.parentNode())
                continue;
            if (!child.contains(targetNode) && elementsToPushDown.size()) {
                for (auto& element : elementsToPushDown) {
                    auto wrapper = element->cloneElementWithoutChildren(document());
                    wrapper->removeAttribute(styleAttr);
                    surroundNodeRangeWithElement(child, child, WTFMove(wrapper));
                }
            }

            // Apply style to all nodes containing targetNode and their siblings but NOT to targetNode
            // But if we've removed styledElement then always apply the style.
            if (&child != targetNode || styledElement)
                applyInlineStyleToPushDown(child, styleToPushDown.ptr());

            // We found the next node for the outer loop (contains targetNode)
            // When reached targetNode, stop the outer loop upon the completion of the current inner loop
            if (&child == targetNode || child.contains(targetNode))
                current = &child;
        }
    }
}

void ApplyStyleCommand::removeInlineStyle(EditingStyle& style, const Position& start, const Position& end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.anchorNode()->isConnected());
    ASSERT(end.anchorNode()->isConnected());
    ASSERT(start <= end);
    // FIXME: We should assert that start/end are not in the middle of a text node.

    Position pushDownStart = start.downstream();
    // If the pushDownStart is at the end of a text node, then this node is not fully selected.
    // Move it to the next deep quivalent position to avoid removing the style from this node.
    // e.g. if pushDownStart was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead.
    auto* pushDownStartContainer = pushDownStart.containerNode();
    if (is<Text>(pushDownStartContainer) && static_cast<unsigned>(pushDownStart.computeOffsetInContainerNode()) == downcast<Text>(*pushDownStartContainer).length())
        pushDownStart = nextVisuallyDistinctCandidate(pushDownStart);
    // If pushDownEnd is at the start of a text node, then this node is not fully selected.
    // Move it to the previous deep equivalent position to avoid removing the style from this node.
    Position pushDownEnd = end.upstream();
    auto* pushDownEndContainer = pushDownEnd.containerNode();
    if (is<Text>(pushDownEndContainer) && !pushDownEnd.computeOffsetInContainerNode())
        pushDownEnd = previousVisuallyDistinctCandidate(pushDownEnd);

    pushDownInlineStyleAroundNode(style, pushDownStart.deprecatedNode());
    pushDownInlineStyleAroundNode(style, pushDownEnd.deprecatedNode());

    // The s and e variables store the positions used to set the ending selection after style removal
    // takes place. This will help callers to recognize when either the start node or the end node
    // are removed from the document during the work of this function.
    // If pushDownInlineStyleAroundNode has pruned start.deprecatedNode() or end.deprecatedNode(),
    // use pushDownStart or pushDownEnd instead, which pushDownInlineStyleAroundNode won't prune.
    Position s = start.isNull() || start.isOrphan() ? pushDownStart : start;
    Position e = end.isNull() || end.isOrphan() ? pushDownEnd : end;

    RefPtr<Node> node = start.deprecatedNode();
    while (node) {
        RefPtr<Node> next;
        if (editingIgnoresContent(*node)) {
            ASSERT(node == end.deprecatedNode() || !node->contains(end.deprecatedNode()));
            next = NodeTraversal::nextSkippingChildren(*node);
        } else
            next = NodeTraversal::next(*node);

        if (is<HTMLElement>(*node) && nodeFullySelected(downcast<HTMLElement>(*node), start, end)) {
            Ref<HTMLElement> element = downcast<HTMLElement>(*node);
            RefPtr<Node> prev = NodeTraversal::previousPostOrder(element);
            RefPtr<Node> next = NodeTraversal::next(element);
            RefPtr<EditingStyle> styleToPushDown;
            RefPtr<Node> childNode;
            if (isStyledInlineElementToRemove(element.ptr())) {
                styleToPushDown = EditingStyle::create();
                childNode = element->firstChild();
            }

            removeInlineStyleFromElement(style, element, RemoveIfNeeded, styleToPushDown.get());
            if (!element->isConnected()) {
                if (s.deprecatedNode() == element.ptr()) {
                    // Since elem must have been fully selected, and it is at the start
                    // of the selection, it is clear we can set the new s offset to 0.
                    ASSERT(s.anchorType() == Position::PositionIsBeforeAnchor || s.offsetInContainerNode() <= 0);
                    s = firstPositionInOrBeforeNode(next.get());
                }
                if (e.deprecatedNode() == element.ptr()) {
                    // Since elem must have been fully selected, and it is at the end
                    // of the selection, it is clear we can set the new e offset to
                    // the max range offset of prev.
                    ASSERT(s.anchorType() == Position::PositionIsAfterAnchor || !offsetIsBeforeLastNodeOffset(s.offsetInContainerNode(), s.containerNode()));
                    e = lastPositionInOrAfterNode(prev.get());
                }
            }

            if (styleToPushDown) {
                for (; childNode; childNode = childNode->nextSibling())
                    applyInlineStyleToPushDown(*childNode, styleToPushDown.get());
            }
        }
        if (node == end.deprecatedNode())
            break;
        node = next.get();
    }

    updateStartEnd(s, e);
}

bool ApplyStyleCommand::nodeFullySelected(Element& element, const Position& start, const Position& end) const
{
    // The tree may have changed and Position::upstream() relies on an up-to-date layout.
    element.document().updateLayoutIgnorePendingStylesheets();
    return firstPositionInOrBeforeNode(&element) >= start && lastPositionInOrAfterNode(&element).upstream() <= end;
}

bool ApplyStyleCommand::nodeFullyUnselected(Element& element, const Position& start, const Position& end) const
{
    // The tree may have changed and Position::upstream() relies on an up-to-date layout.
    element.document().updateLayoutIgnorePendingStylesheets();
    return lastPositionInOrAfterNode(&element).upstream() < start || firstPositionInOrBeforeNode(&element) > end;
}

void ApplyStyleCommand::splitTextAtStart(const Position& start, const Position& end)
{
    ASSERT(is<Text>(start.containerNode()));

    Position newEnd;
    if (end.anchorType() == Position::PositionIsOffsetInAnchor && start.containerNode() == end.containerNode())
        newEnd = Position(end.containerText(), end.offsetInContainerNode() - start.offsetInContainerNode());
    else
        newEnd = end;

    RefPtr<Text> text = start.containerText();
    splitTextNode(*text, start.offsetInContainerNode());
    updateStartEnd(firstPositionInNode(text.get()), newEnd);
}

void ApplyStyleCommand::splitTextAtEnd(const Position& start, const Position& end)
{
    ASSERT(is<Text>(end.containerNode()));

    bool shouldUpdateStart = start.anchorType() == Position::PositionIsOffsetInAnchor && start.containerNode() == end.containerNode();
    Text& text = downcast<Text>(*end.deprecatedNode());
    splitTextNode(text, end.offsetInContainerNode());

    Node* prevNode = text.previousSibling();
    if (!is<Text>(prevNode))
        return;

    Position newStart = shouldUpdateStart ? Position(downcast<Text>(prevNode), start.offsetInContainerNode()) : start;
    updateStartEnd(newStart, lastPositionInNode(prevNode));
}

void ApplyStyleCommand::splitTextElementAtStart(const Position& start, const Position& end)
{
    ASSERT(is<Text>(start.containerNode()));

    Position newEnd;
    if (start.containerNode() == end.containerNode())
        newEnd = Position(end.containerText(), end.offsetInContainerNode() - start.offsetInContainerNode());
    else
        newEnd = end;

    splitTextNodeContainingElement(*start.containerText(), start.offsetInContainerNode());
    updateStartEnd(positionBeforeNode(start.containerNode()), newEnd);
}

void ApplyStyleCommand::splitTextElementAtEnd(const Position& start, const Position& end)
{
    ASSERT(is<Text>(end.containerNode()));

    bool shouldUpdateStart = start.containerNode() == end.containerNode();
    splitTextNodeContainingElement(*end.containerText(), end.offsetInContainerNode());

    Node* parentElement = end.containerNode()->parentNode();
    if (!parentElement || !parentElement->previousSibling())
        return;
    Node* firstTextNode = parentElement->previousSibling()->lastChild();
    if (!is<Text>(firstTextNode))
        return;

    Position newStart = shouldUpdateStart ? Position(downcast<Text>(firstTextNode), start.offsetInContainerNode()) : start;
    updateStartEnd(newStart, positionAfterNode(firstTextNode));
}

bool ApplyStyleCommand::shouldSplitTextElement(Element* element, EditingStyle& style)
{
    if (!is<HTMLElement>(element))
        return false;

    return shouldRemoveInlineStyleFromElement(style, downcast<HTMLElement>(*element));
}

bool ApplyStyleCommand::isValidCaretPositionInTextNode(const Position& position)
{
    Node* node = position.containerNode();
    if (position.anchorType() != Position::PositionIsOffsetInAnchor || !is<Text>(node))
        return false;
    int offsetInText = position.offsetInContainerNode();
    return offsetInText > caretMinOffset(*node) && offsetInText < caretMaxOffset(*node);
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position& start, const Position& end)
{
    auto* startNode = start.containerNode();
    if (start.computeOffsetInContainerNode())
        return false;

    if (isAtomicNode(startNode)) {
        // note: prior siblings could be unrendered elements. it's silly to miss the
        // merge opportunity just for that.
        if (startNode->previousSibling())
            return false;

        startNode = startNode->parentNode();
    }

    auto* previousSibling = startNode->previousSibling();
    if (!previousSibling || !areIdenticalElements(*startNode, *previousSibling))
        return false;

    auto& previousElement = downcast<Element>(*previousSibling);
    auto& element = downcast<Element>(*startNode);
    auto* startChild = element.firstChild();
    ASSERT(startChild);
    mergeIdenticalElements(previousElement, element);

    // FIXME: Inconsistent that we use computeOffsetInContainerNode for start, but deprecatedEditingOffset for end.
    unsigned startOffset = startChild->computeNodeIndex();
    unsigned endOffset = end.deprecatedEditingOffset() + (startNode == end.deprecatedNode() ? startOffset : 0);
    updateStartEnd({ startNode, startOffset, Position::PositionIsOffsetInAnchor },
        { end.deprecatedNode(), endOffset, Position::PositionIsOffsetInAnchor });
    return true;
}

bool ApplyStyleCommand::mergeEndWithNextIfIdentical(const Position& start, const Position& end)
{
    Node* endNode = end.containerNode();

    if (isAtomicNode(endNode)) {
        int endOffset = end.computeOffsetInContainerNode();
        if (offsetIsBeforeLastNodeOffset(endOffset, endNode) || end.deprecatedNode()->nextSibling())
            return false;

        endNode = end.deprecatedNode()->parentNode();
    }

    if (endNode->hasTagName(brTag))
        return false;

    Node* nextSibling = endNode->nextSibling();
    if (!nextSibling || !areIdenticalElements(*endNode, *nextSibling))
        return false;

    auto& nextElement = downcast<Element>(*nextSibling);
    auto& element = downcast<Element>(*endNode);
    Node* nextChild = nextElement.firstChild();

    mergeIdenticalElements(element, nextElement);

    bool shouldUpdateStart = start.containerNode() == endNode;
    unsigned endOffset = nextChild ? nextChild->computeNodeIndex() : nextElement.countChildNodes();
    updateStartEnd(shouldUpdateStart ? Position(&nextElement, start.offsetInContainerNode(), Position::PositionIsOffsetInAnchor) : start,
        { &nextElement, endOffset, Position::PositionIsOffsetInAnchor });
    return true;
}

void ApplyStyleCommand::surroundNodeRangeWithElement(Node& startNode, Node& endNode, Ref<Element>&& elementToInsert)
{
    Ref<Node> protectedStartNode = startNode;
    Ref<Element> element = WTFMove(elementToInsert);

    insertNodeBefore(element.copyRef(), startNode);

    RefPtr<Node> node = &startNode;
    while (node) {
        RefPtr<Node> next = node->nextSibling();
        if (isEditableNode(*node)) {
            removeNode(*node);
            appendNode(*node, element.copyRef());
        }
        if (node == &endNode)
            break;
        node = next;
    }

    RefPtr<Node> nextSibling = element->nextSibling();
    RefPtr<Node> previousSibling = element->previousSibling();

    if (nextSibling && nextSibling->hasEditableStyle() && areIdenticalElements(element, *nextSibling))
        mergeIdenticalElements(element, downcast<Element>(*nextSibling));

    if (is<Element>(previousSibling) && previousSibling->hasEditableStyle()) {
        auto* mergedElement = previousSibling->nextSibling();
        ASSERT(mergedElement);
        if (mergedElement->hasEditableStyle() && areIdenticalElements(*previousSibling, *mergedElement))
            mergeIdenticalElements(downcast<Element>(*previousSibling), downcast<Element>(*mergedElement));
    }

    // FIXME: We should probably call updateStartEnd if the start or end was in the node
    // range so that the endingSelection() is canonicalized.  See the comments at the end of
    // VisibleSelection::validate().
}

static String joinWithSpace(const String& a, const String& b)
{
    if (a.isEmpty())
        return b;
    if (b.isEmpty())
        return a;
    return makeString(a, ' ', b);
}

void ApplyStyleCommand::addBlockStyle(const StyleChange& styleChange, HTMLElement& block)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for inline content.
    ASSERT(styleChange.cssStyle());
    setNodeAttribute(block, styleAttr, joinWithSpace(styleChange.cssStyle()->asText(), block.getAttribute(styleAttr)));
}

void ApplyStyleCommand::addInlineStyleIfNeeded(EditingStyle* style, Node& start, Node& end, EAddStyledElement addStyledElement)
{
    if (!start.isConnected() || !end.isConnected())
        return;

    Ref<Node> protectedStart = start;
    RefPtr<Node> dummyElement;
    StyleChange styleChange(style, positionToComputeInlineStyleChange(start, dummyElement));

    if (dummyElement)
        removeNode(*dummyElement);

    applyInlineStyleChange(start, end, styleChange, addStyledElement);
}

Position ApplyStyleCommand::positionToComputeInlineStyleChange(Node& startNode, RefPtr<Node>& dummyElement)
{
    // It's okay to obtain the style at the startNode because we've removed all relevant styles from the current run.
    if (!is<Element>(startNode)) {
        dummyElement = createStyleSpanElement(document());
        insertNodeAt(*dummyElement, positionBeforeNode(&startNode));
        return firstPositionInOrBeforeNode(dummyElement.get());
    }

    return firstPositionInOrBeforeNode(&startNode);
}

void ApplyStyleCommand::applyInlineStyleChange(Node& passedStart, Node& passedEnd, StyleChange& styleChange, EAddStyledElement addStyledElement)
{
    RefPtr<Node> startNode = &passedStart;
    RefPtr<Node> endNode = &passedEnd;
    ASSERT(startNode->isConnected());
    ASSERT(endNode->isConnected());

    // Find appropriate font and span elements top-down.
    HTMLFontElement* fontContainer = nullptr;
    HTMLElement* styleContainer = nullptr;
    while (startNode == endNode) {
        if (is<HTMLElement>(*startNode)) {
            auto& container = downcast<HTMLElement>(*startNode);
            if (is<HTMLFontElement>(container))
                fontContainer = &downcast<HTMLFontElement>(container);
            if (is<HTMLSpanElement>(container) || (!is<HTMLSpanElement>(styleContainer) && container.hasChildNodes()))
                styleContainer = &container;
        }
        auto* startNodeFirstChild = startNode->firstChild();
        if (!startNodeFirstChild)
            break;
        endNode = startNode->lastChild();
        startNode = startNodeFirstChild;
    }

    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        if (fontContainer) {
            if (styleChange.applyFontColor())
                setNodeAttribute(*fontContainer, colorAttr, styleChange.fontColor());
            if (styleChange.applyFontFace())
                setNodeAttribute(*fontContainer, faceAttr, styleChange.fontFace());
            if (styleChange.applyFontSize())
                setNodeAttribute(*fontContainer, sizeAttr, styleChange.fontSize());
        } else {
            auto fontElement = createFontElement(document());
            if (styleChange.applyFontColor())
                fontElement->setAttributeWithoutSynchronization(colorAttr, styleChange.fontColor());
            if (styleChange.applyFontFace())
                fontElement->setAttributeWithoutSynchronization(faceAttr, styleChange.fontFace());
            if (styleChange.applyFontSize())
                fontElement->setAttributeWithoutSynchronization(sizeAttr, styleChange.fontSize());
            surroundNodeRangeWithElement(*startNode, *endNode, WTFMove(fontElement));
        }
    }

    if (auto styleToMerge = styleChange.cssStyle()) {
        if (styleContainer) {
            if (auto existingStyle = styleContainer->inlineStyle()) {
                auto inlineStyle = EditingStyle::create(existingStyle);
                inlineStyle->overrideWithStyle(*styleToMerge);
                setNodeAttribute(*styleContainer, styleAttr, inlineStyle->style()->asText());
            } else
                setNodeAttribute(*styleContainer, styleAttr, styleToMerge->asText());
        } else {
            auto styleElement = createStyleSpanElement(document());
            styleElement->setAttribute(styleAttr, styleToMerge->asText());
            surroundNodeRangeWithElement(*startNode, *endNode, WTFMove(styleElement));
        }
    }

    if (styleChange.applyBold())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), bTag));

    if (styleChange.applyItalic())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), iTag));

    if (styleChange.applyUnderline())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), uTag));

    if (styleChange.applyLineThrough())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), strikeTag));

    if (styleChange.applySubscript())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), subTag));
    else if (styleChange.applySuperscript())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), supTag));

    if (m_styledInlineElement && addStyledElement == AddStyledElement)
        surroundNodeRangeWithElement(*startNode, *endNode, m_styledInlineElement->cloneElementWithoutChildren(document()));
}

float ApplyStyleCommand::computedFontSize(Node* node)
{
    if (!node)
        return 0;

    auto value = ComputedStyleExtractor(node).propertyValue(CSSPropertyFontSize);
    if (!value)
        return 0;
    return downcast<CSSPrimitiveValue>(*value).floatValue(CSSUnitType::CSS_PX);
}

void ApplyStyleCommand::joinChildTextNodes(Node* node, const Position& start, const Position& end)
{
    if (!node)
        return;

    Position newStart = start;
    Position newEnd = end;

    Vector<Ref<Text>> textNodes;
    for (Text* textNode = TextNodeTraversal::firstChild(*node); textNode; textNode = TextNodeTraversal::nextSibling(*textNode))
        textNodes.append(*textNode);

    for (auto& childText : textNodes) {
        Node* next = childText->nextSibling();
        if (!is<Text>(next))
            continue;
    
        Text& nextText = downcast<Text>(*next);
        if (start.anchorType() == Position::PositionIsOffsetInAnchor && next == start.containerNode())
            newStart = Position(childText.ptr(), childText->length() + start.offsetInContainerNode());
        if (end.anchorType() == Position::PositionIsOffsetInAnchor && next == end.containerNode())
            newEnd = Position(childText.ptr(), childText->length() + end.offsetInContainerNode());
        String textToMove = nextText.data();
        insertTextIntoNode(childText, childText->length(), textToMove);
        removeNode(*next);
        // don't move child node pointer. it may want to merge with more text nodes.
    }

    updateStartEnd(newStart, newEnd);
}

}
