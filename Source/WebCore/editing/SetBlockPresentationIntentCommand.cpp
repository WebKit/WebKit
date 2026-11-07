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
#include "SetBlockPresentationIntentCommand.h"

#include "CSSSerializationContext.h"
#include "Editing.h"
#include "ElementChildIteratorInlines.h"
#include "FormatBlockCommand.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "IndentOutdentCommand.h"
#include "MutableStyleProperties.h"
#include "SimpleRange.h"
#include "StylePropertiesInlines.h"
#include "StyledElement.h"
#include "Text.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibleUnits.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace HTMLNames;

// The <code> of the CommonMark form <pre><code>, which is what reads back as a code block rather than preformatted text.
static RefPtr<HTMLElement> soleCodeChild(const Element& block)
{
    RefPtr<HTMLElement> codeElement;
    for (RefPtr child = block.firstChild(); child; child = child->nextSibling()) {
        if (RefPtr text = dynamicDowncast<Text>(*child)) {
            if (!text->containsOnlyASCIIWhitespace())
                return nullptr;
        } else if (is<Element>(*child)) {
            if (codeElement || !child->hasTagName(codeTag))
                return nullptr;
            codeElement = &downcast<HTMLElement>(*child);
        }
    }
    return codeElement;
}

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
        RefPtr list = enclosingList(protect(paragraph.deepEquivalent().deprecatedNode()).get());
        if (!list || !list->hasTagName(listTag))
            return false;
        if (paragraph == lastParagraph)
            return true;
    }
    return false;
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

SetBlockPresentationIntentCommand::SetBlockPresentationIntentCommand(Ref<Document>&& document, const BlockPresentationIntent& intent)
    : CompositeEditCommand(WTF::move(document))
    , m_intent(intent)
{
}

EditAction SetBlockPresentationIntentCommand::editingAction() const
{
    switch (m_intent.kind) {
    case BlockPresentationIntent::Kind::OrderedList:
        return EditAction::InsertOrderedList;
    case BlockPresentationIntent::Kind::UnorderedList:
        return EditAction::InsertUnorderedList;
    default:
        return EditAction::FormatBlock;
    }
}

void SetBlockPresentationIntentCommand::applyAndTakeEndingSelection(Ref<CompositeEditCommand>&& command)
{
    applyCommandToComposite(command.copyRef(), endingSelection());
    setEndingSelection(command->endingSelection());
}

Vector<Ref<HTMLElement>> SetBlockPresentationIntentCommand::blocksInSelectionWithTag(const QualifiedName& tagName) const
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

// Keeps the ending selection on the element's children rather than on the removed element.
void SetBlockPresentationIntentCommand::removeElementPreservingChildren(HTMLElement& element)
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

void SetBlockPresentationIntentCommand::wrapContentsInCode(HTMLElement& preElement)
{
    RefPtr codeElement = soleCodeChild(preElement);
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

// Code is never kept as code inside another kind of block. Returns whether the selection had code blocks.
bool SetBlockPresentationIntentCommand::leaveCodeBlocks()
{
    bool hadCodeBlock = false;
    for (Ref preElement : blocksInSelectionWithTag(preTag)) {
        if (RefPtr codeElement = soleCodeChild(preElement)) {
            removeElementPreservingChildren(*codeElement);
            hadCodeBlock = true;
        }
    }
    return hadCodeBlock;
}

void SetBlockPresentationIntentCommand::putSelectedParagraphsInList(InsertListCommand::Type listType)
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
        applyAndTakeEndingSelection(InsertListCommand::create(protect(document()), otherListType));
    }
    applyAndTakeEndingSelection(InsertListCommand::create(protect(document()), listType));
}

// Takes the selected paragraphs out of lists, whatever their types.
void SetBlockPresentationIntentCommand::leaveLists()
{
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
    applyAndTakeEndingSelection(InsertListCommand::create(protect(document()), listType));
}

// Mail's quotes of other messages stay, and a quote can only be left if there is somewhere to go.
static bool canLeaveBlockQuote(const HTMLElement& blockQuote)
{
    RefPtr parent = blockQuote.parentNode();
    return !isMailBlockquote(blockQuote) && parent && parent->hasEditableStyle();
}

// IndentOutdentCommand outdents each paragraph out of its innermost quote, so this checks those quotes.
static bool canOutdentSelectedParagraphs(const VisibleSelection& selection)
{
    auto lastParagraph = startOfParagraph(selection.visibleEnd());
    for (auto paragraph = startOfParagraph(selection.visibleStart()); paragraph.isNotNull() && paragraph <= lastParagraph; paragraph = startOfNextParagraph(paragraph)) {
        RefPtr blockQuote = dynamicDowncast<HTMLElement>(enclosingElementWithTag(paragraph.deepEquivalent(), blockquoteTag));
        if (blockQuote && !canLeaveBlockQuote(*blockQuote))
            return false;
        if (paragraph == lastParagraph)
            break;
    }
    return true;
}

// The selected block quotes that outdenting can take the selection out of.
Vector<Ref<HTMLElement>> SetBlockPresentationIntentCommand::blockQuotesToLeave() const
{
    auto blockQuotes = blocksInSelectionWithTag(blockquoteTag);
    blockQuotes.removeAllMatching([](auto& blockQuote) {
        return !canLeaveBlockQuote(blockQuote.get());
    });
    return blockQuotes;
}

void SetBlockPresentationIntentCommand::leaveBlockQuotes()
{
    // Outdenting leaves one level of quoting, so no more passes are needed than the quotes are deep.
    unsigned depth = 0;
    for (Ref blockQuote : blockQuotesToLeave()) {
        unsigned blockQuoteDepth = 0;
        for (RefPtr<Element> element = blockQuote.ptr(); element; element = element->parentElement()) {
            if (element->hasTagName(blockquoteTag))
                ++blockQuoteDepth;
        }
        depth = std::max(depth, blockQuoteDepth);
    }

    for (unsigned pass = 0; pass < depth && !blockQuotesToLeave().isEmpty() && canOutdentSelectedParagraphs(endingSelection()); ++pass)
        applyAndTakeEndingSelection(IndentOutdentCommand::create(protect(document()), IndentOutdentCommand::IndentType::Outdent));
}

void SetBlockPresentationIntentCommand::removeInlineStyleProperties(StyledElement& element, std::span<const CSSPropertyID> properties)
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
void SetBlockPresentationIntentCommand::removePresentationOverridingStyleSheet(HTMLElement& block)
{
    removeInlineStyleProperties(block, blockPresentationProperties);

    Vector<Ref<HTMLElement>> descendants;
    for (Ref descendant : descendantsOfType<HTMLElement>(block)) {
        if (descendant->hasEditableStyle())
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

void SetBlockPresentationIntentCommand::doApply()
{
    Ref document = this->document();

    if (auto listType = listTypeForKind(m_intent.kind)) {
        if (!isEverySelectedParagraphInListWithTag(endingSelection(), listTagForType(*listType))) {
            if (leaveCodeBlocks())
                applyAndTakeEndingSelection(FormatBlockCommand::create(document.copyRef(), pTag));
            // InsertListCommand changes the type of a list that a paragraph is already in, which keeps its nesting.
            if (blocksInSelectionWithTag(olTag).isEmpty() && blocksInSelectionWithTag(ulTag).isEmpty())
                leaveBlockQuotes();
            putSelectedParagraphsInList(*listType);
        }
        for (Ref listItem : blocksInSelectionWithTag(liTag))
            removePresentationOverridingStyleSheet(listItem);
        return;
    }

    auto tagName = blockTagForIntent(m_intent);
    if (!tagName)
        return;

    // A block quote holds other blocks, so quoted blocks are already quoted.
    if (m_intent.kind == BlockPresentationIntent::Kind::BlockQuote && enclosingElementWithTag(endingSelection().start(), blockquoteTag))
        return;

    if (m_intent.kind != BlockPresentationIntent::Kind::CodeBlock)
        leaveCodeBlocks();
    leaveLists();
    if (m_intent.kind != BlockPresentationIntent::Kind::BlockQuote)
        leaveBlockQuotes();

    applyAndTakeEndingSelection(FormatBlockCommand::create(document.copyRef(), *tagName));

    if (m_intent.kind == BlockPresentationIntent::Kind::CodeBlock) {
        for (Ref preElement : blocksInSelectionWithTag(preTag))
            wrapContentsInCode(preElement);
    }

    for (Ref block : blocksInSelectionWithTag(*tagName))
        removePresentationOverridingStyleSheet(block);
}

} // namespace WebCore
