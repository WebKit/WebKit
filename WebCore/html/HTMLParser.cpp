/*
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1999,2001 Lars Knoll (knoll@kde.org)
              (C) 2000,2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "HTMLParser.h"

#include "CharacterNames.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ChromeClient.h"
#include "Comment.h"
#include "Console.h"
#include "DOMWindow.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
#include "HTMLBodyElement.h"
#include "HTMLDocument.h"
#include "HTMLDivElement.h"
#include "HTMLDListElement.h"
#include "HTMLElementFactory.h"
#include "HTMLFormElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHRElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIsIndexElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLParserQuirks.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTokenizer.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "Settings.h"
#include "Text.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

static const unsigned cMaxRedundantTagDepth = 20;
static const unsigned cResidualStyleMaxDepth = 200;

static const int minBlockLevelTagPriority = 3;

// A cap on the number of tags with priority minBlockLevelTagPriority or higher
// allowed in m_blockStack. The cap is enforced by adding such new elements as
// siblings instead of children once it is reached.
static const size_t cMaxBlockDepth = 4096;

struct HTMLStackElem : Noncopyable {
    HTMLStackElem(const AtomicString& t, int lvl, Node* n, bool r, HTMLStackElem* nx)
        : tagName(t)
        , level(lvl)
        , strayTableContent(false)
        , node(n)
        , didRefNode(r)
        , next(nx)
    {
    }

    void derefNode()
    {
        if (didRefNode)
            node->deref();
    }

    AtomicString tagName;
    int level;
    bool strayTableContent;
    Node* node;
    bool didRefNode;
    HTMLStackElem* next;
};

/**
 * The parser parses tokenized input into the document, building up the
 * document tree. If the document is well-formed, parsing it is straightforward.
 *
 * Unfortunately, we have to handle many HTML documents that are not well-formed,
 * so the parser has to be tolerant about errors.
 *
 * We have to take care of at least the following error conditions:
 *
 * 1. The element being added is explicitly forbidden inside some outer tag.
 *    In this case we should close all tags up to the one, which forbids
 *    the element, and add it afterwards.
 *
 * 2. We are not allowed to add the element directly. It could be that
 *    the person writing the document forgot some tag in between (or that the
 *    tag in between is optional). This could be the case with the following
 *    tags: HTML HEAD BODY TBODY TR TD LI (did I forget any?).
 *
 * 3. We want to add a block element inside to an inline element. Close all
 *    inline elements up to the next higher block element.
 *
 * 4. If this doesn't help, close elements until we are allowed to add the
 *    element or ignore the tag.
 *
 */

HTMLParser::HTMLParser(HTMLDocument* doc, bool reportErrors)
    : m_document(doc)
    , m_current(doc)
    , m_didRefCurrent(false)
    , m_blockStack(0)
    , m_blocksInStack(0)
    , m_hasPElementInScope(NotInScope)
    , m_inBody(false)
    , m_haveContent(false)
    , m_haveFrameSet(false)
    , m_isParsingFragment(false)
    , m_reportErrors(reportErrors)
    , m_handlingResidualStyleAcrossBlocks(false)
    , m_inStrayTableContent(0)
    , m_parserQuirks(m_document->page() ? m_document->page()->chrome()->client()->createHTMLParserQuirks() : 0)
{
}

HTMLParser::HTMLParser(DocumentFragment* frag)
    : m_document(frag->document())
    , m_current(frag)
    , m_didRefCurrent(true)
    , m_blockStack(0)
    , m_blocksInStack(0)
    , m_hasPElementInScope(NotInScope)
    , m_inBody(true)
    , m_haveContent(false)
    , m_haveFrameSet(false)
    , m_isParsingFragment(true)
    , m_reportErrors(false)
    , m_handlingResidualStyleAcrossBlocks(false)
    , m_inStrayTableContent(0)
    , m_parserQuirks(m_document->page() ? m_document->page()->chrome()->client()->createHTMLParserQuirks() : 0)
{
    if (frag)
        frag->ref();
}

HTMLParser::~HTMLParser()
{
    freeBlock();
    if (m_didRefCurrent)
        m_current->deref();
}

void HTMLParser::reset()
{
    ASSERT(!m_isParsingFragment);

    setCurrent(m_document);

    freeBlock();

    m_inBody = false;
    m_haveFrameSet = false;
    m_haveContent = false;
    m_inStrayTableContent = 0;

    m_currentFormElement = 0;
    m_currentMapElement = 0;
    m_head = 0;
    m_isindexElement = 0;

    m_skipModeTag = nullAtom;
    
    if (m_parserQuirks)
        m_parserQuirks->reset();
}

void HTMLParser::setCurrent(Node* newCurrent) 
{
    bool didRefNewCurrent = newCurrent && newCurrent != m_document;
    if (didRefNewCurrent) 
        newCurrent->ref(); 
    if (m_didRefCurrent) 
        m_current->deref();
    m_current = newCurrent;
    m_didRefCurrent = didRefNewCurrent;
}

PassRefPtr<Node> HTMLParser::parseToken(Token* t)
{
    if (!m_skipModeTag.isNull()) {
        if (!t->beginTag && t->tagName == m_skipModeTag)
            // Found the end tag for the current skip mode, so we're done skipping.
            m_skipModeTag = nullAtom;
        else if (m_current->localName() == t->tagName)
            // Do not skip </iframe>.
            // FIXME: What does that comment mean? How can it be right to parse a token without clearing m_skipModeTag?
            ;
        else
            return 0;
    }

    // Apparently some sites use </br> instead of <br>. Be compatible with IE and Firefox and treat this like <br>.
    if (t->isCloseTag(brTag) && m_document->inCompatMode()) {
        reportError(MalformedBRError);
        t->beginTag = true;
    }

    if (!t->beginTag) {
        processCloseTag(t);
        return 0;
    }

    // Ignore spaces, if we're not inside a paragraph or other inline code.
    // Do not alter the text if it is part of a scriptTag.
    if (t->tagName == textAtom && t->text && m_current->localName() != scriptTag) {
        if (m_inBody && !skipMode() && m_current->localName() != styleTag &&
            m_current->localName() != titleTag && !t->text->containsOnlyWhitespace())
            m_haveContent = true;
        
        RefPtr<Node> n;
        String text = t->text.get();
        unsigned charsLeft = text.length();
        while (charsLeft) {
            // split large blocks of text to nodes of manageable size
            n = Text::createWithLengthLimit(m_document, text, charsLeft);
            if (!insertNode(n.get(), t->selfClosingTag))
                return 0;
        }
        return n;
    }

    RefPtr<Node> n = getNode(t);
    // just to be sure, and to catch currently unimplemented stuff
    if (!n)
        return 0;

    // set attributes
    if (n->isHTMLElement()) {
        HTMLElement* e = static_cast<HTMLElement*>(n.get());
        e->setAttributeMap(t->attrs.get());

        // take care of optional close tags
        if (e->endTagRequirement() == TagStatusOptional)
            popBlock(t->tagName);
            
        // If the node does not have a forbidden end tag requirement, and if the broken XML self-closing
        // syntax was used, report an error.
        if (t->brokenXMLStyle && e->endTagRequirement() != TagStatusForbidden) {
            if (t->tagName == scriptTag)
                reportError(IncorrectXMLCloseScriptWarning);
            else
                reportError(IncorrectXMLSelfCloseError, &t->tagName);
        }
    }

    if (!insertNode(n.get(), t->selfClosingTag)) {
        // we couldn't insert the node

        if (n->isElementNode()) {
            Element* e = static_cast<Element*>(n.get());
            e->setAttributeMap(0);
        }

        if (m_currentMapElement == n)
            m_currentMapElement = 0;

        if (m_currentFormElement == n)
            m_currentFormElement = 0;

        if (m_head == n)
            m_head = 0;

        return 0;
    }
    return n;
}

void HTMLParser::parseDoctypeToken(DoctypeToken* t)
{
    // Ignore any doctype after the first.  Ignore doctypes in fragments.
    if (m_document->doctype() || m_isParsingFragment || m_current != m_document)
        return;
        
    // Make a new doctype node and set it as our doctype.
    m_document->addChild(DocumentType::create(m_document, String::adopt(t->m_name), String::adopt(t->m_publicID), String::adopt(t->m_systemID)));
}

static bool isTableSection(const Node* n)
{
    return n->hasTagName(tbodyTag) || n->hasTagName(tfootTag) || n->hasTagName(theadTag);
}

static bool isTablePart(const Node* n)
{
    return n->hasTagName(trTag) || n->hasTagName(tdTag) || n->hasTagName(thTag) ||
           isTableSection(n);
}

static bool isTableRelated(const Node* n)
{
    return n->hasTagName(tableTag) || isTablePart(n);
}

static bool isScopingTag(const AtomicString& tagName)
{
    return tagName == appletTag || tagName == captionTag || tagName == tdTag || tagName == thTag || tagName == buttonTag || tagName == marqueeTag || tagName == objectTag || tagName == tableTag || tagName == htmlTag;
}

bool HTMLParser::insertNode(Node* n, bool flat)
{
    RefPtr<Node> protectNode(n);

    const AtomicString& localName = n->localName();
    int tagPriority = n->isHTMLElement() ? static_cast<HTMLElement*>(n)->tagPriority() : 0;
    
    // <table> is never allowed inside stray table content.  Always pop out of the stray table content
    // and close up the first table, and then start the second table as a sibling.
    if (m_inStrayTableContent && localName == tableTag)
        popBlock(tableTag);

    if (tagPriority >= minBlockLevelTagPriority) {
        while (m_blocksInStack >= cMaxBlockDepth)
            popBlock(m_blockStack->tagName);
    }

    if (m_parserQuirks && !m_parserQuirks->shouldInsertNode(m_current, n))
        return false;

    // let's be stupid and just try to insert it.
    // this should work if the document is well-formed
    Node* newNode = m_current->addChild(n);
    if (!newNode)
        return handleError(n, flat, localName, tagPriority); // Try to handle the error.

    // don't push elements without end tags (e.g., <img>) on the stack
    bool parentAttached = m_current->attached();
    if (tagPriority > 0 && !flat) {
        if (newNode == m_current) {
            // This case should only be hit when a demoted <form> is placed inside a table.
            ASSERT(localName == formTag);
            reportError(FormInsideTablePartError, &m_current->localName());
            HTMLFormElement* form = static_cast<HTMLFormElement*>(n);
            form->setDemoted(true);
        } else {
            // The pushBlock function transfers ownership of current to the block stack
            // so we're guaranteed that m_didRefCurrent is false. The code below is an
            // optimized version of setCurrent that takes advantage of that fact and also
            // assumes that newNode is neither 0 nor a pointer to the document.
            pushBlock(localName, tagPriority);
            newNode->beginParsingChildren();
            ASSERT(!m_didRefCurrent);
            newNode->ref(); 
            m_current = newNode;
            m_didRefCurrent = true;
        }
        if (parentAttached && !n->attached() && !m_isParsingFragment)
            n->attach();
    } else {
        if (parentAttached && !n->attached() && !m_isParsingFragment)
            n->attach();
        n->finishParsingChildren();
    }

    if (localName == htmlTag && m_document->frame())
        m_document->frame()->loader()->dispatchDocumentElementAvailable();

    return true;
}

bool HTMLParser::handleError(Node* n, bool flat, const AtomicString& localName, int tagPriority)
{
    // Error handling code.  This is just ad hoc handling of specific parent/child combinations.
    HTMLElement* e;
    bool handled = false;

    // 1. Check out the element's tag name to decide how to deal with errors.
    if (n->isHTMLElement()) {
        HTMLElement* h = static_cast<HTMLElement*>(n);
        if (h->hasLocalName(trTag) || h->hasLocalName(thTag) || h->hasLocalName(tdTag)) {
            if (m_inStrayTableContent && !isTableRelated(m_current)) {
                reportError(MisplacedTablePartError, &localName, &m_current->localName());
                // pop out to the nearest enclosing table-related tag.
                while (m_blockStack && !isTableRelated(m_current))
                    popOneBlock();
                return insertNode(n);
            }
        } else if (h->hasLocalName(headTag)) {
            if (!m_current->isDocumentNode() && !m_current->hasTagName(htmlTag)) {
                reportError(MisplacedHeadError);
                return false;
            }
        } else if (h->hasLocalName(metaTag) || h->hasLocalName(linkTag) || h->hasLocalName(baseTag)) {
            bool createdHead = false;
            if (!m_head) {
                createHead();
                createdHead = true;
            }
            if (m_head) {
                if (!createdHead)
                    reportError(MisplacedHeadContentError, &localName, &m_current->localName());
                if (m_head->addChild(n)) {
                    if (!n->attached() && !m_isParsingFragment)
                        n->attach();
                    return true;
                } else
                    return false;
            }
        } else if (h->hasLocalName(htmlTag)) {
            if (!m_current->isDocumentNode() ) {
                if (m_document->documentElement() && m_document->documentElement()->hasTagName(htmlTag)) {
                    reportError(RedundantHTMLBodyError, &localName);
                    // we have another <HTML> element.... apply attributes to existing one
                    // make sure we don't overwrite already existing attributes
                    NamedNodeMap* map = static_cast<Element*>(n)->attributes(true);
                    Element* existingHTML = static_cast<Element*>(m_document->documentElement());
                    NamedNodeMap* bmap = existingHTML->attributes(false);
                    for (unsigned l = 0; map && l < map->length(); ++l) {
                        Attribute* it = map->attributeItem(l);
                        if (!bmap->getAttributeItem(it->name()))
                            existingHTML->setAttribute(it->name(), it->value());
                    }
                }
                return false;
            }
        } else if (h->hasLocalName(titleTag) || h->hasLocalName(styleTag) || h->hasLocalName(scriptTag)) {
            bool createdHead = false;
            if (!m_head) {
                createHead();
                createdHead = true;
            }
            if (m_head) {
                Node* newNode = m_head->addChild(n);
                if (!newNode) {
                    setSkipMode(h->tagQName());
                    return false;
                }
                
                if (!createdHead)
                    reportError(MisplacedHeadContentError, &localName, &m_current->localName());
                
                pushBlock(localName, tagPriority);
                newNode->beginParsingChildren();
                setCurrent(newNode);
                if (!n->attached() && !m_isParsingFragment)
                    n->attach();
                return true;
            }
            if (m_inBody) {
                setSkipMode(h->tagQName());
                return false;
            }
        } else if (h->hasLocalName(bodyTag)) {
            if (m_inBody && m_document->body()) {
                // we have another <BODY> element.... apply attributes to existing one
                // make sure we don't overwrite already existing attributes
                // some sites use <body bgcolor=rightcolor>...<body bgcolor=wrongcolor>
                reportError(RedundantHTMLBodyError, &localName);
                NamedNodeMap* map = static_cast<Element*>(n)->attributes(true);
                Element* existingBody = m_document->body();
                NamedNodeMap* bmap = existingBody->attributes(false);
                for (unsigned l = 0; map && l < map->length(); ++l) {
                    Attribute* it = map->attributeItem(l);
                    if (!bmap->getAttributeItem(it->name()))
                        existingBody->setAttribute(it->name(), it->value());
                }
                return false;
            }
            else if (!m_current->isDocumentNode())
                return false;
        } else if (h->hasLocalName(areaTag)) {
            if (m_currentMapElement) {
                reportError(MisplacedAreaError, &m_current->localName());
                m_currentMapElement->addChild(n);
                if (!n->attached() && !m_isParsingFragment)
                    n->attach();
                handled = true;
                return true;
            }
            return false;
        } else if (h->hasLocalName(colgroupTag) || h->hasLocalName(captionTag)) {
            if (isTableRelated(m_current)) {
                while (m_blockStack && isTablePart(m_current))
                    popOneBlock();
                return insertNode(n);
            }
        }
    } else if (n->isCommentNode() && !m_head)
        return false;

    // 2. Next we examine our currently active element to do some further error handling.
    if (m_current->isHTMLElement()) {
        HTMLElement* h = static_cast<HTMLElement*>(m_current);
        const AtomicString& currentTagName = h->localName();
        if (h->hasLocalName(htmlTag)) {
            HTMLElement* elt = n->isHTMLElement() ? static_cast<HTMLElement*>(n) : 0;
            if (elt && (elt->hasLocalName(scriptTag) || elt->hasLocalName(styleTag) ||
                elt->hasLocalName(metaTag) || elt->hasLocalName(linkTag) ||
                elt->hasLocalName(objectTag) || elt->hasLocalName(embedTag) ||
                elt->hasLocalName(titleTag) || elt->hasLocalName(isindexTag) ||
                elt->hasLocalName(baseTag))) {
                if (!m_head) {
                    m_head = new HTMLHeadElement(headTag, m_document);
                    insertNode(m_head.get());
                    handled = true;
                }
            } else {
                if (n->isTextNode()) {
                    Text* t = static_cast<Text*>(n);
                    if (t->containsOnlyWhitespace())
                        return false;
                }
                if (!m_haveFrameSet) {
                    // Ensure that head exists.
                    // But not for older versions of Mail, where the implicit <head> isn't expected - <rdar://problem/6863795>
                    if (shouldCreateImplicitHead(m_document))
                        createHead();

                    popBlock(headTag);
                    e = new HTMLBodyElement(bodyTag, m_document);
                    startBody();
                    insertNode(e);
                    handled = true;
                } else
                    reportError(MisplacedFramesetContentError, &localName);
            }
        } else if (h->hasLocalName(headTag)) {
            if (n->hasTagName(htmlTag))
                return false;
            else {
                // This means the body starts here...
                if (!m_haveFrameSet) {
                    ASSERT(currentTagName == headTag);
                    popBlock(currentTagName);
                    e = new HTMLBodyElement(bodyTag, m_document);
                    startBody();
                    insertNode(e);
                    handled = true;
                } else
                    reportError(MisplacedFramesetContentError, &localName);
            }
        } else if (h->hasLocalName(addressTag) || h->hasLocalName(fontTag)
                   || h->hasLocalName(styleTag) || h->hasLocalName(titleTag)) {
            reportError(MisplacedContentRetryError, &localName, &currentTagName);
            popBlock(currentTagName);
            handled = true;
        } else if (h->hasLocalName(captionTag)) {
            // Illegal content in a caption. Close the caption and try again.
            reportError(MisplacedCaptionContentError, &localName);
            popBlock(currentTagName);
            if (isTablePart(n))
                return insertNode(n, flat);
        } else if (h->hasLocalName(tableTag) || h->hasLocalName(trTag) || isTableSection(h)) {
            if (n->hasTagName(tableTag)) {
                reportError(MisplacedTableError, &currentTagName);
                if (m_isParsingFragment && !h->hasLocalName(tableTag))
                    // fragment may contain table parts without <table> ancestor, pop them one by one
                    popBlock(h->localName());
                popBlock(localName); // end the table
                handled = true;      // ...and start a new one
            } else {
                ExceptionCode ec = 0;
                Node* node = m_current;
                Node* parent = node->parentNode();
                // A script may have removed the current node's parent from the DOM
                // http://bugs.webkit.org/show_bug.cgi?id=7137
                // FIXME: we should do real recovery here and re-parent with the correct node.
                if (!parent)
                    return false;
                Node* grandparent = parent->parentNode();

                if (n->isTextNode() ||
                    (h->hasLocalName(trTag) &&
                     isTableSection(parent) && grandparent && grandparent->hasTagName(tableTag)) ||
                     ((!n->hasTagName(tdTag) && !n->hasTagName(thTag) &&
                       !n->hasTagName(formTag) && !n->hasTagName(scriptTag)) && isTableSection(node) &&
                     parent->hasTagName(tableTag))) {
                    node = (node->hasTagName(tableTag)) ? node :
                            ((node->hasTagName(trTag)) ? grandparent : parent);
                    // This can happen with fragments
                    if (!node)
                        return false;
                    Node* parent = node->parentNode();
                    if (!parent)
                        return false;
                    parent->insertBefore(n, node, ec);
                    if (!ec) {
                        reportError(StrayTableContentError, &localName, &currentTagName);
                        if (n->isHTMLElement() && tagPriority > 0 && 
                            !flat && static_cast<HTMLElement*>(n)->endTagRequirement() != TagStatusForbidden)
                        {
                            pushBlock(localName, tagPriority);
                            n->beginParsingChildren();
                            setCurrent(n);
                            m_inStrayTableContent++;
                            m_blockStack->strayTableContent = true;
                        }
                        return true;
                    }
                }

                if (!ec) {
                    if (m_current->hasTagName(trTag)) {
                        reportError(TablePartRequiredError, &localName, &tdTag.localName());
                        e = new HTMLTableCellElement(tdTag, m_document);
                    } else if (m_current->hasTagName(tableTag)) {
                        // Don't report an error in this case, since making a <tbody> happens all the time when you have <table><tr>,
                        // and it isn't really a parse error per se.
                        e = new HTMLTableSectionElement(tbodyTag, m_document);
                    } else {
                        reportError(TablePartRequiredError, &localName, &trTag.localName());
                        e = new HTMLTableRowElement(trTag, m_document);
                    }

                    insertNode(e);
                    handled = true;
                }
            }
        } else if (h->hasLocalName(objectTag)) {
            reportError(MisplacedContentRetryError, &localName, &currentTagName);
            popBlock(objectTag);
            handled = true;
        } else if (h->hasLocalName(pTag) || isHeaderTag(currentTagName)) {
            if (!isInline(n)) {
                popBlock(currentTagName);
                handled = true;
            }
        } else if (h->hasLocalName(optionTag) || h->hasLocalName(optgroupTag)) {
            if (localName == optgroupTag) {
                popBlock(currentTagName);
                handled = true;
            } else if (localName == selectTag) {
                // IE treats a nested select as </select>. Let's do the same
                popBlock(localName);
            }
        } else if (h->hasLocalName(selectTag)) {
            if (localName == inputTag || localName == textareaTag) {
                reportError(MisplacedContentRetryError, &localName, &currentTagName);
                popBlock(currentTagName);
                handled = true;
            }
        } else if (h->hasLocalName(colgroupTag)) {
            popBlock(currentTagName);
            handled = true;
        } else if (!h->hasLocalName(bodyTag)) {
            if (isInline(m_current)) {
                popInlineBlocks();
                handled = true;
            }
        }
    } else if (m_current->isDocumentNode()) {
        if (n->isTextNode()) {
            Text* t = static_cast<Text*>(n);
            if (t->containsOnlyWhitespace())
                return false;
        }

        if (!m_document->documentElement()) {
            e = new HTMLHtmlElement(htmlTag, m_document);
            insertNode(e);
            handled = true;
        }
    }

    // 3. If we couldn't handle the error, just return false and attempt to error-correct again.
    if (!handled) {
        reportError(IgnoredContentError, &localName, &m_current->localName());
        return false;
    }
    return insertNode(n);
}

typedef bool (HTMLParser::*CreateErrorCheckFunc)(Token* t, RefPtr<Node>&);
typedef HashMap<AtomicStringImpl*, CreateErrorCheckFunc> FunctionMap;

bool HTMLParser::textCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    result = new Text(m_document, t->text.get());
    return false;
}

bool HTMLParser::commentCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    result = new Comment(m_document, t->text.get());
    return false;
}

bool HTMLParser::headCreateErrorCheck(Token*, RefPtr<Node>& result)
{
    if (!m_head || m_current->localName() == htmlTag) {
        m_head = new HTMLHeadElement(headTag, m_document);
        result = m_head;
    } else
        reportError(MisplacedHeadError);
    return false;
}

bool HTMLParser::bodyCreateErrorCheck(Token*, RefPtr<Node>&)
{
    // body no longer allowed if we have a frameset
    if (m_haveFrameSet)
        return false;
    
    // Ensure that head exists (unless parsing a fragment).
    // But not for older versions of Mail, where the implicit <head> isn't expected - <rdar://problem/6863795>
    if (!m_isParsingFragment && shouldCreateImplicitHead(m_document))
        createHead();
    
    popBlock(headTag);
    startBody();
    return true;
}

bool HTMLParser::framesetCreateErrorCheck(Token*, RefPtr<Node>&)
{
    popBlock(headTag);
    if (m_inBody && !m_haveFrameSet && !m_haveContent) {
        popBlock(bodyTag);
        // ### actually for IE document.body returns the now hidden "body" element
        // we can't implement that behaviour now because it could cause too many
        // regressions and the headaches are not worth the work as long as there is
        // no site actually relying on that detail (Dirk)
        if (m_document->body())
            m_document->body()->setAttribute(styleAttr, "display:none");
        m_inBody = false;
    }
    if ((m_haveContent || m_haveFrameSet) && m_current->localName() == htmlTag)
        return false;
    m_haveFrameSet = true;
    startBody();
    return true;
}

bool HTMLParser::formCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // Only create a new form if we're not already inside one.
    // This is consistent with other browsers' behavior.
    if (!m_currentFormElement) {
        m_currentFormElement = new HTMLFormElement(formTag, m_document);
        result = m_currentFormElement;
        pCloserCreateErrorCheck(t, result);
    }
    return false;
}

bool HTMLParser::isindexCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    RefPtr<Node> n = handleIsindex(t);
    if (!m_inBody)
        m_isindexElement = n.release();
    else {
        t->selfClosingTag = true;
        result = n.release();
    }
    return false;
}

bool HTMLParser::selectCreateErrorCheck(Token*, RefPtr<Node>&)
{
    return true;
}

bool HTMLParser::ddCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    pCloserCreateErrorCheck(t, result);
    popBlock(dtTag);
    popBlock(ddTag);
    return true;
}

bool HTMLParser::dtCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    pCloserCreateErrorCheck(t, result);
    popBlock(ddTag);
    popBlock(dtTag);
    return true;
}

bool HTMLParser::rpCreateErrorCheck(Token*, RefPtr<Node>&)
{
    popBlock(rpTag);
    popBlock(rtTag);
    return true;
}

bool HTMLParser::rtCreateErrorCheck(Token*, RefPtr<Node>&)
{
    popBlock(rpTag);
    popBlock(rtTag);
    return true;
}

bool HTMLParser::nestedCreateErrorCheck(Token* t, RefPtr<Node>&)
{
    popBlock(t->tagName);
    return true;
}

bool HTMLParser::nestedPCloserCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    pCloserCreateErrorCheck(t, result);
    popBlock(t->tagName);
    return true;
}

bool HTMLParser::nestedStyleCreateErrorCheck(Token* t, RefPtr<Node>&)
{
    return allowNestedRedundantTag(t->tagName);
}

bool HTMLParser::tableCellCreateErrorCheck(Token*, RefPtr<Node>&)
{
    popBlock(tdTag);
    popBlock(thTag);
    return true;
}

bool HTMLParser::tableSectionCreateErrorCheck(Token*, RefPtr<Node>&)
{
    popBlock(theadTag);
    popBlock(tbodyTag);
    popBlock(tfootTag);
    return true;
}

bool HTMLParser::noembedCreateErrorCheck(Token*, RefPtr<Node>&)
{
    setSkipMode(noembedTag);
    return true;
}

bool HTMLParser::noframesCreateErrorCheck(Token*, RefPtr<Node>&)
{
    setSkipMode(noframesTag);
    return true;
}

bool HTMLParser::noscriptCreateErrorCheck(Token*, RefPtr<Node>&)
{
    if (!m_isParsingFragment) {
        Settings* settings = m_document->settings();
        if (settings && settings->isJavaScriptEnabled())
            setSkipMode(noscriptTag);
    }
    return true;
}

bool HTMLParser::pCloserCreateErrorCheck(Token*, RefPtr<Node>&)
{
    if (hasPElementInScope())
        popBlock(pTag);
    return true;
}

bool HTMLParser::pCloserStrictCreateErrorCheck(Token*, RefPtr<Node>&)
{
    if (m_document->inCompatMode())
        return true;
    if (hasPElementInScope())
        popBlock(pTag);
    return true;
}

bool HTMLParser::mapCreateErrorCheck(Token*, RefPtr<Node>& result)
{
    m_currentMapElement = new HTMLMapElement(mapTag, m_document);
    result = m_currentMapElement;
    return false;
}

PassRefPtr<Node> HTMLParser::getNode(Token* t)
{
    // Init our error handling table.
    DEFINE_STATIC_LOCAL(FunctionMap, gFunctionMap, ());
    if (gFunctionMap.isEmpty()) {
        gFunctionMap.set(aTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(addressTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(bTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bigTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(blockquoteTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(bodyTag.localName().impl(), &HTMLParser::bodyCreateErrorCheck);
        gFunctionMap.set(buttonTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(centerTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(commentAtom.impl(), &HTMLParser::commentCreateErrorCheck);
        gFunctionMap.set(ddTag.localName().impl(), &HTMLParser::ddCreateErrorCheck);
        gFunctionMap.set(dirTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(divTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(dlTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(dtTag.localName().impl(), &HTMLParser::dtCreateErrorCheck);
        gFunctionMap.set(formTag.localName().impl(), &HTMLParser::formCreateErrorCheck);
        gFunctionMap.set(fieldsetTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(framesetTag.localName().impl(), &HTMLParser::framesetCreateErrorCheck);
        gFunctionMap.set(h1Tag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(h2Tag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(h3Tag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(h4Tag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(h5Tag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(h6Tag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(headTag.localName().impl(), &HTMLParser::headCreateErrorCheck);
        gFunctionMap.set(hrTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(iTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(isindexTag.localName().impl(), &HTMLParser::isindexCreateErrorCheck);
        gFunctionMap.set(liTag.localName().impl(), &HTMLParser::nestedPCloserCreateErrorCheck);
        gFunctionMap.set(listingTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(mapTag.localName().impl(), &HTMLParser::mapCreateErrorCheck);
        gFunctionMap.set(menuTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(nobrTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(noembedTag.localName().impl(), &HTMLParser::noembedCreateErrorCheck);
        gFunctionMap.set(noframesTag.localName().impl(), &HTMLParser::noframesCreateErrorCheck);
#if !ENABLE(XHTMLMP)
        gFunctionMap.set(noscriptTag.localName().impl(), &HTMLParser::noscriptCreateErrorCheck);
#endif
        gFunctionMap.set(olTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(pTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(plaintextTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(preTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
        gFunctionMap.set(rpTag.localName().impl(), &HTMLParser::rpCreateErrorCheck);
        gFunctionMap.set(rtTag.localName().impl(), &HTMLParser::rtCreateErrorCheck);
        gFunctionMap.set(sTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(selectTag.localName().impl(), &HTMLParser::selectCreateErrorCheck);
        gFunctionMap.set(smallTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(strikeTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(tableTag.localName().impl(), &HTMLParser::pCloserStrictCreateErrorCheck);
        gFunctionMap.set(tbodyTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(tdTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(textAtom.impl(), &HTMLParser::textCreateErrorCheck);
        gFunctionMap.set(tfootTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(thTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(theadTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(trTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(ttTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(uTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(ulTag.localName().impl(), &HTMLParser::pCloserCreateErrorCheck);
    }

    bool proceed = true;
    RefPtr<Node> result;
    if (CreateErrorCheckFunc errorCheckFunc = gFunctionMap.get(t->tagName.impl()))
        proceed = (this->*errorCheckFunc)(t, result);
    if (proceed)
        result = HTMLElementFactory::createHTMLElement(QualifiedName(nullAtom, t->tagName, xhtmlNamespaceURI), m_document, m_currentFormElement.get());
    return result.release();
}

bool HTMLParser::allowNestedRedundantTag(const AtomicString& tagName)
{
    // www.liceo.edu.mx is an example of a site that achieves a level of nesting of
    // about 1500 tags, all from a bunch of <b>s.  We will only allow at most 20
    // nested tags of the same type before just ignoring them all together.
    unsigned i = 0;
    for (HTMLStackElem* curr = m_blockStack;
         i < cMaxRedundantTagDepth && curr && curr->tagName == tagName;
         curr = curr->next, i++) { }
    return i != cMaxRedundantTagDepth;
}

void HTMLParser::processCloseTag(Token* t)
{
    // Support for really broken html.
    // we never close the body tag, since some stupid web pages close it before the actual end of the doc.
    // let's rely on the end() call to close things.
    if (t->tagName == htmlTag || t->tagName == bodyTag || t->tagName == commentAtom)
        return;
    
    bool checkForCloseTagErrors = true;
    if (t->tagName == formTag && m_currentFormElement) {
        m_currentFormElement = 0;
        checkForCloseTagErrors = false;
    } else if (t->tagName == mapTag)
        m_currentMapElement = 0;
    else if (t->tagName == pTag)
        checkForCloseTagErrors = false;
        
    HTMLStackElem* oldElem = m_blockStack;
    popBlock(t->tagName, checkForCloseTagErrors);
    if (oldElem == m_blockStack && t->tagName == pTag) {
        // We encountered a stray </p>.  Amazingly Gecko, WinIE, and MacIE all treat
        // this as a valid break, i.e., <p></p>.  So go ahead and make the empty
        // paragraph.
        t->beginTag = true;
        parseToken(t);
        popBlock(t->tagName);
        reportError(StrayParagraphCloseError);
    }
}

bool HTMLParser::isHeaderTag(const AtomicString& tagName)
{
    DEFINE_STATIC_LOCAL(HashSet<AtomicStringImpl*>, headerTags, ());
    if (headerTags.isEmpty()) {
        headerTags.add(h1Tag.localName().impl());
        headerTags.add(h2Tag.localName().impl());
        headerTags.add(h3Tag.localName().impl());
        headerTags.add(h4Tag.localName().impl());
        headerTags.add(h5Tag.localName().impl());
        headerTags.add(h6Tag.localName().impl());
    }
    
    return headerTags.contains(tagName.impl());
}

bool HTMLParser::isInline(Node* node) const
{
    if (node->isTextNode())
        return true;

    if (node->isHTMLElement()) {
        HTMLElement* e = static_cast<HTMLElement*>(node);
        if (e->hasLocalName(aTag) || e->hasLocalName(fontTag) || e->hasLocalName(ttTag) ||
            e->hasLocalName(uTag) || e->hasLocalName(bTag) || e->hasLocalName(iTag) ||
            e->hasLocalName(sTag) || e->hasLocalName(strikeTag) || e->hasLocalName(bigTag) ||
            e->hasLocalName(smallTag) || e->hasLocalName(emTag) || e->hasLocalName(strongTag) ||
            e->hasLocalName(dfnTag) || e->hasLocalName(codeTag) || e->hasLocalName(sampTag) ||
            e->hasLocalName(kbdTag) || e->hasLocalName(varTag) || e->hasLocalName(citeTag) ||
            e->hasLocalName(abbrTag) || e->hasLocalName(acronymTag) || e->hasLocalName(subTag) ||
            e->hasLocalName(supTag) || e->hasLocalName(spanTag) || e->hasLocalName(nobrTag) ||
            e->hasLocalName(noframesTag) || e->hasLocalName(nolayerTag) ||
            e->hasLocalName(noembedTag))
            return true;
#if !ENABLE(XHTMLMP)
        if (e->hasLocalName(noscriptTag) && !m_isParsingFragment) {
            Settings* settings = m_document->settings();
            if (settings && settings->isJavaScriptEnabled())
                return true;
        }
#endif
    }
    
    return false;
}

bool HTMLParser::isResidualStyleTag(const AtomicString& tagName)
{
    DEFINE_STATIC_LOCAL(HashSet<AtomicStringImpl*>, residualStyleTags, ());
    if (residualStyleTags.isEmpty()) {
        residualStyleTags.add(aTag.localName().impl());
        residualStyleTags.add(fontTag.localName().impl());
        residualStyleTags.add(ttTag.localName().impl());
        residualStyleTags.add(uTag.localName().impl());
        residualStyleTags.add(bTag.localName().impl());
        residualStyleTags.add(iTag.localName().impl());
        residualStyleTags.add(sTag.localName().impl());
        residualStyleTags.add(strikeTag.localName().impl());
        residualStyleTags.add(bigTag.localName().impl());
        residualStyleTags.add(smallTag.localName().impl());
        residualStyleTags.add(emTag.localName().impl());
        residualStyleTags.add(strongTag.localName().impl());
        residualStyleTags.add(dfnTag.localName().impl());
        residualStyleTags.add(codeTag.localName().impl());
        residualStyleTags.add(sampTag.localName().impl());
        residualStyleTags.add(kbdTag.localName().impl());
        residualStyleTags.add(varTag.localName().impl());
        residualStyleTags.add(nobrTag.localName().impl());
    }
    
    return residualStyleTags.contains(tagName.impl());
}

bool HTMLParser::isAffectedByResidualStyle(const AtomicString& tagName)
{
    DEFINE_STATIC_LOCAL(HashSet<AtomicStringImpl*>, unaffectedTags, ());
    if (unaffectedTags.isEmpty()) {
        unaffectedTags.add(bodyTag.localName().impl());
        unaffectedTags.add(tableTag.localName().impl());
        unaffectedTags.add(theadTag.localName().impl());
        unaffectedTags.add(tbodyTag.localName().impl());
        unaffectedTags.add(tfootTag.localName().impl());
        unaffectedTags.add(trTag.localName().impl());
        unaffectedTags.add(thTag.localName().impl());
        unaffectedTags.add(tdTag.localName().impl());
        unaffectedTags.add(captionTag.localName().impl());
        unaffectedTags.add(colgroupTag.localName().impl());
        unaffectedTags.add(colTag.localName().impl());
        unaffectedTags.add(optionTag.localName().impl());
        unaffectedTags.add(optgroupTag.localName().impl());
        unaffectedTags.add(selectTag.localName().impl());
        unaffectedTags.add(objectTag.localName().impl());
        unaffectedTags.add(datagridTag.localName().impl());
    }
    
    return !unaffectedTags.contains(tagName.impl());
}

void HTMLParser::handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem* elem)
{
    HTMLStackElem* maxElem = 0;
    bool finished = false;
    bool strayTableContent = elem->strayTableContent;

    m_handlingResidualStyleAcrossBlocks = true;
    while (!finished) {
        // Find the outermost element that crosses over to a higher level. If there exists another higher-level
        // element, we will do another pass, until we have corrected the innermost one.
        ExceptionCode ec = 0;
        HTMLStackElem* curr = m_blockStack;
        HTMLStackElem* prev = 0;
        HTMLStackElem* prevMaxElem = 0;
        maxElem = 0;
        finished = true;
        while (curr && curr != elem) {
            if (curr->level > elem->level) {
                if (!isAffectedByResidualStyle(curr->tagName))
                    return;
                if (maxElem)
                    // We will need another pass.
                    finished = false;
                maxElem = curr;
                prevMaxElem = prev;
            }

            prev = curr;
            curr = curr->next;
        }

        if (!curr || !maxElem)
            return;

        Node* residualElem = prev->node;
        Node* blockElem = prevMaxElem ? prevMaxElem->node : m_current;
        Node* parentElem = elem->node;

        // Check to see if the reparenting that is going to occur is allowed according to the DOM.
        // FIXME: We should either always allow it or perform an additional fixup instead of
        // just bailing here.
        // Example: <p><font><center>blah</font></center></p> isn't doing a fixup right now.
        if (!parentElem->childAllowed(blockElem))
            return;

        m_hasPElementInScope = Unknown;

        if (maxElem->node->parentNode() != elem->node) {
            // Walk the stack and remove any elements that aren't residual style tags.  These
            // are basically just being closed up.  Example:
            // <font><span>Moo<p>Goo</font></p>.
            // In the above example, the <span> doesn't need to be reopened.  It can just close.
            HTMLStackElem* currElem = maxElem->next;
            HTMLStackElem* prevElem = maxElem;
            while (currElem != elem) {
                HTMLStackElem* nextElem = currElem->next;
                if (!isResidualStyleTag(currElem->tagName)) {
                    prevElem->next = nextElem;
                    prevElem->derefNode();
                    prevElem->node = currElem->node;
                    prevElem->didRefNode = currElem->didRefNode;
                    delete currElem;
                }
                else
                    prevElem = currElem;
                currElem = nextElem;
            }

            // We have to reopen residual tags in between maxElem and elem.  An example of this case is:
            // <font><i>Moo<p>Foo</font>.
            // In this case, we need to transform the part before the <p> into:
            // <font><i>Moo</i></font><i>
            // so that the <i> will remain open.  This involves the modification of elements
            // in the block stack.
            // This will also affect how we ultimately reparent the block, since we want it to end up
            // under the reopened residual tags (e.g., the <i> in the above example.)
            RefPtr<Node> prevNode = 0;
            currElem = maxElem;
            while (currElem->node != residualElem) {
                if (isResidualStyleTag(currElem->node->localName())) {
                    // Create a clone of this element.
                    // We call releaseRef to get a raw pointer since we plan to hand over ownership to currElem.
                    Node* currNode = currElem->node->cloneNode(false).releaseRef();
                    reportError(ResidualStyleError, &currNode->localName());
    
                    // Change the stack element's node to point to the clone.
                    // The stack element adopts the reference we obtained above by calling release().
                    currElem->derefNode();
                    currElem->node = currNode;
                    currElem->didRefNode = true;

                    // Attach the previous node as a child of this new node.
                    if (prevNode)
                        currNode->appendChild(prevNode, ec);
                    else // The new parent for the block element is going to be the innermost clone.
                        parentElem = currNode;  // FIXME: We shifted parentElem to be a residual inline.  We never checked to see if blockElem could be legally placed inside the inline though.

                    prevNode = currNode;
                }

                currElem = currElem->next;
            }

            // Now append the chain of new residual style elements if one exists.
            if (prevNode)
                elem->node->appendChild(prevNode, ec);  // FIXME: This append can result in weird stuff happening, like an inline chain being put into a table section.
        }

        // Check if the block is still in the tree. If it isn't, then we don't
        // want to remove it from its parent (that would crash) or insert it into
        // a new parent later. See http://bugs.webkit.org/show_bug.cgi?id=6778
        bool isBlockStillInTree = blockElem->parentNode();

        // We need to make a clone of |residualElem| and place it just inside |blockElem|.
        // All content of |blockElem| is reparented to be under this clone.  We then
        // reparent |blockElem| using real DOM calls so that attachment/detachment will
        // be performed to fix up the rendering tree.
        // So for this example: <b>...<p>Foo</b>Goo</p>
        // The end result will be: <b>...</b><p><b>Foo</b>Goo</p>
        //
        // Step 1: Remove |blockElem| from its parent, doing a batch detach of all the kids.
        if (isBlockStillInTree)
            blockElem->parentNode()->removeChild(blockElem, ec);

        Node* newNodePtr = 0;
        if (blockElem->firstChild()) {
            // Step 2: Clone |residualElem|.
            RefPtr<Node> newNode = residualElem->cloneNode(false); // Shallow clone. We don't pick up the same kids.
            newNodePtr = newNode.get();
            reportError(ResidualStyleError, &newNode->localName());

            // Step 3: Place |blockElem|'s children under |newNode|.  Remove all of the children of |blockElem|
            // before we've put |newElem| into the document.  That way we'll only do one attachment of all
            // the new content (instead of a bunch of individual attachments).
            Node* currNode = blockElem->firstChild();
            while (currNode) {
                Node* nextNode = currNode->nextSibling();
                newNode->appendChild(currNode, ec);
                currNode = nextNode;
            }

            // Step 4: Place |newNode| under |blockElem|.  |blockElem| is still out of the document, so no
            // attachment can occur yet.
            blockElem->appendChild(newNode.release(), ec);
        } else
            finished = true;

        // Step 5: Reparent |blockElem|.  Now the full attachment of the fixed up tree takes place.
        if (isBlockStillInTree)
            parentElem->appendChild(blockElem, ec);

        // Step 6: Pull |elem| out of the stack, since it is no longer enclosing us.  Also update
        // the node associated with the previous stack element so that when it gets popped,
        // it doesn't make the residual element the next current node.
        HTMLStackElem* currElem = maxElem;
        HTMLStackElem* prevElem = 0;
        while (currElem != elem) {
            prevElem = currElem;
            currElem = currElem->next;
        }
        prevElem->next = elem->next;
        prevElem->derefNode();
        prevElem->node = elem->node;
        prevElem->didRefNode = elem->didRefNode;
        if (!finished) {
            // Repurpose |elem| to represent |newNode| and insert it at the appropriate position
            // in the stack. We do not do this for the innermost block, because in that case the new
            // node is effectively no longer open.
            elem->next = maxElem;
            elem->node = prevMaxElem->node;
            elem->didRefNode = prevMaxElem->didRefNode;
            elem->strayTableContent = false;
            prevMaxElem->next = elem;
            ASSERT(newNodePtr);
            prevMaxElem->node = newNodePtr;
            prevMaxElem->didRefNode = false;
        } else
            delete elem;
    }

    // FIXME: If we ever make a case like this work:
    // <table><b><i><form></b></form></i></table>
    // Then this check will be too simplistic.  Right now the <i><form> chain will end up inside the <tbody>, which is pretty crazy.
    if (strayTableContent)
        m_inStrayTableContent--;

    // Step 7: Reopen intermediate inlines, e.g., <b><p><i>Foo</b>Goo</p>.
    // In the above example, Goo should stay italic.
    // We cap the number of tags we're willing to reopen based off cResidualStyleMaxDepth.
    
    HTMLStackElem* curr = m_blockStack;
    HTMLStackElem* residualStyleStack = 0;
    unsigned stackDepth = 1;
    unsigned redundantStyleCount = 0;
    while (curr && curr != maxElem) {
        // We will actually schedule this tag for reopening
        // after we complete the close of this entire block.
        if (isResidualStyleTag(curr->tagName) && stackDepth++ < cResidualStyleMaxDepth) {
            // We've overloaded the use of stack elements and are just reusing the
            // struct with a slightly different meaning to the variables.  Instead of chaining
            // from innermost to outermost, we build up a list of all the tags we need to reopen
            // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
            // to the outermost tag we need to reopen.
            // We also set curr->node to be the actual element that corresponds to the ID stored in
            // curr->id rather than the node that you should pop to when the element gets pulled off
            // the stack.
            if (residualStyleStack && curr->tagName == residualStyleStack->tagName && curr->node->attributes()->mapsEquivalent(residualStyleStack->node->attributes()))
                redundantStyleCount++;
            else
                redundantStyleCount = 0;

            if (redundantStyleCount < cMaxRedundantTagDepth)
                moveOneBlockToStack(residualStyleStack);
            else
                popOneBlock();
        } else
            popOneBlock();

        curr = m_blockStack;
    }

    reopenResidualStyleTags(residualStyleStack, 0); // Stray table content can't be an issue here, since some element above will always become the root of new stray table content.

    m_handlingResidualStyleAcrossBlocks = false;
}

void HTMLParser::reopenResidualStyleTags(HTMLStackElem* elem, Node* malformedTableParent)
{
    // Loop for each tag that needs to be reopened.
    while (elem) {
        // Create a shallow clone of the DOM node for this element.
        RefPtr<Node> newNode = elem->node->cloneNode(false); 
        reportError(ResidualStyleError, &newNode->localName());

        // Append the new node. In the malformed table case, we need to insert before the table,
        // which will be the last child.
        ExceptionCode ec = 0;
        if (malformedTableParent)
            malformedTableParent->insertBefore(newNode, malformedTableParent->lastChild(), ec);
        else
            m_current->appendChild(newNode, ec);
        // FIXME: Is it really OK to ignore the exceptions here?

        // Now push a new stack element for this node we just created.
        pushBlock(elem->tagName, elem->level);
        newNode->beginParsingChildren();

        // Set our strayTableContent boolean if needed, so that the reopened tag also knows
        // that it is inside a malformed table.
        m_blockStack->strayTableContent = malformedTableParent != 0;
        if (m_blockStack->strayTableContent)
            m_inStrayTableContent++;

        // Clear our malformed table parent variable.
        malformedTableParent = 0;

        // Update |current| manually to point to the new node.
        setCurrent(newNode.get());
        
        // Advance to the next tag that needs to be reopened.
        HTMLStackElem* next = elem->next;
        elem->derefNode();
        delete elem;
        elem = next;
    }
}

void HTMLParser::pushBlock(const AtomicString& tagName, int level)
{
    m_blockStack = new HTMLStackElem(tagName, level, m_current, m_didRefCurrent, m_blockStack);
    if (level >= minBlockLevelTagPriority)
        m_blocksInStack++;
    m_didRefCurrent = false;
    if (tagName == pTag)
        m_hasPElementInScope = InScope;
    else if (isScopingTag(tagName))
        m_hasPElementInScope = NotInScope;
}

void HTMLParser::popBlock(const AtomicString& tagName, bool reportErrors)
{
    HTMLStackElem* elem = m_blockStack;

    if (m_parserQuirks && elem && !m_parserQuirks->shouldPopBlock(elem->tagName, tagName))
        return;

    int maxLevel = 0;

    while (elem && (elem->tagName != tagName)) {
        if (maxLevel < elem->level)
            maxLevel = elem->level;
        elem = elem->next;
    }

    if (!elem) {
        if (reportErrors)
            reportError(StrayCloseTagError, &tagName, 0, true);
        return;
    }

    if (maxLevel > elem->level) {
        // We didn't match because the tag is in a different scope, e.g.,
        // <b><p>Foo</b>.  Try to correct the problem.
        if (!isResidualStyleTag(tagName))
            return;
        return handleResidualStyleCloseTagAcrossBlocks(elem);
    }

    bool isAffectedByStyle = isAffectedByResidualStyle(elem->tagName);
    HTMLStackElem* residualStyleStack = 0;
    Node* malformedTableParent = 0;
    
    elem = m_blockStack;
    unsigned stackDepth = 1;
    unsigned redundantStyleCount = 0;
    while (elem) {
        if (elem->tagName == tagName) {
            int strayTable = m_inStrayTableContent;
            popOneBlock();
            elem = 0;

            // This element was the root of some malformed content just inside an implicit or
            // explicit <tbody> or <tr>.
            // If we end up needing to reopen residual style tags, the root of the reopened chain
            // must also know that it is the root of malformed content inside a <tbody>/<tr>.
            if (strayTable && (m_inStrayTableContent < strayTable) && residualStyleStack) {
                Node* curr = m_current;
                while (curr && !curr->hasTagName(tableTag))
                    curr = curr->parentNode();
                malformedTableParent = curr ? curr->parentNode() : 0;
            }
        }
        else {
            if (m_currentFormElement && elem->tagName == formTag)
                // A <form> is being closed prematurely (and this is
                // malformed HTML).  Set an attribute on the form to clear out its
                // bottom margin.
                m_currentFormElement->setMalformed(true);

            // Schedule this tag for reopening
            // after we complete the close of this entire block.
            if (isAffectedByStyle && isResidualStyleTag(elem->tagName) && stackDepth++ < cResidualStyleMaxDepth) {
                // We've overloaded the use of stack elements and are just reusing the
                // struct with a slightly different meaning to the variables.  Instead of chaining
                // from innermost to outermost, we build up a list of all the tags we need to reopen
                // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
                // to the outermost tag we need to reopen.
                // We also set elem->node to be the actual element that corresponds to the ID stored in
                // elem->id rather than the node that you should pop to when the element gets pulled off
                // the stack.
                if (residualStyleStack && elem->tagName == residualStyleStack->tagName && elem->node->attributes()->mapsEquivalent(residualStyleStack->node->attributes()))
                    redundantStyleCount++;
                else
                    redundantStyleCount = 0;

                if (redundantStyleCount < cMaxRedundantTagDepth)
                    moveOneBlockToStack(residualStyleStack);
                else
                    popOneBlock();
            } else
                popOneBlock();
            elem = m_blockStack;
        }
    }

    reopenResidualStyleTags(residualStyleStack, malformedTableParent);
}

inline HTMLStackElem* HTMLParser::popOneBlockCommon()
{
    HTMLStackElem* elem = m_blockStack;

    // Form elements restore their state during the parsing process.
    // Also, a few elements (<applet>, <object>) need to know when all child elements (<param>s) are available.
    if (m_current && elem->node != m_current)
        m_current->finishParsingChildren();

    if (m_blockStack->level >= minBlockLevelTagPriority) {
        ASSERT(m_blocksInStack > 0);
        m_blocksInStack--;
    }
    m_blockStack = elem->next;
    m_current = elem->node;
    m_didRefCurrent = elem->didRefNode;

    if (elem->strayTableContent)
        m_inStrayTableContent--;

    if (elem->tagName == pTag)
        m_hasPElementInScope = NotInScope;
    else if (isScopingTag(elem->tagName))
        m_hasPElementInScope = Unknown;

    return elem;
}

void HTMLParser::popOneBlock()
{
    // Store the current node before popOneBlockCommon overwrites it.
    Node* lastCurrent = m_current;
    bool didRefLastCurrent = m_didRefCurrent;

    delete popOneBlockCommon();

    if (didRefLastCurrent)
        lastCurrent->deref();
}

void HTMLParser::moveOneBlockToStack(HTMLStackElem*& head)
{
    // We'll be using the stack element we're popping, but for the current node.
    // See the two callers for details.

    // Store the current node before popOneBlockCommon overwrites it.
    Node* lastCurrent = m_current;
    bool didRefLastCurrent = m_didRefCurrent;

    // Pop the block, but don't deref the current node as popOneBlock does because
    // we'll be using the pointer in the new stack element.
    HTMLStackElem* elem = popOneBlockCommon();

    // Transfer the current node into the stack element.
    // No need to deref the old elem->node because popOneBlockCommon transferred
    // it into the m_current/m_didRefCurrent fields.
    elem->node = lastCurrent;
    elem->didRefNode = didRefLastCurrent;
    elem->next = head;
    head = elem;
}

void HTMLParser::checkIfHasPElementInScope()
{
    m_hasPElementInScope = NotInScope;
    HTMLStackElem* elem = m_blockStack;
    while (elem) {
        const AtomicString& tagName = elem->tagName;
        if (tagName == pTag) {
            m_hasPElementInScope = InScope;
            return;
        } else if (isScopingTag(tagName))
            return;
        elem = elem->next;
    }
}

void HTMLParser::popInlineBlocks()
{
    while (m_blockStack && isInline(m_current))
        popOneBlock();
}

void HTMLParser::freeBlock()
{
    while (m_blockStack)
        popOneBlock();
    ASSERT(!m_blocksInStack);
}

void HTMLParser::createHead()
{
    if (m_head)
        return;

    if (!m_document->documentElement()) {
        insertNode(new HTMLHtmlElement(htmlTag, m_document));
        ASSERT(m_document->documentElement());
    }

    m_head = new HTMLHeadElement(headTag, m_document);
    HTMLElement* body = m_document->body();
    ExceptionCode ec = 0;
    m_document->documentElement()->insertBefore(m_head.get(), body, ec);
    if (ec)
        m_head = 0;
        
    // If the body does not exist yet, then the <head> should be pushed as the current block.
    if (m_head && !body) {
        pushBlock(m_head->localName(), m_head->tagPriority());
        setCurrent(m_head.get());
    }
}

PassRefPtr<Node> HTMLParser::handleIsindex(Token* t)
{
    RefPtr<Node> n = new HTMLDivElement(divTag, m_document);

    NamedMappedAttrMap* attrs = t->attrs.get();

    RefPtr<HTMLIsIndexElement> isIndex = new HTMLIsIndexElement(isindexTag, m_document, m_currentFormElement.get());
    isIndex->setAttributeMap(attrs);
    isIndex->setAttribute(typeAttr, "khtml_isindex");

    String text = searchableIndexIntroduction();
    if (attrs) {
        if (Attribute* a = attrs->getAttributeItem(promptAttr))
            text = a->value().string() + " ";
        t->attrs = 0;
    }

    n->addChild(new HTMLHRElement(hrTag, m_document));
    n->addChild(new Text(m_document, text));
    n->addChild(isIndex.release());
    n->addChild(new HTMLHRElement(hrTag, m_document));

    return n.release();
}

void HTMLParser::startBody()
{
    if (m_inBody)
        return;

    m_inBody = true;

    if (m_isindexElement) {
        insertNode(m_isindexElement.get(), true /* don't descend into this node */);
        m_isindexElement = 0;
    }
}

void HTMLParser::finished()
{
    // In the case of a completely empty document, here's the place to create the HTML element.
    if (m_current && m_current->isDocumentNode() && !m_document->documentElement())
        insertNode(new HTMLHtmlElement(htmlTag, m_document));

    // This ensures that "current" is not left pointing to a node when the document is destroyed.
    freeBlock();
    setCurrent(0);

    // Warning, this may delete the tokenizer and parser, so don't try to do anything else after this.
    if (!m_isParsingFragment)
        m_document->finishedParsing();
}

void HTMLParser::reportErrorToConsole(HTMLParserErrorCode errorCode, const AtomicString* tagName1, const AtomicString* tagName2, bool closeTags)
{    
    Frame* frame = m_document->frame();
    if (!frame)
        return;
    
    HTMLTokenizer* htmlTokenizer = static_cast<HTMLTokenizer*>(m_document->tokenizer());
    int lineNumber = htmlTokenizer->lineNumber() + 1;

    AtomicString tag1;
    AtomicString tag2;
    if (tagName1) {
        if (*tagName1 == "#text")
            tag1 = "Text";
        else if (*tagName1 == "#comment")
            tag1 = "<!-- comment -->";
        else
            tag1 = (closeTags ? "</" : "<") + *tagName1 + ">";
    }
    if (tagName2) {
        if (*tagName2 == "#text")
            tag2 = "Text";
        else if (*tagName2 == "#comment")
            tag2 = "<!-- comment -->";
        else
            tag2 = (closeTags ? "</" : "<") + *tagName2 + ">";
    }
        
    const char* errorMsg = htmlParserErrorMessageTemplate(errorCode);
    if (!errorMsg)
        return;
        
    String message;
    if (htmlTokenizer->processingContentWrittenByScript())
        message += htmlParserDocumentWriteMessage();
    message += errorMsg;
    message.replace("%tag1", tag1);
    message.replace("%tag2", tag2);

    frame->domWindow()->console()->addMessage(HTMLMessageSource,
        isWarning(errorCode) ? WarningMessageLevel : ErrorMessageLevel,
        message, lineNumber, m_document->url().string());
}

#ifdef BUILDING_ON_LEOPARD
bool shouldCreateImplicitHead(Document* document)
{
    ASSERT(document);
    
    Settings* settings = document->page() ? document->page()->settings() : 0;
    return settings ? !settings->needsLeopardMailQuirks() : true;
}
#elif defined(BUILDING_ON_TIGER)
bool shouldCreateImplicitHead(Document* document)
{
    ASSERT(document);
    
    Settings* settings = document->page() ? document->page()->settings() : 0;
    return settings ? !settings->needsTigerMailQuirks() : true;
}
#endif

}
