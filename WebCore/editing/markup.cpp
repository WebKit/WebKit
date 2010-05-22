/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "CDATASection.h"
#include "CharacterNames.h"
#include "Comment.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "DeleteButtonController.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Editor.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "Logging.h"
#include "ProcessingInstruction.h"
#include "QualifiedName.h"
#include "Range.h"
#include "VisibleSelection.h"
#include "TextIterator.h"
#include "XMLNSNames.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>
#include "ApplyStyleCommand.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static inline bool shouldSelfClose(const Node *node);

class AttributeChange {
public:
    AttributeChange()
        : m_name(nullAtom, nullAtom, nullAtom)
    {
    }

    AttributeChange(PassRefPtr<Element> element, const QualifiedName& name, const String& value)
        : m_element(element), m_name(name), m_value(value)
    {
    }

    void apply()
    {
        m_element->setAttribute(m_name, m_value);
    }

private:
    RefPtr<Element> m_element;
    QualifiedName m_name;
    String m_value;
};

static void appendAttributeValue(Vector<UChar>& result, const String& attr, bool escapeNBSP)
{
    const UChar* uchars = attr.characters();
    unsigned len = attr.length();
    unsigned lastCopiedFrom = 0;

    DEFINE_STATIC_LOCAL(const String, ampEntity, ("&amp;"));
    DEFINE_STATIC_LOCAL(const String, gtEntity, ("&gt;"));
    DEFINE_STATIC_LOCAL(const String, ltEntity, ("&lt;"));
    DEFINE_STATIC_LOCAL(const String, quotEntity, ("&quot;"));
    DEFINE_STATIC_LOCAL(const String, nbspEntity, ("&nbsp;"));
    
    for (unsigned i = 0; i < len; ++i) {
        UChar c = uchars[i];
        switch (c) {
            case '&':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, ampEntity);
                lastCopiedFrom = i + 1;
                break;
            case '<':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, ltEntity);
                lastCopiedFrom = i + 1;
                break;
            case '>':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, gtEntity);
                lastCopiedFrom = i + 1;
                break;
            case '"':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, quotEntity);
                lastCopiedFrom = i + 1;
                break;
            case noBreakSpace:
                if (escapeNBSP) {
                    result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                    append(result, nbspEntity);
                    lastCopiedFrom = i + 1;
                }
                break;
        }
    }
    
    result.append(uchars + lastCopiedFrom, len - lastCopiedFrom);
}

static void appendEscapedContent(Vector<UChar>& result, pair<const UChar*, size_t> range, bool escapeNBSP)
{
    const UChar* uchars = range.first;
    unsigned len = range.second;
    unsigned lastCopiedFrom = 0;
    
    DEFINE_STATIC_LOCAL(const String, ampEntity, ("&amp;"));
    DEFINE_STATIC_LOCAL(const String, gtEntity, ("&gt;"));
    DEFINE_STATIC_LOCAL(const String, ltEntity, ("&lt;"));
    DEFINE_STATIC_LOCAL(const String, nbspEntity, ("&nbsp;"));

    for (unsigned i = 0; i < len; ++i) {
        UChar c = uchars[i];
        switch (c) {
            case '&':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, ampEntity);
                lastCopiedFrom = i + 1;
                break;
            case '<':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, ltEntity);
                lastCopiedFrom = i + 1;
                break;
            case '>':
                result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                append(result, gtEntity);
                lastCopiedFrom = i + 1;
                break;
            case noBreakSpace:
                if (escapeNBSP) {
                    result.append(uchars + lastCopiedFrom, i - lastCopiedFrom);
                    append(result, nbspEntity);
                    lastCopiedFrom = i + 1;
                }
                break;
        }
    }
    
    result.append(uchars + lastCopiedFrom, len - lastCopiedFrom);
}    

static String escapeContentText(const String& in, bool escapeNBSP)
{
    Vector<UChar> buffer;
    appendEscapedContent(buffer, make_pair(in.characters(), in.length()), escapeNBSP);
    return String::adopt(buffer);
}
    
static void appendQuotedURLAttributeValue(Vector<UChar>& result, const String& urlString)
{
    UChar quoteChar = '\"';
    String strippedURLString = urlString.stripWhiteSpace();
    if (protocolIsJavaScript(strippedURLString)) {
        // minimal escaping for javascript urls
        if (strippedURLString.contains('"')) {
            if (strippedURLString.contains('\''))
                strippedURLString.replace('\"', "&quot;");
            else
                quoteChar = '\'';
        }
        result.append(quoteChar);
        append(result, strippedURLString);
        result.append(quoteChar);
        return;
    }

    // FIXME: This does not fully match other browsers. Firefox percent-escapes non-ASCII characters for innerHTML.
    result.append(quoteChar);
    appendAttributeValue(result, urlString, false);
    result.append(quoteChar);    
}
    
static String stringValueForRange(const Node* node, const Range* range)
{
    if (!range)
        return node->nodeValue();

    String str = node->nodeValue();
    ExceptionCode ec;
    if (node == range->endContainer(ec))
        str.truncate(range->endOffset(ec));
    if (node == range->startContainer(ec))
        str.remove(0, range->startOffset(ec));
    return str;
}

static inline pair<const UChar*, size_t> ucharRange(const Node *node, const Range *range)
{
    String str = node->nodeValue();
    const UChar* characters = str.characters();
    size_t length = str.length();

    if (range) {
        ExceptionCode ec;
        if (node == range->endContainer(ec))
            length = range->endOffset(ec);
        if (node == range->startContainer(ec)) {
            size_t start = range->startOffset(ec);
            characters += start;
            length -= start;
        }
    }
    
    return make_pair(characters, length);
}
    
static inline void appendUCharRange(Vector<UChar>& result, const pair<const UChar*, size_t> range)
{
    result.append(range.first, range.second);
}
    
static String renderedText(const Node* node, const Range* range)
{
    if (!node->isTextNode())
        return String();

    ExceptionCode ec;
    const Text* textNode = static_cast<const Text*>(node);
    unsigned startOffset = 0;
    unsigned endOffset = textNode->length();

    if (range && node == range->startContainer(ec))
        startOffset = range->startOffset(ec);
    if (range && node == range->endContainer(ec))
        endOffset = range->endOffset(ec);
    
    Position start(const_cast<Node*>(node), startOffset);
    Position end(const_cast<Node*>(node), endOffset);
    return plainText(Range::create(node->document(), start, end).get());
}

static PassRefPtr<CSSMutableStyleDeclaration> styleFromMatchedRulesForElement(Element* element, bool authorOnly = true)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    RefPtr<CSSRuleList> matchedRules = element->document()->styleSelector()->styleRulesForElement(element, authorOnly);
    if (matchedRules) {
        for (unsigned i = 0; i < matchedRules->length(); i++) {
            if (matchedRules->item(i)->type() == CSSRule::STYLE_RULE) {
                RefPtr<CSSMutableStyleDeclaration> s = static_cast<CSSStyleRule*>(matchedRules->item(i))->style();
                style->merge(s.get(), true);
            }
        }
    }
    
    return style.release();
}

static void removeEnclosingMailBlockquoteStyle(CSSMutableStyleDeclaration* style, Node* node)
{
    Node* blockquote = nearestMailBlockquote(node);
    if (!blockquote || !blockquote->parentNode())
        return;

    removeStylesAddedByNode(style, blockquote);
}

static void removeDefaultStyles(CSSMutableStyleDeclaration* style, Document* document)
{
    if (!document || !document->documentElement())
        return;

    prepareEditingStyleToApplyAt(style, Position(document->documentElement(), 0));
}

static bool shouldAddNamespaceElem(const Element* elem)
{
    // Don't add namespace attribute if it is already defined for this elem.
    const AtomicString& prefix = elem->prefix();
    AtomicString attr = !prefix.isEmpty() ? "xmlns:" + prefix : "xmlns";
    return !elem->hasAttribute(attr);
}

static bool shouldAddNamespaceAttr(const Attribute* attr, HashMap<AtomicStringImpl*, AtomicStringImpl*>& namespaces)
{
    namespaces.checkConsistency();

    // Don't add namespace attributes twice
    if (attr->name() == XMLNSNames::xmlnsAttr) {
        namespaces.set(emptyAtom.impl(), attr->value().impl());
        return false;
    }
    
    QualifiedName xmlnsPrefixAttr(xmlnsAtom, attr->localName(), XMLNSNames::xmlnsNamespaceURI);
    if (attr->name() == xmlnsPrefixAttr) {
        namespaces.set(attr->localName().impl(), attr->value().impl());
        return false;
    }
    
    return true;
}

static void appendNamespace(Vector<UChar>& result, const AtomicString& prefix, const AtomicString& ns, HashMap<AtomicStringImpl*, AtomicStringImpl*>& namespaces)
{
    namespaces.checkConsistency();
    if (ns.isEmpty())
        return;
        
    // Use emptyAtoms's impl() for both null and empty strings since the HashMap can't handle 0 as a key
    AtomicStringImpl* pre = prefix.isEmpty() ? emptyAtom.impl() : prefix.impl();
    AtomicStringImpl* foundNS = namespaces.get(pre);
    if (foundNS != ns.impl()) {
        namespaces.set(pre, ns.impl());
        result.append(' ');
        append(result, xmlnsAtom.string());
        if (!prefix.isEmpty()) {
            result.append(':');
            append(result, prefix);
        }

        result.append('=');
        result.append('"');
        appendAttributeValue(result, ns, false);
        result.append('"');
    }
}

static void appendDocumentType(Vector<UChar>& result, const DocumentType* n)
{
    if (n->name().isEmpty())
        return;

    append(result, "<!DOCTYPE ");
    append(result, n->name());
    if (!n->publicId().isEmpty()) {
        append(result, " PUBLIC \"");
        append(result, n->publicId());
        append(result, "\"");
        if (!n->systemId().isEmpty()) {
            append(result, " \"");
            append(result, n->systemId());
            append(result, "\"");
        }
    } else if (!n->systemId().isEmpty()) {
        append(result, " SYSTEM \"");
        append(result, n->systemId());
        append(result, "\"");
    }
    if (!n->internalSubset().isEmpty()) {
        append(result, " [");
        append(result, n->internalSubset());
        append(result, "]");
    }
    append(result, ">");
}

static void removeExteriorStyles(CSSMutableStyleDeclaration* style)
{
    style->removeProperty(CSSPropertyFloat);
}

enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

static void appendStartMarkup(Vector<UChar>& result, const Node* node, const Range* range, EAnnotateForInterchange annotate, bool convertBlocksToInlines = false, HashMap<AtomicStringImpl*, AtomicStringImpl*>* namespaces = 0, RangeFullySelectsNode rangeFullySelectsNode = DoesFullySelectNode)
{
    if (namespaces)
        namespaces->checkConsistency();

    bool documentIsHTML = node->document()->isHTMLDocument();
    switch (node->nodeType()) {
        case Node::TEXT_NODE: {
            if (Node* parent = node->parentNode()) {
                if (parent->hasTagName(scriptTag)
                    || parent->hasTagName(styleTag)
                    || parent->hasTagName(xmpTag)) {
                    appendUCharRange(result, ucharRange(node, range));
                    break;
                } else if (parent->hasTagName(textareaTag)) {
                    appendEscapedContent(result, ucharRange(node, range), documentIsHTML);                    
                    break;
                }
            }
            if (!annotate) {
                appendEscapedContent(result, ucharRange(node, range), documentIsHTML);
                break;
            }
            
            bool useRenderedText = !enclosingNodeWithTag(Position(const_cast<Node*>(node), 0), selectTag);
            String markup = escapeContentText(useRenderedText ? renderedText(node, range) : stringValueForRange(node, range), false);
            markup = convertHTMLTextToInterchangeFormat(markup, static_cast<const Text*>(node));
            append(result, markup);
            break;
        }
        case Node::COMMENT_NODE:
            // FIXME: Comment content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "-->".
            append(result, "<!--");
            append(result, static_cast<const Comment*>(node)->data());
            append(result, "-->");
            break;
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
            break;
        case Node::DOCUMENT_TYPE_NODE:
            appendDocumentType(result, static_cast<const DocumentType*>(node));
            break;
        case Node::PROCESSING_INSTRUCTION_NODE: {
            // FIXME: PI data is not escaped, but XMLSerializer (and possibly other callers) this should raise an exception if it includes "?>".
            const ProcessingInstruction* n = static_cast<const ProcessingInstruction*>(node);
            append(result, "<?");
            append(result, n->target());
            append(result, " ");
            append(result, n->data());
            append(result, "?>");
            break;
        }
        case Node::ELEMENT_NODE: {
            result.append('<');
            const Element* el = static_cast<const Element*>(node);
            bool convert = convertBlocksToInlines && isBlock(const_cast<Node*>(node));
            append(result, el->nodeNamePreservingCase());
            NamedNodeMap *attrs = el->attributes();
            unsigned length = attrs->length();
            if (!documentIsHTML && namespaces && shouldAddNamespaceElem(el))
                appendNamespace(result, el->prefix(), el->namespaceURI(), *namespaces);

            for (unsigned int i = 0; i < length; i++) {
                Attribute *attr = attrs->attributeItem(i);
                // We'll handle the style attribute separately, below.
                if (attr->name() == styleAttr && el->isHTMLElement() && (annotate || convert))
                    continue;
                result.append(' ');

                if (documentIsHTML)
                    append(result, attr->name().localName());
                else
                    append(result, attr->name().toString());

                result.append('=');

                if (el->isURLAttribute(attr))
                    appendQuotedURLAttributeValue(result, attr->value());
                else {
                    result.append('\"');
                    appendAttributeValue(result, attr->value(), documentIsHTML);
                    result.append('\"');
                }

                if (!documentIsHTML && namespaces && shouldAddNamespaceAttr(attr, *namespaces))
                    appendNamespace(result, attr->prefix(), attr->namespaceURI(), *namespaces);
            }
            
            if (el->isHTMLElement() && (annotate || convert)) {
                Element* element = const_cast<Element*>(el);
                RefPtr<CSSMutableStyleDeclaration> style = static_cast<HTMLElement*>(element)->getInlineStyleDecl()->copy();
                if (annotate) {
                    RefPtr<CSSMutableStyleDeclaration> styleFromMatchedRules = styleFromMatchedRulesForElement(const_cast<Element*>(el));
                    // Styles from the inline style declaration, held in the variable "style", take precedence 
                    // over those from matched rules.
                    styleFromMatchedRules->merge(style.get());
                    style = styleFromMatchedRules;
                    
                    RefPtr<CSSComputedStyleDeclaration> computedStyleForElement = computedStyle(element);
                    RefPtr<CSSMutableStyleDeclaration> fromComputedStyle = CSSMutableStyleDeclaration::create();
                    
                    {
                        CSSMutableStyleDeclaration::const_iterator end = style->end();
                        for (CSSMutableStyleDeclaration::const_iterator it = style->begin(); it != end; ++it) {
                            const CSSProperty& property = *it;
                            CSSValue* value = property.value();
                            // The property value, if it's a percentage, may not reflect the actual computed value.  
                            // For example: style="height: 1%; overflow: visible;" in quirksmode
                            // FIXME: There are others like this, see <rdar://problem/5195123> Slashdot copy/paste fidelity problem
                            if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE)
                                if (static_cast<CSSPrimitiveValue*>(value)->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
                                    if (RefPtr<CSSValue> computedPropertyValue = computedStyleForElement->getPropertyCSSValue(property.id()))
                                        fromComputedStyle->addParsedProperty(CSSProperty(property.id(), computedPropertyValue));
                        }
                    }
                    
                    style->merge(fromComputedStyle.get());
                }
                if (convert)
                    style->setProperty(CSSPropertyDisplay, CSSValueInline, true);
                // If the node is not fully selected by the range, then we don't want to keep styles that affect its relationship to the nodes around it
                // only the ones that affect it and the nodes within it.
                if (rangeFullySelectsNode == DoesNotFullySelectNode)
                    removeExteriorStyles(style.get());
                if (style->length() > 0) {
                    DEFINE_STATIC_LOCAL(const String, stylePrefix, (" style=\""));
                    append(result, stylePrefix);
                    appendAttributeValue(result, style->cssText(), documentIsHTML);
                    result.append('\"');
                }
            }
            
            if (shouldSelfClose(el)) {
                if (el->isHTMLElement())
                    result.append(' '); // XHTML 1.0 <-> HTML compatibility.
                result.append('/');
            }
            result.append('>');
            break;
        }
        case Node::CDATA_SECTION_NODE: {
            // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "]]>".
            const CDATASection* n = static_cast<const CDATASection*>(node);
            append(result, "<![CDATA[");
            append(result, n->data());
            append(result, "]]>");
            break;
        }
        case Node::ATTRIBUTE_NODE:
        case Node::ENTITY_NODE:
        case Node::ENTITY_REFERENCE_NODE:
        case Node::NOTATION_NODE:
        case Node::XPATH_NAMESPACE_NODE:
            ASSERT_NOT_REACHED();
            break;
    }
}

static String getStartMarkup(const Node* node, const Range* range, EAnnotateForInterchange annotate, bool convertBlocksToInlines = false, HashMap<AtomicStringImpl*, AtomicStringImpl*>* namespaces = 0, RangeFullySelectsNode rangeFullySelectsNode = DoesFullySelectNode)
{
    Vector<UChar> result;
    appendStartMarkup(result, node, range, annotate, convertBlocksToInlines, namespaces, rangeFullySelectsNode);
    return String::adopt(result);
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

static void appendEndMarkup(Vector<UChar>& result, const Node* node)
{
    if (!node->isElementNode() || shouldSelfClose(node) || (!node->hasChildNodes() && doesHTMLForbidEndTag(node)))
        return;

    result.append('<');
    result.append('/');
    append(result, static_cast<const Element*>(node)->nodeNamePreservingCase());
    result.append('>');
}

static String getEndMarkup(const Node *node)
{
    Vector<UChar> result;
    appendEndMarkup(result, node);
    return String::adopt(result);
}

class MarkupAccumulator {
public:
    MarkupAccumulator(Node* nodeToSkip, Vector<Node*>* nodes)
        : m_nodeToSkip(nodeToSkip)
        , m_nodes(nodes)
    {
    }

    void appendMarkup(Node* startNode, EChildrenOnly, const HashMap<AtomicStringImpl*, AtomicStringImpl*>* namespaces = 0);

    String takeResult() { return String::adopt(m_result); }

private:
    Vector<UChar> m_result;
    Node* m_nodeToSkip;
    Vector<Node*>* m_nodes;
};

// FIXME: Would be nice to do this in a non-recursive way.
void MarkupAccumulator::appendMarkup(Node* startNode, EChildrenOnly childrenOnly, const HashMap<AtomicStringImpl*, AtomicStringImpl*>* namespaces)
{
    if (startNode == m_nodeToSkip)
        return;

    HashMap<AtomicStringImpl*, AtomicStringImpl*> namespaceHash;
    if (namespaces)
        namespaceHash = *namespaces;

    // start tag
    if (!childrenOnly) {
        if (m_nodes)
            m_nodes->append(startNode);
        appendStartMarkup(m_result, startNode, 0, DoNotAnnotateForInterchange, false, &namespaceHash);
    }

    // children
    if (!(startNode->document()->isHTMLDocument() && doesHTMLForbidEndTag(startNode))) {
        for (Node* current = startNode->firstChild(); current; current = current->nextSibling())
            appendMarkup(current, IncludeNode, &namespaceHash);
    }

    // end tag
    if (!childrenOnly)
        appendEndMarkup(m_result, startNode);
}

static void completeURLs(Node* node, const String& baseURL)
{
    Vector<AttributeChange> changes;

    KURL parsedBaseURL(ParsedURLString, baseURL);

    Node* end = node->traverseNextSibling();
    for (Node* n = node; n != end; n = n->traverseNextNode()) {
        if (n->isElementNode()) {
            Element* e = static_cast<Element*>(n);
            NamedNodeMap* attrs = e->attributes();
            unsigned length = attrs->length();
            for (unsigned i = 0; i < length; i++) {
                Attribute* attr = attrs->attributeItem(i);
                if (e->isURLAttribute(attr))
                    changes.append(AttributeChange(e, attr->name(), KURL(parsedBaseURL, attr->value()).string()));
            }
        }
    }

    size_t numChanges = changes.size();
    for (size_t i = 0; i < numChanges; ++i)
        changes[i].apply();
}

static bool needInterchangeNewlineAfter(const VisiblePosition& v)
{
    VisiblePosition next = v.next();
    Node* upstreamNode = next.deepEquivalent().upstream().node();
    Node* downstreamNode = v.deepEquivalent().downstream().node();
    // Add an interchange newline if a paragraph break is selected and a br won't already be added to the markup to represent it.
    return isEndOfParagraph(v) && isStartOfParagraph(next) && !(upstreamNode->hasTagName(brTag) && upstreamNode == downstreamNode);
}

static PassRefPtr<CSSMutableStyleDeclaration> styleFromMatchedRulesAndInlineDecl(const Node* node)
{
    if (!node->isHTMLElement())
        return 0;
    
    // FIXME: Having to const_cast here is ugly, but it is quite a bit of work to untangle
    // the non-const-ness of styleFromMatchedRulesForElement.
    HTMLElement* element = const_cast<HTMLElement*>(static_cast<const HTMLElement*>(node));
    RefPtr<CSSMutableStyleDeclaration> style = styleFromMatchedRulesForElement(element);
    RefPtr<CSSMutableStyleDeclaration> inlineStyleDecl = element->getInlineStyleDecl();
    style->merge(inlineStyleDecl.get());
    return style.release();
}

static bool propertyMissingOrEqualToNone(CSSStyleDeclaration* style, int propertyID)
{
    if (!style)
        return false;
    RefPtr<CSSValue> value = style->getPropertyCSSValue(propertyID);
    if (!value)
        return true;
    if (!value->isPrimitiveValue())
        return false;
    return static_cast<CSSPrimitiveValue*>(value.get())->getIdent() == CSSValueNone;
}

static bool isElementPresentational(const Node* node)
{
    if (node->hasTagName(uTag) || node->hasTagName(sTag) || node->hasTagName(strikeTag) ||
        node->hasTagName(iTag) || node->hasTagName(emTag) || node->hasTagName(bTag) || node->hasTagName(strongTag))
        return true;
    RefPtr<CSSMutableStyleDeclaration> style = styleFromMatchedRulesAndInlineDecl(node);
    if (!style)
        return false;
    return !propertyMissingOrEqualToNone(style.get(), CSSPropertyTextDecoration);
}

static String joinMarkups(const Vector<String>& preMarkups, const Vector<String>& postMarkups)
{
    size_t length = 0;

    size_t preCount = preMarkups.size();
    for (size_t i = 0; i < preCount; ++i)
        length += preMarkups[i].length();

    size_t postCount = postMarkups.size();
    for (size_t i = 0; i < postCount; ++i)
        length += postMarkups[i].length();

    Vector<UChar> result;
    result.reserveInitialCapacity(length);

    for (size_t i = preCount; i > 0; --i)
        append(result, preMarkups[i - 1]);

    for (size_t i = 0; i < postCount; ++i)
        append(result, postMarkups[i]);

    return String::adopt(result);
}

static bool isSpecialAncestorBlock(Node* node)
{
    if (!node || !isBlock(node))
        return false;
        
    return node->hasTagName(listingTag) ||
           node->hasTagName(olTag) ||
           node->hasTagName(preTag) ||
           node->hasTagName(tableTag) ||
           node->hasTagName(ulTag) ||
           node->hasTagName(xmpTag) ||
           node->hasTagName(h1Tag) ||
           node->hasTagName(h2Tag) ||
           node->hasTagName(h3Tag) ||
           node->hasTagName(h4Tag) ||
           node->hasTagName(h5Tag);
}

static bool shouldIncludeWrapperForFullySelectedRoot(Node* fullySelectedRoot, CSSMutableStyleDeclaration* style)
{
    if (fullySelectedRoot->isElementNode() && static_cast<Element*>(fullySelectedRoot)->hasAttribute(backgroundAttr))
        return true;
        
    return style->getPropertyCSSValue(CSSPropertyBackgroundImage) ||
           style->getPropertyCSSValue(CSSPropertyBackgroundColor);
}

static void addStyleMarkup(Vector<String>& preMarkups, Vector<String>& postMarkups, CSSStyleDeclaration* style, Document* document, bool isBlock = false)
{
    // All text-decoration-related elements should have been treated as special ancestors
    // If we ever hit this ASSERT, we should export StyleChange in ApplyStyleCommand and use it here
    ASSERT(propertyMissingOrEqualToNone(style, CSSPropertyTextDecoration) && propertyMissingOrEqualToNone(style, CSSPropertyWebkitTextDecorationsInEffect));
    DEFINE_STATIC_LOCAL(const String, divStyle, ("<div style=\""));
    DEFINE_STATIC_LOCAL(const String, divClose, ("</div>"));
    DEFINE_STATIC_LOCAL(const String, styleSpanOpen, ("<span class=\"" AppleStyleSpanClass "\" style=\""));
    DEFINE_STATIC_LOCAL(const String, styleSpanClose, ("</span>"));
    Vector<UChar> openTag;
    append(openTag, isBlock ? divStyle : styleSpanOpen);
    appendAttributeValue(openTag, style->cssText(), document->isHTMLDocument());
    openTag.append('\"');
    openTag.append('>');
    preMarkups.append(String::adopt(openTag));

    postMarkups.append(isBlock ? divClose : styleSpanClose);
}

// FIXME: Shouldn't we omit style info when annotate == DoNotAnnotateForInterchange? 
// FIXME: At least, annotation and style info should probably not be included in range.markupString()
String createMarkup(const Range* range, Vector<Node*>* nodes, EAnnotateForInterchange annotate, bool convertBlocksToInlines)
{
    DEFINE_STATIC_LOCAL(const String, interchangeNewlineString, ("<br class=\"" AppleInterchangeNewline "\">"));

    if (!range)
        return "";

    Document* document = range->ownerDocument();
    if (!document)
        return "";

    // Disable the delete button so it's elements are not serialized into the markup,
    // but make sure neither endpoint is inside the delete user interface.
    Frame* frame = document->frame();
    DeleteButtonController* deleteButton = frame ? frame->editor()->deleteButtonController() : 0;
    RefPtr<Range> updatedRange = avoidIntersectionWithNode(range, deleteButton ? deleteButton->containerElement() : 0);
    if (!updatedRange)
        return "";

    if (deleteButton)
        deleteButton->disable();

    ExceptionCode ec = 0;
    bool collapsed = updatedRange->collapsed(ec);
    ASSERT(ec == 0);
    if (collapsed)
        return "";
    Node* commonAncestor = updatedRange->commonAncestorContainer(ec);
    ASSERT(ec == 0);
    if (!commonAncestor)
        return "";

    document->updateLayoutIgnorePendingStylesheets();

    Vector<String> markups;
    Vector<String> preMarkups;
    Node* pastEnd = updatedRange->pastLastNode();
    Node* lastClosed = 0;
    Vector<Node*> ancestorsToClose;
    
    Node* startNode = updatedRange->firstNode();
    VisiblePosition visibleStart(updatedRange->startPosition(), VP_DEFAULT_AFFINITY);
    VisiblePosition visibleEnd(updatedRange->endPosition(), VP_DEFAULT_AFFINITY);
    if (annotate && needInterchangeNewlineAfter(visibleStart)) {
        if (visibleStart == visibleEnd.previous()) {
            if (deleteButton)
                deleteButton->enable();
            return interchangeNewlineString;
        }

        markups.append(interchangeNewlineString);
        startNode = visibleStart.next().deepEquivalent().node();

        if (pastEnd && Range::compareBoundaryPoints(startNode, 0, pastEnd, 0) >= 0) {
            if (deleteButton)
                deleteButton->enable();
            return interchangeNewlineString;
        }
    }

    Node* next;
    for (Node* n = startNode; n != pastEnd; n = next) {
        // According to <rdar://problem/5730668>, it is possible for n to blow
        // past pastEnd and become null here. This shouldn't be possible.
        // This null check will prevent crashes (but create too much markup)
        // and the ASSERT will hopefully lead us to understanding the problem.
        ASSERT(n);
        if (!n)
            break;
    
        next = n->traverseNextNode();
        bool skipDescendants = false;
        bool addMarkupForNode = true;
        
        if (!n->renderer() && !enclosingNodeWithTag(Position(n, 0), selectTag)) {
            skipDescendants = true;
            addMarkupForNode = false;
            next = n->traverseNextSibling();
            // Don't skip over pastEnd.
            if (pastEnd && pastEnd->isDescendantOf(n))
                next = pastEnd;
        }

        if (isBlock(n) && canHaveChildrenForEditing(n) && next == pastEnd)
            // Don't write out empty block containers that aren't fully selected.
            continue;
        
        // Add the node to the markup.
        if (addMarkupForNode) {
            markups.append(getStartMarkup(n, updatedRange.get(), annotate));
            if (nodes)
                nodes->append(n);
        }
        
        if (n->firstChild() == 0 || skipDescendants) {
            // Node has no children, or we are skipping it's descendants, add its close tag now.
            if (addMarkupForNode) {
                markups.append(getEndMarkup(n));
                lastClosed = n;
            }
            
            // Check if the node is the last leaf of a tree.
            if (!n->nextSibling() || next == pastEnd) {
                if (!ancestorsToClose.isEmpty()) {
                    // Close up the ancestors.
                    do {
                        Node *ancestor = ancestorsToClose.last();
                        if (next != pastEnd && next->isDescendantOf(ancestor))
                            break;
                        // Not at the end of the range, close ancestors up to sibling of next node.
                        markups.append(getEndMarkup(ancestor));
                        lastClosed = ancestor;
                        ancestorsToClose.removeLast();
                    } while (!ancestorsToClose.isEmpty());
                }
                
                // Surround the currently accumulated markup with markup for ancestors we never opened as we leave the subtree(s) rooted at those ancestors.
                Node* nextParent = next ? next->parentNode() : 0;
                if (next != pastEnd && n != nextParent) {
                    Node* lastAncestorClosedOrSelf = n->isDescendantOf(lastClosed) ? lastClosed : n;
                    for (Node *parent = lastAncestorClosedOrSelf->parent(); parent != 0 && parent != nextParent; parent = parent->parentNode()) {
                        // All ancestors that aren't in the ancestorsToClose list should either be a) unrendered:
                        if (!parent->renderer())
                            continue;
                        // or b) ancestors that we never encountered during a pre-order traversal starting at startNode:
                        ASSERT(startNode->isDescendantOf(parent));
                        preMarkups.append(getStartMarkup(parent, updatedRange.get(), annotate));
                        markups.append(getEndMarkup(parent));
                        if (nodes)
                            nodes->append(parent);
                        lastClosed = parent;
                    }
                }
            }
        } else if (addMarkupForNode && !skipDescendants)
            // We added markup for this node, and we're descending into it.  Set it to close eventually.
            ancestorsToClose.append(n);
    }

    // Include ancestors that aren't completely inside the range but are required to retain 
    // the structure and appearance of the copied markup.
    Node* specialCommonAncestor = 0;
    Node* commonAncestorBlock = commonAncestor ? enclosingBlock(commonAncestor) : 0;
    if (annotate && commonAncestorBlock) {
        if (commonAncestorBlock->hasTagName(tbodyTag) || commonAncestorBlock->hasTagName(trTag)) {
            Node* table = commonAncestorBlock->parentNode();
            while (table && !table->hasTagName(tableTag))
                table = table->parentNode();
            if (table)
                specialCommonAncestor = table;
        } else if (isSpecialAncestorBlock(commonAncestorBlock))
            specialCommonAncestor = commonAncestorBlock;
    }
                                      
    // Retain the Mail quote level by including all ancestor mail block quotes.
    if (lastClosed && annotate) {
        for (Node *ancestor = lastClosed->parentNode(); ancestor; ancestor = ancestor->parentNode())
            if (isMailBlockquote(ancestor))
                specialCommonAncestor = ancestor;
    }

    Node* checkAncestor = specialCommonAncestor ? specialCommonAncestor : commonAncestor;
    if (checkAncestor->renderer()) {
        Node* newSpecialCommonAncestor = highestEnclosingNodeOfType(Position(checkAncestor, 0), &isElementPresentational);
        if (newSpecialCommonAncestor)
            specialCommonAncestor = newSpecialCommonAncestor;
    }
    
    // If a single tab is selected, commonAncestor will be a text node inside a tab span.
    // If two or more tabs are selected, commonAncestor will be the tab span.
    // In either case, if there is a specialCommonAncestor already, it will necessarily be above 
    // any tab span that needs to be included.
    if (!specialCommonAncestor && isTabSpanTextNode(commonAncestor))
        specialCommonAncestor = commonAncestor->parentNode();
    if (!specialCommonAncestor && isTabSpanNode(commonAncestor))
        specialCommonAncestor = commonAncestor;
        
    if (Node *enclosingAnchor = enclosingNodeWithTag(Position(specialCommonAncestor ? specialCommonAncestor : commonAncestor, 0), aTag))
        specialCommonAncestor = enclosingAnchor;
    
    Node* body = enclosingNodeWithTag(Position(commonAncestor, 0), bodyTag);
    // FIXME: Do this for all fully selected blocks, not just the body.
    Node* fullySelectedRoot = body && areRangesEqual(VisibleSelection::selectionFromContentsOfNode(body).toNormalizedRange().get(), updatedRange.get()) ? body : 0;
    RefPtr<CSSMutableStyleDeclaration> fullySelectedRootStyle = fullySelectedRoot ? styleFromMatchedRulesAndInlineDecl(fullySelectedRoot) : 0;
    if (annotate && fullySelectedRoot) {
        if (shouldIncludeWrapperForFullySelectedRoot(fullySelectedRoot, fullySelectedRootStyle.get()))
            specialCommonAncestor = fullySelectedRoot;
    }
        
    if (specialCommonAncestor && lastClosed) {
        // Also include all of the ancestors of lastClosed up to this special ancestor.
        for (Node* ancestor = lastClosed->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (ancestor == fullySelectedRoot && !convertBlocksToInlines) {
                
                // Bring the background attribute over, but not as an attribute because a background attribute on a div
                // appears to have no effect.
                if (!fullySelectedRootStyle->getPropertyCSSValue(CSSPropertyBackgroundImage) && static_cast<Element*>(fullySelectedRoot)->hasAttribute(backgroundAttr))
                    fullySelectedRootStyle->setProperty(CSSPropertyBackgroundImage, "url('" + static_cast<Element*>(fullySelectedRoot)->getAttribute(backgroundAttr) + "')");
                
                if (fullySelectedRootStyle->length()) {
                    // Reset the CSS properties to avoid an assertion error in addStyleMarkup().
                    // This assertion is caused at least when we select all text of a <body> element whose
                    // 'text-decoration' property is "inherit", and copy it.
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle.get(), CSSPropertyTextDecoration))
                        fullySelectedRootStyle->setProperty(CSSPropertyTextDecoration, CSSValueNone);
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle.get(), CSSPropertyWebkitTextDecorationsInEffect))
                        fullySelectedRootStyle->setProperty(CSSPropertyWebkitTextDecorationsInEffect, CSSValueNone);
                    addStyleMarkup(preMarkups, markups, fullySelectedRootStyle.get(), document, true);
                }
            } else {
                // Since this node and all the other ancestors are not in the selection we want to set RangeFullySelectsNode to DoesNotFullySelectNode
                // so that styles that affect the exterior of the node are not included.
                preMarkups.append(getStartMarkup(ancestor, updatedRange.get(), annotate, convertBlocksToInlines, 0, DoesNotFullySelectNode));
                markups.append(getEndMarkup(ancestor));
            }
            if (nodes)
                nodes->append(ancestor);
            
            lastClosed = ancestor;
            
            if (ancestor == specialCommonAncestor)
                break;
        }
    }

    // Add a wrapper span with the styles that all of the nodes in the markup inherit.
    Node* parentOfLastClosed = lastClosed ? lastClosed->parentNode() : 0;
    if (parentOfLastClosed && parentOfLastClosed->renderer()) {
        RefPtr<CSSMutableStyleDeclaration> style = ApplyStyleCommand::editingStyleAtPosition(Position(parentOfLastClosed, 0));

        // Styles that Mail blockquotes contribute should only be placed on the Mail blockquote, to help
        // us differentiate those styles from ones that the user has applied.  This helps us
        // get the color of content pasted into blockquotes right.
        removeEnclosingMailBlockquoteStyle(style.get(), parentOfLastClosed);
        
        // Document default styles will be added on another wrapper span.
        removeDefaultStyles(style.get(), document);
        
        // Since we are converting blocks to inlines, remove any inherited block properties that are in the style.
        // This cuts out meaningless properties and prevents properties from magically affecting blocks later
        // if the style is cloned for a new block element during a future editing operation.
        if (convertBlocksToInlines)
            style->removeBlockProperties();

        if (style->length() > 0)
            addStyleMarkup(preMarkups, markups, style.get(), document);
    }
    
    if (lastClosed && lastClosed != document->documentElement()) {
        // Add a style span with the document's default styles.  We add these in a separate
        // span so that at paste time we can differentiate between document defaults and user
        // applied styles.
        RefPtr<CSSMutableStyleDeclaration> defaultStyle = ApplyStyleCommand::editingStyleAtPosition(Position(document->documentElement(), 0));

        if (defaultStyle->length() > 0)
            addStyleMarkup(preMarkups, markups, defaultStyle.get(), document);
    }

    // FIXME: The interchange newline should be placed in the block that it's in, not after all of the content, unconditionally.
    if (annotate && needInterchangeNewlineAfter(visibleEnd.previous()))
        markups.append(interchangeNewlineString);
    
    if (deleteButton)
        deleteButton->enable();

    return joinMarkups(preMarkups, markups);
}

PassRefPtr<DocumentFragment> createFragmentFromMarkup(Document* document, const String& markup, const String& baseURL, FragmentScriptingPermission scriptingPermission)
{
    RefPtr<DocumentFragment> fragment = document->documentElement()->createContextualFragment(markup, scriptingPermission);

    if (fragment && !baseURL.isEmpty() && baseURL != blankURL() && baseURL != document->baseURL())
        completeURLs(fragment.get(), baseURL);

    return fragment.release();
}

String createMarkup(const Node* node, EChildrenOnly childrenOnly, Vector<Node*>* nodes)
{
    if (!node)
        return "";

    HTMLElement* deleteButtonContainerElement = 0;
    if (Frame* frame = node->document()->frame()) {
        deleteButtonContainerElement = frame->editor()->deleteButtonController()->containerElement();
        if (node->isDescendantOf(deleteButtonContainerElement))
            return "";
    }

    MarkupAccumulator accumulator(deleteButtonContainerElement, nodes);
    accumulator.appendMarkup(const_cast<Node*>(node), childrenOnly);
    return accumulator.takeResult();
}

static void fillContainerFromString(ContainerNode* paragraph, const String& string)
{
    Document* document = paragraph->document();

    ExceptionCode ec = 0;
    if (string.isEmpty()) {
        paragraph->appendChild(createBlockPlaceholderElement(document), ec);
        ASSERT(ec == 0);
        return;
    }

    ASSERT(string.find('\n') == -1);

    Vector<String> tabList;
    string.split('\t', true, tabList);
    String tabText = "";
    bool first = true;
    size_t numEntries = tabList.size();
    for (size_t i = 0; i < numEntries; ++i) {
        const String& s = tabList[i];

        // append the non-tab textual part
        if (!s.isEmpty()) {
            if (!tabText.isEmpty()) {
                paragraph->appendChild(createTabSpanElement(document, tabText), ec);
                ASSERT(ec == 0);
                tabText = "";
            }
            RefPtr<Node> textNode = document->createTextNode(stringWithRebalancedWhitespace(s, first, i + 1 == numEntries));
            paragraph->appendChild(textNode.release(), ec);
            ASSERT(ec == 0);
        }

        // there is a tab after every entry, except the last entry
        // (if the last character is a tab, the list gets an extra empty entry)
        if (i + 1 != numEntries)
            tabText.append('\t');
        else if (!tabText.isEmpty()) {
            paragraph->appendChild(createTabSpanElement(document, tabText), ec);
            ASSERT(ec == 0);
        }
        
        first = false;
    }
}

bool isPlainTextMarkup(Node *node)
{
    if (!node->isElementNode() || !node->hasTagName(divTag) || static_cast<Element*>(node)->attributes()->length())
        return false;
    
    if (node->childNodeCount() == 1 && (node->firstChild()->isTextNode() || (node->firstChild()->firstChild())))
        return true;
    
    return (node->childNodeCount() == 2 && isTabSpanTextNode(node->firstChild()->firstChild()) && node->firstChild()->nextSibling()->isTextNode());
}

PassRefPtr<DocumentFragment> createFragmentFromText(Range* context, const String& text)
{
    if (!context)
        return 0;

    Node* styleNode = context->firstNode();
    if (!styleNode) {
        styleNode = context->startPosition().node();
        if (!styleNode)
            return 0;
    }

    Document* document = styleNode->document();
    RefPtr<DocumentFragment> fragment = document->createDocumentFragment();
    
    if (text.isEmpty())
        return fragment.release();

    String string = text;
    string.replace("\r\n", "\n");
    string.replace('\r', '\n');

    ExceptionCode ec = 0;
    RenderObject* renderer = styleNode->renderer();
    if (renderer && renderer->style()->preserveNewline()) {
        fragment->appendChild(document->createTextNode(string), ec);
        ASSERT(ec == 0);
        if (string.endsWith("\n")) {
            RefPtr<Element> element = createBreakElement(document);
            element->setAttribute(classAttr, AppleInterchangeNewline);            
            fragment->appendChild(element.release(), ec);
            ASSERT(ec == 0);
        }
        return fragment.release();
    }

    // A string with no newlines gets added inline, rather than being put into a paragraph.
    if (string.find('\n') == -1) {
        fillContainerFromString(fragment.get(), string);
        return fragment.release();
    }

    // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
    Node* blockNode = enclosingBlock(context->firstNode());
    Element* block = static_cast<Element*>(blockNode);
    bool useClonesOfEnclosingBlock = blockNode
        && blockNode->isElementNode()
        && !block->hasTagName(bodyTag)
        && !block->hasTagName(htmlTag)
        && block != editableRootForPosition(context->startPosition());
    
    Vector<String> list;
    string.split('\n', true, list); // true gets us empty strings in the list
    size_t numLines = list.size();
    for (size_t i = 0; i < numLines; ++i) {
        const String& s = list[i];

        RefPtr<Element> element;
        if (s.isEmpty() && i + 1 == numLines) {
            // For last line, use the "magic BR" rather than a P.
            element = createBreakElement(document);
            element->setAttribute(classAttr, AppleInterchangeNewline);            
        } else {
            if (useClonesOfEnclosingBlock)
                element = block->cloneElementWithoutChildren();
            else
                element = createDefaultParagraphElement(document);
            fillContainerFromString(element.get(), s);
        }
        fragment->appendChild(element.release(), ec);
        ASSERT(ec == 0);
    }
    return fragment.release();
}

PassRefPtr<DocumentFragment> createFragmentFromNodes(Document *document, const Vector<Node*>& nodes)
{
    if (!document)
        return 0;

    // disable the delete button so it's elements are not serialized into the markup
    if (document->frame())
        document->frame()->editor()->deleteButtonController()->disable();

    RefPtr<DocumentFragment> fragment = document->createDocumentFragment();

    ExceptionCode ec = 0;
    size_t size = nodes.size();
    for (size_t i = 0; i < size; ++i) {
        RefPtr<Element> element = createDefaultParagraphElement(document);
        element->appendChild(nodes[i], ec);
        ASSERT(ec == 0);
        fragment->appendChild(element.release(), ec);
        ASSERT(ec == 0);
    }

    if (document->frame())
        document->frame()->editor()->deleteButtonController()->enable();

    return fragment.release();
}

String createFullMarkup(const Node* node)
{
    if (!node)
        return String();
        
    Document* document = node->document();
    if (!document)
        return String();
        
    Frame* frame = document->frame();
    if (!frame)
        return String();

    // FIXME: This is never "for interchange". Is that right?    
    String markupString = createMarkup(node, IncludeNode, 0);
    Node::NodeType nodeType = node->nodeType();
    if (nodeType != Node::DOCUMENT_NODE && nodeType != Node::DOCUMENT_TYPE_NODE)
        markupString = frame->documentTypeString() + markupString;

    return markupString;
}

String createFullMarkup(const Range* range)
{
    if (!range)
        return String();

    Node* node = range->startContainer();
    if (!node)
        return String();
        
    Document* document = node->document();
    if (!document)
        return String();
        
    Frame* frame = document->frame();
    if (!frame)
        return String();

    // FIXME: This is always "for interchange". Is that right? See the previous method.
    return frame->documentTypeString() + createMarkup(range, 0, AnnotateForInterchange);        
}

}
