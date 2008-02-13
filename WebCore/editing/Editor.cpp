/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Editor.h"

#include "AXObjectCache.h"
#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "ClipboardEvent.h"
#include "DeleteButtonController.h"
#include "DeleteSelectionCommand.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "EditorClient.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FontData.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "KeyboardEvent.h"
#include "ModifySelectionListLevel.h"
#include "Page.h"
#include "Pasteboard.h"
#include "RemoveFormatCommand.h"
#include "ReplaceSelectionCommand.h"
#include "Sound.h"
#include "Text.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "markup.h"
#include "visible_units.h"

namespace WebCore {

using namespace std;
using namespace EventNames;
using namespace HTMLNames;

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
Selection Editor::selectionForCommand(Event* event)
{
    Selection selection = m_frame->selectionController()->selection();
    if (!event)
        return selection;
    // If the target is a text control, and the current selection is outside of its shadow tree,
    // then use the saved selection for that text control.
    Node* target = event->target()->toNode();
    Node* selectionStart = selection.start().node();
    if (target && (!selectionStart || target->shadowAncestorNode() != selectionStart->shadowAncestorNode())) {
        if (target->hasTagName(inputTag) && static_cast<HTMLInputElement*>(target)->isTextField())
            return static_cast<HTMLInputElement*>(target)->selection();
        if (target->hasTagName(textareaTag))
            return static_cast<HTMLTextAreaElement*>(target)->selection();
    }
    return selection;
}

EditorClient* Editor::client() const
{
    if (Page* page = m_frame->page())
        return page->editorClient();
    return 0;
}

void Editor::handleKeyboardEvent(KeyboardEvent* event)
{
    if (EditorClient* c = client())
        if (selectionForCommand(event).isContentEditable())
            c->handleKeyboardEvent(event);
}

void Editor::handleInputMethodKeydown(KeyboardEvent* event)
{
    if (EditorClient* c = client())
        if (selectionForCommand(event).isContentEditable())
            c->handleInputMethodKeydown(event);
}

bool Editor::canEdit() const
{
    return m_frame->selectionController()->isContentEditable();
}

bool Editor::canEditRichly() const
{
    return m_frame->selectionController()->isContentRichlyEditable();
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu items.  They
// also send onbeforecopy, apparently for symmetry, but it doesn't affect the menu items.
// We need to use onbeforecopy as a real menu enabler because we allow elements that are not
// normally selectable to implement copy/paste (like divs, or a document body).

bool Editor::canDHTMLCut()
{
    return !m_frame->selectionController()->isInPasswordField() && !dispatchCPPEvent(beforecutEvent, ClipboardNumb);
}

bool Editor::canDHTMLCopy()
{
    return !m_frame->selectionController()->isInPasswordField() && !dispatchCPPEvent(beforecopyEvent, ClipboardNumb);
}

bool Editor::canDHTMLPaste()
{
    return !dispatchCPPEvent(beforepasteEvent, ClipboardNumb);
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

static HTMLImageElement* imageElementFromImageDocument(Document* document)
{
    if (!document)
        return 0;
    if (!document->isImageDocument())
        return 0;
    
    HTMLElement* body = document->body();
    if (!body)
        return 0;
    
    Node* node = body->firstChild();
    if (!node)
        return 0;    
    if (!node->hasTagName(imgTag))
        return 0;
    return static_cast<HTMLImageElement*>(node);
}

bool Editor::canCopy() const
{
    if (imageElementFromImageDocument(m_frame->document()))
        return true;
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && !selectionController->isInPasswordField();
}

bool Editor::canPaste() const
{
    return canEdit();
}

bool Editor::canDelete() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && selectionController->isContentEditable();
}

bool Editor::canDeleteRange(Range* range) const
{
    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    Node* endContainer = range->endContainer(ec);
    if (!startContainer || !endContainer)
        return false;
    
    if (!startContainer->isContentEditable() || !endContainer->isContentEditable())
        return false;
    
    if (range->collapsed(ec)) {
        VisiblePosition start(startContainer, range->startOffset(ec), DOWNSTREAM);
        VisiblePosition previous = start.previous();
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().node()->rootEditableElement() != startContainer->rootEditableElement())
            return false;
    }
    return true;
}

bool Editor::smartInsertDeleteEnabled()
{   
    return client() && client()->smartInsertDeleteEnabled();
}
    
bool Editor::canSmartCopyOrDelete()
{
    return client() && client()->smartInsertDeleteEnabled() && m_frame->selectionGranularity() == WordGranularity;
}

bool Editor::deleteWithDirection(SelectionController::EDirection direction, TextGranularity granularity, bool killRing, bool isTypingAction)
{
    // Delete the selection, if there is one.
    // If not, make a selection using the passed-in direction and granularity.

    if (!canEdit())
        return false;

    if (m_frame->selectionController()->isRange()) {
        if (killRing)
            addToKillRing(selectedRange().get(), false);
        if (isTypingAction) {
            if (m_frame->document()) {
                TypingCommand::deleteKeyPressed(m_frame->document(), canSmartCopyOrDelete(), granularity);
                revealSelectionAfterEditingOperation();
            }
        } else {
            deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
            // Implicitly calls revealSelectionAfterEditingOperation().
        }
    } else {
        SelectionController selectionToDelete;
        selectionToDelete.setSelection(m_frame->selectionController()->selection());
        selectionToDelete.modify(SelectionController::EXTEND, direction, granularity);
        if (killRing && selectionToDelete.isCaret() && granularity != CharacterGranularity)
            selectionToDelete.modify(SelectionController::EXTEND, direction, CharacterGranularity);

        RefPtr<Range> range = selectionToDelete.toRange();

        if (killRing)
            addToKillRing(range.get(), false);

        if (!m_frame->selectionController()->setSelectedRange(range.get(), DOWNSTREAM, (granularity != CharacterGranularity)))
            return true;

        switch (direction) {
            case SelectionController::FORWARD:
            case SelectionController::RIGHT:
                if (m_frame->document())
                    TypingCommand::forwardDeleteKeyPressed(m_frame->document(), false, granularity);
                break;
            case SelectionController::BACKWARD:
            case SelectionController::LEFT:
                if (m_frame->document())
                    TypingCommand::deleteKeyPressed(m_frame->document(), false, granularity);
                break;
        }
        revealSelectionAfterEditingOperation();
    }

    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    if (killRing)
        setStartNewKillRingSequence(false);

    return true;
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete)
{
    if (m_frame->selectionController()->isNone())
        return;
    
    applyCommand(new DeleteSelectionCommand(m_frame->document(), smartDelete));
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard* pasteboard)
{
    String text = pasteboard->plainText(m_frame);
    if (client() && client()->shouldInsertText(text, selectedRange().get(), EditorInsertActionPasted))
        replaceSelectionWithText(text, false, canSmartReplaceWithPasteboard(pasteboard));
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText)
{
    RefPtr<Range> range = selectedRange();
    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = pasteboard->documentFragment(m_frame, range, allowPlainText, chosePlainText);
    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        replaceSelectionWithFragment(fragment, false, canSmartReplaceWithPasteboard(pasteboard), chosePlainText);
}

bool Editor::canSmartReplaceWithPasteboard(Pasteboard* pasteboard)
{
    return client() && client()->smartInsertDeleteEnabled() && pasteboard->canSmartReplace();
}

bool Editor::shouldInsertFragment(PassRefPtr<DocumentFragment> fragment, PassRefPtr<Range> replacingDOMRange, EditorInsertAction givenAction)
{
    if (!client())
        return false;
        
    Node* child = fragment->firstChild();
    if (child && fragment->lastChild() == child && child->isCharacterDataNode())
        return client()->shouldInsertText(static_cast<CharacterData*>(child)->data(), replacingDOMRange.get(), givenAction);

    return client()->shouldInsertNode(fragment.get(), replacingDOMRange.get(), givenAction);
}

void Editor::replaceSelectionWithFragment(PassRefPtr<DocumentFragment> fragment, bool selectReplacement, bool smartReplace, bool matchStyle)
{
    if (m_frame->selectionController()->isNone() || !fragment)
        return;
    
    applyCommand(new ReplaceSelectionCommand(m_frame->document(), fragment, selectReplacement, smartReplace, matchStyle));
    revealSelectionAfterEditingOperation();
}

void Editor::replaceSelectionWithText(const String& text, bool selectReplacement, bool smartReplace)
{
    replaceSelectionWithFragment(createFragmentFromText(selectedRange().get(), text), selectReplacement, smartReplace, true); 
}

PassRefPtr<Range> Editor::selectedRange()
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->toRange();
}

bool Editor::shouldDeleteRange(Range* range) const
{
    ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return false;
    
    if (!canDeleteRange(range))
        return false;

    return client() && client()->shouldDeleteRange(range);
}

bool Editor::tryDHTMLCopy()
{   
    if (m_frame->selectionController()->isInPasswordField())
        return false;

    // Must be done before oncopy adds types and data to the pboard,
    // also done for security, as it erases data from the last copy/paste.
    Pasteboard::generalPasteboard()->clear();

    return !dispatchCPPEvent(copyEvent, ClipboardWritable);
}

bool Editor::tryDHTMLCut()
{
    if (m_frame->selectionController()->isInPasswordField())
        return false;

    // Must be done before oncut adds types and data to the pboard,
    // also done for security, as it erases data from the last copy/paste.
    Pasteboard::generalPasteboard()->clear();

    return !dispatchCPPEvent(cutEvent, ClipboardWritable);
}

bool Editor::tryDHTMLPaste()
{
    return !dispatchCPPEvent(pasteEvent, ClipboardReadable);
}

void Editor::writeSelectionToPasteboard(Pasteboard* pasteboard)
{
    pasteboard->writeSelection(selectedRange().get(), canSmartCopyOrDelete(), m_frame);
}

bool Editor::shouldInsertText(const String& text, Range* range, EditorInsertAction action) const
{
    return client() && client()->shouldInsertText(text, range, action);
}

bool Editor::shouldShowDeleteInterface(HTMLElement* element) const
{
    return client() && client()->shouldShowDeleteInterface(element);
}

void Editor::respondToChangedSelection(const Selection& oldSelection)
{
    if (client())
        client()->respondToChangedSelection();
    m_deleteButtonController->respondToChangedSelection(oldSelection);
}

void Editor::respondToChangedContents(const Selection& endingSelection)
{
    if (AXObjectCache::accessibilityEnabled()) {
        Node* node = endingSelection.start().node();
        if (node)
            m_frame->renderer()->document()->axObjectCache()->postNotification(node->renderer(), "AXValueChanged");
    }
    
    if (client())
        client()->respondToChangedContents();  
}

const SimpleFontData* Editor::fontForSelection(bool& hasMultipleFonts) const
{
#if !PLATFORM(QT)
    hasMultipleFonts = false;

    if (!m_frame->selectionController()->isRange()) {
        Node* nodeToRemove;
        RenderStyle* style = m_frame->styleForSelectionStart(nodeToRemove); // sets nodeToRemove

        const SimpleFontData* result = 0;
        if (style)
            result = style->font().primaryFont();
        
        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(ec == 0);
        }

        return result;
    }

    const SimpleFontData* font = 0;

    RefPtr<Range> range = m_frame->selectionController()->toRange();
    Node* startNode = range->editingStartPosition().node();
    if (startNode) {
        Node* pastEnd = range->pastEndNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for nil also.
        for (Node* n = startNode; n && n != pastEnd; n = n->traverseNextNode()) {
            RenderObject *renderer = n->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            const SimpleFontData* f = renderer->style()->font().primaryFont();
            if (!font)
                font = f;
            else if (font != f) {
                hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
#else
    return 0;
#endif
}

TriState Editor::selectionUnorderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        if (enclosingNodeWithTag(m_frame->selectionController()->selection().start(), ulTag))
            return TrueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start(), ulTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end(), ulTag);
        if (startNode && endNode && startNode == endNode)
            return TrueTriState;
    }

    return FalseTriState;
}

TriState Editor::selectionOrderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        if (enclosingNodeWithTag(m_frame->selectionController()->selection().start(), olTag))
            return TrueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start(), olTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end(), olTag);
        if (startNode && endNode && startNode == endNode)
            return TrueTriState;
    }

    return FalseTriState;
}

PassRefPtr<Node> Editor::insertOrderedList()
{
    if (!canEditRichly())
        return 0;
        
    RefPtr<Node> newList = InsertListCommand::insertList(m_frame->document(), InsertListCommand::OrderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

PassRefPtr<Node> Editor::insertUnorderedList()
{
    if (!canEditRichly())
        return 0;
        
    RefPtr<Node> newList = InsertListCommand::insertList(m_frame->document(), InsertListCommand::UnorderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

bool Editor::canIncreaseSelectionListLevel()
{
    return canEditRichly() && IncreaseSelectionListLevelCommand::canIncreaseSelectionListLevel(m_frame->document());
}

bool Editor::canDecreaseSelectionListLevel()
{
    return canEditRichly() && DecreaseSelectionListLevelCommand::canDecreaseSelectionListLevel(m_frame->document());
}

PassRefPtr<Node> Editor::increaseSelectionListLevel()
{
    if (!canEditRichly() || m_frame->selectionController()->isNone())
        return 0;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevel(m_frame->document());
    revealSelectionAfterEditingOperation();
    return newList;
}

PassRefPtr<Node> Editor::increaseSelectionListLevelOrdered()
{
    if (!canEditRichly() || m_frame->selectionController()->isNone())
        return 0;
    
    PassRefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(m_frame->document());
    revealSelectionAfterEditingOperation();
    return newList;
}

PassRefPtr<Node> Editor::increaseSelectionListLevelUnordered()
{
    if (!canEditRichly() || m_frame->selectionController()->isNone())
        return 0;
    
    PassRefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(m_frame->document());
    revealSelectionAfterEditingOperation();
    return newList;
}

void Editor::decreaseSelectionListLevel()
{
    if (!canEditRichly() || m_frame->selectionController()->isNone())
        return;
    
    DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(m_frame->document());
    revealSelectionAfterEditingOperation();
}

void Editor::removeFormattingAndStyle()
{
    applyCommand(new RemoveFormatCommand(m_frame->document()));
}

void Editor::setLastEditCommand(PassRefPtr<EditCommand> lastEditCommand) 
{
    m_lastEditCommand = lastEditCommand;
}

// Returns whether caller should continue with "the default processing", which is the same as 
// the event handler NOT setting the return value to false
bool Editor::dispatchCPPEvent(const AtomicString &eventType, ClipboardAccessPolicy policy)
{
    Node* target = m_frame->selectionController()->start().element();
    if (!target && m_frame->document())
        target = m_frame->document()->body();
    if (!target)
        return true;
    target = target->shadowAncestorNode();
    
    RefPtr<Clipboard> clipboard = newGeneralClipboard(policy);

    ExceptionCode ec = 0;
    RefPtr<Event> evt = new ClipboardEvent(eventType, true, true, clipboard.get());
    EventTargetNodeCast(target)->dispatchEvent(evt, ec, true);
    bool noDefaultProcessing = evt->defaultPrevented();

    // invalidate clipboard here for security
    clipboard->setAccessPolicy(ClipboardNumb);
    
    return !noDefaultProcessing;
}

void Editor::applyStyle(CSSStyleDeclaration* style, EditAction editingAction)
{
    switch (m_frame->selectionController()->state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET:
            m_frame->computeAndSetTypingStyle(style, editingAction);
            break;
        case Selection::RANGE:
            if (m_frame->document() && style)
                applyCommand(new ApplyStyleCommand(m_frame->document(), style, editingAction));
            break;
    }
}
    
bool Editor::shouldApplyStyle(CSSStyleDeclaration* style, Range* range)
{   
    return client()->shouldApplyStyle(style, range);
}
    
void Editor::applyParagraphStyle(CSSStyleDeclaration* style, EditAction editingAction)
{
    switch (m_frame->selectionController()->state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET:
        case Selection::RANGE:
            if (m_frame->document() && style)
                applyCommand(new ApplyStyleCommand(m_frame->document(), style, editingAction, ApplyStyleCommand::ForceBlockProperties));
            break;
    }
}

void Editor::applyStyleToSelection(CSSStyleDeclaration* style, EditAction editingAction)
{
    if (!style || style->length() == 0 || !canEditRichly())
        return;

    if (client() && client()->shouldApplyStyle(style, m_frame->selectionController()->toRange().get()))
        applyStyle(style, editingAction);
}

void Editor::applyParagraphStyleToSelection(CSSStyleDeclaration* style, EditAction editingAction)
{
    if (!style || style->length() == 0 || !canEditRichly())
        return;
    
    if (client() && client()->shouldApplyStyle(style, m_frame->selectionController()->toRange().get()))
        applyParagraphStyle(style, editingAction);
}

bool Editor::clientIsEditable() const
{
    return client() && client()->isEditable();
}

bool Editor::selectionStartHasStyle(CSSStyleDeclaration* style) const
{
    Node* nodeToRemove;
    RefPtr<CSSComputedStyleDeclaration> selectionStyle = m_frame->selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return false;
    
    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();
    
    bool match = true;
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = mutableStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        if (!equalIgnoringCase(mutableStyle->getPropertyValue(propertyID), selectionStyle->getPropertyValue(propertyID))) {
            match = false;
            break;
        }
    }
    
    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        ASSERT(ec == 0);
    }
    
    return match;
}

static void updateState(CSSMutableStyleDeclaration* desiredStyle, CSSComputedStyleDeclaration* computedStyle, bool& atStart, TriState& state)
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = desiredStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        String desiredProperty = desiredStyle->getPropertyValue(propertyID);
        String computedProperty = computedStyle->getPropertyValue(propertyID);
        TriState propertyState = equalIgnoringCase(desiredProperty, computedProperty)
            ? TrueTriState : FalseTriState;
        if (atStart) {
            state = propertyState;
            atStart = false;
        } else if (state != propertyState) {
            state = MixedTriState;
            break;
        }
    }
}

TriState Editor::selectionHasStyle(CSSStyleDeclaration* style) const
{
    bool atStart = true;
    TriState state = FalseTriState;

    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();

    if (!m_frame->selectionController()->isRange()) {
        Node* nodeToRemove;
        RefPtr<CSSComputedStyleDeclaration> selectionStyle = m_frame->selectionComputedStyle(nodeToRemove);
        if (!selectionStyle)
            return FalseTriState;
        updateState(mutableStyle.get(), selectionStyle.get(), atStart, state);
        if (nodeToRemove) {
            ExceptionCode ec = 0;
            nodeToRemove->remove(ec);
            ASSERT(ec == 0);
        }
    } else {
        for (Node* node = m_frame->selectionController()->start().node(); node; node = node->traverseNextNode()) {
            RefPtr<CSSComputedStyleDeclaration> computedStyle = new CSSComputedStyleDeclaration(node);
            if (computedStyle)
                updateState(mutableStyle.get(), computedStyle.get(), atStart, state);
            if (state == MixedTriState)
                break;
            if (node == m_frame->selectionController()->end().node())
                break;
        }
    }

    return state;
}
void Editor::indent()
{
    applyCommand(new IndentOutdentCommand(m_frame->document(), IndentOutdentCommand::Indent));
}

void Editor::outdent()
{
    applyCommand(new IndentOutdentCommand(m_frame->document(), IndentOutdentCommand::Outdent));
}

static void dispatchEditableContentChangedEvents(const EditCommand& command)
{
    Element* startRoot = command.startingRootEditableElement();
    Element* endRoot = command.endingRootEditableElement();
    ExceptionCode ec;
    if (startRoot)
        startRoot->dispatchEvent(new Event(webkitEditableContentChangedEvent, false, false), ec, true);
    if (endRoot && endRoot != startRoot)
        endRoot->dispatchEvent(new Event(webkitEditableContentChangedEvent, false, false), ec, true);
}

void Editor::appliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
    
    // FIXME: We shouldn't tell setSelection to clear the typing style or removed anchor here.
    // If we didn't, we wouldn't have to save/restore the removedAnchor, and we wouldn't have to have
    // the typing style stored in two places (the Frame and commands).
    RefPtr<Node> anchor = removedAnchor();
    
    Selection newSelection(cmd->endingSelection());
    // If there is no selection change, don't bother sending shouldChangeSelection, but still call setSelection,
    // because there is work that it must do in this situation.
    // The old selection can be invalid here and calling shouldChangeSelection can produce some strange calls.
    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    if (newSelection == m_frame->selectionController()->selection() || m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, false);
    
    setRemovedAnchor(anchor);
    
    // Now set the typing style from the command. Clear it when done.
    // This helps make the case work where you completely delete a piece
    // of styled text and then type a character immediately after.
    // That new character needs to take on the style of the just-deleted text.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (cmd->typingStyle()) {
        m_frame->setTypingStyle(cmd->typingStyle());
        cmd->setTypingStyle(0);
    }
    
    // Command will be equal to last edit command only in the case of typing
    if (m_lastEditCommand.get() == cmd)
        ASSERT(cmd->isTypingCommand());
    else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        m_lastEditCommand = cmd;
        if (client())
            client()->registerCommandForUndo(m_lastEditCommand);
    }
    respondToChangedContents(newSelection);    
}

void Editor::unappliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
    
    Selection newSelection(cmd->startingSelection());
    // If there is no selection change, don't bother sending shouldChangeSelection, but still call setSelection,
    // because there is work that it must do in this situation.
    // The old selection can be invalid here and calling shouldChangeSelection can produce some strange calls.
    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    if (newSelection == m_frame->selectionController()->selection() || m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, true);
    
    m_lastEditCommand = 0;
    if (client())
        client()->registerCommandForRedo(cmd);
    respondToChangedContents(newSelection);    
}

void Editor::reappliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
    
    Selection newSelection(cmd->endingSelection());
    // If there is no selection change, don't bother sending shouldChangeSelection, but still call setSelection,
    // because there is work that it must do in this situation.
    // The old selection can be invalid here and calling shouldChangeSelection can produce some strange calls.
    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    if (newSelection == m_frame->selectionController()->selection() || m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, true);
    
    m_lastEditCommand = 0;
    if (client())
        client()->registerCommandForUndo(cmd);
    respondToChangedContents(newSelection);    
}

Editor::Editor(Frame* frame)
    : m_frame(frame)
    , m_deleteButtonController(new DeleteButtonController(frame))
    , m_ignoreCompositionSelectionChange(false)
    , m_shouldStartNewKillRingSequence(false)
{ 
}

Editor::~Editor()
{
}

void Editor::clear()
{
    m_compositionNode = 0;
    m_customCompositionUnderlines.clear();
}

bool Editor::insertText(const String& text, Event* triggeringEvent)
{
    return m_frame->eventHandler()->handleTextInputEvent(text, triggeringEvent);
}

bool Editor::insertTextWithoutSendingTextEvent(const String& text, bool selectInsertedText, Event* triggeringEvent)
{
    if (text.isEmpty())
        return false;

    Selection selection = selectionForCommand(triggeringEvent);
    if (!selection.isContentEditable())
        return false;
    RefPtr<Range> range = selection.toRange();

    if (!shouldInsertText(text, range.get(), EditorInsertActionTyped))
        return true;

    // Get the selection to use for the event that triggered this insertText.
    // If the event handler changed the selection, we may want to use a different selection
    // that is contained in the event target.
    selection = selectionForCommand(triggeringEvent);
    if (selection.isContentEditable()) {
        if (Node* selectionStart = selection.start().node()) {
            RefPtr<Document> document = selectionStart->document();
            
            // Insert the text
            TypingCommand::insertText(document.get(), text, selection, selectInsertedText);

            // Reveal the current selection 
            if (Frame* editedFrame = document->frame())
                if (Page* page = editedFrame->page())
                    page->focusController()->focusedOrMainFrame()->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
        }
    }

    return true;
}

bool Editor::insertLineBreak()
{
    if (!canEdit())
        return false;

    if (!shouldInsertText("\n", m_frame->selectionController()->toRange().get(), EditorInsertActionTyped))
        return true;

    TypingCommand::insertLineBreak(m_frame->document());
    revealSelectionAfterEditingOperation();
    return true;
}

bool Editor::insertParagraphSeparator()
{
    if (!canEdit())
        return false;

    if (!canEditRichly())
        return insertLineBreak();

    if (!shouldInsertText("\n", m_frame->selectionController()->toRange().get(), EditorInsertActionTyped))
        return true;

    TypingCommand::insertParagraphSeparator(m_frame->document());
    revealSelectionAfterEditingOperation();
    return true;
}

void Editor::cut()
{
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut()) {
        systemBeep();
        return;
    }
    RefPtr<Range> selection = selectedRange();
    if (shouldDeleteRange(selection.get())) {
        Pasteboard::generalPasteboard()->writeSelection(selection.get(), canSmartCopyOrDelete(), m_frame);
        didWriteSelectionToPasteboard();
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
    }
}

void Editor::copy()
{
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy()) {
        systemBeep();
        return;
    }
    
    Document* document = m_frame->document();
    if (HTMLImageElement* imageElement = imageElementFromImageDocument(document))
        Pasteboard::generalPasteboard()->writeImage(imageElement, document->url(), document->title());
    else
        Pasteboard::generalPasteboard()->writeSelection(selectedRange().get(), canSmartCopyOrDelete(), m_frame);
    
    didWriteSelectionToPasteboard();
}

void Editor::paste()
{
    ASSERT(m_frame->document());
    DocLoader* loader = m_frame->document()->docLoader();
#if PLATFORM(MAC)
    // using the platform independent code below requires moving all of
    // WEBHTMLView: _documentFragmentFromPasteboard over to PasteboardMac.
    loader->setAllowStaleResources(true);
    m_frame->issuePasteCommand();
    loader->setAllowStaleResources(false);
#else
    if (tryDHTMLPaste())
        return;     // DHTML did the whole operation
    if (!canPaste())
        return;
    loader->setAllowStaleResources(true);
    if (m_frame->selectionController()->isContentRichlyEditable())
        pasteWithPasteboard(Pasteboard::generalPasteboard(), true);
    else
        pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
    loader->setAllowStaleResources(false);
#endif
}

void Editor::pasteAsPlainText()
{
   if (!canPaste())
        return;
   pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::performDelete()
{
    if (!canDelete()) {
        systemBeep();
        return;
    }

    addToKillRing(selectedRange().get(), false);
    deleteSelectionWithSmartDelete(canSmartCopyOrDelete());

    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    setStartNewKillRingSequence(false);
}

void Editor::copyURL(const KURL& url, const String& title)
{
    Pasteboard::generalPasteboard()->writeURL(url, title, m_frame);
}

void Editor::copyImage(const HitTestResult& result)
{
    KURL url = result.absoluteLinkURL();
    if (url.isEmpty())
        url = result.absoluteImageURL();

    Pasteboard::generalPasteboard()->writeImage(result.innerNonSharedNode(), url, result.altDisplayString());
}

bool Editor::isContinuousSpellCheckingEnabled()
{
    return client() && client()->isContinuousSpellCheckingEnabled();
}

void Editor::toggleContinuousSpellChecking()
{
    if (client())
        client()->toggleContinuousSpellChecking();
}

bool Editor::isGrammarCheckingEnabled()
{
    return client() && client()->isGrammarCheckingEnabled();
}

void Editor::toggleGrammarChecking()
{
    if (client())
        client()->toggleGrammarChecking();
}

int Editor::spellCheckerDocumentTag()
{
    return client() ? client()->spellCheckerDocumentTag() : 0;
}

bool Editor::shouldEndEditing(Range* range)
{
    return client() && client()->shouldEndEditing(range);
}

bool Editor::shouldBeginEditing(Range* range)
{
    return client() && client()->shouldBeginEditing(range);
}

void Editor::clearUndoRedoOperations()
{
    if (client())
        client()->clearUndoRedoOperations();
}

bool Editor::canUndo()
{
    return client() && client()->canUndo();
}

void Editor::undo()
{
    if (client())
        client()->undo();
}

bool Editor::canRedo()
{
    return client() && client()->canRedo();
}

void Editor::redo()
{
    if (client())
        client()->redo();
}

void Editor::didBeginEditing()
{
    if (client())
        client()->didBeginEditing();
}

void Editor::didEndEditing()
{
    if (client())
        client()->didEndEditing();
}

void Editor::didWriteSelectionToPasteboard()
{
    if (client())
        client()->didWriteSelectionToPasteboard();
}

void Editor::toggleBold()
{
    command("ToggleBold").execute();
}

void Editor::toggleUnderline()
{
    command("ToggleUnderline").execute();
}

void Editor::setBaseWritingDirection(const String& direction)
{
    ExceptionCode ec = 0;

    RefPtr<CSSMutableStyleDeclaration> style = new CSSMutableStyleDeclaration;
    style->setProperty(CSS_PROP_DIRECTION, direction, false, ec);
    applyParagraphStyleToSelection(style.get(), EditActionSetWritingDirection);
}

void Editor::selectComposition()
{
    RefPtr<Range> range = compositionRange();
    if (!range)
        return;
    
    // The composition can start inside a composed character sequence, so we have to override checks.
    // See <http://bugs.webkit.org/show_bug.cgi?id=15781>
    Selection selection;
    selection.setWithoutValidation(range->startPosition(), range->endPosition());
    m_frame->selectionController()->setSelection(selection, false, false);
}

void Editor::confirmComposition()
{
    if (!m_compositionNode)
        return;
    confirmComposition(m_compositionNode->data().substring(m_compositionStart, m_compositionEnd - m_compositionStart), false);
}

void Editor::confirmCompositionWithoutDisturbingSelection()
{
    if (!m_compositionNode)
        return;
    confirmComposition(m_compositionNode->data().substring(m_compositionStart, m_compositionEnd - m_compositionStart), true);
}

void Editor::confirmComposition(const String& text)
{
    confirmComposition(text, false);
}

void Editor::confirmComposition(const String& text, bool preserveSelection)
{
    setIgnoreCompositionSelectionChange(true);

    Selection oldSelection = m_frame->selectionController()->selection();

    selectComposition();

    if (m_frame->selectionController()->isNone()) {
        setIgnoreCompositionSelectionChange(false);
        return;
    }
    
    // If there is a composition to replace, remove it with a deletion that will be part of the
    // same Undo step as the next and previous insertions.
    TypingCommand::deleteSelection(m_frame->document(), false);

    m_compositionNode = 0;
    m_customCompositionUnderlines.clear();

    insertText(text, 0);

    if (preserveSelection)
        m_frame->selectionController()->setSelection(oldSelection, false, false);

    setIgnoreCompositionSelectionChange(false);
}

void Editor::setComposition(const String& text, const Vector<CompositionUnderline>& underlines, unsigned selectionStart, unsigned selectionEnd)
{
    setIgnoreCompositionSelectionChange(true);

    selectComposition();

    if (m_frame->selectionController()->isNone()) {
        setIgnoreCompositionSelectionChange(false);
        return;
    }
    
    // If there is a composition to replace, remove it with a deletion that will be part of the
    // same Undo step as the next and previous insertions.
    TypingCommand::deleteSelection(m_frame->document(), false);

    m_compositionNode = 0;
    m_customCompositionUnderlines.clear();

    if (!text.isEmpty()) {
        TypingCommand::insertText(m_frame->document(), text, true, true);

        Node* baseNode = m_frame->selectionController()->base().node();
        unsigned baseOffset = m_frame->selectionController()->base().offset();
        Node* extentNode = m_frame->selectionController()->extent().node();
        unsigned extentOffset = m_frame->selectionController()->extent().offset();

        if (baseNode && baseNode == extentNode && baseNode->isTextNode() && baseOffset + text.length() == extentOffset) {
            m_compositionNode = static_cast<Text*>(baseNode);
            m_compositionStart = baseOffset;
            m_compositionEnd = extentOffset;
            m_customCompositionUnderlines = underlines;
            size_t numUnderlines = m_customCompositionUnderlines.size();
            for (size_t i = 0; i < numUnderlines; ++i) {
                m_customCompositionUnderlines[i].startOffset += baseOffset;
                m_customCompositionUnderlines[i].endOffset += baseOffset;
            }
            if (baseNode->renderer())
                baseNode->renderer()->repaint();

            unsigned start = min(baseOffset + selectionStart, extentOffset);
            unsigned end = min(max(start, baseOffset + selectionEnd), extentOffset);
            RefPtr<Range> selectedRange = new Range(baseNode->document(), baseNode, start, baseNode, end);                
            m_frame->selectionController()->setSelectedRange(selectedRange.get(), DOWNSTREAM, false);
        }
    }

    setIgnoreCompositionSelectionChange(false);
}

void Editor::ignoreSpelling()
{
    if (!client())
        return;

    String text = frame()->selectedText();
    ASSERT(text.length() != 0);
    client()->ignoreWordInSpellDocument(text);
}

void Editor::learnSpelling()
{
    if (!client())
        return;

    String text = frame()->selectedText();
    ASSERT(text.length() != 0);
    client()->learnWord(text);
}

static String findFirstMisspellingInRange(EditorClient* client, Range* searchRange, int& firstMisspellingOffset, bool markAll)
{
    ASSERT_ARG(client, client);
    ASSERT_ARG(searchRange, searchRange);
    
    WordAwareIterator it(searchRange);
    firstMisspellingOffset = 0;
    
    String firstMisspelling;
    int currentChunkOffset = 0;

    while (!it.atEnd()) {
        const UChar* chars = it.characters();
        int len = it.length();
        
        // Skip some work for one-space-char hunks
        if (!(len == 1 && chars[0] == ' ')) {
            
            int misspellingLocation = -1;
            int misspellingLength = 0;
            client->checkSpellingOfString(chars, len, &misspellingLocation, &misspellingLength);

            // 5490627 shows that there was some code path here where the String constructor below crashes.
            // We don't know exactly what combination of bad input caused this, so we're making this much
            // more robust against bad input on release builds.
            ASSERT(misspellingLength >= 0);
            ASSERT(misspellingLocation >= -1);
            ASSERT(misspellingLength == 0 || misspellingLocation >= 0);
            ASSERT(misspellingLocation < len);
            ASSERT(misspellingLength <= len);
            ASSERT(misspellingLocation + misspellingLength <= len);
            
            if (misspellingLocation >= 0 && misspellingLength > 0 && misspellingLocation < len && misspellingLength <= len && misspellingLocation + misspellingLength <= len) {
                
                // Remember first-encountered misspelling and its offset
                if (!firstMisspelling) {
                    firstMisspellingOffset = currentChunkOffset + misspellingLocation;
                    firstMisspelling = String(chars + misspellingLocation, misspellingLength);
                }
                
                // Mark this instance if we're marking all instances. Otherwise bail out because we found the first one.
                if (!markAll)
                    break;
                
                // Compute range of misspelled word
                RefPtr<Range> misspellingRange = TextIterator::subrange(searchRange, currentChunkOffset + misspellingLocation, misspellingLength);
                
                // Store marker for misspelled word
                ExceptionCode ec = 0;
                misspellingRange->startContainer(ec)->document()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
                ASSERT(ec == 0);
            }
        }
        
        currentChunkOffset += len;
        it.advance();
    }
    
    return firstMisspelling;
}

#ifndef BUILDING_ON_TIGER

static PassRefPtr<Range> paragraphAlignedRangeForRange(Range* arbitraryRange, int& offsetIntoParagraphAlignedRange, String& paragraphString)
{
    ASSERT_ARG(arbitraryRange, arbitraryRange);
    
    ExceptionCode ec = 0;
    
    // Expand range to paragraph boundaries
    RefPtr<Range> paragraphRange = arbitraryRange->cloneRange(ec);
    setStart(paragraphRange.get(), startOfParagraph(arbitraryRange->startPosition()));
    setEnd(paragraphRange.get(), endOfParagraph(arbitraryRange->endPosition()));
    
    // Compute offset from start of expanded range to start of original range
    RefPtr<Range> offsetAsRange = new Range(paragraphRange->startContainer(ec)->document(), paragraphRange->startPosition(), arbitraryRange->startPosition());
    offsetIntoParagraphAlignedRange = TextIterator::rangeLength(offsetAsRange.get());
    
    // Fill in out parameter with string representing entire paragraph range.
    // Someday we might have a caller that doesn't use this, but for now all callers do.
    paragraphString = plainText(paragraphRange.get());

    return paragraphRange;
}

static int findFirstGrammarDetailInRange(const Vector<GrammarDetail>& grammarDetails, int badGrammarPhraseLocation, int badGrammarPhraseLength, Range *searchRange, int startOffset, int endOffset, bool markAll)
{
    // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
    // Optionally add a DocumentMarker for each detail in the range.
    int earliestDetailLocationSoFar = -1;
    int earliestDetailIndex = -1;
    for (unsigned i = 0; i < grammarDetails.size(); i++) {
        const GrammarDetail* detail = &grammarDetails[i];
        ASSERT(detail->length > 0 && detail->location >= 0);
        
        int detailStartOffsetInParagraph = badGrammarPhraseLocation + detail->location;
        
        // Skip this detail if it starts before the original search range
        if (detailStartOffsetInParagraph < startOffset)
            continue;
        
        // Skip this detail if it starts after the original search range
        if (detailStartOffsetInParagraph >= endOffset)
            continue;
        
        if (markAll) {
            RefPtr<Range> badGrammarRange = TextIterator::subrange(searchRange, badGrammarPhraseLocation - startOffset + detail->location, detail->length);
            ExceptionCode ec = 0;
            badGrammarRange->startContainer(ec)->document()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, detail->userDescription);
            ASSERT(ec == 0);
        }
        
        // Remember this detail only if it's earlier than our current candidate (the details aren't in a guaranteed order)
        if (earliestDetailIndex < 0 || earliestDetailLocationSoFar > detail->location) {
            earliestDetailIndex = i;
            earliestDetailLocationSoFar = detail->location;
        }
    }
    
    return earliestDetailIndex;
}
    
static String findFirstBadGrammarInRange(EditorClient* client, Range* searchRange, GrammarDetail& outGrammarDetail, int& outGrammarPhraseOffset, bool markAll)
{
    ASSERT_ARG(client, client);
    ASSERT_ARG(searchRange, searchRange);
    
    // Initialize out parameters; these will be updated if we find something to return.
    outGrammarDetail.location = -1;
    outGrammarDetail.length = 0;
    outGrammarDetail.guesses.clear();
    outGrammarDetail.userDescription = "";
    outGrammarPhraseOffset = 0;
    
    String firstBadGrammarPhrase;

    // Expand the search range to encompass entire paragraphs, since grammar checking needs that much context.
    // Determine the character offset from the start of the paragraph to the start of the original search range,
    // since we will want to ignore results in this area.
    int searchRangeStartOffset;
    String paragraphString;
    RefPtr<Range> paragraphRange = paragraphAlignedRangeForRange(searchRange, searchRangeStartOffset, paragraphString);
        
    // Determine the character offset from the start of the paragraph to the end of the original search range, 
    // since we will want to ignore results in this area also.
    int searchRangeEndOffset = searchRangeStartOffset + TextIterator::rangeLength(searchRange);
        
    // Start checking from beginning of paragraph, but skip past results that occur before the start of the original search range.
    int startOffset = 0;
    while (startOffset < searchRangeEndOffset) {
        Vector<GrammarDetail> grammarDetails;
        int badGrammarPhraseLocation = -1;
        int badGrammarPhraseLength = 0;
        client->checkGrammarOfString(paragraphString.characters() + startOffset, paragraphString.length() - startOffset, grammarDetails, &badGrammarPhraseLocation, &badGrammarPhraseLength);
        
        if (badGrammarPhraseLength == 0) {
            ASSERT(badGrammarPhraseLocation == -1);
            return String();
        }

        ASSERT(badGrammarPhraseLocation >= 0);
        badGrammarPhraseLocation += startOffset;

        
        // Found some bad grammar. Find the earliest detail range that starts in our search range (if any).
        int badGrammarIndex = findFirstGrammarDetailInRange(grammarDetails, badGrammarPhraseLocation, badGrammarPhraseLength, searchRange, searchRangeStartOffset, searchRangeEndOffset, markAll);
        if (badGrammarIndex >= 0) {
            ASSERT(static_cast<unsigned>(badGrammarIndex) < grammarDetails.size());
            outGrammarDetail = grammarDetails[badGrammarIndex];
        }

        // If we found a detail in range, then we have found the first bad phrase (unless we found one earlier but
        // kept going so we could mark all instances).
        if (badGrammarIndex >= 0 && firstBadGrammarPhrase.isEmpty()) {
            outGrammarPhraseOffset = badGrammarPhraseLocation - searchRangeStartOffset;
            firstBadGrammarPhrase = paragraphString.substring(badGrammarPhraseLocation, badGrammarPhraseLength);
            
            // Found one. We're done now, unless we're marking each instance.
            if (!markAll)
                break;
        }

        // These results were all between the start of the paragraph and the start of the search range; look
        // beyond this phrase.
        startOffset = badGrammarPhraseLocation + badGrammarPhraseLength;
    }
    
    return firstBadGrammarPhrase;
}
    
#endif /* not BUILDING_ON_TIGER */

void Editor::advanceToNextMisspelling(bool startBeforeSelection)
{
    ExceptionCode ec = 0;

    // The basic approach is to search in two phases - from the selection end to the end of the doc, and
    // then we wrap and search from the doc start to (approximately) where we started.
    
    // Start at the end of the selection, search to edge of document.  Starting at the selection end makes
    // repeated "check spelling" commands work.
    Selection selection(frame()->selectionController()->selection());
    RefPtr<Range> spellingSearchRange(rangeOfContents(frame()->document()));
    bool startedWithSelection = false;
    if (selection.start().node()) {
        startedWithSelection = true;
        if (startBeforeSelection) {
            VisiblePosition start(selection.visibleStart());
            // We match AppKit's rule: Start 1 character before the selection.
            VisiblePosition oneBeforeStart = start.previous();
            setStart(spellingSearchRange.get(), oneBeforeStart.isNotNull() ? oneBeforeStart : start);
        } else
            setStart(spellingSearchRange.get(), selection.visibleEnd());
    }

    Position position = spellingSearchRange->startPosition();
    if (!isEditablePosition(position)) {
        // This shouldn't happen in very often because the Spelling menu items aren't enabled unless the
        // selection is editable.
        // This can happen in Mail for a mix of non-editable and editable content (like Stationary), 
        // when spell checking the whole document before sending the message.
        // In that case the document might not be editable, but there are editable pockets that need to be spell checked.

        position = firstEditablePositionAfterPositionInRoot(position, frame()->document()->documentElement()).deepEquivalent();
        if (position.isNull())
            return;
        
        Position rangeCompliantPosition = rangeCompliantEquivalent(position);
        spellingSearchRange->setStart(rangeCompliantPosition.node(), rangeCompliantPosition.offset(), ec);
        startedWithSelection = false;   // won't need to wrap
    }
    
    // topNode defines the whole range we want to operate on 
    Node* topNode = highestEditableRoot(position);
    spellingSearchRange->setEnd(topNode, maxDeepOffset(topNode), ec);

    // If spellingSearchRange starts in the middle of a word, advance to the next word so we start checking
    // at a word boundary. Going back by one char and then forward by a word does the trick.
    if (startedWithSelection) {
        VisiblePosition oneBeforeStart = startVisiblePosition(spellingSearchRange.get(), DOWNSTREAM).previous();
        if (oneBeforeStart.isNotNull()) {
            setStart(spellingSearchRange.get(), endOfWord(oneBeforeStart));
        } // else we were already at the start of the editable node
    }
    
    if (spellingSearchRange->collapsed(ec))
        return;       // nothing to search in
    
    // Get the spell checker if it is available
    if (!client())
        return;
        
    // We go to the end of our first range instead of the start of it, just to be sure
    // we don't get foiled by any word boundary problems at the start.  It means we might
    // do a tiny bit more searching.
    Node *searchEndNodeAfterWrap = spellingSearchRange->endContainer(ec);
    int searchEndOffsetAfterWrap = spellingSearchRange->endOffset(ec);
    
    int misspellingOffset;
    String misspelledWord = findFirstMisspellingInRange(client(), spellingSearchRange.get(), misspellingOffset, false);
    
    String badGrammarPhrase;

#ifndef BUILDING_ON_TIGER
    int grammarPhraseOffset = 0;
    GrammarDetail grammarDetail;

    // Search for bad grammar that occurs prior to the next misspelled word (if any)
    RefPtr<Range> grammarSearchRange = spellingSearchRange->cloneRange(ec);
    if (!misspelledWord.isEmpty()) {
        // Stop looking at start of next misspelled word
        CharacterIterator chars(grammarSearchRange.get());
        chars.advance(misspellingOffset);
        grammarSearchRange->setEnd(chars.range()->startContainer(ec), chars.range()->startOffset(ec), ec);
    }
    
    if (isGrammarCheckingEnabled())
        badGrammarPhrase = findFirstBadGrammarInRange(client(), grammarSearchRange.get(), grammarDetail, grammarPhraseOffset, false);
#endif
    
    // If we found neither bad grammar nor a misspelled word, wrap and try again (but don't bother if we started at the beginning of the
    // block rather than at a selection).
    if (startedWithSelection && !misspelledWord && !badGrammarPhrase) {
        spellingSearchRange->setStart(topNode, 0, ec);
        // going until the end of the very first chunk we tested is far enough
        spellingSearchRange->setEnd(searchEndNodeAfterWrap, searchEndOffsetAfterWrap, ec);
        
        misspelledWord = findFirstMisspellingInRange(client(), spellingSearchRange.get(), misspellingOffset, false);

#ifndef BUILDING_ON_TIGER
        grammarSearchRange = spellingSearchRange->cloneRange(ec);
        if (!misspelledWord.isEmpty()) {
            // Stop looking at start of next misspelled word
            CharacterIterator chars(grammarSearchRange.get());
            chars.advance(misspellingOffset);
            grammarSearchRange->setEnd(chars.range()->startContainer(ec), chars.range()->startOffset(ec), ec);
        }
        if (isGrammarCheckingEnabled())
            badGrammarPhrase = findFirstBadGrammarInRange(client(), grammarSearchRange.get(), grammarDetail, grammarPhraseOffset, false);
#endif
    }
    
    if (!badGrammarPhrase.isEmpty()) {
#ifdef BUILDING_ON_TIGER
        ASSERT_NOT_REACHED();
#else
        // We found bad grammar. Since we only searched for bad grammar up to the first misspelled word, the bad grammar
        // takes precedence and we ignore any potential misspelled word. Select the grammar detail, update the spelling
        // panel, and store a marker so we draw the green squiggle later.
        
        ASSERT(badGrammarPhrase.length() > 0);
        ASSERT(grammarDetail.location != -1 && grammarDetail.length > 0);
        
        // FIXME 4859190: This gets confused with doubled punctuation at the end of a paragraph
        RefPtr<Range> badGrammarRange = TextIterator::subrange(grammarSearchRange.get(), grammarPhraseOffset + grammarDetail.location, grammarDetail.length);
        frame()->selectionController()->setSelection(Selection(badGrammarRange.get(), SEL_DEFAULT_AFFINITY));
        frame()->revealSelection();
        
        client()->updateSpellingUIWithGrammarString(badGrammarPhrase, grammarDetail);
        frame()->document()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, grammarDetail.userDescription);
#endif        
    } else if (!misspelledWord.isEmpty()) {
        // We found a misspelling, but not any earlier bad grammar. Select the misspelling, update the spelling panel, and store
        // a marker so we draw the red squiggle later.
        
        RefPtr<Range> misspellingRange = TextIterator::subrange(spellingSearchRange.get(), misspellingOffset, misspelledWord.length());
        frame()->selectionController()->setSelection(Selection(misspellingRange.get(), DOWNSTREAM));
        frame()->revealSelection();
        
        client()->updateSpellingUIWithMisspelledWord(misspelledWord);
        frame()->document()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
    }
}

bool Editor::isSelectionMisspelled()
{
    String selectedString = frame()->selectedText();
    int length = selectedString.length();
    if (length == 0)
        return false;

    if (!client())
        return false;
    
    int misspellingLocation = -1;
    int misspellingLength = 0;
    client()->checkSpellingOfString(selectedString.characters(), length, &misspellingLocation, &misspellingLength);
    
    // The selection only counts as misspelled if the selected text is exactly one misspelled word
    if (misspellingLength != length)
        return false;
    
    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    client()->updateSpellingUIWithMisspelledWord(selectedString);
    
    return true;
}

#ifndef BUILDING_ON_TIGER
static bool isRangeUngrammatical(EditorClient* client, Range *range, Vector<String>& guessesVector)
{
    if (!client)
        return false;

    ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return false;
    
    // Returns true only if the passed range exactly corresponds to a bad grammar detail range. This is analogous
    // to isSelectionMisspelled. It's not good enough for there to be some bad grammar somewhere in the range,
    // or overlapping the range; the ranges must exactly match.
    guessesVector.clear();
    int grammarPhraseOffset;
    
    GrammarDetail grammarDetail;
    String badGrammarPhrase = findFirstBadGrammarInRange(client, range, grammarDetail, grammarPhraseOffset, false);    
    
    // No bad grammar in these parts at all.
    if (badGrammarPhrase.isEmpty())
        return false;
    
    // Bad grammar, but phrase (e.g. sentence) starts beyond start of range.
    if (grammarPhraseOffset > 0)
        return false;
    
    ASSERT(grammarDetail.location >= 0 && grammarDetail.length > 0);
    
    // Bad grammar, but start of detail (e.g. ungrammatical word) doesn't match start of range
    if (grammarDetail.location + grammarPhraseOffset != 0)
        return false;
    
    // Bad grammar at start of range, but end of bad grammar is before or after end of range
    if (grammarDetail.length != TextIterator::rangeLength(range))
        return false;
    
    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    client->updateSpellingUIWithGrammarString(badGrammarPhrase, grammarDetail);
    
    return true;
}
#endif

bool Editor::isSelectionUngrammatical()
{
#ifdef BUILDING_ON_TIGER
    return false;
#else
    Vector<String> ignoredGuesses;
    return isRangeUngrammatical(client(), frame()->selectionController()->toRange().get(), ignoredGuesses);
#endif
}

Vector<String> Editor::guessesForUngrammaticalSelection()
{
#ifdef BUILDING_ON_TIGER
    return Vector<String>();
#else
    Vector<String> guesses;
    // Ignore the result of isRangeUngrammatical; we just want the guesses, whether or not there are any
    isRangeUngrammatical(client(), frame()->selectionController()->toRange().get(), guesses);
    return guesses;
#endif
}

Vector<String> Editor::guessesForMisspelledSelection()
{
    String selectedString = frame()->selectedText();
    ASSERT(selectedString.length() != 0);

    Vector<String> guesses;
    if (client())
        client()->getGuessesForWord(selectedString, guesses);
    return guesses;
}

void Editor::showSpellingGuessPanel()
{
    if (!client()) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

#ifndef BUILDING_ON_TIGER
    // Post-Tiger, this menu item is a show/hide toggle, to match AppKit. Leave Tiger behavior alone
    // to match rest of OS X.
    if (client()->spellingUIIsShowing()) {
        client()->showSpellingUI(false);
        return;
    }
#endif
    
    advanceToNextMisspelling(true);
    client()->showSpellingUI(true);
}

bool Editor::spellingPanelIsShowing()
{
    if (!client())
        return false;
    return client()->spellingUIIsShowing();
}

void Editor::markMisspellingsAfterTypingToPosition(const VisiblePosition &p)
{
    if (!isContinuousSpellCheckingEnabled())
        return;
    
    // Check spelling of one word
    markMisspellings(Selection(startOfWord(p, LeftWordIfOnBoundary), endOfWord(p, RightWordIfOnBoundary)));
    
    if (!isGrammarCheckingEnabled())
        return;
    
    // Check grammar of entire sentence
    markBadGrammar(Selection(startOfSentence(p), endOfSentence(p)));
}

static void markAllMisspellingsInRange(EditorClient* client, Range* searchRange)
{
    // Use the "markAll" feature of findFirstMisspellingInRange. Ignore the return value and the "out parameter";
    // all we need to do is mark every instance.
    int ignoredOffset;
    findFirstMisspellingInRange(client, searchRange, ignoredOffset, true);
}

#ifndef BUILDING_ON_TIGER
static void markAllBadGrammarInRange(EditorClient* client, Range* searchRange)
{
    // Use the "markAll" feature of findFirstBadGrammarInRange. Ignore the return value and "out parameters"; all we need to
    // do is mark every instance.
    GrammarDetail ignoredGrammarDetail;
    int ignoredOffset;
    findFirstBadGrammarInRange(client, searchRange, ignoredGrammarDetail, ignoredOffset, true);
}
#endif
    
static void markMisspellingsOrBadGrammar(Editor* editor, const Selection& selection, bool checkSpelling)
{
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.
    
    // This function is used only for as-you-type checking, so if that's off we do nothing. Note that
    // grammar checking can only be on if spell checking is also on.
    if (!editor->isContinuousSpellCheckingEnabled())
        return;
    
    RefPtr<Range> searchRange(selection.toRange());
    if (!searchRange || searchRange->isDetached())
        return;
    
    // If we're not in an editable node, bail.
    int exception = 0;
    Node *editableNode = searchRange->startContainer(exception);
    if (!editableNode->isContentEditable())
        return;
    
    // Get the spell checker if it is available
    if (!editor->client())
        return;
    
    if (checkSpelling)
        markAllMisspellingsInRange(editor->client(), searchRange.get());
    else {
#ifdef BUILDING_ON_TIGER
        ASSERT_NOT_REACHED();
#else
        if (editor->isGrammarCheckingEnabled())
            markAllBadGrammarInRange(editor->client(), searchRange.get());
#endif
    }    
}

void Editor::markMisspellings(const Selection& selection)
{
    markMisspellingsOrBadGrammar(this, selection, true);
}
    
void Editor::markBadGrammar(const Selection& selection)
{
#ifndef BUILDING_ON_TIGER
    markMisspellingsOrBadGrammar(this, selection, false);
#endif
}

PassRefPtr<Range> Editor::rangeForPoint(const IntPoint& windowPoint)
{
    Document* document = m_frame->documentAtPoint(windowPoint);
    if (!document)
        return 0;
    
    Frame* frame = document->frame();
    ASSERT(frame);
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    IntPoint framePoint = frameView->windowToContents(windowPoint);
    Selection selection(frame->visiblePositionForPoint(framePoint));
    return avoidIntersectionWithNode(selection.toRange().get(), deleteButtonController() ? deleteButtonController()->containerElement() : 0);
}

void Editor::revealSelectionAfterEditingOperation()
{
    if (m_ignoreCompositionSelectionChange)
        return;

    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

void Editor::setIgnoreCompositionSelectionChange(bool ignore)
{
    if (m_ignoreCompositionSelectionChange == ignore)
        return;

    m_ignoreCompositionSelectionChange = ignore;
    if (!ignore)
        revealSelectionAfterEditingOperation();
}

PassRefPtr<Range> Editor::compositionRange() const
{
    if (!m_compositionNode)
        return 0;
    unsigned length = m_compositionNode->length();
    unsigned start = min(m_compositionStart, length);
    unsigned end = min(max(start, m_compositionEnd), length);
    if (start >= end)
        return 0;
    return new Range(m_compositionNode->document(), m_compositionNode.get(), start, m_compositionNode.get(), end);
}

bool Editor::getCompositionSelection(unsigned& selectionStart, unsigned& selectionEnd) const
{
    if (!m_compositionNode)
        return false;
    Position start = m_frame->selectionController()->start();
    if (start.node() != m_compositionNode)
        return false;
    Position end = m_frame->selectionController()->end();
    if (end.node() != m_compositionNode)
        return false;

    if (static_cast<unsigned>(start.offset()) < m_compositionStart)
        return false;
    if (static_cast<unsigned>(end.offset()) > m_compositionEnd)
        return false;

    selectionStart = start.offset() - m_compositionStart;
    selectionEnd = start.offset() - m_compositionEnd;
    return true;
}

void Editor::transpose()
{
    if (!canEdit())
        return;

     Selection selection = m_frame->selectionController()->selection();
     if (!selection.isCaret())
         return;

    // Make a selection that goes back one character and forward two characters.
    VisiblePosition caret = selection.visibleStart();
    VisiblePosition next = isEndOfParagraph(caret) ? caret : caret.next();
    VisiblePosition previous = next.previous();
    if (next == previous)
        return;
    previous = previous.previous();
    if (!inSameParagraph(next, previous))
        return;
    RefPtr<Range> range = makeRange(previous, next);
    if (!range)
        return;
    Selection newSelection(range.get(), DOWNSTREAM);

    // Transpose the two characters.
    String text = plainText(range.get());
    if (text.length() != 2)
        return;
    String transposed = text.right(1) + text.left(1);

    // Select the two characters.
    if (newSelection != m_frame->selectionController()->selection()) {
        if (!m_frame->shouldChangeSelection(newSelection))
            return;
        m_frame->selectionController()->setSelection(newSelection);
    }

    // Insert the transposed characters.
    if (!shouldInsertText(transposed, range.get(), EditorInsertActionTyped))
        return;
    replaceSelectionWithText(transposed, false, false);
}

void Editor::addToKillRing(Range* range, bool prepend)
{
    if (m_shouldStartNewKillRingSequence)
        startNewKillRingSequence();

    String text = plainText(range);
    text.replace('\\', m_frame->backslashAsCurrencySymbol());
    if (prepend)
        prependToKillRing(text);
    else
        appendToKillRing(text);
    m_shouldStartNewKillRingSequence = false;
}

#if !PLATFORM(MAC)

void Editor::appendToKillRing(const String&)
{
}

void Editor::prependToKillRing(const String&)
{
}

String Editor::yankFromKillRing()
{
    return String();
}

void Editor::startNewKillRingSequence()
{
}

void Editor::setKillRingToYankedState()
{
}

#endif

} // namespace WebCore
