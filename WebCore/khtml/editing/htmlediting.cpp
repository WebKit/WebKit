/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "htmlediting.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#endif

#include "khtmlview.h"
#include "khtml_part.h"
#include "khtml_selection.h"
#include "dom/dom_position.h"
#include "rendering/render_object.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_textimpl.h"

using DOM::DocumentImpl;
using DOM::DOMPosition;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;

using khtml::AppendNodeStep;
using khtml::CompositeEditStep;
using khtml::DeleteSelectionStep;
using khtml::DeleteTextStep;
using khtml::EditStep;
using khtml::InsertNodeBeforeStep;
using khtml::InsertTextStep;
using khtml::JoinTextNodesStep;
using khtml::ModifyTextNodeStep;
using khtml::RemoveNodeStep;
using khtml::MoveSelectionToStep;
using khtml::SplitTextNodeStep;

using khtml::DeleteTextCommand;
using khtml::EditCommand;
using khtml::InputTextCommand;

#if !APPLE_CHANGES
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#endif

//------------------------------------------------------------------------------------------

#pragma mark EditSteps

//------------------------------------------------------------------------------------------
// EditStep

EditStep::EditStep(DocumentImpl *document) : m_document(document), m_state(NOT_APPLIED)
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    m_document->ref();
    m_startingSelection = m_document->part()->selection();
    m_endingSelection = m_startingSelection;
}

EditStep::~EditStep()
{
    ASSERT(m_document);
    m_document->deref();
}

void EditStep::reapply()
{
    apply();
}

inline void EditStep::beginApply()
{
    ASSERT(state() == NOT_APPLIED);
}

inline void EditStep::endApply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());

    m_state = APPLIED;
    m_document->part()->setSelection(m_endingSelection);
}

inline void EditStep::beginUnapply()
{
    ASSERT(state() == APPLIED);
}

inline void EditStep::endUnapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());

    m_state = NOT_APPLIED;
    m_document->part()->setSelection(m_startingSelection);
}

inline void EditStep::beginReapply()
{
    beginApply();
}

inline void EditStep::endReapply()
{
    endApply();
}

//------------------------------------------------------------------------------------------
// CompositeEditStep

CompositeEditStep::CompositeEditStep(DocumentImpl *document) 
    : EditStep(document)
{
    m_steps.setAutoDelete(true);
}

CompositeEditStep::~CompositeEditStep()
{
}

void CompositeEditStep::unapply()
{
    if (m_steps.count() == 0) {
        ERROR("Unapplying composite step containing zero steps");
    }
    QPtrListIterator<EditStep> it(m_steps);
    for (it.toLast(); it.current(); --it) {
        it.current()->unapply();
    }
}

void CompositeEditStep::reapply()
{
    if (m_steps.count() == 0) {
        ERROR("Reapplying composite step containing zero steps");
    }
    QPtrListIterator<EditStep> it(m_steps);
    for (; it.current(); ++it) {
        it.current()->reapply();
    }
}

//
// sugary-sweet convenience functions to help create and apply edit steps
//
void CompositeEditStep::applyStep(EditStep *step)
{
    step->apply();
    m_steps.append(step);
}

void CompositeEditStep::insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild)
{
    applyStep(new InsertNodeBeforeStep(document(), insertChild, refChild));
}

void CompositeEditStep::insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild)
{
    if (refChild->parentNode()->lastChild() == refChild) {
        appendNode(refChild->parentNode(), insertChild);
    }
    else {
        ASSERT(refChild->nextSibling());
        insertNodeBefore(insertChild, refChild->nextSibling());
    }
}

void CompositeEditStep::appendNode(DOM::NodeImpl *parent, DOM::NodeImpl *appendChild)
{
    applyStep(new AppendNodeStep(document(), parent, appendChild));
}

void CompositeEditStep::removeNode(DOM::NodeImpl *removeChild)
{
    applyStep(new RemoveNodeStep(document(), removeChild));
}

void CompositeEditStep::splitTextNode(DOM::TextImpl *text, long offset)
{
    applyStep(new SplitTextNodeStep(document(), text, offset));
}

void CompositeEditStep::joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2)
{
    applyStep(new JoinTextNodesStep(document(), text1, text2));
}

void CompositeEditStep::insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text)
{
    applyStep(new InsertTextStep(document(), node, offset, text));
}

void CompositeEditStep::deleteText(DOM::TextImpl *node, long offset, long count)
{
    applyStep(new DeleteTextStep(document(), node, offset, count));
}

void CompositeEditStep::moveSelectionTo(const KHTMLSelection &selection)
{
    applyStep(new MoveSelectionToStep(document(), selection));
}

void CompositeEditStep::moveSelectionTo(DOM::NodeImpl *node, long offset)
{
    applyStep(new MoveSelectionToStep(document(), node, offset));
}

void CompositeEditStep::moveSelectionTo(const DOM::DOMPosition &pos)
{
    applyStep(new MoveSelectionToStep(document(), pos));
}

void CompositeEditStep::deleteSelection()
{
    applyStep(new DeleteSelectionStep(document()));
}

void CompositeEditStep::deleteSelection(const KHTMLSelection &selection)
{
    applyStep(new DeleteSelectionStep(document(), selection));
}

//------------------------------------------------------------------------------------------
// InsertNodeBeforeStep

InsertNodeBeforeStep::InsertNodeBeforeStep(DocumentImpl *document, NodeImpl *insertChild, NodeImpl *refChild)
    : EditStep(document), m_insertChild(insertChild), m_refChild(refChild)
{
    ASSERT(m_insertChild);
    m_insertChild->ref();

    ASSERT(m_refChild);
    m_refChild->ref();
}

InsertNodeBeforeStep::~InsertNodeBeforeStep()
{
    if (m_insertChild)
        m_insertChild->deref();
    if (m_refChild)
        m_refChild->deref();
}

void InsertNodeBeforeStep::apply()
{
    beginApply();

    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parent());

    int exceptionCode;
    m_refChild->parent()->insertBefore(m_insertChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);

    endApply();
}

void InsertNodeBeforeStep::unapply()
{
    beginUnapply();

    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parent());

    int exceptionCode;
    m_refChild->parent()->removeChild(m_insertChild, exceptionCode);
    ASSERT(exceptionCode == 0);

    endUnapply();
}

//------------------------------------------------------------------------------------------
// AppendNodeStep

AppendNodeStep::AppendNodeStep(DocumentImpl *document, NodeImpl *parent, NodeImpl *appendChild)
    : EditStep(document), m_parent(parent), m_appendChild(appendChild)
{
    ASSERT(m_parent);
    m_parent->ref();

    ASSERT(m_appendChild);
    m_appendChild->ref();
}

AppendNodeStep::~AppendNodeStep()
{
    if (m_parent)
        m_parent->deref();
    if (m_appendChild)
        m_appendChild->deref();
}

void AppendNodeStep::apply()
{
    beginApply();

    ASSERT(m_parent);
    ASSERT(m_appendChild);

    int exceptionCode;
    m_parent->appendChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);

    endApply();
}

void AppendNodeStep::unapply()
{
    beginUnapply();

    ASSERT(m_parent);
    ASSERT(m_appendChild);
    ASSERT(state() == APPLIED);

    int exceptionCode;
    m_parent->removeChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);

    endUnapply();
}

//------------------------------------------------------------------------------------------
// RemoveNodeStep

RemoveNodeStep::RemoveNodeStep(DocumentImpl *document, NodeImpl *removeChild)
    : EditStep(document), m_parent(0), m_removeChild(removeChild), m_refChild(0)
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

RemoveNodeStep::~RemoveNodeStep()
{
    if (m_parent)
        m_parent->deref();
    if (m_removeChild)
        m_removeChild->deref();
    if (m_refChild)
        m_refChild->deref();
}

void RemoveNodeStep::apply()
{
    beginApply();

    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode;
    m_parent->removeChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);

    endApply();
}

void RemoveNodeStep::unapply()
{
    beginUnapply();

    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode;
    if (m_refChild)
        m_parent->insertBefore(m_removeChild, m_refChild, exceptionCode);
    else
        m_parent->appendChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);

    endUnapply();
}

//------------------------------------------------------------------------------------------
// ModifyTextNodeStep

ModifyTextNodeStep::ModifyTextNodeStep(DocumentImpl *document, TextImpl *text, long offset)
    : EditStep(document), m_text1(0), m_text2(text), m_offset(offset)
{
    ASSERT(m_text2);
    ASSERT(m_text2->length() > 0);
    ASSERT(m_offset >= 0);

    m_text2->ref();
}

ModifyTextNodeStep::ModifyTextNodeStep(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : EditStep(document), m_text1(text1), m_text2(text2), m_offset(0)
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);
    ASSERT(m_text1->length() > 0);
    ASSERT(m_text2->length() > 0);

    m_text1->ref();
    m_text2->ref();
}

ModifyTextNodeStep::~ModifyTextNodeStep()
{
    if (m_text2)
        m_text2->deref();
    if (m_text1)
        m_text1->deref();
}

void ModifyTextNodeStep::splitTextNode()
{
    ASSERT(m_text2);
    ASSERT(m_text1 == 0);
    ASSERT(m_offset > 0);
    ASSERT(state() == splitState());

    ASSERT(m_offset >= m_text2->caretMinOffset() && m_offset <= m_text2->caretMaxOffset());

    int exceptionCode;
    TextImpl *m_text1 = document()->createTextNode(m_text2->substringData(0, m_offset, exceptionCode));
    ASSERT(exceptionCode == 0);
    ASSERT(m_text1);
    m_text1->ref();

    m_text2->deleteData(0, m_offset, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);
        
    ASSERT(m_text2->previousSibling()->isTextNode());
    m_text1 = static_cast<TextImpl *>(m_text2->previousSibling());
}

void ModifyTextNodeStep::joinTextNodes()
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(state() == joinState());
    
    ASSERT(m_text1->nextSibling() == m_text2);

    int exceptionCode;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parent()->removeChild(m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_offset = m_text1->length();
    m_text1->deref();
    m_text1 = 0;
}

//------------------------------------------------------------------------------------------
// SplitTextNodeStep

SplitTextNodeStep::SplitTextNodeStep(DocumentImpl *document, TextImpl *text, long offset)
    : ModifyTextNodeStep(document, text, offset)
{
}

SplitTextNodeStep::~SplitTextNodeStep()
{
}

void SplitTextNodeStep::apply()
{
    beginApply();
    splitTextNode();
    endApply();
}

void SplitTextNodeStep::unapply()
{
    beginUnapply();
    joinTextNodes();
    endUnapply();
}

//------------------------------------------------------------------------------------------
// SplitTextNodeStep

JoinTextNodesStep::JoinTextNodesStep(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : ModifyTextNodeStep(document, text1, text2)
{
}

JoinTextNodesStep::~JoinTextNodesStep()
{
}

void JoinTextNodesStep::apply()
{
    beginApply();
    joinTextNodes();
    endApply();
}

void JoinTextNodesStep::unapply()
{
    beginUnapply();
    splitTextNode();
    endUnapply();
}

//------------------------------------------------------------------------------------------
// InsertTextStep

InsertTextStep::InsertTextStep(DocumentImpl *document, TextImpl *node, long offset, const DOMString &text)
    : EditStep(document), m_node(node), m_offset(offset)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(text.length() > 0);
    
    m_node->ref();
    m_text = text.copy(); // make a copy to ensure that the string never changes
}

InsertTextStep::~InsertTextStep()
{
    if (m_node)
        m_node->deref();
}

void InsertTextStep::apply()
{
    beginApply();

    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);

    endApply();
}

void InsertTextStep::unapply()
{
    beginUnapply();

    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode;
    m_node->deleteData(m_offset, m_text.length(), exceptionCode);
    ASSERT(exceptionCode == 0);

    endUnapply();
}

//------------------------------------------------------------------------------------------
// DeleteTextStep

DeleteTextStep::DeleteTextStep(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditStep(document), m_node(node), m_offset(offset), m_count(count)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(m_count >= 0);
    
    m_node->ref();
}

DeleteTextStep::~DeleteTextStep()
{
    if (m_node)
        m_node->deref();
}

void DeleteTextStep::apply()
{
    beginApply();

    ASSERT(m_node);

    int exceptionCode;
    m_text = m_node->substringData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    m_node->deleteData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);

    endApply();
}

void DeleteTextStep::unapply()
{
    beginUnapply();

    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);

    endUnapply();
}

//------------------------------------------------------------------------------------------
// MoveSelectionToStep

MoveSelectionToStep::MoveSelectionToStep(DocumentImpl *document, const KHTMLSelection &selection)
    : EditStep(document)
{
    setEndingSelection(selection);
}

MoveSelectionToStep::MoveSelectionToStep(DocumentImpl *document, DOM::NodeImpl *node, long offset)
    : EditStep(document)
{
    KHTMLSelection selection(node, offset);
    setEndingSelection(selection);
}

MoveSelectionToStep::MoveSelectionToStep(DOM::DocumentImpl *document, const DOM::DOMPosition &pos)
    : EditStep(document)
{
    KHTMLSelection selection(pos);
    setEndingSelection(selection);
}

void MoveSelectionToStep::apply()
{
    beginApply();
    endApply();
}

void MoveSelectionToStep::unapply()
{
    beginUnapply();
    endUnapply();
}

//------------------------------------------------------------------------------------------
// DeleteSelectionStep

DeleteSelectionStep::DeleteSelectionStep(DOM::DocumentImpl *document)
    : CompositeEditStep(document)
{
}

DeleteSelectionStep::DeleteSelectionStep(DOM::DocumentImpl *document, const KHTMLSelection &selection)
    : CompositeEditStep(document)
{
    setStartingSelection(selection);
}

DeleteSelectionStep::~DeleteSelectionStep()
{
}
	
void DeleteSelectionStep::apply()
{
    beginApply();

    if (startingSelection().isEmpty()) {
        endApply();
        return;
    }

    KHTMLSelection selection = startingSelection();

    //
    // Figure out where to place the caret after doing the delete:
    //
    // 1. If the start node is not completely selected, use the start 
    //    node and start offset for the new position; else
    // 2. If the start and end nodes are completely selected:
    //       a. If there is an editable node following the end node, 
    //          place the caret in the min caret offset of that node; else
    //       b. If there is an editable node before the start node, 
    //          place the caret in the max caret offset of that node; else
    //       c. There is no more editable content in the document.
    //          EDIT FIXME: We do not handle this case now
    // 3. If the start node is completely selected and the end node is 
    //    different than the start node and it is not completely selected,
    //    use the end node and the end node min caret for the new position; else
    //

    DOMPosition deleteStart = DOMPosition(selection.startNode(), selection.startOffset());
    DOMPosition deleteEnd = DOMPosition(selection.endNode(), selection.endOffset());

    bool startIsCompletelySelected = 
        deleteStart.offset() == deleteStart.node()->caretMinOffset() &&
        ((deleteStart.node() != deleteEnd.node()) ||
         (deleteEnd.offset() == deleteEnd.node()->caretMaxOffset()));

    bool endIsCompletelySelected = 
        deleteEnd.offset() == deleteEnd.node()->caretMaxOffset() &&
        ((deleteStart.node() != deleteEnd.node()) ||
         (deleteStart.offset() == deleteStart.node()->caretMinOffset()));

    DOMPosition endingPosition;

    if (!startIsCompletelySelected) {
        // Case 1
        endingPosition = DOMPosition(deleteStart.node(), deleteStart.offset());
    }
    else if (endIsCompletelySelected) {
        DOMPosition pos = selection.nextCharacterPosition(deleteEnd);
        if (pos != deleteEnd) {
            // Case 2a
            endingPosition = DOMPosition(pos.node(), pos.node()->caretMinOffset());
        }
        else {
            pos = selection.previousCharacterPosition(deleteStart);
            if (pos != deleteStart) {
                // Case 2b
                endingPosition = DOMPosition(pos.node(), pos.node()->caretMaxOffset());
            }
            else {
                // Case 2c
                // EDIT FIXME
                endingPosition = DOMPosition();
            }
        }
    }
    else {
        // Case 3
        endingPosition = DOMPosition(deleteEnd.node(), deleteEnd.node()->caretMinOffset());
    }

    //
    // Do the delete
    //
    NodeImpl *n = deleteStart.node()->nextLeafNode();

    // work on start node
    if (startIsCompletelySelected) {
        removeNode(deleteStart.node());
    }
    else if (deleteStart.node()->isTextNode()) {
        TextImpl *text = static_cast<TextImpl *>(deleteStart.node());
        int endOffset = text == deleteEnd.node() ? deleteEnd.offset() : text->length();
        if (endOffset > deleteStart.offset()) {
            deleteText(text, deleteStart.offset(), endOffset - deleteStart.offset());
        }
    }
    else {
        ASSERT_NOT_REACHED();
    }

    if (deleteStart.node() != deleteEnd.node()) {
        // work on intermediate nodes
        while (n != deleteEnd.node()) {
            NodeImpl *d = n;
            n = n->nextLeafNode();
            removeNode(d);
        }
        
        // work on end node
        ASSERT(n == deleteEnd.node());
        if (endIsCompletelySelected) {
            removeNode(deleteEnd.node());
        }
        else if (deleteEnd.node()->isTextNode()) {
            if (deleteEnd.offset() > 0) {
                TextImpl *text = static_cast<TextImpl *>(deleteEnd.node());
                deleteText(text, 0, deleteEnd.offset());
            }
        }
        else {
            ASSERT_NOT_REACHED();
        }
    }

    //
    // set the ending selection
    //
    selection.moveTo(endingPosition);
    selection.moveToRenderedContent();
    setEndingSelection(selection);

    endApply();
}

//------------------------------------------------------------------------------------------

#pragma mark EditCommands

//------------------------------------------------------------------------------------------
// EditCommand

static int cookieCounter = 0;

EditCommand::EditCommand(DocumentImpl *document) : CompositeEditStep(document)
{
    m_cookie = cookieCounter++;
}

EditCommand::~EditCommand()
{
}

const KHTMLSelection &EditCommand::currentSelection() const
{
    ASSERT(document());
    ASSERT(document()->part());
    return document()->part()->selection();
}

//------------------------------------------------------------------------------------------
// InputTextCommand

InputTextCommand::InputTextCommand(DocumentImpl *document, const DOMString &text) 
    : EditCommand(document)
{
    if (text.isEmpty()) {
#if APPLE_CHANGES
        ERROR("InputTextCommand constructed with zero-length string");
#endif
        m_text = "";
    }
    else {
        m_text = text; 
    }
}

bool InputTextCommand::isLineBreak() const
{
    return m_text.length() == 1 && (m_text[0] == '\n' || m_text[0] == '\r');
}

bool InputTextCommand::isSpace() const
{
    return m_text.length() == 1 && (m_text[0] == ' ');
}

void InputTextCommand::apply()
{
    KHTMLSelection selection = startingSelection();

    if (!selection.startNode()->isTextNode())
        return;

    // Delete the current selection
    if (selection.state() == KHTMLSelection::RANGE) {
        deleteSelection();
    }
    selection = currentSelection();
    
    TextImpl *textNode = static_cast<TextImpl *>(selection.startNode());
    
    if (isLineBreak()) {
        int exceptionCode;
        ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);

        bool atStart = selection.startOffset() == textNode->renderer()->caretMinOffset();
        bool atEnd = selection.startOffset() == textNode->renderer()->caretMaxOffset();
        if (atStart) {
            // Set the cursor at the beginning of text node now following the new BR.
            insertNodeBefore(breakNode, textNode);
            moveSelectionTo(textNode, 0);
        }
        else if (atEnd) {
            insertNodeAfter(breakNode, textNode);
            // Set the cursor at the beginning of the the BR.
            moveSelectionTo(selection.nextCharacterPosition());
        }
        else {
            TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.startOffset(), exceptionCode));
            deleteText(textNode, 0, selection.startOffset());
            insertNodeBefore(textBeforeNode, textNode);
            insertNodeBefore(breakNode, textNode);
            textBeforeNode->deref();
            // Set the cursor at the beginning of the node after the BR.
            moveSelectionTo(textNode, 0);
        }
        
        breakNode->deref();
    }
    else {
        insertText(textNode, selection.startOffset(), text());
        moveSelectionTo(selection.startNode(), selection.startOffset() + text().length());
    }
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

DeleteTextCommand::DeleteTextCommand(DocumentImpl *document) 
    : EditCommand(document)
{
}

void DeleteTextCommand::apply()
{
    KHTMLPart *part = document()->part();
    ASSERT(part);

    KHTMLSelection selection = part->selection();
    ASSERT(!selection.isEmpty());

    // Delete the current selection
    if (selection.state() == KHTMLSelection::RANGE) {
        deleteSelection();
        return;
    }

    NodeImpl *caretNode = selection.startNode();

    if (caretNode->isTextNode()) {
        // Check if we can delete character at cursor
        int offset = selection.startOffset() - 1;
        if (offset >= caretNode->caretMinOffset()) {
            TextImpl *textNode = static_cast<TextImpl *>(caretNode);
            deleteText(textNode, offset, 1);
            moveSelectionTo(textNode, offset);
            return;
        }
        
        // Check if previous sibling is a BR element
        NodeImpl *previousSibling = caretNode->previousSibling();
        if (previousSibling->renderer() && previousSibling->renderer()->isBR()) {
            removeNode(previousSibling);
            return;
        }
        
        // Check if previous leaf node is a text node
        NodeImpl *previousLeafNode = caretNode->previousLeafNode();
        if (previousLeafNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(previousLeafNode);
            offset = previousLeafNode->caretMaxOffset() - 1;
            deleteText(textNode, offset, 1);
            moveSelectionTo(textNode, offset);
            return;
        }
    }
}
