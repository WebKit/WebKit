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

#include "cssproperties.h"
#include "css/css_valueimpl.h"
#include "dom/css_value.h"
#include "dom/dom_position.h"
#include "html/html_elementimpl.h"
#include "html/html_imageimpl.h"
#include "htmlattrs.h"
#include "htmltags.h"
#include "khtml_part.h"
#include "khtmlview.h"
#include "qptrlist.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_positioniterator.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_selection.h"
#include "xml/dom_stringimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom2_viewsimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#endif

using DOM::AttrImpl;
using DOM::CSSPrimitiveValue;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSProperty;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSValueImpl;
using DOM::DocumentFragmentImpl;
using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::EditingTextImpl;
using DOM::PositionIterator;
using DOM::ElementImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLImageElementImpl;
using DOM::NamedAttrMapImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;
using DOM::Selection;
using DOM::TextImpl;
using DOM::TreeWalkerImpl;

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

namespace khtml {


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

static inline bool isWS(const Position &pos)
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
        
    if (node->rootEditableElement() == node)
        return false;
        
    if (renderer->isBR() || renderer->isReplaced())
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

static Position leadingWhitespacePosition(const Position &pos)
{
    ASSERT(pos.notEmpty());

    Selection selection(pos);
    Position prev = pos.previousCharacterPosition();
    if (prev != pos && prev.node()->inSameContainingBlockFlowElement(pos.node()) && prev.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(prev.node())->data();
        if (isWS(string[prev.offset()]))
            return prev;
    }

    return Position();
}

static Position trailingWhitespacePosition(const Position &pos)
{
    ASSERT(pos.notEmpty());

    if (pos.node()->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(pos.node());
        if (pos.offset() >= (long)textNode->length()) {
            Position next = pos.nextCharacterPosition();
            if (next != pos && next.node()->inSameContainingBlockFlowElement(pos.node()) && next.node()->isTextNode()) {
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

    return Position();
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

static DOMString &styleSpanClassString()
{
    static DOMString styleSpanClassString = "khtml-style-span";
    return styleSpanClassString;
}

static void debugPosition(const char *prefix, const Position &pos)
{
    LOG(Editing, "%s%s %p : %d", prefix, getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
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

void EditCommandImpl::setStartingSelection(const Selection &s)
{
    m_startingSelection = s;
    EditCommand cmd = parent();
    while (cmd.notNull()) {
        cmd.handle()->m_startingSelection = s;
        cmd = cmd.parent();
    }
}

void EditCommandImpl::setEndingSelection(const Selection &s)
{
    m_endingSelection = s;
    EditCommand cmd = parent();
    while (cmd.notNull()) {
        cmd.handle()->m_endingSelection = s;
        cmd = cmd.parent();
    }
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

    setState(NotApplied);
}

void CompositeEditCommandImpl::doReapply()
{
    if (m_cmds.count() == 0) {
        return;
    }

    for (QValueList<EditCommand>::ConstIterator it = m_cmds.begin(); it != m_cmds.end(); ++it)
        (*it)->reapply();

    setState(Applied);
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommandImpl::applyCommandToComposite(EditCommand &cmd)
{
    cmd.setStartingSelection(endingSelection());
    cmd.setEndingSelection(endingSelection());
    cmd.setParent(this);
    cmd.apply();
    m_cmds.append(cmd);
}

void CompositeEditCommandImpl::insertNodeBefore(NodeImpl *insertChild, NodeImpl *refChild)
{
    InsertNodeBeforeCommand cmd(document(), insertChild, refChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::insertNodeAfter(NodeImpl *insertChild, NodeImpl *refChild)
{
    if (refChild->parentNode()->lastChild() == refChild) {
        appendNode(refChild->parentNode(), insertChild);
    }
    else {
        ASSERT(refChild->nextSibling());
        insertNodeBefore(insertChild, refChild->nextSibling());
    }
}

void CompositeEditCommandImpl::insertNodeAt(NodeImpl *insertChild, NodeImpl *refChild, long offset)
{
    if (refChild->hasChildNodes() || (refChild->renderer() && refChild->renderer()->isBlockFlow())) {
        NodeImpl *child = refChild->firstChild();
        for (long i = 0; child && i < offset; i++)
            child = child->nextSibling();
        if (child)
            insertNodeBefore(insertChild, child);
        else
            appendNode(refChild, insertChild);
    } 
    else if (refChild->caretMinOffset() >= offset) {
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

void CompositeEditCommandImpl::appendNode(NodeImpl *parent, NodeImpl *appendChild)
{
    AppendNodeCommand cmd(document(), parent, appendChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNode(NodeImpl *removeChild)
{
    RemoveNodeCommand cmd(document(), removeChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNodeAndPrune(NodeImpl *pruneNode, NodeImpl *stopNode)
{
    RemoveNodeAndPruneCommand cmd(document(), pruneNode, stopNode);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNodePreservingChildren(NodeImpl *removeChild)
{
    RemoveNodePreservingChildrenCommand cmd(document(), removeChild);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::splitTextNode(TextImpl *text, long offset)
{
    SplitTextNodeCommand cmd(document(), text, offset);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::joinTextNodes(TextImpl *text1, TextImpl *text2)
{
    JoinTextNodesCommand cmd(document(), text1, text2);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::inputText(const DOMString &text)
{
    InputTextCommand cmd(document());
    applyCommandToComposite(cmd);
    cmd.input(text);
}

void CompositeEditCommandImpl::insertText(TextImpl *node, long offset, const DOMString &text)
{
    InsertTextCommand cmd(document(), node, offset, text);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::deleteText(TextImpl *node, long offset, long count)
{
    DeleteTextCommand cmd(document(), node, offset, count);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::replaceText(TextImpl *node, long offset, long count, const DOMString &replacementText)
{
    DeleteTextCommand deleteCommand(document(), node, offset, count);
    applyCommandToComposite(deleteCommand);
    InsertTextCommand insertCommand(document(), node, offset, replacementText);
    applyCommandToComposite(insertCommand);
}

void CompositeEditCommandImpl::deleteSelection()
{
    if (endingSelection().state() == Selection::RANGE) {
        DeleteSelectionCommand cmd(document());
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommandImpl::deleteSelection(const Selection &selection)
{
    if (selection.state() == Selection::RANGE) {
        DeleteSelectionCommand cmd(document(), selection);
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommandImpl::deleteCollapsibleWhitespace()
{
    DeleteCollapsibleWhitespaceCommand cmd(document());
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::deleteCollapsibleWhitespace(const Selection &selection)
{
    DeleteCollapsibleWhitespaceCommand cmd(document(), selection);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeCSSProperty(CSSStyleDeclarationImpl *decl, int property)
{
    RemoveCSSPropertyCommand cmd(document(), decl, property);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNodeAttribute(ElementImpl *element, int attribute)
{
    RemoveNodeAttributeCommand cmd(document(), element, attribute);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::setNodeAttribute(ElementImpl *element, int attribute, const DOMString &value)
{
    SetNodeAttributeCommand cmd(document(), element, attribute, value);
    applyCommandToComposite(cmd);
}

ElementImpl *CompositeEditCommandImpl::createTypingStyleElement() const
{
    int exceptionCode = 0;
    ElementImpl *styleElement = document()->createHTMLElement("SPAN", exceptionCode);
    ASSERT(exceptionCode == 0);
    
    styleElement->setAttribute(ATTR_STYLE, document()->part()->typingStyle()->cssText().implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);

    styleElement->setAttribute(ATTR_CLASS, styleSpanClassString());
    ASSERT(exceptionCode == 0);

    return styleElement;
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
// ApplyStyleCommandImpl

ApplyStyleCommandImpl::ApplyStyleCommandImpl(DocumentImpl *document, CSSStyleDeclarationImpl *style)
    : CompositeEditCommandImpl(document), m_style(style)
{   
    ASSERT(m_style);
    m_style->ref();
}

ApplyStyleCommandImpl::~ApplyStyleCommandImpl()
{
    ASSERT(m_style);
    m_style->deref();
}

int ApplyStyleCommandImpl::commandID() const
{
    return ApplyStyleCommandID;
}

void ApplyStyleCommandImpl::doApply()
{
    if (endingSelection().state() != Selection::RANGE)
        return;

    // adjust to the positions we want to use for applying style
    Position start(endingSelection().start().equivalentDownstreamPosition().equivalentRangeCompliantPosition());
    Position end(endingSelection().end().equivalentUpstreamPosition());

    // remove style from the selection
    removeStyle(start, end);
    bool splitStart = splitTextAtStartIfNeeded(start, end); 
    if (splitStart) {
        start = endingSelection().start();
        end = endingSelection().end();
    }
    splitTextAtEndIfNeeded(start, end);
    start = endingSelection().start();
    end = endingSelection().end();

    
    if (start.node() == end.node()) {
        // simple case...start and end are the same node
        applyStyleIfNeeded(start.node(), end.node());
    }
    else {
        NodeImpl *node = start.node();
        while (1) {
            if (node->childNodeCount() == 0 && node->renderer() && node->renderer()->isInline()) {
                NodeImpl *runStart = node;
                while (1) {
                    if (runStart->parentNode() != node->parentNode() || node->isHTMLElement() || node == end.node() || 
                        (node->renderer() && !node->renderer()->isInline())) {
                        applyStyleIfNeeded(runStart, node);
                        break;
                    }
                    node = node->traverseNextNode();
                }
            }
            if (node == end.node())
                break;
            node = node->traverseNextNode();
        }
    }
}

//------------------------------------------------------------------------------------------
// ApplyStyleCommandImpl: style-removal helpers

bool ApplyStyleCommandImpl::isHTMLStyleNode(HTMLElementImpl *elem)
{
    for (QPtrListIterator<CSSProperty> it(*(style()->values())); it.current(); ++it) {
        CSSProperty *property = it.current();
        switch (property->id()) {
            case CSS_PROP_FONT_WEIGHT:
                if (elem->id() == ID_B)
                    return true;
                break;
            case CSS_PROP_FONT_STYLE:
                if (elem->id() == ID_I)
                    return true;
                break;
        }
    }

    return false;
}

void ApplyStyleCommandImpl::removeHTMLStyleNode(HTMLElementImpl *elem)
{
    // This node can be removed.
    // EDIT FIXME: This does not handle the case where the node
    // has attributes. But how often do people add attributes to <B> tags? 
    // Not so often I think.
    ASSERT(elem);
    removeNodePreservingChildren(elem);
}

void ApplyStyleCommandImpl::removeCSSStyle(HTMLElementImpl *elem)
{
    ASSERT(elem);

    CSSStyleDeclarationImpl *decl = elem->inlineStyleDecl();
    if (!decl)
        return;

    for (QPtrListIterator<CSSProperty> it(*(style()->values())); it.current(); ++it) {
        CSSProperty *property = it.current();
        if (decl->getPropertyCSSValue(property->id()))
            removeCSSProperty(decl, property->id());
    }

    if (elem->id() == ID_SPAN) {
        // Check to see if the span is one we added to apply style.
        // If it is, and there are no more attributes on the span other than our
        // class marker, remove the span.
        NamedAttrMapImpl *map = elem->attributes();
        if (map && map->length() == 1 && elem->getAttribute(ATTR_CLASS) == styleSpanClassString())
            removeNodePreservingChildren(elem);
    }
}

void ApplyStyleCommandImpl::removeStyle(const Position &start, const Position &end)
{
    NodeImpl *node = start.node();
    while (1) {
        NodeImpl *next = node->traverseNextNode();
        if (node->isHTMLElement() && nodeFullySelected(node)) {
            HTMLElementImpl *elem = static_cast<HTMLElementImpl *>(node);
            if (isHTMLStyleNode(elem))
                removeHTMLStyleNode(elem);
            else
                removeCSSStyle(elem);
        }
        if (node == end.node())
            break;
        node = next;
    }
}

bool ApplyStyleCommandImpl::nodeFullySelected(const NodeImpl *node) const
{
    ASSERT(node);

    Position end(endingSelection().end().equivalentUpstreamPosition());
    
    if (node == end.node())
        return end.offset() >= node->caretMaxOffset();

    for (NodeImpl *child = node->lastChild(); child; child = child->lastChild()) {
        if (child == end.node())
            return end.offset() >= child->caretMaxOffset();
    }

    return !node->isAncestor(end.node());
}

//------------------------------------------------------------------------------------------
// ApplyStyleCommandImpl: style-application helpers


bool ApplyStyleCommandImpl::splitTextAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > start.node()->caretMinOffset() && start.offset() < start.node()->caretMaxOffset()) {
        long endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
        TextImpl *text = static_cast<TextImpl *>(start.node());
        SplitTextNodeCommand cmd(document(), text, start.offset());
        applyCommandToComposite(cmd);
        setEndingSelection(Selection(Position(start.node(), 0), Position(end.node(), end.offset() - endOffsetAdjustment)));
        return true;
    }
    return false;
}

NodeImpl *ApplyStyleCommandImpl::splitTextAtEndIfNeeded(const Position &start, const Position &end)
{
    if (end.node()->isTextNode() && end.offset() > end.node()->caretMinOffset() && end.offset() < end.node()->caretMaxOffset()) {
        TextImpl *text = static_cast<TextImpl *>(end.node());
        SplitTextNodeCommand cmd(document(), text, end.offset());
        applyCommandToComposite(cmd);
        NodeImpl *startNode = start.node() == end.node() ? cmd.node()->previousSibling() : start.node();
        ASSERT(startNode);
        setEndingSelection(Selection(Position(startNode, start.offset()), Position(cmd.node()->previousSibling(), cmd.node()->previousSibling()->caretMaxOffset())));
        return cmd.node()->previousSibling();
    }
    return end.node();
}

void ApplyStyleCommandImpl::surroundNodeRangeWithElement(NodeImpl *startNode, NodeImpl *endNode, ElementImpl *element)
{
    ASSERT(startNode);
    ASSERT(endNode);
    ASSERT(element);
    
    NodeImpl *node = startNode;
    while (1) {
        NodeImpl *next = node->traverseNextNode();
        if (node->childNodeCount() == 0 && node->renderer() && node->renderer()->isInline()) {
            removeNode(node);
            appendNode(element, node);
        }
        if (node == endNode)
            break;
        node = next;
    }
}

void ApplyStyleCommandImpl::applyStyleIfNeeded(DOM::NodeImpl *startNode, DOM::NodeImpl *endNode)
{
    StyleChange styleChange = computeStyleChange(Position(startNode, 0), style());
    int exceptionCode = 0;
    
    if (styleChange.cssStyle.length() > 0) {
        ElementImpl *styleElement = document()->createHTMLElement("SPAN", exceptionCode);
        ASSERT(exceptionCode == 0);
        styleElement->setAttribute(ATTR_STYLE, styleChange.cssStyle);
        styleElement->setAttribute(ATTR_CLASS, styleSpanClassString());
        insertNodeBefore(styleElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, styleElement);
    }

    if (styleChange.applyBold) {
        ElementImpl *boldElement = document()->createHTMLElement("B", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeBefore(boldElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, boldElement);
    }

    if (styleChange.applyItalic) {
        ElementImpl *italicElement = document()->createHTMLElement("I", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeBefore(italicElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, italicElement);
    }
}

bool ApplyStyleCommandImpl::currentlyHasStyle(const Position &pos, const CSSProperty *property) const
{
    ASSERT(pos.notEmpty());
    CSSStyleDeclarationImpl *decl = document()->defaultView()->getComputedStyle(pos.element(), 0);
    ASSERT(decl);
    CSSValueImpl *value = decl->getPropertyCSSValue(property->id());
    return strcasecmp(value->cssText(), property->value()->cssText()) == 0;
}

ApplyStyleCommandImpl::StyleChange ApplyStyleCommandImpl::computeStyleChange(const Position &insertionPoint, CSSStyleDeclarationImpl *style)
{
    ASSERT(insertionPoint.notEmpty());
    ASSERT(style);

    StyleChange styleChange;

    for (QPtrListIterator<CSSProperty> it(*(style->values())); it.current(); ++it) {
        CSSProperty *property = it.current();
        if (!currentlyHasStyle(insertionPoint, property)) {
            switch (property->id()) {
                case CSS_PROP_FONT_WEIGHT:
                    if (strcasecmp(property->value()->cssText(), "bold") == 0)
                        styleChange.applyBold = true;
                    else
                        styleChange.cssStyle += property->cssText();
                    break;
                case CSS_PROP_FONT_STYLE: {
                        DOMString cssText(property->value()->cssText());
                        if (strcasecmp(cssText, "italic") == 0 || strcasecmp(cssText, "oblique") == 0)
                            styleChange.applyItalic = true;
                        else
                            styleChange.cssStyle += property->cssText();
                    }
                    break;
                default:
                    styleChange.cssStyle += property->cssText();
                    break;
            }
        }
    }
    return styleChange;
}

Position ApplyStyleCommandImpl::positionInsertionPoint(Position pos)
{
    if (pos.node()->isTextNode() && (pos.offset() > 0 && pos.offset() < pos.node()->maxOffset())) {
        SplitTextNodeCommand split(document(), static_cast<TextImpl *>(pos.node()), pos.offset());
        split.apply();
        pos = Position(split.node(), 0);
    }

#if 0
    // EDIT FIXME: If modified to work with the internals of applying style,
    // this code can work to optimize cases where a style change is taking place on
    // a boundary between nodes where one of the nodes has the desired style. In other
    // words, it is possible for content to be merged into existing nodes rather than adding
    // additional markup.
    if (currentlyHasStyle(pos))
        return pos;
        
    // try next node
    if (pos.offset() >= pos.node()->caretMaxOffset()) {
        NodeImpl *nextNode = pos.node()->traverseNextNode();
        if (nextNode) {
            Position next = Position(nextNode, 0);
            if (currentlyHasStyle(next))
                return next;
        }
    }

    // try previous node
    if (pos.offset() <= pos.node()->caretMinOffset()) {
        NodeImpl *prevNode = pos.node()->traversePreviousNode();
        if (prevNode) {
            Position prev = Position(prevNode, prevNode->maxOffset());
            if (currentlyHasStyle(prev))
                return prev;
        }
    }
#endif
    
    return pos;
}

//------------------------------------------------------------------------------------------
// DeleteCollapsibleWhitespaceCommandImpl

DeleteCollapsibleWhitespaceCommandImpl::DeleteCollapsibleWhitespaceCommandImpl(DocumentImpl *document)
    : CompositeEditCommandImpl(document), m_charactersDeleted(0), m_hasSelectionToCollapse(false)
{
}

DeleteCollapsibleWhitespaceCommandImpl::DeleteCollapsibleWhitespaceCommandImpl(DocumentImpl *document, const Selection &selection)
    : CompositeEditCommandImpl(document), m_charactersDeleted(0), m_selectionToCollapse(selection), m_hasSelectionToCollapse(true)
{
}

DeleteCollapsibleWhitespaceCommandImpl::~DeleteCollapsibleWhitespaceCommandImpl()
{
}

int DeleteCollapsibleWhitespaceCommandImpl::commandID() const
{
    return DeleteCollapsibleWhitespaceCommandID;
}

static bool shouldDeleteUpstreamPosition(const Position &pos)
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

    if (pos.isFirstRenderedPositionOnLine() || pos.isLastRenderedPositionOnLine())
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

Position DeleteCollapsibleWhitespaceCommandImpl::deleteWhitespace(const Position &pos)
{
    Position upstream = pos.equivalentUpstreamPosition();
    Position downstream = pos.equivalentDownstreamPosition();
    
    bool del = shouldDeleteUpstreamPosition(upstream);

    LOG(Editing, "pos:        %s [%p:%d]\n", getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
    if (upstream == downstream) {
        LOG(Editing, "same:       %s [%p:%d]\n", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
    }
    else {
        LOG(Editing, "upstream:   %s %s [%p:%d]\n", del ? "DELETE" : "SKIP", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
        PositionIterator it(upstream);
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
        
    PositionIterator it(upstream);
    Position deleteStart = upstream;
    if (!del) {
        deleteStart = it.peekNext();
        if (deleteStart == downstream)
            return upstream;
    }
    
    Position endingPosition = upstream;
    
    while (it.current() != downstream) {

        Position next = it.peekNext();
        if (next.node() != deleteStart.node()) {
            ASSERT(deleteStart.node()->isTextNode());
            TextImpl *textNode = static_cast<TextImpl *>(deleteStart.node());
            unsigned long count = it.current().offset() - deleteStart.offset();
            if (count == textNode->length()) {
                LOG(Editing, "   removeNodeAndPrune 1: [%p]\n", textNode);
                if (textNode == endingPosition.node())
                    endingPosition = Position(next.node(), next.node()->caretMinOffset());
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
                endingPosition = Position(downstream.node(), downstream.offset() - m_charactersDeleted);
            }
        }
        
        it.setPosition(next);
    }
    
    return endingPosition;
}

void DeleteCollapsibleWhitespaceCommandImpl::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToCollapse)
        m_selectionToCollapse = endingSelection();
    int state = m_selectionToCollapse.state();
    if (state == Selection::CARET) {
        Position endPosition = deleteWhitespace(m_selectionToCollapse.start());
        setEndingSelection(endPosition);
        LOG(Editing, "-----------------------------------------------------\n");
    }
    else if (state == Selection::RANGE) {
        Position startPosition = deleteWhitespace(m_selectionToCollapse.start());
        LOG(Editing, "-----------------------------------------------------\n");
        Position endPosition = m_selectionToCollapse.end();
        if (m_charactersDeleted > 0 && startPosition.node() == endPosition.node()) {
            LOG(Editing, "adjust end position by %d\n", m_charactersDeleted);
            endPosition = Position(endPosition.node(), endPosition.offset() - m_charactersDeleted);
        }
        endPosition = deleteWhitespace(endPosition);
        setEndingSelection(Selection(startPosition, endPosition));
        LOG(Editing, "=====================================================\n");
    }
}

//------------------------------------------------------------------------------------------
// DeleteSelectionCommandImpl

DeleteSelectionCommandImpl::DeleteSelectionCommandImpl(DocumentImpl *document)
    : CompositeEditCommandImpl(document), m_hasSelectionToDelete(false)
{
}

DeleteSelectionCommandImpl::DeleteSelectionCommandImpl(DocumentImpl *document, const Selection &selection)
    : CompositeEditCommandImpl(document), m_selectionToDelete(selection), m_hasSelectionToDelete(true)
{
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
    Selection selection = endingSelection();

    if (selection.state() != Selection::CARET)
        return;

    Position pos(selection.start());
    
    if (!pos.node()->isTextNode())
        return;

    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    
    if (pos.offset() == 0) {
        PositionIterator it(pos);
        Position prev = it.previous();
        if (prev == pos)
            return;
        if (prev.node()->isTextNode()) {
            TextImpl *prevTextNode = static_cast<TextImpl *>(prev.node());
            if (textNodesAreJoinable(prevTextNode, textNode)) {
                joinTextNodes(prevTextNode, textNode);
                setEndingSelection(Position(textNode, prevTextNode->length()));
                LOG(Editing, "joinTextNodesWithSameStyle [1]\n");
            }
        }
    }
    else if (pos.offset() == (long)textNode->length()) {
        PositionIterator it(pos);
        Position next = it.next();
        if (next == pos)
            return;
        if (next.node()->isTextNode()) {
            TextImpl *nextTextNode = static_cast<TextImpl *>(next.node());
            if (textNodesAreJoinable(textNode, nextTextNode)) {
                joinTextNodes(textNode, nextTextNode);
                setEndingSelection(Position(nextTextNode, pos.offset()));
                LOG(Editing, "joinTextNodesWithSameStyle [2]\n");
            }
        }
    }
}

bool DeleteSelectionCommandImpl::containsOnlyWhitespace(const Position &start, const Position &end)
{
    // Returns whether the range contains only whitespace characters.
    // This is inclusive of the start, but not of the end.
    PositionIterator it(start);
    while (!it.atEnd()) {
        if (!it.current().node()->isTextNode())
            return false;
        const DOMString &text = static_cast<TextImpl *>(it.current().node())->data();
        // EDIT FIXME: signed/unsigned mismatch
        if (text.length() > INT_MAX)
            return false;
        if (it.current().offset() < (int)text.length() && !isWS(text[it.current().offset()]))
            return false;
        it.next();
        if (it.current() == end)
            break;
    }
    return true;
}

void DeleteSelectionCommandImpl::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();
        
    if (m_selectionToDelete.state() != Selection::RANGE)
        return;

    deleteCollapsibleWhitespace(m_selectionToDelete);
    Selection selection = endingSelection();

    Position upstreamStart(selection.start().equivalentUpstreamPosition());
    Position downstreamStart(selection.start().equivalentDownstreamPosition());
    Position upstreamEnd(selection.end().equivalentUpstreamPosition());
    Position downstreamEnd(selection.end().equivalentDownstreamPosition());

    if (upstreamStart == downstreamEnd)
        // after collapsing whitespace, selection is empty...no work to do
        return;

    Position endingPosition;
    bool adjustEndingPositionDownstream = false;

    bool onlyWhitespace = containsOnlyWhitespace(upstreamStart, downstreamEnd);
 
    bool startCompletelySelected = !onlyWhitespace &&
        (downstreamStart.offset() <= downstreamStart.node()->caretMinOffset() &&
        ((downstreamStart.node() != upstreamEnd.node()) ||
         (upstreamEnd.offset() >= upstreamEnd.node()->caretMaxOffset())));

    bool endCompletelySelected = !onlyWhitespace &&
        (upstreamEnd.offset() >= upstreamEnd.node()->caretMaxOffset() &&
        ((downstreamStart.node() != upstreamEnd.node()) ||
         (downstreamStart.offset() <= downstreamStart.node()->caretMinOffset())));

    unsigned long startRenderedOffset = downstreamStart.renderedOffset();
    
    bool startAtStartOfRootEditableElement = startRenderedOffset == 0 && downstreamStart.inFirstEditableInRootEditableElement();
    bool startAtStartOfBlock = startAtStartOfRootEditableElement || 
        (startRenderedOffset == 0 && downstreamStart.inFirstEditableInContainingEditableBlock());
    bool endAtEndOfBlock = downstreamEnd.isLastRenderedPositionInEditableBlock();

    NodeImpl *startBlock = upstreamStart.node()->enclosingBlockFlowElement();
    NodeImpl *endBlock = downstreamEnd.node()->enclosingBlockFlowElement();
    bool startBlockEndBlockAreSiblings = startBlock->parentNode() == endBlock->parentNode();

    debugPosition("upstreamStart:       ", upstreamStart);
    debugPosition("downstreamStart:     ", downstreamStart);
    debugPosition("upstreamEnd:         ", upstreamEnd);
    debugPosition("downstreamEnd:       ", downstreamEnd);
    LOG(Editing,  "start selected:      %s", startCompletelySelected ? "YES" : "NO");
    LOG(Editing,  "at start block:      %s", startAtStartOfBlock ? "YES" : "NO");
    LOG(Editing,  "at start root block: %s", startAtStartOfRootEditableElement ? "YES" : "NO");
    LOG(Editing,  "at end block:        %s", endAtEndOfBlock ? "YES" : "NO");
    LOG(Editing,  "only whitespace:     %s", onlyWhitespace ? "YES" : "NO");

    // Determine where to put the caret after the deletion
    if (startAtStartOfBlock) {
        LOG(Editing,  "ending position case 1");
        endingPosition = Position(startBlock, 0);
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
        if (upstreamStart.node()->id() == ID_BR && upstreamStart.offset() == 1)
            adjustEndingPositionDownstream = true;
    }
   
    //
    // Figure out the whitespace conversions to do
    //
    if ((startAtStartOfBlock && !endAtEndOfBlock) || (!startCompletelySelected && adjustEndingPositionDownstream)) {
        // convert trailing whitespace
        Position trailing = trailingWhitespacePosition(downstreamEnd.equivalentDownstreamPosition());
        if (trailing.notEmpty()) {
            debugPosition("convertTrailingWhitespace: ", trailing);
            Position collapse = trailing.nextCharacterPosition();
            if (collapse != trailing)
                deleteCollapsibleWhitespace(collapse);
            TextImpl *textNode = static_cast<TextImpl *>(trailing.node());
            replaceText(textNode, trailing.offset(), 1, nonBreakingSpaceString());
        }
    }
    else if (!startAtStartOfBlock && endAtEndOfBlock) {
        // convert leading whitespace
        Position leading = leadingWhitespacePosition(upstreamStart.equivalentUpstreamPosition());
        if (leading.notEmpty()) {
            debugPosition("convertLeadingWhitespace:  ", leading);
            TextImpl *textNode = static_cast<TextImpl *>(leading.node());
            replaceText(textNode, leading.offset(), 1, nonBreakingSpaceString());
        }
    }
    else if (!startAtStartOfBlock && !endAtEndOfBlock) {
        // convert contiguous whitespace
        Position leading = leadingWhitespacePosition(upstreamStart.equivalentUpstreamPosition());
        Position trailing = trailingWhitespacePosition(downstreamEnd.equivalentDownstreamPosition());
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
        LOG(Editing,  "start node delete case 1");
        removeNodeAndPrune(downstreamStart.node(), startBlock);
    }
    else if (onlyWhitespace) {
        // Selection only contains whitespace. This is really a special-case to 
        // handle significant whitespace that is collapsed at the end of a line,
        // but also handles deleting a space in mid-line.
        LOG(Editing,  "start node delete case 2");
        ASSERT(upstreamStart.node()->isTextNode());
        TextImpl *text = static_cast<TextImpl *>(upstreamStart.node());
        int offset = upstreamStart.offset();
        // EDIT FIXME: Signed/unsigned mismatch
        int length = text->length();
        if (length == upstreamStart.offset())
            offset--;
        deleteText(text, offset, 1);
    }
    else if (downstreamStart.node()->isTextNode()) {
        LOG(Editing,  "start node delete case 3");
        TextImpl *text = static_cast<TextImpl *>(downstreamStart.node());
        int endOffset = text == upstreamEnd.node() ? upstreamEnd.offset() : text->length();
        if (endOffset > downstreamStart.offset()) {
            deleteText(text, downstreamStart.offset(), endOffset - downstreamStart.offset());
        }
    }
    else {
        // we have clipped the end of a non-text element
        // the offset must be 1 here. if it is, do nothing and move on.
        LOG(Editing,  "start node delete case 4");
        ASSERT(downstreamStart.offset() == 1);
    }

    if (!onlyWhitespace && downstreamStart.node() != upstreamEnd.node()) {
        // work on intermediate nodes
        while (n != upstreamEnd.node()) {
            NodeImpl *d = n;
            n = n->traverseNextNode();
            if (d->renderer() && d->renderer()->isEditable())
                removeNodeAndPrune(d, startBlock);
        }
        
        // work on end node
        ASSERT(n == upstreamEnd.node());
        if (endCompletelySelected) {
            removeNodeAndPrune(upstreamEnd.node(), startBlock);
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

    // Do block merge if start and end of selection are in different blocks
    // and the blocks are siblings. This is a first cut at this rule arrived
    // at by doing a bunch of edits and settling on the behavior that made
    // the most sense. This could change in the future as we get more
    // experience with how this should behave.
    if (startBlock != endBlock && startBlockEndBlockAreSiblings) {
        LOG(Editing,  "merging content to start block");
        NodeImpl *node = endBlock->firstChild();
        while (node) {
            NodeImpl *moveNode = node;
            node = node->nextSibling();
            removeNode(moveNode);
            appendNode(startBlock, moveNode);
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

void InputNewlineCommandImpl::insertNodeAfterPosition(NodeImpl *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *after* the block.
    Position upstream(pos.equivalentUpstreamPosition());
    NodeImpl *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(cb, node);
    else
        insertNodeAfter(node, pos.node());
}

void InputNewlineCommandImpl::insertNodeBeforePosition(NodeImpl *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *before* the block.
    Position upstream(pos.equivalentUpstreamPosition());
    NodeImpl *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(cb, node);
    else
        insertNodeBefore(node, pos.node());
}

void InputNewlineCommandImpl::doApply()
{
    deleteSelection();
    Selection selection = endingSelection();

    int exceptionCode = 0;
    ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
    ASSERT(exceptionCode == 0);

    NodeImpl *nodeToInsert = breakNode;
    
    // Handle the case where there is a typing style.
    if (document()->part()->typingStyle()) {
        int exceptionCode = 0;
        ElementImpl *styleElement = createTypingStyleElement();
        styleElement->appendChild(breakNode, exceptionCode);
        ASSERT(exceptionCode == 0);
        nodeToInsert = styleElement;
    }
    
    Position pos(selection.start().equivalentDownstreamPosition());
    bool atStart = pos.offset() <= pos.node()->caretMinOffset();
    bool atEndOfBlock = pos.isLastRenderedPositionInEditableBlock();
    
    if (atEndOfBlock) {
        LOG(Editing, "input newline case 1");
        // Insert an "extra" BR at the end of the block. This makes the "real" BR we want
        // to insert appear in the rendering without any significant side effects (and no
        // real worries either since you can't arrow past this extra one.
        insertNodeAfterPosition(nodeToInsert, pos);
        exceptionCode = 0;
        ElementImpl *extraBreakNode = document()->createHTMLElement("BR", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeAfter(extraBreakNode, nodeToInsert);
        setEndingSelection(Position(extraBreakNode, 0));
    }
    else if (atStart) {
        LOG(Editing, "input newline case 2");
        // Insert node, but place the caret into index 0 of the downstream
        // position. This will make the caret appear after the break, and as we know
        // there is content at that location, this is OK.
        insertNodeBeforePosition(nodeToInsert, pos);
        setEndingSelection(Position(pos.node(), 0));
    }
    else {
        // Split a text node
        LOG(Editing, "input newline case 3");
        ASSERT(pos.node()->isTextNode());
        TextImpl *textNode = static_cast<TextImpl *>(pos.node());
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.start().offset(), exceptionCode));
        deleteText(textNode, 0, selection.start().offset());
        insertNodeBefore(textBeforeNode, textNode);
        insertNodeBefore(nodeToInsert, textNode);
        setEndingSelection(Position(textNode, 0));
    }
}

//------------------------------------------------------------------------------------------
// InputTextCommandImpl

InputTextCommandImpl::InputTextCommandImpl(DocumentImpl *document) 
    : CompositeEditCommandImpl(document), m_charactersAdded(0)
{
}

InputTextCommandImpl::~InputTextCommandImpl() 
{
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

    Selection selection = endingSelection();

    if (!selection.start().node()->isTextNode())
        return;

    int exceptionCode = 0;
    int offset = selection.start().offset() - 1;
    if (offset >= selection.start().node()->caretMinOffset()) {
        TextImpl *textNode = static_cast<TextImpl *>(selection.start().node());
        textNode->deleteData(offset, 1, exceptionCode);
        ASSERT(exceptionCode == 0);
        selection = Selection(Position(textNode, offset));
        setEndingSelection(selection);
        m_charactersAdded--;
    }
}

Position InputTextCommandImpl::prepareForTextInsertion(bool adjustDownstream)
{
    // Prepare for text input by looking at the current position.
    // It may be necessary to insert a text node to receive characters.
    Selection selection = endingSelection();
    ASSERT(selection.state() == Selection::CARET);
    
    Position pos = selection.start();
    if (adjustDownstream)
        pos = pos.equivalentDownstreamPosition();
    else
        pos = pos.equivalentUpstreamPosition();
    
    if (!pos.node()->isTextNode()) {
        NodeImpl *textNode = document()->createEditingTextNode("");
        NodeImpl *nodeToInsert = textNode;
        if (document()->part()->typingStyle()) {
            int exceptionCode = 0;
            ElementImpl *styleElement = createTypingStyleElement();
            styleElement->appendChild(textNode, exceptionCode);
            ASSERT(exceptionCode == 0);
            nodeToInsert = styleElement;
        }
        
        // Now insert the node in the right place
        if (pos.node()->isEditableBlock()) {
            LOG(Editing, "prepareForTextInsertion case 1");
            appendNode(pos.node(), nodeToInsert);
        }
        else if (pos.node()->id() == ID_BR && pos.offset() == 1) {
            LOG(Editing, "prepareForTextInsertion case 2");
            insertNodeAfter(nodeToInsert, pos.node());
        }
        else if (pos.node()->caretMinOffset() == pos.offset()) {
            LOG(Editing, "prepareForTextInsertion case 3");
            insertNodeBefore(nodeToInsert, pos.node());
        }
        else if (pos.node()->caretMaxOffset() == pos.offset()) {
            LOG(Editing, "prepareForTextInsertion case 4");
            insertNodeAfter(nodeToInsert, pos.node());
        }
        else
            ASSERT_NOT_REACHED();
        
        pos = Position(textNode, 0);
    }
    else {
        // Handle the case where there is a typing style.
        if (document()->part()->typingStyle()) {
            if (pos.node()->isTextNode() && pos.offset() > pos.node()->caretMinOffset() && pos.offset() < pos.node()->caretMaxOffset()) {
                // Need to split current text node in order to insert a span.
                TextImpl *text = static_cast<TextImpl *>(pos.node());
                SplitTextNodeCommand cmd(document(), text, pos.offset());
                applyCommandToComposite(cmd);
                setEndingSelection(Position(cmd.node(), 0));
            }
            
            int exceptionCode = 0;
            TextImpl *editingTextNode = document()->createEditingTextNode("");

            ElementImpl *styleElement = createTypingStyleElement();
            styleElement->appendChild(editingTextNode, exceptionCode);
            ASSERT(exceptionCode == 0);

            NodeImpl *node = endingSelection().start().node();
            if (endingSelection().start().isLastRenderedPositionOnLine())
                insertNodeAfter(styleElement, node);
            else
                insertNodeBefore(styleElement, node);
            pos = Position(editingTextNode, 0);
        }
    }
    return pos;
}

void InputTextCommandImpl::execute(const DOMString &text)
{
    Selection selection = endingSelection();
    bool adjustDownstream = selection.start().isFirstRenderedPositionOnLine();

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.state() == Selection::RANGE)
        deleteSelection();
    else
        deleteCollapsibleWhitespace();

    // EDIT FIXME: Need to take typing style from upstream text, if any.
    
    // Make sure the document is set up to receive text
    Position pos = prepareForTextInsertion(adjustDownstream);
    
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    long offset = pos.offset();
    
    // This is a temporary implementation for inserting adjoining spaces
    // into a document. We are working on a CSS-related whitespace solution
    // that will replace this some day.
    if (isWS(text))
        insertSpace(textNode, offset);
    else {
        const DOMString &existingText = textNode->data();
        if (textNode->length() >= 2 && offset >= 2 && isNBSP(existingText[offset - 1]) && !isWS(existingText[offset - 2])) {
            // DOM looks like this:
            // character nbsp caret
            // As we are about to insert a non-whitespace character at the caret
            // convert the nbsp to a regular space.
            // EDIT FIXME: This needs to be improved some day to convert back only
            // those nbsp's added by the editor to make rendering come out right.
            replaceText(textNode, offset - 1, 1, " ");
        }
        insertText(textNode, offset, text);
    }
    setEndingSelection(Position(textNode, offset + text.length()));
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
        Position pos(textNode, offset);
        Position downstream = pos.equivalentDownstreamPosition();
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
    ASSERT(m_refChild->parentNode());

    int exceptionCode = 0;
    m_refChild->parentNode()->insertBefore(m_insertChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertNodeBeforeCommandImpl::doUnapply()
{
    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parentNode());

    int exceptionCode = 0;
    m_refChild->parentNode()->removeChild(m_insertChild, exceptionCode);
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

    m_text2->parentNode()->removeChild(m_text1, exceptionCode);
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
// ReplaceSelectionCommandImpl

ReplaceSelectionCommandImpl::ReplaceSelectionCommandImpl(DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement) 
    : CompositeEditCommandImpl(document), m_fragment(fragment), m_selectReplacement(selectReplacement)
{
}

ReplaceSelectionCommandImpl::~ReplaceSelectionCommandImpl()
{
}

int ReplaceSelectionCommandImpl::commandID() const
{
    return ReplaceSelectionCommandID;
}

void ReplaceSelectionCommandImpl::doApply()
{
    NodeImpl *firstChild = m_fragment->firstChild();
    NodeImpl *lastChild = m_fragment->lastChild();

    Selection selection = endingSelection();

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.state() == Selection::RANGE)
        deleteSelection();
    else
        deleteCollapsibleWhitespace();
    
    selection = endingSelection();
    ASSERT(!selection.isEmpty());
    
    if (!firstChild) {
        // Pasting something that didn't parse or was empty.
        ASSERT(!lastChild);
    } else if (firstChild == lastChild && firstChild->isTextNode()) {
        // Simple text paste. Treat as if the text were typed.
        Position base = selection.base();
        inputText(static_cast<TextImpl *>(firstChild)->data());
        if (m_selectReplacement) {
            setEndingSelection(Selection(base, endingSelection().extent()));
        }
    } 
    else {
        // HTML fragment paste.
        NodeImpl *beforeNode = firstChild;
        NodeImpl *node = firstChild->nextSibling();

        insertNodeAt(firstChild, selection.start().node(), selection.start().offset());
        
        // Insert the nodes from the fragment
        while (node) {
            NodeImpl *next = node->nextSibling();
            insertNodeAfter(node, beforeNode);
            beforeNode = node;
            node = next;
        }
        ASSERT(beforeNode);
	
        // Find the last leaf.
        NodeImpl *lastLeaf = lastChild;
        while (1) {
            NodeImpl *nextChild = lastLeaf->lastChild();
            if (!nextChild)
                break;
            lastLeaf = nextChild;
        }
        
	if (m_selectReplacement) {            
            // Find the first leaf.
            NodeImpl *firstLeaf = firstChild;
            while (1) {
                NodeImpl *nextChild = firstLeaf->firstChild();
                if (!nextChild)
                    break;
                firstLeaf = nextChild;
            }
            // Select what was inserted.
            setEndingSelection(Selection(Position(firstLeaf, firstLeaf->caretMinOffset()), Position(lastLeaf, lastLeaf->caretMaxOffset())));
        } else {
            // Place the cursor after what was inserted.
            setEndingSelection(Position(lastLeaf, lastLeaf->caretMaxOffset()));
        }
    }
}

//------------------------------------------------------------------------------------------
// MoveSelectionCommandImpl

MoveSelectionCommandImpl::MoveSelectionCommandImpl(DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, DOM::Position &position) 
: CompositeEditCommandImpl(document), m_fragment(fragment), m_position(position)
{
}

MoveSelectionCommandImpl::~MoveSelectionCommandImpl()
{
}

int MoveSelectionCommandImpl::commandID() const
{
    return MoveSelectionCommandID;
}

void MoveSelectionCommandImpl::doApply()
{
    Selection selection = endingSelection();
    ASSERT(selection.state() == Selection::RANGE);

    // Update the position otherwise it may become invalid after the selection is deleted.
    NodeImpl *positionNode = m_position.node();
    long positionOffset = m_position.offset();
    Position selectionEnd = selection.end();
    long selectionEndOffset = selectionEnd.offset();    
    if (selectionEnd.node() == positionNode && selectionEndOffset < positionOffset) {
        positionOffset -= selectionEndOffset;
        Position selectionStart = selection.start();
        if (selectionStart.node() == positionNode) {
            positionOffset += selectionStart.offset();
        }
    }
    
    deleteSelection();

    setEndingSelection(Position(positionNode, positionOffset));
    ReplaceSelectionCommand cmd(document(), m_fragment, true);
    applyCommandToComposite(cmd);
}

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommandImpl

RemoveCSSPropertyCommandImpl::RemoveCSSPropertyCommandImpl(DocumentImpl *document, CSSStyleDeclarationImpl *decl, int property)
    : EditCommandImpl(document), m_decl(decl), m_property(property), m_important(false)
{
    ASSERT(m_decl);
    m_decl->ref();
}

RemoveCSSPropertyCommandImpl::~RemoveCSSPropertyCommandImpl()
{
    ASSERT(m_decl);
    m_decl->deref();
}

int RemoveCSSPropertyCommandImpl::commandID() const
{
    return RemoveCSSPropertyCommandID;
}

void RemoveCSSPropertyCommandImpl::doApply()
{
    ASSERT(m_decl);

    m_oldValue = m_decl->getPropertyValue(m_property);
    ASSERT(!m_oldValue.isNull());

    m_important = m_decl->getPropertyPriority(m_property);
    m_decl->removeProperty(m_property);
}

void RemoveCSSPropertyCommandImpl::doUnapply()
{
    ASSERT(m_decl);
    ASSERT(!m_oldValue.isNull());

    m_decl->setProperty(m_property, m_oldValue, m_important);
}

//------------------------------------------------------------------------------------------
// RemoveNodeAttributeCommandImpl

RemoveNodeAttributeCommandImpl::RemoveNodeAttributeCommandImpl(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute)
    : EditCommandImpl(document), m_element(element), m_attribute(attribute)
{
    ASSERT(m_element);
    m_element->ref();
}

RemoveNodeAttributeCommandImpl::~RemoveNodeAttributeCommandImpl()
{
    ASSERT(m_element);
    m_element->deref();
}

int RemoveNodeAttributeCommandImpl::commandID() const
{
    return RemoveNodeAttributeCommandID;
}

void RemoveNodeAttributeCommandImpl::doApply()
{
    ASSERT(m_element);

    m_oldValue = m_element->getAttribute(m_attribute);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->removeAttribute(m_attribute, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeAttributeCommandImpl::doUnapply()
{
    ASSERT(m_element);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->setAttribute(m_attribute, m_oldValue.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
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

RemoveNodeAndPruneCommandImpl::RemoveNodeAndPruneCommandImpl(DocumentImpl *document, NodeImpl *pruneNode, NodeImpl *stopNode)
    : CompositeEditCommandImpl(document), m_pruneNode(pruneNode), m_stopNode(stopNode)
{
    ASSERT(m_pruneNode);
    m_pruneNode->ref();
    if (m_stopNode)
        m_stopNode->ref();
}

RemoveNodeAndPruneCommandImpl::~RemoveNodeAndPruneCommandImpl()
{
    m_pruneNode->deref();
    if (m_stopNode)
        m_stopNode->deref();
}

int RemoveNodeAndPruneCommandImpl::commandID() const
{
    return RemoveNodeAndPruneCommandID;
}

void RemoveNodeAndPruneCommandImpl::doApply()
{
    NodeImpl *editableBlock = m_pruneNode->enclosingBlockFlowElement();
    NodeImpl *pruneNode = m_pruneNode;
    NodeImpl *node = pruneNode->traversePreviousNode();
    removeNode(pruneNode);
    while (1) {
        if (node == m_stopNode || editableBlock != node->enclosingBlockFlowElement() || !shouldPruneNode(node))
            break;
        pruneNode = node;
        node = node->traversePreviousNode();
        removeNode(pruneNode);
    }
}

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommandImpl

RemoveNodePreservingChildrenCommandImpl::RemoveNodePreservingChildrenCommandImpl(DocumentImpl *document, NodeImpl *node)
    : CompositeEditCommandImpl(document), m_node(node)
{
    ASSERT(m_node);
    m_node->ref();
}

RemoveNodePreservingChildrenCommandImpl::~RemoveNodePreservingChildrenCommandImpl()
{
    if (m_node)
        m_node->deref();
}

int RemoveNodePreservingChildrenCommandImpl::commandID() const
{
    return RemoveNodePreservingChildrenCommandID;
}

void RemoveNodePreservingChildrenCommandImpl::doApply()
{
    NodeListImpl *children = node()->childNodes();
    int length = children->length();
    for (int i = 0; i < length; i++) {
        NodeImpl *child = children->item(0);
        removeNode(child);
        insertNodeBefore(child, node());
    }
    removeNode(node());
}

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommandImpl

SetNodeAttributeCommandImpl::SetNodeAttributeCommandImpl(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute, const DOMString &value)
    : EditCommandImpl(document), m_element(element), m_attribute(attribute), m_value(value)
{
    ASSERT(m_element);
    m_element->ref();
    ASSERT(!m_value.isNull());
}

SetNodeAttributeCommandImpl::~SetNodeAttributeCommandImpl()
{
    if (m_element)
        m_element->deref();
}

int SetNodeAttributeCommandImpl::commandID() const
{
    return SetNodeAttributeCommandID;
}

void SetNodeAttributeCommandImpl::doApply()
{
    ASSERT(m_element);
    ASSERT(!m_value.isNull());

    int exceptionCode = 0;
    m_oldValue = m_element->getAttribute(m_attribute);
    m_element->setAttribute(m_attribute, m_value.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

void SetNodeAttributeCommandImpl::doUnapply()
{
    ASSERT(m_element);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->setAttribute(m_attribute, m_oldValue.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// SplitTextNodeCommandImpl

SplitTextNodeCommandImpl::SplitTextNodeCommandImpl(DocumentImpl *document, TextImpl *text, long offset)
    : EditCommandImpl(document), m_text1(0), m_text2(text), m_offset(offset)
{
    ASSERT(m_text2);
    ASSERT(m_text2->length() > 0);

    m_text2->ref();
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

    m_text2->parentNode()->removeChild(m_text1, exceptionCode);
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

void TypingCommandImpl::typingAddedToOpenCommand()
{
    ASSERT(document());
    ASSERT(document()->part());
    EditCommand cmd(this);
    document()->part()->appliedEditing(cmd);
}

void TypingCommandImpl::insertText(const DOMString &text)
{
    if (document()->part()->typingStyle() || m_cmds.count() == 0) {
        InputTextCommand cmd(document());
        applyCommandToComposite(cmd);
        cmd.input(text);
    }
    else {
        EditCommand lastCommand = m_cmds.last();
        if (lastCommand.commandID() == InputTextCommandID) {
            static_cast<InputTextCommand &>(lastCommand).input(text);
        }
        else {
            InputTextCommand cmd(document());
            applyCommandToComposite(cmd);
            cmd.input(text);
        }
    }
    typingAddedToOpenCommand();
}

void TypingCommandImpl::insertNewline()
{
    InputNewlineCommand cmd(document());
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommandImpl::issueCommandForDeleteKey()
{
    Selection selectionToDelete = endingSelection();
    ASSERT(selectionToDelete.state() != Selection::NONE);
    
    if (selectionToDelete.state() == Selection::CARET) {
        Position pos(selectionToDelete.start());
        if (pos.inFirstEditableInRootEditableElement() && pos.offset() <= pos.node()->caretMinOffset()) {
            // we're at the start of a root editable block...do nothing
            return;
        }
        selectionToDelete = Selection(pos.previousCharacterPosition(), pos);
    }
    deleteSelection(selectionToDelete);
    typingAddedToOpenCommand();
}

void TypingCommandImpl::deleteKeyPressed()
{
// EDIT FIXME: The ifdef'ed out code below should be re-enabled.
// In order for this to happen, the deleteCharacter case
// needs work. Specifically, the caret-positioning code
// and whitespace-handling code in DeleteSelectionCommandImpl::doApply()
// needs to be factored out so it can be used again here.
// Until that work is done, issueCommandForDeleteKey() does the
// right thing, but less efficiently and with the cost of more
// objects.
    issueCommandForDeleteKey();
    typingAddedToOpenCommand();
#if 0    
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
        }
        else if (lastCommand.commandID() == InputNewlineCommandID) {
            lastCommand.unapply();
            removeCommand(lastCommand);
        }
        else {
            issueCommandForDeleteKey();
        }
    }
#endif
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

} // namespace khtml
