/*
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1999,2001 Lars Knoll (knoll@kde.org)
              (C) 2000,2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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
#include "Comment.h"
#include "DocumentFragment.h"
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
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTokenizer.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "Settings.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

static const unsigned cMaxRedundantTagDepth = 20;
static const unsigned cResidualStyleMaxDepth = 200;

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
    : document(doc)
    , current(doc)
    , didRefCurrent(false)
    , blockStack(0)
    , head(0)
    , inBody(false)
    , haveContent(false)
    , haveFrameSet(false)
    , m_isParsingFragment(false)
    , m_reportErrors(reportErrors)
    , m_handlingResidualStyleAcrossBlocks(false)
    , inStrayTableContent(0)
{
}

HTMLParser::HTMLParser(DocumentFragment* frag)
    : document(frag->document())
    , current(frag)
    , didRefCurrent(true)
    , blockStack(0)
    , head(0)
    , inBody(true)
    , haveContent(false)
    , haveFrameSet(false)
    , m_isParsingFragment(true)
    , m_reportErrors(false)
    , m_handlingResidualStyleAcrossBlocks(false)
    , inStrayTableContent(0)
{
    if (frag)
        frag->ref();
}

HTMLParser::~HTMLParser()
{
    freeBlock();
    if (didRefCurrent) 
        current->deref(); 
}

void HTMLParser::reset()
{
    ASSERT(!m_isParsingFragment);

    setCurrent(document);

    freeBlock();

    inBody = false;
    haveFrameSet = false;
    haveContent = false;
    inStrayTableContent = 0;

    m_currentFormElement = 0;
    m_currentMapElement = 0;
    head = 0;
    m_isindexElement = 0;

    m_skipModeTag = nullAtom;
}

void HTMLParser::setCurrent(Node* newCurrent) 
{
    bool didRefNewCurrent = newCurrent && newCurrent != document;
    if (didRefNewCurrent) 
        newCurrent->ref(); 
    if (didRefCurrent) 
        current->deref(); 
    current = newCurrent;
    didRefCurrent = didRefNewCurrent;
}

PassRefPtr<Node> HTMLParser::parseToken(Token* t)
{
    if (!m_skipModeTag.isNull()) {
        if (!t->beginTag && t->tagName == m_skipModeTag)
            // Found the end tag for the current skip mode, so we're done skipping.
            m_skipModeTag = nullAtom;
        else if (current->localName() == t->tagName)
            // Do not skip </iframe>.
            // FIXME: What does that comment mean? How can it be right to parse a token without clearing m_skipModeTag?
            ;
        else
            return 0;
    }

    // Apparently some sites use </br> instead of <br>. Be compatible with IE and Firefox and treat this like <br>.
    if (t->isCloseTag(brTag) && document->inCompatMode()) {
        reportError(MalformedBRError);
        t->beginTag = true;
    }

    if (!t->beginTag) {
        processCloseTag(t);
        return 0;
    }

    // ignore spaces, if we're not inside a paragraph or other inline code
    if (t->tagName == textAtom && t->text) {
        if (inBody && !skipMode() && current->localName() != styleTag && current->localName() != titleTag && 
            current->localName() != scriptTag && !t->text->containsOnlyWhitespace()) 
            haveContent = true;
        
        RefPtr<Node> n;
        String text = t->text.get();
        unsigned charsLeft = text.length();
        while (charsLeft) {
            // split large blocks of text to nodes of manageable size
            n = Text::createWithLengthLimit(document, text, charsLeft);
            if (!insertNode(n.get(), t->flat))
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

    if (!insertNode(n.get(), t->flat)) {
        // we couldn't insert the node

        if (n->isElementNode()) {
            Element* e = static_cast<Element*>(n.get());
            e->setAttributeMap(0);
        }

        if (m_currentMapElement == n)
            m_currentMapElement = 0;

        if (m_currentFormElement == n)
            m_currentFormElement = 0;

        if (head == n)
            head = 0;

        return 0;
    }
    return n;
}

static bool isTableSection(Node* n)
{
    return n->hasTagName(tbodyTag) || n->hasTagName(tfootTag) || n->hasTagName(theadTag);
}

static bool isTablePart(Node* n)
{
    return n->hasTagName(trTag) || n->hasTagName(tdTag) || n->hasTagName(thTag) ||
           isTableSection(n);
}

static bool isTableRelated(Node* n)
{
    return n->hasTagName(tableTag) || isTablePart(n);
}

bool HTMLParser::insertNode(Node* n, bool flat)
{
    RefPtr<Node> protectNode(n);

    const AtomicString& localName = n->localName();
    int tagPriority = n->isHTMLElement() ? static_cast<HTMLElement*>(n)->tagPriority() : 0;
    
    // <table> is never allowed inside stray table content.  Always pop out of the stray table content
    // and close up the first table, and then start the second table as a sibling.
    if (inStrayTableContent && localName == tableTag)
        popBlock(tableTag);
    
    // let's be stupid and just try to insert it.
    // this should work if the document is well-formed
    Node* newNode = current->addChild(n);
    if (!newNode)
        return handleError(n, flat, localName, tagPriority); // Try to handle the error.

    // don't push elements without end tags (e.g., <img>) on the stack
    bool parentAttached = current->attached();
    if (tagPriority > 0 && !flat) {
        if (newNode == current) {
            // This case should only be hit when a demoted <form> is placed inside a table.
            ASSERT(localName == formTag);
            reportError(FormInsideTablePartError, &current->localName());
        } else {
            // The pushBlock function transfers ownership of current to the block stack
            // so we're guaranteed that didRefCurrent is false. The code below is an
            // optimized version of setCurrent that takes advantage of that fact and also
            // assumes that newNode is neither 0 nor a pointer to the document.
            pushBlock(localName, tagPriority);
            ASSERT(!didRefCurrent);
            newNode->ref(); 
            current = newNode;
            didRefCurrent = true;
        }
        if (parentAttached && !n->attached() && !m_isParsingFragment)
            n->attach();
    } else {
        if (parentAttached && !n->attached() && !m_isParsingFragment)
            n->attach();
        n->finishParsingChildren();
    }

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
            if (inStrayTableContent && !isTableRelated(current)) {
                reportError(MisplacedTablePartError, &localName, &current->localName());
                // pop out to the nearest enclosing table-related tag.
                while (blockStack && !isTableRelated(current))
                    popOneBlock();
                return insertNode(n);
            }
        } else if (h->hasLocalName(headTag)) {
            if (!current->isDocumentNode() && !current->hasTagName(htmlTag)) {
                reportError(MisplacedHeadError);
                return false;
            }
        } else if (h->hasLocalName(metaTag) || h->hasLocalName(linkTag) || h->hasLocalName(baseTag)) {
            bool createdHead = false;
            if (!head) {
                createHead();
                createdHead = true;
            }
            if (head) {
                if (!createdHead)
                    reportError(MisplacedHeadContentError, &localName, &current->localName());
                if (head->addChild(n)) {
                    if (!n->attached() && !m_isParsingFragment)
                        n->attach();
                    return true;
                } else
                    return false;
            }
        } else if (h->hasLocalName(htmlTag)) {
            if (!current->isDocumentNode() ) {
                if (document->documentElement()->hasTagName(htmlTag)) {
                    reportError(RedundantHTMLBodyError, &localName);
                    // we have another <HTML> element.... apply attributes to existing one
                    // make sure we don't overwrite already existing attributes
                    NamedAttrMap* map = static_cast<Element*>(n)->attributes(true);
                    Element* existingHTML = static_cast<Element*>(document->documentElement());
                    NamedAttrMap* bmap = existingHTML->attributes(false);
                    for (unsigned l = 0; map && l < map->length(); ++l) {
                        Attribute* it = map->attributeItem(l);
                        if (!bmap->getAttributeItem(it->name()))
                            existingHTML->setAttribute(it->name(), it->value());
                    }
                }
                return false;
            }
        } else if (h->hasLocalName(titleTag) || h->hasLocalName(styleTag)) {
            bool createdHead = false;
            if (!head) {
                createHead();
                createdHead = true;
            }
            if (head) {
                Node* newNode = head->addChild(n);
                if (!newNode) {
                    setSkipMode(h->tagQName());
                    return false;
                }
                
                if (!createdHead)
                    reportError(MisplacedHeadContentError, &localName, &current->localName());
                
                pushBlock(localName, tagPriority);
                setCurrent(newNode);
                if (!n->attached() && !m_isParsingFragment)
                    n->attach();
                return true;
            }
            if (inBody) {
                setSkipMode(h->tagQName());
                return false;
            }
        } else if (h->hasLocalName(bodyTag)) {
            if (inBody && document->body()) {
                // we have another <BODY> element.... apply attributes to existing one
                // make sure we don't overwrite already existing attributes
                // some sites use <body bgcolor=rightcolor>...<body bgcolor=wrongcolor>
                reportError(RedundantHTMLBodyError, &localName);
                NamedAttrMap* map = static_cast<Element*>(n)->attributes(true);
                Element* existingBody = document->body();
                NamedAttrMap* bmap = existingBody->attributes(false);
                for (unsigned l = 0; map && l < map->length(); ++l) {
                    Attribute* it = map->attributeItem(l);
                    if (!bmap->getAttributeItem(it->name()))
                        existingBody->setAttribute(it->name(), it->value());
                }
                return false;
            }
            else if (!current->isDocumentNode())
                return false;
        } else if (h->hasLocalName(areaTag)) {
            if (m_currentMapElement) {
                reportError(MisplacedAreaError, &current->localName());
                m_currentMapElement->addChild(n);
                if (!n->attached() && !m_isParsingFragment)
                    n->attach();
                handled = true;
                return true;
            }
            return false;
        } else if (h->hasLocalName(colgroupTag) || h->hasLocalName(captionTag)) {
            if (isTableRelated(current)) {
                while (blockStack && isTablePart(current))
                    popOneBlock();
                return insertNode(n);
            }
        }
    } else if (n->isCommentNode() && !head)
        return false;

    // 2. Next we examine our currently active element to do some further error handling.
    if (current->isHTMLElement()) {
        HTMLElement* h = static_cast<HTMLElement*>(current);
        const AtomicString& currentTagName = current->localName();
        if (h->hasLocalName(htmlTag)) {
            HTMLElement* elt = n->isHTMLElement() ? static_cast<HTMLElement*>(n) : 0;
            if (elt && (elt->hasLocalName(scriptTag) || elt->hasLocalName(styleTag) ||
                elt->hasLocalName(metaTag) || elt->hasLocalName(linkTag) ||
                elt->hasLocalName(objectTag) || elt->hasLocalName(embedTag) ||
                elt->hasLocalName(titleTag) || elt->hasLocalName(isindexTag) ||
                elt->hasLocalName(baseTag))) {
                if (!head) {
                    head = new HTMLHeadElement(document);
                    e = head;
                    insertNode(e);
                    handled = true;
                }
            } else {
                if (n->isTextNode()) {
                    Text* t = static_cast<Text*>(n);
                    if (t->containsOnlyWhitespace())
                        return false;
                }
                if (!haveFrameSet) {
                    e = new HTMLBodyElement(document);
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
                if (!haveFrameSet) {
                    popBlock(currentTagName);
                    e = new HTMLBodyElement(document);
                    startBody();
                    insertNode(e);
                    handled = true;
                } else
                    reportError(MisplacedFramesetContentError, &localName);
            }
        } else if (h->hasLocalName(addressTag) || h->hasLocalName(dlTag) || h->hasLocalName(dtTag)
                   || h->hasLocalName(fontTag) || h->hasLocalName(styleTag) || h->hasLocalName(titleTag)) {
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
                Node* node = current;
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
                            setCurrent(n);
                            inStrayTableContent++;
                            blockStack->strayTableContent = true;
                        }
                        return true;
                    }
                }

                if (!ec) {
                    if (current->hasTagName(trTag)) {
                        reportError(TablePartRequiredError, &localName, &tdTag.localName());
                        e = new HTMLTableCellElement(tdTag, document);
                    } else if (current->hasTagName(tableTag)) {
                        // Don't report an error in this case, since making a <tbody> happens all the time when you have <table><tr>,
                        // and it isn't really a parse error per se.
                        e = new HTMLTableSectionElement(tbodyTag, document); 
                    } else {
                        reportError(TablePartRequiredError, &localName, &trTag.localName());
                        e = new HTMLTableRowElement(document);
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
        } else if (h->hasLocalName(colgroupTag)) {
            popBlock(currentTagName);
            handled = true;
        } else if (!h->hasLocalName(bodyTag)) {
            if (isInline(current)) {
                popInlineBlocks();
                handled = true;
            }
        }
    } else if (current->isDocumentNode()) {
        if (n->isTextNode()) {
            Text* t = static_cast<Text*>(n);
            if (t->containsOnlyWhitespace())
                return false;
        }

        if (!document->documentElement()) {
            e = new HTMLHtmlElement(document);
            insertNode(e);
            handled = true;
        }
    }

    // 3. If we couldn't handle the error, just return false and attempt to error-correct again.
    if (!handled) {
        reportError(IgnoredContentError, &localName, &current->localName());
        return false;
    }
    return insertNode(n);
}

typedef bool (HTMLParser::*CreateErrorCheckFunc)(Token* t, RefPtr<Node>&);
typedef HashMap<AtomicStringImpl*, CreateErrorCheckFunc> FunctionMap;

bool HTMLParser::textCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    result = new Text(document, t->text.get());
    return false;
}

bool HTMLParser::commentCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    result = new Comment(document, t->text.get());
    return false;
}

bool HTMLParser::headCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    if (!head || current->localName() == htmlTag) {
        head = new HTMLHeadElement(document);
        result = head;
    } else
        reportError(MisplacedHeadError);
    return false;
}

bool HTMLParser::bodyCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // body no longer allowed if we have a frameset
    if (haveFrameSet)
        return false;
    popBlock(headTag);
    startBody();
    return true;
}

bool HTMLParser::framesetCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(headTag);
    if (inBody && !haveFrameSet && !haveContent) {
        popBlock(bodyTag);
        // ### actually for IE document.body returns the now hidden "body" element
        // we can't implement that behaviour now because it could cause too many
        // regressions and the headaches are not worth the work as long as there is
        // no site actually relying on that detail (Dirk)
        if (document->body())
            document->body()->setAttribute(styleAttr, "display:none");
        inBody = false;
    }
    if ((haveContent || haveFrameSet) && current->localName() == htmlTag)
        return false;
    haveFrameSet = true;
    startBody();
    return true;
}

bool HTMLParser::iframeCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // a bit of a special case, since the frame is inlined
    setSkipMode(iframeTag);
    return true;
}

bool HTMLParser::formCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    // Only create a new form if we're not already inside one.
    // This is consistent with other browsers' behavior.
    if (!m_currentFormElement) {
        m_currentFormElement = new HTMLFormElement(document);
        result = m_currentFormElement;
    }
    return false;
}

bool HTMLParser::isindexCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    RefPtr<Node> n = handleIsindex(t);
    if (!inBody) {
        m_isindexElement = n.release();
    } else {
        t->flat = true;
        result = n.release();
    }
    return false;
}

bool HTMLParser::selectCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    return true;
}

bool HTMLParser::ddCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(dtTag);
    popBlock(ddTag);
    return true;
}

bool HTMLParser::dtCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(ddTag);
    popBlock(dtTag);
    return true;
}

bool HTMLParser::nestedCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(t->tagName);
    return true;
}

bool HTMLParser::nestedStyleCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    return allowNestedRedundantTag(t->tagName);
}

bool HTMLParser::tableCellCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(tdTag);
    popBlock(thTag);
    return true;
}

bool HTMLParser::tableSectionCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    popBlock(theadTag);
    popBlock(tbodyTag);
    popBlock(tfootTag);
    return true;
}

bool HTMLParser::noembedCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    setSkipMode(noembedTag);
    return true;
}

bool HTMLParser::noframesCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    setSkipMode(noframesTag);
    return true;
}

bool HTMLParser::noscriptCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    if (!m_isParsingFragment) {
        Settings* settings = document->settings();
        if (settings && settings->isJavaScriptEnabled())
            setSkipMode(noscriptTag);
    }
    return true;
}

bool HTMLParser::mapCreateErrorCheck(Token* t, RefPtr<Node>& result)
{
    m_currentMapElement = new HTMLMapElement(document);
    result = m_currentMapElement;
    return false;
}

PassRefPtr<Node> HTMLParser::getNode(Token* t)
{
    // Init our error handling table.
    static FunctionMap gFunctionMap;
    if (gFunctionMap.isEmpty()) {
        gFunctionMap.set(aTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(bTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bigTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bodyTag.localName().impl(), &HTMLParser::bodyCreateErrorCheck);
        gFunctionMap.set(buttonTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(commentAtom.impl(), &HTMLParser::commentCreateErrorCheck);
        gFunctionMap.set(ddTag.localName().impl(), &HTMLParser::ddCreateErrorCheck);
        gFunctionMap.set(dtTag.localName().impl(), &HTMLParser::dtCreateErrorCheck);
        gFunctionMap.set(formTag.localName().impl(), &HTMLParser::formCreateErrorCheck);
        gFunctionMap.set(framesetTag.localName().impl(), &HTMLParser::framesetCreateErrorCheck);
        gFunctionMap.set(headTag.localName().impl(), &HTMLParser::headCreateErrorCheck);
        gFunctionMap.set(iTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(iframeTag.localName().impl(), &HTMLParser::iframeCreateErrorCheck);
        gFunctionMap.set(isindexTag.localName().impl(), &HTMLParser::isindexCreateErrorCheck);
        gFunctionMap.set(liTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(mapTag.localName().impl(), &HTMLParser::mapCreateErrorCheck);
        gFunctionMap.set(nobrTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(noembedTag.localName().impl(), &HTMLParser::noembedCreateErrorCheck);
        gFunctionMap.set(noframesTag.localName().impl(), &HTMLParser::noframesCreateErrorCheck);
        gFunctionMap.set(noscriptTag.localName().impl(), &HTMLParser::noscriptCreateErrorCheck);
        gFunctionMap.set(sTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(selectTag.localName().impl(), &HTMLParser::selectCreateErrorCheck);
        gFunctionMap.set(smallTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(strikeTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(tbodyTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(tdTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(textAtom.impl(), &HTMLParser::textCreateErrorCheck);
        gFunctionMap.set(tfootTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(thTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(theadTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(trTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(ttTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(uTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
    }

    bool proceed = true;
    RefPtr<Node> result;
    if (CreateErrorCheckFunc errorCheckFunc = gFunctionMap.get(t->tagName.impl()))
        proceed = (this->*errorCheckFunc)(t, result);
    if (proceed)
        result = HTMLElementFactory::createHTMLElement(t->tagName, document, m_currentFormElement.get());
    return result.release();
}

bool HTMLParser::allowNestedRedundantTag(const AtomicString& tagName)
{
    // www.liceo.edu.mx is an example of a site that achieves a level of nesting of
    // about 1500 tags, all from a bunch of <b>s.  We will only allow at most 20
    // nested tags of the same type before just ignoring them all together.
    unsigned i = 0;
    for (HTMLStackElem* curr = blockStack;
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
        
    HTMLStackElem* oldElem = blockStack;
    popBlock(t->tagName, checkForCloseTagErrors);
    if (oldElem == blockStack && t->tagName == pTag) {
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
    static HashSet<AtomicStringImpl*> headerTags;
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
        if (e->hasLocalName(noscriptTag) && !m_isParsingFragment) {
            Settings* settings = document->settings();
            if (settings && settings->isJavaScriptEnabled())
                return true;
        }
    }
    
    return false;
}

bool HTMLParser::isResidualStyleTag(const AtomicString& tagName)
{
    static HashSet<AtomicStringImpl*> residualStyleTags;
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
    static HashSet<AtomicStringImpl*> unaffectedTags;
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
        HTMLStackElem* curr = blockStack;
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
        Node* blockElem = prevMaxElem ? prevMaxElem->node : current;
        Node* parentElem = elem->node;

        // Check to see if the reparenting that is going to occur is allowed according to the DOM.
        // FIXME: We should either always allow it or perform an additional fixup instead of
        // just bailing here.
        // Example: <p><font><center>blah</font></center></p> isn't doing a fixup right now.
        if (!parentElem->childAllowed(blockElem))
            return;

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
        ASSERT(finished || blockElem->firstChild());
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
        }

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
        inStrayTableContent--;

    // Step 7: Reopen intermediate inlines, e.g., <b><p><i>Foo</b>Goo</p>.
    // In the above example, Goo should stay italic.
    // We cap the number of tags we're willing to reopen based off cResidualStyleMaxDepth.
    
    HTMLStackElem* curr = blockStack;
    HTMLStackElem* residualStyleStack = 0;
    unsigned stackDepth = 1;
    while (curr && curr != maxElem) {
        // We will actually schedule this tag for reopening
        // after we complete the close of this entire block.
        if (isResidualStyleTag(curr->tagName) && stackDepth++ < cResidualStyleMaxDepth)
            // We've overloaded the use of stack elements and are just reusing the
            // struct with a slightly different meaning to the variables.  Instead of chaining
            // from innermost to outermost, we build up a list of all the tags we need to reopen
            // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
            // to the outermost tag we need to reopen.
            // We also set curr->node to be the actual element that corresponds to the ID stored in
            // curr->id rather than the node that you should pop to when the element gets pulled off
            // the stack.
            moveOneBlockToStack(residualStyleStack);
        else
            popOneBlock();

        curr = blockStack;
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
            current->appendChild(newNode, ec);
        // FIXME: Is it really OK to ignore the exceptions here?

        // Now push a new stack element for this node we just created.
        pushBlock(elem->tagName, elem->level);

        // Set our strayTableContent boolean if needed, so that the reopened tag also knows
        // that it is inside a malformed table.
        blockStack->strayTableContent = malformedTableParent != 0;
        if (blockStack->strayTableContent)
            inStrayTableContent++;

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
    current->beginParsingChildren();
    blockStack = new HTMLStackElem(tagName, level, current, didRefCurrent, blockStack);
    didRefCurrent = false;
}

void HTMLParser::popBlock(const AtomicString& tagName, bool reportErrors)
{
    HTMLStackElem* elem = blockStack;
    
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
    
    elem = blockStack;
    unsigned stackDepth = 1;
    while (elem) {
        if (elem->tagName == tagName) {
            int strayTable = inStrayTableContent;
            popOneBlock();
            elem = 0;

            // This element was the root of some malformed content just inside an implicit or
            // explicit <tbody> or <tr>.
            // If we end up needing to reopen residual style tags, the root of the reopened chain
            // must also know that it is the root of malformed content inside a <tbody>/<tr>.
            if (strayTable && (inStrayTableContent < strayTable) && residualStyleStack) {
                Node* curr = current;
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
            if (isAffectedByStyle && isResidualStyleTag(elem->tagName) && stackDepth++ < cResidualStyleMaxDepth)
                // We've overloaded the use of stack elements and are just reusing the
                // struct with a slightly different meaning to the variables.  Instead of chaining
                // from innermost to outermost, we build up a list of all the tags we need to reopen
                // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
                // to the outermost tag we need to reopen.
                // We also set elem->node to be the actual element that corresponds to the ID stored in
                // elem->id rather than the node that you should pop to when the element gets pulled off
                // the stack.
                moveOneBlockToStack(residualStyleStack);
            else
                popOneBlock();
            elem = blockStack;
        }
    }

    reopenResidualStyleTags(residualStyleStack, malformedTableParent);
}

inline HTMLStackElem* HTMLParser::popOneBlockCommon()
{
    HTMLStackElem* elem = blockStack;

    // Form elements restore their state during the parsing process.
    // Also, a few elements (<applet>, <object>) need to know when all child elements (<param>s) are available.
    if (current && elem->node != current)
        current->finishParsingChildren();

    blockStack = elem->next;
    current = elem->node;
    didRefCurrent = elem->didRefNode;

    if (elem->strayTableContent)
        inStrayTableContent--;

    return elem;
}

void HTMLParser::popOneBlock()
{
    // Store the current node before popOneBlockCommon overwrites it.
    Node* lastCurrent = current;
    bool didRefLastCurrent = didRefCurrent;

    delete popOneBlockCommon();

    if (didRefLastCurrent)
        lastCurrent->deref();
}

void HTMLParser::moveOneBlockToStack(HTMLStackElem*& head)
{
    // We'll be using the stack element we're popping, but for the current node.
    // See the two callers for details.

    // Store the current node before popOneBlockCommon overwrites it.
    Node* lastCurrent = current;
    bool didRefLastCurrent = didRefCurrent;

    // Pop the block, but don't deref the current node as popOneBlock does because
    // we'll be using the pointer in the new stack element.
    HTMLStackElem* elem = popOneBlockCommon();

    // Transfer the current node into the stack element.
    // No need to deref the old elem->node because popOneBlockCommon transferred
    // it into the current/didRefCurrent fields.
    elem->node = lastCurrent;
    elem->didRefNode = didRefLastCurrent;
    elem->next = head;
    head = elem;
}

void HTMLParser::popInlineBlocks()
{
    while (blockStack && isInline(current))
        popOneBlock();
}

void HTMLParser::freeBlock()
{
    while (blockStack)
        popOneBlock();
}

void HTMLParser::createHead()
{
    if (head || !document->documentElement())
        return;

    head = new HTMLHeadElement(document);
    HTMLElement* body = document->body();
    ExceptionCode ec = 0;
    document->documentElement()->insertBefore(head, body, ec);
    if (ec)
        head = 0;
        
    // If the body does not exist yet, then the <head> should be pushed as the current block.
    if (head && !body) {
        pushBlock(head->localName(), head->tagPriority());
        setCurrent(head);
    }
}

PassRefPtr<Node> HTMLParser::handleIsindex(Token* t)
{
    RefPtr<Node> n = new HTMLDivElement(document);

    NamedMappedAttrMap* attrs = t->attrs.get();

    RefPtr<HTMLIsIndexElement> isIndex = new HTMLIsIndexElement(document, m_currentFormElement.get());
    isIndex->setAttributeMap(attrs);
    isIndex->setAttribute(typeAttr, "khtml_isindex");

    String text = searchableIndexIntroduction();
    if (attrs) {
        if (Attribute* a = attrs->getAttributeItem(promptAttr))
            text = a->value().domString() + " ";
        t->attrs = 0;
    }

    n->addChild(new HTMLHRElement(document));
    n->addChild(new Text(document, text));
    n->addChild(isIndex.release());
    n->addChild(new HTMLHRElement(document));

    return n.release();
}

void HTMLParser::startBody()
{
    if (inBody)
        return;

    inBody = true;

    if (m_isindexElement) {
        insertNode(m_isindexElement.get(), true /* don't descend into this node */);
        m_isindexElement = 0;
    }
}

void HTMLParser::finished()
{
    // In the case of a completely empty document, here's the place to create the HTML element.
    if (current && current->isDocumentNode() && !document->documentElement())
        insertNode(new HTMLHtmlElement(document));

    // This ensures that "current" is not left pointing to a node when the document is destroyed.
    freeBlock();
    setCurrent(0);

    // Warning, this may delete the tokenizer and parser, so don't try to do anything else after this.
    if (!m_isParsingFragment)
        document->finishedParsing();
}

void HTMLParser::reportErrorToConsole(HTMLParserErrorCode errorCode, const AtomicString* tagName1, const AtomicString* tagName2, bool closeTags)
{    
    Frame* frame = document->frame();
    if (!frame)
        return;
    
    Page* page = frame->page();
    if (!page)
        return;

    HTMLTokenizer* htmlTokenizer = static_cast<HTMLTokenizer*>(document->tokenizer());
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

    page->chrome()->addMessageToConsole(HTMLMessageSource, isWarning(errorCode) ? WarningMessageLevel: ErrorMessageLevel, message, lineNumber, document->url());
}

}
