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

#include "dom_position.h"

#include "htmltags.h"
#include "rendering/render_line.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "xml/dom_edititerator.h"
#include "xml/dom_nodeimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#endif

using DOM::DOMPosition;
using DOM::EditIterator;
using DOM::NodeImpl;
using khtml::InlineBox;
using khtml::InlineFlowBox;
using khtml::InlineTextBox;
using khtml::RenderObject;
using khtml::RenderText;

#if !APPLE_CHANGES
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#endif

DOMPosition::DOMPosition(NodeImpl *node, long offset) 
    : m_node(0), m_offset(offset) 
{ 
    if (node) {
        m_node = node;
        m_node->ref();
    }
};

DOMPosition::DOMPosition(const DOMPosition &o)
    : m_node(0), m_offset(o.offset()) 
{
    if (o.node()) {
        m_node = o.node();
        m_node->ref();
    }
}

DOMPosition::~DOMPosition() {
    if (m_node) {
        m_node->deref();
    }
}

DOMPosition &DOMPosition::operator=(const DOMPosition &o)
{
    if (m_node) {
        m_node->deref();
    }
    m_node = o.node();
    if (m_node) {
        m_node->ref();
    }

    m_offset = o.offset();
    
    return *this;
}

long DOMPosition::renderedOffset() const
{
    if (!node()->isTextNode())
        return offset();
   
    if (!node()->renderer())
        return offset();
                    
    long result = 0;
    RenderText *textRenderer = static_cast<RenderText *>(node()->renderer());
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        long start = box->m_start;
        long end = box->m_start + box->m_len;
        if (offset() < start)
            return result;
        if (offset() <= end) {
            result += offset() - start;
            return result;
        }
        result += box->m_len;
    }
    return result;
}

DOMPosition DOMPosition::equivalentLeafPosition() const
{
    if (node()->hasChildNodes() == false)
        return *this;
    
    NodeImpl *n = node();
    int count = 0;
    while (1) {
        n = n->nextLeafNode();
        if (!n)
            return *this;
        if (count + n->maxOffset() >= offset()) {
            count = offset() - count;
            break;
        }
        count += n->maxOffset();
    }
    return DOMPosition(n, count);
}

DOMPosition DOMPosition::previousRenderedEditablePosition() const
{
    if (isEmpty())
        return DOMPosition();

    if (node()->isContentEditable() && node()->hasChildNodes() == false && inRenderedContent())
        return *this;

    NodeImpl *n = node();
    while (1) {
        n = n->previousEditable();
        if (!n)
            return DOMPosition();
        if (n->renderer() && n->renderer()->style()->visibility() == khtml::VISIBLE)
            break;
    }
    
    return DOMPosition(n, 0);
}

DOMPosition DOMPosition::nextRenderedEditablePosition() const
{
    if (isEmpty())
        return DOMPosition();

    if (node()->isContentEditable() && node()->hasChildNodes() == false && inRenderedContent())
        return *this;

    NodeImpl *n = node();
    while (1) {
        n = n->nextEditable();
        if (!n)
            return DOMPosition();
        if (n->renderer() && n->renderer()->style()->visibility() == khtml::VISIBLE)
            break;
    }
    
    return DOMPosition(n, 0);
}

DOMPosition DOMPosition::previousCharacterPosition() const
{
    if (isEmpty())
        return DOMPosition();

    NodeImpl *fromRootEditableBlock = node()->rootEditableBlock();
    EditIterator it(*this);

    bool atStartOfLine = isFirstRenderedPositionOnLine();
    
    while (!it.atStart()) {
        DOMPosition pos = it.previous();

        if (pos.node()->rootEditableBlock() != fromRootEditableBlock)
            return *this;

        if (atStartOfLine) {
            if (pos.inRenderedContent())
                return pos;
        }
        else if (rendersInDifferentPosition(pos))
            return pos;
    }
    
    return *this;
}

DOMPosition DOMPosition::nextCharacterPosition() const
{
    if (isEmpty())
        return DOMPosition();

    NodeImpl *fromRootEditableBlock = node()->rootEditableBlock();
    EditIterator it(*this);

    bool atEndOfLine = isLastRenderedPositionOnLine();
    
    while (!it.atEnd()) {
        DOMPosition pos = it.next();

        if (pos.node()->rootEditableBlock() != fromRootEditableBlock)
            return *this;

        if (atEndOfLine) {
            if (pos.inRenderedContent())
                return pos;
        }
        else if (rendersInDifferentPosition(pos))
            return pos;
    }
    
    return *this;
}

DOMPosition DOMPosition::equivalentUpstreamPosition() const
{
    if (!node())
        return DOMPosition();

    NodeImpl *block = node()->containingEditableBlock();
    
    EditIterator it(*this);            
    for (; !it.atStart(); it.previous()) {   
        if (block != it.current().node()->containingEditableBlock())
            return it.next();

        if (!node()->isContentEditable())
            return it.next();
            
        RenderObject *renderer = it.current().node()->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != khtml::VISIBLE)
            continue;

        if (renderer->isBlockFlow() || renderer->isReplaced() || renderer->isBR()) {
            if (it.current().offset() >= renderer->caretMaxOffset())
                return DOMPosition(it.current().node(), renderer->caretMaxOffset());
            else
                continue;
        }

        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (it.current().node() != node())
                return DOMPosition(it.current().node(), renderer->caretMaxOffset());

            if (it.current().offset() < 0)
                continue;
            uint textOffset = it.current().offset();

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset > box->start() && textOffset <= box->start() + box->len())
                    return it.current();
            }
        }
    }
    
    return it.current();
}

DOMPosition DOMPosition::equivalentDownstreamPosition() const
{
    if (!node())
        return DOMPosition();

    NodeImpl *block = node()->containingEditableBlock();
    
    EditIterator it(*this);            
    for (; !it.atEnd(); it.next()) {   
        if (block != it.current().node()->containingEditableBlock())
            return it.previous();

        if (!node()->isContentEditable())
            return it.next();
            
        RenderObject *renderer = it.current().node()->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != khtml::VISIBLE)
            continue;

        if (renderer->isBlockFlow() || renderer->isReplaced() || renderer->isBR()) {
            if (it.current().offset() <= renderer->caretMinOffset())
                return DOMPosition(it.current().node(), renderer->caretMinOffset());
            else
                continue;
        }

        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (it.current().node() != node())
                return DOMPosition(it.current().node(), renderer->caretMinOffset());

            if (it.current().offset() < 0)
                continue;
            uint textOffset = it.current().offset();

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset >= box->start() && textOffset <= box->end())
                    return it.current();
            }
        }
    }
    
    return it.current();
}

bool DOMPosition::atStartOfContainingEditableBlock() const
{
    return renderedOffset() == 0 && inFirstEditableInContainingEditableBlock();
}

bool DOMPosition::atStartOfRootEditableBlock() const
{
    return renderedOffset() == 0 && inFirstEditableInRootEditableBlock();
}

bool DOMPosition::inRenderedContent() const
{
    if (isEmpty())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer || !renderer->isEditable())
        return false;
    
    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;

    if (renderer->isBR() && static_cast<RenderText *>(renderer)->firstTextBox()) {
        return offset() == 0;
    }
    else if (renderer->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(renderer);
        for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (offset() >= box->m_start && offset() <= box->m_start + box->m_len) {
                return true;
            }
            else if (offset() < box->m_start) {
                // The offset we're looking for is before this node
                // this means the offset must be in content that is
                // not rendered. Return false.
                return false;
            }
        }
    }
    else if (offset() >= renderer->caretMinOffset() && offset() <= renderer->caretMaxOffset()) {
        // don't return containing editable blocks unless they are empty
        if (node()->containingEditableBlock() == node() && node()->firstChild())
            return false;
        return true;
    }
    
    return false;
}


static InlineBox *inlineBoxForRenderer(RenderObject *renderer, long offset)
{
    if (!renderer)
        return 0;

    if (renderer->isBR() && static_cast<RenderText *>(renderer)->firstTextBox())
        return static_cast<RenderText *>(renderer)->firstTextBox();
    
    if (renderer->isText()) {
        RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer); 
        if (textRenderer->isBR() && textRenderer->firstTextBox())
            return textRenderer->firstTextBox();
        
        for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (offset >= box->m_start && offset <= box->m_start + box->m_len) {
                return box;
            }
            else if (offset < box->m_start) {
                // The offset we're looking for is before this node
                // this means the offset must be in content that is
                // not rendered.
                return box->prevTextBox() ? box->prevTextBox() : textRenderer->firstTextBox();
            }
        }
    }
    else {
        return renderer->inlineBoxWrapper();
    } 
    
    return 0;
}

static bool renderersOnDifferentLine(RenderObject *r1, long o1, RenderObject *r2, long o2)
{
    InlineBox *b1 = inlineBoxForRenderer(r1, o1);
    InlineBox *b2 = inlineBoxForRenderer(r2, o2);

    if (b1 && b2 && b1->root() != b2->root())
        return true;
    
    return false;
}

static NodeImpl *nextRenderedEditable(NodeImpl *node)
{
    while (1) {
        node = node->nextEditable();
        if (!node)
            return 0;
        if (inlineBoxForRenderer(node->renderer(), 0))
            return node;
    }
    return 0;
}

static NodeImpl *previousRenderedEditable(NodeImpl *node)
{
    while (1) {
        node = node->previousEditable();
        if (!node)
            return 0;
        if (inlineBoxForRenderer(node->renderer(), 0))
            return node;
    }
    return 0;
}

bool DOMPosition::inRenderedText() const
{
    if (!node()->isTextNode())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderText *textRenderer = static_cast<RenderText *>(renderer);
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (offset() < box->m_start) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (offset() >= box->m_start && offset() <= box->m_start + box->m_len)
            return true;
    }
    
    return false;
}

bool DOMPosition::rendersOnSameLine(const DOMPosition &pos) const
{
    if (isEmpty() || pos.isEmpty())
        return false;

    if (node() == pos.node() && offset() == pos.offset())
        return true;

    if (node()->containingEditableBlock() != pos.node()->containingEditableBlock())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderObject *posRenderer = pos.node()->renderer();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE ||
        posRenderer->style()->visibility() != khtml::VISIBLE)
        return false;

    return renderersOnDifferentLine(renderer, offset(), posRenderer, pos.offset());
}

bool DOMPosition::rendersInDifferentPosition(const DOMPosition &pos) const
{
    if (isEmpty() || pos.isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderObject *posRenderer = pos.node()->renderer();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE ||
        posRenderer->style()->visibility() != khtml::VISIBLE)
        return false;
    
    if (node() == pos.node()) {
        if (node()->id() == ID_BR)
            return false;

        if (offset() == pos.offset())
            return false;
            
        if (!node()->isTextNode() && !pos.node()->isTextNode()) {
            if (offset() != pos.offset())
                return true;
        }
    }
    
    if (node()->id() == ID_BR && pos.inRenderedContent())
        return true;
                
    if (pos.node()->id() == ID_BR && inRenderedContent())
        return true;
                
    if (node()->containingEditableBlock() != pos.node()->containingEditableBlock())
        return true;

    if (node()->isTextNode() && !inRenderedText())
        return false;

    if (pos.node()->isTextNode() && !pos.inRenderedText())
        return false;

    long thisRenderedOffset = renderedOffset();
    long posRenderedOffset = pos.renderedOffset();

    if (renderer == posRenderer && thisRenderedOffset == posRenderedOffset)
        return false;

    LOG(Editing, "onDifferentLine:        %s\n", renderersOnDifferentLine(renderer, offset(), posRenderer, pos.offset()) ? "YES" : "NO");
    LOG(Editing, "renderer:               %p [%p]\n", renderer, inlineBoxForRenderer(renderer, offset()));
    LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    LOG(Editing, "posRenderer:            %p [%p]\n", posRenderer, inlineBoxForRenderer(posRenderer, pos.offset()));
    LOG(Editing, "posRenderedOffset:      %d\n", posRenderedOffset);
    LOG(Editing, "node min/max:           %d:%d\n", node()->caretMinOffset(), node()->caretMaxRenderedOffset());
    LOG(Editing, "pos node min/max:       %d:%d\n", pos.node()->caretMinOffset(), pos.node()->caretMaxRenderedOffset());
    LOG(Editing, "----------------------------------------------------------------------\n");

    InlineBox *b1 = inlineBoxForRenderer(renderer, offset());
    InlineBox *b2 = inlineBoxForRenderer(posRenderer, pos.offset());

    if (!b1 || !b2) {
        return false;
    }

    if (b1->root() != b2->root()) {
        return true;
    }

    if (nextRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == (long)node()->caretMaxRenderedOffset() && posRenderedOffset == 0) {
        return false;
    }
    
    if (previousRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == 0 && posRenderedOffset == (long)pos.node()->caretMaxRenderedOffset()) {
        return false;
    }

    return true;
}

bool DOMPosition::isFirstRenderedPositionOnLine() const
{
    if (isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;
    
    DOMPosition pos(node(), offset());
    EditIterator it(pos);
    while (!it.atStart()) {
        it.previous();
        if (it.current().inRenderedContent())
            return renderersOnDifferentLine(renderer, offset(), it.current().node()->renderer(), it.current().offset());
    }
    
    return true;
}

bool DOMPosition::isLastRenderedPositionOnLine() const
{
    if (isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;
    
    if (node()->id() == ID_BR)
        return true;
    
    DOMPosition pos(node(), offset());
    EditIterator it(pos);
    while (!it.atEnd()) {
        it.next();
        if (it.current().inRenderedContent())
            return renderersOnDifferentLine(renderer, offset(), it.current().node()->renderer(), it.current().offset());
    }
    
    return true;
}

bool DOMPosition::isLastRenderedPositionInEditableBlock() const
{
    if (isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;

    if (renderedOffset() != (long)node()->caretMaxRenderedOffset())
        return false;

    DOMPosition pos(node(), offset());
    EditIterator it(pos);
    while (!it.atEnd()) {
        it.next();
        if (!it.current().node()->inSameContainingEditableBlock(node()))
            return true;
        if (it.current().inRenderedContent())
            return false;
    }
    return true;
}

bool DOMPosition::inFirstEditableInRootEditableBlock() const
{
    if (isEmpty() || !inRenderedContent())
        return false;

    EditIterator it(node(), offset());
    while (!it.atStart()) {
        if (it.previous().inRenderedContent())
            return false;
    }

    return true;
}

bool DOMPosition::inLastEditableInRootEditableBlock() const
{
    if (isEmpty() || !inRenderedContent())
        return false;

    EditIterator it(node(), offset());
    while (!it.atEnd()) {
        if (it.next().inRenderedContent())
            return false;
    }

    return true;
}

bool DOMPosition::inFirstEditableInContainingEditableBlock() const
{
    if (isEmpty() || !inRenderedContent())
        return false;
    
    NodeImpl *block = node()->containingEditableBlock();

    EditIterator it(node(), offset());
    while (!it.atStart()) {
        it.previous();
        if (!it.current().inRenderedContent())
            continue;
        return block != it.current().node()->containingEditableBlock();
    }

    return true;
}

bool DOMPosition::inLastEditableInContainingEditableBlock() const
{
    if (isEmpty() || !inRenderedContent())
        return false;
    
    NodeImpl *block = node()->containingEditableBlock();

    EditIterator it(node(), offset());
    while (!it.atEnd()) {
        it.next();
        if (!it.current().inRenderedContent())
            continue;
        return block != it.current().node()->containingEditableBlock();
    }

    return true;
}


