/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "markup.h"

#include "CSSComputedStyleDeclaration.h"
#include "Comment.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "KURL.h"
#include "Logging.h"
#include "ProcessingInstruction.h"
#include "Range.h"
#include "htmlediting.h"
#include "visible_units.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static inline bool shouldSelfClose(const Node *node);

static DeprecatedString escapeTextForMarkup(const DeprecatedString &in)
{
    DeprecatedString s = "";

    unsigned len = in.length();
    for (unsigned i = 0; i < len; ++i) {
        switch (in[i].unicode()) {
            case '&':
                s += "&amp;";
                break;
            case '<':
                s += "&lt;";
                break;
            case '>':
                s += "&gt;";
                break;
            default:
                s += in[i];
        }
    }

    return s;
}

static String stringValueForRange(const Node *node, const Range *range)
{
    String str = node->nodeValue().copy();
    if (range) {
        ExceptionCode ec;
        if (node == range->endContainer(ec))
            str.truncate(range->endOffset(ec));
        if (node == range->startContainer(ec))
            str.remove(0, range->startOffset(ec));
    }
    return str;
}

static DeprecatedString renderedText(const Node *node, const Range *range)
{
    RenderObject *r = node->renderer();
    if (!r)
        return DeprecatedString();
    
    if (!node->isTextNode())
        return DeprecatedString();

    DeprecatedString result = "";

    ExceptionCode ec;
    const Text *textNode = static_cast<const Text*>(node);
    unsigned startOffset = 0;
    unsigned endOffset = textNode->length();

    if (range && node == range->startContainer(ec))
        startOffset = range->startOffset(ec);
    if (range && node == range->endContainer(ec))
        endOffset = range->endOffset(ec);
    
    RenderText *textRenderer = static_cast<RenderText*>(r);
    DeprecatedString str = node->nodeValue().deprecatedString();
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        unsigned start = box->m_start;
        unsigned end = box->m_start + box->m_len;
        if (endOffset < start)
            break;
        if (startOffset <= end) {
            unsigned s = max(start, startOffset);
            unsigned e = min(end, endOffset);
            result.append(str.mid(s, e-s));
            if (e == end) {
                // now add in collapsed-away spaces if at the end of the line
                InlineTextBox *nextBox = box->nextTextBox();
                if (nextBox && box->root() != nextBox->root()) {
                    const char nonBreakingSpace = '\xa0';
                    // count the number of characters between the end of the
                    // current box and the start of the next box.
                    int collapsedStart = e;
                    int collapsedPastEnd = min((unsigned)nextBox->m_start, endOffset + 1);
                    bool addNextNonNBSP = true;
                    for (int i = collapsedStart; i < collapsedPastEnd; i++) {
                        if (str[i] == nonBreakingSpace) {
                            result.append(str[i]);
                            addNextNonNBSP = true;
                        }
                        else if (addNextNonNBSP) {
                            result.append(str[i]);
                            addNextNonNBSP = false;
                        }
                    }
                }
            }
        }

    }
    
    return result;
}

static DeprecatedString startMarkup(const Node *node, const Range *range, EAnnotateForInterchange annotate, CSSMutableStyleDeclaration *defaultStyle)
{
    bool documentIsHTML = node->document()->isHTMLDocument();
    switch (node->nodeType()) {
        case Node::TEXT_NODE: {
            if (Node* parent = node->parentNode()) {
                if (parent->hasTagName(listingTag)
                        || parent->hasTagName(preTag)
                        || parent->hasTagName(scriptTag)
                        || parent->hasTagName(styleTag)
                        || parent->hasTagName(textareaTag))
                    return stringValueForRange(node, range).deprecatedString();
            }
            DeprecatedString markup = annotate ? escapeTextForMarkup(renderedText(node, range)) : escapeTextForMarkup(stringValueForRange(node, range).deprecatedString());            
            if (defaultStyle) {
                Node *element = node->parentNode();
                if (element) {
                    RefPtr<CSSComputedStyleDeclaration> computedStyle = Position(element, 0).computedStyle();
                    RefPtr<CSSMutableStyleDeclaration> style = computedStyle->copyInheritableProperties();
                    defaultStyle->diff(style.get());
                    if (style->length() > 0) {
                        // FIXME: Handle case where style->cssText() has illegal characters in it, like "
                        DeprecatedString openTag = DeprecatedString("<span class=\"") + AppleStyleSpanClass + "\" style=\"" + style->cssText().deprecatedString() + "\">";
                        markup = openTag + markup + "</span>";
                    }
                }            
            }
            return annotate ? convertHTMLTextToInterchangeFormat(markup) : markup;
        }
        case Node::COMMENT_NODE:
            return static_cast<const Comment*>(node)->toString().deprecatedString();
        case Node::DOCUMENT_NODE: {
            // Documents do not normally contain a docType as a child node, force it to print here instead.
            const DocumentType* docType = static_cast<const Document*>(node)->doctype();
            if (docType)
                return docType->toString().deprecatedString();
            return "";
        }
        case Node::DOCUMENT_FRAGMENT_NODE:
            return "";
        case Node::DOCUMENT_TYPE_NODE:
            return static_cast<const DocumentType*>(node)->toString().deprecatedString();
        case Node::PROCESSING_INSTRUCTION_NODE:
            return static_cast<const ProcessingInstruction*>(node)->toString().deprecatedString();
        case Node::ELEMENT_NODE: {
            DeprecatedString markup = QChar('<');
            const Element* el = static_cast<const Element*>(node);
            markup += el->nodeNamePreservingCase().deprecatedString();
            String additionalStyle;
            if (defaultStyle && el->isHTMLElement()) {
                RefPtr<CSSComputedStyleDeclaration> computedStyle = Position(const_cast<Element*>(el), 0).computedStyle();
                RefPtr<CSSMutableStyleDeclaration> style = computedStyle->copyInheritableProperties();
                defaultStyle->diff(style.get());
                if (style->length() > 0) {
                    CSSMutableStyleDeclaration *inlineStyleDecl = static_cast<const HTMLElement*>(el)->inlineStyleDecl();
                    if (inlineStyleDecl)
                        inlineStyleDecl->diff(style.get());
                    additionalStyle = style->cssText();
                }
            }
            NamedAttrMap *attrs = el->attributes();
            unsigned length = attrs->length();
            if (length == 0 && additionalStyle.length() > 0) {
                // FIXME: Handle case where additionalStyle has illegal characters in it, like "
                markup += " " +  styleAttr.localName().deprecatedString() + "=\"" + additionalStyle.deprecatedString() + "\"";
            }
            else {
                for (unsigned int i=0; i<length; i++) {
                    Attribute *attr = attrs->attributeItem(i);
                    String value = attr->value();
                    if (attr->name() == styleAttr && additionalStyle.length() > 0)
                        value += "; " + additionalStyle;
                    // FIXME: Handle case where value has illegal characters in it, like "
                    if (documentIsHTML)
                        markup += " " + attr->name().localName().deprecatedString();
                    else
                        markup += " " + attr->name().toString().deprecatedString();
                    markup += "=\"" + escapeTextForMarkup(value.deprecatedString()) + "\"";
                }
            }
            
            if (shouldSelfClose(el)) {
                if (el->isHTMLElement())
                    markup += " "; // XHTML 1.0 <-> HTML compatibility.
                markup += "/>";
            } else
                markup += ">";
            
            return markup;
        }
        case Node::ATTRIBUTE_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::ENTITY_NODE:
        case Node::ENTITY_REFERENCE_NODE:
        case Node::NOTATION_NODE:
        case Node::XPATH_NAMESPACE_NODE:
            break;
    }
    return "";
}

static inline bool doesHTMLForbidEndTag(const Node *node)
{
    if (node->isHTMLElement()) {
        const HTMLElement* htmlElt = static_cast<const HTMLElement*>(node);
        return (htmlElt->endTagRequirement() == TagStatusForbidden);
    }
    return false;
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not have a "forbidden" end tag will close with a separate end tag.
// 4. Other elements self-close.
static inline bool shouldSelfClose(const Node *node)
{
    if (node->document()->isHTMLDocument())
        return false;
    if (node->hasChildNodes())
        return false;
    if (node->isHTMLElement() && !doesHTMLForbidEndTag(node))
        return false;
    return true;
}

static DeprecatedString endMarkup(const Node *node)
{
    if (node->isElementNode() && !shouldSelfClose(node) && !doesHTMLForbidEndTag(node))
        return "</" + static_cast<const Element*>(node)->nodeNamePreservingCase().deprecatedString() + ">";
    return "";
}

static DeprecatedString markup(const Node *startNode, bool onlyIncludeChildren, bool includeSiblings, DeprecatedPtrList<Node> *nodes)
{
    // Doesn't make sense to only include children and include siblings.
    ASSERT(!(onlyIncludeChildren && includeSiblings));
    DeprecatedString me = "";
    for (const Node *current = startNode; current != NULL; current = includeSiblings ? current->nextSibling() : NULL) {
        if (!onlyIncludeChildren) {
            if (nodes)
                nodes->append(current);
            me += startMarkup(current, 0, DoNotAnnotateForInterchange, 0);
        }
        // print children
        if (Node *n = current->firstChild())
            if (!(n->document()->isHTMLDocument() && doesHTMLForbidEndTag(current)))
                me += markup(n, false, true, nodes);
        
        // Print my ending tag
        if (!onlyIncludeChildren)
            me += endMarkup(current);
    }
    return me;
}

static void completeURLs(Node *node, const DeprecatedString &baseURL)
{
    Node *end = node->traverseNextSibling();
    for (Node *n = node; n != end; n = n->traverseNextNode()) {
        if (n->isElementNode()) {
            Element *e = static_cast<Element*>(n);
            NamedAttrMap *attrs = e->attributes();
            unsigned length = attrs->length();
            for (unsigned i = 0; i < length; i++) {
                Attribute *attr = attrs->attributeItem(i);
                if (e->isURLAttribute(attr))
                    e->setAttribute(attr->name(), KURL(baseURL, attr->value().deprecatedString()).url());
            }
        }
    }
}

DeprecatedString createMarkup(const Range *range, DeprecatedPtrList<Node> *nodes, EAnnotateForInterchange annotate)
{
    if (!range || range->isDetached())
        return DeprecatedString();

    static const DeprecatedString interchangeNewlineString = DeprecatedString("<br class=\"") + AppleInterchangeNewline + "\">";

    ExceptionCode ec = 0;
    if (range->collapsed(ec))
        return "";
    ASSERT(ec == 0);
    Node *commonAncestor = range->commonAncestorContainer(ec);
    ASSERT(ec == 0);

    Document *doc = commonAncestor->document();
    doc->updateLayoutIgnorePendingStylesheets();

    Node *commonAncestorBlock = 0;
    if (commonAncestor)
        commonAncestorBlock = commonAncestor->enclosingBlockFlowElement();
    if (!commonAncestorBlock)
        return "";

    DeprecatedStringList markups;
    Node *pastEnd = range->pastEndNode();
    Node *lastClosed = 0;
    DeprecatedPtrList<Node> ancestorsToClose;

    // calculate the "default style" for this markup
    Position pos(doc->documentElement(), 0);
    RefPtr<CSSComputedStyleDeclaration> computedStyle = pos.computedStyle();
    RefPtr<CSSMutableStyleDeclaration> defaultStyle = computedStyle->copyInheritableProperties();
    
    Node *startNode = range->startNode();
    VisiblePosition visibleStart(range->startPosition(), VP_DEFAULT_AFFINITY);
    VisiblePosition visibleEnd(range->endPosition(), VP_DEFAULT_AFFINITY);
    if (!inSameBlock(visibleStart, visibleStart.next())) {
        if (visibleStart == visibleEnd.previous())
            return interchangeNewlineString;
        markups.append(interchangeNewlineString);
        startNode = startNode->traverseNextNode();
    }
    
    // Iterate through the nodes of the range.
    Node *next;
    for (Node *n = startNode; n != pastEnd; n = next) {
        next = n->traverseNextNode();

        if (n->isBlockFlow() && next == pastEnd)
            // Don't write out an empty block.
            continue;
        
        // Add the node to the markup.
        if (n->renderer()) {
            markups.append(startMarkup(n, range, annotate, defaultStyle.get()));
            if (nodes)
                nodes->append(n);
        }
        
        if (n->firstChild() == 0) {
            // Node has no children, add its close tag now.
            if (n->renderer()) {
                markups.append(endMarkup(n));
                lastClosed = n;
            }
            
            // Check if the node is the last leaf of a tree.
            if (n->nextSibling() == 0 || next == pastEnd) {
                if (!ancestorsToClose.isEmpty()) {
                    // Close up the ancestors.
                    while (Node *ancestor = ancestorsToClose.last()) {
                        if (next != pastEnd && ancestor == next->parentNode())
                            break;
                        // Not at the end of the range, close ancestors up to sibling of next node.
                        markups.append(endMarkup(ancestor));
                        lastClosed = ancestor;
                        ancestorsToClose.removeLast();
                    }
                } else {
                    // No ancestors to close, but need to add ancestors not in range since next node is in another tree. 
                    if (next != pastEnd) {
                        Node *nextParent = next->parentNode();
                        if (n != nextParent) {
                            for (Node *parent = n->parent(); parent != 0 && parent != nextParent; parent = parent->parentNode()) {
                                markups.prepend(startMarkup(parent, range, annotate, defaultStyle.get()));
                                markups.append(endMarkup(parent));
                                if (nodes)
                                    nodes->append(parent);
                                lastClosed = parent;
                            }
                        }
                    }
                }
            }
        } else if (n->renderer())
            // Node is an ancestor, set it to close eventually.
            ancestorsToClose.append(n);
    }
    
    Node *rangeStartNode = range->startNode();
    int rangeStartOffset = range->startOffset(ec);
    ASSERT(ec == 0);
    
    // Add ancestors up to the common ancestor block so inline ancestors such as FONT and B are part of the markup.
    if (lastClosed) {
        for (Node *ancestor = lastClosed->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (Range::compareBoundaryPoints(ancestor, 0, rangeStartNode, rangeStartOffset) >= 0) {
                // we have already added markup for this node
                continue;
            }
            bool breakAtEnd = false;
            if (commonAncestorBlock == ancestor) {
                // Include ancestors that are required to retain the appearance of the copied markup.
                if (ancestor->hasTagName(listingTag)
                        || ancestor->hasTagName(olTag)
                        || ancestor->hasTagName(preTag)
                        || ancestor->hasTagName(tableTag)
                        || ancestor->hasTagName(ulTag)) {
                    breakAtEnd = true;
                } else
                    break;
            }
            markups.prepend(startMarkup(ancestor, range, annotate, defaultStyle.get()));
            markups.append(endMarkup(ancestor));
            if (nodes) {
                nodes->append(ancestor);
            }        
            if (breakAtEnd)
                break;
        }
    }

    if (annotate) {
        if (!inSameBlock(visibleEnd, visibleEnd.previous()))
            markups.append(interchangeNewlineString);
    }

    // Retain the Mail quote level by including all ancestor mail block quotes.
    for (Node *ancestor = commonAncestorBlock; ancestor; ancestor = ancestor->parentNode()) {
        if (isMailBlockquote(ancestor)) {
            markups.prepend(startMarkup(ancestor, range, annotate, defaultStyle.get()));
            markups.append(endMarkup(ancestor));
        }
    }
    
    // add in the "default style" for this markup
    // FIXME: Handle case where value has illegal characters in it, like "
    DeprecatedString openTag = DeprecatedString("<span class=\"") + AppleStyleSpanClass + "\" style=\"" + defaultStyle->cssText().deprecatedString() + "\">";
    markups.prepend(openTag);
    markups.append("</span>");

    return markups.join("");
}

PassRefPtr<DocumentFragment> createFragmentFromMarkup(Document* document, const String& markup, const String& baseURL)
{
    ASSERT(document->documentElement()->isHTMLElement());
    // FIXME: What if the document element is not an HTML element?
    HTMLElement *element = static_cast<HTMLElement*>(document->documentElement());

    RefPtr<DocumentFragment> fragment = element->createContextualFragment(markup);
    ASSERT(fragment);

    if (!baseURL.isEmpty() && baseURL != document->baseURL())
        completeURLs(fragment.get(), baseURL.deprecatedString());

    return fragment.release();
}

DeprecatedString createMarkup(const WebCore::Node *node, EChildrenOnly includeChildren,
    DeprecatedPtrList<WebCore::Node> *nodes, EAnnotateForInterchange annotate)
{
    ASSERT(annotate == DoNotAnnotateForInterchange); // annotation not yet implemented for this code path
    node->document()->updateLayoutIgnorePendingStylesheets();
    return markup(node, includeChildren, false, nodes);
}

static void createParagraphContentsFromString(WebCore::Document *document, Element *paragraph, const DeprecatedString &string)
{
    ExceptionCode ec = 0;
    if (string.isEmpty()) {
        paragraph->appendChild(createBlockPlaceholderElement(document), ec);
        ASSERT(ec == 0);
        return;
    }

    assert(string.find('\n') == -1);

    DeprecatedStringList tabList = DeprecatedStringList::split('\t', string, true);
    DeprecatedString tabText = "";
    while (!tabList.isEmpty()) {
        DeprecatedString s = tabList.first();
        tabList.pop_front();

        // append the non-tab textual part
        if (!s.isEmpty()) {
            if (tabText != "") {
                paragraph->appendChild(createTabSpanElement(document, tabText), ec);
                ASSERT(ec == 0);
                tabText = "";
            }
            RefPtr<Node> textNode = document->createTextNode(s);
            rebalanceWhitespaceInTextNode(textNode.get(), 0, s.length());
            paragraph->appendChild(textNode.release(), ec);
            ASSERT(ec == 0);
        }

        // there is a tab after every entry, except the last entry
        // (if the last character is a tab, the list gets an extra empty entry)
        if (!tabList.isEmpty()) {
            tabText += '\t';
        } else if (tabText != "") {
            paragraph->appendChild(createTabSpanElement(document, tabText), ec);
            ASSERT(ec == 0);
        }
    }
}

PassRefPtr<DocumentFragment> createFragmentFromText(Document *document, const DeprecatedString &text)
{
    if (!document)
        return 0;

    RefPtr<DocumentFragment> fragment = document->createDocumentFragment();
    
    if (!text.isEmpty()) {
        DeprecatedString string = text;

        // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
        string.replace("\r\n", "\n");
        string.replace('\r', '\n');
        DeprecatedStringList list = DeprecatedStringList::split('\n', string, true); // true gets us empty strings in the list
        while (!list.isEmpty()) {
            DeprecatedString s = list.first();
            list.pop_front();

            ExceptionCode ec = 0;
            RefPtr<Element> element;
            if (s.isEmpty() && list.isEmpty()) {
                // For last line, use the "magic BR" rather than a P.
                element = document->createElementNS(xhtmlNamespaceURI, "br", ec);
                ASSERT(ec == 0);
                element->setAttribute(classAttr, AppleInterchangeNewline);            
            } else {
                element = createDefaultParagraphElement(document);
                createParagraphContentsFromString(document, element.get(), s);
            }
            fragment->appendChild(element.get(), ec);
            ASSERT(ec == 0);
        }
    }
    return fragment.release();
}

PassRefPtr<DocumentFragment> createFragmentFromNodeList(Document *document, const DeprecatedPtrList<Node> &nodeList)
{
    if (!document)
        return 0;
    
    RefPtr<DocumentFragment> fragment = document->createDocumentFragment();

    ExceptionCode ec = 0;
    for (DeprecatedPtrListIterator<Node> i(nodeList); i.current(); ++i) {
        RefPtr<Element> element = createDefaultParagraphElement(document);
        element->appendChild(i.current(), ec);
        ASSERT(ec == 0);
        fragment->appendChild(element.release(), ec);
        ASSERT(ec == 0);
    }

    return fragment.release();
}

}
