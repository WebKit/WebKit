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
#include "rendering/render_object.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_textimpl.h"

using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;

using khtml::DeleteTextCommand;
using khtml::EditCommand;
using khtml::EditCommandID;
using khtml::InputTextCommand;

//------------------------------------------------------------------------------------------
// EditCommand

EditCommand::EditCommand(DOM::DocumentImpl *document) : m_document(0)
{
    m_document = document;
    if (m_document) {
        m_document->ref();
    }
}

EditCommand::~EditCommand()
{
    if (m_document)
        m_document->deref();
}

void EditCommand::deleteSelection()
{
    if (!m_document || !m_document->view() || !m_document->view()->part())
        return;
    
    KHTMLSelection &selection = m_document->view()->part()->getKHTMLSelection();
    Range range(selection.startNode(), selection.startOffset(), selection.endNode(), selection.endOffset());
    range.deleteContents();
    selection.setSelection(selection.startNode(), selection.startOffset());
    m_document->clearSelection();
}

void EditCommand::pruneEmptyNodes() const
{
    return;

#if 0
    KHTMLView *view = document()->view();
    if (!view)
        return;

    KHTMLPart *part = view->part();
    if (!part)
        return;

    KHTMLSelection &selection = part->getKHTMLSelection();
    
    bool prunedNodes = false;
    NodeImpl *node = selection.startNode();
    while (1) {
        if (node->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(node);
            if (textNode->length() == 0) {
                node = textNode->traversePreviousNode();
                removeNode(textNode);
                prunedNodes = true;
            }
            else {
                break;
            }
        }
        else if (!node->hasChildNodes()) {
            NodeImpl *n = node;
            node = node->traversePreviousNode();
            removeNode(n);
            prunedNodes = true;
        }
        else {
            break;
        }
    }
    
    if (prunedNodes) {
        selection.setSelection(node, node->caretMaxOffset());
    }
#endif
}

void EditCommand::removeNode(DOM::NodeImpl *node) const
{
    if (!node)
        return;
    
    int exceptionCode;
    node->remove(exceptionCode);
}

//------------------------------------------------------------------------------------------
// InputTextCommand

EditCommandID InputTextCommand::commandID() const { return InputTextCommandID; }

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

bool InputTextCommand::apply()
{
    KHTMLView *view = document()->view();
    if (!view)
        return false;

    KHTMLPart *part = view->part();
    if (!part)
        return false;

    KHTMLSelection &selection = part->getKHTMLSelection();
    if (!selection.startNode()->isTextNode())
        return false;

    // Delete the current selection
    if (selection.state() == KHTMLSelection::RANGE) {
        deleteSelection();
        // EDIT FIXME: adjust selection position
    }
    
    TextImpl *textNode = static_cast<TextImpl *>(selection.startNode());
    int exceptionCode;
    
    if (isLineBreak()) {
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.startOffset(), exceptionCode));
        textNode->deleteData(0, selection.startOffset(), exceptionCode);
        ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
        textNode->parentNode()->insertBefore(textBeforeNode, textNode, exceptionCode);
        textNode->parentNode()->insertBefore(breakNode, textNode, exceptionCode);
        textBeforeNode->deref();
        breakNode->deref();
        
        // Set the cursor at the beginning of the node after the split.
        selection.setSelection(textNode, 0);
    }
    else {
        textNode->insertData(selection.startOffset(), text(), exceptionCode);
        // EDIT FIXME: this is a hack for now
        // advance the cursor
        int textLength = text().length();
        selection.setSelection(selection.startNode(), selection.startOffset() + textLength);
    }

    return true;
}

bool InputTextCommand::canUndo() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

EditCommandID DeleteTextCommand::commandID() const { return DeleteTextCommandID; }

DeleteTextCommand::DeleteTextCommand(DocumentImpl *document) 
    : EditCommand(document)
{
}

bool DeleteTextCommand::apply()
{
    KHTMLView *view = document()->view();
    if (!view)
        return false;

    KHTMLPart *part = view->part();
    if (!part)
        return false;

    KHTMLSelection &selection = part->getKHTMLSelection();

    // Delete the current selection
    if (selection.state() == KHTMLSelection::RANGE) {
        deleteSelection();
        // EDIT FIXME: adjust selection position
        return true;
    }

    if (!selection.startNode())
        return false;

    NodeImpl *caretNode = selection.startNode();

    if (caretNode->isTextNode()) {
        int exceptionCode;

        // Check if we can delete character at cursor
        int offset = selection.startOffset() - 1;
        if (offset >= 0) {
            TextImpl *textNode = static_cast<TextImpl *>(caretNode);
            textNode->deleteData(offset, 1, exceptionCode);
            selection.setSelection(textNode, offset);
            pruneEmptyNodes();
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
            selection.setSelection(textNode, offset);
            pruneEmptyNodes();
            return true;
        }
    }

    return false;
}

bool DeleteTextCommand::canUndo() const
{
    return true;
}

