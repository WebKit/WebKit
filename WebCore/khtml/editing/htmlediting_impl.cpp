/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "htmlediting_impl.h"

#include "dom/dom_position.h"
#include "html/html_elementimpl.h"
#include "html/html_imageimpl.h"
#include "htmlattrs.h"
#include "htmltags.h"
#include "khtml_part.h"
#include "khtml_selection.h"
#include "khtmlview.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_edititerator.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_stringimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_rangeimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#endif

using DOM::DocumentFragmentImpl;
using DOM::DocumentImpl;
using DOM::DOMPosition;
using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::EditIterator;
using DOM::ElementImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLImageElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;

using khtml::AppendNodeCommand;
using khtml::AppendNodeCommandImpl;
using khtml::CompositeEditCommand;
using khtml::CompositeEditCommandImpl;
using khtml::DeleteCollapsibleWhitespaceCommand;
using khtml::DeleteCollapsibleWhitespaceCommandImpl;
using khtml::DeleteSelectionCommand;
using khtml::DeleteSelectionCommandImpl;
using khtml::DeleteTextCommand;
using khtml::DeleteTextCommandImpl;
using khtml::EditCommand;
using khtml::EditCommandImpl;
using khtml::InlineTextBox;
using khtml::InputNewlineCommand;
using khtml::InputNewlineCommandImpl;
using khtml::InputTextCommand;
using khtml::InputTextCommandImpl;
using khtml::InsertNodeBeforeCommand;
using khtml::InsertNodeBeforeCommandImpl;
using khtml::InsertTextCommand;
using khtml::InsertTextCommandImpl;
using khtml::JoinTextNodesCommand;
using khtml::JoinTextNodesCommandImpl;
using khtml::RemoveNodeCommand;
using khtml::RemoveNodeCommandImpl;
using khtml::RemoveNodeAndPruneCommand;
using khtml::RemoveNodeAndPruneCommandImpl;
using khtml::RenderObject;
using khtml::RenderStyle;
using khtml::RenderText;
using khtml::PasteHTMLCommand;
using khtml::PasteHTMLCommandImpl;
using khtml::PasteImageCommand;
using khtml::PasteImageCommandImpl;
using khtml::SplitTextNodeCommand;
using khtml::SplitTextNodeCommandImpl;
using khtml::TypingCommand;
using khtml::TypingCommandImpl;

#if !APPLE_CHANGES
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#if LOG_DISABLED
#define debugPosition(a,b) ((void)0)
#endif
#endif

static inline bool isNBSP(const QChar &c)
{
    return c == QChar(0xa0);
}

static inline bool isWS(const QChar &c)
{
    return c.isSpace() && c != QChar(0xa0);
}

static inline bool isWS(const DOMString &text)
{
    if (text.length() != 1)
        return false;
    
    return isWS(text[0]);
}

static inline bool isWS(const DOMPosition &pos)
{
    if (!pos.node())
        return false;
        
    if (!pos.node()->isTextNode())
        return false;

    const DOMString &string = static_cast<TextImpl *>(pos.node())->data();
    return isWS(string[pos.offset()]);
}

static bool shouldPruneNode(NodeImpl *node)
{
    if (!node)
        return false;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return true;

    if (node->hasChildNodes())
        return false;
        
    if (renderer->isBR() || renderer->isBlockFlow() || renderer->isReplaced())
        return false;
        
    if (node->isTextNode()) {
        TextImpl *text = static_cast<TextImpl *>(node);
        if (text->length() == 0)
            return true;
        return false;
    }
    
    if (!node->isHTMLElement() && !node->isXMLElementNode())
        return false;
    
    if (node->id() == ID_BODY)
        return false;
            
    if (!node->isContentEditable())
        return false;
            
    return true;
}

static DOMPosition leadingWhitespacePosition(const DOMPosition &pos)
{
    ASSERT(pos.notEmpty());

    KHTMLSelection selection(pos);
    DOMPosition prev = pos.previousCharacterPosition();
    if (prev != pos && prev.node()->inSameContainingEditableBlock(pos.node()) && prev.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(prev.node())->data();
        if (isWS(string[prev.offset()]))
            return prev;
    }

    return DOMPosition();
}

static DOMPosition trailingWhitespacePosition(const DOMPosition &pos)
{
    ASSERT(pos.notEmpty());

    if (pos.node()->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(pos.node());
        if (pos.offset() >= (long)textNode->length()) {
            DOMPosition next = pos.nextCharacterPosition();
            if (next != pos && next.node()->inSameContainingEditableBlock(pos.node()) && next.node()->isTextNode()) {
                DOMString string = static_cast<TextImpl *>(next.node())->data();
                if (isWS(string[0]))
                    return next;
            }
        }
        else {
            DOMString string = static_cast<TextImpl *>(pos.node())->data();
            if (isWS(string[pos.offset()]))
                return pos;
        }
    }

    return DOMPosition();
}

static bool textNodesAreJoinable(TextImpl *text1, TextImpl *text2)
{
    ASSERT(text1);
    ASSERT(text2);
    
    return (text1->nextSibling() == text2);
}

static DOMString &nonBreakingSpaceString()
{
    static DOMString nonBreakingSpaceString = QString(QChar(0xa0));
    return nonBreakingSpaceString;
}

//------------------------------------------------------------------------------------------
// EditCommandImpl

EditCommandImpl::EditCommandImpl(DocumentImpl *document) 
    : SharedCommandImpl(), m_document(document), m_state(NotApplied), m_parent(0)
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    m_document->ref();
    m_startingSelection = m_document->part()->selection();
    m_endingSelection = m_startingSelection;
}

EditCommandImpl::~EditCommandImpl()
{
    ASSERT(m_document);
    m_document->deref();
}

int EditCommandImpl::commandID() const
{
    return EditCommandID;
}

void EditCommandImpl::apply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == NotApplied);
    
    doApply();
    
    m_state = Applied;

    if (!isCompositeStep()) {
        EditCommand cmd(this);
        m_document->part()->appliedEditing(cmd);
    }
}

void EditCommandImpl::unapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == Applied);
    
    doUnapply();
    
    m_state = NotApplied;

    if (!isCompositeStep()) {
        EditCommand cmd(this);
        m_document->part()->unappliedEditing(cmd);
    }
}

void EditCommandImpl::reapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == NotApplied);
    
    doReapply();
    
    m_state = Applied;

    if (!isCompositeStep()) {
        EditCommand cmd(this);
        m_document->part()->reappliedEditing(cmd);
    }
}

void EditCommandImpl::doReapply()
{
    doApply();
}

KHTMLSelection EditCommandImpl::currentSelection() const
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    return m_document->part()->selection();
}

void EditCommandImpl::setStartingSelection(const KHTMLSelection &s)
{
    m_startingSelection = s;
    EditCommand cmd = parent();
    while (cmd.notNull()) {
        cmd.setStartingSelection(s);
        cmd = cmd.parent();
    }
    moveToStartingSelection();
}

void EditCommandImpl::setEndingSelection(const KHTMLSelection &s)
{
    m_endingSelection = s;
    EditCommand cmd = parent();
    while (cmd.notNull()) {
        cmd.setEndingSelection(s);
        cmd = cmd.parent();
    }
    moveToEndingSelection();
}

void EditCommandImpl::moveToStartingSelection()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    m_document->part()->takeSelectionFrom(this, false);
}

void EditCommandImpl::moveToEndingSelection()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    m_document->part()->takeSelectionFrom(this, true);
}

EditCommand EditCommandImpl::parent() const
{
    return m_parent;
}

void EditCommandImpl::setParent(const EditCommand &cmd)
{
    m_parent = cmd;
}

//------------------------------------------------------------------------------------------
// CompositeEditCommandImpl

CompositeEditCommandImpl::CompositeEditCommandImpl(DocumentImpl *document) 
    : EditCommandImpl(document)
{
}

CompositeEditCommandImpl::~CompositeEditCommandImpl()
{
}

int CompositeEditCommandImpl::commandID() const
{
    return CompositeEditCommandID;
}

void CompositeEditCommandImpl::doUnapply()
{
    if (m_cmds.count() == 0) {
        return;
    }
    
    for (int i = m_cmds.count() - 1; i >= 0; --i)
        m_cmds[i]->unapply();

    moveToStartingSelection();
    setState(NotApplied);
}

void CompositeEditCommandImpl::doReapply()
{
    if (m_cmds.count() == 0) {
        return;
    }

    for (QValueList<EditCommand>::ConstIterator it = m_cmds.begin(); it != m_cmds.end(); ++it)
        (*it)->reapply();

    moveToEndingSelection();
    setState(Applied);
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommandImpl::applyCommandToComposite(EditCommand &cmd)
{
    cmd.setParent(this);
    cmd.apply();
    m_cmds.append(cmd);
}

void CompositeEditCommandImpl::insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild)
{
    InsertNodeBeforeCommand cmd(document(), insertChild, refChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild)
{
    if (refChild->parentNode()->lastChild() == refChild) {
        appendNode(refChild->parentNode(), insertChild);
    }
    else {
        ASSERT(refChild->nextSibling());
        insertNodeBefore(insertChild, refChild->nextSibling());
    }
}

void CompositeEditCommandImpl::insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset)
{
    if (refChild->caretMinOffset() >= offset) {
        insertNodeBefore(insertChild, refChild);
    } 
    else if (refChild->isTextNode() && refChild->caretMaxOffset() > offset) {
        splitTextNode(static_cast<TextImpl *>(refChild), offset);
        insertNodeBefore(insertChild, refChild);
    } 
    else {
        insertNodeAfter(insertChild, refChild);
    }
}

void CompositeEditCommandImpl::appendNode(DOM::NodeImpl *parent, DOM::NodeImpl *appendChild)
{
    AppendNodeCommand cmd(document(), parent, appendChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNode(DOM::NodeImpl *removeChild)
{
    RemoveNodeCommand cmd(document(), removeChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNodeAndPrune(DOM::NodeImpl *removeChild)
{
    RemoveNodeAndPruneCommand cmd(document(), removeChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::splitTextNode(DOM::TextImpl *text, long offset)
{
    SplitTextNodeCommand cmd(document(), text, offset);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2)
{
    JoinTextNodesCommand cmd(document(), text1, text2);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text)
{
    InsertTextCommand cmd(document(), node, offset, text);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::deleteText(DOM::TextImpl *node, long offset, long count)
{
    DeleteTextCommand cmd(document(), node, offset, count);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::replaceText(DOM::TextImpl *node, long offset, long count, const DOM::DOMString &replacementText)
{
    DeleteTextCommand deleteCommand(document(), node, offset, count);
    applyCommandToComposite(deleteCommand);
    InsertTextCommand insertCommand(document(), node, offset, replacementText);
    applyCommandToComposite(insertCommand);
}

void CompositeEditCommandImpl::deleteSelection()
{
    if (currentSelection().state() == KHTMLSelection::RANGE) {
        DeleteSelectionCommand cmd(document());
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommandImpl::deleteSelection(const KHTMLSelection &selection)
{
    if (selection.state() == KHTMLSelection::RANGE) {
        DeleteSelectionCommand cmd(document(), selection);
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommandImpl::deleteCollapsibleWhitespace()
{
    DeleteCollapsibleWhitespaceCommand cmd(document());
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::deleteCollapsibleWhitespace(const KHTMLSelection &selection)
{
    DeleteCollapsibleWhitespaceCommand cmd(document(), selection);
    applyCommandToComposite(cmd);
}

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommandImpl

AppendNodeCommandImpl::AppendNodeCommandImpl(DocumentImpl *document, NodeImpl *parentNode, NodeImpl *appendChild)
    : EditCommandImpl(document), m_parentNode(parentNode), m_appendChild(appendChild)
{
    ASSERT(m_parentNode);
    m_parentNode->ref();

    ASSERT(m_appendChild);
    m_appendChild->ref();
}

AppendNodeCommandImpl::~AppendNodeCommandImpl()
{
    if (m_parentNode)
        m_parentNode->deref();
    if (m_appendChild)
        m_appendChild->deref();
}

int AppendNodeCommandImpl::commandID() const
{
    return AppendNodeCommandID;
}

void AppendNodeCommandImpl::doApply()
{
    ASSERT(m_parentNode);
    ASSERT(m_appendChild);

    int exceptionCode = 0;
    m_parentNode->appendChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void AppendNodeCommandImpl::doUnapply()
{
    ASSERT(m_parentNode);
    ASSERT(m_appendChild);
    ASSERT(state() == Applied);

    int exceptionCode = 0;
    m_parentNode->removeChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// DeleteCollapsibleWhitespaceCommandImpl

DeleteCollapsibleWhitespaceCommandImpl::DeleteCollapsibleWhitespaceCommandImpl(DocumentImpl *document)
    : CompositeEditCommandImpl(document), m_selectionToCollapse(currentSelection()), m_charactersDeleted(0)
{
}

DeleteCollapsibleWhitespaceCommandImpl::DeleteCollapsibleWhitespaceCommandImpl(DocumentImpl *document, const KHTMLSelection &selection)
    : CompositeEditCommandImpl(document), m_selectionToCollapse(selection), m_charactersDeleted(0)
{
}

DeleteCollapsibleWhitespaceCommandImpl::~DeleteCollapsibleWhitespaceCommandImpl()
{
}

int DeleteCollapsibleWhitespaceCommandImpl::commandID() const
{
    return DeleteCollapsibleWhitespaceCommandID;
}

static bool shouldDeleteUpstreamPosition(const DOMPosition &pos)
{
    if (!pos.node()->isTextNode())
        return false;
        
    RenderObject *renderer = pos.node()->renderer();
    if (!renderer)
        return true;
        
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    if (pos.offset() >= (long)textNode->length())
        return false;

    if (pos.isLastRenderedPositionInEditableBlock())
        return false;

    if (pos.isFirstRenderedPositionOnLine())
        return false;

    RenderText *textRenderer = static_cast<RenderText *>(renderer);

    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (pos.offset() < box->m_start) {
            return true;
        }
        if (pos.offset() >= box->m_start && pos.offset() < box->m_start + box->m_len)
            return false;
    }
    
    return true;
}

DOMPosition DeleteCollapsibleWhitespaceCommandImpl::deleteWhitespace(const DOMPosition &pos)
{
    DOMPosition upstream = pos.equivalentUpstreamPosition();
    DOMPosition downstream = pos.equivalentDownstreamPosition();
    
    bool del = shouldDeleteUpstreamPosition(upstream);

    LOG(Editing, "pos:        %s [%p:%d]\n", getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
    if (upstream == downstream) {
        LOG(Editing, "same:       %s [%p:%d]\n", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
    }
    else {
        LOG(Editing, "upstream:   %s %s [%p:%d]\n", del ? "DELETE" : "SKIP", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
        EditIterator it(upstream);
        for (it.next(); it.current() != downstream; it.next()) {
            if (it.current().node()->isTextNode() && (long)static_cast<TextImpl *>(it.current().node())->length() == it.current().offset())
                LOG(Editing, "   node:    AT END %s [%p:%d]\n", getTagName(it.current().node()->id()).string().latin1(), it.current().node(), it.current().offset());
            else
                LOG(Editing, "   node:    DELETE %s [%p:%d]\n", getTagName(it.current().node()->id()).string().latin1(), it.current().node(), it.current().offset());
        }
        LOG(Editing, "downstream: %s [%p:%d]\n", getTagName(downstream.node()->id()).string().latin1(), downstream.node(), downstream.offset());
    }

    if (upstream == downstream)
        return upstream;
        
    EditIterator it(upstream);
    DOMPosition deleteStart = upstream;
    if (!del) {
        deleteStart = it.peekNext();
        if (deleteStart == downstream)
            return upstream;
    }
    
    DOMPosition endingPosition = upstream;
    
    while (it.current() != downstream) {

        DOMPosition next = it.peekNext();
        if (next.node() != deleteStart.node()) {
            ASSERT(deleteStart.node()->isTextNode());
            TextImpl *textNode = static_cast<TextImpl *>(deleteStart.node());
            unsigned long count = it.current().offset() - deleteStart.offset();
            if (count == textNode->length()) {
                LOG(Editing, "   removeNodeAndPrune 1: [%p]\n", textNode);
                removeNodeAndPrune(textNode);
            }
            else {
                LOG(Editing, "   deleteText 1: [%p:%d:%d:%d]\n", textNode, textNode->length(), deleteStart.offset(), it.current().offset() - deleteStart.offset());
                deleteText(textNode, deleteStart.offset(), count);
            }
            deleteStart = next;
        }
        else if (next == downstream) {
            ASSERT(deleteStart.node() == downstream.node());
            ASSERT(downstream.node()->isTextNode());
            TextImpl *textNode = static_cast<TextImpl *>(deleteStart.node());
            unsigned long count = downstream.offset() - deleteStart.offset();
            ASSERT(count <= textNode->length());
            if (count == textNode->length()) {
                LOG(Editing, "   removeNodeAndPrune 2: [%p]\n", textNode);
                removeNodeAndPrune(textNode);
            }
            else {
                LOG(Editing, "   deleteText 2: [%p:%d:%d:%d]\n", textNode, textNode->length(), deleteStart.offset(), count);
                deleteText(textNode, deleteStart.offset(), count);
                m_charactersDeleted = count;
                endingPosition = DOMPosition(downstream.node(), downstream.offset() - m_charactersDeleted);
            }
        }
        
        it.setPosition(next);
    }
    
    return endingPosition;
}

void DeleteCollapsibleWhitespaceCommandImpl::doApply()
{
    int state = m_selectionToCollapse.state();
    if (state == KHTMLSelection::CARET) {
        DOMPosition endPosition = deleteWhitespace(m_selectionToCollapse.startPosition());
        setEndingSelection(endPosition);
        LOG(Editing, "-----------------------------------------------------\n");
    }
    else if (state == KHTMLSelection::RANGE) {
        DOMPosition startPosition = deleteWhitespace(m_selectionToCollapse.startPosition());
        LOG(Editing, "-----------------------------------------------------\n");
        DOMPosition endPosition = m_selectionToCollapse.endPosition();
        if (m_charactersDeleted > 0 && startPosition.node() == endPosition.node()) {
            LOG(Editing, "adjust end position by %d\n", m_charactersDeleted);
            endPosition = DOMPosition(endPosition.node(), endPosition.offset() - m_charactersDeleted);
        }
        endPosition = deleteWhitespace(endPosition);
        setEndingSelection(KHTMLSelection(startPosition, endPosition));
        LOG(Editing, "=====================================================\n");
    }
}

//------------------------------------------------------------------------------------------
// DeleteSelectionCommandImpl

DeleteSelectionCommandImpl::DeleteSelectionCommandImpl(DOM::DocumentImpl *document)
    : CompositeEditCommandImpl(document)
{
    m_selectionToDelete = startingSelection();
}

DeleteSelectionCommandImpl::DeleteSelectionCommandImpl(DOM::DocumentImpl *document, const KHTMLSelection &selection)
    : CompositeEditCommandImpl(document)
{
    m_selectionToDelete = selection;
}

DeleteSelectionCommandImpl::~DeleteSelectionCommandImpl()
{
}
	
int DeleteSelectionCommandImpl::commandID() const
{
    return DeleteSelectionCommandID;
}

void DeleteSelectionCommandImpl::joinTextNodesWithSameStyle()
{
    KHTMLSelection selection = currentSelection();

    if (selection.state() != KHTMLSelection::CARET)
        return;

    DOMPosition pos = selection.startPosition();
    
    if (!pos.node()->isTextNode())
        return;

    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    
    if (pos.offset() == 0) {
        EditIterator it(pos);
        DOMPosition prev = it.previous();
        if (prev == pos)
            return;
        if (prev.node()->isTextNode()) {
            TextImpl *prevTextNode = static_cast<TextImpl *>(prev.node());
            if (textNodesAreJoinable(prevTextNode, textNode)) {
                joinTextNodes(prevTextNode, textNode);
                setEndingSelection(DOMPosition(textNode, prevTextNode->length()));
                LOG(Editing, "joinTextNodesWithSameStyle [1]\n");
            }
        }
    }
    else if (pos.offset() == (long)textNode->length()) {
        EditIterator it(pos);
        DOMPosition next = it.next();
        if (next == pos)
            return;
        if (next.node()->isTextNode()) {
            TextImpl *nextTextNode = static_cast<TextImpl *>(next.node());
            if (textNodesAreJoinable(textNode, nextTextNode)) {
                joinTextNodes(textNode, nextTextNode);
                setEndingSelection(DOMPosition(nextTextNode, pos.offset()));
                LOG(Editing, "joinTextNodesWithSameStyle [2]\n");
            }
        }
    }
}

static void debugPosition(const char *prefix, const DOMPosition &pos)
{
    LOG(Editing, "%s%s %p : %d", prefix, getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
}

enum { NoPositionModification, MoveDownstreamPositionModification, MoveToNextCharacterModification };

void DeleteSelectionCommandImpl::doApply()
{
    if (m_selectionToDelete.state() != KHTMLSelection::RANGE)
        return;

    KHTMLSelection selection = m_selectionToDelete;

    DOMPosition endingPosition;
    bool adjustEndingPositionDownstream = false;

    DOMPosition upstreamStart = selection.startPosition().equivalentUpstreamPosition();
    DOMPosition downstreamStart = selection.startPosition().equivalentDownstreamPosition();
    DOMPosition upstreamEnd = selection.endPosition().equivalentUpstreamPosition();
    DOMPosition downstreamEnd = selection.endPosition().equivalentDownstreamPosition();

    bool startCompletelySelected = 
        downstreamStart.offset() <= downstreamStart.node()->caretMinOffset() &&
        ((downstreamStart.node() != upstreamEnd.node()) ||
         (upstreamEnd.offset() >= upstreamEnd.node()->caretMaxOffset()));

    bool endCompletelySelected = 
        upstreamEnd.offset() >= upstreamEnd.node()->caretMaxOffset() &&
        ((downstreamStart.node() != upstreamEnd.node()) ||
         (downstreamStart.offset() <= downstreamStart.node()->caretMinOffset()));

    unsigned long startRenderedOffset = downstreamStart.renderedOffset();
    //unsigned long endRenderedOffset = upstreamEnd.renderedOffset();
    
    bool startAtStartOfRootEditableBlock = startRenderedOffset == 0 && downstreamStart.inFirstEditableInRootEditableBlock();
    bool startAtStartOfBlock = startAtStartOfRootEditableBlock || 
        (startRenderedOffset == 0 && downstreamStart.inFirstEditableInContainingEditableBlock());
    bool endAtEndOfBlock = downstreamEnd.isLastRenderedPositionInEditableBlock();

    debugPosition("upstreamStart:       ", upstreamStart);
    debugPosition("downstreamStart:     ", downstreamStart);
    debugPosition("upstreamEnd:         ", upstreamEnd);
    debugPosition("downstreamEnd:       ", downstreamEnd);
    LOG(Editing,  "start selected:      %s", startCompletelySelected ? "YES" : "NO");
    LOG(Editing,  "at start block:      %s", startAtStartOfBlock ? "YES" : "NO");
    LOG(Editing,  "at start root block: %s", startAtStartOfRootEditableBlock ? "YES" : "NO");
    LOG(Editing,  "at end block:        %s", endAtEndOfBlock ? "YES" : "NO");

    // Start is not completely selected
    if (startAtStartOfBlock) {
        LOG(Editing,  "ending position case 1");
        endingPosition = DOMPosition(downstreamStart.node()->containingEditableBlock(), 1);
        adjustEndingPositionDownstream = true;
    }
    else if (!startCompletelySelected) {
        LOG(Editing,  "ending position case 2");
        endingPosition = upstreamStart;
        if (upstreamStart.node()->id() == ID_BR && upstreamStart.offset() == 1)
            adjustEndingPositionDownstream = true;
    }
    else if (upstreamStart != downstreamStart) {
        LOG(Editing,  "ending position case 3");
        endingPosition = upstreamStart;
        if (downstreamStart.node()->id() == ID_BR && downstreamStart.offset() == 0)
            adjustEndingPositionDownstream = true;
    }
   
    //
    // Figure out the whitespace conversions to do
    //
    if (startAtStartOfBlock && !endAtEndOfBlock) {
        // convert trailing whitespace
        DOMPosition trailing = trailingWhitespacePosition(downstreamEnd.equivalentDownstreamPosition());
        if (trailing.notEmpty()) {
            debugPosition("convertTrailingWhitespace: ", trailing);
            DOMPosition collapse = trailing.nextCharacterPosition();
            if (collapse != trailing)
                deleteCollapsibleWhitespace(collapse);
            TextImpl *textNode = static_cast<TextImpl *>(trailing.node());
            replaceText(textNode, trailing.offset(), 1, nonBreakingSpaceString());
        }
    }
    else if (!startAtStartOfBlock && endAtEndOfBlock) {
        // convert leading whitespace
        DOMPosition leading = leadingWhitespacePosition(upstreamStart.equivalentUpstreamPosition());
        if (leading.notEmpty()) {
            debugPosition("convertLeadingWhitespace:  ", leading);
            TextImpl *textNode = static_cast<TextImpl *>(leading.node());
            replaceText(textNode, leading.offset(), 1, nonBreakingSpaceString());
        }
    }
    else if (!startAtStartOfBlock && !endAtEndOfBlock) {
        // convert contiguous whitespace
        DOMPosition leading = leadingWhitespacePosition(upstreamStart.equivalentUpstreamPosition());
        DOMPosition trailing = trailingWhitespacePosition(downstreamEnd.equivalentDownstreamPosition());
        if (leading.notEmpty() && trailing.notEmpty()) {
            debugPosition("convertLeadingWhitespace [contiguous]:  ", leading);
            TextImpl *textNode = static_cast<TextImpl *>(leading.node());
            replaceText(textNode, leading.offset(), 1, nonBreakingSpaceString());
        }
    }
        
    //
    // Do the delete
    //
    NodeImpl *n = downstreamStart.node()->traverseNextNode();

    // work on start node
    if (startCompletelySelected) {
        removeNodeAndPrune(downstreamStart.node());
    }
    else if (downstreamStart.node()->isTextNode()) {
        TextImpl *text = static_cast<TextImpl *>(downstreamStart.node());
        int endOffset = text == upstreamEnd.node() ? upstreamEnd.offset() : text->length();
        if (endOffset > downstreamStart.offset()) {
            deleteText(text, downstreamStart.offset(), endOffset - downstreamStart.offset());
        }
    }
    else {
        // we have clipped the end of a non-text element
        // the offset must be 1 here. if it is, do nothing and move on.
        ASSERT(downstreamStart.offset() == 1);
    }

    if (downstreamStart.node() != upstreamEnd.node()) {
        // work on intermediate nodes
        while (n != upstreamEnd.node()) {
            NodeImpl *d = n;
            n = n->traverseNextNode();
            if (d->renderer() && d->renderer()->isEditable())
                removeNodeAndPrune(d);
        }
        
        // work on end node
        ASSERT(n == upstreamEnd.node());
        if (endCompletelySelected) {
            removeNodeAndPrune(upstreamEnd.node());
        }
        else if (upstreamEnd.node()->isTextNode()) {
            if (upstreamEnd.offset() > 0) {
                TextImpl *text = static_cast<TextImpl *>(upstreamEnd.node());
                deleteText(text, 0, upstreamEnd.offset());
            }
        }
        else {
            // we have clipped the beginning of a non-text element
            // the offset must be 0 here. if it is, do nothing and move on.
            ASSERT(downstreamStart.offset() == 0);
        }
    }

    if (adjustEndingPositionDownstream) {
        LOG(Editing,  "adjust ending position downstream");
        endingPosition = endingPosition.equivalentDownstreamPosition();
    }

    debugPosition("ending position:     ", endingPosition);
    setEndingSelection(endingPosition);

    LOG(Editing, "-----------------------------------------------------\n");
}

//------------------------------------------------------------------------------------------
// DeleteTextCommandImpl

DeleteTextCommandImpl::DeleteTextCommandImpl(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditCommandImpl(document), m_node(node), m_offset(offset), m_count(count)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(m_count >= 0);
    
    m_node->ref();
}

DeleteTextCommandImpl::~DeleteTextCommandImpl()
{
    if (m_node)
        m_node->deref();
}

int DeleteTextCommandImpl::commandID() const
{
    return DeleteTextCommandID;
}

void DeleteTextCommandImpl::doApply()
{
    ASSERT(m_node);

    int exceptionCode = 0;
    m_text = m_node->substringData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    m_node->deleteData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void DeleteTextCommandImpl::doUnapply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// InputNewlineCommandImpl

InputNewlineCommandImpl::InputNewlineCommandImpl(DocumentImpl *document) 
    : CompositeEditCommandImpl(document)
{
}

InputNewlineCommandImpl::~InputNewlineCommandImpl() 
{
}

int InputNewlineCommandImpl::commandID() const
{
    return InputNewlineCommandID;
}

void InputNewlineCommandImpl::doApply()
{
    KHTMLSelection selection = currentSelection();

    if (!selection.startNode()->isTextNode())
        return;

    // Delete the current selection
    deleteSelection();
    
    // reset the current selection since it may have changed due to the delete
    selection = currentSelection();

    int exceptionCode = 0;
    ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
    ASSERT(exceptionCode == 0);

    TextImpl *textNode = static_cast<TextImpl *>(selection.startNode());
    bool atStart = selection.startOffset() == textNode->renderer()->caretMinOffset();
    bool atEnd = selection.startOffset() == textNode->renderer()->caretMaxOffset();
    if (atStart) {
        // Set the cursor at the beginning of text node now following the new BR.
        insertNodeBefore(breakNode, textNode);
        selection = KHTMLSelection(textNode, 0);
        setEndingSelection(selection);
    }
    else if (atEnd) {
        insertNodeAfter(breakNode, textNode);
        // Set the cursor at the beginning of the the BR.
        DOMPosition pos(breakNode, 0);
        setEndingSelection(pos);
    }
    else {
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.startOffset(), exceptionCode));
        deleteText(textNode, 0, selection.startOffset());
        insertNodeBefore(textBeforeNode, textNode);
        insertNodeBefore(breakNode, textNode);
        textBeforeNode->deref();
        // Set the cursor at the beginning of the node after the BR.
        selection = KHTMLSelection(textNode, 0);
        setEndingSelection(selection);
    }
    
    breakNode->deref();
}

//------------------------------------------------------------------------------------------
// InputTextCommandImpl

InputTextCommandImpl::InputTextCommandImpl(DocumentImpl *document) 
    : CompositeEditCommandImpl(document), m_insertedTextNode(0), m_charactersAdded(0)
{
}

InputTextCommandImpl::~InputTextCommandImpl() 
{
    if (m_insertedTextNode)
        m_insertedTextNode->deref();
}

int InputTextCommandImpl::commandID() const
{
    return InputTextCommandID;
}

void InputTextCommandImpl::doApply()
{
}

void InputTextCommandImpl::input(const DOMString &text)
{
    execute(text);
}

void InputTextCommandImpl::deleteCharacter()
{
    ASSERT(state() == Applied);

    KHTMLSelection selection = currentSelection();

    if (!selection.startNode()->isTextNode())
        return;

    int exceptionCode = 0;
    int offset = selection.startOffset() - 1;
    if (offset >= selection.startNode()->caretMinOffset()) {
        TextImpl *textNode = static_cast<TextImpl *>(selection.startNode());
        textNode->deleteData(offset, 1, exceptionCode);
        ASSERT(exceptionCode == 0);
        selection = KHTMLSelection(textNode, offset);
        setEndingSelection(selection);
        m_charactersAdded--;
    }
}

DOMPosition InputTextCommandImpl::prepareForTextInsertion()
{
    // Prepare for text input by looking at the current position.
    // It may be necessary to insert a text node to receive characters.
    KHTMLSelection selection = currentSelection();
    ASSERT(selection.state() == KHTMLSelection::CARET);
    
    DOMPosition pos = selection.startPosition().equivalentUpstreamPosition();
    if (!pos.node()->inSameContainingEditableBlock(selection.startNode()))
        pos = selection.startPosition();
    
    if (!pos.node()->isTextNode()) {
        if (!m_insertedTextNode) {
            m_insertedTextNode = document()->createTextNode("");
            m_insertedTextNode->setRendererIsNeeded();
            m_insertedTextNode->ref();
        }
        
        if (pos.node()->isEditableBlock())
            appendNode(pos.node(), m_insertedTextNode);
        else if (pos.node()->id() == ID_BR || pos.offset() == 1)
            insertNodeAfter(m_insertedTextNode, pos.node());
        else {
            ASSERT(pos.offset() == 0);
            insertNodeBefore(m_insertedTextNode, pos.node());
        }
        
        pos = DOMPosition(m_insertedTextNode, 0);
    }
    
    return pos;
}

void InputTextCommandImpl::execute(const DOMString &text)
{
    KHTMLSelection selection = currentSelection();

    // Delete the current selection
    deleteSelection();
    
    // Make sure the document is set up to receive text
    DOMPosition pos = prepareForTextInsertion();
    
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    long offset = pos.offset();
    
    // This is a temporary implementation for inserting adjoining spaces
    // into a document. We are working on a CSS-related whitespace solution
    // that will replace this some day.
    if (isWS(text))
        insertSpace(textNode, offset);
    else
        insertText(textNode, offset, text);
    selection = KHTMLSelection(textNode, offset + text.length());
    setEndingSelection(selection);
    m_charactersAdded += text.length();
}

void InputTextCommandImpl::insertSpace(TextImpl *textNode, unsigned long offset)
{
    ASSERT(textNode);

    DOMString text(textNode->data());

    // count up all spaces and newlines in front of the caret
    // delete all collapsed ones
    // this will work out OK since the offset we have been passed has been upstream-ized 
    int count = 0;
    for (unsigned int i = offset; i < text.length(); i++) {
        if (isWS(text[i]))
            count++;
        else 
            break;
    }
    if (count > 0) {
        // By checking the character at the downstream position, we can
        // check if there is a rendered WS at the caret
        DOMPosition pos(textNode, offset);
        DOMPosition downstream = pos.equivalentDownstreamPosition();
        if (downstream.offset() < (long)text.length() && isWS(text[downstream.offset()]))
            count--; // leave this WS in
        if (count > 0)
            deleteText(textNode, offset, count);
    }

    if (offset > 0 && offset <= text.length() - 1 && !isWS(text[offset]) && !isWS(text[offset - 1])) {
        // insert a "regular" space
        insertText(textNode, offset, " ");
        return;
    }

    if (text.length() >= 2 && offset >= 2 && isNBSP(text[offset - 2]) && isNBSP(text[offset - 1])) {
        // DOM looks like this:
        // nbsp nbsp caret
        // insert a space between the two nbsps
        insertText(textNode, offset - 1, " ");
        return;
    }

    // insert an nbsp
    insertText(textNode, offset, nonBreakingSpaceString());
}

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommandImpl

InsertNodeBeforeCommandImpl::InsertNodeBeforeCommandImpl(DocumentImpl *document, NodeImpl *insertChild, NodeImpl *refChild)
    : EditCommandImpl(document), m_insertChild(insertChild), m_refChild(refChild)
{
    ASSERT(m_insertChild);
    m_insertChild->ref();

    ASSERT(m_refChild);
    m_refChild->ref();
}

InsertNodeBeforeCommandImpl::~InsertNodeBeforeCommandImpl()
{
    if (m_insertChild)
        m_insertChild->deref();
    if (m_refChild)
        m_refChild->deref();
}

int InsertNodeBeforeCommandImpl::commandID() const
{
    return InsertNodeBeforeCommandID;
}

void InsertNodeBeforeCommandImpl::doApply()
{
    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parent());

    int exceptionCode = 0;
    m_refChild->parent()->insertBefore(m_insertChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertNodeBeforeCommandImpl::doUnapply()
{
    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parent());

    int exceptionCode = 0;
    m_refChild->parent()->removeChild(m_insertChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// InsertTextCommandImpl

InsertTextCommandImpl::InsertTextCommandImpl(DocumentImpl *document, TextImpl *node, long offset, const DOMString &text)
    : EditCommandImpl(document), m_node(node), m_offset(offset)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(text.length() > 0);
    
    m_node->ref();
    m_text = text.copy(); // make a copy to ensure that the string never changes
}

InsertTextCommandImpl::~InsertTextCommandImpl()
{
    if (m_node)
        m_node->deref();
}

int InsertTextCommandImpl::commandID() const
{
    return InsertTextCommandID;
}

void InsertTextCommandImpl::doApply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertTextCommandImpl::doUnapply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->deleteData(m_offset, m_text.length(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// JoinTextNodesCommandImpl

JoinTextNodesCommandImpl::JoinTextNodesCommandImpl(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : EditCommandImpl(document), m_text1(text1), m_text2(text2)
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);
    ASSERT(m_text1->length() > 0);
    ASSERT(m_text2->length() > 0);

    m_text1->ref();
    m_text2->ref();
}

JoinTextNodesCommandImpl::~JoinTextNodesCommandImpl()
{
    if (m_text1)
        m_text1->deref();
    if (m_text2)
        m_text2->deref();
}

int JoinTextNodesCommandImpl::commandID() const
{
    return JoinTextNodesCommandID;
}

void JoinTextNodesCommandImpl::doApply()
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);

    int exceptionCode = 0;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parent()->removeChild(m_text1, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_offset = m_text1->length();
}

void JoinTextNodesCommandImpl::doUnapply()
{
    ASSERT(m_text2);
    ASSERT(m_offset > 0);

    int exceptionCode = 0;

    m_text2->deleteData(0, m_offset, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);
        
    ASSERT(m_text2->previousSibling()->isTextNode());
    ASSERT(m_text2->previousSibling() == m_text1);
}

//------------------------------------------------------------------------------------------
// PasteHTMLCommandImpl

PasteHTMLCommandImpl::PasteHTMLCommandImpl(DocumentImpl *document, const DOMString &HTMLString) 
    : CompositeEditCommandImpl(document)
{
    ASSERT(!HTMLString.isEmpty());
    m_HTMLString = HTMLString; 
}

PasteHTMLCommandImpl::~PasteHTMLCommandImpl()
{
}

int PasteHTMLCommandImpl::commandID() const
{
    return PasteHTMLCommandID;
}

void PasteHTMLCommandImpl::doApply()
{
    DOM::DocumentFragmentImpl *root = static_cast<HTMLElementImpl *>(document()->documentElement())->createContextualFragment(m_HTMLString);
    ASSERT(root);
    
    DOM::NodeImpl *firstChild = root->firstChild();
    DOM::NodeImpl *lastChild = root->lastChild();
    ASSERT(firstChild);
    ASSERT(lastChild);
    
    deleteSelection();
    
    KHTMLPart *part = document()->part();
    ASSERT(part);
    
    KHTMLSelection selection = part->selection();
    ASSERT(!selection.isEmpty());
    
    DOM::NodeImpl *startNode = selection.startNode();
    long startOffset = selection.startOffset();
    TextImpl *textNode = startNode->isTextNode() ? static_cast<TextImpl *>(startNode) : NULL;

    if (textNode && firstChild == lastChild && firstChild->isTextNode()) {
        // Simple text paste. Add the text to the text node with the caret.
        insertText(textNode, startOffset, static_cast<TextImpl *>(firstChild)->data());
        selection = KHTMLSelection(textNode, startOffset + static_cast<TextImpl *>(firstChild)->length());
        setEndingSelection(selection);
    } 
    else {
        // HTML tree paste.
        insertNodeAt(firstChild, startNode, startOffset);
        
        DOM::NodeImpl *child = startNode->nextSibling();
        DOM::NodeImpl *beforeNode = startNode;
		
        // Insert the nodes from the clipping.
        while (child) {
            DOM::NodeImpl *nextSibling = child->nextSibling();
            insertNodeAfter(child, beforeNode);
            beforeNode = child;
            child = nextSibling;
        }
		
		// Find the last leaf and place the caret after it.
        child = lastChild;
        while (1) {
            DOM::NodeImpl *nextChild = child->lastChild();
            if (!nextChild) {
                break;
            }
            child = nextChild;
        }
        selection = KHTMLSelection(child, child->caretMaxOffset());
        setEndingSelection(selection);
    }
}

//------------------------------------------------------------------------------------------
// PasteImageCommandImpl

PasteImageCommandImpl::PasteImageCommandImpl(DocumentImpl *document, const DOMString &src) 
: CompositeEditCommandImpl(document)
{
    ASSERT(!src.isEmpty());
    m_src = src; 
}

PasteImageCommandImpl::~PasteImageCommandImpl()
{
}

int PasteImageCommandImpl::commandID() const
{
    return PasteImageCommandID;
}

void PasteImageCommandImpl::doApply()
{
    deleteSelection();
    
    KHTMLPart *part = document()->part();
    ASSERT(part);
    
    KHTMLSelection selection = part->selection();
    ASSERT(!selection.isEmpty());
    
    DOM::NodeImpl *startNode = selection.startNode();
    HTMLImageElementImpl *imageNode = new HTMLImageElementImpl(startNode->docPtr());
    imageNode->setAttribute(ATTR_SRC, m_src);
    
    insertNodeAt(imageNode, startNode, selection.startOffset());
    selection = KHTMLSelection(imageNode, imageNode->caretMaxOffset());
    setEndingSelection(selection);
}

//------------------------------------------------------------------------------------------
// RemoveNodeCommandImpl

RemoveNodeCommandImpl::RemoveNodeCommandImpl(DocumentImpl *document, NodeImpl *removeChild)
    : EditCommandImpl(document), m_parent(0), m_removeChild(removeChild), m_refChild(0)
{
    ASSERT(m_removeChild);
    m_removeChild->ref();

    m_parent = m_removeChild->parentNode();
    ASSERT(m_parent);
    m_parent->ref();
    
    NodeListImpl *children = m_parent->childNodes();
    for (int i = children->length(); i >= 0; i--) {
        NodeImpl *node = children->item(i);
        if (node == m_removeChild)
            break;
        m_refChild = node;
    }
    
    if (m_refChild)
        m_refChild->ref();
}

RemoveNodeCommandImpl::~RemoveNodeCommandImpl()
{
    if (m_parent)
        m_parent->deref();
    if (m_removeChild)
        m_removeChild->deref();
    if (m_refChild)
        m_refChild->deref();
}

int RemoveNodeCommandImpl::commandID() const
{
    return RemoveNodeCommandID;
}

void RemoveNodeCommandImpl::doApply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->removeChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeCommandImpl::doUnapply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    if (m_refChild)
        m_parent->insertBefore(m_removeChild, m_refChild, exceptionCode);
    else
        m_parent->appendChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// RemoveNodeAndPruneCommandImpl

RemoveNodeAndPruneCommandImpl::RemoveNodeAndPruneCommandImpl(DocumentImpl *document, NodeImpl *removeChild)
    : CompositeEditCommandImpl(document), m_removeChild(removeChild)
{
    ASSERT(m_removeChild);
    m_removeChild->ref();
}

RemoveNodeAndPruneCommandImpl::~RemoveNodeAndPruneCommandImpl()
{
    if (m_removeChild)
        m_removeChild->deref();
}

int RemoveNodeAndPruneCommandImpl::commandID() const
{
    return RemoveNodeAndPruneCommandID;
}

void RemoveNodeAndPruneCommandImpl::doApply()
{
    NodeImpl *editableBlock = m_removeChild->containingEditableBlock();
    NodeImpl *pruneNode = m_removeChild;
    NodeImpl *node = pruneNode->traversePreviousNode();
    removeNode(pruneNode);
    while (1) {
        if (editableBlock != node->containingEditableBlock() || !shouldPruneNode(node))
            break;
        pruneNode = node;
        node = node->traversePreviousNode();
        removeNode(pruneNode);
    }
}

//------------------------------------------------------------------------------------------
// SplitTextNodeCommandImpl

SplitTextNodeCommandImpl::SplitTextNodeCommandImpl(DocumentImpl *document, TextImpl *text, long offset)
    : EditCommandImpl(document), m_text2(text), m_offset(offset)
{
}

SplitTextNodeCommandImpl::~SplitTextNodeCommandImpl()
{
    if (m_text1)
        m_text1->deref();
    if (m_text2)
        m_text2->deref();
}

int SplitTextNodeCommandImpl::commandID() const
{
    return SplitTextNodeCommandID;
}

void SplitTextNodeCommandImpl::doApply()
{
    ASSERT(m_text2);
    ASSERT(m_offset > 0);

    int exceptionCode = 0;

    // EDIT FIXME: This should use better smarts for figuring out which portion
    // of the split to copy (based on their comparitive sizes). We should also
    // just use the DOM's splitText function.
    
    if (!m_text1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_text1 = document()->createTextNode(m_text2->substringData(0, m_offset, exceptionCode));
        ASSERT(exceptionCode == 0);
        ASSERT(m_text1);
        m_text1->ref();
    }

    m_text2->deleteData(0, m_offset, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);
        
    ASSERT(m_text2->previousSibling()->isTextNode());
    ASSERT(m_text2->previousSibling() == m_text1);
}

void SplitTextNodeCommandImpl::doUnapply()
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    
    ASSERT(m_text1->nextSibling() == m_text2);

    int exceptionCode = 0;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parent()->removeChild(m_text1, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_offset = m_text1->length();
}

//------------------------------------------------------------------------------------------
// TypingCommandImpl

TypingCommandImpl::TypingCommandImpl(DocumentImpl *document)
    : CompositeEditCommandImpl(document), m_openForMoreTyping(true)
{
}

TypingCommandImpl::~TypingCommandImpl()
{
}

int TypingCommandImpl::commandID() const
{
    return TypingCommandID;
}

void TypingCommandImpl::doApply()
{
}

void TypingCommandImpl::insertText(const DOM::DOMString &text)
{
    if (m_cmds.count() == 0) {
        InputTextCommand cmd(document());
        applyCommandToComposite(cmd);
        cmd.input(text);
        setEndingSelection(cmd.endingSelection());
    }
    else {
        EditCommand lastCommand = m_cmds.last();
        if (lastCommand.commandID() == InputTextCommandID) {
            static_cast<InputTextCommand &>(lastCommand).input(text);
            setEndingSelection(lastCommand.endingSelection());
        }
        else {
            InputTextCommand cmd(document());
            applyCommandToComposite(cmd);
            cmd.input(text);
            setEndingSelection(cmd.endingSelection());
        }
    }
}

void TypingCommandImpl::insertNewline()
{
    InputNewlineCommand cmd(document());
    applyCommandToComposite(cmd);
}

void TypingCommandImpl::issueCommandForDeleteKey()
{
    KHTMLSelection selection = currentSelection();
    ASSERT(selection.state() != KHTMLSelection::NONE);
    
    if (selection.state() == KHTMLSelection::CARET) {
        KHTMLSelection selectionToDelete(selection.startPosition().previousCharacterPosition(), selection.startPosition());
        setEndingSelection(selectionToDelete);
        deleteCollapsibleWhitespace();
        selection = currentSelection();
        deleteSelection(selection);
    }
    else { // selection.state() == KHTMLSelection::RANGE
        deleteCollapsibleWhitespace();
        deleteSelection();
    }
}

void TypingCommandImpl::deleteKeyPressed()
{
    if (m_cmds.count() == 0) {
        issueCommandForDeleteKey();
    }
    else {
        EditCommand lastCommand = m_cmds.last();
        if (lastCommand.commandID() == InputTextCommandID) {
            InputTextCommand cmd = static_cast<InputTextCommand &>(lastCommand);
            cmd.deleteCharacter();
            if (cmd.charactersAdded() == 0) {
                removeCommand(cmd);
            }
            else {
                setEndingSelection(cmd.endingSelection());
            }
        }
        else if (lastCommand.commandID() == InputNewlineCommandID) {
            lastCommand.unapply();
            removeCommand(lastCommand);
        }
        else {
            issueCommandForDeleteKey();
        }
    }
}

void TypingCommandImpl::removeCommand(const EditCommand &cmd)
{
    // NOTE: If the passed-in command is the last command in the
    // composite, we could remove all traces of this typing command
    // from the system, including the undo chain. Other editors do
    // not do this, but we could.

    m_cmds.remove(cmd);
    if (m_cmds.count() == 0)
        setEndingSelection(startingSelection());
    else
        setEndingSelection(m_cmds.last().endingSelection());
}

//------------------------------------------------------------------------------------------

