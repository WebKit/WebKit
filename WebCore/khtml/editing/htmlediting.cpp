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
#include "html_interchange.h"
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
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::CSSPrimitiveValue;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSProperty;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSValueImpl;
using DOM::DocumentFragmentImpl;
using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::DoNotStayInBlock;
using DOM::DoNotUpdateLayout;
using DOM::EditingTextImpl;
using DOM::ElementImpl;
using DOM::EStayInBlock;
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
#define debugNode(a,b) ((void)0)
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

static DOMString &blockPlaceholderClassString()
{
    static DOMString blockPlaceholderClassString = "khtml-block-placeholder";
    return blockPlaceholderClassString;
}

static void derefNodesInList(QPtrList<NodeImpl> &list)
{
    for (QPtrListIterator<NodeImpl> it(list); it.current(); ++it)
        it.current()->deref();
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

static void debugNode(const char *prefix, const NodeImpl *node)
{
    if (!prefix)
        prefix = "";
    if (!node)
        LOG(Editing, "%s <null>", prefix);
    else
        LOG(Editing, "%s%s %p", prefix, getTagName(node->id()).string().latin1(), node);
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

bool EditCommandPtr::isInsertTextCommand() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isInsertTextCommand();
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

CSSMutableStyleDeclarationImpl *EditCommandPtr::typingStyle() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->typingStyle();
}

void EditCommandPtr::setTypingStyle(CSSMutableStyleDeclarationImpl *style) const
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

StyleChange::StyleChange(CSSStyleDeclarationImpl *style, ELegacyHTMLStyles usesLegacyStyles)
    : m_applyBold(false), m_applyItalic(false), m_usesLegacyStyles(usesLegacyStyles)
{
    init(style, Position());
}

StyleChange::StyleChange(CSSStyleDeclarationImpl *style, const Position &position, ELegacyHTMLStyles usesLegacyStyles)
    : m_applyBold(false), m_applyItalic(false), m_usesLegacyStyles(usesLegacyStyles)
{
    init(style, position);
}

void StyleChange::init(CSSStyleDeclarationImpl *style, const Position &position)
{
    style->ref();
    CSSMutableStyleDeclarationImpl *mutableStyle = style->makeMutable();
    mutableStyle->ref();
    style->deref();
    
    QString styleText("");

    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = mutableStyle->valuesIterator(); it != end; ++it) {
        const CSSProperty *property = &*it;

        // If position is empty or the position passed in already has the 
        // style, just move on.
        if (position.isNotNull() && currentlyHasStyle(position, property))
            continue;
        
        // If needed, figure out if this change is a legacy HTML style change.
        if (m_usesLegacyStyles && checkForLegacyHTMLStyleChange(property))
            continue;

        // Add this property
        styleText += property->cssText().string();
    }

    mutableStyle->deref();

    // Save the result for later
    m_cssStyle = styleText.stripWhiteSpace();
}

bool StyleChange::checkForLegacyHTMLStyleChange(const DOM::CSSProperty *property)
{
    DOMString valueText(property->value()->cssText());
    switch (property->id()) {
        case CSS_PROP_FONT_WEIGHT:
            if (strcasecmp(valueText, "bold") == 0) {
                m_applyBold = true;
                return true;
            }
            break;
        case CSS_PROP_FONT_STYLE:
            if (strcasecmp(valueText, "italic") == 0 || strcasecmp(valueText, "oblique") == 0) {
                m_applyItalic = true;
                return true;
            }
            break;
    }
    return false;
}

bool StyleChange::currentlyHasStyle(const Position &pos, const CSSProperty *property)
{
    ASSERT(pos.isNotNull());
    CSSComputedStyleDeclarationImpl *style = pos.computedStyle();
    ASSERT(style);
    style->ref();
    CSSValueImpl *value = style->getPropertyCSSValue(property->id(), DoNotUpdateLayout);
    style->deref();
    if (!value)
        return false;
    value->ref();
    bool result = strcasecmp(value->cssText(), property->value()->cssText()) == 0;
    value->deref();
    return result;
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
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setEndingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::assignTypingStyle(CSSMutableStyleDeclarationImpl *style)
{
    if (m_typingStyle == style)
        return;
        
    CSSMutableStyleDeclarationImpl *old = m_typingStyle;
    m_typingStyle = style;
    if (m_typingStyle)
        m_typingStyle->ref();
    if (old)
        old->deref();
}

void EditCommand::setTypingStyle(CSSMutableStyleDeclarationImpl *style)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->assignTypingStyle(style);
}

bool EditCommand::preservesTypingStyle() const
{
    return false;
}

bool EditCommand::isInsertTextCommand() const
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

void CompositeEditCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
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
    InsertTextCommand *impl = new InsertTextCommand(document());
    EditCommandPtr cmd(impl);
    applyCommandToComposite(cmd);
    impl->input(text, selectInsertedText);
}

void CompositeEditCommand::insertTextIntoNode(TextImpl *node, long offset, const DOMString &text)
{
    EditCommandPtr cmd(new InsertIntoTextNode(document(), node, offset, text));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::deleteTextFromNode(TextImpl *node, long offset, long count)
{
    EditCommandPtr cmd(new DeleteFromTextNodeCommand(document(), node, offset, count));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::replaceTextInNode(TextImpl *node, long offset, long count, const DOMString &replacementText)
{
    EditCommandPtr deleteCommand(new DeleteFromTextNodeCommand(document(), node, offset, count));
    applyCommandToComposite(deleteCommand);
    EditCommandPtr insertCommand(new InsertIntoTextNode(document(), node, offset, replacementText));
    applyCommandToComposite(insertCommand);
}

void CompositeEditCommand::deleteSelection(bool smartDelete, bool mergeBlocksAfterDelete)
{
    if (endingSelection().isRange()) {
        EditCommandPtr cmd(new DeleteSelectionCommand(document(), smartDelete, mergeBlocksAfterDelete));
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommand::deleteSelection(const Selection &selection, bool smartDelete, bool mergeBlocksAfterDelete)
{
    if (selection.isRange()) {
        EditCommandPtr cmd(new DeleteSelectionCommand(document(), selection, smartDelete, mergeBlocksAfterDelete));
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

void CompositeEditCommand::deleteInsignificantText(TextImpl *textNode, int start, int end)
{
    if (!textNode || !textNode->renderer() || start >= end)
        return;

    RenderText *textRenderer = static_cast<RenderText *>(textNode->renderer());
    InlineTextBox *box = textRenderer->firstTextBox();
    if (!box) {
        // whole text node is empty
        removeNode(textNode);
        return;    
    }
    
    long length = textNode->length();
    if (start >= length || end > length)
        return;

    int removed = 0;
    InlineTextBox *prevBox = 0;
    DOMStringImpl *str = 0;

    // This loop structure works to process all gaps preceding a box,
    // and also will look at the gap after the last box.
    while (prevBox || box) {
        int gapStart = prevBox ? prevBox->m_start + prevBox->m_len : 0;
        if (end < gapStart)
            // No more chance for any intersections
            break;

        int gapEnd = box ? box->m_start : length;
        bool indicesIntersect = start <= gapEnd && end >= gapStart;
        int gapLen = gapEnd - gapStart;
        if (indicesIntersect && gapLen > 0) {
            gapStart = kMax(gapStart, start);
            gapEnd = kMin(gapEnd, end);
            if (!str) {
                str = textNode->string()->substring(start, end - start);
                str->ref();
            }    
            // remove text in the gap
            str->remove(gapStart - start - removed, gapLen);
            removed += gapLen;
        }
        
        prevBox = box;
        if (box)
            box = box->nextTextBox();
    }

    if (str) {
        // Replace the text between start and end with our pruned version.
        if (str->l > 0) {
            replaceTextInNode(textNode, start, end - start, str);
        }
        else {
            // Assert that we are not going to delete all of the text in the node.
            // If we were, that should have been done above with the call to 
            // removeNode and return.
            ASSERT(start > 0 || (unsigned long)end - start < textNode->length());
            deleteTextFromNode(textNode, start, end - start);
        }
        str->deref();
    }
}

void CompositeEditCommand::deleteInsignificantText(const Position &start, const Position &end)
{
    if (start.isNull() || end.isNull())
        return;

    if (RangeImpl::compareBoundaryPoints(start, end) >= 0)
        return;

    NodeImpl *node = start.node();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
    
        if (node->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(node);
            bool isStartNode = node == start.node();
            bool isEndNode = node == end.node();
            int startOffset = isStartNode ? start.offset() : 0;
            int endOffset = isEndNode ? end.offset() : textNode->length();
            deleteInsignificantText(textNode, startOffset, endOffset);
        }
            
        if (node == end.node())
            break;
        node = next;
    }
}

void CompositeEditCommand::deleteInsignificantTextDownstream(const DOM::Position &pos)
{
    Position end = VisiblePosition(pos).next().deepEquivalent().downstream(StayInBlock);
    deleteInsignificantText(pos, end);
}

void CompositeEditCommand::insertBlockPlaceholderIfNeeded(NodeImpl *node)
{
    document()->updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer->isBlockFlow())
        return;
    
    if (renderer->height() > 0)
        return;

    int exceptionCode = 0;
    ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
    ASSERT(exceptionCode == 0);
    breakNode->setAttribute(ATTR_CLASS, blockPlaceholderClassString());
    appendNode(breakNode, node);
}

bool CompositeEditCommand::removeBlockPlaceholderIfNeeded(NodeImpl *node)
{
    document()->updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer->isBlockFlow())
        return false;

    // This code will remove a block placeholder if it still is at the end
    // of a block, where we placed it in insertBlockPlaceholderIfNeeded().
    // Of course, a person who hand-edits an HTML file could move a 
    // placeholder around, but it seems OK to be unconcerned about that case.
    NodeImpl *last = node->lastChild();
    if (last && last->isHTMLElement()) {
        ElementImpl *element = static_cast<ElementImpl *>(last);
        if (element->getAttribute(ATTR_CLASS) == blockPlaceholderClassString()) {
            removeNode(element);
            return true;
        }
    }
    return false;
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
    : CompositeEditCommand(document), m_style(style->makeMutable())
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
    // apply the block-centric properties of the style
    CSSMutableStyleDeclarationImpl *blockStyle = m_style->copyBlockProperties();
    blockStyle->ref();
    applyBlockStyle(blockStyle);

    // apply any remaining styles to the inline elements
    // NOTE: hopefully, this string comparison is the same as checking for a non-null diff
    if (blockStyle->length() < m_style->length()) {
        CSSMutableStyleDeclarationImpl *inlineStyle = m_style->copy();
        inlineStyle->ref();
        blockStyle->diff(inlineStyle);
        applyInlineStyle(inlineStyle);
        inlineStyle->deref();
    }

    blockStyle->deref();
    
    setEndingSelectionNeedsLayout();
}

void ApplyStyleCommand::applyBlockStyle(CSSMutableStyleDeclarationImpl *style)
{
    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document()->updateLayout();

    // get positions we want to use for applying style
    Position start(endingSelection().start());
    Position end(endingSelection().end());
    
    // remove current values, if any, of the specified styles from the blocks
    // NOTE: tracks the previous block to avoid repeated processing
    NodeImpl *beyondEnd = end.node()->traverseNextNode();
    NodeImpl *prevBlock = 0;
    for (NodeImpl *node = start.node(); node != beyondEnd; node = node->traverseNextNode()) {
        NodeImpl *block = node->enclosingBlockFlowElement();
        if (block != prevBlock && block->isHTMLElement()) {
            removeCSSStyle(style, static_cast<HTMLElementImpl *>(block));
            prevBlock = block;
        }
    }
    
    // apply specified styles to the block flow elements in the selected range
    prevBlock = 0;
    for (NodeImpl *node = start.node(); node != beyondEnd; node = node->traverseNextNode()) {
        NodeImpl *block = node->enclosingBlockFlowElement();
        if (block != prevBlock && block->isHTMLElement()) {
            addBlockStyleIfNeeded(style, static_cast<HTMLElementImpl *>(block));
            prevBlock = block;
        }
    }
}

void ApplyStyleCommand::applyInlineStyle(CSSMutableStyleDeclarationImpl *style)
{
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
    removeInlineStyle(style, start.upstream(), end);
    
    // split the start node if the selection starts inside of it
    bool splitStart = splitTextAtStartIfNeeded(start, end); 
    if (splitStart) {
        start = endingSelection().start();
        end = endingSelection().end();
    }

    // split the end node if the selection ends inside of it
    splitTextAtEndIfNeeded(start, end);
    start = endingSelection().start();
    end = endingSelection().end();

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document()->updateLayout();
    
    if (start.node() == end.node()) {
        // simple case...start and end are the same node
        addInlineStyleIfNeeded(style, start.node(), end.node());
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
                addInlineStyleIfNeeded(style, runStart, node);
            }
            if (node == end.node())
                break;
            node = node->traverseNextNode();
        }
    }
}

//------------------------------------------------------------------------------------------
// ApplyStyleCommand: style-removal helpers

bool ApplyStyleCommand::isHTMLStyleNode(CSSMutableStyleDeclarationImpl *style, HTMLElementImpl *elem)
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = style->valuesIterator(); it != end; ++it) {
        switch ((*it).id()) {
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

void ApplyStyleCommand::removeCSSStyle(CSSMutableStyleDeclarationImpl *style, HTMLElementImpl *elem)
{
    ASSERT(style);
    ASSERT(elem);

    CSSMutableStyleDeclarationImpl *decl = elem->inlineStyleDecl();
    if (!decl)
        return;

    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = style->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        CSSValueImpl *value = decl->getPropertyCSSValue(propertyID);
        if (value) {
            value->ref();
            removeCSSProperty(decl, propertyID);
            value->deref();
        }
    }

    if (elem->id() == ID_SPAN && elem->renderer() && elem->renderer()->isInline()) {
        // Check to see if the span is one we added to apply style.
        // If it is, and there are no more attributes on the span other than our
        // class marker, remove the span.
        if (decl->length() == 0) {
            removeNodeAttribute(elem, ATTR_STYLE);
            NamedAttrMapImpl *map = elem->attributes();
            if (map && map->length() == 1 && elem->getAttribute(ATTR_CLASS) == styleSpanClassString())
                removeNodePreservingChildren(elem);
        }
    }
}

void ApplyStyleCommand::removeBlockStyle(CSSMutableStyleDeclarationImpl *style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(RangeImpl::compareBoundaryPoints(start, end) <= 0);
    
}

void ApplyStyleCommand::removeInlineStyle(CSSMutableStyleDeclarationImpl *style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(RangeImpl::compareBoundaryPoints(start, end) <= 0);
    
    NodeImpl *node = start.node();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (node->isHTMLElement() && nodeFullySelected(node, start, end)) {
            HTMLElementImpl *elem = static_cast<HTMLElementImpl *>(node);
            if (isHTMLStyleNode(style, elem))
                removeHTMLStyleNode(elem);
            else
                removeCSSStyle(style, elem);
        }
        if (node == end.node())
            break;
        node = next;
    }
}

bool ApplyStyleCommand::nodeFullySelected(NodeImpl *node, const Position &start, const Position &end) const
{
    ASSERT(node);

    Position pos = Position(node, node->childNodeCount()).upstream();
    return RangeImpl::compareBoundaryPoints(node, 0, start.node(), start.offset()) >= 0 &&
        RangeImpl::compareBoundaryPoints(pos, end) <= 0;
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

void ApplyStyleCommand::addBlockStyleIfNeeded(CSSMutableStyleDeclarationImpl *style, HTMLElementImpl *block)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for
    // inline content.
    StyleChange styleChange(style, Position(block, 0), StyleChange::DoNotUseLegacyHTMLStyles);
    if (styleChange.cssStyle().length() > 0) {
        DOMString cssText = styleChange.cssStyle();
        CSSMutableStyleDeclarationImpl *decl = block->inlineStyleDecl();
        if (decl)
            cssText += decl->cssText();
        block->setAttribute(ATTR_STYLE, cssText);
    }
}

void ApplyStyleCommand::addInlineStyleIfNeeded(CSSMutableStyleDeclarationImpl *style, NodeImpl *startNode, NodeImpl *endNode)
{
    // FIXME: This function should share code with CompositeEditCommand::applyTypingStyle.
    // Both functions do similar work, and the common parts could be factored out.

    StyleChange styleChange(style, Position(startNode, 0));
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
// DeleteFromTextNodeCommand

DeleteFromTextNodeCommand::DeleteFromTextNodeCommand(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditCommand(document), m_node(node), m_offset(offset), m_count(count)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(m_offset < (long)m_node->length());
    ASSERT(m_count >= 0);
    
    m_node->ref();
}

DeleteFromTextNodeCommand::~DeleteFromTextNodeCommand()
{
    ASSERT(m_node);
    m_node->deref();
}

void DeleteFromTextNodeCommand::doApply()
{
    ASSERT(m_node);

    int exceptionCode = 0;
    m_text = m_node->substringData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    m_node->deleteData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void DeleteFromTextNodeCommand::doUnapply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// DeleteSelectionCommand

DeleteSelectionCommand::DeleteSelectionCommand(DocumentImpl *document, bool smartDelete, bool mergeBlocksAfterDelete)
    : CompositeEditCommand(document), 
      m_hasSelectionToDelete(false), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_startBlock(0),
      m_endBlock(0),
      m_startNode(0),
      m_typingStyle(0)
{
}

DeleteSelectionCommand::DeleteSelectionCommand(DocumentImpl *document, const Selection &selection, bool smartDelete, bool mergeBlocksAfterDelete)
    : CompositeEditCommand(document), 
      m_hasSelectionToDelete(true), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_selectionToDelete(selection),
      m_startBlock(0),
      m_endBlock(0),
      m_startNode(0),
      m_typingStyle(0)
{
}

void DeleteSelectionCommand::initializePositionData()
{
    //
    // Handle setting some basic positions
    //
    Position start = m_selectionToDelete.start();
    Position end = m_selectionToDelete.end();

    m_upstreamStart = start.upstream(StayInBlock);
    m_downstreamStart = start.downstream(StayInBlock);
    m_upstreamEnd = end.upstream(StayInBlock);
    m_downstreamEnd = end.downstream(StayInBlock);

    //
    // Handle leading and trailing whitespace, as well as smart delete adjustments to the selection
    //
    m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition();
    bool hasLeadingWhitespaceBeforeAdjustment = m_leadingWhitespace.isNotNull();
    if (m_smartDelete && hasLeadingWhitespaceBeforeAdjustment) {
        Position pos = VisiblePosition(start).previous().deepEquivalent();
        // Expand out one character upstream for smart delete and recalculate
        // positions based on this change.
        m_upstreamStart = pos.upstream(StayInBlock);
        m_downstreamStart = pos.downstream(StayInBlock);
        m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition();
    }
    m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition();
    // Note: trailing whitespace is only considered for smart delete if there is no leading
    // whitespace, as in the case where you double-click the first word of a paragraph.
    if (m_smartDelete && !hasLeadingWhitespaceBeforeAdjustment && m_trailingWhitespace.isNotNull()) {
        // Expand out one character downstream for smart delete and recalculate
        // positions based on this change.
        Position pos = VisiblePosition(end).next().deepEquivalent();
        m_upstreamEnd = pos.upstream(StayInBlock);
        m_downstreamEnd = pos.downstream(StayInBlock);
        m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition();
    }
    m_trailingWhitespaceValid = true;
    
    //
    // Handle setting start and end blocks and the start node.
    //
    m_startBlock = m_downstreamStart.node()->enclosingBlockFlowElement();
    m_startBlock->ref();
    m_endBlock = m_upstreamEnd.node()->enclosingBlockFlowElement();
    m_endBlock->ref();
    m_startNode = m_upstreamStart.node();
    m_startNode->ref();

    //
    // Handle detecting if the line containing the selection end is itself fully selected.
    // This is one of the tests that determines if block merging of content needs to be done.
    //
    VisiblePosition visibleEnd(end);
    if (isFirstVisiblePositionOnLine(visibleEnd)) {
        Position previousLineStart = previousLinePosition(visibleEnd, DOWNSTREAM, 0).deepEquivalent();
        if (previousLineStart.isNull() || RangeImpl::compareBoundaryPoints(previousLineStart, m_downstreamStart) >= 0)
            m_mergeBlocksAfterDelete = false;
    }

    debugPosition("m_upstreamStart      ", m_upstreamStart);
    debugPosition("m_downstreamStart    ", m_downstreamStart);
    debugPosition("m_upstreamEnd        ", m_upstreamEnd);
    debugPosition("m_downstreamEnd      ", m_downstreamEnd);
    debugPosition("m_leadingWhitespace  ", m_leadingWhitespace);
    debugPosition("m_trailingWhitespace ", m_trailingWhitespace);
    debugNode(    "m_startBlock         ", m_startBlock);
    debugNode(    "m_endBlock           ", m_endBlock);    
    debugNode(    "m_startNode          ", m_startNode);    
}

void DeleteSelectionCommand::saveTypingStyleState()
{
    // Figure out the typing style in effect before the delete is done.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSComputedStyleDeclarationImpl *computedStyle = m_selectionToDelete.start().computedStyle();
    computedStyle->ref();
    m_typingStyle = computedStyle->copyInheritableProperties();
    m_typingStyle->ref();
    computedStyle->deref();
}

bool DeleteSelectionCommand::handleSpecialCaseAllContentDelete()
{
    Position start = m_downstreamStart;
    Position end = m_upstreamEnd;

    ElementImpl *rootElement = start.node()->rootEditableElement();
    Position rootStart = Position(rootElement, 0);
    Position rootEnd = Position(rootElement, rootElement ? rootElement->childNodeCount() : 0).equivalentDeepPosition();
    if (start == VisiblePosition(rootStart).downstreamDeepEquivalent() && end == VisiblePosition(rootEnd).deepEquivalent()) {
        // Delete every child of the root editable element
        NodeImpl *node = rootElement->firstChild();
        while (node) {
            NodeImpl *next = node->traverseNextSibling();
            removeNode(node);
            node = next;
        }
        return true;
    }
    return false;
}

bool DeleteSelectionCommand::handleSpecialCaseBRDelete()
{
    // Check for special-case where the selection contains only a BR on a line by itself after another BR.
    bool upstreamStartIsBR = m_startNode->id() == ID_BR;
    bool downstreamStartIsBR = m_downstreamStart.node()->id() == ID_BR;
    bool isBROnLineByItself = upstreamStartIsBR && downstreamStartIsBR && m_downstreamStart.node() == m_upstreamEnd.node();
    if (isBROnLineByItself) {
        removeNode(m_downstreamStart.node());
        m_endingPosition = m_upstreamStart;
        m_mergeBlocksAfterDelete = false;
        return true;
    }

    // Check for special-case where the selection contains only a BR right after a block ended.
    bool downstreamEndIsBR = m_downstreamEnd.node()->id() == ID_BR;
    Position upstreamFromBR = m_downstreamEnd.upstream();
    bool startIsBRAfterBlock = downstreamEndIsBR && m_downstreamEnd.node()->enclosingBlockFlowElement() != upstreamFromBR.node()->enclosingBlockFlowElement();
    if (startIsBRAfterBlock) {
        removeNode(m_downstreamEnd.node());
        m_endingPosition = upstreamFromBR;
        m_mergeBlocksAfterDelete = false;
        return true;
    }

    // Not a special-case delete per se, but we can detect that the merging of content between blocks
    // should not be done.
    if (upstreamStartIsBR && downstreamStartIsBR)
        m_mergeBlocksAfterDelete = false;

    return false;
}

void DeleteSelectionCommand::handleGeneralDelete()
{
    int startOffset = m_upstreamStart.offset();

    if (startOffset == 0 && m_startNode->isBlockFlow() && m_startBlock != m_endBlock && !m_endBlock->isAncestor(m_startBlock)) {
        // The block containing the start of the selection is completely selected. 
        // Delete it all in one step right here.
        ASSERT(!m_downstreamEnd.node()->isAncestor(m_startNode));

        // shift the start node to the start of the next block.
        NodeImpl *old = m_startNode;
        m_startNode = m_startBlock->traverseNextSibling();
        m_startNode->ref();
        old->deref();
        startOffset = 0;

        removeFullySelectedNode(m_startBlock);
    }
    else if (startOffset >= m_startNode->caretMaxOffset()) {
        // Move the start node to the next node in the tree since the startOffset is equal to
        // or beyond the start node's caretMaxOffset This means there is nothing visible to delete. 
        // However, before moving on, delete any insignificant text that may be present in a text node.
        if (m_startNode->isTextNode()) {
            // Delete any insignificant text from this node.
            TextImpl *text = static_cast<TextImpl *>(m_startNode);
            if (text->length() > (unsigned)m_startNode->caretMaxOffset())
                deleteTextFromNode(text, m_startNode->caretMaxOffset(), text->length() - m_startNode->caretMaxOffset());
        }
        
        // shift the start node to the next
        NodeImpl *old = m_startNode;
        m_startNode = old->traverseNextNode();
        m_startNode->ref();
        old->deref();
        startOffset = 0;
    }

    if (m_startNode == m_downstreamEnd.node()) {
        // The selection to delete is all in one node.
        if (!m_startNode->renderer() || 
            (startOffset <= m_startNode->caretMinOffset() && m_downstreamEnd.offset() >= m_startNode->caretMaxOffset())) {
            // just delete
            removeFullySelectedNode(m_startNode);
        }
        else if (m_downstreamEnd.offset() - startOffset > 0) {
            // in a text node that needs to be trimmed
            TextImpl *text = static_cast<TextImpl *>(m_startNode);
            deleteTextFromNode(text, startOffset, m_downstreamEnd.offset() - startOffset);
            m_trailingWhitespaceValid = false;
        }
    }
    else {
        // The selection to delete spans more than one node.
        NodeImpl *node = m_startNode;
        
        if (startOffset > 0) {
            // in a text node that needs to be trimmed
            TextImpl *text = static_cast<TextImpl *>(node);
            deleteTextFromNode(text, startOffset, text->length() - startOffset);
            node = node->traverseNextNode();
        }
        
        // handle deleting all nodes that are completely selected
        while (node && node != m_downstreamEnd.node()) {
            if (!m_downstreamEnd.node()->isAncestor(node)) {
                NodeImpl *nextNode = node->traverseNextSibling();
                removeFullySelectedNode(node);
                node = nextNode;
            }
            else {
                NodeImpl *n = node->lastChild();
                while (n && n->lastChild())
                    n = n->lastChild();
                if (n == m_downstreamEnd.node() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMaxOffset()) {
                    // remove an ancestor of m_downstreamEnd.node(), and thus m_downstreamEnd.node() itself
                    removeFullySelectedNode(node);
                    m_trailingWhitespaceValid = false;
                    node = 0;
                } 
                else {
                    node = node->traverseNextNode();
                }
            }
        }

        if (m_downstreamEnd.node() != m_startNode && m_downstreamEnd.node()->inDocument() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMinOffset()) {
            if (m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMaxOffset()) {
                // need to delete whole node
                // we can get here if this is the last node in the block
                removeFullySelectedNode(m_downstreamEnd.node());
                m_trailingWhitespaceValid = false;
            }
            else {
                // in a text node that needs to be trimmed
                TextImpl *text = static_cast<TextImpl *>(m_downstreamEnd.node());
                if (m_downstreamEnd.offset() > 0) {
                    deleteTextFromNode(text, 0, m_downstreamEnd.offset());
                    m_downstreamEnd = Position(text, 0);
                    m_trailingWhitespaceValid = false;
                }
            }
        }
    }
}

void DeleteSelectionCommand::fixupWhitespace()
{
    document()->updateLayout();
    if (m_leadingWhitespace.isNotNull() && (m_trailingWhitespace.isNotNull() || !m_leadingWhitespace.isRenderedCharacter())) {
        LOG(Editing, "replace leading");
        TextImpl *textNode = static_cast<TextImpl *>(m_leadingWhitespace.node());
        replaceTextInNode(textNode, m_leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    else if (m_trailingWhitespace.isNotNull()) {
        if (m_trailingWhitespaceValid) {
            if (!m_trailingWhitespace.isRenderedCharacter()) {
                LOG(Editing, "replace trailing [valid]");
                TextImpl *textNode = static_cast<TextImpl *>(m_trailingWhitespace.node());
                replaceTextInNode(textNode, m_trailingWhitespace.offset(), 1, nonBreakingSpaceString());
            }
        }
        else {
            Position pos = m_endingPosition.downstream(StayInBlock);
            pos = Position(pos.node(), pos.offset() - 1);
            if (isWS(pos) && !pos.isRenderedCharacter()) {
                LOG(Editing, "replace trailing [invalid]");
                TextImpl *textNode = static_cast<TextImpl *>(pos.node());
                replaceTextInNode(textNode, pos.offset(), 1, nonBreakingSpaceString());
                // need to adjust ending position since the trailing position is not valid.
                m_endingPosition = pos;
            }
        }
    }
}

// This function moves nodes in the block containing startNode to dstBlock, starting
// from startNode and proceeding to the end of the block. Nodes in the block containing
// startNode that appear in document order before startNode are not moved.
// This function is an important helper for deleting selections that cross block
// boundaries.
void DeleteSelectionCommand::moveNodesAfterNode()
{
    if (!m_mergeBlocksAfterDelete)
        return;

    if (m_endBlock == m_startBlock)
        return;

    NodeImpl *startNode = m_downstreamEnd.node();
    NodeImpl *dstNode = m_upstreamStart.node();

    if (!startNode->inDocument() || !dstNode->inDocument())
        return;

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    if (isTableStructureNode(startBlock))
        // Do not move content between parts of a table
        return;

    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    removeBlockPlaceholderIfNeeded(startBlock);

    // Move the subtree containing node
    NodeImpl *node = startNode->enclosingInlineElement();

    // Insert after the subtree containing destNode
    NodeImpl *refNode = dstNode->enclosingInlineElement();

    // Nothing to do if start is already at the beginning of dstBlock
    NodeImpl *dstBlock = refNode->enclosingBlockFlowElement();
    if (startBlock == dstBlock->firstChild())
        return;

    // Do the move.
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
    document()->updateLayout();
    if (!startBlock->renderer() || !startBlock->renderer()->firstChild()) {
        removeNode(startBlock);
        if (refNode->renderer() && refNode->renderer()->inlineBox() && refNode->renderer()->inlineBox()->nextOnLineExists()) {
            int exceptionCode = 0;
            ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
            ASSERT(exceptionCode == 0);
            insertNodeAfter(breakNode, refNode);
        }
    }
}

void DeleteSelectionCommand::calculateEndingPosition()
{
    if (m_endingPosition.isNotNull() && m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = m_upstreamStart;
    if (m_endingPosition.node()->inDocument())
        return;
    
    m_endingPosition = m_downstreamEnd;
    if (m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = Position(m_startBlock, 0);
    if (m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = Position(m_endBlock, 0);
    if (m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = Position(document()->documentElement(), 0);
}

void DeleteSelectionCommand::calculateTypingStyleAfterDelete()
{
    // Compute the difference between the style before the delete and the style now
    // after the delete has been done. Set this style on the part, so other editing
    // commands being composed with this one will work, and also cache it on the command,
    // so the KHTMLPart::appliedEditing can set it after the whole composite command 
    // has completed.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (m_startNode == m_endingPosition.node())
        document()->part()->setTypingStyle(0);
    else {
        CSSComputedStyleDeclarationImpl endingStyle(m_endingPosition.node());
        endingStyle.diff(m_typingStyle);
        if (!m_typingStyle->length()) {
            m_typingStyle->deref();
            m_typingStyle = 0;
        }
        document()->part()->setTypingStyle(m_typingStyle);
        setTypingStyle(m_typingStyle);
    }
}

void DeleteSelectionCommand::clearTransientState()
{
    m_selectionToDelete.clear();
    m_upstreamStart.clear();
    m_downstreamStart.clear();
    m_upstreamEnd.clear();
    m_downstreamEnd.clear();
    m_endingPosition.clear();
    m_leadingWhitespace.clear();
    m_trailingWhitespace.clear();

    if (m_startBlock) {
        m_startBlock->deref();
        m_startBlock = 0;
    }
    if (m_endBlock) {
        m_endBlock->deref();
        m_endBlock = 0;
    }
    if (m_startNode) {
        m_startNode->deref();
        m_startNode = 0;
    }
    if (m_typingStyle) {
        m_typingStyle->deref();
        m_typingStyle = 0;
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

    initializePositionData();

    if (!m_startBlock || !m_endBlock) {
        // Can't figure out what blocks we're in. This can happen if
        // the document structure is not what we are expecting, like if
        // the document has no body element, or if the editable block
        // has been changed to display: inline. Some day it might
        // be nice to be able to deal with this, but for now, bail.
        clearTransientState();
        return;
    }

    // Delete any text that may hinder our ability to fixup whitespace after the detele
    deleteInsignificantTextDownstream(m_trailingWhitespace);    

    saveTypingStyleState();
    
    if (!handleSpecialCaseAllContentDelete())
        if (!handleSpecialCaseBRDelete())
            handleGeneralDelete();
    
    // Do block merge if start and end of selection are in different blocks.
    moveNodesAfterNode();
    
    calculateEndingPosition();
    fixupWhitespace();

    // If the delete emptied a block, add in a placeholder so the block does not
    // seem to disappear.
    insertBlockPlaceholderIfNeeded(m_endingPosition.node());
    calculateTypingStyleAfterDelete();
    setEndingSelection(m_endingPosition);
    debugPosition("endingPosition   ", m_endingPosition);
    clearTransientState();
}

bool DeleteSelectionCommand::preservesTypingStyle() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// InsertIntoTextNode

InsertIntoTextNode::InsertIntoTextNode(DocumentImpl *document, TextImpl *node, long offset, const DOMString &text)
    : EditCommand(document), m_node(node), m_offset(offset)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(!text.isEmpty());
    
    m_node->ref();
    m_text = text.copy(); // make a copy to ensure that the string never changes
}

InsertIntoTextNode::~InsertIntoTextNode()
{
    if (m_node)
        m_node->deref();
}

void InsertIntoTextNode::doApply()
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void InsertIntoTextNode::doUnapply()
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->deleteData(m_offset, m_text.length(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// InsertLineBreakCommand

InsertLineBreakCommand::InsertLineBreakCommand(DocumentImpl *document) 
    : CompositeEditCommand(document)
{
}

void InsertLineBreakCommand::insertNodeAfterPosition(NodeImpl *node, const Position &pos)
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

void InsertLineBreakCommand::insertNodeBeforePosition(NodeImpl *node, const Position &pos)
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

void InsertLineBreakCommand::doApply()
{
    deleteSelection();
    Selection selection = endingSelection();

    int exceptionCode = 0;
    ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
    ASSERT(exceptionCode == 0);

    NodeImpl *nodeToInsert = breakNode;
    
    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    if (typingStyle && typingStyle->length() > 0)
        nodeToInsert = applyTypingStyle(breakNode);
    
    Position pos(selection.start().upstream(StayInBlock));
    bool atStart = pos.offset() <= pos.node()->caretMinOffset();
    bool atEnd = pos.offset() >= pos.node()->caretMaxOffset();
    bool atEndOfBlock = isLastVisiblePositionInBlock(VisiblePosition(pos));
    
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
        
        // Do the split
        TextImpl *textNode = static_cast<TextImpl *>(pos.node());
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, selection.start().offset(), exceptionCode));
        deleteTextFromNode(textNode, 0, pos.offset());
        insertNodeBefore(textBeforeNode, textNode);
        insertNodeBefore(nodeToInsert, textNode);
        Position endingPosition = Position(textNode, 0);
        
        // Handle whitespace that occurs after the split
        document()->updateLayout();
        if (!endingPosition.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            deleteInsignificantTextDownstream(endingPosition);
            insertTextIntoNode(textNode, 0, nonBreakingSpaceString());
        }
        
        setEndingSelection(endingPosition);
    }
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
// InsertParagraphSeparatorCommand

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(DocumentImpl *document) 
    : CompositeEditCommand(document)
{
}

InsertParagraphSeparatorCommand::~InsertParagraphSeparatorCommand() 
{
    derefNodesInList(clonedNodes);
}

void InsertParagraphSeparatorCommand::doApply()
{
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    // Delete the current selection.
    // If the selection is a range and the start and end nodes are in different blocks, 
    // then this command bails after the delete, but takes the one additional step of
    // moving the selection downstream so it is in the ending block (if that block is
    // still around, that is).
    Position pos = selection.start();
    if (selection.isRange()) {
        NodeImpl *startBlockBeforeDelete = selection.start().node()->enclosingBlockFlowElement();
        NodeImpl *endBlockBeforeDelete = selection.end().node()->enclosingBlockFlowElement();
        bool doneAfterDelete = startBlockBeforeDelete != endBlockBeforeDelete;
        deleteSelection(false, false);
        if (doneAfterDelete) {
            document()->updateLayout();
            setEndingSelection(endingSelection().start().downstream());
            return;
        }
        pos = endingSelection().start().upstream();
    }
    
    // Find the start block.
    NodeImpl *startNode = pos.node();
    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    if (!startBlock || !startBlock->parentNode())
        return;

    // Build up list of ancestors in between the start node and the start block.
    for (NodeImpl *n = startNode->parentNode(); n && n != startBlock; n = n->parentNode())
        ancestors.prepend(n);

    // Make new block to represent the newline.
    // If the start block is the body, just make a P tag, otherwise, make a shallow clone
    // of the the start block.
    NodeImpl *addedBlock = 0;
    if (startBlock->id() == ID_BODY) {
        int exceptionCode = 0;
        addedBlock = document()->createHTMLElement("P", exceptionCode);
        ASSERT(exceptionCode == 0);
        appendNode(addedBlock, startBlock);
    }
    else {
        addedBlock = startBlock->cloneNode(false);
        insertNodeAfter(addedBlock, startBlock);
    }
    addedBlock->ref();
    clonedNodes.append(addedBlock);

    if (!isLastVisiblePositionInNode(VisiblePosition(pos), startBlock)) {
        // Split at pos if in the middle of a text node.
        if (startNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(startNode);
            bool atEnd = (unsigned long)pos.offset() >= textNode->length();
            if (pos.offset() > 0 && !atEnd) {
                SplitTextNodeCommand *splitCommand = new SplitTextNodeCommand(document(), textNode, pos.offset());
                EditCommandPtr cmd(splitCommand);
                applyCommandToComposite(cmd);
                startNode = splitCommand->node();
                pos = Position(startNode, 0);
            }
            else if (atEnd) {
                startNode = startNode->traverseNextNode();
                ASSERT(startNode);
            }
        }
        else if (pos.offset() > 0) {
            startNode = startNode->traverseNextNode();
            ASSERT(startNode);
        }

        // Make clones of ancestors in between the start node and the start block.
        NodeImpl *parent = addedBlock;
        for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
            NodeImpl *child = it.current()->cloneNode(false); // shallow clone
            child->ref();
            clonedNodes.append(child);
            appendNode(child, parent);
            parent = child;
        }

        // Move the start node and the siblings of the start node.
        NodeImpl *n = startNode;
        if (n->id() == ID_BR)
            n = n->nextSibling();
        while (n && n != addedBlock) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent);
            n = next;
        }
        
        // Move everything after the start node.
        NodeImpl *leftParent = ancestors.last();
        while (leftParent && leftParent != startBlock) {
            parent = parent->parentNode();
            NodeImpl *n = leftParent->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
            leftParent = leftParent->parentNode();
        }
    }
    
    // Put the selection right at the start of the added block.
    setEndingSelection(Position(addedBlock, 0));
}

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorInQuotedContentCommand

InsertParagraphSeparatorInQuotedContentCommand::InsertParagraphSeparatorInQuotedContentCommand(DocumentImpl *document)
    : CompositeEditCommand(document)
{
}

InsertParagraphSeparatorInQuotedContentCommand::~InsertParagraphSeparatorInQuotedContentCommand()
{
    derefNodesInList(clonedNodes);
    if (m_breakNode)
        m_breakNode->deref();
}

bool InsertParagraphSeparatorInQuotedContentCommand::isMailBlockquote(const NodeImpl *node) const
{
    if (!node || !node->renderer() || !node->isElementNode() && node->id() != ID_BLOCKQUOTE)
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("type") == "cite";
}

void InsertParagraphSeparatorInQuotedContentCommand::doApply()
{
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    // Delete the current selection.
    Position pos = selection.start();
    if (selection.isRange()) {
        deleteSelection(false, false);
        pos = endingSelection().start().upstream();
    }
    
    // Find the top-most blockquote from the start.
    NodeImpl *startNode = pos.node();
    NodeImpl *topBlockquote = 0;
    for (NodeImpl *n = startNode->parentNode(); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            topBlockquote = n;
    }
    if (!topBlockquote || !topBlockquote->parentNode())
        return;

    // Build up list of ancestors in between the start node and the top blockquote.
    for (NodeImpl *n = startNode->parentNode(); n && n != topBlockquote; n = n->parentNode())
        ancestors.prepend(n);

    // Insert a break after the top blockquote.
    int exceptionCode = 0;
    m_breakNode = document()->createHTMLElement("BR", exceptionCode);
    m_breakNode->ref();
    ASSERT(exceptionCode == 0);
    insertNodeAfter(m_breakNode, topBlockquote);

    if (!isLastVisiblePositionInNode(VisiblePosition(pos), topBlockquote)) {
        // Split at pos if in the middle of a text node.
        if (startNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(startNode);
            bool atEnd = (unsigned long)pos.offset() >= textNode->length();
            if (pos.offset() > 0 && !atEnd) {
                SplitTextNodeCommand *splitCommand = new SplitTextNodeCommand(document(), textNode, pos.offset());
                EditCommandPtr cmd(splitCommand);
                applyCommandToComposite(cmd);
                startNode = splitCommand->node();
                pos = Position(startNode, 0);
            }
            else if (atEnd) {
                startNode = startNode->traverseNextNode();
                ASSERT(startNode);
            }
        }
        else if (pos.offset() > 0) {
            startNode = startNode->traverseNextNode();
            ASSERT(startNode);
        }

        // Insert a clone of the top blockquote after the break.
        NodeImpl *clonedBlockquote = topBlockquote->cloneNode(false);
        clonedBlockquote->ref();
        clonedNodes.append(clonedBlockquote);
        insertNodeAfter(clonedBlockquote, m_breakNode);
        
        // Make clones of ancestors in between the start node and the top blockquote.
        NodeImpl *parent = clonedBlockquote;
        for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
            NodeImpl *child = it.current()->cloneNode(false); // shallow clone
            child->ref();
            clonedNodes.append(child);
            appendNode(child, parent);
            parent = child;
        }

        // Move the start node and the siblings of the start node.
        NodeImpl *n = startNode;
        bool startIsBR = n->id() == ID_BR;
        if (startIsBR)
            n = n->nextSibling();
        while (n) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent);
            n = next;
        }
        
        // Move everything after the start node.
        NodeImpl *leftParent = ancestors.last();

        if (!startIsBR) {
            if (!leftParent)
                leftParent = topBlockquote;
            ElementImpl *b = document()->createHTMLElement("BR", exceptionCode);
            b->ref();
            clonedNodes.append(b);
            ASSERT(exceptionCode == 0);
            appendNode(b, leftParent);
        }
        
        leftParent = ancestors.last();
        while (leftParent && leftParent != topBlockquote) {
            parent = parent->parentNode();
            NodeImpl *n = leftParent->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
            leftParent = leftParent->parentNode();
        }
    }
    
    // Put the selection right before the break.
    setEndingSelection(Position(m_breakNode, 0));
}

//------------------------------------------------------------------------------------------
// InsertTextCommand

InsertTextCommand::InsertTextCommand(DocumentImpl *document) 
    : CompositeEditCommand(document), m_charactersAdded(0)
{
}

void InsertTextCommand::doApply()
{
}

void InsertTextCommand::deleteCharacter()
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

Position InsertTextCommand::prepareForTextInsertion(bool adjustDownstream)
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
        CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
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
        CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
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

void InsertTextCommand::input(const DOMString &text, bool selectInsertedText)
{
    Selection selection = endingSelection();
    bool adjustDownstream = selection.start().downstream(StayInBlock).isFirstRenderedPositionOnLine();

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.isRange())
        deleteSelection();
    
    // Delete any insignificant text that could get in the way of whitespace turning
    // out correctly after the insertion.
    deleteInsignificantTextDownstream(endingSelection().end().trailingWhitespacePosition());
    
    // Make sure the document is set up to receive text
    Position pos = prepareForTextInsertion(adjustDownstream);
    
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    long offset = pos.offset();

    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    removeBlockPlaceholderIfNeeded(textNode->enclosingBlockFlowElement());
    
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
            replaceTextInNode(textNode, offset - 1, 1, " ");
        }
        insertTextIntoNode(textNode, offset, text);
        if (selectInsertedText)
            setEndingSelection(Selection(Position(textNode, offset), Position(textNode, offset + text.length())));
        else
            setEndingSelection(Position(textNode, offset + text.length()));
        m_charactersAdded += text.length();
    }
}

void InsertTextCommand::insertSpace(TextImpl *textNode, unsigned long offset)
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
            deleteTextFromNode(textNode, offset, count);
    }

    if (offset > 0 && offset <= text.length() - 1 && !isWS(text[offset]) && !isWS(text[offset - 1])) {
        // insert a "regular" space
        insertTextIntoNode(textNode, offset, " ");
        return;
    }

    if (text.length() >= 2 && offset >= 2 && isNBSP(text[offset - 2]) && isNBSP(text[offset - 1])) {
        // DOM looks like this:
        // nbsp nbsp caret
        // insert a space between the two nbsps
        insertTextIntoNode(textNode, offset - 1, " ");
        return;
    }

    // insert an nbsp
    insertTextIntoNode(textNode, offset, nonBreakingSpaceString());
}

bool InsertTextCommand::isInsertTextCommand() const
{
    return true;
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
    : EditCommand(document), m_decl(decl->makeMutable()), m_property(property), m_important(false)
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
// ReplaceSelectionCommand

ReplacementFragment::ReplacementFragment(DocumentFragmentImpl *fragment)
    : m_fragment(fragment), m_hasInterchangeNewlineComment(false), m_hasMoreThanOneBlock(false)
{
    if (!m_fragment) {
        m_type = EmptyFragment;
        return;
    }

    m_fragment->ref();

    NodeImpl *firstChild = m_fragment->firstChild();
    NodeImpl *lastChild = m_fragment->lastChild();

    if (!firstChild) {
        m_type = EmptyFragment;
        return;
    }

    if (firstChild == lastChild && firstChild->isTextNode()) {
        m_type = SingleTextNodeFragment;
        return;
    }
    
    m_type = TreeFragment;

    NodeImpl *node = firstChild;
    int realBlockCount = 0;
    NodeImpl *commentToDelete = 0;
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (isInterchangeNewlineComment(node)) {
            m_hasInterchangeNewlineComment = true;
            commentToDelete = node;
        }
        else if (isInterchangeConvertedSpaceSpan(node)) {
            NodeImpl *n = 0;
            while ((n = node->firstChild())) {
                n->ref();
                removeNode(n);
                insertNodeBefore(n, node);
                n->deref();
            }
            removeNode(node);
            if (n)
                next = n->traverseNextNode();
        }
        else if (isProbablyBlock(node))
            realBlockCount++;    
        node = next;
    }

    if (commentToDelete)
        removeNode(commentToDelete);

    int blockCount = realBlockCount;
    firstChild = m_fragment->firstChild();
    lastChild = m_fragment->lastChild();
    if (!isProbablyBlock(firstChild))
        blockCount++;
    if (!isProbablyBlock(lastChild) && realBlockCount > 0)
        blockCount++;

     if (blockCount > 1)
        m_hasMoreThanOneBlock = true;
}

ReplacementFragment::~ReplacementFragment()
{
    if (m_fragment)
        m_fragment->deref();
}

NodeImpl *ReplacementFragment::firstChild() const 
{ 
    return m_fragment->firstChild(); 
}

NodeImpl *ReplacementFragment::lastChild() const 
{ 
    return  m_fragment->lastChild(); 
}

NodeImpl *ReplacementFragment::mergeStartNode() const
{
    NodeImpl *node = m_fragment->firstChild();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (!isProbablyBlock(node))
            return node;
        node = next;
     }
     return 0;
}

NodeImpl *ReplacementFragment::mergeEndNode() const
{
    NodeImpl *node = m_fragment->lastChild();
    while (node && node->lastChild())
        node = node->lastChild();
    while (node) {
        NodeImpl *prev = node->traversePreviousNode();
        if (!isProbablyBlock(node)) {
            NodeImpl *previousSibling = node->previousSibling();
            while (1) {
                if (!previousSibling || isProbablyBlock(previousSibling))
                    return node;
                node = previousSibling;
                previousSibling = node->previousSibling();
            }
        }
        node = prev;
    }
    return 0;
}

void ReplacementFragment::pruneEmptyNodes()
{
    bool run = true;
    while (run) {
        run = false;
        NodeImpl *node = m_fragment->firstChild();
        while (node) {
            if ((node->isTextNode() && static_cast<TextImpl *>(node)->length() == 0) ||
                (isProbablyBlock(node) && node->childNodeCount() == 0)) {
                NodeImpl *next = node->traverseNextSibling();
                removeNode(node);
                node = next;
                run = true;
            }
            else {
                node = node->traverseNextNode();
            }
         }
    }
}

bool ReplacementFragment::isInterchangeNewlineComment(const NodeImpl *node)
{
    return isComment(node) && node->nodeValue() == KHTMLInterchangeNewline;
}

bool ReplacementFragment::isInterchangeConvertedSpaceSpan(const NodeImpl *node)
{
    static DOMString convertedSpaceSpanClass(AppleConvertedSpace);
    return node->isHTMLElement() && static_cast<const HTMLElementImpl *>(node)->getAttribute(ATTR_CLASS) == convertedSpaceSpanClass;
}

void ReplacementFragment::removeNode(NodeImpl *node)
{
    if (!node)
        return;
        
    NodeImpl *parent = node->parentNode();
    if (!parent)
        return;
        
    int exceptionCode = 0;
    parent->removeChild(node, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void ReplacementFragment::insertNodeBefore(NodeImpl *node, NodeImpl *refNode)
{
    if (!node || !refNode)
        return;
        
    NodeImpl *parent = refNode->parentNode();
    if (!parent)
        return;
        
    int exceptionCode = 0;
    parent->insertBefore(node, refNode, exceptionCode);
    ASSERT(exceptionCode == 0);
 }


bool isComment(const NodeImpl *node)
{
    return node && node->nodeType() == Node::COMMENT_NODE;
}

bool isProbablyBlock(const NodeImpl *node)
{
    if (!node)
        return false;
    
    switch (node->id()) {
        case ID_BLOCKQUOTE:
        case ID_DD:
        case ID_DIV:
        case ID_DL:
        case ID_DT:
        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
        case ID_HR:
        case ID_LI:
        case ID_OL:
        case ID_P:
        case ID_PRE:
        case ID_TD:
        case ID_TH:
        case ID_UL:
            return true;
    }
    
    return false;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, bool selectReplacement, bool smartReplace) 
    : CompositeEditCommand(document), 
      m_fragment(fragment),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace)
{
}

ReplaceSelectionCommand::~ReplaceSelectionCommand()
{
}

void ReplaceSelectionCommand::doApply()
{
    Selection selection = endingSelection();
    VisiblePosition visibleStart(selection.start());
    VisiblePosition visibleEnd(selection.end());
    bool startAtStartOfLine = isFirstVisiblePositionOnLine(visibleStart);
    bool startAtStartOfBlock = isFirstVisiblePositionInBlock(visibleStart);
    bool startAtEndOfBlock = isLastVisiblePositionInBlock(visibleStart);
    bool startAtBlockBoundary = startAtStartOfBlock || startAtEndOfBlock;
    NodeImpl *startBlock = selection.start().node()->enclosingBlockFlowElement();
    NodeImpl *endBlock = selection.end().node()->enclosingBlockFlowElement();

    bool mergeStart = !(startAtStartOfLine && (m_fragment.hasInterchangeNewlineComment() || m_fragment.hasMoreThanOneBlock()));
    bool mergeEnd = !m_fragment.hasInterchangeNewlineComment() && m_fragment.hasMoreThanOneBlock();
    Position startPos = Position(selection.start().node()->enclosingBlockFlowElement(), 0);
    Position endPos; 
    EStayInBlock upstreamStayInBlock = StayInBlock;

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.isRange()) {
        deleteSelection(false, !(m_fragment.hasInterchangeNewlineComment() || m_fragment.hasMoreThanOneBlock()));
    }
    else if (selection.isCaret() && mergeEnd && !startAtBlockBoundary) {
        // The start and the end need to wind up in separate blocks.
        // Insert a paragraph separator to make that happen.
        insertParagraphSeparator();
        upstreamStayInBlock = DoNotStayInBlock;
    }
    
    selection = endingSelection();
    if (startAtStartOfBlock && startBlock->inDocument())
        startPos = Position(startBlock, 0);
    else if (startAtEndOfBlock)
        startPos = selection.start().downstream(StayInBlock);
    else
        startPos = selection.start().upstream(upstreamStayInBlock);
    endPos = selection.end().downstream(); 
    
    // This command does not use any typing style that is set as a residual effect of
    // a delete.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    KHTMLPart *part = document()->part();
    part->clearTypingStyle();
    setTypingStyle(0);

    if (!m_fragment.firstChild())
        return;
    
    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    NodeImpl *block = startPos.node()->enclosingBlockFlowElement();
    if (removeBlockPlaceholderIfNeeded(block)) {
        startPos = Position(block, 0);
    }
    
    bool addLeadingSpace = false;
    bool addTrailingSpace = false;
    if (m_smartReplace) {
        addLeadingSpace = startPos.leadingWhitespacePosition().isNull();
        if (addLeadingSpace) {
            QChar previousChar = VisiblePosition(startPos).previous().character();
            if (!previousChar.isNull()) {
                addLeadingSpace = !part->isCharacterSmartReplaceExempt(previousChar, true);
            }
        }
        addTrailingSpace = endPos.trailingWhitespacePosition().isNull();
        if (addTrailingSpace) {
            QChar thisChar = VisiblePosition(endPos).character();
            if (!thisChar.isNull()) {
                addTrailingSpace = !part->isCharacterSmartReplaceExempt(thisChar, false);
            }
        }
    }

    document()->updateLayout();

    NodeImpl *refBlock = startPos.node()->enclosingBlockFlowElement();
    Position insertionPos = startPos;
    bool insertBlocksBefore = true;

    NodeImpl *firstNodeInserted = 0;
    NodeImpl *lastNodeInserted = 0;
    bool lastNodeInsertedInMergeEnd = false;

    // prune empty nodes from fragment
    m_fragment.pruneEmptyNodes();

    // Merge content into the end block, if necessary.
    if (mergeEnd) {
        NodeImpl *node = m_fragment.mergeEndNode();
        if (node) {
            NodeImpl *refNode = node;
            NodeImpl *node = refNode ? refNode->nextSibling() : 0;
            insertNodeAt(refNode, endPos.node(), endPos.offset());
            firstNodeInserted = refNode;
            lastNodeInserted = refNode;
            while (node && !isProbablyBlock(node)) {
                NodeImpl *next = node->nextSibling();
                insertNodeAfter(node, refNode);
                lastNodeInserted = node;
                refNode = node;
                node = next;
            }
            lastNodeInsertedInMergeEnd = true;
        }
    }
    
    // prune empty nodes from fragment
    m_fragment.pruneEmptyNodes();

    // Merge content into the start block, if necessary.
    if (mergeStart) {
        NodeImpl *node = m_fragment.mergeStartNode();
        NodeImpl *insertionNode = 0;
        if (node) {
            NodeImpl *refNode = node;
            NodeImpl *node = refNode ? refNode->nextSibling() : 0;
            insertNodeAt(refNode, startPos.node(), startPos.offset());
            firstNodeInserted = refNode;
            if (!lastNodeInsertedInMergeEnd)
                lastNodeInserted = refNode;
            insertionNode = refNode;
            while (node && !isProbablyBlock(node)) {
                NodeImpl *next = node->nextSibling();
                insertNodeAfter(node, refNode);
                if (!lastNodeInsertedInMergeEnd)
                    lastNodeInserted = node;
                insertionNode = node;
                refNode = node;
                node = next;
            }
        }
        if (insertionNode)
            insertionPos = Position(insertionNode, insertionNode->caretMaxOffset());
        insertBlocksBefore = false;
    }

    // prune empty nodes from fragment
    m_fragment.pruneEmptyNodes();
    
    // Merge everything remaining.
    NodeImpl *node = m_fragment.firstChild();
    if (node) {
        NodeImpl *refNode = node;
        NodeImpl *node = refNode ? refNode->nextSibling() : 0;
        if (isProbablyBlock(refNode) && (insertBlocksBefore || startAtStartOfBlock)) {
            insertNodeBefore(refNode, refBlock);
        }
        else if (isProbablyBlock(refNode) && startAtEndOfBlock) {
            insertNodeAfter(refNode, refBlock);
        }
        else {
            insertNodeAt(refNode, insertionPos.node(), insertionPos.offset());
        }
        if (!firstNodeInserted)
            firstNodeInserted = refNode;
        if (!lastNodeInsertedInMergeEnd)
            lastNodeInserted = refNode;
        while (node) {
            NodeImpl *next = node->nextSibling();
            insertNodeAfter(node, refNode);
            if (!lastNodeInsertedInMergeEnd)
                lastNodeInserted = node;
            refNode = node;
            node = next;
        }
        document()->updateLayout();
        insertionPos = Position(lastNodeInserted, lastNodeInserted->caretMaxOffset());
    }

    // Handle "smart replace" whitespace
    if (addTrailingSpace && lastNodeInserted) {
        if (lastNodeInserted->isTextNode()) {
            TextImpl *text = static_cast<TextImpl *>(lastNodeInserted);
            insertTextIntoNode(text, text->length(), nonBreakingSpaceString());
            insertionPos = Position(text, text->length());
        }
        else {
            NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
            insertNodeAfter(node, lastNodeInserted);
            if (!firstNodeInserted)
                firstNodeInserted = node;
            lastNodeInserted = node;
            insertionPos = Position(node, 1);
        }
    }

    if (addLeadingSpace && firstNodeInserted) {
        if (firstNodeInserted->isTextNode()) {
            TextImpl *text = static_cast<TextImpl *>(firstNodeInserted);
            insertTextIntoNode(text, 0, nonBreakingSpaceString());
        }
        else {
            NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
            insertNodeBefore(node, firstNodeInserted);
            firstNodeInserted = node;
            if (!lastNodeInsertedInMergeEnd)
                lastNodeInserted = node;
        }
    }

    // Handle trailing newline
    if (m_fragment.hasInterchangeNewlineComment()) {
        if (startBlock == endBlock && !isProbablyBlock(lastNodeInserted)) {
            setEndingSelection(insertionPos);
            insertParagraphSeparator();
            endPos = endingSelection().end().downstream();
        }
        completeHTMLReplacement(startPos, endPos);
    }
    else {
        completeHTMLReplacement(firstNodeInserted, lastNodeInserted);
    }
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &start, const Position &end)
 {
    if (start.isNull() || !start.node()->inDocument() || end.isNull() || !end.node()->inDocument())
        return;
    m_selectReplacement ? setEndingSelection(Selection(start, end)) : setEndingSelection(end);
}

void ReplaceSelectionCommand::completeHTMLReplacement(NodeImpl *firstNodeInserted, NodeImpl *lastNodeInserted)
{
    if (!firstNodeInserted || !firstNodeInserted->inDocument() ||
        !lastNodeInserted || !lastNodeInserted->inDocument())
        return;

    // Find the last leaf.
    NodeImpl *lastLeaf = lastNodeInserted;
    while (1) {
        NodeImpl *nextChild = lastLeaf->lastChild();
        if (!nextChild)
            break;
        lastLeaf = nextChild;
    }

    // Find the first leaf.
    NodeImpl *firstLeaf = firstNodeInserted;
    while (1) {
        NodeImpl *nextChild = firstLeaf->firstChild();
        if (!nextChild)
            break;
        firstLeaf = nextChild;
    }
    
    Position start(firstLeaf, firstLeaf->caretMinOffset());
    Position end(lastLeaf, lastLeaf->caretMaxOffset());
    Selection replacementSelection(start, end);
    if (m_selectReplacement) {
        // Select what was inserted.
        setEndingSelection(replacementSelection);
    } 
    else {
        // Place the cursor after what was inserted, and mark misspellings in the inserted content.
        setEndingSelection(end);
    }
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

void TypingCommand::insertLineBreak(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertLineBreak();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertLineBreak));
        cmd.apply();
    }
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertParagraphSeparatorInQuotedContent();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertParagraphSeparatorInQuotedContent));
        cmd.apply();
    }
}

void TypingCommand::insertParagraphSeparator(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertParagraphSeparator();
    }
    else {
        EditCommandPtr cmd(new TypingCommand(document, InsertParagraphSeparator));
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
        case InsertLineBreak:
            insertLineBreak();
            return;
        case InsertParagraphSeparator:
            insertParagraphSeparator();
            return;
        case InsertParagraphSeparatorInQuotedContent:
            insertParagraphSeparatorInQuotedContent();
            return;
        case InsertText:
            insertText(m_textToInsert, m_selectInsertedText);
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
        InsertTextCommand *impl = new InsertTextCommand(document());
        EditCommandPtr cmd(impl);
        applyCommandToComposite(cmd);
        impl->input(text, selectInsertedText);
    }
    else {
        EditCommandPtr lastCommand = m_cmds.last();
        if (lastCommand.isInsertTextCommand()) {
            InsertTextCommand *impl = static_cast<InsertTextCommand *>(lastCommand.get());
            impl->input(text, selectInsertedText);
        }
        else {
            InsertTextCommand *impl = new InsertTextCommand(document());
            EditCommandPtr cmd(impl);
            applyCommandToComposite(cmd);
            impl->input(text, selectInsertedText);
        }
    }
    typingAddedToOpenCommand();
}

void TypingCommand::insertLineBreak()
{
    EditCommandPtr cmd(new InsertLineBreakCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparatorInQuotedContent()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorInQuotedContentCommand(document()));
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
        if (lastCommand.isInsertTextCommand()) {
            InsertTextCommand &cmd = static_cast<InsertTextCommand &>(lastCommand);
            cmd.deleteCharacter();
            if (cmd.charactersAdded() == 0) {
                removeCommand(lastCommand);
            }
        }
        else if (lastCommand.isInsertLineBreakCommand()) {
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
        case InsertLineBreak:
        case InsertParagraphSeparator:
        case InsertParagraphSeparatorInQuotedContent:
        case InsertText:
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
