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
using DOM::DOMException;
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
using khtml::SetSelectionStep;
using khtml::SplitTextNodeStep;

using khtml::DeleteTextCommand;
using khtml::EditCommand;
using khtml::InputTextCommand;

#define APPLY_STEP(s) do { \
        int result = s->apply(); \
        if (result) { \
            return result; \
        } \
        m_steps.append(s); \
    } while (0)

//------------------------------------------------------------------------------------------

#pragma mark EditSteps

//------------------------------------------------------------------------------------------
// EditStep

EditStep::EditStep(DocumentImpl *document) : m_document(document), m_state(NOT_APPLIED)
{
    assert(m_document);
    assert(m_document->part());
    m_document->ref();
    m_startingSelection = m_document->part()->selection();
    m_endingSelection = m_startingSelection;
}

EditStep::~EditStep()
{
    assert(m_document);
    m_document->deref();
}

int EditStep::apply()
{
    assert(m_document);
    assert(m_document->part());

    m_state = APPLIED;
    m_document->part()->setSelection(m_endingSelection);
    return EditResultOK;
}

int EditStep::unapply()
{
    assert(m_document);
    assert(m_document->part());

    m_state = NOT_APPLIED;
    m_document->part()->setSelection(m_startingSelection);
    return EditResultOK;
}

int EditStep::reapply()
{
    return apply();
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

int CompositeEditStep::unapply()
{
    QPtrListIterator<EditStep> it(m_steps);
    for (it.toLast(); it.current(); --it) {
        int result = it.current()->unapply();
        if (result != EditResultOK)
            return result;
    }

    return EditStep::unapply();
}

int CompositeEditStep::reapply()
{
    QPtrListIterator<EditStep> it(m_steps);
    for (; it.current(); ++it) {
        int result = it.current()->reapply();
        if (result != EditResultOK)
            return result;
    }

    // Calls apply() and not reapply(), given that the default implementation of
    // EditStep::reapply() calls apply(), which dispatches virtually.
    return EditStep::apply();
}

//------------------------------------------------------------------------------------------
// InsertNodeBeforeStep

InsertNodeBeforeStep::InsertNodeBeforeStep(DocumentImpl *document, NodeImpl *insertChild, NodeImpl *refChild)
    : EditStep(document), m_insertChild(insertChild), m_refChild(refChild)
{
    assert(m_insertChild);
    m_insertChild->ref();

    assert(m_refChild);
    m_refChild->ref();
}

InsertNodeBeforeStep::~InsertNodeBeforeStep()
{
    if (m_insertChild)
        m_insertChild->deref();
    if (m_refChild)
        m_refChild->deref();
}

int InsertNodeBeforeStep::apply()
{
    assert(m_insertChild);
    assert(m_refChild);
    assert(m_refChild->parent());
    assert(state() == NOT_APPLIED);

    int exceptionCode;
    m_refChild->parent()->insertBefore(m_insertChild, m_refChild, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::apply();
}

int InsertNodeBeforeStep::unapply()
{
    assert(m_insertChild);
    assert(m_refChild);
    assert(m_refChild->parent());
    assert(state() == APPLIED);

    int exceptionCode;
    m_refChild->parent()->removeChild(m_insertChild, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::unapply();
}

//------------------------------------------------------------------------------------------
// AppendNodeStep

AppendNodeStep::AppendNodeStep(DocumentImpl *document, NodeImpl *parent, NodeImpl *appendChild)
    : EditStep(document), m_parent(parent), m_appendChild(appendChild)
{
    assert(m_parent);
    m_parent->ref();

    assert(m_appendChild);
    m_appendChild->ref();
}

AppendNodeStep::~AppendNodeStep()
{
    if (m_parent)
        m_parent->deref();
    if (m_appendChild)
        m_appendChild->deref();
}

int AppendNodeStep::apply()
{
    assert(m_parent);
    assert(m_appendChild);
    assert(state() == NOT_APPLIED);

    int exceptionCode;
    m_parent->appendChild(m_appendChild, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::apply();
}

int AppendNodeStep::unapply()
{
    assert(m_parent);
    assert(m_appendChild);
    assert(state() == APPLIED);

    int exceptionCode;
    m_parent->removeChild(m_appendChild, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::unapply();
}

//------------------------------------------------------------------------------------------
// RemoveNodeStep

RemoveNodeStep::RemoveNodeStep(DocumentImpl *document, NodeImpl *removeChild)
    : EditStep(document), m_parent(0), m_removeChild(removeChild), m_refChild(0)
{
    assert(m_removeChild);
    m_removeChild->ref();

    m_parent = m_removeChild->parentNode();
    assert(m_parent);
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

int RemoveNodeStep::apply()
{
    assert(m_parent);
    assert(m_removeChild);
    assert(state() == NOT_APPLIED);

    int exceptionCode;
    m_parent->removeChild(m_removeChild, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::apply();
}

int RemoveNodeStep::unapply()
{
    assert(m_parent);
    assert(m_removeChild);
    assert(state() == APPLIED);

    int exceptionCode;
    if (m_refChild)
        m_parent->insertBefore(m_removeChild, m_refChild, exceptionCode);
    else
        m_parent->appendChild(m_removeChild, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::unapply();
}

//------------------------------------------------------------------------------------------
// ModifyTextNodeStep

ModifyTextNodeStep::ModifyTextNodeStep(DocumentImpl *document, TextImpl *text, long offset)
    : EditStep(document), m_text1(0), m_text2(text), m_offset(offset)
{
    assert(m_text2);
    assert(m_text2->length() > 0);
    assert(m_offset >= 0);

    m_text2->ref();
}

ModifyTextNodeStep::ModifyTextNodeStep(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : EditStep(document), m_text1(text1), m_text2(text2), m_offset(0)
{
    assert(m_text1);
    assert(m_text2);
    assert(m_text1->nextSibling() == m_text2);
    assert(m_text1->length() > 0);
    assert(m_text2->length() > 0);

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

int ModifyTextNodeStep::splitTextNode()
{
    assert(m_text2);
    assert(m_text1 == 0);
    assert(m_offset > 0);
    assert(state() == splitState());

    RenderObject *textRenderer = m_text2->renderer();
    if (!textRenderer)
        return EditResultFailed;

    if (m_offset <= textRenderer->caretMinOffset() || m_offset >= textRenderer->caretMaxOffset())
        return EditResultNoActionTaken;

    int exceptionCode;
    TextImpl *m_text1 = document()->createTextNode(m_text2->substringData(0, m_offset, exceptionCode));
    if (exceptionCode)
        return exceptionCode;
    assert(m_text1);
    m_text1->ref();

    m_text2->deleteData(0, m_offset, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    if (exceptionCode)
        return exceptionCode;
        
    assert(m_text2->previousSibling()->isTextNode());
    m_text1 = static_cast<TextImpl *>(m_text2->previousSibling());
    
    return EditResultOK;
}

int ModifyTextNodeStep::joinTextNodes()
{
    assert(m_text1);
    assert(m_text2);
    assert(state() == joinState());
    
    if (m_text1->nextSibling() != m_text2)
        return EditResultFailed;

    int exceptionCode;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    m_text2->parent()->removeChild(m_text2, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    m_offset = m_text1->length();
    m_text1->deref();
    m_text1 = 0;

    return EditResultOK;
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

int SplitTextNodeStep::apply()
{
    int result = splitTextNode();
    if (result != EditResultOK)
        return result;
    else
        return EditStep::apply(); // skips unimplemented ModifyTextNodeStep::apply()
}

int SplitTextNodeStep::unapply()
{
    int result = joinTextNodes();
    if (result != EditResultOK)
        return result;
    else
        return EditStep::unapply(); // skips unimplemented ModifyTextNodeStep::unapply()
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

int JoinTextNodesStep::apply()
{
    int result = joinTextNodes();
    if (result != EditResultOK)
        return result;
    else
        return EditStep::apply(); // skips unimplemented ModifyTextNodeStep::apply()
}

int JoinTextNodesStep::unapply()
{
    int result = splitTextNode();
    if (result != EditResultOK)
        return result;
    else
        return EditStep::unapply(); // skips unimplemented ModifyTextNodeStep::unapply()
}

//------------------------------------------------------------------------------------------
// InsertTextStep

InsertTextStep::InsertTextStep(DocumentImpl *document, TextImpl *node, long offset, const DOMString &text)
    : EditStep(document), m_node(node), m_offset(offset)
{
    assert(m_node);
    assert(m_offset >= 0);
    assert(text.length() > 0);
    
    m_node->ref();
    m_text = text.copy(); // make a copy to ensure that the string never changes
}

InsertTextStep::~InsertTextStep()
{
    if (m_node)
        m_node->deref();
}

int InsertTextStep::apply()
{
    assert(m_node);
    assert(!m_text.isEmpty());
    assert(state() == NOT_APPLIED);

    int exceptionCode;
    m_node->insertData(m_offset, m_text, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::apply();
}

int InsertTextStep::unapply()
{
    assert(m_node);
    assert(!m_text.isEmpty());
    assert(state() == APPLIED);

    int exceptionCode;
    m_node->deleteData(m_offset, m_text.length(), exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::unapply();
}

//------------------------------------------------------------------------------------------
// DeleteTextStep

DeleteTextStep::DeleteTextStep(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditStep(document), m_node(node), m_offset(offset), m_count(count)
{
    assert(m_node);
    assert(m_offset >= 0);
    assert(m_count >= 0);
    
    m_node->ref();
}

DeleteTextStep::~DeleteTextStep()
{
    if (m_node)
        m_node->deref();
}

int DeleteTextStep::apply()
{
    assert(m_node);
    assert(state() == NOT_APPLIED);

    int exceptionCode;
    m_text = m_node->substringData(m_offset, m_count, exceptionCode);
    if (exceptionCode)
        return exceptionCode;
    
    m_node->deleteData(m_offset, m_count, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::apply();
}

int DeleteTextStep::unapply()
{
    assert(m_node);
    assert(!m_text.isEmpty());
    assert(state() == APPLIED);

    int exceptionCode;
    m_node->insertData(m_offset, m_text, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    return EditStep::unapply();
}

//------------------------------------------------------------------------------------------
// SetSelectionStep

SetSelectionStep::SetSelectionStep(DocumentImpl *document, const KHTMLSelection &selection)
    : EditStep(document)
{
    setEndingSelection(selection);
}

SetSelectionStep::SetSelectionStep(DocumentImpl *document, DOM::NodeImpl *node, long offset)
    : EditStep(document)
{
    KHTMLSelection selection(node, offset);
    setEndingSelection(selection);
}

SetSelectionStep::SetSelectionStep(DOM::DocumentImpl *document, const DOM::DOMPosition &pos)
    : EditStep(document)
{
    KHTMLSelection selection(pos);
    setEndingSelection(selection);
}

int SetSelectionStep::apply()
{
    assert(state() == NOT_APPLIED);
    return EditStep::apply();
}

int SetSelectionStep::unapply()
{
    assert(state() == APPLIED);
    return EditStep::unapply();
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
	
int DeleteSelectionStep::apply()
{
    assert(state() == NOT_APPLIED);

    if (startingSelection().isEmpty())
        return EditStep::apply();

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
    EditStep *step;
    NodeImpl *n = deleteStart.node()->nextLeafNode();

    // work on start node
    if (startIsCompletelySelected) {
        step = new RemoveNodeStep(document(), deleteStart.node());
        APPLY_STEP(step);
    }
    else if (deleteStart.node()->isTextNode()) {
        TextImpl *text = static_cast<TextImpl *>(deleteStart.node());
        int endOffset = text == deleteEnd.node() ? deleteEnd.offset() : text->length();
        if (endOffset > deleteStart.offset()) {
            step = new DeleteTextStep(document(), text, deleteStart.offset(), endOffset - deleteStart.offset());
            APPLY_STEP(step);
        }
    }
    else {
        // never should reach this code
        assert(0);
    }

    if (deleteStart.node() != deleteEnd.node()) {
        // work on intermediate nodes
        while (n != deleteEnd.node()) {
            NodeImpl *d = n;
            n = n->nextLeafNode();
            step = new RemoveNodeStep(document(), d);
            APPLY_STEP(step);
        }
        
        // work on end node
        assert(n == deleteEnd.node());
        if (endIsCompletelySelected) {
            step = new RemoveNodeStep(document(), deleteEnd.node());
            APPLY_STEP(step);
        }
        else if (deleteEnd.node()->isTextNode()) {
            if (deleteEnd.offset() > 0) {
                TextImpl *text = static_cast<TextImpl *>(deleteEnd.node());
                step = new DeleteTextStep(document(), text, 0, deleteEnd.offset());
                APPLY_STEP(step);
            }
        }
        else {
            // never should reach this code
            assert(0);
        }
    }

    //
    // set the ending selection
    //
    selection.moveTo(endingPosition);
    selection.moveToRenderedContent();
    setEndingSelection(selection);

    return CompositeEditStep::apply();
}

//------------------------------------------------------------------------------------------

#pragma mark EditCommands

//------------------------------------------------------------------------------------------
// EditCommand

static int cookieCounter = 0;

EditCommand::EditCommand(DocumentImpl *document) : m_document(document)
{
    assert(m_document);
    m_document->ref();
    m_cookie = cookieCounter++;
    m_steps.setAutoDelete(true);
}

EditCommand::~EditCommand()
{
    assert(m_document);
    m_document->deref();
}

const KHTMLSelection &EditCommand::selection() const
{
    assert(m_document);
    assert(m_document->part());
    return m_document->part()->selection();
}

int EditCommand::unapply()
{
    assert(m_steps.count() > 0);
    
    QPtrListIterator<EditStep> it(m_steps);
    for (it.toLast(); it.current(); --it) {
        int result = it.current()->unapply();
        if (result != EditResultOK)
            return result;
    }

    return EditResultOK;
}

int EditCommand::reapply()
{
    assert(m_steps.count() > 0);
    
    QPtrListIterator<EditStep> it(m_steps);
    for (; it.current(); ++it) {
        int result = it.current()->reapply();
        if (result != EditResultOK)
            return result;
    }

    return EditResultOK;
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

int InputTextCommand::apply()
{
    KHTMLPart *part = document()->part();
    assert(part);

    KHTMLSelection selection = part->selection();
    if (!selection.startNode()->isTextNode())
        return EditResultFailed;

    EditStep *step;

    // Delete the current selection
    if (selection.state() == KHTMLSelection::RANGE) {
        step = new DeleteSelectionStep(document());
        APPLY_STEP(step);
    }
    
    TextImpl *textNode = static_cast<TextImpl *>(selection.startNode());
    
    if (isLineBreak()) {
        int exceptionCode;
        ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);

        bool atStart = selection.startOffset() == textNode->renderer()->caretMinOffset();
        bool atEnd = selection.startOffset() == textNode->renderer()->caretMaxOffset();
        if (atStart) {
            // Set the cursor at the beginning of text node now following the new BR.
            step = new InsertNodeBeforeStep(document(), breakNode, textNode);
            APPLY_STEP(step);

            step = new SetSelectionStep(document(), textNode, 0);
            APPLY_STEP(step);
        }
        else if (atEnd) {
            if (textNode->parentNode()->lastChild() == textNode) {
                step = new AppendNodeStep(document(), textNode->parentNode(), breakNode);
                APPLY_STEP(step);
            }
            else {
                step = new InsertNodeBeforeStep(document(), breakNode, textNode->nextSibling());
                APPLY_STEP(step);
            }
            // Set the cursor at the beginning of the the BR.
            step = new SetSelectionStep(document(), selection.nextCharacterPosition());
            APPLY_STEP(step);
        }
        else {
            TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.startOffset(), exceptionCode));
            step = new DeleteTextStep(document(), textNode, 0, selection.startOffset());
            APPLY_STEP(step);

            step = new InsertNodeBeforeStep(document(), textBeforeNode, textNode);
            APPLY_STEP(step);

            step = new InsertNodeBeforeStep(document(), breakNode, textNode);
            APPLY_STEP(step);

            textBeforeNode->deref();
            // Set the cursor at the beginning of the node after the BR.
            step = new SetSelectionStep(document(), textNode, 0);
            APPLY_STEP(step);
        }
        
        breakNode->deref();
    }
    else {
        step = new InsertTextStep(document(), textNode, selection.startOffset(), text());
        APPLY_STEP(step);

        step = new SetSelectionStep(document(), selection.startNode(), selection.startOffset() + text().length());
        APPLY_STEP(step);
    }

    return EditResultOK;
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

DeleteTextCommand::DeleteTextCommand(DocumentImpl *document) 
    : EditCommand(document)
{
}

int DeleteTextCommand::apply()
{
    KHTMLPart *part = document()->part();
    assert(part);

    KHTMLSelection selection = part->selection();
    EditStep *step;

    // Delete the current selection
    if (selection.state() == KHTMLSelection::RANGE) {
        step = new DeleteSelectionStep(document());
        APPLY_STEP(step);
        return EditResultOK;
    }

    if (!selection.startNode())
        return EditResultFailed;

    NodeImpl *caretNode = selection.startNode();

    if (caretNode->isTextNode()) {
        int exceptionCode;

        // Check if we can delete character at cursor
        int offset = selection.startOffset() - 1;
        if (offset >= 0) {
            TextImpl *textNode = static_cast<TextImpl *>(caretNode);
            textNode->deleteData(offset, 1, exceptionCode);
            selection.moveTo(textNode, offset);
            return true;
        }
        
        // Check if previous sibling is a BR element
        NodeImpl *previousSibling = caretNode->previousSibling();
        if (previousSibling->renderer() && previousSibling->renderer()->isBR()) {
            caretNode->parentNode()->removeChild(previousSibling, exceptionCode);
            // EDIT FIXME: adjust selection position
            return true;
        }
        
        // Check if previous leaf node is a text node
        NodeImpl *previousLeafNode = caretNode->previousLeafNode();
        if (previousLeafNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(previousLeafNode);
            offset = previousLeafNode->caretMaxOffset() - 1;
            textNode->deleteData(offset, 1, exceptionCode);
            selection.moveTo(textNode, offset);
            return true;
        }
    }

    return false;
}
