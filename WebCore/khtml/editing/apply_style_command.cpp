/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "apply_style_command.h"

#include "html_interchange.h"

#include "css/css_valueimpl.h"
#include "css/css_computedstyle.h"
#include "css/cssparser.h"
#include "css/cssproperties.h"
#include "dom/dom_string.h"
#include "htmlediting.h"
#include "html/html_elementimpl.h"
#include "htmlnames.h"
#include "rendering/render_object.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_rangeimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#else
#define ASSERT(assertion) assert(assertion)
#endif

using namespace DOM::HTMLNames;

using DOM::CSSComputedStyleDeclarationImpl;
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::CSSParser;
using DOM::CSSPrimitiveValue;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSProperty;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSValue;
using DOM::CSSValueImpl;
using DOM::DOMString;
using DOM::DoNotUpdateLayout;
using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::HTMLElementImpl;
using DOM::NamedAttrMapImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::RangeImpl;
using DOM::TextImpl;

namespace khtml {

class StyleChange {
public:
    enum ELegacyHTMLStyles { DoNotUseLegacyHTMLStyles, UseLegacyHTMLStyles };

    explicit StyleChange(CSSStyleDeclarationImpl *, ELegacyHTMLStyles usesLegacyStyles=UseLegacyHTMLStyles);
    StyleChange(CSSStyleDeclarationImpl *, const Position &, ELegacyHTMLStyles usesLegacyStyles=UseLegacyHTMLStyles);

    static ELegacyHTMLStyles styleModeForParseMode(bool);

    DOMString cssStyle() const { return m_cssStyle; }
    bool applyBold() const { return m_applyBold; }
    bool applyItalic() const { return m_applyItalic; }
    bool applyFontColor() const { return m_applyFontColor.length() > 0; }
    bool applyFontFace() const { return m_applyFontFace.length() > 0; }
    bool applyFontSize() const { return m_applyFontSize.length() > 0; }

    DOMString fontColor() { return m_applyFontColor; }
    DOMString fontFace() { return m_applyFontFace; }
    DOMString fontSize() { return m_applyFontSize; }

    bool usesLegacyStyles() const { return m_usesLegacyStyles; }

private:
    void init(CSSStyleDeclarationImpl *, const Position &);
    bool checkForLegacyHTMLStyleChange(const CSSProperty *);
    static bool currentlyHasStyle(const Position &, const CSSProperty *);
    
    DOMString m_cssStyle;
    bool m_applyBold;
    bool m_applyItalic;
    DOMString m_applyFontColor;
    DOMString m_applyFontFace;
    DOMString m_applyFontSize;
    bool m_usesLegacyStyles;
};



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
    SharedPtr<CSSMutableStyleDeclarationImpl> mutableStyle = style->makeMutable();
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
            styleText += alteredProperty.cssText().qstring();
        } else {
            styleText += property->cssText().qstring();
        }
    }

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
    SharedPtr<CSSComputedStyleDeclarationImpl> style = pos.computedStyle();
    SharedPtr<CSSValueImpl> value = style->getPropertyCSSValue(property->id(), DoNotUpdateLayout);
    if (!value)
        return false;
    return strcasecmp(value->cssText(), property->value()->cssText()) == 0;
}

static DOMString &styleSpanClassString()
{
    static DOMString styleSpanClassString = AppleStyleSpanClass;
    return styleSpanClassString;
}

bool isStyleSpan(const NodeImpl *node)
{
    if (!node || !node->isHTMLElement())
        return false;

    const HTMLElementImpl *elem = static_cast<const HTMLElementImpl *>(node);
    return elem->hasLocalName(spanAttr) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static bool isEmptyStyleSpan(const NodeImpl *node)
{
    if (!node || !node->isHTMLElement() || !node->hasTagName(spanTag))
        return false;

    const HTMLElementImpl *elem = static_cast<const HTMLElementImpl *>(node);
    CSSMutableStyleDeclarationImpl *inlineStyleDecl = elem->inlineStyleDecl();
    return (!inlineStyleDecl || inlineStyleDecl->length() == 0) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static bool isEmptyFontTag(const NodeImpl *node)
{
    if (!node || !node->hasTagName(fontTag))
        return false;

    const ElementImpl *elem = static_cast<const ElementImpl *>(node);
    NamedAttrMapImpl *map = elem->attributes(true); // true for read-only
    return (!map || map->length() == 1) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static ElementImpl *createFontElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *fontNode = document->createElementNS(xhtmlNamespaceURI, "font", exceptionCode);
    ASSERT(exceptionCode == 0);
    fontNode->setAttribute(classAttr, styleSpanClassString());
    return fontNode;
}

ElementImpl *createStyleSpanElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *styleElement = document->createElementNS(xhtmlNamespaceURI, "span", exceptionCode);
    ASSERT(exceptionCode == 0);
    styleElement->setAttribute(classAttr, styleSpanClassString());
    return styleElement;
}

ApplyStyleCommand::ApplyStyleCommand(DocumentImpl *document, CSSStyleDeclarationImpl *style, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document), m_style(style->makeMutable()), m_editingAction(editingAction), m_propertyLevel(propertyLevel)
{   
    ASSERT(style);
}

ApplyStyleCommand::~ApplyStyleCommand()
{
}

void ApplyStyleCommand::doApply()
{
    switch (m_propertyLevel) {
        case PropertyDefault: {
            // apply the block-centric properties of the style
            SharedPtr<CSSMutableStyleDeclarationImpl> blockStyle = m_style->copyBlockProperties();
            applyBlockStyle(blockStyle.get());
            // apply any remaining styles to the inline elements
            // NOTE: hopefully, this string comparison is the same as checking for a non-null diff
            if (blockStyle->length() < m_style->length()) {
                SharedPtr<CSSMutableStyleDeclarationImpl> inlineStyle = m_style->copy();
                applyRelativeFontStyleChange(inlineStyle.get());
                blockStyle->diff(inlineStyle.get());
                applyInlineStyle(inlineStyle.get());
            }
            break;
        }
        case ForceBlockProperties:
            // Force all properties to be applied as block styles.
            applyBlockStyle(m_style.get());
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
    SharedPtr<CSSValueImpl> value = style->getPropertyCSSValue(CSS_PROP_FONT_SIZE);
    if (value) {
        // Explicit font size overrides any delta.
        style->removeProperty(CSS_PROP__KHTML_FONT_SIZE_DELTA);
        return;
    }

    // Get the adjustment amount out of the style.
    value = style->getPropertyCSSValue(CSS_PROP__KHTML_FONT_SIZE_DELTA);
    if (!value)
        return;
    float adjustment = NoFontDelta;
    if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
        CSSPrimitiveValueImpl *primitiveValue = static_cast<CSSPrimitiveValueImpl *>(value.get());
        if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PX) {
            // Only PX handled now. If we handle more types in the future, perhaps
            // a switch statement here would be more appropriate.
            adjustment = primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PX);
        }
    }
    style->removeProperty(CSS_PROP__KHTML_FONT_SIZE_DELTA);
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
        SharedPtr<CSSValueImpl> value = inlineStyleDecl->getPropertyCSSValue(CSS_PROP_FONT_SIZE);
        if (value) {
            inlineStyleDecl->removeProperty(CSS_PROP_FONT_SIZE, true);
            currentFontSize = computedFontSize(node);
        }
        if (currentFontSize != desiredFontSize) {
            QString desiredFontSizeString = QString::number(desiredFontSize);
            desiredFontSizeString += "px";
            inlineStyleDecl->setProperty(CSS_PROP_FONT_SIZE, desiredFontSizeString, false, false);
            setNodeAttribute(elem, styleAttr, inlineStyleDecl->cssText());
        }
        if (inlineStyleDecl->length() == 0) {
            removeNodeAttribute(elem, styleAttr);
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
                        (next->isElementNode() && !next->hasTagName(brTag)) || 
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

bool ApplyStyleCommand::isHTMLStyleNode(CSSMutableStyleDeclarationImpl *style, HTMLElementImpl *elem)
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = style->valuesIterator(); it != end; ++it) {
        switch ((*it).id()) {
            case CSS_PROP_FONT_WEIGHT:
                if (elem->hasLocalName(bTag))
                    return true;
                break;
            case CSS_PROP_FONT_STYLE:
                if (elem->hasLocalName(iTag))
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

    if (!elem->hasLocalName(fontTag))
        return;

    int exceptionCode = 0;
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = style->valuesIterator(); it != end; ++it) {
        switch ((*it).id()) {
            case CSS_PROP_COLOR:
                elem->removeAttribute(colorAttr, exceptionCode);
                ASSERT(exceptionCode == 0);
                break;
            case CSS_PROP_FONT_FAMILY:
                elem->removeAttribute(faceAttr, exceptionCode);
                ASSERT(exceptionCode == 0);
                break;
            case CSS_PROP_FONT_SIZE:
                elem->removeAttribute(sizeAttr, exceptionCode);
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
        SharedPtr<CSSValueImpl> value = decl->getPropertyCSSValue(propertyID);
        if (value && (propertyID != CSS_PROP_WHITE_SPACE || !isTabSpanNode(elem)))
            removeCSSProperty(decl, propertyID);
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
    SharedPtr<CSSValueImpl> value = style.getPropertyCSSValue(CSS_PROP_TEXT_DECORATION, DoNotUpdateLayout);
    return value && strcasecmp(value->cssText(), "none") != 0;
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
    SharedPtr<CSSMutableStyleDeclarationImpl> style = element->inlineStyleDecl();
    if (!style)
        return 0;

    int properties[1] = { CSS_PROP_TEXT_DECORATION };
    CSSMutableStyleDeclarationImpl *textDecorationStyle = style->copyPropertiesInSet(properties, 1);

    SharedPtr<CSSValueImpl> property = style->getPropertyCSSValue(CSS_PROP_TEXT_DECORATION);
    if (property && strcasecmp(property->cssText(), "none") != 0)
        removeCSSProperty(style.get(), CSS_PROP_TEXT_DECORATION);

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
    SharedPtr<CSSComputedStyleDeclarationImpl> computedStyle = new CSSComputedStyleDeclarationImpl(element);
    ASSERT(computedStyle);

    int properties[1] = { CSS_PROP_TEXT_DECORATION };
    CSSMutableStyleDeclarationImpl *textDecorationStyle = computedStyle->copyPropertiesInSet(properties, 1);

    SharedPtr<CSSValueImpl> property = computedStyle->getPropertyCSSValue(CSS_PROP_TEXT_DECORATION);
    if (property && strcasecmp(property->cssText(), "none") != 0) {
        SharedPtr<CSSMutableStyleDeclarationImpl> newStyle = textDecorationStyle->copy();
        newStyle->setProperty(CSS_PROP_TEXT_DECORATION, "none");
        applyTextDecorationStyle(node, newStyle.get());
    }

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
        setNodeAttribute(element, styleAttr, cssText);
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
            
            SharedPtr<CSSMutableStyleDeclarationImpl> decoration = force ? extractAndNegateTextDecorationStyle(current) : extractTextDecorationStyle(current);

            for (NodeImpl *child = current->firstChild(); child; child = nextChild) {
                nextChild = child->nextSibling();

                if (node == child) {
                    nextCurrent = child;
                } else if (node->isAncestor(child)) {
                    applyTextDecorationStyle(child, decoration.get());
                    nextCurrent = child;
                } else {
                    applyTextDecorationStyle(child, decoration.get());
                }
            }
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

static int maxRangeOffset(NodeImpl *n)
{
    if (DOM::offsetInCharacters(n->nodeType()))
        return n->maxOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

void ApplyStyleCommand::removeInlineStyle(CSSMutableStyleDeclarationImpl *style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(RangeImpl::compareBoundaryPoints(start, end) < 0);
    
    SharedPtr<CSSValueImpl> textDecorationSpecialProperty = style->getPropertyCSSValue(CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT);

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


bool ApplyStyleCommand::splitTextAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > start.node()->caretMinOffset() && start.offset() < start.node()->caretMaxOffset()) {
        int endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
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
        int endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
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
    
    if (!firstElement->tagName().matches(secondElement->tagName()))
        return false;

    NamedAttrMapImpl *firstMap = firstElement->attributes();
    NamedAttrMapImpl *secondMap = secondElement->attributes();

    unsigned firstLength = firstMap->length();

    if (firstLength != secondMap->length())
        return false;

    for (unsigned i = 0; i < firstLength; i++) {
        DOM::AttributeImpl *attribute = firstMap->attributeItem(i);
        DOM::AttributeImpl *secondAttribute = secondMap->getAttributeItem(attribute->name());

        if (!secondAttribute || attribute->value() != secondAttribute->value())
            return false;
    }
    
    return true;
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position &start, const Position &end)
{
    NodeImpl *startNode = start.node();
    int startOffset = start.offset();

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

        int startOffsetAdjustment = startChild->nodeIndex();
        int endOffsetAdjustment = startNode == end.node() ? startOffsetAdjustment : 0;

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

    if (!endNode->isElementNode() || endNode->hasTagName(brTag))
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
        setNodeAttribute(block, styleAttr, cssText);
    }
}

void ApplyStyleCommand::addInlineStyleIfNeeded(CSSMutableStyleDeclarationImpl *style, NodeImpl *startNode, NodeImpl *endNode)
{
    StyleChange styleChange(style, Position(startNode, 0), StyleChange::styleModeForParseMode(document()->inCompatMode()));
    int exceptionCode = 0;
    
    // Prevent style changes to our tab spans, because it might remove the whitespace:pre we are after
    if (isTabSpanTextNode(startNode))
        return;
    
    //
    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    //
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        ElementImpl *fontElement = createFontElement(document());
        ASSERT(exceptionCode == 0);
        insertNodeBefore(fontElement, startNode);
        if (styleChange.applyFontColor())
            fontElement->setAttribute(colorAttr, styleChange.fontColor());
        if (styleChange.applyFontFace())
            fontElement->setAttribute(faceAttr, styleChange.fontFace());
        if (styleChange.applyFontSize())
            fontElement->setAttribute(sizeAttr, styleChange.fontSize());
        surroundNodeRangeWithElement(startNode, endNode, fontElement);
    }

    if (styleChange.cssStyle().length() > 0) {
        SharedPtr<ElementImpl> styleElement = createStyleSpanElement(document());
        styleElement->setAttribute(styleAttr, styleChange.cssStyle());
        insertNodeBefore(styleElement.get(), startNode);
        surroundNodeRangeWithElement(startNode, endNode, styleElement.get());
    }

    if (styleChange.applyBold()) {
        ElementImpl *boldElement = document()->createElementNS(xhtmlNamespaceURI, "b", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeBefore(boldElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, boldElement);
    }

    if (styleChange.applyItalic()) {
        ElementImpl *italicElement = document()->createElementNS(xhtmlNamespaceURI, "i", exceptionCode);
        ASSERT(exceptionCode == 0);
        insertNodeBefore(italicElement, startNode);
        surroundNodeRangeWithElement(startNode, endNode, italicElement);
    }
}

float ApplyStyleCommand::computedFontSize(const NodeImpl *node)
{
    if (!node)
        return 0;
    
    Position pos(const_cast<NodeImpl *>(node), 0);
    SharedPtr<CSSComputedStyleDeclarationImpl> computedStyle = pos.computedStyle();
    if (!computedStyle)
        return 0;

    SharedPtr<CSSPrimitiveValueImpl> value = static_cast<CSSPrimitiveValueImpl *>(computedStyle->getPropertyCSSValue(CSS_PROP_FONT_SIZE));
    if (!value)
        return 0;

    return value->getFloatValue(CSSPrimitiveValue::CSS_PX);
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

} // namespace khtml
