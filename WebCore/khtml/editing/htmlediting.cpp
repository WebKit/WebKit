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

#include "htmlediting.h"

#include "css_computedstyle.h"
#include "css_value.h"
#include "css_valueimpl.h"
#include "cssproperties.h"
#include "dom_doc.h"
#include "dom_docimpl.h"
#include "dom_docimpl.h"
#include "dom_elementimpl.h"
#include "dom_nodeimpl.h"
#include "dom_position.h"
#include "dom_positioniterator.h"
#include "dom_stringimpl.h"
#include "dom_textimpl.h"
#include "dom2_rangeimpl.h"
#include "html_elementimpl.h"
#include "html_imageimpl.h"
#include "htmlattrs.h"
#include "htmltags.h"
#include "khtml_part.h"
#include "khtml_part.h"
#include "khtmlview.h"
#include "qptrlist.h"
#include "render_object.h"
#include "render_style.h"
#include "render_text.h"
#include "visible_position.h"
#include "visible_units.h"

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
using DOM::DoNotUpdateLayout;
using DOM::EditingTextImpl;
using DOM::ElementImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLImageElementImpl;
using DOM::NamedAttrMapImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::Position;
using DOM::PositionIterator;
using DOM::Range;
using DOM::RangeImpl;
using DOM::StayInBlock;
using DOM::TextImpl;
using DOM::TreeWalkerImpl;

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#include "KWQKHTMLPart.h"
#endif

#if !APPLE_CHANGES
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#define ASSERT(assertion) assert(assertion)
#if LOG_DISABLED
#define debugPosition(a,b) ((void)0)
#endif
#endif

#define IF_IMPL_NULL_RETURN_ARG(arg) do { \
        if (isNull()) { return arg; } \
    } while (0)
        
#define IF_IMPL_NULL_RETURN do { \
        if (isNull()) { return; } \
    } while (0)

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

static inline bool isTableStructureNode(const NodeImpl *node)
{
    RenderObject *r = node->renderer();
    return (r && (r->isTableCell() || r->isTableRow() || r->isTableSection() || r->isTableCol()));
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
    if (pos.isNull())
        LOG(Editing, "%s <null>", prefix);
    else
        LOG(Editing, "%s%s %p : %d", prefix, getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
}

//------------------------------------------------------------------------------------------
// EditCommandPtr

EditCommandPtr::EditCommandPtr()
{
}

EditCommandPtr::EditCommandPtr(EditCommand *impl) : SharedPtr<EditCommand>(impl)
{
}

EditCommandPtr::EditCommandPtr(const EditCommandPtr &o) : SharedPtr<EditCommand>(o)
{
}

EditCommandPtr::~EditCommandPtr()
{
}

EditCommandPtr &EditCommandPtr::operator=(const EditCommandPtr &c)
{
    static_cast<SharedPtr<EditCommand> &>(*this) = c;
    return *this;
}

bool EditCommandPtr::isCompositeStep() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isCompositeStep();
}

bool EditCommandPtr::isInputTextCommand() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isInputTextCommand();
}

bool EditCommandPtr::isTypingCommand() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isTypingCommand();
}

void EditCommandPtr::apply() const
{
    IF_IMPL_NULL_RETURN;
    get()->apply();
}

void EditCommandPtr::unapply() const
{
    IF_IMPL_NULL_RETURN;
    get()->unapply();
}

void EditCommandPtr::reapply() const
{
    IF_IMPL_NULL_RETURN;
    get()->reapply();
}

DocumentImpl * const EditCommandPtr::document() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->document();
}

Selection EditCommandPtr::startingSelection() const
{
    IF_IMPL_NULL_RETURN_ARG(Selection());
    return get()->startingSelection();
}

Selection EditCommandPtr::endingSelection() const
{
    IF_IMPL_NULL_RETURN_ARG(Selection());
    return get()->endingSelection();
}

void EditCommandPtr::setStartingSelection(const Selection &s) const
{
    IF_IMPL_NULL_RETURN;
    get()->setStartingSelection(s);
}

void EditCommandPtr::setEndingSelection(const Selection &s) const
{
    IF_IMPL_NULL_RETURN;
    get()->setEndingSelection(s);
}

CSSStyleDeclarationImpl *EditCommandPtr::typingStyle() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->typingStyle();
}

void EditCommandPtr::setTypingStyle(CSSStyleDeclarationImpl *style) const
{
    IF_IMPL_NULL_RETURN;
    get()->setTypingStyle(style);
}

EditCommandPtr EditCommandPtr::parent() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->parent();
}

void EditCommandPtr::setParent(const EditCommandPtr &cmd) const
{
    IF_IMPL_NULL_RETURN;
    get()->setParent(cmd.get());
}

EditCommandPtr &EditCommandPtr::emptyCommand()
{
    static EditCommandPtr m_emptyCommand;
    return m_emptyCommand;
}

//------------------------------------------------------------------------------------------
// StyleChange

StyleChange::StyleChange(CSSStyleDeclarationImpl *style) 
{
    init(style, Position());
}

StyleChange::StyleChange(CSSStyleDeclarationImpl *style, const Position &position)
{
    init(style, position);
}

void StyleChange::init(CSSStyleDeclarationImpl *style, const Position &position)
{
    m_applyBold = false;
    m_applyItalic = false;

    QString styleText;

    for (QPtrListIterator<CSSProperty> it(*(style->values())); it.current(); ++it) {
        CSSProperty *property = it.current();

        // If position is empty or the position passed in already has the 
        // style, just move on.
        if (position.isNotNull() && currentlyHasStyle(position, property))
            continue;

        // Figure out the manner of change that is needed.
        DOMString valueText(property->value()->cssText());
        switch (property->id()) {
            case CSS_PROP_FONT_WEIGHT:
                if (strcasecmp(valueText, "bold") == 0) {
                    m_applyBold = true;
                    continue;
                }
                break;
            case CSS_PROP_FONT_STYLE:
                if (strcasecmp(valueText, "italic") == 0 || strcasecmp(valueText, "oblique") == 0) {
                    m_applyItalic = true;
                    continue;
                }
                break;
        }

        styleText += property->cssText().string();
    }

    m_cssStyle = styleText.stripWhiteSpace();
}

bool StyleChange::currentlyHasStyle(const Position &pos, const CSSProperty *property)
{
    ASSERT(pos.isNotNull());
    CSSComputedStyleDeclarationImpl *style = pos.computedStyle();
    ASSERT(style);
    style->ref();
    CSSValueImpl *value = style->getPropertyCSSValue(property->id(), DoNotUpdateLayout);
    style->deref();
    return value && strcasecmp(value->cssText(), property->value()->cssText()) == 0;
}

//------------------------------------------------------------------------------------------
// EditCommand

EditCommand::EditCommand(DocumentImpl *document) 
    : m_document(document), m_state(NotApplied), m_typingStyle(0), m_parent(0)
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    m_document->ref();
    m_startingSelection = m_document->part()->selection();
    m_endingSelection = m_startingSelection;

    m_document->part()->setSelection(Selection(), false, true);
}

EditCommand::~EditCommand()
{
    ASSERT(m_document);
    m_document->deref();
    if (m_typingStyle)
        m_typingStyle->deref();
}

void EditCommand::apply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == NotApplied);
 
    KHTMLPart *part = m_document->part();

    ASSERT(part->selection().isNone());

    doApply();
    
    m_state = Applied;

    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!preservesTypingStyle())
        setTypingStyle(0);

    if (!isCompositeStep()) {
        document()->updateLayout();
        EditCommandPtr cmd(this);
        part->appliedEditing(cmd);
    }
}

void EditCommand::unapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == Applied);

    bool topLevel = !isCompositeStep();
 
    KHTMLPart *part = m_document->part();

    if (topLevel) {
        part->setSelection(Selection(), false, true);
    }
    ASSERT(part->selection().isNone());
    
    doUnapply();
    
    m_state = NotApplied;

    if (topLevel) {
        document()->updateLayout();
        EditCommandPtr cmd(this);
        part->unappliedEditing(cmd);
    }
}

void EditCommand::reapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == NotApplied);
    
    bool topLevel = !isCompositeStep();
 
    KHTMLPart *part = m_document->part();

    if (topLevel) {
        part->setSelection(Selection(), false, true);
    }
    ASSERT(part->selection().isNone());
    
    doReapply();
    
    m_state = Applied;

    if (topLevel) {
        document()->updateLayout();
        EditCommandPtr cmd(this);
        part->reappliedEditing(cmd);
    }
}

void EditCommand::doReapply()
{
    doApply();
}

void EditCommand::setStartingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent.get())
        cmd->m_startingSelection = s;
}

void EditCommand::setEndingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent.get())
        cmd->m_endingSelection = s;
}

void EditCommand::assignTypingStyle(CSSStyleDeclarationImpl *style)
{
    CSSStyleDeclarationImpl *old = m_typingStyle;
    m_typingStyle = style;
    if (m_typingStyle)
        m_typingStyle->ref();
    if (old)
        old->deref();
}

void EditCommand::setTypingStyle(CSSStyleDeclarationImpl *style)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent.get())
        cmd->assignTypingStyle(style);
}

bool EditCommand::preservesTypingStyle() const
{
    return false;
}

bool EditCommand::isInputTextCommand() const
{
    return false;
}

bool EditCommand::isTypingCommand() const
{
    return false;
}

//------------------------------------------------------------------------------------------
// CompositeEditCommand

CompositeEditCommand::CompositeEditCommand(DocumentImpl *document) 
    : EditCommand(document)
{
}

void CompositeEditCommand::doUnapply()
{
    if (m_cmds.count() == 0) {
        return;
    }
    
    for (int i = m_cmds.count() - 1; i >= 0; --i)
        m_cmds[i]->unapply();

    setState(NotApplied);
}

void CompositeEditCommand::doReapply()
{
    if (m_cmds.count() == 0) {
        return;
    }

    for (QValueList<EditCommandPtr>::ConstIterator it = m_cmds.begin(); it != m_cmds.end(); ++it)
        (*it)->reapply();

    setState(Applied);
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommand::applyCommandToComposite(EditCommandPtr &cmd)
{
    cmd.setStartingSelection(endingSelection());
    cmd.setEndingSelection(endingSelection());
    cmd.setParent(this);
    cmd.apply();
    m_cmds.append(cmd);
}

void CompositeEditCommand::insertNodeBefore(NodeImpl *insertChild, NodeImpl *refChild)
{
    EditCommandPtr cmd(new InsertNodeBeforeCommand(document(), insertChild, refChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeAfter(NodeImpl *insertChild, NodeImpl *refChild)
{
    if (refChild->parentNode()->lastChild() == refChild) {
        appendNode(insertChild, refChild->parentNode());
    }
    else {
        ASSERT(refChild->nextSibling());
        insertNodeBefore(insertChild, refChild->nextSibling());
    }
}

void CompositeEditCommand::insertNodeAt(NodeImpl *insertChild, NodeImpl *refChild, long offset)
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

void CompositeEditCommand::appendNode(NodeImpl *appendChild, NodeImpl *parent)
{
    EditCommandPtr cmd(new AppendNodeCommand(document(), appendChild, parent));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeFullySelectedNode(NodeImpl *node)
{
    if (isTableStructureNode(node)) {
        // Do not remove an element of table structure; remove its contents.
        NodeImpl *child = node->firstChild();
        while (child) {
            NodeImpl *remove = child;
            child = child->nextSibling();
            removeFullySelectedNode(remove);
        }
    }
    else {
        EditCommandPtr cmd(new RemoveNodeCommand(document(), node));
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommand::removeNode(NodeImpl *removeChild)
{
    EditCommandPtr cmd(new RemoveNodeCommand(document(), removeChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodePreservingChildren(NodeImpl *removeChild)
{
    EditCommandPtr cmd(new RemoveNodePreservingChildrenCommand(document(), removeChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitTextNode(TextImpl *text, long offset)
{
    EditCommandPtr cmd(new SplitTextNodeCommand(document(), text, offset));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::joinTextNodes(TextImpl *text1, TextImpl *text2)
{
    EditCommandPtr cmd(new JoinTextNodesCommand(document(), text1, text2));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::inputText(const DOMString &text, bool selectInsertedText)
{
    InputTextCommand *impl = new InputTextCommand(document());
    EditCommandPtr cmd(impl);
    applyCommandToComposite(cmd);
    impl->input(text, selectInsertedText);
}

void CompositeEditCommand::insertText(TextImpl *node, long offset, const DOMString &text)
{
    EditCommandPtr cmd(new InsertTextCommand(document(), node, offset, text));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::deleteText(TextImpl *node, long offset, long count)
{
    EditCommandPtr cmd(new DeleteTextCommand(document(), node, offset, count));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::replaceText(TextImpl *node, long offset, long count, const DOMString &replacementText)
{
    EditCommandPtr deleteCommand(new DeleteTextCommand(document(), node, offset, count));
    applyCommandToComposite(deleteCommand);
    EditCommandPtr insertCommand(new InsertTextCommand(document(), node, offset, replacementText));
    applyCommandToComposite(insertCommand);
}

void CompositeEditCommand::deleteSelection(bool smartDelete)
{
    if (endingSelection().isRange()) {
        EditCommandPtr cmd(new DeleteSelectionCommand(document(), smartDelete));
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommand::deleteSelection(const Selection &selection, bool smartDelete)
{
    if (selection.isRange()) {
        EditCommandPtr cmd(new DeleteSelectionCommand(document(), selection, smartDelete));
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommand::removeCSSProperty(CSSStyleDeclarationImpl *decl, int property)
{
    EditCommandPtr cmd(new RemoveCSSPropertyCommand(document(), decl, property));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodeAttribute(ElementImpl *element, int attribute)
{
    EditCommandPtr cmd(new RemoveNodeAttributeCommand(document(), element, attribute));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::setNodeAttribute(ElementImpl *element, int attribute, const DOMString &value)
{
    EditCommandPtr cmd(new SetNodeAttributeCommand(document(), element, attribute, value));
    applyCommandToComposite(cmd);
}

NodeImpl *CompositeEditCommand::applyTypingStyle(NodeImpl *child) const
{
    // FIXME: This function should share code with ApplyStyleCommand::applyStyleIfNeeded
    // and ApplyStyleCommand::computeStyleChange.
    // Both function do similar work, and the common parts could be factored out.

    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document()->updateLayout();

    StyleChange styleChange(document()->part()->typingStyle());

    NodeImpl *childToAppend = child;
    int exceptionCode = 0;

    if (styleChange.applyItalic()) {
        ElementImpl *italicElement = document()->createHTMLElement("I", exceptionCode);
        ASSERT(exceptionCode == 0);
        italicElement->appendChild(childToAppend, exceptionCode);
        ASSERT(exceptionCode == 0);
        childToAppend = italicElement;
    }

    if (styleChange.applyBold()) {
        ElementImpl *boldElement = document()->createHTMLElement("B", exceptionCode);
        ASSERT(exceptionCode == 0);
        boldElement->appendChild(childToAppend, exceptionCode);
        ASSERT(exceptionCode == 0);
        childToAppend = boldElement;
    }

    if (styleChange.cssStyle().length() > 0) {
        ElementImpl *styleElement = document()->createHTMLElement("SPAN", exceptionCode);
        ASSERT(exceptionCode == 0);
        styleElement->setAttribute(ATTR_STYLE, styleChange.cssStyle());
        styleElement->setAttribute(ATTR_CLASS, styleSpanClassString());
        styleElement->appendChild(childToAppend, exceptionCode);
        ASSERT(exceptionCode == 0);
        childToAppend = styleElement;
    }

    return childToAppend;
}

void CompositeEditCommand::deleteUnrenderedText(NodeImpl *node)
{
    if (!node)
        return;

    if (node->isTextNode()) {
        if (!node->renderer() || !static_cast<RenderText *>(node->renderer())->firstTextBox())
            removeNode(node);
        else {
            TextImpl *text = static_cast<TextImpl *>(node);
            if (text->caretMinOffset() > 0)
                deleteText(text, 0, text->caretMinOffset());
            if ((int)text->length() > text->caretMaxOffset())
                deleteText(text, text->caretMaxOffset(), text->length() - text->caretMaxOffset());
        }
    }
}

void CompositeEditCommand::deleteUnrenderedText(const Position &pos)
{
    if (pos.isNull())
        return;

    Position upstream = pos.upstream(StayInBlock);
    Position downstream = pos.downstream(StayInBlock);
    Position block = Position(pos.node()->enclosingBlockFlowElement(), 0);
    
    NodeImpl *node = upstream.node();
    while (node && node != downstream.node()) {
        NodeImpl *next = node->traverseNextNode();
        deleteUnrenderedText(node);
        node = next;
    }
    deleteUnrenderedText(downstream.node());
    
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
// AppendNodeCommand

AppendNodeCommand::AppendNodeCommand(DocumentImpl *document, NodeImpl *appendChild, NodeImpl *parentNode)
    : EditCommand(document), m_appendChild(appendChild), m_parentNode(parentNode)
{
    ASSERT(m_appendChild);
    m_appendChild->ref();

    ASSERT(m_parentNode);
    m_parentNode->ref();
}

AppendNodeCommand::~AppendNodeCommand()
{
    ASSERT(m_appendChild);
    m_appendChild->deref();

    ASSERT(m_parentNode);
    m_parentNode->deref();
}

void AppendNodeCommand::doApply()
{
    ASSERT(m_appendChild);
    ASSERT(m_parentNode);

    int exceptionCode = 0;
    m_parentNode->appendChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void AppendNodeCommand::doUnapply()
{
    ASSERT(m_appendChild);
    ASSERT(m_parentNode);
    ASSERT(state() == Applied);

    int exceptionCode = 0;
    m_parentNode->removeChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// ApplyStyleCommand

ApplyStyleCommand::ApplyStyleCommand(DocumentImpl *document, CSSStyleDeclarationImpl *style)
    : CompositeEditCommand(document), m_style(style)
{   
    ASSERT(m_style);
    m_style->ref();
}

ApplyStyleCommand::~ApplyStyleCommand()
{
    ASSERT(m_style);
    m_style->deref();
}

void ApplyStyleCommand::doApply()
{
    if (!endingSelection().isRange())
        return;

    // adjust to the positions we want to use for applying style
    Position start(endingSelection().start().downstream(StayInBlock).equivalentRangeCompliantPosition());
    Position end(endingSelection().end().upstream(StayInBlock));

    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document()->updateLayout();

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

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document()->updateLayout();
    
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
// ApplyStyleCommand: style-removal helpers

bool ApplyStyleCommand::isHTMLStyleNode(HTMLElementImpl *elem)
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

void ApplyStyleCommand::removeHTMLStyleNode(HTMLElementImpl *elem)
{
    // This node can be removed.
    // EDIT FIXME: This does not handle the case where the node
    // has attributes. But how often do people add attributes to <B> tags? 
    // Not so often I think.
    ASSERT(elem);
    removeNodePreservingChildren(elem);
}

void ApplyStyleCommand::removeCSSStyle(HTMLElementImpl *elem)
{
    ASSERT(elem);

    CSSStyleDeclarationImpl *decl = elem->inlineStyleDecl();
    if (!decl)
        return;

    for (QPtrListIterator<CSSProperty> it(*(style()->values())); it.current(); ++it) {
        CSSProperty *property = it.current();
        if (decl->getPropertyCSSValue(property->id()), DoNotUpdateLayout)
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

void ApplyStyleCommand::removeStyle(const Position &start, const Position &end)
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

bool ApplyStyleCommand::nodeFullySelected(const Position &start, const NodeImpl *node) const
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
// ApplyStyleCommand: style-application helpers


bool ApplyStyleCommand::splitTextAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > start.node()->caretMinOffset() && start.offset() < start.node()->caretMaxOffset()) {
        long endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
        TextImpl *text = static_cast<TextImpl *>(start.node());
        EditCommandPtr cmd(new SplitTextNodeCommand(document(), text, start.offset()));
        applyCommandToComposite(cmd);
        setEndingSelection(Selection(Position(start.node(), 0), Position(end.node(), end.offset() - endOffsetAdjustment)));
        return true;
    }
    return false;
}

NodeImpl *ApplyStyleCommand::splitTextAtEndIfNeeded(const Position &start, const Position &end)
{
    if (end.node()->isTextNode() && end.offset() > end.node()->caretMinOffset() && end.offset() < end.node()->caretMaxOffset()) {
        TextImpl *text = static_cast<TextImpl *>(end.node());
        SplitTextNodeCommand *impl = new SplitTextNodeCommand(document(), text, end.offset());
        EditCommandPtr cmd(impl);
        applyCommandToComposite(cmd);
        NodeImpl *startNode = start.node() == end.node() ? impl->node()->previousSibling() : start.node();
        ASSERT(startNode);
        setEndingSelection(Selection(Position(startNode, start.offset()), Position(impl->node()->previousSibling(), impl->node()->previousSibling()->caretMaxOffset())));
        return impl->node()->previousSibling();
    }
    return end.node();
}

void ApplyStyleCommand::surroundNodeRangeWithElement(NodeImpl *startNode, NodeImpl *endNode, ElementImpl *element)
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

void ApplyStyleCommand::applyStyleIfNeeded(NodeImpl *startNode, NodeImpl *endNode)
{
    // FIXME: This function should share code with CompositeEditCommand::applyTypingStyle.
    // Both functions do similar work, and the common parts could be factored out.

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

Position ApplyStyleCommand::positionInsertionPoint(Position pos)
{
    if (pos.node()->isTextNode() && (pos.offset() > 0 && pos.offset() < pos.node()->maxOffset())) {
        SplitTextNodeCommand *impl = new SplitTextNodeCommand(document(), static_cast<TextImpl *>(pos.node()), pos.offset());
        EditCommandPtr split(impl);
        split.apply();
        pos = Position(impl->node(), 0);
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
// DeleteSelectionCommand

DeleteSelectionCommand::DeleteSelectionCommand(DocumentImpl *document, bool smartDelete)
    : CompositeEditCommand(document), m_hasSelectionToDelete(false), m_smartDelete(smartDelete)
{
}

DeleteSelectionCommand::DeleteSelectionCommand(DocumentImpl *document, const Selection &selection, bool smartDelete)
    : CompositeEditCommand(document), m_selectionToDelete(selection), m_hasSelectionToDelete(true), m_smartDelete(smartDelete)
{
}

// This function moves nodes in the block containing startNode to dstBlock, starting
// from startNode and proceeding to the end of the block. Nodes in the block containing
// startNode that appear in document order before startNode are not moved.
// This function is an important helper for deleting selections that cross block
// boundaries.
void DeleteSelectionCommand::moveNodesAfterNode(NodeImpl *startNode, NodeImpl *dstNode)
{
    if (!startNode || !dstNode)
        return;

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    if (isTableStructureNode(startBlock))
        // Do not move content between parts of a table
        return;

    NodeImpl *node = startNode == startBlock ? startBlock->firstChild() : startNode;

    // Only do the move if node is a text node.
    // This is done to duplicate the behavior in NSText, particularly to
    // mimic the behavior in Mail where deletions are done between blocks
    // that are quoted.
    if (!node || !node->isTextNode())
        return;
    
    // Do the move.
    NodeImpl *refNode = dstNode;
    while (node && node->isAncestor(startBlock)) {
        NodeImpl *moveNode = node;
        node = node->nextSibling();
        removeNode(moveNode);
        insertNodeAfter(moveNode, refNode);
        refNode = moveNode;
    }

    // If the startBlock no longer has any kids, we may need to deal with adding a BR
    // to make the layout come out right. Consider this document:
    //
    // One
    // <div>Two</div>
    // Three
    // 
    // Placing the insertion before before the 'T' of 'Two' and hitting delete will
    // move the contents of the div to the block containing 'One' and delete the div.
    // This will have the side effect of moving 'Three' on to the same line as 'One'
    // and 'Two'. This is undesirable. We fix this up by adding a BR before the 'Three'.
    // This may not be ideal, but it is better than nothing.
    if (!startBlock->firstChild()) {
        removeNode(startBlock);
        document()->updateLayout();
        if (refNode->renderer() && refNode->renderer()->inlineBox() && refNode->renderer()->inlineBox()->nextOnLineExists()) {
            int exceptionCode = 0;
            ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
            ASSERT(exceptionCode == 0);
            insertNodeAfter(breakNode, refNode);
        }
    }
}

void DeleteSelectionCommand::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();
        
    if (!m_selectionToDelete.isRange())
        return;

    if (m_smartDelete) {
        if (!m_selectionToDelete.start().leadingWhitespacePosition().isNull()) {
            m_selectionToDelete.modify(Selection::EXTEND, Selection::LEFT, CHARACTER);
        } else if (!m_selectionToDelete.end().trailingWhitespacePosition().isNull()) {
            m_selectionToDelete.modify(Selection::EXTEND, Selection::RIGHT, CHARACTER);
        }
    }
    
    Position upstreamStart(m_selectionToDelete.start().upstream(StayInBlock));
    Position downstreamStart(m_selectionToDelete.start().downstream(StayInBlock));
    Position upstreamEnd(m_selectionToDelete.end().upstream(StayInBlock));
    Position downstreamEnd(m_selectionToDelete.end().downstream(StayInBlock));
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

    if (startBlock != endBlock) {
        // Delete some unrendered whitespace. This prepares the startBlock to
        // receive content that will be merged from endBlock. Do this before 
        // deleting, since deleting content can alter the notion of what 
        // should collapse away.
        // stay in this block and delete unrenderered text from the upstreamStart location
        deleteUnrenderedText(upstreamStart);
        Position upstreamInPreviousBlock(upstreamStart.upstream()); // Note no StayInBlock on upstream call.
        if (upstreamInPreviousBlock != upstreamStart)
            // cross blocks and delete unrenderered text from the upstream
            // position in startBlock. 
            deleteUnrenderedText(upstreamInPreviousBlock);
    }

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
            removeFullySelectedNode(startNode);
        }
        else if (downstreamEnd.offset() - startOffset > 0) {
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
                removeFullySelectedNode(node);
                node = nextNode;
            }
            else {
                NodeImpl *n = node->lastChild();
                while (n && n->lastChild())
                    n = n->lastChild();
                if (n == downstreamEnd.node() && downstreamEnd.offset() >= downstreamEnd.node()->caretMaxOffset()) {
                    NodeImpl *nextNode = node->traverseNextSibling();
                    removeFullySelectedNode(node);
                    node = nextNode;
                } 
                else {
                    node = node->traverseNextNode();
                }
            }
        }

        if (downstreamEnd.node() != startNode && downstreamEnd.node()->inDocument() && downstreamEnd.offset() >= downstreamEnd.node()->caretMinOffset()) {
            if (downstreamEnd.offset() >= downstreamEnd.node()->caretMaxOffset()) {
                // need to delete whole node
                // we can get here if this is the last node in the block
                removeFullySelectedNode(downstreamEnd.node());
                trailingValid = false;
            }
            else {
                // in a text node that needs to be trimmed
                TextImpl *text = static_cast<TextImpl *>(downstreamEnd.node());
                if (downstreamEnd.offset() > 0) {
                    deleteText(text, 0, downstreamEnd.offset());
                    trailingValid = false;
                }
            }
            if (!downstreamEnd.node()->inDocument() && downstreamEnd.node()->inDocument())
                endingPosition = Position(downstreamEnd.node(), 0);
        }
    }
    
    // Do block merge if start and end of selection are in different blocks.
    if (endBlock != startBlock && downstreamEnd.node()->inDocument()) {
        LOG(Editing,  "merging content from end block");
        moveNodesAfterNode(downstreamEnd.node(), upstreamStart.node());
    }
      
    // Figure out where the end position should be
    if (endingPosition.isNotNull())
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

    if (leading.isNotNull() || trailing.isNotNull())
        document()->updateLayout();

    debugPosition("endingPosition   ", endingPosition);
    
    if (leading.isNotNull() && !leading.isRenderedCharacter()) {
        LOG(Editing, "replace leading");
        TextImpl *textNode = static_cast<TextImpl *>(leading.node());
        replaceText(textNode, leading.offset(), 1, nonBreakingSpaceString());
    }

    if (trailing.isNotNull()) {
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
    if (startNode == endingPosition.node())
        document()->part()->setTypingStyle(0);
    else {
        CSSComputedStyleDeclarationImpl endingStyle(endingPosition.node());
        endingStyle.diff(style);
        if (!style->length()) {
            style->deref();
            style = 0;
        }
        document()->part()->setTypingStyle(style);
        setTypingStyle(style);
    }
    if (style)
        style->deref();
    setEndingSelection(endingPosition);
}

bool DeleteSelectionCommand::preservesTypingStyle() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

DeleteTextCommand::DeleteTextCommand(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditCommand(document), m_node(node), m_offset(offset), m_count(count)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(m_offset < (long)m_node->length());
    ASSERT(m_count >= 0);
    
    m_node->ref();
}

DeleteTextCommand::~DeleteTextCommand()
{
    ASSERT(m_node);
    m_node->deref();
}

void DeleteTextCommand::doApply()
{
    ASSERT(m_node);

    int exceptionCode = 0;
    m_text = m_node->substringData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    m_node->deleteData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void DeleteTextCommand::doUnapply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// InputNewlineCommand

InputNewlineCommand::InputNewlineCommand(DocumentImpl *document) 
    : CompositeEditCommand(document)
{
}

void InputNewlineCommand::insertNodeAfterPosition(NodeImpl *node, const Position &pos)
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

void InputNewlineCommand::insertNodeBeforePosition(NodeImpl *node, const Position &pos)
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

void InputNewlineCommand::doApply()
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
    bool atEndOfBlock = VisiblePosition(pos).isLastInBlock();
    
    if (atEndOfBlock) {
        LOG(Editing, "input newline case 1");
        // Check for a trailing BR. If there isn't one, we'll need to insert an "extra" one.
        // This makes the "real" BR we want to insert appear in the rendering without any 
        // significant side effects (and no real worries either since you can't arrow past 
        // this extra one.
        if (pos.node()->id() == ID_BR && pos.offset() == 0) {
            // Already placed in a trailing BR. Insert "real" BR before it and leave the selection alone.
            insertNodeBefore(nodeToInsert, pos.node());
        }
        else {
            NodeImpl *next = pos.node()->traverseNextNode();
            bool hasTrailingBR = next && next->id() == ID_BR && pos.node()->enclosingBlockFlowElement() == next->enclosingBlockFlowElement();
            insertNodeAfterPosition(nodeToInsert, pos);
            if (hasTrailingBR) {
                setEndingSelection(Position(next, 0));
            }
            else {
                // Insert an "extra" BR at the end of the block. 
                ElementImpl *extraBreakNode = document()->createHTMLElement("BR", exceptionCode);
                ASSERT(exceptionCode == 0);
                insertNodeAfter(extraBreakNode, nodeToInsert);
                setEndingSelection(Position(extraBreakNode, 0));
            }
        }
    }
    else if (atStart) {
        LOG(Editing, "input newline case 2");
        // Insert node before downstream position, and place caret there as well. 
        Position endingPosition = pos.downstream(StayInBlock);
        insertNodeBeforePosition(nodeToInsert, endingPosition);
        setEndingSelection(endingPosition);
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
        if (trailing.isNotNull() && !endingPosition.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            deleteUnrenderedText(endingPosition);
            insertText(textNode, 0, nonBreakingSpaceString());
        }
        
        setEndingSelection(endingPosition);
    }
}

//------------------------------------------------------------------------------------------
// InputTextCommand

InputTextCommand::InputTextCommand(DocumentImpl *document) 
    : CompositeEditCommand(document), m_charactersAdded(0)
{
}

void InputTextCommand::doApply()
{
}

void InputTextCommand::deleteCharacter()
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

Position InputTextCommand::prepareForTextInsertion(bool adjustDownstream)
{
    // Prepare for text input by looking at the current position.
    // It may be necessary to insert a text node to receive characters.
    Selection selection = endingSelection();
    ASSERT(selection.isCaret());
    
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
        else if (pos.node()->caretMinOffset() == pos.offset()) {
            LOG(Editing, "prepareForTextInsertion case 2");
            insertNodeBefore(nodeToInsert, pos.node());
        }
        else if (pos.node()->caretMaxOffset() == pos.offset()) {
            LOG(Editing, "prepareForTextInsertion case 3");
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
                SplitTextNodeCommand *impl = new SplitTextNodeCommand(document(), text, pos.offset());
                EditCommandPtr cmd(impl);
                applyCommandToComposite(cmd);
                setEndingSelection(Position(impl->node(), 0));
            }
            
            TextImpl *editingTextNode = document()->createEditingTextNode("");
            NodeImpl *node = endingSelection().start().upstream(StayInBlock).node();
            if (node->isBlockFlow())
                insertNodeAt(applyTypingStyle(editingTextNode), node, 0);
            else
                insertNodeAfter(applyTypingStyle(editingTextNode), node);
            pos = Position(editingTextNode, 0);
        }
    }
    return pos;
}

void InputTextCommand::input(const DOMString &text, bool selectInsertedText)
{
    Selection selection = endingSelection();
    bool adjustDownstream = selection.start().downstream(StayInBlock).isFirstRenderedPositionOnLine();

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.isRange())
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
        if (selectInsertedText)
            setEndingSelection(Selection(Position(textNode, offset), Position(textNode, offset + spacesPerTab)));
        else
            setEndingSelection(Position(textNode, offset + spacesPerTab));
        m_charactersAdded += spacesPerTab;
    }
    else if (isWS(text)) {
        insertSpace(textNode, offset);
        if (selectInsertedText)
            setEndingSelection(Selection(Position(textNode, offset), Position(textNode, offset + 1)));
        else
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
        if (selectInsertedText)
            setEndingSelection(Selection(Position(textNode, offset), Position(textNode, offset + text.length())));
        else
            setEndingSelection(Position(textNode, offset + text.length()));
        m_charactersAdded += text.length();
    }
}

void InputTextCommand::insertSpace(TextImpl *textNode, unsigned long offset)
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

bool InputTextCommand::isInputTextCommand() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommand

InsertNodeBeforeCommand::InsertNodeBeforeCommand(DocumentImpl *document, NodeImpl *insertChild, NodeImpl *refChild)
    : EditCommand(document), m_insertChild(insertChild), m_refChild(refChild)
{
    ASSERT(m_insertChild);
    m_insertChild->ref();

    ASSERT(m_refChild);
    m_refChild->ref();
}

InsertNodeBeforeCommand::~InsertNodeBeforeCommand()
{
    ASSERT(m_insertChild);
    m_insertChild->deref();

    ASSERT(m_refChild);
    m_refChild->deref();
}

void InsertNodeBeforeCommand::doApply()
{
    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parentNode());

    int exceptionCode = 0;
    m_refChild->parentNode()->insertBefore(m_insertChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertNodeBeforeCommand::doUnapply()
{
    ASSERT(m_insertChild);
    ASSERT(m_refChild);
    ASSERT(m_refChild->parentNode());

    int exceptionCode = 0;
    m_refChild->parentNode()->removeChild(m_insertChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// InsertTextCommand

InsertTextCommand::InsertTextCommand(DocumentImpl *document, TextImpl *node, long offset, const DOMString &text)
    : EditCommand(document), m_node(node), m_offset(offset)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    
    m_node->ref();
    m_text = text.copy(); // make a copy to ensure that the string never changes
}

InsertTextCommand::~InsertTextCommand()
{
    if (m_node)
        m_node->deref();
}

void InsertTextCommand::doApply()
{
    ASSERT(m_node);

    if (m_text.isEmpty())
	return;

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertTextCommand::doUnapply()
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
// JoinTextNodesCommand

JoinTextNodesCommand::JoinTextNodesCommand(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : EditCommand(document), m_text1(text1), m_text2(text2)
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);
    ASSERT(m_text1->length() > 0);
    ASSERT(m_text2->length() > 0);

    m_text1->ref();
    m_text2->ref();
}

JoinTextNodesCommand::~JoinTextNodesCommand()
{
    ASSERT(m_text1);
    m_text1->deref();
    ASSERT(m_text2);
    m_text2->deref();
}

void JoinTextNodesCommand::doApply()
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

void JoinTextNodesCommand::doUnapply()
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
// ReplaceSelectionCommand

ReplaceSelectionCommand::ReplaceSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, bool selectReplacement, bool smartReplace) 
    : CompositeEditCommand(document), m_fragment(fragment), m_selectReplacement(selectReplacement), m_smartReplace(smartReplace)
{
    ASSERT(m_fragment);
    m_fragment->ref();
}

ReplaceSelectionCommand::~ReplaceSelectionCommand()
{
    ASSERT(m_fragment);
    m_fragment->deref();
}

void ReplaceSelectionCommand::doApply()
{
    NodeImpl *firstChild = m_fragment->firstChild();
    NodeImpl *lastChild = m_fragment->lastChild();

    Selection selection = endingSelection();

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.isRange())
        deleteSelection();
    
    // This command does not use any typing style that is set as a residual effect of
    // a delete.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    document()->part()->clearTypingStyle();
    setTypingStyle(0);
    
    selection = endingSelection();
    ASSERT(selection.isCaret());
    
    bool addLeadingSpace = false;
    bool addTrailingSpace = false;
    if (m_smartReplace) {
        addLeadingSpace = selection.start().leadingWhitespacePosition().isNull();
        addTrailingSpace = selection.start().trailingWhitespacePosition().isNull();
    }
    
    if (!firstChild) {
        // Pasting something that didn't parse or was empty.
        ASSERT(!lastChild);
    } else if (firstChild == lastChild && firstChild->isTextNode()) {
        // FIXME: HTML fragment case needs to be improved to the point
        // where we can remove this separate case.
        
        // Simple text paste. Treat as if the text were typed.
        Position upstreamStart(selection.start().upstream(StayInBlock));
        DOMString text = static_cast<TextImpl *>(firstChild)->data();
        if (addLeadingSpace) {
            text = " " + text;
        }
        if (addTrailingSpace) {
            text += " ";
        }
        inputText(text, m_selectReplacement);
    } 
    else {
        // HTML fragment paste.
        
        // FIXME: Add leading and trailing spaces to the fragment?
        // Or just insert them as we insert it?
        
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
        }
    }
}

//------------------------------------------------------------------------------------------
// MoveSelectionCommand

MoveSelectionCommand::MoveSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, Position &position, bool smartMove) 
    : CompositeEditCommand(document), m_fragment(fragment), m_position(position), m_smartMove(smartMove)
{
    ASSERT(m_fragment);
    m_fragment->ref();
}

MoveSelectionCommand::~MoveSelectionCommand()
{
    ASSERT(m_fragment);
    m_fragment->deref();
}

void MoveSelectionCommand::doApply()
{
    Selection selection = endingSelection();
    ASSERT(selection.isRange());

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
    
    deleteSelection(m_smartMove);

    setEndingSelection(Position(positionNode, positionOffset));
    EditCommandPtr cmd(new ReplaceSelectionCommand(document(), m_fragment, true, m_smartMove));
    applyCommandToComposite(cmd);
}

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommand

RemoveCSSPropertyCommand::RemoveCSSPropertyCommand(DocumentImpl *document, CSSStyleDeclarationImpl *decl, int property)
    : EditCommand(document), m_decl(decl), m_property(property), m_important(false)
{
    ASSERT(m_decl);
    m_decl->ref();
}

RemoveCSSPropertyCommand::~RemoveCSSPropertyCommand()
{
    ASSERT(m_decl);
    m_decl->deref();
}

void RemoveCSSPropertyCommand::doApply()
{
    ASSERT(m_decl);

    m_oldValue = m_decl->getPropertyValue(m_property);
    ASSERT(!m_oldValue.isNull());

    m_important = m_decl->getPropertyPriority(m_property);
    m_decl->removeProperty(m_property);
}

void RemoveCSSPropertyCommand::doUnapply()
{
    ASSERT(m_decl);
    ASSERT(!m_oldValue.isNull());

    m_decl->setProperty(m_property, m_oldValue, m_important);
}

//------------------------------------------------------------------------------------------
// RemoveNodeAttributeCommand

RemoveNodeAttributeCommand::RemoveNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute)
    : EditCommand(document), m_element(element), m_attribute(attribute)
{
    ASSERT(m_element);
    m_element->ref();
}

RemoveNodeAttributeCommand::~RemoveNodeAttributeCommand()
{
    ASSERT(m_element);
    m_element->deref();
}

void RemoveNodeAttributeCommand::doApply()
{
    ASSERT(m_element);

    m_oldValue = m_element->getAttribute(m_attribute);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->removeAttribute(m_attribute, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeAttributeCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->setAttribute(m_attribute, m_oldValue.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// RemoveNodeCommand

RemoveNodeCommand::RemoveNodeCommand(DocumentImpl *document, NodeImpl *removeChild)
    : EditCommand(document), m_parent(0), m_removeChild(removeChild), m_refChild(0)
{
    ASSERT(m_removeChild);
    m_removeChild->ref();

    m_parent = m_removeChild->parentNode();
    ASSERT(m_parent);
    m_parent->ref();
    
    m_refChild = m_removeChild->nextSibling();
    if (m_refChild)
        m_refChild->ref();
}

RemoveNodeCommand::~RemoveNodeCommand()
{
    ASSERT(m_parent);
    m_parent->deref();

    ASSERT(m_removeChild);
    m_removeChild->deref();

    if (m_refChild)
        m_refChild->deref();
}

void RemoveNodeCommand::doApply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->removeChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeCommand::doUnapply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->insertBefore(m_removeChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommand

RemoveNodePreservingChildrenCommand::RemoveNodePreservingChildrenCommand(DocumentImpl *document, NodeImpl *node)
    : CompositeEditCommand(document), m_node(node)
{
    ASSERT(m_node);
    m_node->ref();
}

RemoveNodePreservingChildrenCommand::~RemoveNodePreservingChildrenCommand()
{
    ASSERT(m_node);
    m_node->deref();
}

void RemoveNodePreservingChildrenCommand::doApply()
{
    while (NodeImpl* curr = node()->firstChild()) {
        removeNode(curr);
        insertNodeBefore(curr, node());
    }
    removeNode(node());
}

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommand

SetNodeAttributeCommand::SetNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute, const DOMString &value)
    : EditCommand(document), m_element(element), m_attribute(attribute), m_value(value)
{
    ASSERT(m_element);
    m_element->ref();
    ASSERT(!m_value.isNull());
}

SetNodeAttributeCommand::~SetNodeAttributeCommand()
{
    ASSERT(m_element);
    m_element->deref();
}

void SetNodeAttributeCommand::doApply()
{
    ASSERT(m_element);
    ASSERT(!m_value.isNull());

    int exceptionCode = 0;
    m_oldValue = m_element->getAttribute(m_attribute);
    m_element->setAttribute(m_attribute, m_value.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

void SetNodeAttributeCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->setAttribute(m_attribute, m_oldValue.implementation(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// SplitTextNodeCommand

SplitTextNodeCommand::SplitTextNodeCommand(DocumentImpl *document, TextImpl *text, long offset)
    : EditCommand(document), m_text1(0), m_text2(text), m_offset(offset)
{
    ASSERT(m_text2);
    ASSERT(m_text2->length() > 0);

    m_text2->ref();
}

SplitTextNodeCommand::~SplitTextNodeCommand()
{
    if (m_text1)
        m_text1->deref();

    ASSERT(m_text2);
    m_text2->deref();
}

void SplitTextNodeCommand::doApply()
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

void SplitTextNodeCommand::doUnapply()
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
// TypingCommand

TypingCommand::TypingCommand(DocumentImpl *document, ETypingCommand commandType, const DOMString &textToInsert, bool selectInsertedText)
    : CompositeEditCommand(document), m_commandType(commandType), m_textToInsert(textToInsert), m_openForMoreTyping(true), m_applyEditing(false), m_selectInsertedText(selectInsertedText)
{
}

void TypingCommand::deleteKeyPressed(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->deleteKeyPressed();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, DeleteKey));
        cmd.apply();
    }
}

void TypingCommand::insertText(DocumentImpl *document, const DOMString &text, bool selectInsertedText)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertText(text, selectInsertedText);
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertText, text, selectInsertedText));
        cmd.apply();
    }
}

void TypingCommand::insertNewline(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertNewline();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertNewline));
        cmd.apply();
    }
}

bool TypingCommand::isOpenForMoreTypingCommand(const EditCommandPtr &cmd)
{
    return cmd.isTypingCommand() &&
        static_cast<const TypingCommand *>(cmd.get())->openForMoreTyping();
}

void TypingCommand::closeTyping(const EditCommandPtr &cmd)
{
    if (isOpenForMoreTypingCommand(cmd))
        static_cast<TypingCommand *>(cmd.get())->closeTyping();
}

void TypingCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    switch (m_commandType) {
        case DeleteKey:
            deleteKeyPressed();
            return;
        case InsertText:
            insertText(m_textToInsert, m_selectInsertedText);
            return;
        case InsertNewline:
            insertNewline();
            return;
    }

    ASSERT_NOT_REACHED();
}

void TypingCommand::markMisspellingsAfterTyping()
{
    // Take a look at the selection that results after typing and determine whether we need to spellcheck. 
    // Since the word containing the current selection is never marked, this does a check to
    // see if typing made a new word that is not in the current selection. Basically, you
    // get this by being at the end of a word and typing a space.    
    VisiblePosition start(endingSelection().start());
    VisiblePosition previous = start.previous();
    if (previous.isNotNull()) {
        VisiblePosition p1 = startOfWord(previous, LeftWordIfOnBoundary);
        VisiblePosition p2 = startOfWord(start, LeftWordIfOnBoundary);
        if (p1 != p2)
            KWQ(document()->part())->markMisspellingsInAdjacentWords(p1);
    }
}

void TypingCommand::typingAddedToOpenCommand()
{
    markMisspellingsAfterTyping();
    // Do not apply editing to the part on the first time through.
    // The part will get told in the same way as all other commands.
    // But since this command stays open and is used for additional typing, 
    // we need to tell the part here as other commands are added.
    if (m_applyEditing) {
        EditCommandPtr cmd(this);
        document()->part()->appliedEditing(cmd);
    }
    m_applyEditing = true;
}

void TypingCommand::insertText(const DOMString &text, bool selectInsertedText)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (document()->part()->typingStyle() || m_cmds.count() == 0) {
        InputTextCommand *impl = new InputTextCommand(document());
        EditCommandPtr cmd(impl);
        applyCommandToComposite(cmd);
        impl->input(text, selectInsertedText);
    }
    else {
        EditCommandPtr lastCommand = m_cmds.last();
        if (lastCommand.isInputTextCommand()) {
            InputTextCommand *impl = static_cast<InputTextCommand *>(lastCommand.get());
            impl->input(text, selectInsertedText);
        }
        else {
            InputTextCommand *impl = new InputTextCommand(document());
            EditCommandPtr cmd(impl);
            applyCommandToComposite(cmd);
            impl->input(text, selectInsertedText);
        }
    }
    typingAddedToOpenCommand();
}

void TypingCommand::insertNewline()
{
    EditCommandPtr cmd(new InputNewlineCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::issueCommandForDeleteKey()
{
    Selection selectionToDelete;
    
    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            break;
        case Selection::CARET: {
            // Handle delete at beginning-of-block case.
            // Do nothing in the case that the caret is at the start of a
            // root editable element or at the start of a document.
            Position pos(endingSelection().start());
            Position start = VisiblePosition(pos).previous().deepEquivalent();
            Position end = VisiblePosition(pos).deepEquivalent();
            if (start.isNotNull() && end.isNotNull() && start.node()->rootEditableElement() == end.node()->rootEditableElement())
                selectionToDelete = Selection(start, end);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange()) {
        deleteSelection(selectionToDelete);
        typingAddedToOpenCommand();
    }
}

void TypingCommand::deleteKeyPressed()
{
// EDIT FIXME: The ifdef'ed out code below should be re-enabled.
// In order for this to happen, the deleteCharacter case
// needs work. Specifically, the caret-positioning code
// and whitespace-handling code in DeleteSelectionCommand::doApply()
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
        EditCommandPtr lastCommand = m_cmds.last();
        if (lastCommand.isInputTextCommand()) {
            InputTextCommand &cmd = static_cast<InputTextCommand &>(lastCommand);
            cmd.deleteCharacter();
            if (cmd.charactersAdded() == 0) {
                removeCommand(lastCommand);
            }
        }
        else if (lastCommand.isInputNewlineCommand()) {
            lastCommand.unapply();
            removeCommand(lastCommand);
        }
        else {
            issueCommandForDeleteKey();
        }
    }
#endif
}

void TypingCommand::removeCommand(const EditCommandPtr &cmd)
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

bool TypingCommand::preservesTypingStyle() const
{
    switch (m_commandType) {
        case DeleteKey:
            return true;
        case InsertText:
        case InsertNewline:
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool TypingCommand::isTypingCommand() const
{
    return true;
}

} // namespace khtml
