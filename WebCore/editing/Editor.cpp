/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "CharacterData.h"
#include "Clipboard.h"
#include "ClipboardEvent.h"
#include "DeleteButtonController.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FontData.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "IndentOutdentCommand.h"
#include "KeyboardEvent.h"
#include "KURL.h"
#include "Page.h"
#include "Pasteboard.h"
#include "Range.h"
#include "ReplaceSelectionCommand.h"
#include "SelectionController.h"
#include "Sound.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "markup.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
static Selection selectionForEvent(Frame* frame, Event* event)
{
    Page* page = frame->page();
    if (!page)
        return Selection();
    Selection selection = page->selection();
    if (!event)
        return selection;
    Node* target = event->target()->toNode();
    Node* selectionStart = selection.start().node();
    
    // If the target is a text control, and the current selection is outside of its shadow tree,
    // then use the saved selection for that text control.
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

void Editor::handleKeyPress(KeyboardEvent* event)
{
    if (EditorClient* c = client()) {
        Selection selection = selectionForEvent(m_frame, event);
        if (selection.isCaretOrRange() && selection.isContentEditable())
            c->handleKeyPress(event);
    }
}

bool Editor::canEdit() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isCaretOrRange() && selectionController->isContentEditable();
}

bool Editor::canEditRichly() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return canEdit() && selectionController->isContentRichlyEditable();
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

bool Editor::canCopy() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && !selectionController->isInPasswordField();
}

bool Editor::canPaste() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isCaretOrRange() && selectionController->isContentEditable();
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
    if (client())
        return client()->smartInsertDeleteEnabled();
    return false;
}
    
bool Editor::canSmartCopyOrDelete()
{
    if (client())
        return client()->smartInsertDeleteEnabled() && m_frame->selectionGranularity() == WordGranularity;
    return false;
}

void Editor::deleteRange(Range* range, bool killRing, bool prepend, bool smartDeleteOK, EditorDeleteAction deletionAction, TextGranularity granularity)
{
    if (killRing)
        addToKillRing(range, prepend);

    ExceptionCode ec = 0;

    SelectionController* selectionController = m_frame->selectionController();
    bool smartDelete = smartDeleteOK && canSmartCopyOrDelete();
    switch (deletionAction) {
        case deleteSelectionAction:
            selectionController->setSelectedRange(range, DOWNSTREAM, true, ec);
            if (ec)
                return;
            deleteSelectionWithSmartDelete(smartDelete);
            break;
        case deleteKeyAction:
            selectionController->setSelectedRange(range, DOWNSTREAM, (granularity != CharacterGranularity), ec);
            if (ec)
                return;
            if (m_frame->document()) {
                TypingCommand::deleteKeyPressed(m_frame->document(), smartDelete, granularity);
                m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
            }
            break;
        case forwardDeleteKeyAction:
            selectionController->setSelectedRange(range, DOWNSTREAM, (granularity != CharacterGranularity), ec);
            if (ec)
                return;
            if (m_frame->document()) {
                TypingCommand::forwardDeleteKeyPressed(m_frame->document(), smartDelete, granularity);
                m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
            }
            break;
    }
}

bool Editor::deleteWithDirection(SelectionController::EDirection direction, TextGranularity granularity, bool killRing, bool isTypingAction)
{
    // Delete the selection, if there is one.
    // If not, make a selection using the passed-in direction and granularity.

    if (!canEdit())
        return false;

    RefPtr<Range> range;
    EditorDeleteAction deletionAction = deleteSelectionAction;

    bool smartDeleteOK = false;
    
    if (m_frame->selectionController()->isRange()) {
        range = selectedRange();
        smartDeleteOK = true;
        if (isTypingAction)
            deletionAction = deleteKeyAction;
    } else {
        SelectionController selectionController;
        selectionController.setSelection(m_frame->selectionController()->selection());
        selectionController.modify(SelectionController::EXTEND, direction, granularity);
        range = selectionController.toRange();
        
        switch (direction) {
            case SelectionController::FORWARD:
            case SelectionController::RIGHT:
                deletionAction = forwardDeleteKeyAction;
                break;
            case SelectionController::BACKWARD:
            case SelectionController::LEFT:
                deletionAction = deleteKeyAction;
                break;
        }
    }

    deleteRange(range.get(), killRing, false, smartDeleteOK, deletionAction, granularity);

    return true;
}

void Editor::deleteSelectionWithSmartDelete()
{
    deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
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
    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = pasteboard->documentFragment(m_frame, selectedRange(), allowPlainText, chosePlainText);
    if (fragment && shouldInsertFragment(fragment, selectedRange(), EditorInsertActionPasted))
        replaceSelectionWithFragment(fragment, false, canSmartReplaceWithPasteboard(pasteboard), chosePlainText);
}

bool Editor::canSmartReplaceWithPasteboard(Pasteboard* pasteboard)
{
    if (client())
        return client()->smartInsertDeleteEnabled() && pasteboard->canSmartReplace();
    return false;
}

bool Editor::shouldInsertFragment(PassRefPtr<DocumentFragment> fragment, PassRefPtr<Range> replacingDOMRange, EditorInsertAction givenAction)
{
    if (client()) {
        Node* child = fragment->firstChild();
        if (fragment->lastChild() == child && child->isCharacterDataNode())
            return client()->shouldInsertText(static_cast<CharacterData*>(child)->data(), replacingDOMRange.get(), givenAction);
        return client()->shouldInsertNode(fragment.get(), replacingDOMRange.get(), givenAction);
    }
    return false;
}

void Editor::replaceSelectionWithFragment(PassRefPtr<DocumentFragment> fragment, bool selectReplacement, bool smartReplace, bool matchStyle)
{
    if (m_frame->selectionController()->isNone() || !fragment)
        return;
    
    applyCommand(new ReplaceSelectionCommand(m_frame->document(), fragment, selectReplacement, smartReplace, matchStyle));
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

void Editor::replaceSelectionWithText(String text, bool selectReplacement, bool smartReplace)
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

    if (client())
        return client()->shouldDeleteRange(range);
    return false;
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

bool Editor::shouldInsertText(String text, Range* range, EditorInsertAction action) const
{
    if (client())
        return client()->shouldInsertText(text, range, action);
    return false;
}

bool Editor::shouldShowDeleteInterface(HTMLElement* element) const
{
    if (client())
        return client()->shouldShowDeleteInterface(element);
    return false;
}

void Editor::respondToChangedSelection(const Selection& oldSelection)
{
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
    m_deleteButtonController->respondToChangedContents();
}

const FontData* Editor::fontForSelection(bool& hasMultipleFonts) const
{
    if (hasMultipleFonts)
        hasMultipleFonts = false;

    if (!m_frame->selectionController()->isRange()) {
        Node* nodeToRemove;
        RenderStyle* style = m_frame->styleForSelectionStart(nodeToRemove); // sets nodeToRemove

        const FontData* result = 0;
        if (style)
            result = style->font().primaryFont();
        
        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(ec == 0);
        }

        return result;
    }

    const FontData* font = 0;

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
            const FontData* f = renderer->style()->font().primaryFont();
            if (!font) {
                font = f;
                if (!hasMultipleFonts)
                    break;
            } else if (font != f) {
                hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

Frame::TriState Editor::selectionUnorderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        Node* selectionNode = m_frame->selectionController()->selection().start().node();
        if (enclosingNodeWithTag(selectionNode, ulTag))
            return Frame::trueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start().node(), ulTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end().node(), ulTag);
        if (startNode && endNode && startNode == endNode)
            return Frame::trueTriState;
    }

    return Frame::falseTriState;
}

Frame::TriState Editor::selectionOrderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        Node* selectionNode = m_frame->selectionController()->selection().start().node();
        if (enclosingNodeWithTag(selectionNode, olTag))
            return Frame::trueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start().node(), olTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end().node(), olTag);
        if (startNode && endNode && startNode == endNode)
            return Frame::trueTriState;
    }

    return Frame::falseTriState;
}

void Editor::removeFormattingAndStyle()
{
    Document* document = m_frame->document();
    
    // Make a plain text string from the selection to remove formatting like tables and lists.
    RefPtr<DocumentFragment> text = createFragmentFromText(m_frame->selectionController()->toRange().get(), m_frame->selectionController()->toString());
    
    // Put the fragment made from that string into a style span with the document's
    // default style to make sure that it is unstyled regardless of where it is inserted.
    Position pos(document->documentElement(), 0);
    RefPtr<CSSComputedStyleDeclaration> computedStyle = pos.computedStyle();
    RefPtr<CSSMutableStyleDeclaration> defaultStyle = computedStyle->copyInheritableProperties();
    
    RefPtr<Element> span = createStyleSpanElement(document);
    span->setAttribute(styleAttr, defaultStyle->cssText());
    
    ExceptionCode ec;
    
    while (text->firstChild())
        span->appendChild(text->firstChild(), ec);
    
    RefPtr<DocumentFragment> fragment = new DocumentFragment(document);
    fragment->appendChild(span, ec);
    
    applyCommand(new ReplaceSelectionCommand(document, fragment, false, false, false, true, EditActionUnspecified));
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
        case Selection::CARET: {
            m_frame->computeAndSetTypingStyle(style, editingAction);
            break;
        }
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

bool Editor::selectWordBeforeMenuEvent() const
{
    if (client())
        return client()->selectWordBeforeMenuEvent();
    return false;
}

bool Editor::clientIsEditable() const
{
    if (client())
        return client()->isEditable();
    return false;
}

bool Editor::selectionStartHasStyle(CSSStyleDeclaration* style) const
{
    Node* nodeToRemove;
    RefPtr<CSSStyleDeclaration> selectionStyle = m_frame->selectionComputedStyle(nodeToRemove);
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
        assert(ec == 0);
    }
    
    return match;
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
        startRoot->dispatchEvent(new Event(khtmlEditableContentChangedEvent, false, false), ec, true);
    if (endRoot && endRoot != startRoot)
        endRoot->dispatchEvent(new Event(khtmlEditableContentChangedEvent, false, false), ec, true);
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
    if (newSelection == m_frame->selectionController()->selection() || m_frame->shouldChangeSelection(newSelection))
        m_frame->selectionController()->setSelection(newSelection, true);
    
    m_lastEditCommand = 0;
    if (client())
        client()->registerCommandForUndo(cmd);
    respondToChangedContents(newSelection);    
}

// Execute command functions

static bool execCopy(Frame* frame)
{
    frame->editor()->copy();
    return true;
}

static bool execCut(Frame* frame)
{
    frame->editor()->cut();
    return true;
}

static bool execDelete(Frame* frame)
{
    frame->editor()->performDelete();
    return true;
}

static bool execBackwardDelete(Frame* frame)
{
    frame->editor()->deleteWithDirection(SelectionController::BACKWARD, CharacterGranularity, false, true);
    return true;
}

static bool execForwardDelete(Frame* frame)
{
    frame->editor()->deleteWithDirection(SelectionController::FORWARD, CharacterGranularity, false, true);
    return true;
}

static bool execMoveBackward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveBackwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveUpByPageAndModifyCaret(Frame* frame)
{
    RenderObject* renderer = frame->document()->focusedNode()->renderer();
    if (renderer->style()->overflowY() == OSCROLL
        || renderer->style()->overflowY() == OAUTO
        || renderer->isTextArea()) {
        int height = -(frame->document()->focusedNode()->renderer()->clientHeight()-PAGE_KEEP);
        bool handledScroll = renderer->scroll(ScrollUp, ScrollByPage);
        bool handledCaretMove = frame->selectionController()->modify(SelectionController::MOVE, height);
        return handledScroll || handledCaretMove;
    }
    return false;
}

static bool execMoveDown(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

static bool execMoveDownAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

static bool execMoveForward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveForwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, CharacterGranularity, true);
    return true;
}

static bool execMoveDownByPageAndModifyCaret(Frame* frame)
{
    RenderObject* renderer = frame->document()->focusedNode()->renderer();
    if (renderer->style()->overflowY() == OSCROLL
        || renderer->style()->overflowY() == OAUTO
        || renderer->isTextArea()) {
        int height = frame->document()->focusedNode()->renderer()->clientHeight()-PAGE_KEEP;
        bool handledScroll = renderer->scroll(ScrollDown, ScrollByPage);
        bool handledCaretMove = frame->selectionController()->modify(SelectionController::MOVE, height);
        return handledScroll || handledCaretMove;
    }
    return false;
}

static bool execMoveLeft(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

static bool execMoveLeftAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

static bool execMoveRight(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

static bool execMoveRightAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

static bool execMoveToBeginningOfDocument(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToBeginningOfDocumentAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToBeginningOfSentence(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToBeginningOfSentenceAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToBeginningOfLine(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineBoundary, true);
    return true;
}

static bool execMoveToBeginningOfLineAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineBoundary, true);
    return true;
}

static bool execMoveToBeginningOfParagraph(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveToBeginningOfParagraphAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveToEndOfDocument(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToEndOfDocumentAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, DocumentBoundary, true);
    return true;
}

static bool execMoveToEndOfSentence(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToEndOfSentenceAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, SentenceBoundary, true);
    return true;
}

static bool execMoveToEndOfLine(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineBoundary, true);
    return true;
}

static bool execMoveToEndOfLineAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineBoundary, true);
    return true;
}

static bool execMoveToEndOfParagraph(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveToEndOfParagraphAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, ParagraphBoundary, true);
    return true;
}

static bool execMoveParagraphBackwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, ParagraphGranularity, true);
    return true;
}

static bool execMoveParagraphForwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, ParagraphGranularity, true);
    return true;
}

static bool execMoveUp(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

static bool execMoveUpAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

static bool execMoveWordBackward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::BACKWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordBackwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordForward(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::FORWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordForwardAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::FORWARD, WordGranularity, true);
    return true;
}

static bool execMoveWordLeft(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::LEFT, WordGranularity, true);
    return true;
}

static bool execMoveWordLeftAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::LEFT, WordGranularity, true);
    return true;
}

static bool execMoveWordRight(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::MOVE, SelectionController::RIGHT, WordGranularity, true);
    return true;
}

static bool execMoveWordRightAndModifySelection(Frame* frame)
{
    frame->selectionController()->modify(SelectionController::EXTEND, SelectionController::RIGHT, WordGranularity, true);
    return true;
}

static bool execPaste(Frame* frame)
{
    frame->editor()->paste();
    return true;
}

static bool execSelectAll(Frame* frame)
{
    frame->selectionController()->selectAll();
    return true;
}

static bool execToggleBold(Frame* frame)
{
    ExceptionCode ec;
    
    RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
    style->setProperty(CSS_PROP_FONT_WEIGHT, "bold", false, ec);
    if (frame->editor()->selectionStartHasStyle(style.get()))
        style->setProperty(CSS_PROP_FONT_WEIGHT, "normal", false, ec);
    frame->editor()->applyStyleToSelection(style.get(), EditActionSetFont);
    return true;
}

static bool execToggleItalic(Frame* frame)
{
    ExceptionCode ec;
    
    RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
    style->setProperty(CSS_PROP_FONT_STYLE, "italic", false, ec);
    if (frame->editor()->selectionStartHasStyle(style.get()))
        style->setProperty(CSS_PROP_FONT_STYLE, "normal", false, ec);
    frame->editor()->applyStyleToSelection(style.get(), EditActionSetFont);
    return true;
}

static bool execRedo(Frame* frame)
{
    frame->editor()->redo();
    return true;
}

static bool execUndo(Frame* frame)
{
    frame->editor()->undo();
    return true;
}

// Enabled functions

static bool enabled(Frame*)
{
    return true;
}

static bool canPaste(Frame* frame)
{
    return frame->editor()->canPaste();
}

static bool hasEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentEditable();
}

static bool hasEditableRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange() && frame->selectionController()->isContentEditable();
}

static bool hasRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange();
}

static bool hasRichlyEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentRichlyEditable();
}

static bool canRedo(Frame* frame)
{
    return frame->editor()->canRedo();
}

static bool canUndo(Frame* frame)
{
    return frame->editor()->canUndo();
}

struct Command {
    bool (*enabled)(Frame* frame);
    bool (*exec)(Frame* frame);
};

typedef HashMap<RefPtr<AtomicStringImpl>, const Command*> CommandMap;

static CommandMap* createCommandMap()
{
    struct CommandEntry { const char* name; Command command; };
    
    static const CommandEntry commands[] = {
        { "BackwardDelete", { hasEditableSelection, execBackwardDelete } },
        { "Copy", { hasRangeSelection, execCopy } },
        { "Cut", { hasEditableRangeSelection, execCut } },
        { "Delete", { hasEditableSelection, execDelete } },
        { "ForwardDelete", { hasEditableSelection, execForwardDelete } },
        { "MoveBackward", { hasEditableSelection, execMoveBackward } },
        { "MoveBackwardAndModifySelection", { hasEditableSelection, execMoveBackwardAndModifySelection } },
        { "MoveUpByPageAndModifyCaret", { hasEditableSelection, execMoveUpByPageAndModifyCaret } },
        { "MoveDown", { hasEditableSelection, execMoveDown } },
        { "MoveDownAndModifySelection", { hasEditableSelection, execMoveDownAndModifySelection } },
        { "MoveForward", { hasEditableSelection, execMoveForward } },
        { "MoveForwardAndModifySelection", { hasEditableSelection, execMoveForwardAndModifySelection } },
        { "MoveDownByPageAndModifyCaret", { hasEditableSelection, execMoveDownByPageAndModifyCaret } },
        { "MoveLeft", { hasEditableSelection, execMoveLeft } },
        { "MoveLeftAndModifySelection", { hasEditableSelection, execMoveLeftAndModifySelection } },
        { "MoveRight", { hasEditableSelection, execMoveRight } },
        { "MoveRightAndModifySelection", { hasEditableSelection, execMoveRightAndModifySelection } },
        { "MoveToBeginningOfDocument", { hasEditableSelection, execMoveToBeginningOfDocument } },
        { "MoveToBeginningOfDocumentAndModifySelection", { hasEditableSelection, execMoveToBeginningOfDocumentAndModifySelection } },
        { "MoveToBeginningOfSentence", { hasEditableSelection, execMoveToBeginningOfSentence } },
        { "MoveToBeginningOfSentenceAndModifySelection", { hasEditableSelection, execMoveToBeginningOfSentenceAndModifySelection } },
        { "MoveToBeginningOfLine", { hasEditableSelection, execMoveToBeginningOfLine } },
        { "MoveToBeginningOfLineAndModifySelection", { hasEditableSelection, execMoveToBeginningOfLineAndModifySelection } },
        { "MoveToBeginningOfParagraph", { hasEditableSelection, execMoveToBeginningOfParagraph } },
        { "MoveToBeginningOfParagraphAndModifySelection", { hasEditableSelection, execMoveToBeginningOfParagraphAndModifySelection } },
        { "MoveToEndOfDocument", { hasEditableSelection, execMoveToEndOfDocument } },
        { "MoveToEndOfDocumentAndModifySelection", { hasEditableSelection, execMoveToEndOfDocumentAndModifySelection } },
        { "MoveToEndOfSentence", { hasEditableSelection, execMoveToEndOfSentence } },
        { "MoveToEndOfSentenceAndModifySelection", { hasEditableSelection, execMoveToEndOfSentenceAndModifySelection } },
        { "MoveToEndOfLine", { hasEditableSelection, execMoveToEndOfLine } },
        { "MoveToEndOfLineAndModifySelection", { hasEditableSelection, execMoveToEndOfLineAndModifySelection } },
        { "MoveToEndOfParagraph", { hasEditableSelection, execMoveToEndOfParagraph } },
        { "MoveToEndOfParagraphAndModifySelection", { hasEditableSelection, execMoveToEndOfParagraphAndModifySelection } },
        { "MoveParagraphBackwardAndModifySelection", { hasEditableSelection, execMoveParagraphBackwardAndModifySelection } },
        { "MoveParagraphForwardAndModifySelection", { hasEditableSelection, execMoveParagraphForwardAndModifySelection } },
        { "MoveUp", { hasEditableSelection, execMoveUp } },
        { "MoveUpAndModifySelection", { hasEditableSelection, execMoveUpAndModifySelection } },
        { "MoveWordBackward", { hasEditableSelection, execMoveWordBackward } },
        { "MoveWordBackwardAndModifySelection", { hasEditableSelection, execMoveWordBackwardAndModifySelection } },
        { "MoveWordForward", { hasEditableSelection, execMoveWordForward } },
        { "MoveWordForwardAndModifySelection", { hasEditableSelection, execMoveWordForwardAndModifySelection } },
        { "MoveWordLeft", { hasEditableSelection, execMoveWordLeft } },
        { "MoveWordLeftAndModifySelection", { hasEditableSelection, execMoveWordLeftAndModifySelection } },
        { "MoveWordRight", { hasEditableSelection, execMoveWordRight } },
        { "MoveWordRightAndModifySelection", { hasEditableSelection, execMoveWordRightAndModifySelection } },
        { "Paste", { canPaste, execPaste } },
        { "Redo", { canRedo, execRedo } },
        { "SelectAll", { enabled, execSelectAll } },
        { "ToggleBold", { hasRichlyEditableSelection, execToggleBold } },
        { "ToggleItalic", { hasRichlyEditableSelection, execToggleItalic } },
        { "Undo", { canUndo, execUndo } }
    };

    CommandMap* commandMap = new CommandMap;

    const unsigned numCommands = sizeof(commands) / sizeof(commands[0]);
    for (unsigned i = 0; i < numCommands; i++)
        commandMap->set(AtomicString(commands[i].name).impl(), &commands[i].command);

    return commandMap;
}

// =============================================================================
//
// public editing commands
//
// =============================================================================

Editor::Editor(Frame* frame)
    : m_frame(frame)
    , m_deleteButtonController(new DeleteButtonController(frame))
{ 
}

Editor::~Editor()
{
}

bool Editor::execCommand(const AtomicString& command)
{
    static CommandMap* commandMap;
    if (!commandMap)
        commandMap = createCommandMap();
    
    const Command* c = commandMap->get(command.impl());
    ASSERT(c);
    
    bool handled = false;
    
    if (c->enabled(m_frame)) {
        m_frame->document()->updateLayoutIgnorePendingStylesheets();
        handled = c->exec(m_frame);
    }
    
    return handled;
}

bool Editor::insertText(const String& text, bool selectInsertedText, Event* triggeringEvent)
{
    // Get the selection to use for the event that triggered this insertText.
    // If the event handler changed the selection, we may want to use a different selection
    // that is contained in the event target.
    Selection selection = selectionForEvent(m_frame, triggeringEvent);
    if (!selection.isCaretOrRange() || !selection.isContentEditable())
        return false;
    Node* selectionStart = selection.start().node();
    if (!selectionStart)
        return false;
    RefPtr<Document> document = selectionStart->document();
    
    // Insert the text
    TypingCommand::insertText(document.get(), text, selection, selectInsertedText);

    // Reveal the current selection 
    if (Frame* editedFrame = document->frame())
        if (Page* page = editedFrame->page())
            page->focusController()->focusedOrMainFrame()->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
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
        deleteSelectionWithSmartDelete();
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
    Pasteboard::generalPasteboard()->writeSelection(selectedRange().get(), canSmartCopyOrDelete(), m_frame);
    didWriteSelectionToPasteboard();
}

void Editor::paste()
{
    if (tryDHTMLPaste())
        return;     // DHTML did the whole operation
    if (!canPaste())
        return;
    if (canEditRichly()) // or should we call m_frame->selectionController()->isContentRichlyEditable()
        pasteWithPasteboard(Pasteboard::generalPasteboard(), true);
    else
        pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::performDelete()
{
    if (!canDelete()) {
        systemBeep();
        return;
    }
    deleteRange(selectedRange().get(), true, false, true, deleteSelectionAction, CharacterGranularity);
}

void Editor::copyURL(const KURL& url, const String& title)
{
    Pasteboard::generalPasteboard()->writeURL(url, title, m_frame);
}

bool Editor::isContinuousSpellCheckingEnabled()
{
    if (client())
        return client()->isContinuousSpellCheckingEnabled();
    return false;
}

void Editor::toggleContinuousSpellChecking()
{
    if (client())
        client()->toggleContinuousSpellChecking();
}

bool Editor::isGrammarCheckingEnabled()
{
    if (client())
        return client()->isGrammarCheckingEnabled();
    return false;
}

void Editor::toggleGrammarChecking()
{
    if (client())
        client()->toggleGrammarChecking();
}

int Editor::spellCheckerDocumentTag()
{
    if (client())
        return client()->spellCheckerDocumentTag();
    return 0;
}

bool Editor::shouldEndEditing(Range* range)
{
    if (client())
        return client()->shouldEndEditing(range);
    return false;
}

bool Editor::shouldBeginEditing(Range* range)
{
    if (client())
        return client()->shouldBeginEditing(range);
    return false;
}

void Editor::clearUndoRedoOperations()
{
    if (client())
        client()->clearUndoRedoOperations();
}

bool Editor::canUndo()
{
    if (client())
        return client()->canUndo();
    return false;
}

void Editor::undo()
{
    if (client())
        client()->undo();
}

bool Editor::canRedo()
{
    if (client())
        return client()->canRedo();
    return false;
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
    execToggleBold(frame());
}

void Editor::toggleUnderline()
{
    ExceptionCode ec = 0;

    RefPtr<CSSStyleDeclaration> style = frame()->document()->createCSSStyleDeclaration();
    style->setProperty(CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT, "underline", false, ec);
    if (selectionStartHasStyle(style.get()))
        style->setProperty(CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT, "none", false, ec);
    applyStyleToSelection(style.get(), EditActionUnderline);
}

void Editor::setBaseWritingDirection(String direction)
{
    ExceptionCode ec = 0;

    RefPtr<CSSStyleDeclaration> style = frame()->document()->createCSSStyleDeclaration();
    style->setProperty(CSS_PROP_DIRECTION, direction, false, ec);
    applyParagraphStyleToSelection(style.get(), EditActionSetWritingDirection);
}

} // namespace WebCore
