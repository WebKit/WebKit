/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ApplyBlockPresentationIntentCommand.h"

#include "CSSSerializationContext.h"
#include "DocumentFragment.h"
#include "Editing.h"
#include "EditingInlines.h"
#include "ElementChildIteratorInlines.h"
#include "FormatBlockCommand.h"
#include "HTMLElement.h"
#include "HTMLHRElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "IndentOutdentCommand.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include "ReplaceSelectionCommand.h"
#include "SimpleRange.h"
#include "StylePropertiesInlines.h"
#include "StyledElement.h"
#include "Text.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibleUnits.h"
#include <wtf/Borrow.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace HTMLNames;

static InsertListCommand::Type listTypeForList(const HTMLElement& list)
{
    return is<HTMLOListElement>(list) ? InsertListCommand::Type::OrderedList : InsertListCommand::Type::UnorderedList;
}

static const HTMLQualifiedName& listTagForType(InsertListCommand::Type type)
{
    return type == InsertListCommand::Type::OrderedList ? olTag : ulTag;
}

static std::optional<InsertListCommand::Type> listTypeForKind(BlockPresentationIntent::Kind kind)
{
    switch (kind) {
    case BlockPresentationIntent::Kind::OrderedList:
        return InsertListCommand::Type::OrderedList;
    case BlockPresentationIntent::Kind::UnorderedList:
        return InsertListCommand::Type::UnorderedList;
    default:
        return std::nullopt;
    }
}

// Whether every paragraph in the selection is in a list with the tag, the only case where InsertListCommand takes
// paragraphs out of a list rather than putting them in one.
static bool isEverySelectedParagraphInListWithTag(const VisibleSelection& selection, const HTMLQualifiedName& listTag)
{
    auto lastParagraph = startOfParagraph(selection.visibleEnd());
    for (auto paragraph = startOfParagraph(selection.visibleStart()); paragraph.isNotNull() && paragraph <= lastParagraph; paragraph = startOfNextParagraph(paragraph)) {
        RefPtr list = enclosingList(protect(paragraph.deepEquivalent().containerNode()).get());
        if (!list || !list->hasTagName(listTag))
            return false;
        if (paragraph == lastParagraph)
            return true;
    }
    return false;
}

static unsigned listDepthOfSelectedParagraphs(const VisibleSelection& selection)
{
    unsigned depth = 0;
    auto lastParagraph = startOfParagraph(selection.visibleEnd());
    for (auto paragraph = startOfParagraph(selection.visibleStart()); paragraph.isNotNull() && paragraph <= lastParagraph; paragraph = startOfNextParagraph(paragraph)) {
        unsigned paragraphDepth = 0;
        for (RefPtr list = enclosingList(protect(paragraph.deepEquivalent().containerNode()).get()); list; list = enclosingList(list.get()))
            ++paragraphDepth;
        depth = std::max(depth, paragraphDepth);
        if (paragraph == lastParagraph)
            break;
    }
    return depth;
}

static bool isInMailBlockquote(Node& node)
{
    return !!enclosingNodeOfType(firstPositionInOrBeforeNode(&node), isMailBlockquote, EditingBoundaryCrossingRule::CanCross);
}

static std::optional<QualifiedName> headingTagForLevel(uint8_t level)
{
    switch (level) {
    case 1:
        return h1Tag;
    case 2:
        return h2Tag;
    case 3:
        return h3Tag;
    case 4:
        return h4Tag;
    case 5:
        return h5Tag;
    case 6:
        return h6Tag;
    default:
        return std::nullopt;
    }
}

static std::optional<QualifiedName> blockTagForIntent(const BlockPresentationIntent& intent)
{
    switch (intent.kind) {
    case BlockPresentationIntent::Kind::Paragraph:
        return pTag;
    case BlockPresentationIntent::Kind::Header:
        return headingTagForLevel(intent.headerLevel);
    case BlockPresentationIntent::Kind::CodeBlock:
        return preTag;
    case BlockPresentationIntent::Kind::BlockQuote:
        return blockquoteTag;
    case BlockPresentationIntent::Kind::OrderedList:
    case BlockPresentationIntent::Kind::UnorderedList:
    case BlockPresentationIntent::Kind::ListItem:
    case BlockPresentationIntent::Kind::ThematicBreak:
        return std::nullopt;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

// Pasted rich text carries its source's look inline. A block intent's look comes from the page's style sheet, so these go.
static constexpr std::array blockPresentationProperties {
    CSSPropertyID::Font, CSSPropertyID::FontSize, CSSPropertyID::FontFamily, CSSPropertyID::FontWeight, CSSPropertyID::LineHeight, CSSPropertyID::Margin,
};
static constexpr std::array inlinePresentationProperties {
    CSSPropertyID::Font, CSSPropertyID::FontSize, CSSPropertyID::FontFamily, CSSPropertyID::LineHeight,
};

ApplyBlockPresentationIntentCommand::ApplyBlockPresentationIntentCommand(Ref<Document>&& document, const BlockPresentationIntent& intent)
    : CompositeEditCommand(WTF::move(document))
    , m_intent(intent)
{
}

EditAction ApplyBlockPresentationIntentCommand::editingAction() const
{
    switch (m_intent.kind) {
    case BlockPresentationIntent::Kind::OrderedList:
        return EditAction::InsertOrderedList;
    case BlockPresentationIntent::Kind::UnorderedList:
        return EditAction::InsertUnorderedList;
    case BlockPresentationIntent::Kind::ThematicBreak:
        return EditAction::Insert;
    default:
        return EditAction::FormatBlock;
    }
}

Vector<Ref<HTMLElement>> ApplyBlockPresentationIntentCommand::blocksInSelectionWithTag(const QualifiedName& tagName) const
{
    Vector<Ref<HTMLElement>> blocks;
    auto addBlock = [&](Node& node) {
        RefPtr element = dynamicDowncast<HTMLElement>(node);
        if (!element || element->tagQName() != tagName || !element->hasEditableStyle())
            return;
        if (!blocks.containsIf([&](auto& block) { return block.ptr() == element; }))
            blocks.append(element.releaseNonNull());
    };

    if (RefPtr enclosingBlock = enclosingElementWithTag(endingSelection().start(), tagName))
        addBlock(*enclosingBlock);
    if (auto range = endingSelection().firstRange()) {
        for (Ref node : intersectingNodes(*range))
            addBlock(node);
    }
    return blocks;
}

// RemoveNodePreservingChildrenCommand doesn't update the ending selection, so this keeps it on the element's children
// rather than on the removed element.
void ApplyBlockPresentationIntentCommand::removeElementPreservingChildren(HTMLElement& element)
{
    auto updatePosition = [&](const Position& position) {
        RefPtr parent = element.parentNode();
        if (position.anchorType() == Position::PositionIsOffsetInAnchor && position.containerNode() == parent && static_cast<unsigned>(position.offsetInContainerNode()) > element.computeNodeIndex())
            return Position { parent, position.offsetInContainerNode() + element.countChildNodes() - 1, Position::PositionIsOffsetInAnchor };
        if (position.anchorNode() != &element)
            return position;
        unsigned offsetInElement = 0;
        switch (position.anchorType()) {
        case Position::PositionIsOffsetInAnchor:
            offsetInElement = position.offsetInContainerNode();
            break;
        case Position::PositionIsBeforeAnchor:
        case Position::PositionIsBeforeChildren:
            break;
        case Position::PositionIsAfterAnchor:
        case Position::PositionIsAfterChildren:
            offsetInElement = element.countChildNodes();
            break;
        }
        return Position { parent, element.computeNodeIndex() + offsetInElement, Position::PositionIsOffsetInAnchor };
    };

    auto selection = endingSelection();
    auto anchor = updatePosition(selection.anchor());
    auto focus = updatePosition(selection.focus());
    removeNodePreservingChildren(element);
    setEndingSelection(VisibleSelection { anchor, focus, selection.affinity(), selection.directionality() });
}

void ApplyBlockPresentationIntentCommand::wrapContentsInCode(HTMLElement& preElement)
{
    RefPtr codeElement = codeElementOfCodeBlock(preElement);
    if (!codeElement) {
        codeElement = HTMLElement::create(codeTag, protect(document()));
        RefPtr firstChild = preElement.firstChild();
        appendNode(Ref { *codeElement }, preElement);
        moveRemainingSiblingsToNewParent(firstChild.get(), codeElement.get(), *codeElement);
    }

    // The CommonMark form of a language hint.
    if (!m_intent.languageHint.isEmpty())
        setNodeAttribute(*codeElement, classAttr, AtomString { makeString("language-"_s, m_intent.languageHint) });
}

bool ApplyBlockPresentationIntentCommand::removeCodeElementsPreservingChildren()
{
    bool hadCodeBlock = false;
    for (Ref preElement : blocksInSelectionWithTag(preTag)) {
        if (RefPtr codeElement = codeElementOfCodeBlock(preElement)) {
            removeElementPreservingChildren(*codeElement);
            hadCodeBlock = true;
        }
    }
    return hadCodeBlock;
}

void ApplyBlockPresentationIntentCommand::putSelectedParagraphsInList(InsertListCommand::Type listType)
{
    auto& listTag = listTagForType(listType);
    if (isEverySelectedParagraphInListWithTag(endingSelection(), listTag))
        return;

    // FIXME: InsertListCommand::selectionHasListOfType() does not check the selection's last paragraph, so
    // InsertListCommand takes the selection out of a list of its type when every paragraph but the last is in one.
    // This works around it by first applying the other list type when the first paragraph is in a list of this one.
    // Remove that first InsertListCommand once selectionHasListOfType() checks the last paragraph.
    RefPtr list = enclosingList(protect(endingSelection().start().containerNode()).get());
    if (list && list->hasTagName(listTag)) {
        auto otherListType = listType == InsertListCommand::Type::OrderedList ? InsertListCommand::Type::UnorderedList : InsertListCommand::Type::OrderedList;
        applyCommandToComposite(InsertListCommand::create(protect(document()), otherListType));
    }
    applyCommandToComposite(InsertListCommand::create(protect(document()), listType));
}

void ApplyBlockPresentationIntentCommand::removeListElementsPreservingChildren()
{
    // Taking paragraphs out of a nested list puts them in the list around it.
    unsigned depth = listDepthOfSelectedParagraphs(endingSelection());
    for (unsigned pass = 0; pass < depth && listDepthOfSelectedParagraphs(endingSelection()); ++pass) {
        RefPtr list = enclosingList(protect(endingSelection().start().containerNode()).get());
        if (!list) {
            auto lists = blocksInSelectionWithTag(olTag);
            if (lists.isEmpty())
                lists = blocksInSelectionWithTag(ulTag);
            if (lists.isEmpty())
                return;
            list = lists.first().ptr();
        }

        // InsertListCommand only takes paragraphs out of a list when they are all in one of its type.
        auto listType = listTypeForList(*list);
        putSelectedParagraphsInList(listType);
        applyCommandToComposite(InsertListCommand::create(protect(document()), listType));
    }
}

// Mail's quotes of other messages stay, and a quote can only be removed if there is somewhere for its contents to go.
static bool canRemoveBlockquote(const HTMLElement& blockquote)
{
    RefPtr parent = blockquote.parentNode();
    return !isMailBlockquote(blockquote) && parent && parent->hasEditableStyle();
}

// IndentOutdentCommand outdents each paragraph out of its innermost quote, so this checks those quotes.
static bool canOutdentSelectedParagraphs(const VisibleSelection& selection)
{
    auto lastParagraph = startOfParagraph(selection.visibleEnd());
    for (auto paragraph = startOfParagraph(selection.visibleStart()); paragraph.isNotNull() && paragraph <= lastParagraph; paragraph = startOfNextParagraph(paragraph)) {
        RefPtr blockquote = dynamicDowncast<HTMLElement>(enclosingElementWithTag(paragraph.deepEquivalent(), blockquoteTag));
        if (blockquote && !canRemoveBlockquote(*blockquote))
            return false;
        if (paragraph == lastParagraph)
            break;
    }
    return true;
}

// The selected block quotes that outdenting can take the selection out of.
Vector<Ref<HTMLElement>> ApplyBlockPresentationIntentCommand::blockquotesToRemove() const
{
    auto blockquotes = blocksInSelectionWithTag(blockquoteTag);
    blockquotes.removeAllMatching([](auto& blockquote) {
        return !canRemoveBlockquote(blockquote.get());
    });
    return blockquotes;
}

void ApplyBlockPresentationIntentCommand::removeBlockquotesPreservingChildren()
{
    // Outdenting leaves one level of quoting, so no more passes are needed than the quotes are deep.
    unsigned depth = 0;
    for (Ref blockquote : blockquotesToRemove()) {
        unsigned blockquoteDepth = 0;
        for (RefPtr<Element> element = blockquote.ptr(); element; element = element->parentElement()) {
            if (element->hasTagName(blockquoteTag))
                ++blockquoteDepth;
        }
        depth = std::max(depth, blockquoteDepth);
    }

    for (unsigned pass = 0; pass < depth && !blockquotesToRemove().isEmpty() && canOutdentSelectedParagraphs(endingSelection()); ++pass)
        applyCommandToComposite(IndentOutdentCommand::create(protect(document()), IndentOutdentCommand::IndentType::Outdent));
}

// A quote is never put inside a list or a paragraph-holding block; it goes around them.
static bool canHoldBlockquote(Node& node)
{
    using namespace ElementNames;

    if (isTableCell(node))
        return true;
    RefPtr element = dynamicDowncast<HTMLElement>(node);
    if (!element || !isBlock(*element))
        return false;
    switch (element->elementName()) {
    case HTML::article:
    case HTML::aside:
    case HTML::blockquote:
    case HTML::body:
    case HTML::dd:
    case HTML::details:
    case HTML::dialog:
    case HTML::div:
    case HTML::fieldset:
    case HTML::figure:
    case HTML::footer:
    case HTML::form:
    case HTML::header:
    case HTML::main:
    case HTML::nav:
    case HTML::section:
        return true;
    default:
        return false;
    }
}

static RefPtr<Node> nodeAtStartOfParagraph(const Position& position)
{
    RefPtr anchor = position.anchorNode();
    if (position.anchorType() == Position::PositionIsBeforeAnchor || is<CharacterData>(*anchor))
        return anchor;
    if (RefPtr node = position.computeNodeAfterPosition())
        return node;
    return anchor;
}

static RefPtr<Node> nodeAtEndOfParagraph(const Position& position)
{
    RefPtr anchor = position.anchorNode();
    if (position.anchorType() == Position::PositionIsBeforeAnchor || position.anchorType() == Position::PositionIsAfterAnchor || is<CharacterData>(*anchor))
        return anchor;
    if (RefPtr node = position.computeNodeBeforePosition())
        return node;
    return anchor;
}

// A position anchored to a node rather than to an offset in its parent, so that it survives moving the node.
static Position positionAnchoredToNode(const Position& position)
{
    if (position.anchorType() != Position::PositionIsOffsetInAnchor || is<CharacterData>(position.containerNode()))
        return position;
    if (RefPtr node = position.computeNodeAfterPosition())
        return { WTF::move(node), Position::PositionIsBeforeAnchor };
    if (RefPtr node = position.computeNodeBeforePosition())
        return { WTF::move(node), Position::PositionIsAfterAnchor };
    return position;
}

static bool containsBlockquote(const Node& node)
{
    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        return false;
    if (element->hasTagName(blockquoteTag))
        return true;
    for (Ref descendant : descendantsOfType<HTMLElement>(*element)) {
        if (descendant->hasTagName(blockquoteTag))
            return true;
    }
    return false;
}

void ApplyBlockPresentationIntentCommand::wrapSelectedBlocksInBlockquote()
{
    protect(document())->updateLayoutIgnorePendingStylesheets();

    auto startOfFirstParagraph = startOfParagraph(endingSelection().visibleStart());
    auto endOfLastParagraph = endOfParagraph(endingSelection().visibleEnd());
    auto start = startOfFirstParagraph.deepEquivalent();
    auto end = endOfLastParagraph.deepEquivalent();
    RefPtr root = editableRootForPosition(start);
    if (!root || start.isNull() || end.isNull())
        return;

    // The quote goes in the innermost block that holds more than the selected blocks.
    auto holdsOnlySelectedParagraphs = [&](Node& node) {
        return VisiblePosition { firstPositionInNode(node) } == startOfFirstParagraph && VisiblePosition { lastPositionInNode(node) } == endOfLastParagraph;
    };
    RefPtr<Node> container = commonInclusiveAncestor(start, end);
    while (container && container != root && !isTableCell(*container) && (!canHoldBlockquote(*container) || holdsOnlySelectedParagraphs(*container)))
        container = container->parentNode();

    // FormatBlockCommand would also put the quote inside an editable root such as <p contenteditable>.
    if (container == root && !canHoldBlockquote(*root))
        return;

    RefPtr startNode = nodeAtStartOfParagraph(start);
    RefPtr endNode = nodeAtEndOfParagraph(end);
    auto childOfContainer = [&](RefPtr<Node> node) {
        while (node && node->parentNode() != container)
            node = node->parentNode();
        return node;
    };
    if (!container || !childOfContainer(startNode) || !childOfContainer(endNode)) {
        applyCommandToComposite(FormatBlockCommand::create(protect(document()), blockquoteTag));
        return;
    }

    // A line break after inline content ends its paragraph.
    while (!endNode->nextSibling() && endNode->parentNode() != container && !isBlock(*endNode->parentNode()))
        endNode = endNode->parentNode();
    if (RefPtr next = endNode->nextSibling(); next && next->hasTagName(brTag) && !endNode->hasTagName(brTag) && !isBlock(*endNode))
        endNode = WTF::move(next);

    // Tables are quoted whole, and quoted blocks are already quoted, so neither is split.
    auto outermostUnsplittableAncestor = [&](RefPtr<Node> node, bool includeBlockQuotes) {
        RefPtr result = node;
        for (; node && node != container; node = node->parentNode()) {
            if (node->hasTagName(tableTag) || (includeBlockQuotes && node->hasTagName(blockquoteTag)))
                result = node;
        }
        return result;
    };
    startNode = outermostUnsplittableAncestor(startNode, false);
    endNode = outermostUnsplittableAncestor(endNode, true);

    auto selection = endingSelection();
    auto anchor = positionAnchoredToNode(selection.anchor());
    auto focus = positionAnchoredToNode(selection.focus());

    // Splits the blocks around the selected paragraphs at paragraph boundaries, so that only they are quoted.
    for (RefPtr<Node> node = endNode; node->parentNode() != container; node = node->parentNode()) {
        RefPtr parent = dynamicDowncast<Element>(node->parentNode());
        if (!parent || editingIgnoresContent(*parent))
            break;
        if (RefPtr next = node->nextSibling(); next && VisiblePosition { lastPositionInOrAfterNode(node.get()) } != VisiblePosition { lastPositionInNode(*parent) })
            splitElement(*parent, *next);
    }
    if (startNode != container)
        splitTreeToNode(*startNode, *container);

    RefPtr first = childOfContainer(startNode);
    RefPtr last = childOfContainer(endNode);
    if (!first || !last)
        return;

    // Quotes, including Mail's quotes of other messages, are never moved or nested, so the blocks between them are quoted
    // separately, and blocks holding a quote are not quoted.
    Vector<Vector<Ref<Node>>> runs;
    Vector<Ref<Node>> currentRun;
    RefPtr<Node> child = first;
    for (; child; child = child->nextSibling()) {
        if (containsBlockquote(*child)) {
            if (!currentRun.isEmpty())
                runs.append(std::exchange(currentRun, { }));
        } else
            currentRun.append(*child);
        if (child == last)
            break;
    }
    if (!child) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!currentRun.isEmpty())
        runs.append(WTF::move(currentRun));

    for (auto& run : borrow(runs).get()) {
        bool hasContent = run.containsIf([](auto& node) {
            RefPtr text = dynamicDowncast<Text>(node);
            return !text || !text->containsOnlyASCIIWhitespace();
        });
        if (!hasContent)
            continue;
        Ref blockquote = createHTMLElement(protect(document()), blockquoteTag);
        RefPtr pastLast = run.last()->nextSibling();
        insertNodeBefore(blockquote.copyRef(), run.first());
        moveRemainingSiblingsToNewParent(run.first().ptr(), pastLast.get(), blockquote);
    }

    // Simple commands such as RemoveNodeCommand don't update the ending selection, so it is restored from positions
    // that moving the nodes didn't invalidate.
    setEndingSelection(VisibleSelection { anchor, focus, selection.affinity(), selection.directionality() });
}

void ApplyBlockPresentationIntentCommand::insertThematicBreakAfterSelectedParagraphs()
{
    Ref document = this->document();
    setEndingSelection(VisibleSelection { endOfParagraph(endingSelection().visibleEnd()), endingSelection().directionality() });
    Ref fragment = DocumentFragment::create(document);
    Ref rule = HTMLHRElement::create(document);
    if (fragment->appendChild(rule).hasException())
        return;
    applyCommandToComposite(ReplaceSelectionCommand::create(WTF::move(document), WTF::move(fragment), ReplaceSelectionCommand::PreventNesting, EditAction::Insert));
}

void ApplyBlockPresentationIntentCommand::removeInlineStyleProperties(StyledElement& element, std::span<const CSSPropertyID> properties)
{
    RefPtr style = element.inlineStyle();
    if (!style)
        return;

    Ref mutableStyle = style->mutableCopy();
    bool removedProperty = false;
    for (auto property : properties)
        removedProperty |= mutableStyle->removeProperty(property);
    if (!removedProperty)
        return;

    if (mutableStyle->isEmpty())
        removeNodeAttribute(element, styleAttr);
    else
        setNodeAttribute(element, styleAttr, AtomString { mutableStyle->asText(CSS::defaultSerializationContext()) });
}

// Removes the sizes, faces and spacing that would override the style sheet's look for the block, keeping colors and
// emphasis.
void ApplyBlockPresentationIntentCommand::removePresentationOverridingStyleSheet(HTMLElement& block)
{
    removeInlineStyleProperties(block, blockPresentationProperties);

    Vector<Ref<HTMLElement>> descendants;
    for (Ref descendant : descendantsOfType<HTMLElement>(block)) {
        if (descendant->hasEditableStyle() && !isInMailBlockquote(descendant))
            descendants.append(WTF::move(descendant));
    }

    for (Ref element : descendants) {
        removeInlineStyleProperties(element, inlinePresentationProperties);
        if (!element->hasTagName(fontTag))
            continue;
        if (element->hasAttributeWithoutSynchronization(faceAttr))
            removeNodeAttribute(element, faceAttr);
        if (element->hasAttributeWithoutSynchronization(sizeAttr))
            removeNodeAttribute(element, sizeAttr);
        if (!element->hasAttributes())
            removeElementPreservingChildren(element);
    }
}

// Blocks that were already the kind keep their look, as do Mail's quotes of other messages.
void ApplyBlockPresentationIntentCommand::removePresentationOverridingStyleSheetFromChangedBlocks(const QualifiedName& tagName, const Vector<Ref<HTMLElement>>& unchangedBlocks)
{
    for (Ref block : blocksInSelectionWithTag(tagName)) {
        if (!unchangedBlocks.containsIf([&](auto& unchangedBlock) { return unchangedBlock.ptr() == block.ptr(); }) && !isInMailBlockquote(block))
            removePresentationOverridingStyleSheet(block);
    }
}

void ApplyBlockPresentationIntentCommand::doApply()
{
    Ref document = this->document();

    // InsertListCommand and FormatBlockCommand leave out the paragraph that a selection ends at the start of.
    auto visibleStart = endingSelection().visibleStart();
    auto visibleEnd = endingSelection().visibleEnd();
    if (visibleStart != visibleEnd && isStartOfParagraph(visibleEnd)) {
        VisibleSelection selection { visibleStart, visibleEnd.previous(EditingBoundaryCrossingRule::CannotCross), endingSelection().directionality() };
        if (selection.isNone())
            return;
        setEndingSelection(selection);
    }

    if (m_intent.kind == BlockPresentationIntent::Kind::ThematicBreak) {
        insertThematicBreakAfterSelectedParagraphs();
        return;
    }

    if (auto listType = listTypeForKind(m_intent.kind)) {
        if (isEverySelectedParagraphInListWithTag(endingSelection(), listTagForType(*listType)))
            return;
        auto unchangedListItems = blocksInSelectionWithTag(liTag);
        if (removeCodeElementsPreservingChildren())
            applyCommandToComposite(FormatBlockCommand::create(document.copyRef(), pTag));
        // InsertListCommand changes the type of a list that a paragraph is already in, which keeps its nesting.
        if (blocksInSelectionWithTag(olTag).isEmpty() && blocksInSelectionWithTag(ulTag).isEmpty())
            removeBlockquotesPreservingChildren();
        putSelectedParagraphsInList(*listType);
        removePresentationOverridingStyleSheetFromChangedBlocks(liTag, unchangedListItems);
        return;
    }

    auto tagName = blockTagForIntent(m_intent);
    if (!tagName)
        return;

    if (m_intent.kind == BlockPresentationIntent::Kind::BlockQuote) {
        // A block quote holds other blocks, so quoted blocks are already quoted.
        if (!enclosingElementWithTag(endingSelection().start(), blockquoteTag))
            wrapSelectedBlocksInBlockquote();
        return;
    }

    auto unchangedBlocks = blocksInSelectionWithTag(*tagName);
    if (m_intent.kind == BlockPresentationIntent::Kind::CodeBlock) {
        unchangedBlocks.removeAllMatching([](auto& preElement) {
            return !codeElementOfCodeBlock(preElement);
        });
    } else
        removeCodeElementsPreservingChildren();
    removeListElementsPreservingChildren();
    removeBlockquotesPreservingChildren();

    applyCommandToComposite(FormatBlockCommand::create(document.copyRef(), *tagName));

    if (m_intent.kind == BlockPresentationIntent::Kind::CodeBlock) {
        for (Ref preElement : blocksInSelectionWithTag(preTag))
            wrapContentsInCode(preElement);
    }

    removePresentationOverridingStyleSheetFromChangedBlocks(*tagName, unchangedBlocks);
}

} // namespace WebCore
