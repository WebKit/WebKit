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
#include "css/css_computedstyle.h"
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

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#include "KWQKHTMLPart.h"
#endif

using DOM::AttrImpl;
using DOM::CSSComputedStyleDeclarationImpl;
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
using DOM::StayInBlock;
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

static const int spacesPerTab = 4;

static inline bool isTab(const DOMString &text)
{
    static QChar tabCharacter = QChar(0x9);
    if (text.length() != 1)
        return false;
    
    return text[0] == tabCharacter;
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
    if (!prefix)
        prefix = "";
    if (pos.isEmpty())
        LOG(Editing, "%s <empty>", prefix);
    else
        LOG(Editing, "%s%s %p : %d", prefix, getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
}

//------------------------------------------------------------------------------------------
// StyleChange

StyleChange::StyleChange(CSSStyleDeclarationImpl *style) 
    : m_applyBold(false), m_applyItalic(false)
{
    init(style, Position());
}

StyleChange::StyleChange(CSSStyleDeclarationImpl *style, const Position &position)
     : m_applyBold(false), m_applyItalic(false)
{
    init(style, position);
}

void StyleChange::init(CSSStyleDeclarationImpl *style, const Position &position)
{
    for (QPtrListIterator<CSSProperty> it(*(style->values())); it.current(); ++it) {
        CSSProperty *property = it.current();

        // If position is empty or the position passed in already has the 
        // style, just move on.
        if (position.notEmpty() && currentlyHasStyle(position, property))
            continue;

        // Figure out the manner of change that is needed.
        switch (property->id()) {
            case CSS_PROP_FONT_WEIGHT:
                if (strcasecmp(property->value()->cssText(), "bold") == 0)
                    m_applyBold = true;
                else
                    m_cssStyle += property->cssText();
                break;
            case CSS_PROP_FONT_STYLE: {
                    DOMString cssText(property->value()->cssText());
                    if (strcasecmp(cssText, "italic") == 0 || strcasecmp(cssText, "oblique") == 0)
                        m_applyItalic = true;
                    else
                        m_cssStyle += property->cssText();
                }
                break;
            default:
                m_cssStyle += property->cssText();
                break;
        }
    }
}

bool StyleChange::currentlyHasStyle(const Position &pos, const CSSProperty *property)
{
    ASSERT(pos.notEmpty());
    CSSComputedStyleDeclarationImpl *style = pos.computedStyle();
    ASSERT(style);
    style->ref();
    CSSValueImpl *value = style->getPropertyCSSValue(property->id());
    style->deref();
    return strcasecmp(value->cssText(), property->value()->cssText()) == 0;
}

//------------------------------------------------------------------------------------------
// EditCommandImpl

EditCommandImpl::EditCommandImpl(DocumentImpl *document) 
    : SharedCommandImpl(), m_document(document), m_state(NotApplied), m_typingStyle(0), m_parent(0)
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
    if (m_typingStyle)
        m_typingStyle->deref();
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

    // The delete selection command is a special case where we want the  
    // typing style retained. For all other commands, clear it after
    // applying.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (commandID() != DeleteSelectionCommandID)
        setTypingStyle(0);

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

void EditCommandImpl::assignTypingStyle(DOM::CSSStyleDeclarationImpl *style)
{
    CSSStyleDeclarationImpl *old = m_typingStyle;
    m_typingStyle = style;
    if (m_typingStyle)
        m_typingStyle->ref();
    if (old)
        old->deref();
}

void EditCommandImpl::setTypingStyle(CSSStyleDeclarationImpl *style)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    assignTypingStyle(style);
    EditCommand cmd = parent();
    while (cmd.notNull()) {
        cmd.handle()->assignTypingStyle(style);
        cmd = cmd.parent();
    }
}

void EditCommandImpl::markMisspellingsInSelection(const Selection &s)
{
    KWQ(document()->part())->markMisspellingsInSelection(s);
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
        appendNode(insertChild, refChild->parentNode());
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
            appendNode(insertChild, refChild);
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

void CompositeEditCommandImpl::appendNode(NodeImpl *appendChild, NodeImpl *parent)
{
    AppendNodeCommand cmd(document(), appendChild, parent);
    applyCommandToComposite(cmd);
}

void CompositeEditCommandImpl::removeNode(NodeImpl *removeChild)
{
    RemoveNodeCommand cmd(document(), removeChild);
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

ElementImpl *CompositeEditCommandImpl::applyTypingStyle(NodeImpl *child) const
{
    // FIXME: This function should share code with ApplyStyleCommandImpl::applyStyleIfNeeded
    // and ApplyStyleCommandImpl::computeStyleChange.
    // Both function do similar work, and the common parts could be factored out.

    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    StyleChange styleChange(document()->part()->typingStyle());

    NodeImpl *childToAppend = child;
    ElementImpl *element = 0;
    int exceptionCode = 0;

    if (styleChange.applyItalic()) {
        ElementImpl *italicElement = document()->createHTMLElement("I", exceptionCode);
        ASSERT(exceptionCode == 0);
        italicElement->appendChild(childToAppend, exceptionCode);
        ASSERT(exceptionCode == 0);
        element = italicElement;
        childToAppend = italicElement;
    }

    if (styleChange.applyBold()) {
        ElementImpl *boldElement = document()->createHTMLElement("B", exceptionCode);
        ASSERT(exceptionCode == 0);
        boldElement->appendChild(childToAppend, exceptionCode);
        ASSERT(exceptionCode == 0);
        element = boldElement;
        childToAppend = boldElement;
    }

    if (styleChange.cssStyle().length() > 0) {
        ElementImpl *styleElement = document()->createHTMLElement("SPAN", exceptionCode);
        ASSERT(exceptionCode == 0);
        styleElement->setAttribute(ATTR_STYLE, styleChange.cssStyle());
        styleElement->setAttribute(ATTR_CLASS, styleSpanClassString());
        styleElement->appendChild(childToAppend, exceptionCode);
        ASSERT(exceptionCode == 0);
        element = styleElement;
        childToAppend = styleElement;
    }

    return element;
}

void CompositeEditCommandImpl::deleteUnrenderedText(const Position &pos)
{
    Position upstream = pos.upstream(StayInBlock);
    Position downstream = pos.downstream(StayInBlock);
    Position block = Position(pos.node()->enclosingBlockFlowElement(), 0);
    
    NodeImpl *node = upstream.node();
    while (node != downstream.node()) {
        NodeImpl *next = node->traverseNextNode();
        if (node->isTextNode()) {
            if (!node->renderer() || !static_cast<RenderText *>(node->renderer())->firstTextBox())
                removeNode(node);
            else {
                TextImpl *text = static_cast<TextImpl *>(node);
                if ((int)text->length() > text->caretMaxOffset())
                    deleteText(text, text->caretMaxOffset(), text->length() - text->caretMaxOffset());
            }
        }
        node = next;
    }
    if (downstream.node()->isTextNode()) {
        if (!node->renderer() || !static_cast<RenderText *>(node->renderer())->firstTextBox())
            removeNode(downstream.node());
        else {
            TextImpl *text = static_cast<TextImpl *>(downstream.node());
            if (text->caretMinOffset() > 0)
                deleteText(text, 0, text->caretMinOffset());
        }
    }
    
    if (pos.node()->inDocument())
        setEndingSelection(pos);
    else if (upstream.node()->inDocument())
        setEndingSelection(upstream);
    else if (downstream.node()->inDocument())
        setEndingSelection(downstream);
    else
        setEndingSelection(block);
}


//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommandImpl

AppendNodeCommandImpl::AppendNodeCommandImpl(DocumentImpl *document, NodeImpl *appendChild, NodeImpl *parentNode)
    : EditCommandImpl(document), m_appendChild(appendChild), m_parentNode(parentNode)
{
    ASSERT(m_appendChild);
    m_appendChild->ref();

    ASSERT(m_parentNode);
    m_parentNode->ref();
}

AppendNodeCommandImpl::~AppendNodeCommandImpl()
{
    if (m_appendChild)
        m_appendChild->deref();
    if (m_parentNode)
        m_parentNode->deref();
}

int AppendNodeCommandImpl::commandID() const
{
    return AppendNodeCommandID;
}

void AppendNodeCommandImpl::doApply()
{
    ASSERT(m_appendChild);
    ASSERT(m_parentNode);

    int exceptionCode = 0;
    m_parentNode->appendChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void AppendNodeCommandImpl::doUnapply()
{
    ASSERT(m_appendChild);
    ASSERT(m_parentNode);
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
    Position start(endingSelection().start().downstream(StayInBlock).equivalentRangeCompliantPosition());
    Position end(endingSelection().end().upstream(StayInBlock));

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    removeStyle(start.upstream(), end);
    
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
                    NodeImpl *next = node->traverseNextNode();
                    // Break if node is the end node, or if the next node does not fit in with
                    // the current group.
                    if (node == end.node() || 
                        runStart->parentNode() != next->parentNode() || 
                        (next->isHTMLElement() && next->id() != ID_BR) || 
                        (next->renderer() && !next->renderer()->isInline()))
                        break;
                    node = next;
                }
                // Now apply style to the run we found.
                applyStyleIfNeeded(runStart, node);
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
        if (decl->values()->count() == 0) {
            removeNodeAttribute(elem, ATTR_STYLE);
            NamedAttrMapImpl *map = elem->attributes();
            if (map && map->length() == 1 && elem->getAttribute(ATTR_CLASS) == styleSpanClassString())
                removeNodePreservingChildren(elem);
        }
    }
}

void ApplyStyleCommandImpl::removeStyle(const Position &start, const Position &end)
{
    NodeImpl *node = start.node();
    while (1) {
        NodeImpl *next = node->traverseNextNode();
        if (node->isHTMLElement() && nodeFullySelected(start, node)) {
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

bool ApplyStyleCommandImpl::nodeFullySelected(const Position &start, const NodeImpl *node) const
{
    ASSERT(node);

    if (node == start.node())
        return start.offset() >= node->caretMaxOffset();

    for (NodeImpl *child = node->lastChild(); child; child = child->lastChild()) {
        if (child == start.node())
            return start.offset() >= child->caretMaxOffset();
    }

    return !start.node()->isAncestor(node);
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
            appendNode(node, element);
        }
        if (node == endNode)
            break;
        node = next;
    }
}

void ApplyStyleCommandImpl::applyStyleIfNeeded(NodeImpl *startNode, NodeImpl *endNode)
{
    // FIXME: This function should share code with CompositeEditCommandImpl::applyTypingStyle.
    // Both function do similar work, and the common parts could be factored out.

    StyleChange styleChange(style(), Position(startNode, 0));
    int exceptionCode = 0;
    
    if (styleChange.cssStyle().length() > 0) {
        ElementImpl *styleElement = document()->createHTMLElement("SPAN", exceptionCode);
        ASSERT(exceptionCode == 0);
        styleElement->setAttribute(ATTR_STYLE, styleChange.cssStyle());
        styleElement->setAttribute(ATTR_CLASS, styleSpanClassString());
        insertNodeBefore(styleElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, styleElement);
    }

    if (styleChange.applyBold()) {
        ElementImpl *boldElement = document()->createHTMLElement("B", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeBefore(boldElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, boldElement);
    }

    if (styleChange.applyItalic()) {
        ElementImpl *italicElement = document()->createHTMLElement("I", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeBefore(italicElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, italicElement);
    }
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

CSSStyleDeclarationImpl *DeleteSelectionCommandImpl::computeTypingStyle(const Position &pos) const
{
    ElementImpl *element = pos.element();
    if (!element)
        return 0;

    ElementImpl *shallowElement = pos.equivalentShallowPosition().element();
    if (!shallowElement)
        return 0;

    ElementImpl *parent = Position(shallowElement->parentNode(), 0).element();
    if (!parent)
        return 0;

    // Loop from the element up to the shallowElement, building up the
    // style that this node has that its parent does not.
    CSSStyleDeclarationImpl *result = document()->createCSSStyleDeclaration();
    NodeImpl *node = element;
    while (1) {
        // check for an inline style declaration
        if (node->isHTMLElement()) {
            CSSStyleDeclarationImpl *s = static_cast<HTMLElementImpl *>(node)->inlineStyleDecl();
            if (s)
                result->merge(s, false);
        }
        // check if this is a bold tag
        if (node->id() == ID_B) {
            CSSValueImpl *boldValue = result->getPropertyCSSValue(CSS_PROP_FONT_WEIGHT);
            if (!boldValue)
                result->setProperty(CSS_PROP_FONT_WEIGHT, "bold");
        }
        // check if this is an italic tag
        if (node->id() == ID_I) {
            CSSValueImpl *italicValue = result->getPropertyCSSValue(CSS_PROP_FONT_STYLE);
            if (!italicValue)
                result->setProperty(CSS_PROP_FONT_STYLE, "italic");
        }
        if (node == shallowElement)
            break;
        node = node->parentNode();
    }

    return result;
}

void DeleteSelectionCommandImpl::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();
        
    if (m_selectionToDelete.state() != Selection::RANGE)
        return;

    Position upstreamStart(m_selectionToDelete.start().upstream(StayInBlock));
    Position downstreamStart(m_selectionToDelete.start().downstream());
    Position upstreamEnd(m_selectionToDelete.end().upstream(StayInBlock));
    Position downstreamEnd(m_selectionToDelete.end().downstream());
    Position endingPosition;

    // Save away whitespace situation before doing any deletions
    Position leading = upstreamStart.leadingWhitespacePosition();
    Position trailing = downstreamEnd.trailingWhitespacePosition();
    bool trailingValid = true;
    
    debugPosition("upstreamStart    ", upstreamStart);
    debugPosition("downstreamStart  ", downstreamStart);
    debugPosition("upstreamEnd      ", upstreamEnd);
    debugPosition("downstreamEnd    ", downstreamEnd);
    debugPosition("leading          ", leading);
    debugPosition("trailing         ", trailing);
    
    NodeImpl *startBlock = downstreamStart.node()->enclosingBlockFlowElement();
    NodeImpl *endBlock = upstreamEnd.node()->enclosingBlockFlowElement();
    if (!startBlock || !endBlock)
        // Can't figure out what blocks we're in. This can happen if
        // the document structure is not what we are expecting, like if
        // the document has no body element, or if the editable block
        // has been changed to display: inline. Some day it might
        // be nice to be able to deal with this, but for now, bail.
        return;

    // Figure out the typing style in effect before the delete is done.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSComputedStyleDeclarationImpl *computedStyle = downstreamStart.computedStyle();
    computedStyle->ref();
    CSSStyleDeclarationImpl *style = computedStyle->copyInheritableProperties();
    style->ref();
    computedStyle->deref();
    
    NodeImpl *startNode = upstreamStart.node();
    int startOffset = upstreamStart.offset();
    if (startOffset >= startNode->caretMaxOffset()) {
        // None of the first node is to be deleted, so move to next.
        startNode = startNode->traverseNextNode();
        startOffset = 0;
    }

    if (startNode == downstreamEnd.node()) {
        // handle delete in one node
        if (!startNode->renderer() || 
            (startOffset <= startNode->caretMinOffset() && downstreamEnd.offset() >= startNode->caretMaxOffset())) {
            // just delete
            removeNode(startNode);
        }
        else {
            // in a text node that needs to be trimmed
            TextImpl *text = static_cast<TextImpl *>(startNode);
            deleteText(text, startOffset, downstreamEnd.offset() - startOffset);
            trailingValid = false;
        }
    }
    else {
        NodeImpl *node = startNode;
        
        if (startOffset > 0) {
            // in a text node that needs to be trimmed
            TextImpl *text = static_cast<TextImpl *>(node);
            deleteText(text, startOffset, text->length() - startOffset);
            node = node->traverseNextNode();
        }
        
        // handle deleting all nodes that are completely selected
        while (node && node != downstreamEnd.node()) {
            if (!downstreamEnd.node()->isAncestor(node)) {
                NodeImpl *nextNode = node->traverseNextSibling();
                removeNode(node);
                node = nextNode;
            }
            else {
                NodeImpl *n = node->lastChild();
                while (n && n->lastChild())
                    n = n->lastChild();
                if (n == downstreamEnd.node() && downstreamEnd.offset() >= downstreamEnd.node()->caretMaxOffset()) {
                    NodeImpl *nextNode = node->traverseNextSibling();
                    removeNode(node);
                    node = nextNode;
                } 
                else {
                    node = node->traverseNextNode();
                }
            }
        }

        if (upstreamEnd.node() != startNode && upstreamEnd.node()->inDocument() && upstreamEnd.offset() >= upstreamEnd.node()->caretMinOffset()) {
            if (upstreamEnd.offset() >= upstreamEnd.node()->caretMaxOffset()) {
                // need to delete whole node
                // we can get here if this is the last node in the block
                removeNode(upstreamEnd.node());
                trailingValid = false;
            }
            else {
                // in a text node that needs to be trimmed
                TextImpl *text = static_cast<TextImpl *>(upstreamEnd.node());
                if (upstreamEnd.offset() > 0) {
                    deleteText(text, 0, upstreamEnd.offset());
                    trailingValid = false;
                }
            }
            if (!upstreamStart.node()->inDocument() && upstreamEnd.node()->inDocument())
                endingPosition = Position(upstreamEnd.node(), 0);
        }
    }
    
    // Do block merge if start and end of selection are in different blocks.
    if (endBlock != startBlock && endBlock->inDocument()) {
        LOG(Editing,  "merging content to start block");
        NodeImpl *node = endBlock->firstChild();
        while (node) {
            NodeImpl *moveNode = node;
            node = node->nextSibling();
            removeNode(moveNode);
            appendNode(moveNode, startBlock);
        }
        removeNode(endBlock);
    }
      
    // Figure out where the end position should be
    if (endingPosition.notEmpty())
        goto FixupWhitespace;

    endingPosition = upstreamStart;
    if (endingPosition.node()->inDocument())
        goto FixupWhitespace;
    
    endingPosition = downstreamEnd;
    if (endingPosition.node()->inDocument())
        goto FixupWhitespace;

    endingPosition = Position(startBlock, 0);
    if (endingPosition.node()->inDocument())
        goto FixupWhitespace;

    endingPosition = Position(endBlock, 0);
    if (endingPosition.node()->inDocument())
        goto FixupWhitespace;

    endingPosition = Position(document()->documentElement(), 0);

    // Perform whitespace fixup
    FixupWhitespace:

    if (leading.notEmpty() || trailing.notEmpty())
        document()->updateLayout();

    debugPosition("endingPosition   ", endingPosition);
    
    if (leading.notEmpty() && !leading.isRenderedCharacter()) {
        LOG(Editing, "replace leading");
        TextImpl *textNode = static_cast<TextImpl *>(leading.node());
        replaceText(textNode, leading.offset(), 1, nonBreakingSpaceString());
    }

    if (trailing.notEmpty()) {
        if (trailingValid) {
            if (!trailing.isRenderedCharacter()) {
                LOG(Editing, "replace trailing [valid]");
                TextImpl *textNode = static_cast<TextImpl *>(trailing.node());
                replaceText(textNode, trailing.offset(), 1, nonBreakingSpaceString());
            }
        }
        else {
            Position pos = endingPosition.downstream(StayInBlock);
            pos = Position(pos.node(), pos.offset() - 1);
            if (isWS(pos) && !pos.isRenderedCharacter()) {
                LOG(Editing, "replace trailing [invalid]");
                TextImpl *textNode = static_cast<TextImpl *>(pos.node());
                replaceText(textNode, pos.offset(), 1, nonBreakingSpaceString());
                endingPosition = pos;
            }
        }
    }

    // Compute the difference between the style before the delete and the style now
    // after the delete has been done. Set this style on the part, so other editing
    // commands being composed with this one will work, and also cache it on the command,
    // so the KHTMLPart::appliedEditing can set it after the whole composite command 
    // has completed.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSComputedStyleDeclarationImpl endingStyle(endingPosition.node());
    endingStyle.diff(style);
    document()->part()->setTypingStyle(style);
    setTypingStyle(style);
    style->deref();

    setEndingSelection(endingPosition);
}

//------------------------------------------------------------------------------------------
// DeleteTextCommandImpl

DeleteTextCommandImpl::DeleteTextCommandImpl(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditCommandImpl(document), m_node(node), m_offset(offset), m_count(count)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(m_offset < (long)m_node->length());
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
    Position upstream(pos.upstream(StayInBlock));
    NodeImpl *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(node, cb);
    else
        insertNodeAfter(node, pos.node());
}

void InputNewlineCommandImpl::insertNodeBeforePosition(NodeImpl *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *before* the block.
    Position upstream(pos.upstream(StayInBlock));
    NodeImpl *cb = pos.node()->enclosingBlockFlowElement();
    if (cb == pos.node())
        appendNode(node, cb);
    else
        insertNodeBefore(node, pos.node());
}

void InputNewlineCommandImpl::doApply()
{
    deleteSelection();
    deleteUnrenderedText(endingSelection().start());
    
    Selection selection = endingSelection();

    int exceptionCode = 0;
    ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
    ASSERT(exceptionCode == 0);

    NodeImpl *nodeToInsert = breakNode;
    
    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    if (typingStyle && typingStyle->length() > 0)
        nodeToInsert = applyTypingStyle(breakNode);
    
    Position pos(selection.start().upstream(StayInBlock));
    bool atStart = pos.offset() <= pos.node()->caretMinOffset();
    bool atEnd = pos.offset() >= pos.node()->caretMaxOffset();
    bool atEndOfBlock = pos.isLastRenderedPositionInEditableBlock();
    
    if (atEndOfBlock) {
        LOG(Editing, "input newline case 1");
        // Check for a trailing BR. If there isn't one, we'll need to insert an "extra" one.
        // This makes the "real" BR we want to insert appear in the rendering without any 
        // significant side effects (and no real worries either since you can't arrow past 
        // this extra one.
        NodeImpl *next = pos.node()->traverseNextNode();
        bool hasTrailingBR = next && next->id() == ID_BR;
        insertNodeAfterPosition(nodeToInsert, pos);
        if (hasTrailingBR) {
            setEndingSelection(Position(nodeToInsert, 0));
        }
        else {
            // Insert an "extra" BR at the end of the block. 
            ElementImpl *extraBreakNode = document()->createHTMLElement("BR", exceptionCode);
            ASSERT(exceptionCode == 0);
            insertNodeAfter(extraBreakNode, nodeToInsert);
            setEndingSelection(Position(extraBreakNode, 0));
        }
    }
    else if (atStart) {
        LOG(Editing, "input newline case 2");
        // Insert node, but place the caret into index 0 of the downstream
        // position. This will make the caret appear after the break, and as we know
        // there is content at that location, this is OK.
        insertNodeBeforePosition(nodeToInsert, pos);
        setEndingSelection(Position(pos.node(), pos.node()->caretMinOffset()));
    }
    else if (atEnd) {
        LOG(Editing, "input newline case 3");
        // Insert BR after this node. Place caret in the position that is downstream
        // of the current position, reckoned before inserting the BR in between.
        Position endingPosition = pos.downstream(StayInBlock);
        insertNodeAfterPosition(nodeToInsert, pos);
        setEndingSelection(endingPosition);
    }
    else {
        // Split a text node
        LOG(Editing, "input newline case 4");
        ASSERT(pos.node()->isTextNode());
        
        // See if there is trailing whitespace we need to consider
        // Note: leading whitespace just works. Blame the web.
        Position trailing = pos.downstream(StayInBlock).trailingWhitespacePosition();

        // Do the split
        TextImpl *textNode = static_cast<TextImpl *>(pos.node());
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.start().offset(), exceptionCode));
        deleteText(textNode, 0, pos.offset());
        insertNodeBefore(textBeforeNode, textNode);
        insertNodeBefore(nodeToInsert, textNode);
        Position endingPosition = Position(textNode, 0);
        
        // Handle whitespace that occurs after the split
        document()->updateLayout();
        if (trailing.notEmpty() && !endingPosition.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            deleteUnrenderedText(endingPosition);
            insertText(textNode, 0, nonBreakingSpaceString());
        }
        
        setEndingSelection(endingPosition);
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
        pos = pos.downstream(StayInBlock);
    else
        pos = pos.upstream(StayInBlock);
    
    if (!pos.node()->isTextNode()) {
        NodeImpl *textNode = document()->createEditingTextNode("");
        NodeImpl *nodeToInsert = textNode;

        // Handle the case where there is a typing style.
        // FIXME: Improve typing style.
        // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
        CSSStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
        if (typingStyle && typingStyle->length() > 0)
            nodeToInsert = applyTypingStyle(textNode);
        
        // Now insert the node in the right place
        if (pos.node()->isEditableBlock()) {
            LOG(Editing, "prepareForTextInsertion case 1");
            appendNode(nodeToInsert, pos.node());
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
        // FIXME: Improve typing style.
        // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
        CSSStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
        if (typingStyle && typingStyle->length() > 0) {
            if (pos.node()->isTextNode() && pos.offset() > pos.node()->caretMinOffset() && pos.offset() < pos.node()->caretMaxOffset()) {
                // Need to split current text node in order to insert a span.
                TextImpl *text = static_cast<TextImpl *>(pos.node());
                SplitTextNodeCommand cmd(document(), text, pos.offset());
                applyCommandToComposite(cmd);
                setEndingSelection(Position(cmd.node(), 0));
            }
            
            TextImpl *editingTextNode = document()->createEditingTextNode("");
            NodeImpl *node = endingSelection().start().upstream(StayInBlock).node();
            insertNodeAfter(applyTypingStyle(editingTextNode), node);
            pos = Position(editingTextNode, 0);
        }
    }
    return pos;
}

void InputTextCommandImpl::execute(const DOMString &text)
{
    Selection selection = endingSelection();
    bool adjustDownstream = selection.start().downstream(StayInBlock).isFirstRenderedPositionOnLine();

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.state() == Selection::RANGE)
        deleteSelection();
    
    deleteUnrenderedText(endingSelection().start());
    
    // Make sure the document is set up to receive text
    Position pos = prepareForTextInsertion(adjustDownstream);
    
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    long offset = pos.offset();
    
    // These are temporary implementations for inserting adjoining spaces
    // into a document. We are working on a CSS-related whitespace solution
    // that will replace this some day. We hope.
    if (isTab(text)) {
        // Treat a tab like a number of spaces. This seems to be the HTML editing convention,
        // although the number of spaces varies (we choose four spaces). 
        // Note that there is no attempt to make this work like a real tab stop, it is merely 
        // a set number of spaces. This also seems to be the HTML editing convention.
        for (int i = 0; i < spacesPerTab; i++) {
            insertSpace(textNode, offset);
            document()->updateLayout();
        }
        setEndingSelection(Position(textNode, offset + spacesPerTab));
        m_charactersAdded += spacesPerTab;
    }
    else if (isWS(text)) {
        insertSpace(textNode, offset);
        setEndingSelection(Position(textNode, offset + 1));
        m_charactersAdded++;
    }
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
        setEndingSelection(Position(textNode, offset + text.length()));
        m_charactersAdded += text.length();
    }
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
        Position downstream = pos.downstream();
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

    if (m_text.isEmpty())
	return;


    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertTextCommandImpl::doUnapply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    if (m_text.isEmpty())
	return;

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
    
    // This command does not use any typing style that is set as a residual effect of
    // a delete.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    document()->part()->clearTypingStyle();
    setTypingStyle(0);
    
    selection = endingSelection();
    ASSERT(!selection.isEmpty());
    
    if (!firstChild) {
        // Pasting something that didn't parse or was empty.
        ASSERT(!lastChild);
    } else if (firstChild == lastChild && firstChild->isTextNode()) {
        // Simple text paste. Treat as if the text were typed.
        Position upstreamStart(selection.start().upstream(StayInBlock));
        inputText(static_cast<TextImpl *>(firstChild)->data());
        if (m_selectReplacement) {
            // Select what was inserted.
            setEndingSelection(Selection(selection.base(), endingSelection().extent()));
        }
        else {
            // Mark misspellings in the inserted content.
            markMisspellingsInSelection(Selection(upstreamStart, endingSelection().extent()));
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

        // Find the first leaf.
        NodeImpl *firstLeaf = firstChild;
        while (1) {
            NodeImpl *nextChild = firstLeaf->firstChild();
            if (!nextChild)
                break;
            firstLeaf = nextChild;
        }
        
        Selection replacementSelection(Position(firstLeaf, firstLeaf->caretMinOffset()), Position(lastLeaf, lastLeaf->caretMaxOffset()));
	if (m_selectReplacement) {
            // Select what was inserted.
            setEndingSelection(replacementSelection);
        } 
        else {
            // Place the cursor after what was inserted, and mark misspellings in the inserted content.
            selection = Selection(Position(lastLeaf, lastLeaf->caretMaxOffset()));
            setEndingSelection(selection);
            markMisspellingsInSelection(replacementSelection);
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

TypingCommandImpl::TypingCommandImpl(DocumentImpl *document, TypingCommand::ETypingCommand commandType, const DOM::DOMString &textToInsert)
    : CompositeEditCommandImpl(document), m_commandType(commandType), m_textToInsert(textToInsert), m_openForMoreTyping(true), m_applyEditing(false)
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
    if (endingSelection().state() == Selection::NONE)
        return;

    switch (m_commandType) {
        case TypingCommand::DeleteKey:
            deleteKeyPressed();
            break;
        case TypingCommand::InsertText:
            insertText(m_textToInsert);
            break;
        case TypingCommand::InsertNewline:
            insertNewline();
            break;
    }
}

void TypingCommandImpl::markMisspellingsAfterTyping()
{
    // Take a look at the selection that results after typing and determine whether we need to spellcheck. 
    // Since the word containing the current selection is never marked, this does a check to
    // see if typing made a new word that is not in the current selection. Basically, you
    // get this by being at the end of a word and typing a space.    
    Position start(endingSelection().start());
    Position p1 = start.previousCharacterPosition().previousWordBoundary();
    Position p2 = start.previousWordBoundary();
    if (p1 != p2)
        markMisspellingsInSelection(Selection(p1, start));
}

void TypingCommandImpl::typingAddedToOpenCommand()
{
    markMisspellingsAfterTyping();
    // Do not apply editing to the part on the first time through.
    // The part will get told in the same way as all other commands.
    // But since this command stays open and is used for additional typing, 
    // we need to tell the part here as other commands are added.
    if (m_applyEditing) {
        EditCommand cmd(this);
        document()->part()->appliedEditing(cmd);
    }
    m_applyEditing = true;
}

void TypingCommandImpl::insertText(const DOMString &text)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
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
        if (pos.inRenderedContent())
            selectionToDelete = Selection(pos.previousCharacterPosition(), pos);
        else
            selectionToDelete = Selection(pos.upstream(StayInBlock).previousCharacterPosition(), pos.downstream());
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
