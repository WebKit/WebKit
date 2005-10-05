/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1999,2001 Lars Knoll (knoll@kde.org)
              (C) 2000,2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/
//----------------------------------------------------------------------------
//
// KDE HTML Widget -- HTML Parser
//#define PARSER_DEBUG

#include "config.h"
#include "html/htmlparser.h"

#include "dom/dom_exception.h"

#include "html/html_baseimpl.h"
#include "html/html_blockimpl.h"
#include "html/html_canvasimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_elementimpl.h"
#include "html/html_formimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_inlineimpl.h"
#include "html/html_listimpl.h"
#include "html/html_miscimpl.h"
#include "html/html_tableimpl.h"
#include "html/html_objectimpl.h"
#include "htmlfactory.h"
#include "xml/dom_textimpl.h"
#include "xml/dom_nodeimpl.h"
#include <kxmlcore/HashSet.h>
#include "html/htmltokenizer.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"

#include "rendering/render_object.h"

#include <kxmlcore/HashMap.h>

#include <kdebug.h>
#include <klocale.h>

using namespace DOM;
using namespace HTMLNames;
using namespace khtml;

//----------------------------------------------------------------------------

/**
 * @internal
 */
class HTMLStackElem
{
public:
    HTMLStackElem(const AtomicString& _tagName,
                  int _level,
                  DOM::NodeImpl *_node,
                  HTMLStackElem * _next
        )
        :
        tagName(_tagName),
        level(_level),
        strayTableContent(false),
        node(_node),
        holdingRef(!_node->isDocumentNode()),
        next(_next)
    {
        if (holdingRef)
            _node->ref();
    }

    ~HTMLStackElem()
    {
        if (holdingRef)
            node->deref();
    }

    AtomicString tagName;
    int level;
    bool strayTableContent;
    NodeImpl *node;
    bool holdingRef;
    HTMLStackElem* next;
};

/**
 * @internal
 *
 * The parser parses tokenized input into the document, building up the
 * document tree. If the document is wellformed, parsing it is
 * straightforward.
 * Unfortunately, people can't write wellformed HTML documents, so the parser
 * has to be tolerant about errors.
 *
 * We have to take care of the following error conditions:
 * 1. The element being added is explicitly forbidden inside some outer tag.
 *    In this case we should close all tags up to the one, which forbids
 *    the element, and add it afterwards.
 * 2. We are not allowed to add the element directly. It could be, that
 *    the person writing the document forgot some tag inbetween (or that the
 *    tag inbetween is optional...) This could be the case with the following
 *    tags: HTML HEAD BODY TBODY TR TD LI (did I forget any?)
 * 3. We wan't to add a block element inside to an inline element. Close all
 *    inline elements up to the next higher block element.
 * 4. If this doesn't help close elements, until we are allowed to add the
 *    element or ignore the tag.
 *
 */
HTMLParser::HTMLParser(KHTMLView *_parent, DocumentPtr *doc, bool includesComments) 
    : current(0), currentIsReferenced(false), includesCommentsInDOM(includesComments)
{
    HTMLWidget    = _parent;
    document      = doc;
    document->ref();

    blockStack = 0;

    reset();
}

HTMLParser::HTMLParser(DOM::DocumentFragmentImpl *i, DocumentPtr *doc, bool includesComments)
    : current(0), currentIsReferenced(false), includesCommentsInDOM(includesComments)
{
    HTMLWidget = 0;
    document = doc;
    document->ref();

    blockStack = 0;

    reset();
    setCurrent(i);
    inBody = true;
}

HTMLParser::~HTMLParser()
{
    freeBlock();

    setCurrent(0);

    document->deref();

    if (isindex)
        isindex->deref();
}

void HTMLParser::reset()
{
    setCurrent(doc());

    freeBlock();

    inBody = false;
    haveFrameSet = false;
    haveContent = false;
    inSelect = false;
    inStrayTableContent = 0;
    
    form = 0;
    map = 0;
    head = 0;
    end = false;
    isindex = 0;
    
    discard_until = nullAtom;
}

void HTMLParser::setCurrent(DOM::NodeImpl *newCurrent) 
{
    bool newCurrentIsReferenced = newCurrent && newCurrent != doc();
    if (newCurrentIsReferenced) 
	newCurrent->ref(); 
    if (currentIsReferenced) 
	current->deref(); 
    current = newCurrent;
    currentIsReferenced = newCurrentIsReferenced;
}

void HTMLParser::parseToken(Token *t)
{
    if (!discard_until.isNull()) {
        if (t->tagName == discard_until && !t->beginTag)
            discard_until = nullAtom;

        // do not skip </iframe>
        if (!discard_until.isNull() || (current->localName() != t->tagName))
            return;
    }

    // Apparently some sites use </br> instead of <br>.  Be compatible with IE and Firefox and treat this like <br>.
    if (t->isCloseTag(brTag) && doc()->inCompatMode())
        t->beginTag = true;

    if (!t->beginTag) {
        processCloseTag(t);
        return;
    }

    // ignore spaces, if we're not inside a paragraph or other inline code
    if (t->tagName == textAtom && t->text) {
        if (inBody && !skipMode() && current->localName() != styleTag && current->localName() != titleTag && 
            current->localName() != scriptTag && !t->text->containsOnlyWhitespace()) 
            haveContent = true;
    }

    NodeImpl *n = getNode(t);
    // just to be sure, and to catch currently unimplemented stuff
    if (!n)
        return;

    SharedPtr<NodeImpl> protectNode(n);

    // set attributes
    if (n->isHTMLElement()) {
        HTMLElementImpl *e = static_cast<HTMLElementImpl*>(n);
        e->setAttributeMap(t->attrs);

        // take care of optional close tags
        if (e->endTagRequirement() == TagStatusOptional)
            popBlock(t->tagName);
            
        if (isHeaderTag(t->tagName))
            // Do not allow two header tags to be nested if the intervening tags are inlines.
            popNestedHeaderTag();
    }

    if (!insertNode(n, t->flat)) {
        // we couldn't insert the node...
        if (n->isElementNode()) {
            ElementImpl* e = static_cast<ElementImpl*>(n);
            e->setAttributeMap(0);
        }
            
        if (map == n)
            map = 0;

        if (form == n)
            form = 0;
    }
}

static bool isTableSection(NodeImpl* n)
{
    return n->hasTagName(tbodyTag) || n->hasTagName(tfootTag) || n->hasTagName(theadTag);
}

static bool isTablePart(NodeImpl* n)
{
    return n->hasTagName(trTag) || n->hasTagName(tdTag) || n->hasTagName(thTag) ||
           isTableSection(n);
}

static bool isTableRelated(NodeImpl* n)
{
    return n->hasTagName(tableTag) || isTablePart(n);
}

bool HTMLParser::insertNode(NodeImpl *n, bool flat)
{
    SharedPtr<NodeImpl> protectNode(n);

    const AtomicString& localName = n->localName();
    int tagPriority = n->isHTMLElement() ? static_cast<HTMLElementImpl*>(n)->tagPriority() : 0;
        
    // let's be stupid and just try to insert it.
    // this should work if the document is well-formed
    NodeImpl *newNode = current->addChild(n);
    if (newNode) {
        // don't push elements without end tags (e.g., <img>) on the stack
        bool parentAttached = current->attached();
        if (tagPriority > 0 && !flat) {
            pushBlock(localName, tagPriority);
            if (newNode == current)
                popBlock(localName);
            else
                setCurrent(newNode);
            if (parentAttached && !n->attached() && HTMLWidget)
                n->attach();
        } else {
            if (parentAttached && !n->attached() && HTMLWidget)
                n->attach();
            if (n->maintainsState()) {
                doc()->registerMaintainsState(n);
                QStringList &states = doc()->restoreState();
                if (!states.isEmpty())
                    n->restoreState(states);
            }
            n->closeRenderer();
        }

        return true;
    } else
        return handleError(n, flat, localName, tagPriority); // Try to handle the error.
}

bool HTMLParser::handleError(NodeImpl* n, bool flat, const AtomicString& localName, int tagPriority)
{
    // Error handling code.  This is just ad hoc handling of specific parent/child combinations.
    HTMLElementImpl *e;
    bool handled = false;

    // 1. Check out the element's tag name to decide how to deal with errors.
    if (n->isTextNode()) {
        if (current->hasTagName(selectTag))
            return false;
    } else if (n->isHTMLElement()) {
        HTMLElementImpl* h = static_cast<HTMLElementImpl*>(n);
        if (h->hasLocalName(trTag) || h->hasLocalName(thTag) || h->hasLocalName(tdTag)) {
            if (inStrayTableContent && !isTableRelated(current)) {
                // pop out to the nearest enclosing table-related tag.
                while (blockStack && !isTableRelated(current))
                    popOneBlock();
                return insertNode(n);
            }
        } else if (h->hasLocalName(headTag)) {
            if (!current->isDocumentNode() && !current->hasTagName(htmlTag))
                return false;
        } else if (h->hasLocalName(metaTag) || h->hasLocalName(linkTag) || h->hasLocalName(baseTag)) {
            if (!head)
                createHead();
            if (head) {
                if (head->addChild(n)) {
                    if (!n->attached() && HTMLWidget)
                        n->attach();
                    return true;
                } else
                    return false;
            }
        } else if (h->hasLocalName(htmlTag)) {
            if (!current->isDocumentNode() ) {
                if (doc()->firstChild()->hasTagName(htmlTag)) {
                    // we have another <HTML> element.... apply attributes to existing one
                    // make sure we don't overwrite already existing attributes
                    NamedAttrMapImpl *map = static_cast<ElementImpl*>(n)->attributes(true);
                    NamedAttrMapImpl *bmap = static_cast<ElementImpl*>(doc()->firstChild())->attributes(false);
                    bool changed = false;
                    for (unsigned l = 0; map && l < map->length(); ++l) {
                        AttributeImpl* it = map->attributeItem(l);
                        changed = !bmap->getAttributeItem(it->name());
                        bmap->insertAttribute(it->clone(false));
                    }
                    if (changed)
                        doc()->recalcStyle(NodeImpl::Inherit);
                }
                return false;
            }
        } else if (h->hasLocalName(titleTag) || h->hasLocalName(styleTag)) {
            if (!head)
                createHead();
            if (head) {
                DOM::NodeImpl *newNode = head->addChild(n);
                if (newNode) {
                    pushBlock(localName, tagPriority);
                    setCurrent(newNode);
                    if (!n->attached() && HTMLWidget)
                        n->attach();
                } else {
                    setSkipMode(styleTag);
                    return false;
                }
                return true;
            } else if(inBody) {
                setSkipMode(styleTag);
                return false;
            }
        } else if (h->hasLocalName(bodyTag)) {
            if (inBody && doc()->body()) {
                // we have another <BODY> element.... apply attributes to existing one
                // make sure we don't overwrite already existing attributes
                // some sites use <body bgcolor=rightcolor>...<body bgcolor=wrongcolor>
                NamedAttrMapImpl *map = static_cast<ElementImpl*>(n)->attributes(true);
                NamedAttrMapImpl *bmap = doc()->body()->attributes(false);
                bool changed = false;
                for (unsigned l = 0; map && l < map->length(); ++l) {
                    AttributeImpl* it = map->attributeItem(l);
                    changed = !bmap->getAttributeItem(it->name());
                    bmap->insertAttribute(it->clone(false));
                }
                if (changed)
                    doc()->recalcStyle(NodeImpl::Inherit);
                return false;
            }
            else if (!current->isDocumentNode())
                return false;
        } else if (h->hasLocalName(inputTag)) {
            DOMString type = h->getAttribute(typeAttr);
            if (strcasecmp(type, "hidden") == 0 && form) {
                form->addChild(n);
                if (!n->attached() && HTMLWidget)
                    n->attach();
                return true;
            }
        } else if (h->hasLocalName(ddTag) || h->hasLocalName(dtTag)) {
            e = new HTMLDListElementImpl(document);
            if (insertNode(e)) {
                insertNode(n);
                return true;
            }
        } else if (h->hasLocalName(areaTag)) {
            if (map) {
                map->addChild(n);
                if (!n->attached() && HTMLWidget)
                    n->attach();
                handled = true;
                return true;
            }
            return false;
        } else if (h->hasLocalName(captionTag)) {
            if (isTablePart(current)) {
                NodeImpl* tsection = current;
                if (current->hasTagName(trTag))
                    tsection = current->parent();
                else if (current->hasTagName(tdTag) || current->hasTagName(thTag))
                    tsection = current->parent()->parent();
                NodeImpl* table = tsection->parent();
                int exceptioncode = 0;
                table->insertBefore(n, tsection, exceptioncode);
                pushBlock(localName, tagPriority);
                setCurrent(n);
                inStrayTableContent++;
                blockStack->strayTableContent = true;
                return true;
            }
        } else if (h->hasLocalName(theadTag) || h->hasLocalName(tbodyTag) ||
                   h->hasLocalName(tfootTag) || h->hasLocalName(colgroupTag)) {
            if (isTableRelated(current)) {
                while (blockStack && isTablePart(current))
                    popOneBlock();
                return insertNode(n);
            }
        }
    }
    
    // 2. Next we examine our currently active element to do some further error handling.
    if (current->isHTMLElement()) {
        HTMLElementImpl* h = static_cast<HTMLElementImpl*>(current);
        const AtomicString& currentTagName = current->localName();
        if (h->hasLocalName(htmlTag)) {
            HTMLElementImpl* elt = n->isHTMLElement() ? static_cast<HTMLElementImpl*>(n) : 0;
            if (elt && (elt->hasLocalName(scriptTag) || elt->hasLocalName(styleTag) ||
                elt->hasLocalName(metaTag) || elt->hasLocalName(linkTag) ||
                elt->hasLocalName(objectTag) || elt->hasLocalName(embedTag) ||
                elt->hasLocalName(titleTag) || elt->hasLocalName(isindexTag) ||
                elt->hasLocalName(baseTag))) {
                if (!head) {
                    head = new HTMLHeadElementImpl(document);
                    e = head;
                    insertNode(e);
                    handled = true;
                }
            } else {
                if (n->isTextNode()) {
                    TextImpl *t = static_cast<TextImpl *>(n);
                    if (t->containsOnlyWhitespace())
                        return false;
                }
                if (!haveFrameSet) {
                    e = new HTMLBodyElementImpl(document);
                    startBody();
                    insertNode(e);
                    handled = true;
                }
            }
        } else if (h->hasLocalName(headTag)) {
            if (n->hasTagName(htmlTag))
                return false;
            else {
                // This means the body starts here...
                if (!haveFrameSet) {
                    popBlock(currentTagName);
                    e = new HTMLBodyElementImpl(document);
                    startBody();
                    insertNode(e);
                    handled = true;
                }
            }
        } else if (h->hasLocalName(captionTag)) {
            // Illegal content in a caption. Close the caption and try again.
            popBlock(currentTagName);
            if (isTablePart(n))
                return insertNode(n, flat);
        } else if (h->hasLocalName(tableTag) || h->hasLocalName(trTag) || isTableSection(h)) {
            if (n->hasTagName(tableTag)) {
                popBlock(localName); // end the table
                handled = true;      // ...and start a new one
            } else {
                bool possiblyMoveStrayContent = true;
                int exceptionCode = 0;
                if (n->isTextNode()) {
                    TextImpl *t = static_cast<TextImpl *>(n);
                    if (t->containsOnlyWhitespace())
                        return false;
                    DOMStringImpl *i = t->string();
                    unsigned int pos = 0;
                    while (pos < i->l && (*(i->s+pos) == QChar(' ') ||
                                          *(i->s+pos) == QChar(0xa0))) pos++;
                    if (pos == i->l)
                        possiblyMoveStrayContent = false;
                }
                if (possiblyMoveStrayContent) {
                    NodeImpl *node = current;
                    NodeImpl *parent = node->parentNode();
                    NodeImpl *grandparent = parent->parentNode();

                    if (n->isTextNode() ||
                        (h->hasLocalName(trTag) &&
                         isTableSection(parent) && grandparent->hasTagName(tableTag)) ||
                         ((!n->hasTagName(tdTag) && !n->hasTagName(thTag) &&
                           !n->hasTagName(formTag) && !n->hasTagName(scriptTag)) && isTableSection(node) &&
                         parent->hasTagName(tableTag))) {
                        node = (node->hasTagName(tableTag)) ? node :
                                ((node->hasTagName(trTag)) ? grandparent : parent);
                        NodeImpl *parent = node->parentNode();
                        parent->insertBefore(n, node, exceptionCode);
                        if (!exceptionCode) {
                            if (n->isHTMLElement() && tagPriority > 0 && 
                                !flat && static_cast<HTMLElementImpl*>(n)->endTagRequirement() != TagStatusForbidden)
                            {
                                pushBlock(localName, tagPriority);
                                setCurrent(n);
                                inStrayTableContent++;
                                blockStack->strayTableContent = true;
                            }
                            return true;
                        }
                    }

                    if (!exceptionCode) {
                        if (current->hasTagName(trTag))
                            e = new HTMLTableCellElementImpl(tdTag, document);
                        else if (current->hasTagName(tableTag))
                            e = new HTMLTableSectionElementImpl(tbodyTag, document, true); // implicit 
                        else
                            e = new HTMLTableRowElementImpl(document);
                        
                        insertNode(e);
                        handled = true;
                    }
                }
            }
        } else if (h->hasLocalName(objectTag)) {
            setSkipMode(objectTag);
            return false;
        } else if (h->hasLocalName(ulTag) || h->hasLocalName(olTag) ||
                 h->hasLocalName(dirTag) || h->hasLocalName(menuTag)) {
            e = new HTMLDivElementImpl(document);
            insertNode(e);
            handled = true;
        } else if (h->hasLocalName(dlTag) || h->hasLocalName(dtTag)) {
            popBlock(currentTagName);
            handled = true;
        } else if (h->hasLocalName(selectTag)) {
            if (isInline(n))
                return false;
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
        } else if (h->hasLocalName(addressTag)) {
            popBlock(currentTagName);
            handled = true;
        } else if (h->hasLocalName(colgroupTag)) {
            if (!n->isTextNode()) {
                popBlock(currentTagName);
                handled = true;
            }
        } else if (h->hasLocalName(fontTag)) {
            popBlock(currentTagName);
            handled = true;
        } else if (!h->hasLocalName(bodyTag)) {
            if (isInline(current)) {
                popInlineBlocks();
                handled = true;
            }
        }
    } else if (current->isDocumentNode()) {
        if (current->firstChild() == 0) {
            e = new HTMLHtmlElementImpl(document);
            insertNode(e);
            handled = true;
        }
    }

    // 3. If we couldn't handle the error, just return false and attempt to error-correct again.
    if (!handled)
        return false;
    return insertNode(n);
}

typedef bool (HTMLParser::*CreateErrorCheckFunc)(Token* t, NodeImpl*&);
typedef HashMap<DOMStringImpl *, CreateErrorCheckFunc, PointerHash<DOMStringImpl *> > FunctionMap;

bool HTMLParser::textCreateErrorCheck(Token* t, NodeImpl*& result)
{
    result = new TextImpl(document, t->text);
    return false;
}

bool HTMLParser::commentCreateErrorCheck(Token* t, NodeImpl*& result)
{
    if (includesCommentsInDOM)
        result = new CommentImpl(document, t->text);
    return false;
}

bool HTMLParser::headCreateErrorCheck(Token* t, NodeImpl*& result)
{
    return (!head || current->localName() == htmlTag);
}

bool HTMLParser::bodyCreateErrorCheck(Token* t, NodeImpl*& result)
{
    // body no longer allowed if we have a frameset
    if (haveFrameSet)
        return false;
    popBlock(headTag);
    startBody();
    return true;
}

bool HTMLParser::framesetCreateErrorCheck(Token* t, NodeImpl*& result)
{
    popBlock(headTag);
    if (inBody && !haveFrameSet && !haveContent) {
        popBlock(bodyTag);
        // ### actually for IE document.body returns the now hidden "body" element
        // we can't implement that behaviour now because it could cause too many
        // regressions and the headaches are not worth the work as long as there is
        // no site actually relying on that detail (Dirk)
        if (doc()->body())
            doc()->body()->setAttribute(styleAttr, "display:none");
        inBody = false;
    }
    if ((haveContent || haveFrameSet) && current->localName() == htmlTag)
        return false;
    haveFrameSet = true;
    startBody();
    return true;
}

bool HTMLParser::iframeCreateErrorCheck(Token* t, NodeImpl*& result)
{
    // a bit of a special case, since the frame is inlined
    setSkipMode(iframeTag);
    return true;
}

bool HTMLParser::formCreateErrorCheck(Token* t, NodeImpl*& result)
{
    // Only create a new form if we're not already inside one.
    // This is consistent with other browsers' behavior.
    if (!form) {
        form = new HTMLFormElementImpl(document);
        result = form;
    }
    return false;
}

bool HTMLParser::isindexCreateErrorCheck(Token* t, NodeImpl*& result)
{
    NodeImpl *n = handleIsindex(t);
    if (!inBody) {
        if (isindex)
            isindex->deref();
        isindex = n;
        isindex->ref();
    } else {
        t->flat = true;
        result = n;
    }
    return false;
}

bool HTMLParser::selectCreateErrorCheck(Token* t, NodeImpl*& result)
{
    inSelect = true;
    return true;
}

bool HTMLParser::ddCreateErrorCheck(Token* t, NodeImpl*& result)
{
    popBlock(dtTag);
    popBlock(ddTag);
    return true;
}

bool HTMLParser::dtCreateErrorCheck(Token* t, NodeImpl*& result)
{
    popBlock(ddTag);
    popBlock(dtTag);
    return true;
}

bool HTMLParser::nestedCreateErrorCheck(Token* t, NodeImpl*& result)
{
    popBlock(t->tagName);
    return true;
}

bool HTMLParser::nestedStyleCreateErrorCheck(Token* t, NodeImpl*& result)
{
    return allowNestedRedundantTag(t->tagName);
}

bool HTMLParser::tableCellCreateErrorCheck(Token* t, NodeImpl*& result)
{
    popBlock(tdTag);
    popBlock(thTag);
    return true;
}

bool HTMLParser::tableSectionCreateErrorCheck(Token* t, NodeImpl*& result)
{
    popBlock(theadTag);
    popBlock(tbodyTag);
    popBlock(tfootTag);
    return true;
}

bool HTMLParser::noembedCreateErrorCheck(Token* t, NodeImpl*& result)
{
    setSkipMode(noembedTag);
    return true;
}

bool HTMLParser::noframesCreateErrorCheck(Token* t, NodeImpl*& result)
{
    setSkipMode(noframesTag);
    return true;
}

bool HTMLParser::noscriptCreateErrorCheck(Token* t, NodeImpl*& result)
{
    if (HTMLWidget && HTMLWidget->part()->jScriptEnabled())
        setSkipMode(noscriptTag);
    return true;
}

bool HTMLParser::mapCreateErrorCheck(Token* t, NodeImpl*& result)
{
    map = new HTMLMapElementImpl(document);
    result = map;
    return false;
}

NodeImpl *HTMLParser::getNode(Token* t)
{
    // Init our error handling table.
    static FunctionMap gFunctionMap;
    if (gFunctionMap.isEmpty()) {
        gFunctionMap.set(textAtom.impl(), &HTMLParser::textCreateErrorCheck);
        gFunctionMap.set(commentAtom.impl(), &HTMLParser::commentCreateErrorCheck);
        gFunctionMap.set(headTag.localName().impl(), &HTMLParser::headCreateErrorCheck);
        gFunctionMap.set(bodyTag.localName().impl(), &HTMLParser::bodyCreateErrorCheck);
        gFunctionMap.set(framesetTag.localName().impl(), &HTMLParser::framesetCreateErrorCheck);
        gFunctionMap.set(iframeTag.localName().impl(), &HTMLParser::iframeCreateErrorCheck);
        gFunctionMap.set(formTag.localName().impl(), &HTMLParser::formCreateErrorCheck);
        gFunctionMap.set(isindexTag.localName().impl(), &HTMLParser::isindexCreateErrorCheck);
        gFunctionMap.set(selectTag.localName().impl(), &HTMLParser::selectCreateErrorCheck);
        gFunctionMap.set(ddTag.localName().impl(), &HTMLParser::ddCreateErrorCheck);
        gFunctionMap.set(dtTag.localName().impl(), &HTMLParser::dtCreateErrorCheck);
        gFunctionMap.set(liTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(aTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(nobrTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(wbrTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(trTag.localName().impl(), &HTMLParser::nestedCreateErrorCheck);
        gFunctionMap.set(tdTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(thTag.localName().impl(), &HTMLParser::tableCellCreateErrorCheck);
        gFunctionMap.set(tbodyTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(theadTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(tfootTag.localName().impl(), &HTMLParser::tableSectionCreateErrorCheck);
        gFunctionMap.set(ttTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(uTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(iTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(sTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(strikeTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(bigTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(smallTag.localName().impl(), &HTMLParser::nestedStyleCreateErrorCheck);
        gFunctionMap.set(noembedTag.localName().impl(), &HTMLParser::noembedCreateErrorCheck);
        gFunctionMap.set(noframesTag.localName().impl(), &HTMLParser::noframesCreateErrorCheck);
        gFunctionMap.set(noscriptTag.localName().impl(), &HTMLParser::noscriptCreateErrorCheck);
        gFunctionMap.set(mapTag.localName().impl(), &HTMLParser::mapCreateErrorCheck);
    }

    bool proceed = true;
    NodeImpl *result = 0;
    if (CreateErrorCheckFunc errorCheckFunc = gFunctionMap.get(t->tagName.impl()))
        proceed = (this->*(errorCheckFunc))(t, result);
    if (proceed)
        result = HTMLElementFactory::createHTMLElement(t->tagName, doc(), form);
    return result;
}

#define MAX_REDUNDANT 20

bool HTMLParser::allowNestedRedundantTag(const AtomicString& _tagName)
{
    // www.liceo.edu.mx is an example of a site that achieves a level of nesting of
    // about 1500 tags, all from a bunch of <b>s.  We will only allow at most 20
    // nested tags of the same type before just ignoring them all together.
    int i = 0;
    for (HTMLStackElem* curr = blockStack;
         i < MAX_REDUNDANT && curr && curr->tagName == _tagName;
         curr = curr->next, i++);
    return i != MAX_REDUNDANT;
}

void HTMLParser::processCloseTag(Token *t)
{
    // Support for really broken html.
    // we never close the body tag, since some stupid web pages close it before the actual end of the doc.
    // let's rely on the end() call to close things.
    if (t->tagName == htmlTag || t->tagName == bodyTag)
        return;
    
    if (t->tagName == formTag)
        form = 0;
    else if (t->tagName == mapTag)
        map = 0;
    else if (t->tagName == selectTag)
        inSelect = false;
        
    HTMLStackElem* oldElem = blockStack;
    popBlock(t->tagName);
    if (oldElem == blockStack && t->tagName == pTag) {
        // We encountered a stray </p>.  Amazingly Gecko, WinIE, and MacIE all treat
        // this as a valid break, i.e., <p></p>.  So go ahead and make the empty
        // paragraph.
        t->beginTag = true;
        parseToken(t);
        popBlock(t->tagName);
    }
#ifdef PARSER_DEBUG
    kdDebug( 6035 ) << "closeTag --> current = " << current->nodeName().qstring() << endl;
#endif
}

bool HTMLParser::isHeaderTag(const AtomicString& tagName)
{
    static HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > headerTags;
    if (headerTags.isEmpty()) {
        headerTags.insert(h1Tag.localName().impl());
        headerTags.insert(h2Tag.localName().impl());
        headerTags.insert(h3Tag.localName().impl());
        headerTags.insert(h4Tag.localName().impl());
        headerTags.insert(h5Tag.localName().impl());
        headerTags.insert(h6Tag.localName().impl());
    }
    
    return headerTags.contains(tagName.impl());
}

void HTMLParser::popNestedHeaderTag()
{
    // This function only cares about checking for nested headers that have only inlines in between them.
    NodeImpl* currNode = current;
    for (HTMLStackElem* curr = blockStack; curr; curr = curr->next) {
        if (isHeaderTag(curr->tagName)) {
            popBlock(curr->tagName);
            return;
        }
        if (currNode && !isInline(currNode))
            return;
        currNode = curr->node;
    }
}

bool HTMLParser::isInline(DOM::NodeImpl* node) const
{
    if (node->isTextNode())
        return true;

    if (node->isHTMLElement()) {
        HTMLElementImpl* e = static_cast<HTMLElementImpl*>(node);
        if (e->hasLocalName(aTag) || e->hasLocalName(fontTag) || e->hasLocalName(ttTag) ||
            e->hasLocalName(uTag) || e->hasLocalName(bTag) || e->hasLocalName(iTag) ||
            e->hasLocalName(sTag) || e->hasLocalName(strikeTag) || e->hasLocalName(bigTag) ||
            e->hasLocalName(smallTag) || e->hasLocalName(emTag) || e->hasLocalName(strongTag) ||
            e->hasLocalName(dfnTag) || e->hasLocalName(codeTag) || e->hasLocalName(sampTag) ||
            e->hasLocalName(kbdTag) || e->hasLocalName(varTag) || e->hasLocalName(citeTag) ||
            e->hasLocalName(abbrTag) || e->hasLocalName(acronymTag) || e->hasLocalName(subTag) ||
            e->hasLocalName(supTag) || e->hasLocalName(spanTag) || e->hasLocalName(nobrTag) ||
            e->hasLocalName(wbrTag))
            return true;
    }
    
    return false;
}

bool HTMLParser::isResidualStyleTag(const AtomicString& tagName)
{
    static HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > residualStyleTags;
    if (residualStyleTags.isEmpty()) {
        residualStyleTags.insert(aTag.localName().impl());
        residualStyleTags.insert(fontTag.localName().impl());
        residualStyleTags.insert(ttTag.localName().impl());
        residualStyleTags.insert(uTag.localName().impl());
        residualStyleTags.insert(bTag.localName().impl());
        residualStyleTags.insert(iTag.localName().impl());
        residualStyleTags.insert(sTag.localName().impl());
        residualStyleTags.insert(strikeTag.localName().impl());
        residualStyleTags.insert(bigTag.localName().impl());
        residualStyleTags.insert(smallTag.localName().impl());
        residualStyleTags.insert(emTag.localName().impl());
        residualStyleTags.insert(strongTag.localName().impl());
        residualStyleTags.insert(dfnTag.localName().impl());
        residualStyleTags.insert(codeTag.localName().impl());
        residualStyleTags.insert(sampTag.localName().impl());
        residualStyleTags.insert(kbdTag.localName().impl());
        residualStyleTags.insert(varTag.localName().impl());
        residualStyleTags.insert(nobrTag.localName().impl());
        residualStyleTags.insert(wbrTag.localName().impl());
    }
    
    return residualStyleTags.contains(tagName.impl());
}

bool HTMLParser::isAffectedByResidualStyle(const AtomicString& tagName)
{
    if (isResidualStyleTag(tagName))
        return true;

    static HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > affectedBlockTags;
    if (affectedBlockTags.isEmpty()) {
        affectedBlockTags.insert(h1Tag.localName().impl());
        affectedBlockTags.insert(h2Tag.localName().impl());
        affectedBlockTags.insert(h3Tag.localName().impl());
        affectedBlockTags.insert(h4Tag.localName().impl());
        affectedBlockTags.insert(h5Tag.localName().impl());
        affectedBlockTags.insert(h6Tag.localName().impl());
        affectedBlockTags.insert(pTag.localName().impl());
        affectedBlockTags.insert(divTag.localName().impl());
        affectedBlockTags.insert(blockquoteTag.localName().impl());
        affectedBlockTags.insert(addressTag.localName().impl());
        affectedBlockTags.insert(centerTag.localName().impl());
        affectedBlockTags.insert(ulTag.localName().impl());
        affectedBlockTags.insert(olTag.localName().impl());
        affectedBlockTags.insert(liTag.localName().impl());
        affectedBlockTags.insert(dlTag.localName().impl());
        affectedBlockTags.insert(dtTag.localName().impl());
        affectedBlockTags.insert(ddTag.localName().impl());
        affectedBlockTags.insert(preTag.localName().impl());
    }
    
    return affectedBlockTags.contains(tagName.impl());
}

void HTMLParser::handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem* elem)
{
    // Find the element that crosses over to a higher level.   For now, if there is more than
    // one, we will just give up and not attempt any sort of correction.  It's highly unlikely that
    // there will be more than one, since <p> tags aren't allowed to be nested.
    int exceptionCode = 0;
    HTMLStackElem* curr = blockStack;
    HTMLStackElem* maxElem = 0;
    HTMLStackElem* prev = 0;
    HTMLStackElem* prevMaxElem = 0;
    while (curr && curr != elem) {
        if (curr->level > elem->level) {
            if (maxElem)
                return;
            maxElem = curr;
            prevMaxElem = prev;
        }

        prev = curr;
        curr = curr->next;
    }

    if (!curr || !maxElem || !isAffectedByResidualStyle(maxElem->tagName)) return;

    NodeImpl* residualElem = prev->node;
    NodeImpl* blockElem = prevMaxElem ? prevMaxElem->node : current;
    NodeImpl* parentElem = elem->node;

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
                prevElem->node = currElem->node;
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
        NodeImpl* prevNode = 0;
        NodeImpl* currNode = 0;
        currElem = maxElem;
        while (currElem->node != residualElem) {
            if (isResidualStyleTag(currElem->node->localName())) {
                // Create a clone of this element.
                currNode = currElem->node->cloneNode(false);

                // Change the stack element's node to point to the clone.
                currElem->node = currNode;
                
                // Attach the previous node as a child of this new node.
                if (prevNode)
                    currNode->appendChild(prevNode, exceptionCode);
                else // The new parent for the block element is going to be the innermost clone.
                    parentElem = currNode;
                                
                prevNode = currNode;
            }
            
            currElem = currElem->next;
        }

        // Now append the chain of new residual style elements if one exists.
        if (prevNode)
            elem->node->appendChild(prevNode, exceptionCode);
    }
         
    // We need to make a clone of |residualElem| and place it just inside |blockElem|.
    // All content of |blockElem| is reparented to be under this clone.  We then
    // reparent |blockElem| using real DOM calls so that attachment/detachment will
    // be performed to fix up the rendering tree.
    // So for this example: <b>...<p>Foo</b>Goo</p>
    // The end result will be: <b>...</b><p><b>Foo</b>Goo</p>
    //
    // Step 1: Remove |blockElem| from its parent, doing a batch detach of all the kids.
    blockElem->parentNode()->removeChild(blockElem, exceptionCode);
        
    // Step 2: Clone |residualElem|.
    NodeImpl* newNode = residualElem->cloneNode(false); // Shallow clone. We don't pick up the same kids.

    // Step 3: Place |blockElem|'s children under |newNode|.  Remove all of the children of |blockElem|
    // before we've put |newElem| into the document.  That way we'll only do one attachment of all
    // the new content (instead of a bunch of individual attachments).
    NodeImpl* currNode = blockElem->firstChild();
    while (currNode) {
        NodeImpl* nextNode = currNode->nextSibling();
        blockElem->removeChild(currNode, exceptionCode);
        newNode->appendChild(currNode, exceptionCode);
        currNode = nextNode;
    }

    // Step 4: Place |newNode| under |blockElem|.  |blockElem| is still out of the document, so no
    // attachment can occur yet.
    blockElem->appendChild(newNode, exceptionCode);
    
    // Step 5: Reparent |blockElem|.  Now the full attachment of the fixed up tree takes place.
    parentElem->appendChild(blockElem, exceptionCode);
        
    // Step 6: Elide |elem|, since it is effectively no longer open.  Also update
    // the node associated with the previous stack element so that when it gets popped,
    // it doesn't make the residual element the next current node.
    HTMLStackElem* currElem = maxElem;
    HTMLStackElem* prevElem = 0;
    while (currElem != elem) {
        prevElem = currElem;
        currElem = currElem->next;
    }
    prevElem->next = elem->next;
    prevElem->node = elem->node;
    delete elem;
    
    // Step 7: Reopen intermediate inlines, e.g., <b><p><i>Foo</b>Goo</p>.
    // In the above example, Goo should stay italic.
    curr = blockStack;
    HTMLStackElem* residualStyleStack = 0;
    while (curr && curr != maxElem) {
        // We will actually schedule this tag for reopening
        // after we complete the close of this entire block.
        NodeImpl* currNode = current;
        if (isResidualStyleTag(curr->tagName)) {
            // We've overloaded the use of stack elements and are just reusing the
            // struct with a slightly different meaning to the variables.  Instead of chaining
            // from innermost to outermost, we build up a list of all the tags we need to reopen
            // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
            // to the outermost tag we need to reopen.
            // We also set curr->node to be the actual element that corresponds to the ID stored in
            // curr->id rather than the node that you should pop to when the element gets pulled off
            // the stack.
            popOneBlock(false);
            curr->node = currNode;
            curr->next = residualStyleStack;
            residualStyleStack = curr;
        }
        else
            popOneBlock();

        curr = blockStack;
    }

    reopenResidualStyleTags(residualStyleStack, 0); // FIXME: Deal with stray table content some day
                                                    // if it becomes necessary to do so.
}

void HTMLParser::reopenResidualStyleTags(HTMLStackElem* elem, DOM::NodeImpl* malformedTableParent)
{
    // Loop for each tag that needs to be reopened.
    while (elem) {
        // Create a shallow clone of the DOM node for this element.
        NodeImpl* newNode = elem->node->cloneNode(false); 

        // Append the new node. In the malformed table case, we need to insert before the table,
        // which will be the last child.
        int exceptionCode = 0;
        if (malformedTableParent)
            malformedTableParent->insertBefore(newNode, malformedTableParent->lastChild(), exceptionCode);
        else
            current->appendChild(newNode, exceptionCode);
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
        setCurrent(newNode);
        
        // Advance to the next tag that needs to be reopened.
        HTMLStackElem* next = elem->next;
        delete elem;
        elem = next;
    }
}

void HTMLParser::pushBlock(const AtomicString& tagName, int _level)
{
    HTMLStackElem *Elem = new HTMLStackElem(tagName, _level, current, blockStack);
    blockStack = Elem;
}

void HTMLParser::popBlock(const AtomicString& _tagName)
{
    HTMLStackElem *Elem = blockStack;
    
    int maxLevel = 0;

    while (Elem && (Elem->tagName != _tagName)) {
        if (maxLevel < Elem->level)
            maxLevel = Elem->level;
        Elem = Elem->next;
    }

    if (!Elem)
        return;

    if (maxLevel > Elem->level) {
        // We didn't match because the tag is in a different scope, e.g.,
        // <b><p>Foo</b>.  Try to correct the problem.
        if (!isResidualStyleTag(_tagName))
            return;
        return handleResidualStyleCloseTagAcrossBlocks(Elem);
    }

    bool isAffectedByStyle = isAffectedByResidualStyle(Elem->tagName);
    HTMLStackElem* residualStyleStack = 0;
    NodeImpl* malformedTableParent = 0;
    
    Elem = blockStack;
    while (Elem) {
        if (Elem->tagName == _tagName) {
            int strayTable = inStrayTableContent;
            popOneBlock();
            Elem = 0;

            // This element was the root of some malformed content just inside an implicit or
            // explicit <tbody> or <tr>.
            // If we end up needing to reopen residual style tags, the root of the reopened chain
            // must also know that it is the root of malformed content inside a <tbody>/<tr>.
            if (strayTable && (inStrayTableContent < strayTable) && residualStyleStack) {
                NodeImpl* curr = current;
                while (curr && !curr->hasTagName(tableTag))
                    curr = curr->parentNode();
                malformedTableParent = curr ? curr->parentNode() : 0;
            }
        }
        else {
            if (form && Elem->tagName == formTag)
                // A <form> is being closed prematurely (and this is
                // malformed HTML).  Set an attribute on the form to clear out its
                // bottom margin.
                form->setMalformed(true);

            // Schedule this tag for reopening
            // after we complete the close of this entire block.
            NodeImpl* currNode = current;
            if (isAffectedByStyle && isResidualStyleTag(Elem->tagName)) {
                // We've overloaded the use of stack elements and are just reusing the
                // struct with a slightly different meaning to the variables.  Instead of chaining
                // from innermost to outermost, we build up a list of all the tags we need to reopen
                // from the outermost to the innermost, i.e., residualStyleStack will end up pointing
                // to the outermost tag we need to reopen.
                // We also set Elem->node to be the actual element that corresponds to the ID stored in
                // Elem->id rather than the node that you should pop to when the element gets pulled off
                // the stack.
                popOneBlock(false);
                Elem->next = residualStyleStack;
                Elem->node = currNode;
                residualStyleStack = Elem;
            }
            else
                popOneBlock();
            Elem = blockStack;
        }
    }

    reopenResidualStyleTags(residualStyleStack, malformedTableParent);
}

void HTMLParser::popOneBlock(bool delBlock)
{
    HTMLStackElem *Elem = blockStack;

    // we should never get here, but some bad html might cause it.
    if (!Elem) return;
    
    if ((Elem->node != current)) {
        if (current->maintainsState() && doc()){
            doc()->registerMaintainsState(current);
            QStringList &states = doc()->restoreState();
            if (!states.isEmpty())
                current->restoreState(states);
        }
        
        // A few elements (<applet>, <object>) need to know when all child elements (<param>s) are available:
        current->closeRenderer();
    }

    blockStack = Elem->next;
    setCurrent(Elem->node);

    if (Elem->strayTableContent)
        inStrayTableContent--;
    
    if (delBlock)
        delete Elem;
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
    if(head || !doc()->firstChild())
        return;

    head = new HTMLHeadElementImpl(document);
    HTMLElementImpl *body = doc()->body();
    int exceptioncode = 0;
    doc()->firstChild()->insertBefore(head, body, exceptioncode);
    if ( exceptioncode ) {
#ifdef PARSER_DEBUG
        kdDebug( 6035 ) << "creation of head failed!!!!" << endl;
#endif
        head = 0;
    }
}

NodeImpl *HTMLParser::handleIsindex( Token *t )
{
    NodeImpl *n;
    HTMLFormElementImpl *myform = form;
    if (!myform) {
        myform = new HTMLFormElementImpl(document);
        n = myform;
    } else
        n = new HTMLDivElementImpl(document);

    NamedMappedAttrMapImpl *attrs = t->attrs;
    t->attrs = NULL;

    HTMLIsIndexElementImpl *isIndex = new HTMLIsIndexElementImpl(document, myform);
    isIndex->setAttributeMap(attrs);
    isIndex->setAttribute(typeAttr, "khtml_isindex");

#if APPLE_CHANGES
    DOMString text = searchableIndexIntroduction();
#else
    DOMString text = i18n("This is a searchable index. Enter search keywords: ");
#endif
    if (attrs)
        if (AttributeImpl *a = attrs->getAttributeItem(promptAttr))
            text = a->value().domString() + " ";

    attrs->deref();

    n->addChild(new HTMLHRElementImpl(document));
    n->addChild(new TextImpl(document, text));
    n->addChild(isIndex);
    n->addChild(new HTMLHRElementImpl(document));

    return n;
}

void HTMLParser::startBody()
{
    if(inBody) return;

    inBody = true;

    if( isindex ) {
        insertNode( isindex, true /* don't decend into this node */ );
        isindex = 0;
    }
}

void HTMLParser::finished()
{
    // In the case of a completely empty document, here's the place to create the HTML element.
    if (current && current->isDocumentNode() && current->firstChild() == 0) {
        insertNode(new HTMLHtmlElementImpl(document));
    }

    // This ensures that "current" is not left pointing to a node when the document is destroyed.
    freeBlock();
    setCurrent(0);
}
