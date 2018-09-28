/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "CompositeEditCommand.h"

#include "AXObjectCache.h"
#include "AppendNodeCommand.h"
#include "ApplyStyleCommand.h"
#include "BreakBlockquoteCommand.h"
#include "DataTransfer.h"
#include "DeleteFromTextNodeCommand.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorInsertAction.h"
#include "ElementTraversal.h"
#include "Event.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLLIElement.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "InlineTextBox.h"
#include "InsertIntoTextNodeCommand.h"
#include "InsertLineBreakCommand.h"
#include "InsertNodeBeforeCommand.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "MergeIdenticalElementsCommand.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RemoveNodeCommand.h"
#include "RemoveNodePreservingChildrenCommand.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "RenderedDocumentMarker.h"
#include "ReplaceNodeWithSpanCommand.h"
#include "ReplaceSelectionCommand.h"
#include "ScopedEventQueue.h"
#include "SetNodeAttributeCommand.h"
#include "SplitElementCommand.h"
#include "SplitTextNodeCommand.h"
#include "SplitTextNodeContainingElementCommand.h"
#include "StaticRange.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "WrapContentsInDummySpanCommand.h"
#include "markup.h"

namespace WebCore {

using namespace HTMLNames;

int AccessibilityUndoReplacedText::indexForVisiblePosition(const VisiblePosition& position, RefPtr<ContainerNode>& scope) const
{
    if (position.deepEquivalent().isNull())
        return -1;
    return WebCore::indexForVisiblePosition(position, scope);
}

void AccessibilityUndoReplacedText::configureRangeDeletedByReapplyWithEndingSelection(const VisibleSelection& selection)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (selection.isNone())
        return;
    m_rangeDeletedByReapply.endIndex.value = indexForVisiblePosition(selection.end(), m_rangeDeletedByReapply.endIndex.scope);
}

void AccessibilityUndoReplacedText::configureRangeDeletedByReapplyWithStartingSelection(const VisibleSelection& selection)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (selection.isNone())
        return;
    if (m_rangeDeletedByReapply.startIndex.value == -1)
        m_rangeDeletedByReapply.startIndex.value = indexForVisiblePosition(selection.start(), m_rangeDeletedByReapply.startIndex.scope);
}

void AccessibilityUndoReplacedText::setRangeDeletedByUnapply(const VisiblePositionIndexRange& range)
{
    if (m_rangeDeletedByUnapply.isNull())
        m_rangeDeletedByUnapply = range;
}

void AccessibilityUndoReplacedText::captureTextForUnapply()
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    m_replacedText = textDeletedByReapply();
}

void AccessibilityUndoReplacedText::captureTextForReapply()
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    m_replacedText = textDeletedByUnapply();
}

static String stringForVisiblePositionIndexRange(const VisiblePositionIndexRange& range)
{
    if (range.isNull())
        return String();
    VisiblePosition start = visiblePositionForIndex(range.startIndex.value, range.startIndex.scope.get());
    VisiblePosition end = visiblePositionForIndex(range.endIndex.value, range.endIndex.scope.get());
    return AccessibilityObject::stringForVisiblePositionRange(VisiblePositionRange(start, end));
}

String AccessibilityUndoReplacedText::textDeletedByUnapply()
{
    if (!AXObjectCache::accessibilityEnabled())
        return String();
    return stringForVisiblePositionIndexRange(m_rangeDeletedByUnapply);
}

String AccessibilityUndoReplacedText::textDeletedByReapply()
{
    if (!AXObjectCache::accessibilityEnabled())
        return String();
    return stringForVisiblePositionIndexRange(m_rangeDeletedByReapply);
}

static void postTextStateChangeNotification(AXObjectCache* cache, const VisiblePosition& position, const String& deletedText, const String& insertedText)
{
    ASSERT(cache);
    auto* node = highestEditableRoot(position.deepEquivalent(), HasEditableAXRole);
    if (!node)
        return;
    if (insertedText.length() && deletedText.length())
        cache->postTextReplacementNotification(node, AXTextEditTypeDelete, insertedText, AXTextEditTypeInsert, deletedText, position);
    else if (deletedText.length())
        cache->postTextStateChangeNotification(node, AXTextEditTypeInsert, deletedText, position);
    else if (insertedText.length())
        cache->postTextStateChangeNotification(node, AXTextEditTypeDelete, insertedText, position);
}

void AccessibilityUndoReplacedText::postTextStateChangeNotificationForUnapply(AXObjectCache* cache)
{
    if (!cache)
        return;
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (m_rangeDeletedByUnapply.isNull())
        return;
    VisiblePosition position = visiblePositionForIndex(m_rangeDeletedByUnapply.endIndex.value, m_rangeDeletedByUnapply.endIndex.scope.get());
    if (position.isNull())
        return;
    postTextStateChangeNotification(cache, position, textDeletedByUnapply(), m_replacedText);
    m_replacedText = String();
}

void AccessibilityUndoReplacedText::postTextStateChangeNotificationForReapply(AXObjectCache* cache)
{
    if (!cache)
        return;
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (m_rangeDeletedByReapply.isNull())
        return;
    VisiblePosition position = visiblePositionForIndex(m_rangeDeletedByReapply.startIndex.value, m_rangeDeletedByReapply.startIndex.scope.get());
    if (position.isNull())
        return;
    postTextStateChangeNotification(cache, position, textDeletedByReapply(), m_replacedText);
    m_replacedText = String();
}

Ref<EditCommandComposition> EditCommandComposition::create(Document& document,
    const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction editAction)
{
    return adoptRef(*new EditCommandComposition(document, startingSelection, endingSelection, editAction));
}

EditCommandComposition::EditCommandComposition(Document& document, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction editAction)
    : m_document(&document)
    , m_startingSelection(startingSelection)
    , m_endingSelection(endingSelection)
    , m_startingRootEditableElement(startingSelection.rootEditableElement())
    , m_endingRootEditableElement(endingSelection.rootEditableElement())
    , m_editAction(editAction)
{
    m_replacedText.configureRangeDeletedByReapplyWithStartingSelection(startingSelection);
}

void EditCommandComposition::unapply()
{
    ASSERT(m_document);
    RefPtr<Frame> frame = m_document->frame();
    if (!frame)
        return;

    m_replacedText.captureTextForUnapply();

    // Changes to the document may have been made since the last editing operation that require a layout, as in <rdar://problem/5658603>.
    // Low level operations, like RemoveNodeCommand, don't require a layout because the high level operations that use them perform one
    // if one is necessary (like for the creation of VisiblePositions).
    m_document->updateLayoutIgnorePendingStylesheets();
#if PLATFORM(IOS)
    // FIXME: Where should iPhone code deal with the composition?
    // Since editing commands don't save/restore the composition, undoing without fixing
    // up the composition will leave a stale, invalid composition, as in <rdar://problem/6831637>.
    // Desktop handles this in -[WebHTMLView _updateSelectionForInputManager], but the phone
    // goes another route.
    frame->editor().cancelComposition();
#endif

    if (!frame->editor().willUnapplyEditing(*this))
        return;

    size_t size = m_commands.size();
    for (size_t i = size; i; --i)
        m_commands[i - 1]->doUnapply();

    frame->editor().unappliedEditing(*this);

    if (AXObjectCache::accessibilityEnabled())
        m_replacedText.postTextStateChangeNotificationForUnapply(m_document->existingAXObjectCache());
}

void EditCommandComposition::reapply()
{
    ASSERT(m_document);
    RefPtr<Frame> frame = m_document->frame();
    if (!frame)
        return;

    m_replacedText.captureTextForReapply();

    // Changes to the document may have been made since the last editing operation that require a layout, as in <rdar://problem/5658603>.
    // Low level operations, like RemoveNodeCommand, don't require a layout because the high level operations that use them perform one
    // if one is necessary (like for the creation of VisiblePositions).
    m_document->updateLayoutIgnorePendingStylesheets();

    if (!frame->editor().willReapplyEditing(*this))
        return;

    for (auto& command : m_commands)
        command->doReapply();

    frame->editor().reappliedEditing(*this);

    if (AXObjectCache::accessibilityEnabled())
        m_replacedText.postTextStateChangeNotificationForReapply(m_document->existingAXObjectCache());
}

void EditCommandComposition::append(SimpleEditCommand* command)
{
    m_commands.append(command);
}

void EditCommandComposition::setStartingSelection(const VisibleSelection& selection)
{
    m_startingSelection = selection;
    m_startingRootEditableElement = selection.rootEditableElement();
    m_replacedText.configureRangeDeletedByReapplyWithStartingSelection(selection);
}

void EditCommandComposition::setEndingSelection(const VisibleSelection& selection)
{
    m_endingSelection = selection;
    m_endingRootEditableElement = selection.rootEditableElement();
    m_replacedText.configureRangeDeletedByReapplyWithEndingSelection(selection);
}

void EditCommandComposition::setRangeDeletedByUnapply(const VisiblePositionIndexRange& range)
{
    m_replacedText.setRangeDeletedByUnapply(range);
}

#ifndef NDEBUG
void EditCommandComposition::getNodesInCommand(HashSet<Node*>& nodes)
{
    for (auto& command : m_commands)
        command->getNodesInCommand(nodes);
}
#endif

CompositeEditCommand::CompositeEditCommand(Document& document, EditAction editingAction)
    : EditCommand(document, editingAction)
{
}

CompositeEditCommand::~CompositeEditCommand()
{
    ASSERT(isTopLevelCommand() || !m_composition);
}

bool CompositeEditCommand::willApplyCommand()
{
    return frame().editor().willApplyEditing(*this, targetRangesForBindings());
}

void CompositeEditCommand::apply()
{
    if (!endingSelection().isContentRichlyEditable()) {
        switch (editingAction()) {
        case EditAction::TypingDeleteSelection:
        case EditAction::TypingDeleteBackward:
        case EditAction::TypingDeleteForward:
        case EditAction::TypingDeleteWordBackward:
        case EditAction::TypingDeleteWordForward:
        case EditAction::TypingDeleteLineBackward:
        case EditAction::TypingDeleteLineForward:
        case EditAction::TypingDeletePendingComposition:
        case EditAction::TypingDeleteFinalComposition:
        case EditAction::TypingInsertText:
        case EditAction::TypingInsertLineBreak:
        case EditAction::TypingInsertParagraph:
        case EditAction::TypingInsertPendingComposition:
        case EditAction::TypingInsertFinalComposition:
        case EditAction::Paste:
        case EditAction::DeleteByDrag:
        case EditAction::SetWritingDirection:
        case EditAction::Cut:
        case EditAction::Unspecified:
        case EditAction::Insert:
        case EditAction::InsertReplacement:
        case EditAction::InsertFromDrop:
        case EditAction::Delete:
        case EditAction::Dictation:
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
    }
    ensureComposition();

    // Changes to the document may have been made since the last editing operation that require a layout, as in <rdar://problem/5658603>.
    // Low level operations, like RemoveNodeCommand, don't require a layout because the high level operations that use them perform one
    // if one is necessary (like for the creation of VisiblePositions).
    document().updateLayoutIgnorePendingStylesheets();

    if (!willApplyCommand())
        return;

    {
        EventQueueScope eventQueueScope;
        doApply();
    }

    didApplyCommand();
    setShouldRetainAutocorrectionIndicator(false);
}

void CompositeEditCommand::didApplyCommand()
{
    frame().editor().appliedEditing(*this);
}

Vector<RefPtr<StaticRange>> CompositeEditCommand::targetRanges() const
{
    ASSERT(!isEditingTextAreaOrTextInput());
    auto firstRange = frame().selection().selection().firstRange();
    if (!firstRange)
        return { };

    RefPtr<StaticRange> range = StaticRange::createFromRange(*firstRange);
    return { 1, range };
}

Vector<RefPtr<StaticRange>> CompositeEditCommand::targetRangesForBindings() const
{
    if (isEditingTextAreaOrTextInput())
        return { };

    return targetRanges();
}

RefPtr<DataTransfer> CompositeEditCommand::inputEventDataTransfer() const
{
    return nullptr;
}

EditCommandComposition* CompositeEditCommand::composition() const
{
    for (auto* command = this; command; command = command->parent()) {
        if (auto composition = command->m_composition) {
            ASSERT(!command->parent());
            return composition.get();
        }
    }
    return nullptr;
}

EditCommandComposition& CompositeEditCommand::ensureComposition()
{
    auto* command = this;
    while (auto* parent = command->parent())
        command = parent;
    if (!command->m_composition)
        command->m_composition = EditCommandComposition::create(document(), startingSelection(), endingSelection(), editingAction());
    return *command->m_composition;
}

bool CompositeEditCommand::isCreateLinkCommand() const
{
    return false;
}

bool CompositeEditCommand::preservesTypingStyle() const
{
    return false;
}

bool CompositeEditCommand::isTypingCommand() const
{
    return false;
}

bool CompositeEditCommand::shouldRetainAutocorrectionIndicator() const
{
    return false;
}

void CompositeEditCommand::setShouldRetainAutocorrectionIndicator(bool)
{
}

String CompositeEditCommand::inputEventTypeName() const
{
    return inputTypeNameForEditingAction(editingAction());
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommand::applyCommandToComposite(Ref<EditCommand>&& command)
{
    command->setParent(this);
    command->doApply();
    if (command->isSimpleEditCommand()) {
        command->setParent(nullptr);
        ensureComposition().append(toSimpleEditCommand(command.ptr()));
    }
    m_commands.append(WTFMove(command));
}

void CompositeEditCommand::applyCommandToComposite(Ref<CompositeEditCommand>&& command, const VisibleSelection& selection)
{
    command->setParent(this);
    if (selection != command->endingSelection()) {
        command->setStartingSelection(selection);
        command->setEndingSelection(selection);
    }
    command->doApply();
    m_commands.append(WTFMove(command));
}

void CompositeEditCommand::applyStyle(const EditingStyle* style, EditAction editingAction)
{
    applyCommandToComposite(ApplyStyleCommand::create(document(), style, editingAction));
}

void CompositeEditCommand::applyStyle(const EditingStyle* style, const Position& start, const Position& end, EditAction editingAction)
{
    applyCommandToComposite(ApplyStyleCommand::create(document(), style, start, end, editingAction));
}

void CompositeEditCommand::applyStyledElement(Ref<Element>&& element)
{
    applyCommandToComposite(ApplyStyleCommand::create(WTFMove(element), false));
}

void CompositeEditCommand::removeStyledElement(Ref<Element>&& element)
{
    applyCommandToComposite(ApplyStyleCommand::create(WTFMove(element), true));
}

void CompositeEditCommand::insertParagraphSeparator(bool useDefaultParagraphElement, bool pasteBlockqutoeIntoUnquotedArea)
{
    applyCommandToComposite(InsertParagraphSeparatorCommand::create(document(), useDefaultParagraphElement, pasteBlockqutoeIntoUnquotedArea, editingAction()));
}

void CompositeEditCommand::insertLineBreak()
{
    applyCommandToComposite(InsertLineBreakCommand::create(document()));
}

bool CompositeEditCommand::isRemovableBlock(const Node* node)
{
    ASSERT(node);
    if (!is<HTMLDivElement>(*node))
        return false;

    Node* parentNode = node->parentNode();
    if (parentNode && parentNode->firstChild() != parentNode->lastChild())
        return false;

    if (!downcast<HTMLDivElement>(*node).hasAttributes())
        return true;

    return false;
}

void CompositeEditCommand::insertNodeBefore(Ref<Node>&& insertChild, Node& refChild, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    applyCommandToComposite(InsertNodeBeforeCommand::create(WTFMove(insertChild), refChild, shouldAssumeContentIsAlwaysEditable, editingAction()));
}

void CompositeEditCommand::insertNodeAfter(Ref<Node>&& insertChild, Node& refChild)
{
    ContainerNode* parent = refChild.parentNode();
    if (!parent)
        return;

    ASSERT(!parent->isShadowRoot());
    if (parent->lastChild() == &refChild)
        appendNode(WTFMove(insertChild), *parent);
    else {
        ASSERT(refChild.nextSibling());
        insertNodeBefore(WTFMove(insertChild), *refChild.nextSibling());
    }
}

void CompositeEditCommand::insertNodeAt(Ref<Node>&& insertChild, const Position& editingPosition)
{
    ASSERT(isEditablePosition(editingPosition));
    // For editing positions like [table, 0], insert before the table,
    // likewise for replaced elements, brs, etc.
    Position p = editingPosition.parentAnchoredEquivalent();
    Node* refChild = p.deprecatedNode();
    int offset = p.deprecatedEditingOffset();
    
    if (canHaveChildrenForEditing(*refChild)) {
        Node* child = refChild->firstChild();
        for (int i = 0; child && i < offset; i++)
            child = child->nextSibling();
        if (child)
            insertNodeBefore(WTFMove(insertChild), *child);
        else
            appendNode(WTFMove(insertChild), downcast<ContainerNode>(*refChild));
    } else if (caretMinOffset(*refChild) >= offset)
        insertNodeBefore(WTFMove(insertChild), *refChild);
    else if (is<Text>(*refChild) && caretMaxOffset(*refChild) > offset) {
        splitTextNode(downcast<Text>(*refChild), offset);

        // Mutation events (bug 22634) from the text node insertion may have removed the refChild
        if (!refChild->isConnected())
            return;
        insertNodeBefore(WTFMove(insertChild), *refChild);
    } else
        insertNodeAfter(WTFMove(insertChild), *refChild);
}

void CompositeEditCommand::appendNode(Ref<Node>&& node, Ref<ContainerNode>&& parent)
{
    ASSERT(canHaveChildrenForEditing(parent));
    applyCommandToComposite(AppendNodeCommand::create(WTFMove(parent), WTFMove(node), editingAction()));
}

void CompositeEditCommand::removeChildrenInRange(Node& node, unsigned from, unsigned to)
{
    Vector<Ref<Node>> children;
    Node* child = node.traverseToChildAt(from);
    for (unsigned i = from; child && i < to; i++, child = child->nextSibling())
        children.append(*child);

    for (auto& child : children)
        removeNode(child);
}

void CompositeEditCommand::removeNode(Node& node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    if (!node.nonShadowBoundaryParentNode())
        return;
    applyCommandToComposite(RemoveNodeCommand::create(node, shouldAssumeContentIsAlwaysEditable, editingAction()));
}

void CompositeEditCommand::removeNodePreservingChildren(Node& node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    applyCommandToComposite(RemoveNodePreservingChildrenCommand::create(node, shouldAssumeContentIsAlwaysEditable, editingAction()));
}

void CompositeEditCommand::removeNodeAndPruneAncestors(Node& node)
{
    RefPtr<ContainerNode> parent = node.parentNode();
    removeNode(node);
    prune(parent.get());
}

void CompositeEditCommand::moveRemainingSiblingsToNewParent(Node* node, Node* pastLastNodeToMove, Element& newParent)
{
    NodeVector nodesToRemove;
    Ref<Element> protectedNewParent = newParent;

    for (; node && node != pastLastNodeToMove; node = node->nextSibling())
        nodesToRemove.append(*node);

    for (auto& nodeToRemove : nodesToRemove) {
        removeNode(nodeToRemove);
        appendNode(WTFMove(nodeToRemove), newParent);
    }
}

void CompositeEditCommand::updatePositionForNodeRemovalPreservingChildren(Position& position, Node& node)
{
    int offset = (position.anchorType() == Position::PositionIsOffsetInAnchor) ? position.offsetInContainerNode() : 0;
    updatePositionForNodeRemoval(position, node);
    if (offset)
        position.moveToOffset(offset);    
}

HTMLElement* CompositeEditCommand::replaceElementWithSpanPreservingChildrenAndAttributes(HTMLElement& element)
{
    // It would also be possible to implement all of ReplaceNodeWithSpanCommand
    // as a series of existing smaller edit commands.  Someone who wanted to
    // reduce the number of edit commands could do so here.
    auto command = ReplaceNodeWithSpanCommand::create(element);
    auto* commandPtr = command.ptr();
    applyCommandToComposite(WTFMove(command));
    // Returning a raw pointer here is OK because the command is retained by
    // applyCommandToComposite (thus retaining the span), and the span is also
    // in the DOM tree, and thus alive whie it has a parent.
    ASSERT(commandPtr->spanElement()->isConnected());
    return commandPtr->spanElement();
}

void CompositeEditCommand::prune(Node* node)
{
    if (auto* highestNodeToRemove = highestNodeToRemoveInPruning(node))
        removeNode(*highestNodeToRemove);
}

void CompositeEditCommand::splitTextNode(Text& node, unsigned offset)
{
    applyCommandToComposite(SplitTextNodeCommand::create(node, offset));
}

void CompositeEditCommand::splitElement(Element& element, Node& atChild)
{
    applyCommandToComposite(SplitElementCommand::create(element, atChild));
}

void CompositeEditCommand::mergeIdenticalElements(Element& first, Element& second)
{
    Ref<Element> protectedFirst = first;
    Ref<Element> protectedSecond = second;
    ASSERT(!first.isDescendantOf(&second) && &second != &first);
    if (first.nextSibling() != &second) {
        removeNode(second);
        insertNodeAfter(second, first);
    }
    applyCommandToComposite(MergeIdenticalElementsCommand::create(first, second));
}

void CompositeEditCommand::wrapContentsInDummySpan(Element& element)
{
    applyCommandToComposite(WrapContentsInDummySpanCommand::create(element));
}

void CompositeEditCommand::splitTextNodeContainingElement(Text& text, unsigned offset)
{
    applyCommandToComposite(SplitTextNodeContainingElementCommand::create(text, offset));
}

void CompositeEditCommand::inputText(const String& text, bool selectInsertedText)
{
    unsigned offset = 0;
    unsigned length = text.length();
    
    RefPtr<ContainerNode> scope;
    unsigned startIndex = indexForVisiblePosition(endingSelection().visibleStart(), scope);
    
    size_t newline;
    do {
        newline = text.find('\n', offset);
        if (newline != offset) {
            int substringLength = newline == notFound ? length - offset : newline - offset;
            applyCommandToComposite(InsertTextCommand::create(document(), text.substring(offset, substringLength), false));
        }
        if (newline != notFound) {
            VisiblePosition caret(endingSelection().visibleStart());
            if (enclosingNodeOfType(caret.deepEquivalent(), &isMailBlockquote)) {
                // FIXME: Breaking a blockquote when the caret is just after a space will collapse the 
                // space. Modify startIndex or length to compensate for this so that the ending selection 
                // will be positioned correctly.
                // <rdar://problem/9914462> breaking a Mail blockquote just after a space collapses the space
                if (caret.previous().characterAfter() == ' ') {
                    if (!offset && !startIndex)
                        startIndex--;
                    else if (!length)
                        length--;
                }
                applyCommandToComposite(BreakBlockquoteCommand::create(document()));
            } else
                insertLineBreak();
        }
            
        offset = newline + 1;
    } while (newline != notFound && offset != length);
    
    if (selectInsertedText)
        setEndingSelection(VisibleSelection(visiblePositionForIndex(startIndex, scope.get()), visiblePositionForIndex(startIndex + length, scope.get())));
}

void CompositeEditCommand::insertTextIntoNode(Text& node, unsigned offset, const String& text)
{
    if (!text.isEmpty())
        applyCommandToComposite(InsertIntoTextNodeCommand::create(node, offset, text, editingAction()));
}

void CompositeEditCommand::deleteTextFromNode(Text& node, unsigned offset, unsigned count)
{
    applyCommandToComposite(DeleteFromTextNodeCommand::create(node, offset, count, editingAction()));
}

void CompositeEditCommand::replaceTextInNode(Text& node, unsigned offset, unsigned count, const String& replacementText)
{
    applyCommandToComposite(DeleteFromTextNodeCommand::create(node, offset, count));
    if (!replacementText.isEmpty())
        applyCommandToComposite(InsertIntoTextNodeCommand::create(node, offset, replacementText, editingAction()));
}

Position CompositeEditCommand::replaceSelectedTextInNode(const String& text)
{
    Position start = endingSelection().start();
    Position end = endingSelection().end();
    if (start.containerNode() != end.containerNode() || !start.containerNode()->isTextNode() || isTabSpanTextNode(start.containerNode()))
        return Position();

    RefPtr<Text> textNode = start.containerText();
    replaceTextInNode(*textNode, start.offsetInContainerNode(), end.offsetInContainerNode() - start.offsetInContainerNode(), text);

    return Position(textNode.get(), start.offsetInContainerNode() + text.length());
}

static Vector<RenderedDocumentMarker> copyMarkers(const Vector<RenderedDocumentMarker*>& markerPointers)
{
    Vector<RenderedDocumentMarker> markers;
    markers.reserveInitialCapacity(markerPointers.size());
    for (auto& markerPointer : markerPointers)
        markers.uncheckedAppend(*markerPointer);

    return markers;
}

void CompositeEditCommand::replaceTextInNodePreservingMarkers(Text& node, unsigned offset, unsigned count, const String& replacementText)
{
    Ref<Text> protectedNode(node);
    DocumentMarkerController& markerController = document().markers();
    auto markers = copyMarkers(markerController.markersInRange(Range::create(document(), &node, offset, &node, offset + count), DocumentMarker::allMarkers()));
    replaceTextInNode(node, offset, count, replacementText);
    RefPtr<Range> newRange = Range::create(document(), &node, offset, &node, offset + replacementText.length());
    for (const auto& marker : markers) {
#if PLATFORM(IOS)
        if (marker.isDictation()) {
            markerController.addMarker(newRange.get(), marker.type(), marker.description(), marker.alternatives(), marker.metadata());
            continue;
        }
#endif
        markerController.addMarker(newRange.get(), marker.type(), marker.description());
    }
}

Position CompositeEditCommand::positionOutsideTabSpan(const Position& position)
{
    if (!isTabSpanTextNode(position.anchorNode()))
        return position;

    switch (position.anchorType()) {
    case Position::PositionIsBeforeChildren:
    case Position::PositionIsAfterChildren:
        ASSERT_NOT_REACHED();
        return position;
    case Position::PositionIsOffsetInAnchor:
        break;
    case Position::PositionIsBeforeAnchor:
        return positionInParentBeforeNode(position.anchorNode());
    case Position::PositionIsAfterAnchor:
        return positionInParentAfterNode(position.anchorNode());
    }

    auto* tabSpan = tabSpanNode(position.containerNode());

    if (position.offsetInContainerNode() <= caretMinOffset(*position.containerNode()))
        return positionInParentBeforeNode(tabSpan);

    if (position.offsetInContainerNode() >= caretMaxOffset(*position.containerNode()))
        return positionInParentAfterNode(tabSpan);

    splitTextNodeContainingElement(downcast<Text>(*position.containerNode()), position.offsetInContainerNode());
    return positionInParentBeforeNode(tabSpan);
}

void CompositeEditCommand::insertNodeAtTabSpanPosition(Ref<Node>&& node, const Position& pos)
{
    // insert node before, after, or at split of tab span
    insertNodeAt(WTFMove(node), positionOutsideTabSpan(pos));
}

static EditAction deleteSelectionEditingActionForEditingAction(EditAction editingAction)
{
    switch (editingAction) {
    case EditAction::Cut:
        return EditAction::Cut;
    default:
        return EditAction::Delete;
    }
}

void CompositeEditCommand::deleteSelection(bool smartDelete, bool mergeBlocksAfterDelete, bool replace, bool expandForSpecialElements, bool sanitizeMarkup)
{
    if (endingSelection().isRange())
        applyCommandToComposite(DeleteSelectionCommand::create(document(), smartDelete, mergeBlocksAfterDelete, replace, expandForSpecialElements, sanitizeMarkup, deleteSelectionEditingActionForEditingAction(editingAction())));
}

void CompositeEditCommand::deleteSelection(const VisibleSelection &selection, bool smartDelete, bool mergeBlocksAfterDelete, bool replace, bool expandForSpecialElements, bool sanitizeMarkup)
{
    if (selection.isRange())
        applyCommandToComposite(DeleteSelectionCommand::create(selection, smartDelete, mergeBlocksAfterDelete, replace, expandForSpecialElements, sanitizeMarkup));
}

void CompositeEditCommand::removeNodeAttribute(Element& element, const QualifiedName& attribute)
{
    setNodeAttribute(element, attribute, nullAtom());
}

void CompositeEditCommand::setNodeAttribute(Element& element, const QualifiedName& attribute, const AtomicString& value)
{
    applyCommandToComposite(SetNodeAttributeCommand::create(element, attribute, value));
}

static inline bool containsOnlyDeprecatedEditingWhitespace(const String& text)
{
    for (unsigned i = 0; i < text.length(); ++i) {
        if (!deprecatedIsEditingWhitespace(text[i]))
            return false;
    }
    return true;
}

bool CompositeEditCommand::shouldRebalanceLeadingWhitespaceFor(const String& text) const
{
    return containsOnlyDeprecatedEditingWhitespace(text);
}

bool CompositeEditCommand::canRebalance(const Position& position) const
{
    Node* node = position.containerNode();
    if (position.anchorType() != Position::PositionIsOffsetInAnchor || !is<Text>(node))
        return false;

    Text& textNode = downcast<Text>(*node);
    if (!textNode.length())
        return false;

    node->document().updateStyleIfNeeded();

    RenderObject* renderer = textNode.renderer();
    if (renderer && !renderer->style().collapseWhiteSpace())
        return false;

    return true;
}

// FIXME: Doesn't go into text nodes that contribute adjacent text (siblings, cousins, etc).
void CompositeEditCommand::rebalanceWhitespaceAt(const Position& position)
{
    Node* node = position.containerNode();
    if (!canRebalance(position))
        return;

    // If the rebalance is for the single offset, and neither text[offset] nor text[offset - 1] are some form of whitespace, do nothing.
    int offset = position.deprecatedEditingOffset();
    String text = downcast<Text>(*node).data();
    if (!deprecatedIsEditingWhitespace(text[offset])) {
        offset--;
        if (offset < 0 || !deprecatedIsEditingWhitespace(text[offset]))
            return;
    }

    rebalanceWhitespaceOnTextSubstring(downcast<Text>(*node), position.offsetInContainerNode(), position.offsetInContainerNode());
}

void CompositeEditCommand::rebalanceWhitespaceOnTextSubstring(Text& textNode, int startOffset, int endOffset)
{
    String text = textNode.data();
    ASSERT(!text.isEmpty());

    // Set upstream and downstream to define the extent of the whitespace surrounding text[offset].
    int upstream = startOffset;
    while (upstream > 0 && deprecatedIsEditingWhitespace(text[upstream - 1]))
        upstream--;
    
    int downstream = endOffset;
    while ((unsigned)downstream < text.length() && deprecatedIsEditingWhitespace(text[downstream]))
        downstream++;
    
    int length = downstream - upstream;
    if (!length)
        return;

    VisiblePosition visibleUpstreamPos(Position(&textNode, upstream));
    VisiblePosition visibleDownstreamPos(Position(&textNode, downstream));
    
    String string = text.substring(upstream, length);
    String rebalancedString = stringWithRebalancedWhitespace(string,
    // FIXME: Because of the problem mentioned at the top of this function, we must also use nbsps at the start/end of the string because
    // this function doesn't get all surrounding whitespace, just the whitespace in the current text node.
                                                             isStartOfParagraph(visibleUpstreamPos) || upstream == 0, 
                                                             isEndOfParagraph(visibleDownstreamPos) || (unsigned)downstream == text.length());
    
    if (string != rebalancedString)
        replaceTextInNodePreservingMarkers(textNode, upstream, length, rebalancedString);
}

void CompositeEditCommand::prepareWhitespaceAtPositionForSplit(Position& position)
{
    Node* node = position.deprecatedNode();
    if (!is<Text>(node))
        return;
    Text& textNode = downcast<Text>(*node);
    
    if (!textNode.length())
        return;
    RenderObject* renderer = textNode.renderer();
    if (renderer && !renderer->style().collapseWhiteSpace())
        return;

    // Delete collapsed whitespace so that inserting nbsps doesn't uncollapse it.
    Position upstreamPos = position.upstream();
    deleteInsignificantText(position.upstream(), position.downstream());
    position = upstreamPos.downstream();

    VisiblePosition visiblePos(position);
    VisiblePosition previousVisiblePos(visiblePos.previous());
    Position previous(previousVisiblePos.deepEquivalent());
    
    if (deprecatedIsCollapsibleWhitespace(previousVisiblePos.characterAfter()) && is<Text>(*previous.deprecatedNode()) && !is<HTMLBRElement>(*previous.deprecatedNode()))
        replaceTextInNodePreservingMarkers(downcast<Text>(*previous.deprecatedNode()), previous.deprecatedEditingOffset(), 1, nonBreakingSpaceString());
    if (deprecatedIsCollapsibleWhitespace(visiblePos.characterAfter()) && is<Text>(*position.deprecatedNode()) && !is<HTMLBRElement>(*position.deprecatedNode()))
        replaceTextInNodePreservingMarkers(downcast<Text>(*position.deprecatedNode()), position.deprecatedEditingOffset(), 1, nonBreakingSpaceString());
}

void CompositeEditCommand::rebalanceWhitespace()
{
    VisibleSelection selection = endingSelection();
    if (selection.isNone())
        return;
        
    rebalanceWhitespaceAt(selection.start());
    if (selection.isRange())
        rebalanceWhitespaceAt(selection.end());
}

void CompositeEditCommand::deleteInsignificantText(Text& textNode, unsigned start, unsigned end)
{
    if (start >= end)
        return;

    document().updateLayout();

    RenderText* textRenderer = textNode.renderer();
    if (!textRenderer)
        return;

    Vector<InlineTextBox*> sortedTextBoxes;
    size_t sortedTextBoxesPosition = 0;
   
    for (InlineTextBox* textBox = textRenderer->firstTextBox(); textBox; textBox = textBox->nextTextBox())
        sortedTextBoxes.append(textBox);
    
    // If there is mixed directionality text, the boxes can be out of order,
    // (like Arabic with embedded LTR), so sort them first. 
    if (textRenderer->containsReversedText())    
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), InlineTextBox::compareByStart);
    InlineTextBox* box = sortedTextBoxes.isEmpty() ? 0 : sortedTextBoxes[sortedTextBoxesPosition];

    if (!box) {
        // whole text node is empty
        removeNode(textNode);
        return;    
    }
    
    unsigned length = textNode.length();
    if (start >= length || end > length)
        return;

    unsigned removed = 0;
    InlineTextBox* prevBox = nullptr;
    String str;

    // This loop structure works to process all gaps preceding a box,
    // and also will look at the gap after the last box.
    while (prevBox || box) {
        unsigned gapStart = prevBox ? prevBox->start() + prevBox->len() : 0;
        if (end < gapStart)
            // No more chance for any intersections
            break;

        unsigned gapEnd = box ? box->start() : length;
        bool indicesIntersect = start <= gapEnd && end >= gapStart;
        int gapLen = gapEnd - gapStart;
        if (indicesIntersect && gapLen > 0) {
            gapStart = std::max(gapStart, start);
            gapEnd = std::min(gapEnd, end);
            if (str.isNull())
                str = textNode.data().substring(start, end - start);
            // remove text in the gap
            str.remove(gapStart - start - removed, gapLen);
            removed += gapLen;
        }
        
        prevBox = box;
        if (box) {
            if (++sortedTextBoxesPosition < sortedTextBoxes.size())
                box = sortedTextBoxes[sortedTextBoxesPosition];
            else
                box = nullptr;
        }
    }

    if (!str.isNull()) {
        // Replace the text between start and end with our pruned version.
        if (!str.isEmpty())
            replaceTextInNode(textNode, start, end - start, str);
        else {
            // Assert that we are not going to delete all of the text in the node.
            // If we were, that should have been done above with the call to 
            // removeNode and return.
            ASSERT(start > 0 || end - start < textNode.length());
            deleteTextFromNode(textNode, start, end - start);
        }
    }
}

void CompositeEditCommand::deleteInsignificantText(const Position& start, const Position& end)
{
    if (start.isNull() || end.isNull())
        return;

    if (comparePositions(start, end) >= 0)
        return;

    Vector<Ref<Text>> nodes;
    for (Node* node = start.deprecatedNode(); node; node = NodeTraversal::next(*node)) {
        if (is<Text>(*node))
            nodes.append(downcast<Text>(*node));
        if (node == end.deprecatedNode())
            break;
    }

    for (auto& textNode : nodes) {
        int startOffset = textNode.ptr() == start.deprecatedNode() ? start.deprecatedEditingOffset() : 0;
        int endOffset = textNode.ptr() == end.deprecatedNode() ? end.deprecatedEditingOffset() : static_cast<int>(textNode->length());
        deleteInsignificantText(textNode, startOffset, endOffset);
    }
}

void CompositeEditCommand::deleteInsignificantTextDownstream(const Position& pos)
{
    Position end = VisiblePosition(pos, VP_DEFAULT_AFFINITY).next().deepEquivalent().downstream();
    deleteInsignificantText(pos, end);
}

Ref<Element> CompositeEditCommand::appendBlockPlaceholder(Ref<Element>&& container)
{
    document().updateLayoutIgnorePendingStylesheets();
    
    // Should assert isBlockFlow || isInlineFlow when deletion improves. See 4244964.
    ASSERT(container->renderer());

    auto placeholder = createBlockPlaceholderElement(document());
    appendNode(placeholder.copyRef(), WTFMove(container));
    return placeholder;
}

RefPtr<Node> CompositeEditCommand::insertBlockPlaceholder(const Position& pos)
{
    if (pos.isNull())
        return nullptr;

    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(pos.deprecatedNode()->renderer());

    auto placeholder = createBlockPlaceholderElement(document());
    insertNodeAt(placeholder.copyRef(), pos);
    return WTFMove(placeholder);
}

RefPtr<Node> CompositeEditCommand::addBlockPlaceholderIfNeeded(Element* container)
{
    if (!container)
        return nullptr;

    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = container->renderer();
    if (!is<RenderBlockFlow>(renderer))
        return nullptr;
    
    // Append the placeholder to make sure it follows any unrendered blocks.
    auto& blockFlow = downcast<RenderBlockFlow>(*renderer);
    if (!blockFlow.height() || (blockFlow.isListItem() && !blockFlow.firstChild()))
        return appendBlockPlaceholder(*container);

    return nullptr;
}

// Assumes that the position is at a placeholder and does the removal without much checking.
void CompositeEditCommand::removePlaceholderAt(const Position& p)
{
    ASSERT(lineBreakExistsAtPosition(p));
    
    // We are certain that the position is at a line break, but it may be a br or a preserved newline.
    if (is<HTMLBRElement>(*p.anchorNode())) {
        removeNode(*p.anchorNode());
        return;
    }
    
    deleteTextFromNode(downcast<Text>(*p.anchorNode()), p.offsetInContainerNode(), 1);
}

Ref<HTMLElement> CompositeEditCommand::insertNewDefaultParagraphElementAt(const Position& position)
{
    auto paragraphElement = createDefaultParagraphElement(document());
    paragraphElement->appendChild(HTMLBRElement::create(document()));
    insertNodeAt(paragraphElement.copyRef(), position);
    return paragraphElement;
}

// If the paragraph is not entirely within it's own block, create one and move the paragraph into 
// it, and return that block.  Otherwise return 0.
RefPtr<Node> CompositeEditCommand::moveParagraphContentsToNewBlockIfNecessary(const Position& pos)
{
    if (pos.isNull())
        return nullptr;
    
    document().updateLayoutIgnorePendingStylesheets();
    
    // It's strange that this function is responsible for verifying that pos has not been invalidated
    // by an earlier call to this function.  The caller, applyBlockStyle, should do this.
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    VisiblePosition visibleParagraphStart(startOfParagraph(visiblePos));
    VisiblePosition visibleParagraphEnd = endOfParagraph(visiblePos);
    VisiblePosition next = visibleParagraphEnd.next();
    VisiblePosition visibleEnd = next.isNotNull() ? next : visibleParagraphEnd;
    
    Position upstreamStart = visibleParagraphStart.deepEquivalent().upstream();
    Position upstreamEnd = visibleEnd.deepEquivalent().upstream();

    // If there are no VisiblePositions in the same block as pos then 
    // upstreamStart will be outside the paragraph
    if (comparePositions(pos, upstreamStart) < 0)
        return nullptr;

    // Perform some checks to see if we need to perform work in this function.
    if (isBlock(upstreamStart.deprecatedNode())) {
        // If the block is the root editable element, always move content to a new block,
        // since it is illegal to modify attributes on the root editable element for editing.
        if (upstreamStart.deprecatedNode() == editableRootForPosition(upstreamStart)) {
            // If the block is the root editable element and it contains no visible content, create a new
            // block but don't try and move content into it, since there's nothing for moveParagraphs to move.
            if (!Position::hasRenderedNonAnonymousDescendantsWithHeight(downcast<RenderElement>(*upstreamStart.deprecatedNode()->renderer())))
                return insertNewDefaultParagraphElementAt(upstreamStart);
        } else if (isBlock(upstreamEnd.deprecatedNode())) {
            if (!upstreamEnd.deprecatedNode()->isDescendantOf(upstreamStart.deprecatedNode())) {
                // If the paragraph end is a descendant of paragraph start, then we need to run
                // the rest of this function. If not, we can bail here.
                return nullptr;
            }
        } else if (enclosingBlock(upstreamEnd.deprecatedNode()) != upstreamStart.deprecatedNode()) {
            // The visibleEnd. If it is an ancestor of the paragraph start, then
            // we can bail as we have a full block to work with.
            if (upstreamStart.deprecatedNode()->isDescendantOf(enclosingBlock(upstreamEnd.deprecatedNode())))
                return nullptr;
        } else if (isEndOfEditableOrNonEditableContent(visibleEnd)) {
            // At the end of the editable region. We can bail here as well.
            return nullptr;
        }
    }

    // If upstreamStart is not editable, then we can bail here.
    if (!isEditablePosition(upstreamStart))
        return nullptr;
    auto newBlock = insertNewDefaultParagraphElementAt(upstreamStart);

    bool endWasBr = visibleParagraphEnd.deepEquivalent().deprecatedNode()->hasTagName(brTag);

    moveParagraphs(visibleParagraphStart, visibleParagraphEnd, VisiblePosition(firstPositionInNode(newBlock.ptr())));

    if (newBlock->lastChild() && newBlock->lastChild()->hasTagName(brTag) && !endWasBr)
        removeNode(*newBlock->lastChild());

    return WTFMove(newBlock);
}

void CompositeEditCommand::pushAnchorElementDown(Element& anchorElement)
{
    ASSERT(anchorElement.isLink());
    
    setEndingSelection(VisibleSelection::selectionFromContentsOfNode(&anchorElement));
    applyStyledElement(anchorElement);
    // Clones of anchorElement have been pushed down, now remove it.
    if (anchorElement.isConnected())
        removeNodePreservingChildren(anchorElement);
}

// Clone the paragraph between start and end under blockElement,
// preserving the hierarchy up to outerNode. 

void CompositeEditCommand::cloneParagraphUnderNewElement(const Position& start, const Position& end, Node* passedOuterNode, Element* blockElement)
{
    ASSERT(comparePositions(start, end) <= 0);

    // First we clone the outerNode
    RefPtr<Node> lastNode;
    RefPtr<Node> outerNode = passedOuterNode;

    if (outerNode->isRootEditableElement()) {
        lastNode = blockElement;
    } else {
        lastNode = outerNode->cloneNode(isRenderedTable(outerNode.get()));
        appendNode(*lastNode, *blockElement);
    }

    if (start.deprecatedNode() != outerNode && lastNode->isElementNode() && start.anchorNode()->isDescendantOf(outerNode.get())) {
        Vector<RefPtr<Node>> ancestors;
        
        // Insert each node from innerNode to outerNode (excluded) in a list.
        for (Node* n = start.deprecatedNode(); n && n != outerNode; n = n->parentNode())
            ancestors.append(n);

        // Clone every node between start.deprecatedNode() and outerBlock.

        for (size_t i = ancestors.size(); i != 0; --i) {
            Node* item = ancestors[i - 1].get();
            auto child = item->cloneNode(isRenderedTable(item));
            appendNode(child.copyRef(), downcast<Element>(*lastNode));
            lastNode = WTFMove(child);
        }
    }

    // Handle the case of paragraphs with more than one node,
    // cloning all the siblings until end.deprecatedNode() is reached.
    
    if (start.deprecatedNode() != end.deprecatedNode() && !start.deprecatedNode()->isDescendantOf(end.deprecatedNode())) {
        // If end is not a descendant of outerNode we need to
        // find the first common ancestor to increase the scope
        // of our nextSibling traversal.
        while (!end.deprecatedNode()->isDescendantOf(outerNode.get())) {
            outerNode = outerNode->parentNode();
        }

        RefPtr<Node> startNode = start.deprecatedNode();
        for (RefPtr<Node> node = NodeTraversal::nextSkippingChildren(*startNode, outerNode.get()); node; node = NodeTraversal::nextSkippingChildren(*node, outerNode.get())) {
            // Move lastNode up in the tree as much as node was moved up in the
            // tree by NodeTraversal::nextSkippingChildren, so that the relative depth between
            // node and the original start node is maintained in the clone.
            while (startNode->parentNode() != node->parentNode()) {
                startNode = startNode->parentNode();
                lastNode = lastNode->parentNode();
            }

            auto clonedNode = node->cloneNode(true);
            insertNodeAfter(clonedNode.copyRef(), *lastNode);
            lastNode = WTFMove(clonedNode);
            if (node == end.deprecatedNode() || end.deprecatedNode()->isDescendantOf(*node))
                break;
        }
    }
}

    
// There are bugs in deletion when it removes a fully selected table/list.
// It expands and removes the entire table/list, but will let content
// before and after the table/list collapse onto one line.   
// Deleting a paragraph will leave a placeholder. Remove it (and prune
// empty or unrendered parents).

void CompositeEditCommand::cleanupAfterDeletion(VisiblePosition destination)
{
    VisiblePosition caretAfterDelete = endingSelection().visibleStart();
    if (!caretAfterDelete.equals(destination) && isStartOfParagraph(caretAfterDelete) && isEndOfParagraph(caretAfterDelete)) {
        // Note: We want the rightmost candidate.
        Position position = caretAfterDelete.deepEquivalent().downstream();
        Node* node = position.deprecatedNode();
        ASSERT(node);
        // Normally deletion will leave a br as a placeholder.
        if (is<HTMLBRElement>(*node))
            removeNodeAndPruneAncestors(*node);
        // If the selection to move was empty and in an empty block that 
        // doesn't require a placeholder to prop itself open (like a bordered
        // div or an li), remove it during the move (the list removal code
        // expects this behavior).
        else if (isBlock(node)) {
            // If caret position after deletion and destination position coincides,
            // node should not be removed.
            if (!position.rendersInDifferentPosition(destination.deepEquivalent())) {
                prune(node);
                return;
            }
            removeNodeAndPruneAncestors(*node);
        }
        else if (lineBreakExistsAtPosition(position)) {
            // There is a preserved '\n' at caretAfterDelete.
            // We can safely assume this is a text node.
            Text& textNode = downcast<Text>(*node);
            if (textNode.length() == 1)
                removeNodeAndPruneAncestors(textNode);
            else
                deleteTextFromNode(textNode, position.deprecatedEditingOffset(), 1);
        }
    }
}
    
// This is a version of moveParagraph that preserves style by keeping the original markup
// It is currently used only by IndentOutdentCommand but it is meant to be used in the
// future by several other commands such as InsertList and the align commands.
// The blockElement parameter is the element to move the paragraph to,
// outerNode is the top element of the paragraph hierarchy. 

void CompositeEditCommand::moveParagraphWithClones(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, Element* blockElement, Node* outerNode)
{
    if (startOfParagraphToMove.isNull() || endOfParagraphToMove.isNull())
        return;
    
    ASSERT(outerNode);
    ASSERT(blockElement);

    VisiblePosition beforeParagraph = startOfParagraphToMove.previous();
    VisiblePosition afterParagraph(endOfParagraphToMove.next());
    
    // We upstream() the end and downstream() the start so that we don't include collapsed whitespace in the move.
    // When we paste a fragment, spaces after the end and before the start are treated as though they were rendered.
    Position start = startOfParagraphToMove.deepEquivalent().downstream();
    Position end = startOfParagraphToMove == endOfParagraphToMove ? start : endOfParagraphToMove.deepEquivalent().upstream();

    cloneParagraphUnderNewElement(start, end, outerNode, blockElement);
      
    setEndingSelection(VisibleSelection(start, end, DOWNSTREAM));
    deleteSelection(false, false, false, false);
    
    // There are bugs in deletion when it removes a fully selected table/list.
    // It expands and removes the entire table/list, but will let content
    // before and after the table/list collapse onto one line.
       
    cleanupAfterDeletion();
    
    // Add a br if pruning an empty block level element caused a collapse.  For example:
    // foo^
    // <div>bar</div>
    // baz
    // Imagine moving 'bar' to ^.  'bar' will be deleted and its div pruned.  That would
    // cause 'baz' to collapse onto the line with 'foobar' unless we insert a br.
    // Must recononicalize these two VisiblePositions after the pruning above.
    beforeParagraph = VisiblePosition(beforeParagraph.deepEquivalent());
    afterParagraph = VisiblePosition(afterParagraph.deepEquivalent());

    if (beforeParagraph.isNotNull() && !isRenderedTable(beforeParagraph.deepEquivalent().deprecatedNode())
        && ((!isEndOfParagraph(beforeParagraph) && !isStartOfParagraph(beforeParagraph)) || beforeParagraph == afterParagraph)
        && isEditablePosition(beforeParagraph.deepEquivalent())) {
        // FIXME: Trim text between beforeParagraph and afterParagraph if they aren't equal.
        insertNodeAt(HTMLBRElement::create(document()), beforeParagraph.deepEquivalent());
    }
}
    
    
// This moves a paragraph preserving its style.
void CompositeEditCommand::moveParagraph(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, const VisiblePosition& destination, bool preserveSelection, bool preserveStyle)
{
    ASSERT(isStartOfParagraph(startOfParagraphToMove));
    ASSERT(isEndOfParagraph(endOfParagraphToMove));
    moveParagraphs(startOfParagraphToMove, endOfParagraphToMove, destination, preserveSelection, preserveStyle);
}

void CompositeEditCommand::moveParagraphs(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, const VisiblePosition& destination, bool preserveSelection, bool preserveStyle)
{
    if (startOfParagraphToMove == destination)
        return;
    
    int startIndex = -1;
    int endIndex = -1;
    int destinationIndex = -1;
    bool originalIsDirectional = endingSelection().isDirectional();
    if (preserveSelection && !endingSelection().isNone()) {
        VisiblePosition visibleStart = endingSelection().visibleStart();
        VisiblePosition visibleEnd = endingSelection().visibleEnd();
        
        bool startAfterParagraph = comparePositions(visibleStart, endOfParagraphToMove) > 0;
        bool endBeforeParagraph = comparePositions(visibleEnd, startOfParagraphToMove) < 0;
        
        if (!startAfterParagraph && !endBeforeParagraph) {
            bool startInParagraph = comparePositions(visibleStart, startOfParagraphToMove) >= 0;
            bool endInParagraph = comparePositions(visibleEnd, endOfParagraphToMove) <= 0;
            
            startIndex = 0;
            if (startInParagraph) {
                RefPtr<Range> startRange = Range::create(document(), startOfParagraphToMove.deepEquivalent().parentAnchoredEquivalent(), visibleStart.deepEquivalent().parentAnchoredEquivalent());
                startIndex = TextIterator::rangeLength(startRange.get(), true);
            }

            endIndex = 0;
            if (endInParagraph) {
                RefPtr<Range> endRange = Range::create(document(), startOfParagraphToMove.deepEquivalent().parentAnchoredEquivalent(), visibleEnd.deepEquivalent().parentAnchoredEquivalent());
                endIndex = TextIterator::rangeLength(endRange.get(), true);
            }
        }
    }
    
    VisiblePosition beforeParagraph = startOfParagraphToMove.previous(CannotCrossEditingBoundary);
    VisiblePosition afterParagraph(endOfParagraphToMove.next(CannotCrossEditingBoundary));

    // We upstream() the end and downstream() the start so that we don't include collapsed whitespace in the move.
    // When we paste a fragment, spaces after the end and before the start are treated as though they were rendered.
    Position start = startOfParagraphToMove.deepEquivalent().downstream();
    Position end = endOfParagraphToMove.deepEquivalent().upstream();
     
    // start and end can't be used directly to create a Range; they are "editing positions"
    Position startRangeCompliant = start.parentAnchoredEquivalent();
    Position endRangeCompliant = end.parentAnchoredEquivalent();
    RefPtr<Range> range = Range::create(document(), startRangeCompliant.deprecatedNode(), startRangeCompliant.deprecatedEditingOffset(), endRangeCompliant.deprecatedNode(), endRangeCompliant.deprecatedEditingOffset());

    // FIXME: This is an inefficient way to preserve style on nodes in the paragraph to move. It
    // shouldn't matter though, since moved paragraphs will usually be quite small.
    RefPtr<DocumentFragment> fragment;
    // This used to use a ternary for initialization, but that confused some versions of GCC, see bug 37912
    if (startOfParagraphToMove != endOfParagraphToMove)
        fragment = createFragmentFromMarkup(document(), serializePreservingVisualAppearance(*range, nullptr, AnnotateForInterchange::No, ConvertBlocksToInlines::Yes), emptyString());

    // A non-empty paragraph's style is moved when we copy and move it.  We don't move 
    // anything if we're given an empty paragraph, but an empty paragraph can have style
    // too, <div><b><br></b></div> for example.  Save it so that we can preserve it later.
    RefPtr<EditingStyle> styleInEmptyParagraph;
#if !PLATFORM(IOS)
    if (startOfParagraphToMove == endOfParagraphToMove && preserveStyle) {
#else
    if (startOfParagraphToMove == endOfParagraphToMove && preserveStyle && isRichlyEditablePosition(destination.deepEquivalent())) {
#endif
        styleInEmptyParagraph = EditingStyle::create(startOfParagraphToMove.deepEquivalent());
        styleInEmptyParagraph->mergeTypingStyle(document());
        // The moved paragraph should assume the block style of the destination.
        styleInEmptyParagraph->removeBlockProperties();
    }
    
    // FIXME (5098931): We should add a new insert action "WebViewInsertActionMoved" and call shouldInsertFragment here.
    
    setEndingSelection(VisibleSelection(start, end, DOWNSTREAM));
    frame().editor().clearMisspellingsAndBadGrammar(endingSelection());
    deleteSelection(false, false, false, false);

    ASSERT(destination.deepEquivalent().anchorNode()->isConnected());
    cleanupAfterDeletion(destination);
    ASSERT(destination.deepEquivalent().anchorNode()->isConnected());

    // Add a br if pruning an empty block level element caused a collapse. For example:
    // foo^
    // <div>bar</div>
    // baz
    // Imagine moving 'bar' to ^. 'bar' will be deleted and its div pruned. That would
    // cause 'baz' to collapse onto the line with 'foobar' unless we insert a br.
    // Must recononicalize these two VisiblePositions after the pruning above.
    beforeParagraph = VisiblePosition(beforeParagraph.deepEquivalent());
    afterParagraph = VisiblePosition(afterParagraph.deepEquivalent());
    if (beforeParagraph.isNotNull() && ((!isStartOfParagraph(beforeParagraph) && !isEndOfParagraph(beforeParagraph)) || beforeParagraph == afterParagraph)) {
        // FIXME: Trim text between beforeParagraph and afterParagraph if they aren't equal.
        insertNodeAt(HTMLBRElement::create(document()), beforeParagraph.deepEquivalent());
        // Need an updateLayout here in case inserting the br has split a text node.
        document().updateLayoutIgnorePendingStylesheets();
    }

    RefPtr<Range> startToDestinationRange(Range::create(document(), firstPositionInNode(document().documentElement()), destination.deepEquivalent().parentAnchoredEquivalent()));
    destinationIndex = TextIterator::rangeLength(startToDestinationRange.get(), true);

    setEndingSelection(VisibleSelection(destination, originalIsDirectional));
    ASSERT(endingSelection().isCaretOrRange());
    OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::SelectReplacement, ReplaceSelectionCommand::MovingParagraph };
    if (!preserveStyle)
        options.add(ReplaceSelectionCommand::MatchStyle);
    applyCommandToComposite(ReplaceSelectionCommand::create(document(), WTFMove(fragment), options));

    frame().editor().markMisspellingsAndBadGrammar(endingSelection());

    // If the selection is in an empty paragraph, restore styles from the old empty paragraph to the new empty paragraph.
    bool selectionIsEmptyParagraph = endingSelection().isCaret() && isStartOfParagraph(endingSelection().visibleStart()) && isEndOfParagraph(endingSelection().visibleStart());
    if (styleInEmptyParagraph && selectionIsEmptyParagraph)
        applyStyle(styleInEmptyParagraph.get());

    if (preserveSelection && startIndex != -1) {
        // Fragment creation (using createMarkup) incorrectly uses regular
        // spaces instead of nbsps for some spaces that were rendered (11475), which
        // causes spaces to be collapsed during the move operation.  This results
        // in a call to rangeFromLocationAndLength with a location past the end
        // of the document (which will return null).
        RefPtr<Range> start = TextIterator::rangeFromLocationAndLength(document().documentElement(), destinationIndex + startIndex, 0, true);
        RefPtr<Range> end = TextIterator::rangeFromLocationAndLength(document().documentElement(), destinationIndex + endIndex, 0, true);
        if (start && end)
            setEndingSelection(VisibleSelection(start->startPosition(), end->startPosition(), DOWNSTREAM, originalIsDirectional));
    }
}

std::optional<VisibleSelection> CompositeEditCommand::shouldBreakOutOfEmptyListItem() const
{
    auto emptyListItem = enclosingEmptyListItem(endingSelection().visibleStart());
    if (!emptyListItem)
        return std::nullopt;

    auto listNode = emptyListItem->parentNode();
    // FIXME: Can't we do something better when the immediate parent wasn't a list node?
    if (!listNode
        || (!listNode->hasTagName(ulTag) && !listNode->hasTagName(olTag))
        || !listNode->hasEditableStyle()
        || listNode == emptyListItem->rootEditableElement())
        return std::nullopt;

    return VisibleSelection(endingSelection().start().previous(BackwardDeletion), endingSelection().end());
}

// FIXME: Send an appropriate shouldDeleteRange call.
bool CompositeEditCommand::breakOutOfEmptyListItem()
{
    if (!shouldBreakOutOfEmptyListItem())
        return false;

    auto emptyListItem = enclosingEmptyListItem(endingSelection().visibleStart());
    auto listNode = emptyListItem->parentNode();
    auto style = EditingStyle::create(endingSelection().start());
    style->mergeTypingStyle(document());

    RefPtr<Element> newBlock;
    if (ContainerNode* blockEnclosingList = listNode->parentNode()) {
        if (is<HTMLLIElement>(*blockEnclosingList)) { // listNode is inside another list item
            if (visiblePositionAfterNode(*blockEnclosingList) == visiblePositionAfterNode(*listNode)) {
                // If listNode appears at the end of the outer list item, then move listNode outside of this list item
                // e.g. <ul><li>hello <ul><li><br></li></ul> </li></ul> should become <ul><li>hello</li> <ul><li><br></li></ul> </ul> after this section
                // If listNode does NOT appear at the end, then we should consider it as a regular paragraph.
                // e.g. <ul><li> <ul><li><br></li></ul> hello</li></ul> should become <ul><li> <div><br></div> hello</li></ul> at the end
                splitElement(downcast<HTMLLIElement>(*blockEnclosingList), *listNode);
                removeNodePreservingChildren(*listNode->parentNode());
                newBlock = HTMLLIElement::create(document());
            }
            // If listNode does NOT appear at the end of the outer list item, then behave as if in a regular paragraph.
        } else if (blockEnclosingList->hasTagName(olTag) || blockEnclosingList->hasTagName(ulTag))
            newBlock = HTMLLIElement::create(document());
    }
    if (!newBlock)
        newBlock = createDefaultParagraphElement(document());

    RefPtr<Node> previousListNode = emptyListItem->isElementNode() ? ElementTraversal::previousSibling(*emptyListItem): emptyListItem->previousSibling();
    RefPtr<Node> nextListNode = emptyListItem->isElementNode() ? ElementTraversal::nextSibling(*emptyListItem): emptyListItem->nextSibling();
    if (isListItem(nextListNode.get()) || isListHTMLElement(nextListNode.get())) {
        // If emptyListItem follows another list item or nested list, split the list node.
        if (isListItem(previousListNode.get()) || isListHTMLElement(previousListNode.get()))
            splitElement(downcast<Element>(*listNode), *emptyListItem);

        // If emptyListItem is followed by other list item or nested list, then insert newBlock before the list node.
        // Because we have splitted the element, emptyListItem is the first element in the list node.
        // i.e. insert newBlock before ul or ol whose first element is emptyListItem
        insertNodeBefore(*newBlock, *listNode);
        removeNode(*emptyListItem);
    } else {
        // When emptyListItem does not follow any list item or nested list, insert newBlock after the enclosing list node.
        // Remove the enclosing node if emptyListItem is the only child; otherwise just remove emptyListItem.
        insertNodeAfter(*newBlock, *listNode);
        removeNode(isListItem(previousListNode.get()) || isListHTMLElement(previousListNode.get()) ? *emptyListItem : *listNode);
    }

    appendBlockPlaceholder(*newBlock);
    setEndingSelection(VisibleSelection(firstPositionInNode(newBlock.get()), DOWNSTREAM, endingSelection().isDirectional()));

    style->prepareToApplyAt(endingSelection().start());
    if (!style->isEmpty())
        applyStyle(style.ptr());

    return true;
}

// If the caret is in an empty quoted paragraph, and either there is nothing before that
// paragraph, or what is before is unquoted, and the user presses delete, unquote that paragraph.
bool CompositeEditCommand::breakOutOfEmptyMailBlockquotedParagraph()
{
    if (!endingSelection().isCaret())
        return false;
        
    VisiblePosition caret(endingSelection().visibleStart());
    Node* highestBlockquote = highestEnclosingNodeOfType(caret.deepEquivalent(), &isMailBlockquote);
    if (!highestBlockquote)
        return false;

    if (!isStartOfParagraph(caret) || !isEndOfParagraph(caret))
        return false;

    VisiblePosition previous(caret.previous(CannotCrossEditingBoundary));
    // Only move forward if there's nothing before the caret, or if there's unquoted content before it.
    if (enclosingNodeOfType(previous.deepEquivalent(), &isMailBlockquote))
        return false;
    
    auto br = HTMLBRElement::create(document());
    auto* brPtr = br.ptr();
    // We want to replace this quoted paragraph with an unquoted one, so insert a br
    // to hold the caret before the highest blockquote.
    insertNodeBefore(WTFMove(br), *highestBlockquote);
    VisiblePosition atBR(positionBeforeNode(brPtr));
    // If the br we inserted collapsed, for example foo<br><blockquote>...</blockquote>, insert
    // a second one.
    if (!isStartOfParagraph(atBR))
        insertNodeBefore(HTMLBRElement::create(document()), *brPtr);
    setEndingSelection(VisibleSelection(atBR, endingSelection().isDirectional()));
    
    // If this is an empty paragraph there must be a line break here.
    if (!lineBreakExistsAtVisiblePosition(caret))
        return false;

    Position caretPos(caret.deepEquivalent().downstream());
    // A line break is either a br or a preserved newline.
    ASSERT(caretPos.deprecatedNode()->hasTagName(brTag) || (caretPos.deprecatedNode()->isTextNode() && caretPos.deprecatedNode()->renderer()->style().preserveNewline()));
    
    if (caretPos.deprecatedNode()->hasTagName(brTag))
        removeNodeAndPruneAncestors(*caretPos.deprecatedNode());
    else if (is<Text>(*caretPos.deprecatedNode())) {
        ASSERT(caretPos.deprecatedEditingOffset() == 0);
        Text& textNode = downcast<Text>(*caretPos.deprecatedNode());
        ContainerNode* parentNode = textNode.parentNode();
        // The preserved newline must be the first thing in the node, since otherwise the previous
        // paragraph would be quoted, and we verified that it wasn't above.
        deleteTextFromNode(textNode, 0, 1);
        prune(parentNode);
    }

    return true;
}

// Operations use this function to avoid inserting content into an anchor when at the start or the end of
// that anchor, as in NSTextView.
// FIXME: This is only an approximation of NSTextViews insertion behavior, which varies depending on how
// the caret was made. 
Position CompositeEditCommand::positionAvoidingSpecialElementBoundary(const Position& original)
{
    if (original.isNull())
        return original;
        
    VisiblePosition visiblePos(original);
    Element* enclosingAnchor = enclosingAnchorElement(original);
    Position result = original;

    if (!enclosingAnchor)
        return result;

    // Don't avoid block level anchors, because that would insert content into the wrong paragraph.
    if (enclosingAnchor && !isBlock(enclosingAnchor)) {
        VisiblePosition firstInAnchor(firstPositionInNode(enclosingAnchor));
        VisiblePosition lastInAnchor(lastPositionInNode(enclosingAnchor));
        // If visually just after the anchor, insert *inside* the anchor unless it's the last
        // VisiblePosition in the document, to match NSTextView.
        if (visiblePos == lastInAnchor) {
            // Make sure anchors are pushed down before avoiding them so that we don't
            // also avoid structural elements like lists and blocks (5142012).
            if (original.deprecatedNode() != enclosingAnchor && original.deprecatedNode()->parentNode() != enclosingAnchor) {
                pushAnchorElementDown(*enclosingAnchor);
                enclosingAnchor = enclosingAnchorElement(original);
                if (!enclosingAnchor)
                    return original;
            }
            // Don't insert outside an anchor if doing so would skip over a line break.  It would
            // probably be safe to move the line break so that we could still avoid the anchor here.
            Position downstream(visiblePos.deepEquivalent().downstream());
            if (lineBreakExistsAtVisiblePosition(visiblePos) && downstream.deprecatedNode()->isDescendantOf(enclosingAnchor))
                return original;
            
            result = positionInParentAfterNode(enclosingAnchor);
        }
        // If visually just before an anchor, insert *outside* the anchor unless it's the first
        // VisiblePosition in a paragraph, to match NSTextView.
        if (visiblePos == firstInAnchor) {
            // Make sure anchors are pushed down before avoiding them so that we don't
            // also avoid structural elements like lists and blocks (5142012).
            if (original.deprecatedNode() != enclosingAnchor && original.deprecatedNode()->parentNode() != enclosingAnchor) {
                pushAnchorElementDown(*enclosingAnchor);
                enclosingAnchor = enclosingAnchorElement(original);
            }
            if (!enclosingAnchor)
                return original;

            result = positionInParentBeforeNode(enclosingAnchor);
        }
    }
        
    if (result.isNull() || !editableRootForPosition(result))
        result = original;
    
    return result;
}

// Splits the tree parent by parent until we reach the specified ancestor. We use VisiblePositions
// to determine if the split is necessary. Returns the last split node.
RefPtr<Node> CompositeEditCommand::splitTreeToNode(Node& start, Node& end, bool shouldSplitAncestor)
{
    ASSERT(&start != &end);

    RefPtr<Node> adjustedEnd = &end;
    if (shouldSplitAncestor && adjustedEnd->parentNode())
        adjustedEnd = adjustedEnd->parentNode();

    ASSERT(adjustedEnd);
    RefPtr<Node> node;
    for (node = &start; node && node->parentNode() != adjustedEnd; node = node->parentNode()) {
        if (!is<Element>(*node->parentNode()))
            break;
        // Do not split a node when doing so introduces an empty node.
        VisiblePosition positionInParent = firstPositionInNode(node->parentNode());
        VisiblePosition positionInNode = firstPositionInOrBeforeNode(node.get());
        if (positionInParent != positionInNode)
            splitElement(downcast<Element>(*node->parentNode()), *node);
    }

    return node;
}

Ref<Element> createBlockPlaceholderElement(Document& document)
{
    return HTMLBRElement::create(document);
}

} // namespace WebCore
