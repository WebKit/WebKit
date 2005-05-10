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
#include "cssparser.h"
#include "cssproperties.h"
#include "dom_doc.h"
#include "dom_docimpl.h"
#include "dom_elementimpl.h"
#include "dom_nodeimpl.h"
#include "dom_position.h"
#include "dom_stringimpl.h"
#include "dom_textimpl.h"
#include "dom2_range.h"
#include "dom2_rangeimpl.h"
#include "html_elementimpl.h"
#include "html_imageimpl.h"
#include "html_interchange.h"
#include "htmlattrs.h"
#include "htmltags.h"
#include "khtml_part.h"
#include "khtml_part.h"
#include "khtmlview.h"
#include "qcolor.h"
#include "qptrlist.h"
#include "render_object.h"
#include "render_style.h"
#include "render_text.h"
#include "visible_position.h"
#include "visible_text.h"
#include "visible_units.h"

using DOM::AttrImpl;
using DOM::CSSComputedStyleDeclarationImpl;
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::CSSParser;
using DOM::CSSPrimitiveValue;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSProperty;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSValue;
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
using DOM::Range;
using DOM::RangeImpl;
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
    return c.unicode() == 0xa0;
}

// FIXME: Can't really determine this without taking white-space mode into account.
static inline bool nextCharacterIsCollapsibleWhitespace(const Position &pos)
{
    if (!pos.node())
        return false;
    if (!pos.node()->isTextNode())
        return false;
    return isCollapsibleWhitespace(static_cast<TextImpl *>(pos.node())->data()[pos.offset()]);
}

static const int spacesPerTab = 4;

static bool isTableStructureNode(const NodeImpl *node)
{
    RenderObject *r = node->renderer();
    return (r && (r->isTableCell() || r->isTableRow() || r->isTableSection() || r->isTableCol()));
}

static bool isListStructureNode(const NodeImpl *node)
{
    // FIXME: Irritating that we can get away with just going at the render tree for isTableStructureNode,
    // but here we also have to peek at the type of DOM node?
    RenderObject *r = node->renderer();
    NodeImpl::Id nodeID = node->id();
    return (r && r->isListItem())
        || (nodeID == ID_OL || nodeID == ID_UL || nodeID == ID_DD || nodeID == ID_DT || nodeID == ID_DIR || nodeID == ID_MENU);
}

static DOMString &nonBreakingSpaceString()
{
    static DOMString nonBreakingSpaceString = QString(QChar(0xa0));
    return nonBreakingSpaceString;
}

static DOMString &styleSpanClassString()
{
    static DOMString styleSpanClassString = AppleStyleSpanClass;
    return styleSpanClassString;
}

static bool isEmptyStyleSpan(const NodeImpl *node)
{
    if (!node || !node->isHTMLElement() || node->id() != ID_SPAN)
        return false;

    const HTMLElementImpl *elem = static_cast<const HTMLElementImpl *>(node);
    CSSMutableStyleDeclarationImpl *inlineStyleDecl = elem->inlineStyleDecl();
    return (!inlineStyleDecl || inlineStyleDecl->length() == 0) && elem->getAttribute(ATTR_CLASS) == styleSpanClassString();
}

static bool isStyleSpan(const NodeImpl *node)
{
    if (!node || !node->isHTMLElement())
        return false;

    const HTMLElementImpl *elem = static_cast<const HTMLElementImpl *>(node);
    return elem->id() == ID_SPAN && elem->getAttribute(ATTR_CLASS) == styleSpanClassString();
}

static bool isEmptyFontTag(const NodeImpl *node)
{
    if (!node || node->id() != ID_FONT)
        return false;

    const ElementImpl *elem = static_cast<const ElementImpl *>(node);
    NamedAttrMapImpl *map = elem->attributes(true); // true for read-only
    return (!map || map->length() == 1) && elem->getAttribute(ATTR_CLASS) == styleSpanClassString();
}

static DOMString &blockPlaceholderClassString()
{
    static DOMString blockPlaceholderClassString = "khtml-block-placeholder";
    return blockPlaceholderClassString;
}

static DOMString &matchNearestBlockquoteColorString()
{
    static DOMString matchNearestBlockquoteColorString = "match";
    return matchNearestBlockquoteColorString;
}

static void derefNodesInList(QPtrList<NodeImpl> &list)
{
    for (QPtrListIterator<NodeImpl> it(list); it.current(); ++it)
        it.current()->deref();
}

static int maxRangeOffset(NodeImpl *n)
{
    if (DOM::offsetInCharacters(n->nodeType()))
        return n->maxOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

static int maxDeepOffset(NodeImpl *n)
{
    if (n->isAtomicNode())
        return n->caretMaxOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

static void debugPosition(const char *prefix, const Position &pos)
{
    if (!prefix)
        prefix = "";
    if (pos.isNull())
        LOG(Editing, "%s <null>", prefix);
    else
        LOG(Editing, "%s%s %p : %d", prefix, pos.node()->nodeName().string().latin1(), pos.node(), pos.offset());
}

static void debugNode(const char *prefix, const NodeImpl *node)
{
    if (!prefix)
        prefix = "";
    if (!node)
        LOG(Editing, "%s <null>", prefix);
    else
        LOG(Editing, "%s%s %p", prefix, node->nodeName().string().latin1(), node);
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

EditAction EditCommandPtr::editingAction() const
{
    IF_IMPL_NULL_RETURN_ARG(EditActionUnspecified);
    return get()->editingAction();
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

void EditCommandPtr::setStartingSelection(const VisiblePosition &p) const
{
    IF_IMPL_NULL_RETURN;
    get()->setStartingSelection(p);
}

void EditCommandPtr::setStartingSelection(const Position &p, EAffinity affinity) const
{
    IF_IMPL_NULL_RETURN;
    Selection s = Selection(p, affinity);
    get()->setStartingSelection(s);
}

void EditCommandPtr::setEndingSelection(const Selection &s) const
{
    IF_IMPL_NULL_RETURN;
    get()->setEndingSelection(s);
}

void EditCommandPtr::setEndingSelection(const VisiblePosition &p) const
{
    IF_IMPL_NULL_RETURN;
    get()->setEndingSelection(p);
}

void EditCommandPtr::setEndingSelection(const Position &p, EAffinity affinity) const
{
    IF_IMPL_NULL_RETURN;
    Selection s = Selection(p, affinity);
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

        if (property->id() == CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT) {
            // we have to special-case text decorations
            CSSProperty alteredProperty = CSSProperty(CSS_PROP_TEXT_DECORATION, property->value(), property->isImportant());
            styleText += alteredProperty.cssText().string();
        } else {
            styleText += property->cssText().string();
        }
    }

    mutableStyle->deref();

    // Save the result for later
    m_cssStyle = styleText.stripWhiteSpace();
}

StyleChange::ELegacyHTMLStyles StyleChange::styleModeForParseMode(bool isQuirksMode)
{
    return isQuirksMode ? UseLegacyHTMLStyles : DoNotUseLegacyHTMLStyles;
}

bool StyleChange::checkForLegacyHTMLStyleChange(const CSSProperty *property)
{
    if (!property || !property->value()) {
        return false;
    }
    
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
        case CSS_PROP_COLOR: {
            QColor color(CSSParser::parseColor(valueText));
            m_applyFontColor = color.name();
            return true;
        }
        case CSS_PROP_FONT_FAMILY:
            m_applyFontFace = valueText;
            return true;
        case CSS_PROP_FONT_SIZE:
            if (property->value()->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
                CSSPrimitiveValueImpl *value = static_cast<CSSPrimitiveValueImpl *>(property->value());
                float number = value->getFloatValue(CSSPrimitiveValue::CSS_PX);
                if (number <= 9)
                    m_applyFontSize = "1";
                else if (number <= 10)
                    m_applyFontSize = "2";
                else if (number <= 13)
                    m_applyFontSize = "3";
                else if (number <= 16)
                    m_applyFontSize = "4";
                else if (number <= 18)
                    m_applyFontSize = "5";
                else if (number <= 24)
                    m_applyFontSize = "6";
                else
                    m_applyFontSize = "7";
                // Huge quirk in Microsft Entourage is that they understand CSS font-size, but also write 
                // out legacy 1-7 values in font tags (I guess for mailers that are not CSS-savvy at all, 
                // like Eudora). Yes, they write out *both*. We need to write out both as well. Return false.
                return false; 
            }
            else {
                // Can't make sense of the number. Put no font size.
                return true;
            }
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

EditAction EditCommand::editingAction() const
{
    return EditActionUnspecified;
}

void EditCommand::setStartingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setStartingSelection(const VisiblePosition &p)
{
    Selection s = Selection(p);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setStartingSelection(const Position &p, EAffinity affinity)
{
    Selection s = Selection(p, affinity);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setEndingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::setEndingSelection(const VisiblePosition &p)
{
    Selection s = Selection(p);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::setEndingSelection(const Position &p, EAffinity affinity)
{
    Selection s = Selection(p, affinity);
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

CSSMutableStyleDeclarationImpl *EditCommand::styleAtPosition(const Position &pos)
{
    CSSComputedStyleDeclarationImpl *computedStyle = pos.computedStyle();
    computedStyle->ref();
    CSSMutableStyleDeclarationImpl *style = computedStyle->copyInheritableProperties();
    computedStyle->deref();
 
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    if (typingStyle)
        style->merge(typingStyle);
    
    return style;
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

void CompositeEditCommand::applyStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), style, editingAction));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeBefore(NodeImpl *insertChild, NodeImpl *refChild)
{
    ASSERT(refChild->id() != ID_BODY);
    EditCommandPtr cmd(new InsertNodeBeforeCommand(document(), insertChild, refChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeAfter(NodeImpl *insertChild, NodeImpl *refChild)
{
    ASSERT(refChild->id() != ID_BODY);
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
    if (isTableStructureNode(node) || node == node->rootEditableElement()) {
        // Do not remove an element of table structure; remove its contents.
        // Likewise for the root editable element.
        NodeImpl *child = node->firstChild();
        while (child) {
            NodeImpl *remove = child;
            child = child->nextSibling();
            removeFullySelectedNode(remove);
        }
    }
    else {
        removeNode(node);
    }
}

void CompositeEditCommand::removeChildrenInRange(NodeImpl *node, int from, int to)
{
    NodeImpl *nodeToRemove = node->childNode(from);
    for (int i = from; i < to; i++) {
        ASSERT(nodeToRemove);
        NodeImpl *next = nodeToRemove->nextSibling();
        removeNode(nodeToRemove);
        nodeToRemove = next;
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

void CompositeEditCommand::splitElement(ElementImpl *element, NodeImpl *atChild)
{
    EditCommandPtr cmd(new SplitElementCommand(document(), element, atChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::mergeIdenticalElements(DOM::ElementImpl *first, DOM::ElementImpl *second)
{
    EditCommandPtr cmd(new MergeIdenticalElementsCommand(document(), first, second));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::wrapContentsInDummySpan(DOM::ElementImpl *element)
{
    EditCommandPtr cmd(new WrapContentsInDummySpanCommand(document(), element));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitTextNodeContainingElement(DOM::TextImpl *text, long offset)
{
    EditCommandPtr cmd(new SplitTextNodeContainingElementCommand(document(), text, offset));
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
    DOMString value = element->getAttribute(attribute);
    if (value.isEmpty())
        return;
    EditCommandPtr cmd(new RemoveNodeAttributeCommand(document(), element, attribute));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::setNodeAttribute(ElementImpl *element, int attribute, const DOMString &value)
{
    EditCommandPtr cmd(new SetNodeAttributeCommand(document(), element, attribute, value));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::rebalanceWhitespace()
{
    Selection selection = endingSelection();
    if (selection.isCaretOrRange()) {
        EditCommandPtr startCmd(new RebalanceWhitespaceCommand(document(), endingSelection().start()));
        applyCommandToComposite(startCmd);
        if (selection.isRange()) {
            EditCommandPtr endCmd(new RebalanceWhitespaceCommand(document(), endingSelection().end()));
            applyCommandToComposite(endCmd);
        }
    }
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
    Position end = VisiblePosition(pos, VP_DEFAULT_AFFINITY).next().deepEquivalent().downstream();
    deleteInsignificantText(pos, end);
}

NodeImpl *CompositeEditCommand::appendBlockPlaceholder(NodeImpl *node)
{
    if (!node)
        return NULL;

    ASSERT(node->renderer() && node->renderer()->isBlockFlow());

    NodeImpl *placeholder = createBlockPlaceholderElement(document());
    appendNode(placeholder, node);
    return placeholder;
}

NodeImpl *CompositeEditCommand::insertBlockPlaceholder(const Position &pos)
{
    if (pos.isNull())
        return NULL;

    ASSERT(pos.node()->renderer() && pos.node()->renderer()->isBlockFlow());

    NodeImpl *placeholder = createBlockPlaceholderElement(document());
    insertNodeAt(placeholder, pos.node(), pos.offset());
    return placeholder;
}

NodeImpl *CompositeEditCommand::addBlockPlaceholderIfNeeded(NodeImpl *node)
{
    if (!node)
        return false;

    document()->updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return false;
    
    // append the placeholder to make sure it follows
    // any unrendered blocks
    if (renderer->height() == 0) {
        return appendBlockPlaceholder(node);
    }

    return NULL;
}

bool CompositeEditCommand::removeBlockPlaceholder(NodeImpl *node)
{
    NodeImpl *placeholder = findBlockPlaceholder(node);
    if (placeholder) {
        removeNode(placeholder);
        return true;
    }
    return false;
}

NodeImpl *CompositeEditCommand::findBlockPlaceholder(NodeImpl *node)
{
    if (!node)
        return 0;

    document()->updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return 0;

    for (NodeImpl *checkMe = node; checkMe; checkMe = checkMe->traverseNextNode(node)) {
        if (checkMe->isElementNode()) {
            ElementImpl *element = static_cast<ElementImpl *>(checkMe);
            if (element->enclosingBlockFlowElement() == node && 
                element->getAttribute(ATTR_CLASS) == blockPlaceholderClassString()) {
                return element;
            }
        }
    }
    
    return 0;
}

void CompositeEditCommand::moveParagraphContentsToNewBlockIfNecessary(const Position &pos)
{
    if (pos.isNull())
        return;
    
    document()->updateLayout();
    
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    VisiblePosition visibleParagraphStart(startOfParagraph(visiblePos));
    VisiblePosition visibleParagraphEnd(endOfParagraph(visiblePos, IncludeLineBreak));
    Position paragraphStart = visibleParagraphStart.deepEquivalent().upstream();
    Position paragraphEnd = visibleParagraphEnd.deepEquivalent().upstream();
    
    // Perform some checks to see if we need to perform work in this function.
    if (paragraphStart.node()->isBlockFlow()) {
        if (paragraphEnd.node()->isBlockFlow()) {
            if (!paragraphEnd.node()->isAncestor(paragraphStart.node())) {
                // If the paragraph end is a descendant of paragraph start, then we need to run
                // the rest of this function. If not, we can bail here.
                return;
            }
        }
        else if (paragraphEnd.node()->enclosingBlockFlowElement() != paragraphStart.node()) {
            // The paragraph end is in another block that is an ancestor of the paragraph start.
            // We can bail as we have a full block to work with.
            ASSERT(paragraphStart.node()->isAncestor(paragraphEnd.node()->enclosingBlockFlowElement()));
            return;
        }
        else if (isEndOfDocument(visibleParagraphEnd)) {
            // At the end of the document. We can bail here as well.
            return;
        }
    }

    // Create the block to insert. Most times, this will be a shallow clone of the block containing
    // the start of the selection (the start block), except for two cases:
    //    1) When the start block is a body element.
    //    2) When the start block is a mail blockquote and we are not in a position to insert
    //       the new block as a peer of the start block. This prevents creating an unwanted 
    //       additional level of quoting.
    NodeImpl *startBlock = paragraphStart.node()->enclosingBlockFlowElement();
    NodeImpl *newBlock = 0;
    if (startBlock->id() == ID_BODY || (isMailBlockquote(startBlock) && paragraphStart.node() != startBlock))
        newBlock = createDefaultParagraphElement(document());
    else
        newBlock = startBlock->cloneNode(false);

    NodeImpl *moveNode = paragraphStart.node();
    if (paragraphStart.offset() >= paragraphStart.node()->caretMaxOffset())
        moveNode = moveNode->traverseNextNode();
    NodeImpl *endNode = paragraphEnd.node();

    if (paragraphStart.node()->id() == ID_BODY) {
        insertNodeAt(newBlock, paragraphStart.node(), 0);
    }
    else if (paragraphStart.node()->id() == ID_BR) {
        insertNodeAfter(newBlock, paragraphStart.node());
    }
    else {
        insertNodeBefore(newBlock, paragraphStart.upstream().node());
    }

    while (moveNode && !moveNode->isBlockFlow()) {
        NodeImpl *next = moveNode->traverseNextSibling();
        removeNode(moveNode);
        appendNode(moveNode, newBlock);
        if (moveNode == endNode)
            break;
        moveNode = next;
    }
}

static bool isSpecialElement(NodeImpl *n)
{
    if (!n->isHTMLElement())
        return false;

    if (n->id() == ID_A && n->hasAnchor())
        return true;

    if (n->id() == ID_UL || n->id() == ID_OL || n->id() == ID_DL)
        return true;

    RenderObject *renderer = n->renderer();

    if (renderer && (renderer->style()->display() == TABLE || renderer->style()->display() == INLINE_TABLE))
        return true;

    if (renderer && renderer->style()->isFloating())
        return true;

    if (renderer && renderer->style()->position() != STATIC)
        return true;

    return false;
}

// This version of the function is meant to be called on positions in a document fragment,
// so it does not check for a root editable element, it is assumed these nodes will be put
// somewhere editable in the future
static bool isFirstVisiblePositionInSpecialElementInFragment(const Position& pos)
{
    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            return false;
        if (isSpecialElement(n))
            return true;
    }

    return false;
}

static bool isFirstVisiblePositionInSpecialElement(const Position& pos)
{
    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            return false;
        if (n->rootEditableElement() == NULL)
            return false;
        if (isSpecialElement(n))
            return true;
    }

    return false;
}

static Position positionBeforeNode(NodeImpl *node)
{
    return Position(node->parentNode(), node->nodeIndex());
}

static Position positionBeforeContainingSpecialElement(const Position& pos)
{
    ASSERT(isFirstVisiblePositionInSpecialElement(pos));

    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
    
    NodeImpl *outermostSpecialElement = NULL;

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            break;
        if (n->rootEditableElement() == NULL)
            break;
        if (isSpecialElement(n))
            outermostSpecialElement = n;
    }
    
    ASSERT(outermostSpecialElement);

    Position result = positionBeforeNode(outermostSpecialElement);
    if (result.isNull() || !result.node()->rootEditableElement())
        return pos;
    
    return result;
}

static bool isLastVisiblePositionInSpecialElement(const Position& pos)
{
    // make sure to get a range-compliant version of the position
    Position rangePos = VisiblePosition(pos, DOWNSTREAM).position();

    VisiblePosition vPos = VisiblePosition(rangePos, DOWNSTREAM);

    for (NodeImpl *n = rangePos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, maxRangeOffset(n), DOWNSTREAM) != vPos)
            return false;
        if (n->rootEditableElement() == NULL)
            return false;
        if (isSpecialElement(n))
            return true;
    }

    return false;
}

static Position positionAfterNode(NodeImpl *node)
{
    return Position(node->parentNode(), node->nodeIndex() + 1);
}

static Position positionAfterContainingSpecialElement(const Position& pos)
{
    ASSERT(isLastVisiblePositionInSpecialElement(pos));

    // make sure to get a range-compliant version of the position
    Position rangePos = VisiblePosition(pos, DOWNSTREAM).position();

    VisiblePosition vPos = VisiblePosition(rangePos, DOWNSTREAM);

    NodeImpl *outermostSpecialElement = NULL;

    for (NodeImpl *n = rangePos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, maxRangeOffset(n), DOWNSTREAM) != vPos)
            break;
        if (n->rootEditableElement() == NULL)
            break;
        if (isSpecialElement(n))
            outermostSpecialElement = n;
    }
    
    ASSERT(outermostSpecialElement);

    Position result = positionAfterNode(outermostSpecialElement);
    if (result.isNull() || !result.node()->rootEditableElement())
        return pos;
    
    return result;
}

static Position positionOutsideContainingSpecialElement(const Position &pos)
{
    if (isFirstVisiblePositionInSpecialElement(pos)) {
        return positionBeforeContainingSpecialElement(pos);
    } else if (isLastVisiblePositionInSpecialElement(pos)) {
        return positionAfterContainingSpecialElement(pos);
    }

    return pos;
}

static Position positionBeforePossibleContainingSpecialElement(const Position &pos)
{
    if (isFirstVisiblePositionInSpecialElement(pos)) {
        return positionBeforeContainingSpecialElement(pos);
    } 

    return pos;
}

static Position positionAfterPossibleContainingSpecialElement(const Position &pos)
{
    if (isLastVisiblePositionInSpecialElement(pos)) {
        return positionAfterContainingSpecialElement(pos);
    }

    return pos;
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

ApplyStyleCommand::ApplyStyleCommand(DocumentImpl *document, CSSStyleDeclarationImpl *style, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document), m_style(style->makeMutable()), m_editingAction(editingAction), m_propertyLevel(propertyLevel)
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
    switch (m_propertyLevel) {
        case PropertyDefault: {
            // apply the block-centric properties of the style
            CSSMutableStyleDeclarationImpl *blockStyle = m_style->copyBlockProperties();
            blockStyle->ref();
            applyBlockStyle(blockStyle);
            // apply any remaining styles to the inline elements
            // NOTE: hopefully, this string comparison is the same as checking for a non-null diff
            if (blockStyle->length() < m_style->length()) {
                CSSMutableStyleDeclarationImpl *inlineStyle = m_style->copy();
                inlineStyle->ref();
                applyRelativeFontStyleChange(inlineStyle);
                blockStyle->diff(inlineStyle);
                applyInlineStyle(inlineStyle);
                inlineStyle->deref();
            }
            blockStyle->deref();
            break;
        }
        case ForceBlockProperties:
            // Force all properties to be applied as block styles.
            applyBlockStyle(m_style);
            break;
    }
   
    setEndingSelectionNeedsLayout();
}

EditAction ApplyStyleCommand::editingAction() const
{
    return m_editingAction;
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
    // Also, gather up all the nodes we want to process in a QPtrList before
    // doing anything. This averts any bugs iterating over these nodes
    // once you start removing and applying style.
    NodeImpl *beyondEnd = end.node()->traverseNextNode();
    QPtrList<NodeImpl> nodes;
    for (NodeImpl *node = start.node(); node != beyondEnd; node = node->traverseNextNode())
        nodes.append(node);
        
    NodeImpl *prevBlock = 0;
    for (QPtrListIterator<NodeImpl> it(nodes); it.current(); ++it) {
        NodeImpl *block = it.current()->enclosingBlockFlowElement();
        if (block != prevBlock && block->isHTMLElement()) {
            removeCSSStyle(style, static_cast<HTMLElementImpl *>(block));
            prevBlock = block;
        }
    }
    
    // apply specified styles to the block flow elements in the selected range
    prevBlock = 0;
    for (QPtrListIterator<NodeImpl> it(nodes); it.current(); ++it) {
        NodeImpl *node = it.current();
        if (node->renderer()) {
            NodeImpl *block = node->enclosingBlockFlowElement();
            if (block != prevBlock) {
                addBlockStyleIfNeeded(style, node);
                prevBlock = block;
            }
        }
    }
}

#define NoFontDelta (0.0f)
#define MinimumFontSize (0.1f)

void ApplyStyleCommand::applyRelativeFontStyleChange(CSSMutableStyleDeclarationImpl *style)
{
    if (style->getPropertyCSSValue(CSS_PROP_FONT_SIZE)) {
        // Explicit font size overrides any delta.
        style->removeProperty(CSS_PROP__KHTML_FONT_SIZE_DELTA);
        return;
    }

    // Get the adjustment amount out of the style.
    CSSValueImpl *value = style->getPropertyCSSValue(CSS_PROP__KHTML_FONT_SIZE_DELTA);
    if (!value)
        return;
    value->ref();
    float adjustment = NoFontDelta;
    if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
        CSSPrimitiveValueImpl *primitiveValue = static_cast<CSSPrimitiveValueImpl *>(value);
        if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PX) {
            // Only PX handled now. If we handle more types in the future, perhaps
            // a switch statement here would be more appropriate.
            adjustment = primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PX);
        }
    }
    style->removeProperty(CSS_PROP__KHTML_FONT_SIZE_DELTA);
    value->deref();
    if (adjustment == NoFontDelta)
        return;
    
    // Adjust to the positions we want to use for applying style.
    Selection selection = endingSelection();
    Position start(selection.start().downstream());
    Position end(selection.end().upstream());
    if (RangeImpl::compareBoundaryPoints(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    // Join up any adjacent text nodes.
    if (start.node()->isTextNode()) {
        joinChildTextNodes(start.node()->parentNode(), start, end);
        selection = endingSelection();
        start = selection.start();
        end = selection.end();
    }
    if (end.node()->isTextNode() && start.node()->parentNode() != end.node()->parentNode()) {
        joinChildTextNodes(end.node()->parentNode(), start, end);
        selection = endingSelection();
        start = selection.start();
        end = selection.end();
    }

    // Split the start text nodes if needed to apply style.
    bool splitStart = splitTextAtStartIfNeeded(start, end); 
    if (splitStart) {
        start = endingSelection().start();
        end = endingSelection().end();
    }
    bool splitEnd = splitTextAtEndIfNeeded(start, end);
    if (splitEnd) {
        start = endingSelection().start();
        end = endingSelection().end();
    }

    NodeImpl *beyondEnd = end.node()->traverseNextNode(); // Calculate loop end point.
    start = start.upstream(); // Move upstream to ensure we do not add redundant spans.
    NodeImpl *startNode = start.node();
    if (startNode->isTextNode() && start.offset() >= startNode->caretMaxOffset()) // Move out of text node if range does not include its characters.
        startNode = startNode->traverseNextNode();

    // Store away font size before making any changes to the document.
    // This ensures that changes to one node won't effect another.
    QMap<const NodeImpl *,float> startingFontSizes;
    for (const NodeImpl *node = startNode; node != beyondEnd; node = node->traverseNextNode())
        startingFontSizes.insert(node, computedFontSize(node));

    // These spans were added by us. If empty after font size changes, they can be removed.
    QPtrList<NodeImpl> emptySpans;
    
    NodeImpl *lastStyledNode = 0;
    for (NodeImpl *node = startNode; node != beyondEnd; node = node->traverseNextNode()) {
        HTMLElementImpl *elem = 0;
        if (node->isHTMLElement()) {
            // Only work on fully selected nodes.
            if (!nodeFullySelected(node, start, end))
                continue;
            elem = static_cast<HTMLElementImpl *>(node);
        }
        else if (node->isTextNode() && node->parentNode() != lastStyledNode) {
            // Last styled node was not parent node of this text node, but we wish to style this
            // text node. To make this possible, add a style span to surround this text node.
            elem = static_cast<HTMLElementImpl *>(createStyleSpanElement(document()));
            insertNodeBefore(elem, node);
            surroundNodeRangeWithElement(node, node, elem);
        }
        else {
            // Only handle HTML elements and text nodes.
            continue;
        }
        lastStyledNode = node;
        
        CSSMutableStyleDeclarationImpl *inlineStyleDecl = elem->getInlineStyleDecl();
        float currentFontSize = computedFontSize(node);
        float desiredFontSize = kMax(MinimumFontSize, startingFontSizes[node] + adjustment);
        if (inlineStyleDecl->getPropertyCSSValue(CSS_PROP_FONT_SIZE)) {
            inlineStyleDecl->removeProperty(CSS_PROP_FONT_SIZE, true);
            currentFontSize = computedFontSize(node);
        }
        if (currentFontSize != desiredFontSize) {
            QString desiredFontSizeString = QString::number(desiredFontSize);
            desiredFontSizeString += "px";
            inlineStyleDecl->setProperty(CSS_PROP_FONT_SIZE, desiredFontSizeString, false, false);
            setNodeAttribute(elem, ATTR_STYLE, inlineStyleDecl->cssText());
        }
        if (inlineStyleDecl->length() == 0) {
            removeNodeAttribute(elem, ATTR_STYLE);
            if (isEmptyStyleSpan(elem))
                emptySpans.append(elem);
        }
    }

    for (QPtrListIterator<NodeImpl> it(emptySpans); it.current(); ++it)
        removeNodePreservingChildren(it.current());
}

#undef NoFontDelta
#undef MinimumFontSize

void ApplyStyleCommand::applyInlineStyle(CSSMutableStyleDeclarationImpl *style)
{
    // adjust to the positions we want to use for applying style
    Position start(endingSelection().start().downstream().equivalentRangeCompliantPosition());
    Position end(endingSelection().end().upstream());

    if (RangeImpl::compareBoundaryPoints(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document()->updateLayout();

    // split the start node and containing element if the selection starts inside of it
    bool splitStart = splitTextElementAtStartIfNeeded(start, end); 
    if (splitStart) {
        start = endingSelection().start();
        end = endingSelection().end();
    }

    // split the end node and containing element if the selection ends inside of it
    bool splitEnd = splitTextElementAtEndIfNeeded(start, end);
    start = endingSelection().start();
    end = endingSelection().end();

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    removeInlineStyle(style, start.upstream(), end);
    start = endingSelection().start();
    end = endingSelection().end();

    if (splitStart) {
        bool mergedStart = mergeStartWithPreviousIfIdentical(start, end);
        if (mergedStart) {
            start = endingSelection().start();
            end = endingSelection().end();
        }
    }

    if (splitEnd) {
        mergeEndWithNextIfIdentical(start, end);
        start = endingSelection().start();
        end = endingSelection().end();
    }

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
        if (start.offset() >= start.node()->caretMaxOffset())
            node = node->traverseNextNode();
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

    if (splitStart || splitEnd) {
        cleanUpEmptyStyleSpans(start, end);
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

void ApplyStyleCommand::removeHTMLFontStyle(CSSMutableStyleDeclarationImpl *style, HTMLElementImpl *elem)
{
    ASSERT(style);
    ASSERT(elem);

    if (elem->id() != ID_FONT)
        return;

    int exceptionCode = 0;
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = style->valuesIterator(); it != end; ++it) {
        switch ((*it).id()) {
            case CSS_PROP_COLOR:
                elem->removeAttribute(ATTR_COLOR, exceptionCode);
                ASSERT(exceptionCode == 0);
                break;
            case CSS_PROP_FONT_FAMILY:
                elem->removeAttribute(ATTR_FACE, exceptionCode);
                ASSERT(exceptionCode == 0);
                break;
            case CSS_PROP_FONT_SIZE:
                elem->removeAttribute(ATTR_SIZE, exceptionCode);
                ASSERT(exceptionCode == 0);
                break;
        }
    }

    if (isEmptyFontTag(elem))
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

    if (isEmptyStyleSpan(elem))
        removeNodePreservingChildren(elem);
}

void ApplyStyleCommand::removeBlockStyle(CSSMutableStyleDeclarationImpl *style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(RangeImpl::compareBoundaryPoints(start, end) <= 0);
    
}

static bool hasTextDecorationProperty(NodeImpl *node)
{
    if (!node->isElementNode())
        return false;

    ElementImpl *element = static_cast<ElementImpl *>(node);
    CSSComputedStyleDeclarationImpl style(element);

    CSSValueImpl *value = style.getPropertyCSSValue(CSS_PROP_TEXT_DECORATION, DoNotUpdateLayout);

    if (value) {
        value->ref();
        DOMString valueText(value->cssText());
        value->deref();
        if (strcasecmp(valueText,"none") != 0)
            return true;
    }

    return false;
}

static NodeImpl* highestAncestorWithTextDecoration(NodeImpl *node)
{
    NodeImpl *result = NULL;

    for (NodeImpl *n = node; n; n = n->parentNode()) {
        if (hasTextDecorationProperty(n))
            result = n;
    }

    return result;
}

CSSMutableStyleDeclarationImpl *ApplyStyleCommand::extractTextDecorationStyle(NodeImpl *node)
{
    ASSERT(node);
    ASSERT(node->isElementNode());
    
    // non-html elements not handled yet
    if (!node->isHTMLElement())
        return 0;

    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
    CSSMutableStyleDeclarationImpl *style = element->inlineStyleDecl();
    if (!style)
        return 0;

    style->ref();
    int properties[1] = { CSS_PROP_TEXT_DECORATION };
    CSSMutableStyleDeclarationImpl *textDecorationStyle = style->copyPropertiesInSet(properties, 1);

    CSSValueImpl *property = style->getPropertyCSSValue(CSS_PROP_TEXT_DECORATION);
    if (property && strcasecmp(property->cssText(), "none") != 0) {
        removeCSSProperty(style, CSS_PROP_TEXT_DECORATION);
    }

    style->deref();

    return textDecorationStyle;
}

CSSMutableStyleDeclarationImpl *ApplyStyleCommand::extractAndNegateTextDecorationStyle(NodeImpl *node)
{
    ASSERT(node);
    ASSERT(node->isElementNode());
    
    // non-html elements not handled yet
    if (!node->isHTMLElement())
        return 0;

    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
    CSSComputedStyleDeclarationImpl *computedStyle = new CSSComputedStyleDeclarationImpl(element);
    ASSERT(computedStyle);

    computedStyle->ref();

    int properties[1] = { CSS_PROP_TEXT_DECORATION };
    CSSMutableStyleDeclarationImpl *textDecorationStyle = computedStyle->copyPropertiesInSet(properties, 1);
    

    CSSValueImpl *property = computedStyle->getPropertyCSSValue(CSS_PROP_TEXT_DECORATION);
    if (property && strcasecmp(property->cssText(), "none") != 0) {
        property->ref();
        CSSMutableStyleDeclarationImpl *newStyle = textDecorationStyle->copy();

        newStyle->ref();
        newStyle->setProperty(CSS_PROP_TEXT_DECORATION, "none");
        applyTextDecorationStyle(node, newStyle);
        newStyle->deref();

        property->deref();
    }

    computedStyle->deref();

    return textDecorationStyle;
}

void ApplyStyleCommand::applyTextDecorationStyle(NodeImpl *node, CSSMutableStyleDeclarationImpl *style)
{
    ASSERT(node);

    if (!style || !style->cssText().length())
        return;

    if (node->isTextNode()) {
        HTMLElementImpl *styleSpan = static_cast<HTMLElementImpl *>(createStyleSpanElement(document()));
        insertNodeBefore(styleSpan, node);
        surroundNodeRangeWithElement(node, node, styleSpan);
        node = styleSpan;
    }

    if (!node->isElementNode())
        return;

    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
        
    StyleChange styleChange(style, Position(element, 0), StyleChange::styleModeForParseMode(document()->inCompatMode()));
    if (styleChange.cssStyle().length() > 0) {
        DOMString cssText = styleChange.cssStyle();
        CSSMutableStyleDeclarationImpl *decl = element->inlineStyleDecl();
        if (decl)
            cssText += decl->cssText();
        setNodeAttribute(element, ATTR_STYLE, cssText);
    }
}

void ApplyStyleCommand::pushDownTextDecorationStyleAroundNode(NodeImpl *node, const Position &start, const Position &end, bool force)
{
    NodeImpl *highestAncestor = highestAncestorWithTextDecoration(node);
    
    if (highestAncestor) {
        NodeImpl *nextCurrent;
        NodeImpl *nextChild;
        for (NodeImpl *current = highestAncestor; current != node; current = nextCurrent) {
            ASSERT(current);
            
            nextCurrent = NULL;
            
            CSSMutableStyleDeclarationImpl *decoration = force ? extractAndNegateTextDecorationStyle(current) : extractTextDecorationStyle(current);
            if (decoration)
                decoration->ref();

            for (NodeImpl *child = current->firstChild(); child; child = nextChild) {
                nextChild = child->nextSibling();

                if (node == child) {
                    nextCurrent = child;
                } else if (node->isAncestor(child)) {
                    applyTextDecorationStyle(child, decoration);
                    nextCurrent = child;
                } else {
                    applyTextDecorationStyle(child, decoration);
                }
            }

            if (decoration)
                decoration->deref();
        }
    }
}

void ApplyStyleCommand::pushDownTextDecorationStyleAtBoundaries(const Position &start, const Position &end)
{
    // We need to work in two passes. First we push down any inline
    // styles that set text decoration. Then we look for any remaining
    // styles (caused by stylesheets) and explicitly negate text
    // decoration while pushing down.

    pushDownTextDecorationStyleAroundNode(start.node(), start, end, false);
    document()->updateLayout();
    pushDownTextDecorationStyleAroundNode(start.node(), start, end, true);

    pushDownTextDecorationStyleAroundNode(end.node(), start, end, false);
    document()->updateLayout();
    pushDownTextDecorationStyleAroundNode(end.node(), start, end, true);
}

void ApplyStyleCommand::removeInlineStyle(CSSMutableStyleDeclarationImpl *style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(RangeImpl::compareBoundaryPoints(start, end) < 0);
    
    CSSValueImpl *textDecorationSpecialProperty = style->getPropertyCSSValue(CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT);

    if (textDecorationSpecialProperty) {
        pushDownTextDecorationStyleAtBoundaries(start.downstream(), end.upstream());
        style = style->copy();
        style->setProperty(CSS_PROP_TEXT_DECORATION, textDecorationSpecialProperty->cssText(), style->getPropertyPriority(CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT));
    }

    // The s and e variables store the positions used to set the ending selection after style removal
    // takes place. This will help callers to recognize when either the start node or the end node
    // are removed from the document during the work of this function.
    Position s = start;
    Position e = end;

    NodeImpl *node = start.node();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (node->isHTMLElement() && nodeFullySelected(node, start, end)) {
            HTMLElementImpl *elem = static_cast<HTMLElementImpl *>(node);
            NodeImpl *prev = elem->traversePreviousNodePostOrder();
            NodeImpl *next = elem->traverseNextNode();
            if (isHTMLStyleNode(style, elem)) {
                removeHTMLStyleNode(elem);
            }
            else {
                removeHTMLFontStyle(style, elem);
                removeCSSStyle(style, elem);
            }
            if (!elem->inDocument()) {
                if (s.node() == elem) {
                    // Since elem must have been fully selected, and it is at the start
                    // of the selection, it is clear we can set the new s offset to 0.
                    ASSERT(s.offset() <= s.node()->caretMinOffset());
                    s = Position(next, 0);
                }
                if (e.node() == elem) {
                    // Since elem must have been fully selected, and it is at the end
                    // of the selection, it is clear we can set the new e offset to
                    // the max range offset of prev.
                    ASSERT(e.offset() >= maxRangeOffset(e.node()));
                    e = Position(prev, maxRangeOffset(prev));
                }
            }
        }
        if (node == end.node())
            break;
        node = next;
    }


    if (textDecorationSpecialProperty) {
        style->deref();
    }
    
    ASSERT(s.node()->inDocument());
    ASSERT(e.node()->inDocument());
    setEndingSelection(Selection(s, VP_DEFAULT_AFFINITY, e, VP_DEFAULT_AFFINITY));
}

bool ApplyStyleCommand::nodeFullySelected(NodeImpl *node, const Position &start, const Position &end) const
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    Position pos = Position(node, node->childNodeCount()).upstream();
    return RangeImpl::compareBoundaryPoints(node, 0, start.node(), start.offset()) >= 0 &&
        RangeImpl::compareBoundaryPoints(pos, end) <= 0;
}

bool ApplyStyleCommand::nodeFullyUnselected(NodeImpl *node, const Position &start, const Position &end) const
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    Position pos = Position(node, node->childNodeCount()).upstream();
    bool isFullyBeforeStart = RangeImpl::compareBoundaryPoints(pos, start) < 0;
    bool isFullyAfterEnd = RangeImpl::compareBoundaryPoints(node, 0, end.node(), end.offset()) > 0;

    return isFullyBeforeStart || isFullyAfterEnd;
}


//------------------------------------------------------------------------------------------
// ApplyStyleCommand: style-application helpers

bool ApplyStyleCommand::splitTextAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > start.node()->caretMinOffset() && start.offset() < start.node()->caretMaxOffset()) {
        long endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
        TextImpl *text = static_cast<TextImpl *>(start.node());
        splitTextNode(text, start.offset());
        setEndingSelection(Selection(Position(start.node(), 0), SEL_DEFAULT_AFFINITY, Position(end.node(), end.offset() - endOffsetAdjustment), SEL_DEFAULT_AFFINITY));
        return true;
    }
    return false;
}

bool ApplyStyleCommand::splitTextAtEndIfNeeded(const Position &start, const Position &end)
{
    if (end.node()->isTextNode() && end.offset() > end.node()->caretMinOffset() && end.offset() < end.node()->caretMaxOffset()) {
        TextImpl *text = static_cast<TextImpl *>(end.node());
        splitTextNode(text, end.offset());
        
        NodeImpl *prevNode = text->previousSibling();
        ASSERT(prevNode);
        NodeImpl *startNode = start.node() == end.node() ? prevNode : start.node();
        ASSERT(startNode);
        setEndingSelection(Selection(Position(startNode, start.offset()), SEL_DEFAULT_AFFINITY, Position(prevNode, prevNode->caretMaxOffset()), SEL_DEFAULT_AFFINITY));
        return true;
    }
    return false;
}

bool ApplyStyleCommand::splitTextElementAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > start.node()->caretMinOffset() && start.offset() < start.node()->caretMaxOffset()) {
        long endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
        TextImpl *text = static_cast<TextImpl *>(start.node());
        splitTextNodeContainingElement(text, start.offset());

        setEndingSelection(Selection(Position(start.node()->parentNode(), start.node()->nodeIndex()), SEL_DEFAULT_AFFINITY, Position(end.node(), end.offset() - endOffsetAdjustment), SEL_DEFAULT_AFFINITY));
        return true;
    }
    return false;
}

bool ApplyStyleCommand::splitTextElementAtEndIfNeeded(const Position &start, const Position &end)
{
    if (end.node()->isTextNode() && end.offset() > end.node()->caretMinOffset() && end.offset() < end.node()->caretMaxOffset()) {
        TextImpl *text = static_cast<TextImpl *>(end.node());
        splitTextNodeContainingElement(text, end.offset());

        NodeImpl *prevNode = text->parent()->previousSibling()->lastChild();
        ASSERT(prevNode);
        NodeImpl *startNode = start.node() == end.node() ? prevNode : start.node();
        ASSERT(startNode);
        setEndingSelection(Selection(Position(startNode, start.offset()), SEL_DEFAULT_AFFINITY, Position(prevNode->parent(), prevNode->nodeIndex() + 1), SEL_DEFAULT_AFFINITY));
        return true;
    }
    return false;
}

static bool areIdenticalElements(NodeImpl *first, NodeImpl *second)
{
    // check that tag name and all attribute names and values are identical

    if (!first->isElementNode())
        return false;
    
    if (!second->isElementNode())
        return false;

    ElementImpl *firstElement = static_cast<ElementImpl *>(first);
    ElementImpl *secondElement = static_cast<ElementImpl *>(second);
    
    if (firstElement->id() != secondElement->id())
        return false;

    NamedAttrMapImpl *firstMap = firstElement->attributes();
    NamedAttrMapImpl *secondMap = secondElement->attributes();

    unsigned firstLength = firstMap->length();

    if (firstLength != secondMap->length())
        return false;

    for (unsigned i = 0; i < firstLength; i++) {
        DOM::AttributeImpl *attribute = firstMap->attributeItem(i);
        DOM::AttributeImpl *secondAttribute = secondMap->getAttributeItem(attribute->id());

        if (!secondAttribute || attribute->value() != secondAttribute->value())
            return false;
    }
    
    return true;
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position &start, const Position &end)
{
    NodeImpl *startNode = start.node();
    long startOffset = start.offset();

    if (start.node()->isAtomicNode()) {
        if (start.offset() != 0)
            return false;

        if (start.node()->previousSibling())
            return false;

        startNode = start.node()->parent();
        startOffset = 0;
    }

    if (!startNode->isElementNode())
        return false;

    if (startOffset != 0)
        return false;

    NodeImpl *previousSibling = startNode->previousSibling();

    if (previousSibling && areIdenticalElements(startNode, previousSibling)) {
        ElementImpl *previousElement = static_cast<ElementImpl *>(previousSibling);
        ElementImpl *element = static_cast<ElementImpl *>(startNode);
        NodeImpl *startChild = element->firstChild();
        ASSERT(startChild);
        mergeIdenticalElements(previousElement, element);

        long startOffsetAdjustment = startChild->nodeIndex();
        long endOffsetAdjustment = startNode == end.node() ? startOffsetAdjustment : 0;

        setEndingSelection(Selection(Position(startNode, startOffsetAdjustment), SEL_DEFAULT_AFFINITY,
                                     Position(end.node(), end.offset() + endOffsetAdjustment), SEL_DEFAULT_AFFINITY)); 

        return true;
    }

    return false;
}

bool ApplyStyleCommand::mergeEndWithNextIfIdentical(const Position &start, const Position &end)
{
    NodeImpl *endNode = end.node();
    int endOffset = end.offset();

    if (endNode->isAtomicNode()) {
        if (endOffset < endNode->caretMaxOffset())
            return false;

        unsigned parentLastOffset = end.node()->parent()->childNodes()->length() - 1;
        if (end.node()->nextSibling())
            return false;

        endNode = end.node()->parent();
        endOffset = parentLastOffset;
    }

    if (!endNode->isElementNode() || endNode->id() == ID_BR)
        return false;

    NodeImpl *nextSibling = endNode->nextSibling();

    if (nextSibling && areIdenticalElements(endNode, nextSibling)) {
        ElementImpl *nextElement = static_cast<ElementImpl *>(nextSibling);
        ElementImpl *element = static_cast<ElementImpl *>(endNode);
        NodeImpl *nextChild = nextElement->firstChild();

        mergeIdenticalElements(element, nextElement);

        NodeImpl *startNode = start.node() == endNode ? nextElement : start.node();
        ASSERT(startNode);

        int endOffset = nextChild ? nextChild->nodeIndex() : nextElement->childNodes()->length();

        setEndingSelection(Selection(Position(startNode, start.offset()), SEL_DEFAULT_AFFINITY, 
                                     Position(nextElement, endOffset), SEL_DEFAULT_AFFINITY));
        return true;
    }

    return false;
}

void ApplyStyleCommand::cleanUpEmptyStyleSpans(const Position &start, const Position &end)
{
    NodeImpl *node;
    for (node = start.node(); node && !node->previousSibling(); node = node->parentNode()) {
    }

    if (node && isEmptyStyleSpan(node->previousSibling())) {
        removeNodePreservingChildren(node->previousSibling());
    }

    if (start.node() == end.node()) {
        if (start.node()->isTextNode()) {
            for (NodeImpl *last = start.node(), *cur = last->parentNode(); cur && !last->previousSibling() && !last->nextSibling(); last = cur, cur = cur->parentNode()) {
                if (isEmptyStyleSpan(cur)) {
                    removeNodePreservingChildren(cur);
                    break;
                }
            }

        }
    } else {
        if (start.node()->isTextNode()) {
            for (NodeImpl *last = start.node(), *cur = last->parentNode(); cur && !last->previousSibling(); last = cur, cur = cur->parentNode()) {
                if (isEmptyStyleSpan(cur)) {
                    removeNodePreservingChildren(cur);
                    break;
                }
            }
        }

        if (end.node()->isTextNode()) {
            for (NodeImpl *last = end.node(), *cur = last->parentNode(); cur && !last->nextSibling(); last = cur, cur = cur->parentNode()) {
                if (isEmptyStyleSpan(cur)) {
                    removeNodePreservingChildren(cur);
                    break;
                }
            }
        }
    }
    
    for (node = end.node(); node && !node->nextSibling(); node = node->parentNode()) {
    }
    if (node && isEmptyStyleSpan(node->nextSibling())) {
        removeNodePreservingChildren(node->nextSibling());
    }
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

void ApplyStyleCommand::addBlockStyleIfNeeded(CSSMutableStyleDeclarationImpl *style, NodeImpl *node)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for
    // inline content.
    if (!node)
        return;
    
    HTMLElementImpl *block = static_cast<HTMLElementImpl *>(node->enclosingBlockFlowElement());
    if (!block)
        return;
        
    StyleChange styleChange(style, Position(block, 0), StyleChange::styleModeForParseMode(document()->inCompatMode()));
    if (styleChange.cssStyle().length() > 0) {
        moveParagraphContentsToNewBlockIfNecessary(Position(node, 0));
        block = static_cast<HTMLElementImpl *>(node->enclosingBlockFlowElement());
        DOMString cssText = styleChange.cssStyle();
        CSSMutableStyleDeclarationImpl *decl = block->inlineStyleDecl();
        if (decl)
            cssText += decl->cssText();
        setNodeAttribute(block, ATTR_STYLE, cssText);
    }
}

void ApplyStyleCommand::addInlineStyleIfNeeded(CSSMutableStyleDeclarationImpl *style, NodeImpl *startNode, NodeImpl *endNode)
{
    StyleChange styleChange(style, Position(startNode, 0), StyleChange::styleModeForParseMode(document()->inCompatMode()));
    int exceptionCode = 0;
    
    //
    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    //
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        ElementImpl *fontElement = createFontElement(document());
        ASSERT(exceptionCode == 0);
        insertNodeBefore(fontElement, startNode);
        if (styleChange.applyFontColor())
            fontElement->setAttribute(ATTR_COLOR, styleChange.fontColor());
        if (styleChange.applyFontFace())
            fontElement->setAttribute(ATTR_FACE, styleChange.fontFace());
        if (styleChange.applyFontSize())
            fontElement->setAttribute(ATTR_SIZE, styleChange.fontSize());
        surroundNodeRangeWithElement(startNode, endNode, fontElement);
    }

    if (styleChange.cssStyle().length() > 0) {
        ElementImpl *styleElement = createStyleSpanElement(document());
        styleElement->ref();
        styleElement->setAttribute(ATTR_STYLE, styleChange.cssStyle());
        insertNodeBefore(styleElement, startNode);
        styleElement->deref();
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

float ApplyStyleCommand::computedFontSize(const NodeImpl *node)
{
    float size = 0.0f;
    
    if (!node)
        return size;
    
    Position pos(const_cast<NodeImpl *>(node), 0);
    CSSComputedStyleDeclarationImpl *computedStyle = pos.computedStyle();
    if (!computedStyle)
        return size;
    computedStyle->ref();

    CSSPrimitiveValueImpl *value = static_cast<CSSPrimitiveValueImpl *>(computedStyle->getPropertyCSSValue(CSS_PROP_FONT_SIZE));
    if (value) {
        value->ref();
        size = value->getFloatValue(CSSPrimitiveValue::CSS_PX);
        value->deref();
    }

    computedStyle->deref();
    return size;
}

void ApplyStyleCommand::joinChildTextNodes(NodeImpl *node, const Position &start, const Position &end)
{
    if (!node)
        return;

    Position newStart = start;
    Position newEnd = end;
    
    NodeImpl *child = node->firstChild();
    while (child) {
        NodeImpl *next = child->nextSibling();
        if (child->isTextNode() && next && next->isTextNode()) {
            TextImpl *childText = static_cast<TextImpl *>(child);
            TextImpl *nextText = static_cast<TextImpl *>(next);
            if (next == start.node())
                newStart = Position(childText, childText->length() + start.offset());
            if (next == end.node())
                newEnd = Position(childText, childText->length() + end.offset());
            DOMString textToMove = nextText->data();
            insertTextIntoNode(childText, childText->length(), textToMove);
            removeNode(next);
            // don't move child node pointer. it may want to merge with more text nodes.
        }
        else {
            child = child->nextSibling();
        }
    }

    setEndingSelection(Selection(newStart, SEL_DEFAULT_AFFINITY, newEnd, SEL_DEFAULT_AFFINITY));
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
    start = positionOutsideContainingSpecialElement(start);
    Position end = m_selectionToDelete.end();
    end = positionOutsideContainingSpecialElement(end);

    m_upstreamStart = positionBeforePossibleContainingSpecialElement(start.upstream());
    m_downstreamStart = positionBeforePossibleContainingSpecialElement(start.downstream());
    m_upstreamEnd = positionAfterPossibleContainingSpecialElement(end.upstream());
    m_downstreamEnd = positionAfterPossibleContainingSpecialElement(end.downstream());

    //
    // Handle leading and trailing whitespace, as well as smart delete adjustments to the selection
    //
    m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(m_selectionToDelete.startAffinity());
    m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VP_DEFAULT_AFFINITY);

    if (m_smartDelete) {
    
        // skip smart delete if the selection to delete already starts or ends with whitespace
        Position pos = VisiblePosition(m_upstreamStart, m_selectionToDelete.startAffinity()).deepEquivalent();
        bool skipSmartDelete = pos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNotNull();
        if (!skipSmartDelete)
            skipSmartDelete = m_downstreamEnd.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNotNull();

        // extend selection upstream if there is whitespace there
        bool hasLeadingWhitespaceBeforeAdjustment = m_upstreamStart.leadingWhitespacePosition(m_selectionToDelete.startAffinity(), true).isNotNull();
        if (!skipSmartDelete && hasLeadingWhitespaceBeforeAdjustment) {
            VisiblePosition visiblePos = VisiblePosition(start, m_selectionToDelete.startAffinity()).previous();
            pos = visiblePos.deepEquivalent();
            // Expand out one character upstream for smart delete and recalculate
            // positions based on this change.
            m_upstreamStart = pos.upstream();
            m_downstreamStart = pos.downstream();
            m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(visiblePos.affinity());
        }
        
        // trailing whitespace is only considered for smart delete if there is no leading
        // whitespace, as in the case where you double-click the first word of a paragraph.
        if (!skipSmartDelete && !hasLeadingWhitespaceBeforeAdjustment && m_downstreamEnd.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNotNull()) {
            // Expand out one character downstream for smart delete and recalculate
            // positions based on this change.
            pos = VisiblePosition(end, m_selectionToDelete.endAffinity()).next().deepEquivalent();
            m_upstreamEnd = pos.upstream();
            m_downstreamEnd = pos.downstream();
            m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VP_DEFAULT_AFFINITY);
        }
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
    VisiblePosition visibleEnd(end, m_selectionToDelete.endAffinity());
    if (isFirstVisiblePositionInParagraph(visibleEnd) || isLastVisiblePositionInParagraph(visibleEnd)) {
        Position previousLineStart = previousLinePosition(visibleEnd, 0).deepEquivalent();
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

void DeleteSelectionCommand::insertPlaceholderForAncestorBlockContent()
{
    // This code makes sure a line does not disappear when deleting in this case:
    // <p>foo</p>bar<p>baz</p>
    // Select "bar" and hit delete. If nothing is done, the line containing bar will disappear.
    // It needs to be held open by inserting a placeholder.
    // Also see:
    // <rdar://problem/3928305> selecting an entire line and typing over causes new inserted text at top of document
    //
    // The checks below detect the case where the selection contains content in an ancestor block 
    // surrounded by child blocks.
    //
    VisiblePosition visibleStart(m_upstreamStart, VP_DEFAULT_AFFINITY);
    VisiblePosition beforeStart = visibleStart.previous();
    NodeImpl *startBlock = enclosingBlockFlowElement(visibleStart);
    NodeImpl *beforeStartBlock = enclosingBlockFlowElement(beforeStart);
    
    if (!beforeStart.isNull() &&
        !inSameBlock(visibleStart, beforeStart) &&
        beforeStartBlock->isAncestor(startBlock) &&
        startBlock != m_upstreamStart.node()) {

        VisiblePosition visibleEnd(m_downstreamEnd, VP_DEFAULT_AFFINITY);
        VisiblePosition afterEnd = visibleEnd.next();
        
        if ((!afterEnd.isNull() && !inSameBlock(afterEnd, visibleEnd) && !inSameBlock(afterEnd, visibleStart)) ||
            (m_downstreamEnd == m_selectionToDelete.end() && isEndOfParagraph(visibleEnd))) {
            NodeImpl *block = createDefaultParagraphElement(document());
            insertNodeBefore(block, m_upstreamStart.node());
            addBlockPlaceholderIfNeeded(block);
            m_endingPosition = Position(block, 0);
        }
    }
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

    // Not a special-case delete per se, but we can detect that the merging of content between blocks
    // should not be done.
    if (upstreamStartIsBR && downstreamStartIsBR)
        m_mergeBlocksAfterDelete = false;

    return false;
}

void DeleteSelectionCommand::setStartNode(NodeImpl *node)
{
    NodeImpl *old = m_startNode;
    m_startNode = node;
    if (m_startNode)
        m_startNode->ref();
    if (old)
        old->deref();
}

void DeleteSelectionCommand::handleGeneralDelete()
{
    int startOffset = m_upstreamStart.offset();
    VisiblePosition visibleEnd = VisiblePosition(m_downstreamEnd, m_selectionToDelete.endAffinity());
    bool endAtEndOfBlock = isEndOfBlock(visibleEnd);

    // Handle some special cases where the selection begins and ends on specific visible units.
    // Sometimes a node that is actually selected needs to be retained in order to maintain
    // user expectations for the delete operation. Here is an example:
    //     1. Open a new Blot or Mail document
    //     2. hit Return ten times or so
    //     3. Type a letter (do not hit Return after it)
    //     4. Type shift-up-arrow to select the line containing the letter and the previous blank line
    //     5. Hit Delete
    // You expect the insertion point to wind up at the start of the line where your selection began.
    // Because of the nature of HTML, the editing code needs to perform a special check to get
    // this behavior. So:
    // If the entire start block is selected, and the selection does not extend to the end of the 
    // end of a block other than the block containing the selection start, then do not delete the 
    // start block, otherwise delete the start block.
    // A similar case is provided to cover selections starting in BR elements.
    if (startOffset == 1 && m_startNode && m_startNode->id() == ID_BR) {
        setStartNode(m_startNode->traverseNextNode());
        startOffset = 0;
    }
    if (m_startBlock != m_endBlock && startOffset == 0 && m_startNode && m_startNode->id() == ID_BR && endAtEndOfBlock) {
        // Don't delete the BR element
        setStartNode(m_startNode->traverseNextNode());
    }
    else if (m_startBlock != m_endBlock && isStartOfBlock(VisiblePosition(m_upstreamStart, m_selectionToDelete.startAffinity()))) {
        if (!m_startBlock->isAncestor(m_endBlock) && !isStartOfBlock(visibleEnd) && endAtEndOfBlock) {
            // Delete all the children of the block, but not the block itself.
            setStartNode(m_startBlock->firstChild());
            startOffset = 0;
        }
    }
    else if (startOffset >= m_startNode->caretMaxOffset() &&
             (m_startNode->isAtomicNode() || startOffset == 0)) {
        // Move the start node to the next node in the tree since the startOffset is equal to
        // or beyond the start node's caretMaxOffset This means there is nothing visible to delete. 
        // But don't do this if the node is not atomic - we don't want to move into the first child.

        // Also, before moving on, delete any insignificant text that may be present in a text node.
        if (m_startNode->isTextNode()) {
            // Delete any insignificant text from this node.
            TextImpl *text = static_cast<TextImpl *>(m_startNode);
            if (text->length() > (unsigned)m_startNode->caretMaxOffset())
                deleteTextFromNode(text, m_startNode->caretMaxOffset(), text->length() - m_startNode->caretMaxOffset());
        }
        
        // shift the start node to the next
        setStartNode(m_startNode->traverseNextNode());
        startOffset = 0;
    }

    // Done adjusting the start.  See if we're all done.
    if (!m_startNode)
        return;

    if (m_startNode == m_downstreamEnd.node()) {
        // The selection to delete is all in one node.
        if (!m_startNode->renderer() || 
            (startOffset == 0 && m_downstreamEnd.offset() >= maxDeepOffset(m_startNode))) {
            // just delete
            removeFullySelectedNode(m_startNode);
        } else if (m_downstreamEnd.offset() - startOffset > 0) {
            if (m_startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                TextImpl *text = static_cast<TextImpl *>(m_startNode);
                deleteTextFromNode(text, startOffset, m_downstreamEnd.offset() - startOffset);
                m_trailingWhitespaceValid = false;
            } else {
                removeChildrenInRange(m_startNode, startOffset, m_downstreamEnd.offset());
                m_endingPosition = m_upstreamStart;
            }
        }
    }
    else {
        // The selection to delete spans more than one node.
        NodeImpl *node = m_startNode;
        
        if (startOffset > 0) {
            if (m_startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                TextImpl *text = static_cast<TextImpl *>(node);
                deleteTextFromNode(text, startOffset, text->length() - startOffset);
                node = node->traverseNextNode();
            } else {
                node = m_startNode->childNode(startOffset);
            }
        }
        
        // handle deleting all nodes that are completely selected
        while (node && node != m_downstreamEnd.node()) {
            if (RangeImpl::compareBoundaryPoints(Position(node, 0), m_downstreamEnd) >= 0) {
                // traverseNextSibling just blew past the end position, so stop deleting
                node = 0;
            } else if (!m_downstreamEnd.node()->isAncestor(node)) {
                NodeImpl *nextNode = node->traverseNextSibling();
                // if we just removed a node from the end container, update end position so the
                // check above will work
                if (node->parentNode() == m_downstreamEnd.node()) {
                    ASSERT(node->nodeIndex() < (unsigned)m_downstreamEnd.offset());
                    m_downstreamEnd = Position(m_downstreamEnd.node(), m_downstreamEnd.offset() - 1);
                }
                removeFullySelectedNode(node);
                node = nextNode;
            } else {
                NodeImpl *n = node->lastChild();
                while (n && n->lastChild())
                    n = n->lastChild();
                if (n == m_downstreamEnd.node() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMaxOffset()) {
                    removeFullySelectedNode(node);
                    m_trailingWhitespaceValid = false;
                    node = 0;
                } 
                else {
                    node = node->traverseNextNode();
                }
            }
        }

        
        if (m_downstreamEnd.node() != m_startNode && !m_upstreamStart.node()->isAncestor(m_downstreamEnd.node()) && m_downstreamEnd.node()->inDocument() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMinOffset()) {
            if (m_downstreamEnd.offset() >= maxDeepOffset(m_downstreamEnd.node())) {
                // need to delete whole node
                // we can get here if this is the last node in the block
                // remove an ancestor of m_downstreamEnd.node(), and thus m_downstreamEnd.node() itself
                if (!m_upstreamStart.node()->inDocument() ||
                    m_upstreamStart.node() == m_downstreamEnd.node() ||
                    m_upstreamStart.node()->isAncestor(m_downstreamEnd.node())) {
                    m_upstreamStart = Position(m_downstreamEnd.node()->parentNode(), m_downstreamEnd.node()->nodeIndex());
                }
                
                removeFullySelectedNode(m_downstreamEnd.node());
                m_trailingWhitespaceValid = false;
            } else {
                if (m_downstreamEnd.node()->isTextNode()) {
                    // in a text node that needs to be trimmed
                    TextImpl *text = static_cast<TextImpl *>(m_downstreamEnd.node());
                    if (m_downstreamEnd.offset() > 0) {
                        deleteTextFromNode(text, 0, m_downstreamEnd.offset());
                        m_downstreamEnd = Position(text, 0);
                        m_trailingWhitespaceValid = false;
                    }
                } else {
                    int offset = 0;
                    if (m_upstreamStart.node()->isAncestor(m_downstreamEnd.node())) {
                        NodeImpl *n = m_upstreamStart.node();
                        while (n && n->parentNode() != m_downstreamEnd.node())
                            n = n->parentNode();
                        if (n)
                            offset = n->nodeIndex() + 1;
                    }
                    removeChildrenInRange(m_downstreamEnd.node(), offset, m_downstreamEnd.offset());
                    m_downstreamEnd = Position(m_downstreamEnd.node(), offset);
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
            Position pos = m_endingPosition.downstream();
            pos = Position(pos.node(), pos.offset() - 1);
            if (nextCharacterIsCollapsibleWhitespace(pos) && !pos.isRenderedCharacter()) {
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
// from startNode and proceeding to the end of the paragraph. Nodes in the block containing
// startNode that appear in document order before startNode are not moved.
// This function is an important helper for deleting selections that cross paragraph
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
    if (isTableStructureNode(startBlock) || isListStructureNode(startBlock))
        // Do not move content between parts of a table or list.
        return;

    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    removeBlockPlaceholder(startBlock);

    // Move the subtree containing node
    NodeImpl *node = startNode->enclosingInlineElement();

    // Insert after the subtree containing destNode
    NodeImpl *refNode = dstNode->enclosingInlineElement();

    // Nothing to do if start is already at the beginning of dstBlock
    NodeImpl *dstBlock = refNode->enclosingBlockFlowElement();
    if (startBlock == dstBlock->firstChild())
        return;

    // Do the move.
    NodeImpl *rootNode = refNode->rootEditableElement();
    while (node && node->isAncestor(startBlock)) {
        NodeImpl *moveNode = node;
        node = node->nextSibling();
        removeNode(moveNode);
        if (moveNode->id() == ID_BR && !moveNode->renderer()) {
            // Just remove this node, and don't put it back.
            // If the BR was not rendered (since it was at the end of a block, for instance), 
            // putting it back in the document might make it appear, and that is not desirable.
            break;
        }
        if (refNode == rootNode)
            insertNodeAt(moveNode, refNode, 0);
        else
            insertNodeAfter(moveNode, refNode);
        refNode = moveNode;
        if (moveNode->id() == ID_BR)
            break;
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
        document()->updateLayout();
        if (refNode->renderer() && refNode->renderer()->inlineBox() && refNode->renderer()->inlineBox()->nextOnLineExists()) {
            insertNodeAfter(createBreakElement(document()), refNode);
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

void DeleteSelectionCommand::calculateTypingStyleAfterDelete(NodeImpl *insertedPlaceholder)
{
    // Compute the difference between the style before the delete and the style now
    // after the delete has been done. Set this style on the part, so other editing
    // commands being composed with this one will work, and also cache it on the command,
    // so the KHTMLPart::appliedEditing can set it after the whole composite command 
    // has completed.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSComputedStyleDeclarationImpl endingStyle(m_endingPosition.node());
    endingStyle.diff(m_typingStyle);
    if (!m_typingStyle->length()) {
        m_typingStyle->deref();
        m_typingStyle = 0;
    }
    if (insertedPlaceholder && m_typingStyle) {
        // Apply style to the placeholder. This makes sure that the single line in the
        // paragraph has the right height, and that the paragraph takes on the style
        // of the preceding line and retains it even if you click away, click back, and
        // then start typing. In this case, the typing style is applied right now, and
        // is not retained until the next typing action.

        // FIXME: is this even right? I don't think post-deletion typing style is supposed 
        // to be saved across clicking away and clicking back, it certainly isn't in TextEdit

        Position pastPlaceholder(insertedPlaceholder, 1);

        setEndingSelection(Selection(m_endingPosition, m_selectionToDelete.endAffinity(), pastPlaceholder, DOWNSTREAM));

        applyStyle(m_typingStyle, EditActionUnspecified);

        m_typingStyle->deref();
        m_typingStyle = 0;
    }
    // Set m_typingStyle as the typing style.
    // It's perfectly OK for m_typingStyle to be null.
    document()->part()->setTypingStyle(m_typingStyle);
    setTypingStyle(m_typingStyle);
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

    // save this to later make the selection with
    EAffinity affinity = m_selectionToDelete.startAffinity();
    
    // set up our state
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
    
    // if all we are deleting is complete paragraph(s), we need to make
    // sure a blank paragraph remains when we are done
    bool forceBlankParagraph = isStartOfParagraph(VisiblePosition(m_upstreamStart, VP_DEFAULT_AFFINITY)) &&
                               isEndOfParagraph(VisiblePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY));

    // Delete any text that may hinder our ability to fixup whitespace after the detele
    deleteInsignificantTextDownstream(m_trailingWhitespace);    

    saveTypingStyleState();
    insertPlaceholderForAncestorBlockContent();
    
    if (!handleSpecialCaseBRDelete())
        handleGeneralDelete();
    
    // Do block merge if start and end of selection are in different blocks.
    moveNodesAfterNode();
    
    calculateEndingPosition();
    fixupWhitespace();

    // if the m_endingPosition is already a blank paragraph, there is
    // no need to force a new one
    if (forceBlankParagraph &&
        isStartOfParagraph(VisiblePosition(m_endingPosition, VP_DEFAULT_AFFINITY)) &&
        isEndOfParagraph(VisiblePosition(m_endingPosition, VP_DEFAULT_AFFINITY))) {
        forceBlankParagraph = false;
    }
    
    NodeImpl *addedPlaceholder = forceBlankParagraph ? insertBlockPlaceholder(m_endingPosition) :
        addBlockPlaceholderIfNeeded(m_endingPosition.node());

    calculateTypingStyleAfterDelete(addedPlaceholder);
    debugPosition("endingPosition   ", m_endingPosition);
    setEndingSelection(Selection(m_endingPosition, affinity));
    clearTransientState();
    rebalanceWhitespace();
}

EditAction DeleteSelectionCommand::editingAction() const
{
    // Note that DeleteSelectionCommand is also used when the user presses the Delete key,
    // but in that case there's a TypingCommand that supplies the editingAction(), so
    // the Undo menu correctly shows "Undo Typing"
    return EditActionCut;
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

bool InsertLineBreakCommand::preservesTypingStyle() const
{
    return true;
}

void InsertLineBreakCommand::insertNodeAfterPosition(NodeImpl *node, const Position &pos)
{
    // Insert the BR after the caret position. In the case the
    // position is a block, do an append. We don't want to insert
    // the BR *after* the block.
    Position upstream(pos.upstream());
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
    Position upstream(pos.upstream());
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

    ElementImpl *breakNode = createBreakElement(document());
    NodeImpl *nodeToInsert = breakNode;
    
    Position pos(selection.start().upstream());

    pos = positionOutsideContainingSpecialElement(pos);

    bool atStart = pos.offset() <= pos.node()->caretMinOffset();
    bool atEnd = pos.offset() >= pos.node()->caretMaxOffset();
    bool atEndOfBlock = isEndOfBlock(VisiblePosition(pos, selection.startAffinity()));
    
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
                setEndingSelection(Selection(Position(next, 0), DOWNSTREAM));
            }
            else if (!document()->inStrictMode()) {
                // Insert an "extra" BR at the end of the block. 
                ElementImpl *extraBreakNode = createBreakElement(document());
                insertNodeAfter(extraBreakNode, nodeToInsert);
                setEndingSelection(Position(extraBreakNode, 0), DOWNSTREAM);
            }
        }
    }
    else if (atStart) {
        LOG(Editing, "input newline case 2");
        // Insert node before downstream position, and place caret there as well. 
        Position endingPosition = pos.downstream();
        insertNodeBeforePosition(nodeToInsert, endingPosition);
        setEndingSelection(endingPosition, DOWNSTREAM);
    }
    else if (atEnd) {
        LOG(Editing, "input newline case 3");
        // Insert BR after this node. Place caret in the position that is downstream
        // of the current position, reckoned before inserting the BR in between.
        Position endingPosition = pos.downstream();
        insertNodeAfterPosition(nodeToInsert, pos);
        setEndingSelection(endingPosition, DOWNSTREAM);
    }
    else {
        // Split a text node
        LOG(Editing, "input newline case 4");
        ASSERT(pos.node()->isTextNode());
        
        // Do the split
        int exceptionCode = 0;
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
        
        setEndingSelection(endingPosition, DOWNSTREAM);
    }

    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    
    if (typingStyle && typingStyle->length() > 0) {
        Selection selectionBeforeStyle = endingSelection();

        DOM::RangeImpl *rangeAroundNode = document()->createRange();
        int exception;
        rangeAroundNode->selectNode(nodeToInsert, exception);

        // affinity is not really important since this is a temp selection
        // just for calling applyStyle
        setEndingSelection(Selection(rangeAroundNode, khtml::SEL_DEFAULT_AFFINITY, khtml::SEL_DEFAULT_AFFINITY));
        applyStyle(typingStyle);

        setEndingSelection(selectionBeforeStyle);
    }

    rebalanceWhitespace();
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
    : CompositeEditCommand(document), m_style(0)
{
}

InsertParagraphSeparatorCommand::~InsertParagraphSeparatorCommand() 
{
    derefNodesInList(clonedNodes);
    if (m_style)
        m_style->deref();
}

bool InsertParagraphSeparatorCommand::preservesTypingStyle() const
{
    return true;
}

ElementImpl *InsertParagraphSeparatorCommand::createParagraphElement()
{
    ElementImpl *element = createDefaultParagraphElement(document());
    element->ref();
    clonedNodes.append(element);
    return element;
}

void InsertParagraphSeparatorCommand::calculateStyleBeforeInsertion(const Position &pos)
{
    // It is only important to set a style to apply later if we're at the boundaries of
    // a paragraph. Otherwise, content that is moved as part of the work of the command
    // will lend their styles to the new paragraph without any extra work needed.
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    if (!isFirstVisiblePositionInParagraph(visiblePos) && !isLastVisiblePositionInParagraph(visiblePos))
        return;
    
    if (m_style)
        m_style->deref();
    m_style = styleAtPosition(pos);
    m_style->ref();
}

void InsertParagraphSeparatorCommand::applyStyleAfterInsertion()
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!m_style)
        return;

    CSSComputedStyleDeclarationImpl endingStyle(endingSelection().start().node());
    endingStyle.diff(m_style);
    if (m_style->length() > 0) {
        applyStyle(m_style);
    }
}

void InsertParagraphSeparatorCommand::doApply()
{
    bool splitText = false;
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    Position pos = selection.start();
    EAffinity affinity = selection.startAffinity();
        
    // Delete the current selection.
    if (selection.isRange()) {
        calculateStyleBeforeInsertion(pos);
        deleteSelection(false, false);
        pos = endingSelection().start();
        affinity = endingSelection().startAffinity();
    }

    pos = positionOutsideContainingSpecialElement(pos);

    calculateStyleBeforeInsertion(pos);

    // Find the start block.
    NodeImpl *startNode = pos.node();
    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    if (!startBlock || !startBlock->parentNode())
        return;

    VisiblePosition visiblePos(pos, affinity);
    bool isFirstInBlock = isStartOfBlock(visiblePos);
    bool isLastInBlock = isEndOfBlock(visiblePos);
    bool startBlockIsRoot = startBlock == startBlock->rootEditableElement();

    // This is the block that is going to be inserted.
    NodeImpl *blockToInsert = startBlockIsRoot ? createParagraphElement() : startBlock->cloneNode(false);

    //---------------------------------------------------------------------
    // Handle empty block case.
    if (isFirstInBlock && isLastInBlock) {
        LOG(Editing, "insert paragraph separator: empty block case");
        if (startBlockIsRoot) {
            NodeImpl *extraBlock = createParagraphElement();
            appendNode(extraBlock, startBlock);
            appendBlockPlaceholder(extraBlock);
            appendNode(blockToInsert, startBlock);
        }
        else {
            insertNodeAfter(blockToInsert, startBlock);
        }
        appendBlockPlaceholder(blockToInsert);
        setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block. 
    if (isLastInBlock) {
        LOG(Editing, "insert paragraph separator: last in block case");
        if (startBlockIsRoot)
            appendNode(blockToInsert, startBlock);
        else
            insertNodeAfter(blockToInsert, startBlock);
        appendBlockPlaceholder(blockToInsert);
        setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block.
    // and similar case where upstream position is in another block.
    bool prevInDifferentBlock = !inSameBlock(visiblePos, visiblePos.previous());

    if (prevInDifferentBlock || isFirstInBlock) {
        LOG(Editing, "insert paragraph separator: first in block case");
        pos = pos.downstream();
        pos = positionOutsideContainingSpecialElement(pos);
        Position refPos;
        NodeImpl *refNode;
        if (isFirstInBlock && !startBlockIsRoot) {
            refNode = startBlock;
        } else if (pos.node() == startBlock && startBlockIsRoot) {
            ASSERT(startBlock->childNode(pos.offset())); // must be true or we'd be in the end of block case
            refNode = startBlock->childNode(pos.offset());
        } else {
            refNode = pos.node();
        }

        insertNodeBefore(blockToInsert, refNode);
        appendBlockPlaceholder(blockToInsert);
        setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        setEndingSelection(pos, DOWNSTREAM);
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    LOG(Editing, "insert paragraph separator: general case");

    // Check if pos.node() is a <br>. If it is, and the document is in quirks mode, 
    // then this <br> will collapse away when we add a block after it. Add an extra <br>.
    if (!document()->inStrictMode()) {
        Position upstreamPos = pos.upstream();
        if (upstreamPos.node()->id() == ID_BR)
            insertNodeAfter(createBreakElement(document()), upstreamPos.node());
    }
    
    // Move downstream. Typing style code will take care of carrying along the 
    // style of the upstream position.
    pos = pos.downstream();
    startNode = pos.node();

    // Build up list of ancestors in between the start node and the start block.
    if (startNode != startBlock) {
        for (NodeImpl *n = startNode->parentNode(); n && n != startBlock; n = n->parentNode())
            ancestors.prepend(n);
    }

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but pos.downstream() does not give it
    Position leadingWhitespace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY);
    if (leadingWhitespace.isNotNull()) {
        TextImpl *textNode = static_cast<TextImpl *>(leadingWhitespace.node());
        replaceTextInNode(textNode, leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    
    // Split at pos if in the middle of a text node.
    if (startNode->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(startNode);
        bool atEnd = (unsigned long)pos.offset() >= textNode->length();
        if (pos.offset() > 0 && !atEnd) {
            splitTextNode(textNode, pos.offset());
            pos = Position(startNode, 0);
            splitText = true;
        }
    }

    // Put the added block in the tree.
    if (startBlockIsRoot) {
        appendNode(blockToInsert, startBlock);
    } else {
        insertNodeAfter(blockToInsert, startBlock);
    }

    // Make clones of ancestors in between the start node and the start block.
    NodeImpl *parent = blockToInsert;
    for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
        NodeImpl *child = it.current()->cloneNode(false); // shallow clone
        child->ref();
        clonedNodes.append(child);
        appendNode(child, parent);
        parent = child;
    }

    // Insert a block placeholder if the next visible position is in a different paragraph,
    // because we know that there will be no content on the first line of the new block 
    // before the first block child. So, we need the placeholder to "hold the first line open".
    VisiblePosition next = visiblePos.next();
    if (!next.isNull() && !inSameBlock(visiblePos, next))
        appendBlockPlaceholder(blockToInsert);

    // Move the start node and the siblings of the start node.
    if (startNode != startBlock) {
        NodeImpl *n = startNode;
        if (pos.offset() >= startNode->caretMaxOffset()) {
            n = startNode->nextSibling();
        }
        while (n && n != blockToInsert) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent);
            n = next;
        }
    }            

    // Move everything after the start node.
    NodeImpl *leftParent = ancestors.last();
    while (leftParent && leftParent != startBlock) {
        parent = parent->parentNode();
        NodeImpl *n = leftParent->nextSibling();
        while (n && n != blockToInsert) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent);
            n = next;
        }
        leftParent = leftParent->parentNode();
    }

    // Handle whitespace that occurs after the split
    if (splitText) {
        document()->updateLayout();
        pos = Position(startNode, 0);
        if (!pos.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            ASSERT(startNode && startNode->isTextNode());
            deleteInsignificantTextDownstream(pos);
            insertTextIntoNode(static_cast<TextImpl *>(startNode), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(Position(blockToInsert, 0), DOWNSTREAM);
    rebalanceWhitespace();
    applyStyleAfterInsertion();
}

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorInQuotedContentCommand

InsertParagraphSeparatorInQuotedContentCommand::InsertParagraphSeparatorInQuotedContentCommand(DocumentImpl *document)
    : CompositeEditCommand(document), m_breakNode(0)
{
}

InsertParagraphSeparatorInQuotedContentCommand::~InsertParagraphSeparatorInQuotedContentCommand()
{
    derefNodesInList(clonedNodes);
    if (m_breakNode)
        m_breakNode->deref();
}

void InsertParagraphSeparatorInQuotedContentCommand::doApply()
{
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    // Delete the current selection.
    Position pos = selection.start();
    EAffinity affinity = selection.startAffinity();
    if (selection.isRange()) {
        deleteSelection(false, false);
        pos = endingSelection().start().upstream();
        affinity = endingSelection().startAffinity();
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
    
    // Insert a break after the top blockquote.
    m_breakNode = createBreakElement(document());
    m_breakNode->ref();
    insertNodeAfter(m_breakNode, topBlockquote);
    
    if (!isLastVisiblePositionInNode(VisiblePosition(pos, affinity), topBlockquote)) {
        
        NodeImpl *newStartNode = 0;
        // Split at pos if in the middle of a text node.
        if (startNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(startNode);
            bool atEnd = (unsigned long)pos.offset() >= textNode->length();
            if (pos.offset() > 0 && !atEnd) {
                splitTextNode(textNode, pos.offset());
                pos = Position(startNode, 0);
            }
            else if (atEnd) {
                newStartNode = startNode->traverseNextNode();
                ASSERT(newStartNode);
            }
        }
        else if (pos.offset() > 0) {
            newStartNode = startNode->traverseNextNode();
            ASSERT(newStartNode);
        }
        
        // If a new start node was determined, find a new top block quote.
        if (newStartNode) {
            startNode = newStartNode;
            for (NodeImpl *n = startNode->parentNode(); n; n = n->parentNode()) {
                if (isMailBlockquote(n))
                    topBlockquote = n;
            }
            if (!topBlockquote || !topBlockquote->parentNode())
                return;
        }
        
        // Build up list of ancestors in between the start node and the top blockquote.
        if (startNode != topBlockquote) {
            for (NodeImpl *n = startNode->parentNode(); n && n != topBlockquote; n = n->parentNode())
                ancestors.prepend(n);
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
        bool startIsBR = false;
        if (startNode != topBlockquote) {
            NodeImpl *n = startNode;
            startIsBR = n->id() == ID_BR;
            if (startIsBR)
                n = n->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
        }
        
        // Move everything after the start node.
        NodeImpl *leftParent = ancestors.last();
        
        // Insert an extra new line when the start is at the beginning of a line.
        if (!newStartNode && !startIsBR) {
            if (!leftParent)
                leftParent = topBlockquote;
            ElementImpl *b = createBreakElement(document());
            b->ref();
            clonedNodes.append(b);
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
        
        // Make sure the cloned block quote renders.
        addBlockPlaceholderIfNeeded(clonedBlockquote);
    }
    
    // Put the selection right before the break.
    setEndingSelection(Position(m_breakNode, 0), DOWNSTREAM);
    rebalanceWhitespace();
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

Position InsertTextCommand::prepareForTextInsertion(bool adjustDownstream)
{
    // Prepare for text input by looking at the current position.
    // It may be necessary to insert a text node to receive characters.
    Selection selection = endingSelection();
    ASSERT(selection.isCaret());
    
    Position pos = selection.start();
    if (adjustDownstream)
        pos = pos.downstream();
    else
        pos = pos.upstream();
    
    Selection typingStyleRange;

    pos = positionOutsideContainingSpecialElement(pos);

    if (!pos.node()->isTextNode()) {
        NodeImpl *textNode = document()->createEditingTextNode("");
        NodeImpl *nodeToInsert = textNode;

        // Now insert the node in the right place
        if (pos.node()->rootEditableElement() != NULL) {
            LOG(Editing, "prepareForTextInsertion case 1");
            insertNodeAt(nodeToInsert, pos.node(), pos.offset());
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

    return pos;
}

void InsertTextCommand::input(const DOMString &text, bool selectInsertedText)
{
    assert(text.find('\n') == -1);

    Selection selection = endingSelection();
    bool adjustDownstream = isFirstVisiblePositionOnLine(VisiblePosition(selection.start().downstream(), DOWNSTREAM));

    // Delete the current selection, or collapse whitespace, as needed
    if (selection.isRange())
        deleteSelection();
    
    // Delete any insignificant text that could get in the way of whitespace turning
    // out correctly after the insertion.
    selection = endingSelection();
    deleteInsignificantTextDownstream(selection.end().trailingWhitespacePosition(selection.endAffinity()));
    
    // Make sure the document is set up to receive text
    Position startPosition = prepareForTextInsertion(adjustDownstream);
    
    Position endPosition;

    TextImpl *textNode = static_cast<TextImpl *>(startPosition.node());
    long offset = startPosition.offset();

    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    removeBlockPlaceholder(textNode->enclosingBlockFlowElement());
    
    // These are temporary implementations for inserting adjoining spaces
    // into a document. We are working on a CSS-related whitespace solution
    // that will replace this some day. We hope.
    if (text == "\t") {
        // Treat a tab like a number of spaces. This seems to be the HTML editing convention,
        // although the number of spaces varies (we choose four spaces). 
        // Note that there is no attempt to make this work like a real tab stop, it is merely 
        // a set number of spaces. This also seems to be the HTML editing convention.
        for (int i = 0; i < spacesPerTab; i++) {
            insertSpace(textNode, offset);
            rebalanceWhitespace();
            document()->updateLayout();
        }
        
        endPosition = Position(textNode, offset + spacesPerTab);

        m_charactersAdded += spacesPerTab;
    }
    else if (text == " ") {
        insertSpace(textNode, offset);
        endPosition = Position(textNode, offset + 1);

        m_charactersAdded++;
        rebalanceWhitespace();
    }
    else {
        const DOMString &existingText = textNode->data();
        if (textNode->length() >= 2 && offset >= 2 && isNBSP(existingText[offset - 1]) && !isCollapsibleWhitespace(existingText[offset - 2])) {
            // DOM looks like this:
            // character nbsp caret
            // As we are about to insert a non-whitespace character at the caret
            // convert the nbsp to a regular space.
            // EDIT FIXME: This needs to be improved some day to convert back only
            // those nbsp's added by the editor to make rendering come out right.
            replaceTextInNode(textNode, offset - 1, 1, " ");
        }
        insertTextIntoNode(textNode, offset, text);
        endPosition = Position(textNode, offset + text.length());

        m_charactersAdded += text.length();
    }

    setEndingSelection(Selection(startPosition, DOWNSTREAM, endPosition, SEL_DEFAULT_AFFINITY));

    // Handle the case where there is a typing style.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    if (typingStyle && typingStyle->length() > 0)
        applyStyle(typingStyle);

    if (!selectInsertedText)
        setEndingSelection(endingSelection().end(), endingSelection().endAffinity());
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
        if (isCollapsibleWhitespace(text[i]))
            count++;
        else 
            break;
    }
    if (count > 0) {
        // By checking the character at the downstream position, we can
        // check if there is a rendered WS at the caret
        Position pos(textNode, offset);
        Position downstream = pos.downstream();
        if (downstream.offset() < (long)text.length() && isCollapsibleWhitespace(text[downstream.offset()]))
            count--; // leave this WS in
        if (count > 0)
            deleteTextFromNode(textNode, offset, count);
    }

    if (offset > 0 && offset <= text.length() - 1 && !isCollapsibleWhitespace(text[offset]) && !isCollapsibleWhitespace(text[offset - 1])) {
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

    Position pos = m_position;
    if (pos.isNull())
        return;
        
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
        pos = Position(positionNode, positionOffset);
    }

    deleteSelection(m_smartMove);

    // If the node for the destination has been removed as a result of the deletion,
    // set the destination to the ending point after the deletion.
    // Fixes: <rdar://problem/3910425> REGRESSION (Mail): Crash in ReplaceSelectionCommand; 
    //        selection is empty, leading to null deref
    if (!pos.node()->inDocument())
        pos = endingSelection().start();

    setEndingSelection(pos, endingSelection().startAffinity());
    EditCommandPtr cmd(new ReplaceSelectionCommand(document(), m_fragment, true, m_smartMove));
    applyCommandToComposite(cmd);
}

EditAction MoveSelectionCommand::editingAction() const
{
    return EditActionDrag;
}

//------------------------------------------------------------------------------------------
// RebalanceWhitespaceCommand

RebalanceWhitespaceCommand::RebalanceWhitespaceCommand(DocumentImpl *document, const Position &pos)
    : EditCommand(document), m_position(pos), m_upstreamOffset(InvalidOffset), m_downstreamOffset(InvalidOffset)
{
}

RebalanceWhitespaceCommand::~RebalanceWhitespaceCommand()
{
}

void RebalanceWhitespaceCommand::doApply()
{
    static DOMString space(" ");

    if (m_position.isNull() || !m_position.node()->isTextNode())
        return;
        
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    if (text.length() == 0)
        return;
    
    // find upstream offset
    long upstream = m_position.offset();
    while (upstream > 0 && isCollapsibleWhitespace(text[upstream - 1]) || isNBSP(text[upstream - 1])) {
        upstream--;
        m_upstreamOffset = upstream;
    }

    // find downstream offset
    long downstream = m_position.offset();
    while ((unsigned)downstream < text.length() && isCollapsibleWhitespace(text[downstream]) || isNBSP(text[downstream])) {
        downstream++;
        m_downstreamOffset = downstream;
    }

    if (m_upstreamOffset == InvalidOffset && m_downstreamOffset == InvalidOffset)
        return;
        
    m_upstreamOffset = upstream;
    m_downstreamOffset = downstream;
    long length = m_downstreamOffset - m_upstreamOffset;
    
    m_beforeString = text.substring(m_upstreamOffset, length);
    
    // The following loop figures out a "rebalanced" whitespace string for any length
    // string, and takes into account the special cases that need to handled for the
    // start and end of strings (i.e. first and last character must be an nbsp.
    long i = m_upstreamOffset;
    while (i < m_downstreamOffset) {
        long add = (m_downstreamOffset - i) % 3;
        switch (add) {
            case 0:
                m_afterString += nonBreakingSpaceString();
                m_afterString += space;
                m_afterString += nonBreakingSpaceString();
                add = 3;
                break;
            case 1:
                if (i == 0 || (unsigned)i + 1 == text.length()) // at start or end of string
                    m_afterString += nonBreakingSpaceString();
                else
                    m_afterString += space;
                break;
            case 2:
                if ((unsigned)i + 2 == text.length()) {
                     // at end of string
                    m_afterString += nonBreakingSpaceString();
                    m_afterString += nonBreakingSpaceString();
                }
                else {
                    m_afterString += nonBreakingSpaceString();
                    m_afterString += space;
                }
                break;
        }
        i += add;
    }
    
    text.remove(m_upstreamOffset, length);
    text.insert(m_afterString, m_upstreamOffset);
}

void RebalanceWhitespaceCommand::doUnapply()
{
    if (m_upstreamOffset == InvalidOffset && m_downstreamOffset == InvalidOffset)
        return;
    
    ASSERT(m_position.node()->isTextNode());
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    text.remove(m_upstreamOffset, m_afterString.length());
    text.insert(m_beforeString, m_upstreamOffset);
}

bool RebalanceWhitespaceCommand::preservesTypingStyle() const
{
    return true;
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

ReplacementFragment::ReplacementFragment(DocumentImpl *document, DocumentFragmentImpl *fragment, bool matchStyle)
    : m_document(document), 
      m_fragment(fragment), 
      m_matchStyle(matchStyle), 
      m_hasInterchangeNewlineAtStart(false), 
      m_hasInterchangeNewlineAtEnd(false), 
      m_hasMoreThanOneBlock(false)
{
    if (!m_document)
        return;

    if (!m_fragment) {
        m_type = EmptyFragment;
        return;
    }

    m_document->ref();
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

    NodeImpl *node = m_fragment->firstChild();
    NodeImpl *newlineAtStartNode = 0;
    NodeImpl *newlineAtEndNode = 0;
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (isInterchangeNewlineNode(node)) {
            if (next || node == m_fragment->firstChild()) {
                m_hasInterchangeNewlineAtStart = true;
                newlineAtStartNode = node;
            }
            else {
                m_hasInterchangeNewlineAtEnd = true;
                newlineAtEndNode = node;
            }
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
        node = next;
    }

    if (newlineAtStartNode)
        removeNode(newlineAtStartNode);
    if (newlineAtEndNode)
        removeNode(newlineAtEndNode);
    
    NodeImpl *holder = insertFragmentForTestRendering();
    if (holder)
        holder->ref();
    if (!m_matchStyle) {
        computeStylesUsingTestRendering(holder);
    }
    removeUnrenderedNodesUsingTestRendering(holder);
    m_hasMoreThanOneBlock = countRenderedBlocks(holder) > 1;
    restoreTestRenderingNodesToFragment(holder);
    removeNode(holder);
    holder->deref();
    removeStyleNodes();
}

ReplacementFragment::~ReplacementFragment()
{
    if (m_document)
        m_document->deref();
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
    while (node && isProbablyBlock(node) && !isMailPasteAsQuotationNode(node))
        node = node->traverseNextNode();
    return node;
}

void ReplacementFragment::pruneEmptyNodes()
{
    bool run = true;
    while (run) {
        run = false;
        NodeImpl *node = m_fragment->firstChild();
        while (node) {
            if ((node->isTextNode() && static_cast<TextImpl *>(node)->length() == 0) ||
                (isProbablyBlock(node) && !isProbablyTableStructureNode(node) && node->childNodeCount() == 0)) {
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

bool ReplacementFragment::isInterchangeNewlineNode(const NodeImpl *node)
{
    static DOMString interchangeNewlineClassString(AppleInterchangeNewline);
    return node && node->id() == ID_BR && static_cast<const ElementImpl *>(node)->getAttribute(ATTR_CLASS) == interchangeNewlineClassString;
}

bool ReplacementFragment::isInterchangeConvertedSpaceSpan(const NodeImpl *node)
{
    static DOMString convertedSpaceSpanClassString(AppleConvertedSpace);
    return node->isHTMLElement() && static_cast<const HTMLElementImpl *>(node)->getAttribute(ATTR_CLASS) == convertedSpaceSpanClassString;
}

NodeImpl *ReplacementFragment::enclosingBlock(NodeImpl *node) const
{
    while (node && !isProbablyBlock(node))
        node = node->parentNode();    
    return node ? node : m_fragment;
}

void ReplacementFragment::removeNodePreservingChildren(NodeImpl *node)
{
    if (!node)
        return;

    while (NodeImpl *n = node->firstChild()) {
        n->ref();
        removeNode(n);
        insertNodeBefore(n, node);
        n->deref();
    }
    removeNode(node);
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

NodeImpl *ReplacementFragment::insertFragmentForTestRendering()
{
    NodeImpl *body = m_document->body();
    if (!body)
        return 0;

    ElementImpl *holder = createDefaultParagraphElement(m_document);
    holder->ref();
    
    int exceptionCode = 0;
    holder->appendChild(m_fragment, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    body->appendChild(holder, exceptionCode);
    ASSERT(exceptionCode == 0);
    holder->deref();
    
    m_document->updateLayout();
    
    return holder;
}

void ReplacementFragment::restoreTestRenderingNodesToFragment(NodeImpl *holder)
{
    if (!holder)
        return;

    int exceptionCode = 0;
    while (NodeImpl *node = holder->firstChild()) {
        node->ref();
        holder->removeChild(node, exceptionCode);
        ASSERT(exceptionCode == 0);
        m_fragment->appendChild(node, exceptionCode);
        ASSERT(exceptionCode == 0);
        node->deref();
    }
}

void ReplacementFragment::computeStylesUsingTestRendering(NodeImpl *holder)
{
    if (!holder)
        return;

    m_document->updateLayout();

    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder))
        computeAndStoreNodeDesiredStyle(node, m_styles);
}

void ReplacementFragment::removeUnrenderedNodesUsingTestRendering(NodeImpl *holder)
{
    if (!holder)
        return;

    QPtrList<NodeImpl> unrendered;

    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (!isNodeRendered(node) && !isTableStructureNode(node))
            unrendered.append(node);
    }

    for (QPtrListIterator<NodeImpl> it(unrendered); it.current(); ++it)
        removeNode(it.current());
}

int ReplacementFragment::countRenderedBlocks(NodeImpl *holder)
{
    if (!holder)
        return 0;
    
    int count = 0;
    NodeImpl *prev = 0;
    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (node->isBlockFlow()) {
            if (!prev) {
                count++;
                prev = node;
            }
        }
        else {
            NodeImpl *block = node->enclosingBlockFlowElement();
            if (block != prev) {
                count++;
                prev = block;
            }
        }
    }
    
    return count;
}

void ReplacementFragment::removeStyleNodes()
{
    // Since style information has been computed and cached away in
    // computeStylesForNodes(), these style nodes can be removed, since
    // the correct styles will be added back in fixupNodeStyles().
    NodeImpl *node = m_fragment->firstChild();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        // This list of tags change the appearance of content
        // in ways we can add back on later with CSS, if necessary.
        if (node->id() == ID_B || 
            node->id() == ID_BIG || 
            node->id() == ID_CENTER || 
            node->id() == ID_FONT || 
            node->id() == ID_I || 
            node->id() == ID_S || 
            node->id() == ID_SMALL || 
            node->id() == ID_STRIKE || 
            node->id() == ID_SUB || 
            node->id() == ID_SUP || 
            node->id() == ID_TT || 
            node->id() == ID_U || 
            isStyleSpan(node)) {
            removeNodePreservingChildren(node);
        }
        else if (node->isHTMLElement()) {
            HTMLElementImpl *elem = static_cast<HTMLElementImpl *>(node);
            CSSMutableStyleDeclarationImpl *inlineStyleDecl = elem->inlineStyleDecl();
            if (inlineStyleDecl) {
                inlineStyleDecl->removeBlockProperties();
                inlineStyleDecl->removeInheritableProperties();
            }
        }
        node = next;
    }
}

NodeDesiredStyle::NodeDesiredStyle(NodeImpl *node, CSSMutableStyleDeclarationImpl *style) 
    : m_node(node), m_style(style)
{
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
}

NodeDesiredStyle::NodeDesiredStyle(const NodeDesiredStyle &other)
    : m_node(other.node()), m_style(other.style())
{
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
}

NodeDesiredStyle::~NodeDesiredStyle()
{
    if (m_node)
        m_node->deref();
    if (m_style)
        m_style->deref();
}

NodeDesiredStyle &NodeDesiredStyle::operator=(const NodeDesiredStyle &other)
{
    NodeImpl *oldNode = m_node;
    CSSMutableStyleDeclarationImpl *oldStyle = m_style;

    m_node = other.node();
    m_style = other.style();
    
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
    
    if (oldNode)
        oldNode->deref();
    if (oldStyle)
        oldStyle->deref();
        
    return *this;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, bool selectReplacement, bool smartReplace, bool matchStyle) 
    : CompositeEditCommand(document), 
      m_fragment(document, fragment, matchStyle),
      m_firstNodeInserted(0),
      m_lastNodeInserted(0),
      m_lastTopNodeInserted(0),
      m_insertionStyle(0),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace),
      m_matchStyle(matchStyle)
{
}

ReplaceSelectionCommand::~ReplaceSelectionCommand()
{
    if (m_firstNodeInserted)
        m_firstNodeInserted->deref();
    if (m_lastNodeInserted)
        m_lastNodeInserted->deref();
    if (m_lastTopNodeInserted)
        m_lastTopNodeInserted->deref();
    if (m_insertionStyle)
        m_insertionStyle->deref();
}

void ReplaceSelectionCommand::doApply()
{
    // collect information about the current selection, prior to deleting the selection
    Selection selection = endingSelection();
    ASSERT(selection.isCaretOrRange());

    VisiblePosition visibleStart(selection.start(), selection.startAffinity());
    VisiblePosition visibleEnd(selection.end(), selection.endAffinity());
    bool startAtStartOfBlock = isStartOfBlock(visibleStart);
    bool startAtEndOfBlock = isEndOfBlock(visibleStart);
    bool startAtBlockBoundary = startAtStartOfBlock || startAtEndOfBlock;
    NodeImpl *startBlock = selection.start().node()->enclosingBlockFlowElement();
    NodeImpl *endBlock = selection.end().node()->enclosingBlockFlowElement();

    // decide whether to later merge content into the startBlock
    bool mergeStart = false;
    if (startBlock == startBlock->rootEditableElement() && startAtStartOfBlock && startAtEndOfBlock) {
        // empty editable subtree, need to mergeStart so that fragment ends up
        // inside the editable subtree rather than just before it
        mergeStart = false;
    } else {
        // merge if current selection starts inside a paragraph, or there is only one block and no interchange newline to add
        mergeStart = !m_fragment.hasInterchangeNewlineAtStart() && 
            (!isStartOfParagraph(visibleStart) || (!m_fragment.hasInterchangeNewlineAtEnd() && !m_fragment.hasMoreThanOneBlock())) &&
            !isLastVisiblePositionInSpecialElement(selection.start());
        
        // This is a workaround for this bug:
        // <rdar://problem/4013642> REGRESSION (Mail): Copied quoted word does not paste as a quote if pasted at the start of a line
        // We need more powerful logic in this whole mergeStart code for this case to come out right without
        // breaking other cases.
        if (isStartOfParagraph(visibleStart) && isMailBlockquote(m_fragment.firstChild()))
            mergeStart = false;
    }
    
    // decide whether to later append nodes to the end
    NodeImpl *beyondEndNode = 0;
    if (!isEndOfParagraph(visibleEnd) && !m_fragment.hasInterchangeNewlineAtEnd()) {
        Position beyondEndPos = selection.end().downstream();
        if (!isFirstVisiblePositionInSpecialElement(beyondEndPos))
            beyondEndNode = beyondEndPos.node();
    }
    bool moveNodesAfterEnd = beyondEndNode && (startBlock != endBlock || m_fragment.hasMoreThanOneBlock());

    Position startPos = selection.start();
    
    // delete the current range selection, or insert paragraph for caret selection, as needed
    if (selection.isRange()) {
        deleteSelection(false, !(m_fragment.hasInterchangeNewlineAtStart() || m_fragment.hasInterchangeNewlineAtEnd() || m_fragment.hasMoreThanOneBlock()));
        document()->updateLayout();
        visibleStart = VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY);
        if (m_fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            }
            else {
                insertParagraphSeparator();
                setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY));
            }
        }
        startPos = endingSelection().start();
    } 
    else {
        ASSERT(selection.isCaret());
        if (m_fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            }
            else {
                insertParagraphSeparator();
                setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY));
            }
        }
        if (!m_fragment.hasInterchangeNewlineAtEnd() && m_fragment.hasMoreThanOneBlock() && 
            !startAtBlockBoundary && !isEndOfParagraph(visibleEnd)) {
            // The start and the end need to wind up in separate blocks.
            // Insert a paragraph separator to make that happen.
            insertParagraphSeparator();
            setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY).previous());
        }
        startPos = endingSelection().start();
    }

    if (startAtStartOfBlock && startBlock->inDocument())
        startPos = Position(startBlock, 0);

    startPos = positionOutsideContainingSpecialElement(startPos);

    KHTMLPart *part = document()->part();
    if (m_matchStyle) {
        m_insertionStyle = styleAtPosition(startPos);
        m_insertionStyle->ref();
    }
    
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    part->clearTypingStyle();
    setTypingStyle(0);    
    
    // done if there is nothing to add
    if (!m_fragment.firstChild())
        return;
    
    // check for a line placeholder, and store it away for possible removal later.
    NodeImpl *block = startPos.node()->enclosingBlockFlowElement();
    NodeImpl *linePlaceholder = findBlockPlaceholder(block);
    if (!linePlaceholder) {
        Position downstream = startPos.downstream();
        downstream = positionOutsideContainingSpecialElement(downstream);
        if (downstream.node()->id() == ID_BR && downstream.offset() == 0 && 
            m_fragment.hasInterchangeNewlineAtEnd() &&
            isFirstVisiblePositionOnLine(VisiblePosition(downstream, VP_DEFAULT_AFFINITY)))
            linePlaceholder = downstream.node();
    }
    
    // check whether to "smart replace" needs to add leading and/or trailing space
    bool addLeadingSpace = false;
    bool addTrailingSpace = false;
    // FIXME: We need the affinity for startPos and endPos, but Position::downstream
    // and Position::upstream do not give it
    if (m_smartReplace) {
        VisiblePosition visiblePos = VisiblePosition(startPos, VP_DEFAULT_AFFINITY);
        assert(visiblePos.isNotNull());
        addLeadingSpace = startPos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isFirstVisiblePositionOnLine(visiblePos);
        if (addLeadingSpace) {
            QChar previousChar = visiblePos.previous().character();
            if (!previousChar.isNull()) {
                addLeadingSpace = !part->isCharacterSmartReplaceExempt(previousChar, true);
            }
        }
        addTrailingSpace = startPos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isLastVisiblePositionOnLine(visiblePos);
        if (addTrailingSpace) {
            QChar thisChar = visiblePos.character();
            if (!thisChar.isNull()) {
                addTrailingSpace = !part->isCharacterSmartReplaceExempt(thisChar, false);
            }
        }
    }
    
    // There are five steps to adding the content: merge blocks at start, add remaining blocks,
    // add "smart replace" space, handle trailing newline, clean up.
    
    // initially, we say the insertion point is the start of selection
    document()->updateLayout();
    Position insertionPos = startPos;

    // step 1: merge content into the start block, if that is needed
    if (mergeStart && !isFirstVisiblePositionInSpecialElementInFragment(Position(m_fragment.mergeStartNode(), 0))) {
        NodeImpl *refNode = m_fragment.mergeStartNode();
        if (refNode) {
            NodeImpl *node = refNode ? refNode->nextSibling() : 0;
            insertNodeAtAndUpdateNodesInserted(refNode, startPos.node(), startPos.offset());
            while (node && !isProbablyBlock(node)) {
                NodeImpl *next = node->nextSibling();
                insertNodeAfterAndUpdateNodesInserted(node, refNode);
                refNode = node;
                node = next;
            }
        }
        
        // update insertion point to be at the end of the last block inserted
        if (m_lastNodeInserted) {
            document()->updateLayout();
            insertionPos = Position(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
        }
    }

    // prune empty nodes from fragment
    // NOTE: why was this not done earlier, before the mergeStart?
    m_fragment.pruneEmptyNodes();
    
    // step 2 : merge everything remaining in the fragment
    if (m_fragment.firstChild()) {
        NodeImpl *refNode = m_fragment.firstChild();
        NodeImpl *node = refNode ? refNode->nextSibling() : 0;
        NodeImpl *insertionBlock = insertionPos.node()->enclosingBlockFlowElement();
        bool insertionBlockIsRoot = insertionBlock == insertionBlock->rootEditableElement();
        VisiblePosition visiblePos(insertionPos, DOWNSTREAM);
        if (!insertionBlockIsRoot && isProbablyBlock(refNode) && isStartOfBlock(visiblePos))
            insertNodeBeforeAndUpdateNodesInserted(refNode, insertionBlock);
        else if (!insertionBlockIsRoot && isProbablyBlock(refNode) && isEndOfBlock(visiblePos)) {
            insertNodeAfterAndUpdateNodesInserted(refNode, insertionBlock);
        } else if (mergeStart && !isProbablyBlock(refNode)) {
            Position pos = visiblePos.next().deepEquivalent().downstream();
            insertNodeAtAndUpdateNodesInserted(refNode, pos.node(), pos.offset());
        } else {
            insertNodeAtAndUpdateNodesInserted(refNode, insertionPos.node(), insertionPos.offset());
        }
        
        while (node) {
            NodeImpl *next = node->nextSibling();
            insertNodeAfterAndUpdateNodesInserted(node, refNode);
            refNode = node;
            node = next;
        }
        document()->updateLayout();
        insertionPos = Position(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
    }

    // step 3 : handle "smart replace" whitespace
    if (addTrailingSpace && m_lastNodeInserted) {
        document()->updateLayout();
        Position pos(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
        bool needsTrailingSpace = pos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull();
        if (needsTrailingSpace) {
            if (m_lastNodeInserted->isTextNode()) {
                TextImpl *text = static_cast<TextImpl *>(m_lastNodeInserted);
                insertTextIntoNode(text, text->length(), nonBreakingSpaceString());
                insertionPos = Position(text, text->length());
            }
            else {
                NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeAfterAndUpdateNodesInserted(node, m_lastNodeInserted);
                insertionPos = Position(node, 1);
            }
        }
    }

    if (addLeadingSpace && m_firstNodeInserted) {
        document()->updateLayout();
        Position pos(m_firstNodeInserted, 0);
        bool needsLeadingSpace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull();
        if (needsLeadingSpace) {
            if (m_firstNodeInserted->isTextNode()) {
                TextImpl *text = static_cast<TextImpl *>(m_firstNodeInserted);
                insertTextIntoNode(text, 0, nonBreakingSpaceString());
            } else {
                NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeBeforeAndUpdateNodesInserted(node, m_firstNodeInserted);
            }
        }
    }
    
    Position lastPositionToSelect;

    // step 4 : handle trailing newline
    if (m_fragment.hasInterchangeNewlineAtEnd()) {
        removeLinePlaceholderIfNeeded(linePlaceholder);

        if (!m_lastNodeInserted) {
            lastPositionToSelect = endingSelection().end().downstream();
        }
        else {
            bool insertParagraph = false;
            VisiblePosition pos(insertionPos, VP_DEFAULT_AFFINITY);

            if (startBlock == endBlock && !isProbablyBlock(m_lastTopNodeInserted)) {
                insertParagraph = true;
            } else {
                // Handle end-of-document case.
                document()->updateLayout();
                if (isEndOfDocument(pos))
                    insertParagraph = true;
            }
            if (insertParagraph) {
                setEndingSelection(insertionPos, DOWNSTREAM);
                insertParagraphSeparator();
                VisiblePosition next = pos.next();

                // Select up to the paragraph separator that was added.
                lastPositionToSelect = next.deepEquivalent().downstream();
                updateNodesInserted(lastPositionToSelect.node());
            } else {
                // Select up to the preexising paragraph separator.
                VisiblePosition next = pos.next();
                lastPositionToSelect = next.deepEquivalent().downstream();
            }
        }
    } 
    else {
        if (m_lastNodeInserted && m_lastNodeInserted->id() == ID_BR && !document()->inStrictMode()) {
            document()->updateLayout();
            VisiblePosition pos(Position(m_lastNodeInserted, 1), DOWNSTREAM);
            if (isEndOfBlock(pos)) {
                NodeImpl *next = m_lastNodeInserted->traverseNextNode();
                bool hasTrailingBR = next && next->id() == ID_BR && m_lastNodeInserted->enclosingBlockFlowElement() == next->enclosingBlockFlowElement();
                if (!hasTrailingBR) {
                    // Insert an "extra" BR at the end of the block. 
                    insertNodeBefore(createBreakElement(document()), m_lastNodeInserted);
                }
            }
        }

        if (moveNodesAfterEnd && !isLastVisiblePositionInSpecialElement(Position(m_lastNodeInserted, maxRangeOffset(m_lastNodeInserted)))) {
            document()->updateLayout();
            QValueList<NodeDesiredStyle> styles;
            QPtrList<NodeImpl> blocks;
            NodeImpl *node = beyondEndNode;
            NodeImpl *refNode = m_lastNodeInserted;
            while (node) {
                RenderObject *renderer = node->renderer();
                // Stop at the first table or block.
                if (renderer && (renderer->isBlockFlow() || renderer->isTable()))
                    break;
                NodeImpl *next = node->nextSibling();
                blocks.append(node->enclosingBlockFlowElement());
                computeAndStoreNodeDesiredStyle(node, styles);
                removeNode(node);
                // No need to update inserted node variables.
                insertNodeAfter(node, refNode);
                refNode = node;
                // We want to move the first BR we see, so check for that here.
                if (node->id() == ID_BR)
                    break;
                node = next;
            }
            document()->updateLayout();
            for (QPtrListIterator<NodeImpl> it(blocks); it.current(); ++it) {
                NodeImpl *blockToRemove = it.current();
                if (!blockToRemove->inDocument())
                    continue;
                if (!blockToRemove->renderer() || !blockToRemove->renderer()->firstChild()) {
                    if (blockToRemove->parentNode())
                        blocks.append(blockToRemove->parentNode()->enclosingBlockFlowElement());
                    removeNode(blockToRemove);
                    document()->updateLayout();
                }
            }

            fixupNodeStyles(styles);
        }
    }
    
    if (!m_matchStyle)
        fixupNodeStyles(m_fragment.desiredStyles());
    completeHTMLReplacement(lastPositionToSelect);
    
    // step 5 : mop up
    removeLinePlaceholderIfNeeded(linePlaceholder);
}

void ReplaceSelectionCommand::removeLinePlaceholderIfNeeded(NodeImpl *linePlaceholder)
{
    if (!linePlaceholder)
        return;
        
    document()->updateLayout();
    if (linePlaceholder->inDocument()) {
        VisiblePosition placeholderPos(linePlaceholder, linePlaceholder->renderer()->caretMinOffset(), DOWNSTREAM);
        if (placeholderPos.next().isNull() ||
            !(isFirstVisiblePositionOnLine(placeholderPos) && isLastVisiblePositionOnLine(placeholderPos))) {
            NodeImpl *block = linePlaceholder->enclosingBlockFlowElement();
            removeNode(linePlaceholder);
            document()->updateLayout();
            if (!block->renderer() || block->renderer()->height() == 0)
                removeNode(block);
        }
    }
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start;
    Position end;

    if (m_firstNodeInserted && m_firstNodeInserted->inDocument() &&
        m_lastNodeInserted && m_lastNodeInserted->inDocument()) {

        // Find the last leaf.
        NodeImpl *lastLeaf = m_lastNodeInserted;
        while (1) {
            NodeImpl *nextChild = lastLeaf->lastChild();
            if (!nextChild)
                break;
            lastLeaf = nextChild;
        }
    
        // Find the first leaf.
        NodeImpl *firstLeaf = m_firstNodeInserted;
        while (1) {
            NodeImpl *nextChild = firstLeaf->firstChild();
            if (!nextChild)
                break;
            firstLeaf = nextChild;
        }
        
        // Call updateLayout so caretMinOffset and caretMaxOffset return correct values.
        document()->updateLayout();
        start = Position(firstLeaf, firstLeaf->caretMinOffset());
        end = Position(lastLeaf, lastLeaf->caretMaxOffset());

        if (m_matchStyle) {
            assert(m_insertionStyle);
            setEndingSelection(Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY));
            applyStyle(m_insertionStyle);
        }    
        
        if (lastPositionToSelect.isNotNull())
            end = lastPositionToSelect;
    }
    else if (lastPositionToSelect.isNotNull()) {
        start = end = lastPositionToSelect;
    }
    else {
        return;
    }
    
    if (m_selectReplacement)
        setEndingSelection(Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY));
    else
        setEndingSelection(end, SEL_DEFAULT_AFFINITY);
    
    rebalanceWhitespace();
}

EditAction ReplaceSelectionCommand::editingAction() const
{
    return EditActionPaste;
}

void ReplaceSelectionCommand::insertNodeAfterAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild)
{
    insertNodeAfter(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeAtAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild, long offset)
{
    insertNodeAt(insertChild, refChild, offset);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeBeforeAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild)
{
    insertNodeBefore(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::updateNodesInserted(NodeImpl *node)
{
    if (!node)
        return;

    // update m_lastTopNodeInserted
    node->ref();
    if (m_lastTopNodeInserted)
        m_lastTopNodeInserted->deref();
    m_lastTopNodeInserted = node;
    
    // update m_firstNodeInserted
    if (!m_firstNodeInserted) {
        m_firstNodeInserted = node;
        m_firstNodeInserted->ref();
    }
    
    if (node == m_lastNodeInserted)
        return;
    
    // update m_lastNodeInserted
    NodeImpl *old = m_lastNodeInserted;
    m_lastNodeInserted = node->lastDescendent();
    m_lastNodeInserted->ref();
    if (old)
        old->deref();
}

void ReplaceSelectionCommand::fixupNodeStyles(const QValueList<NodeDesiredStyle> &list)
{
    // This function uses the mapped "desired style" to apply the additional style needed, if any,
    // to make the node have the desired style.

    document()->updateLayout();

    QValueListConstIterator<NodeDesiredStyle> it;
    for (it = list.begin(); it != list.end(); ++it) {
        NodeImpl *node = (*it).node();
        CSSMutableStyleDeclarationImpl *desiredStyle = (*it).style();
        ASSERT(desiredStyle);

        if (!node->inDocument())
            continue;

        // The desiredStyle declaration tells what style this node wants to be.
        // Compare that to the style that it is right now in the document.
        Position pos(node, 0);
        CSSComputedStyleDeclarationImpl *currentStyle = pos.computedStyle();
        currentStyle->ref();

        // Check for the special "match nearest blockquote color" property and resolve to the correct
        // color if necessary.
        DOMString matchColorCheck = desiredStyle->getPropertyValue(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);
        if (matchColorCheck == matchNearestBlockquoteColorString()) {
            NodeImpl *blockquote = nearestMailBlockquote(node);
            Position pos(blockquote ? blockquote : node->getDocument()->documentElement(), 0);
            CSSComputedStyleDeclarationImpl *style = pos.computedStyle();
            DOMString desiredColor = desiredStyle->getPropertyValue(CSS_PROP_COLOR);
            DOMString nearestColor = style->getPropertyValue(CSS_PROP_COLOR);
            if (desiredColor != nearestColor)
                desiredStyle->setProperty(CSS_PROP_COLOR, nearestColor);
        }
        desiredStyle->removeProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);

        currentStyle->diff(desiredStyle);
        
        // Only add in block properties if the node is at the start of a 
        // paragraph. This matches AppKit.
        if (!isStartOfParagraph(VisiblePosition(pos, DOWNSTREAM)))
            desiredStyle->removeBlockProperties();
        
        // If the desiredStyle is non-zero length, that means the current style differs
        // from the desired by the styles remaining in the desiredStyle declaration.
        if (desiredStyle->length() > 0) {
            DOM::RangeImpl *rangeAroundNode = document()->createRange();
            rangeAroundNode->ref();
            int exceptionCode = 0;
            rangeAroundNode->selectNode(node, exceptionCode);
            ASSERT(exceptionCode == 0);
            // affinity is not really important since this is a temp selection
            // just for calling applyStyle
            setEndingSelection(Selection(rangeAroundNode, SEL_DEFAULT_AFFINITY, SEL_DEFAULT_AFFINITY));
            applyStyle(desiredStyle);
            rangeAroundNode->deref();
        }

        currentStyle->deref();
    }
}

void computeAndStoreNodeDesiredStyle(DOM::NodeImpl *node, QValueList<NodeDesiredStyle> &list)
{
    if (!node || !node->inDocument())
        return;
        
    CSSComputedStyleDeclarationImpl *computedStyle = Position(node, 0).computedStyle();
    computedStyle->ref();
    CSSMutableStyleDeclarationImpl *style = computedStyle->copyInheritableProperties();
    list.append(NodeDesiredStyle(node, style));
    computedStyle->deref();

    // In either of the color-matching tests below, set the color to a pseudo-color that will
    // make the content take on the color of the nearest-enclosing blockquote (if any) after
    // being pasted in.
    if (NodeImpl *blockquote = nearestMailBlockquote(node)) {
        CSSComputedStyleDeclarationImpl *blockquoteStyle = Position(blockquote, 0).computedStyle();
        if (blockquoteStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR)) {
            style->setProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
            return;
        }
    }
    NodeImpl *documentElement = node->getDocument() ? node->getDocument()->documentElement() : 0;
    if (documentElement) {
        CSSComputedStyleDeclarationImpl *documentStyle = Position(documentElement, 0).computedStyle();
        if (documentStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR)) {
            style->setProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
        }
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

    int exceptionCode = 0;
    if (m_oldValue.isNull())
        m_element->removeAttribute(m_attribute, exceptionCode);
    else
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
// SplitElementCommand

SplitElementCommand::SplitElementCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element, DOM::NodeImpl *atChild)
    : EditCommand(document), m_element1(0), m_element2(element), m_atChild(atChild)
{
    ASSERT(m_element2);
    ASSERT(m_atChild);

    m_element2->ref();
    m_atChild->ref();
}

SplitElementCommand::~SplitElementCommand()
{
    if (m_element1)
        m_element1->deref();

    ASSERT(m_element2);
    m_element2->deref();
    ASSERT(m_atChild);
    m_atChild->deref();
}

void SplitElementCommand::doApply()
{
    ASSERT(m_element2);
    ASSERT(m_atChild);

    int exceptionCode = 0;

    if (!m_element1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_element1 = static_cast<ElementImpl *>(m_element2->cloneNode(false));
        ASSERT(m_element1);
        m_element1->ref();
    }

    m_element2->parent()->insertBefore(m_element1, m_element2, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    while (m_element2->firstChild() != m_atChild) {
        ASSERT(m_element2->firstChild());
        m_element1->appendChild(m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }
}

void SplitElementCommand::doUnapply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);
    ASSERT(m_atChild);

    ASSERT(m_element1->nextSibling() == m_element2);
    ASSERT(m_element2->firstChild() && m_element2->firstChild() == m_atChild);

    int exceptionCode = 0;

    while (m_element1->lastChild()) {
        m_element2->insertBefore(m_element1->lastChild(), m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element2->parentNode()->removeChild(m_element1, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// MergeIdenticalElementsCommand

MergeIdenticalElementsCommand::MergeIdenticalElementsCommand(DOM::DocumentImpl *document, DOM::ElementImpl *first, DOM::ElementImpl *second)
    : EditCommand(document), m_element1(first), m_element2(second), m_atChild(0)
{
    ASSERT(m_element1);
    ASSERT(m_element2);

    m_element1->ref();
    m_element2->ref();
}

MergeIdenticalElementsCommand::~MergeIdenticalElementsCommand()
{
    if (m_atChild)
        m_atChild->deref();

    ASSERT(m_element1);
    m_element1->deref();
    ASSERT(m_element2);
    m_element2->deref();
}

void MergeIdenticalElementsCommand::doApply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);
    ASSERT(m_element1->nextSibling() == m_element2);

    int exceptionCode = 0;

    if (!m_atChild) {
        m_atChild = m_element2->firstChild();
        m_atChild->ref();
    }

    while (m_element1->lastChild()) {
        m_element2->insertBefore(m_element1->lastChild(), m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element2->parentNode()->removeChild(m_element1, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void MergeIdenticalElementsCommand::doUnapply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);

    int exceptionCode = 0;

    m_element2->parent()->insertBefore(m_element1, m_element2, exceptionCode);
    ASSERT(exceptionCode == 0);

    while (m_element2->firstChild() != m_atChild) {
        ASSERT(m_element2->firstChild());
        m_element1->appendChild(m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }
}

//------------------------------------------------------------------------------------------
// WrapContentsInDummySpanCommand

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element)
    : EditCommand(document), m_element(element), m_dummySpan(0)
{
    ASSERT(m_element);

    m_element->ref();
}

WrapContentsInDummySpanCommand::~WrapContentsInDummySpanCommand()
{
    if (m_dummySpan)
        m_dummySpan->deref();

    ASSERT(m_element);
    m_element->deref();
}

void WrapContentsInDummySpanCommand::doApply()
{
    ASSERT(m_element);

    int exceptionCode = 0;

    if (!m_dummySpan) {
        m_dummySpan = createStyleSpanElement(document());
        m_dummySpan->ref();
    }

    while (m_element->firstChild()) {
        m_dummySpan->appendChild(m_element->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element->appendChild(m_dummySpan, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void WrapContentsInDummySpanCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(m_dummySpan);

    ASSERT(m_element->firstChild() == m_dummySpan);
    ASSERT(!m_element->firstChild()->nextSibling());

    int exceptionCode = 0;

    while (m_dummySpan->firstChild()) {
        m_element->appendChild(m_dummySpan->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element->removeChild(m_dummySpan, exceptionCode);
    ASSERT(exceptionCode == 0);
}

//------------------------------------------------------------------------------------------
// SplitTextNodeContainingElementCommand

SplitTextNodeContainingElementCommand::SplitTextNodeContainingElementCommand(DocumentImpl *document, TextImpl *text, long offset)
    : CompositeEditCommand(document), m_text(text), m_offset(offset)
{
    ASSERT(m_text);
    ASSERT(m_text->length() > 0);

    m_text->ref();
}

SplitTextNodeContainingElementCommand::~SplitTextNodeContainingElementCommand()
{
    ASSERT(m_text);
    m_text->deref();
}

void SplitTextNodeContainingElementCommand::doApply()
{
    ASSERT(m_text);
    ASSERT(m_offset > 0);

    splitTextNode(m_text, m_offset);
    
    NodeImpl *parentNode = m_text->parentNode();
    if (!parentNode->renderer() || !parentNode->renderer()->isInline()) {
        wrapContentsInDummySpan(static_cast<ElementImpl *>(parentNode));
        parentNode = parentNode->firstChild();
    }

    splitElement(static_cast<ElementImpl *>(parentNode), m_text);
}

//------------------------------------------------------------------------------------------
// TypingCommand

TypingCommand::TypingCommand(DocumentImpl *document, ETypingCommand commandType, const DOMString &textToInsert, bool selectInsertedText)
    : CompositeEditCommand(document), 
      m_commandType(commandType), 
      m_textToInsert(textToInsert), 
      m_openForMoreTyping(true), 
      m_applyEditing(false), 
      m_selectInsertedText(selectInsertedText),
      m_smartDelete(false)
{
}

void TypingCommand::deleteKeyPressed(DocumentImpl *document, bool smartDelete)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->deleteKeyPressed();
    }
    else {
        Selection selection = part->selection();
        if (selection.isCaret() && VisiblePosition(selection.start(), selection.startAffinity()).previous().isNull()) {
            // do nothing for a delete key at the start of an editable element.
        }
        else {
            TypingCommand *typingCommand = new TypingCommand(document, DeleteKey);
            typingCommand->setSmartDelete(smartDelete);
            EditCommandPtr cmd(typingCommand);
            cmd.apply();
        }
    }
}

void TypingCommand::forwardDeleteKeyPressed(DocumentImpl *document, bool smartDelete)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommandPtr lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->forwardDeleteKeyPressed();
    }
    else {
        Selection selection = part->selection();
        if (selection.isCaret() && isEndOfDocument(VisiblePosition(selection.start(), selection.startAffinity()))) {
            // do nothing for a delete key at the start of an editable element.
        }
        else {
            TypingCommand *typingCommand = new TypingCommand(document, ForwardDeleteKey);
            typingCommand->setSmartDelete(smartDelete);
            EditCommandPtr cmd(typingCommand);
            cmd.apply();
        }
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
        case ForwardDeleteKey:
            forwardDeleteKeyPressed();
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

EditAction TypingCommand::editingAction() const
{
    return EditActionTyping;
}

void TypingCommand::markMisspellingsAfterTyping()
{
    // Take a look at the selection that results after typing and determine whether we need to spellcheck. 
    // Since the word containing the current selection is never marked, this does a check to
    // see if typing made a new word that is not in the current selection. Basically, you
    // get this by being at the end of a word and typing a space.    
    VisiblePosition start(endingSelection().start(), endingSelection().startAffinity());
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
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    int offset = 0;
    int newline;
    while ((newline = text.find('\n', offset)) != -1) {
        if (newline != offset) {
            insertTextRunWithoutNewlines(text.substring(offset, newline - offset), false);
        }
        insertParagraphSeparator();
        offset = newline + 1;
    }
    if (offset == 0) {
        insertTextRunWithoutNewlines(text, selectInsertedText);
    } else {
        int length = text.length();
        if (length != offset) {
            insertTextRunWithoutNewlines(text.substring(offset, length - offset), selectInsertedText);
        }
    }
}

void TypingCommand::insertTextRunWithoutNewlines(const DOMString &text, bool selectInsertedText)
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

void TypingCommand::deleteKeyPressed()
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
            Position start = VisiblePosition(pos, endingSelection().startAffinity()).previous().deepEquivalent();
            Position end = VisiblePosition(pos, endingSelection().startAffinity()).deepEquivalent();
            if (start.isNotNull() && end.isNotNull() && start.node()->rootEditableElement() == end.node()->rootEditableElement())
                selectionToDelete = Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange()) {
        deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
}

void TypingCommand::forwardDeleteKeyPressed()
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
            Position start = VisiblePosition(pos, endingSelection().startAffinity()).next().deepEquivalent();
            Position end = VisiblePosition(pos, endingSelection().startAffinity()).deepEquivalent();
            if (start.isNotNull() && end.isNotNull() && start.node()->rootEditableElement() == end.node()->rootEditableElement())
                selectionToDelete = Selection(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange()) {
        deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
}

bool TypingCommand::preservesTypingStyle() const
{
    switch (m_commandType) {
        case DeleteKey:
        case ForwardDeleteKey:
        case InsertParagraphSeparator:
        case InsertLineBreak:
            return true;
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

ElementImpl *floatRefdElement(ElementImpl *element)
{
    assert(!element->parentNode());
    element->setParent(element->getDocument());
    element->deref();
    element->setParent(0);
    return element;
}    

ElementImpl *createDefaultParagraphElement(DocumentImpl *document)
{
    // We would need this margin-zeroing code back if we ever return to using <p> elements for default paragraphs.
    // static const DOMString defaultParagraphStyle("margin-top: 0; margin-bottom: 0");    
    int exceptionCode = 0;
    ElementImpl *element = document->createHTMLElement("div", exceptionCode);
    ASSERT(exceptionCode == 0);
    return element;
}

ElementImpl *createBlockPlaceholderElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *breakNode = document->createHTMLElement("br", exceptionCode);
    ASSERT(exceptionCode == 0);
    breakNode->ref();
    breakNode->setAttribute(ATTR_CLASS, blockPlaceholderClassString());
    return floatRefdElement(breakNode);
}

ElementImpl *createBreakElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *breakNode = document->createHTMLElement("br", exceptionCode);
    ASSERT(exceptionCode == 0);
    return breakNode;
}

ElementImpl *createFontElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *fontNode = document->createHTMLElement("font", exceptionCode);
    ASSERT(exceptionCode == 0);
    fontNode->ref();
    fontNode->setAttribute(ATTR_CLASS, styleSpanClassString());
    return floatRefdElement(fontNode);
}

ElementImpl *createStyleSpanElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *styleElement = document->createHTMLElement("SPAN", exceptionCode);
    ASSERT(exceptionCode == 0);
    styleElement->ref();
    styleElement->setAttribute(ATTR_CLASS, styleSpanClassString());
    return floatRefdElement(styleElement);
}

bool isNodeRendered(const NodeImpl *node)
{
    if (!node)
        return false;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return false;

    return renderer->style()->visibility() == VISIBLE;
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

bool isProbablyTableStructureNode(const NodeImpl *node)
{
    if (!node)
        return false;
    
    switch (node->id()) {
        case ID_TABLE:
        case ID_TBODY:
        case ID_TD:
        case ID_TFOOT:
        case ID_THEAD:
        case ID_TR:
            return true;
    }
    return false;
}

NodeImpl *nearestMailBlockquote(const NodeImpl *node)
{
    for (NodeImpl *n = const_cast<NodeImpl *>(node); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            return n;
    }
    return 0;
}

bool isMailBlockquote(const NodeImpl *node)
{
    if (!node || !node->renderer() || !node->isElementNode() && node->id() != ID_BLOCKQUOTE)
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("type") == "cite";
}

bool isMailPasteAsQuotationNode(const NodeImpl *node)
{
    if (!node)
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("class") == ApplePasteAsQuotation;
}

} // namespace khtml
