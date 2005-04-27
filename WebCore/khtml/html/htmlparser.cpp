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
#include "xml/dom_textimpl.h"
#include "xml/dom_nodeimpl.h"
#include "misc/htmlhashes.h"
#include "html/htmltokenizer.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"

#include "rendering/render_object.h"

#include <kdebug.h>
#include <klocale.h>

using namespace DOM;
using namespace khtml;

//----------------------------------------------------------------------------

/**
 * @internal
 */
class HTMLStackElem
{
public:
    HTMLStackElem( int _id,
                   int _level,
                   DOM::NodeImpl *_node,
                   HTMLStackElem * _next
        )
        :
        id(_id),
        level(_level),
        strayTableContent(false),
        node(_node),
        next(_next)
        { }

    int       id;
    int       level;
    bool      strayTableContent;
    NodeImpl *node;
    HTMLStackElem *next;
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
KHTMLParser::KHTMLParser(KHTMLView *_parent, DocumentPtr *doc, bool includesComments) 
    : current(0), currentIsReferenced(false), includesCommentsInDOM(includesComments)
{
    //kdDebug( 6035 ) << "parser constructor" << endl;
#if SPEED_DEBUG > 0
    qt.start();
#endif

    HTMLWidget    = _parent;
    document      = doc;
    document->ref();

    blockStack = 0;

    reset();
}

KHTMLParser::KHTMLParser(DOM::DocumentFragmentImpl *i, DocumentPtr *doc, bool includesComments)
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

KHTMLParser::~KHTMLParser()
{
#if SPEED_DEBUG > 0
    kdDebug( ) << "TIME: parsing time was = " << qt.elapsed() << endl;
#endif

    freeBlock();

    setCurrent(0);

    document->deref();

    if (isindex)
        isindex->deref();
}

void KHTMLParser::reset()
{
    setCurrent(doc());

    freeBlock();

    // before parsing, no tags are forbidden
    memset(forbiddenTag, 0, sizeof(forbiddenTag));

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
    
    discard_until = 0;
}

void KHTMLParser::setCurrent(DOM::NodeImpl *newCurrent) 
{
    bool newCurrentIsReferenced = newCurrent && newCurrent != doc();
    if (newCurrentIsReferenced) 
	newCurrent->ref(); 
    if (currentIsReferenced) 
	current->deref(); 
    current = newCurrent;
    currentIsReferenced = newCurrentIsReferenced;
}

void KHTMLParser::parseToken(Token *t)
{
    if(discard_until) {
        if(t->id == discard_until)
            discard_until = 0;

        // do not skip </iframe>
        if ( discard_until || current->id() + ID_CLOSE_TAG != t->id )
            return;
    }

#ifdef PARSER_DEBUG
    kdDebug( 6035 ) << "\n\n==> parser: processing token " << getTagName(t->id).string() << "(" << t->id << ")"
                    << " current = " << getTagName(current->id()).string() << "(" << current->id() << ")" << endl;
    kdDebug(6035) << " inBody=" << inBody << " haveFrameSet=" << haveFrameSet << endl;
#endif

    // holy shit. apparently some sites use </br> instead of <br>
    // be compatible with IE and NS
    if (t->id == ID_BR + ID_CLOSE_TAG && doc()->inCompatMode())
        t->id = ID_BR;

    if (t->id > ID_CLOSE_TAG)
    {
        processCloseTag(t);
        return;
    }

    // ignore spaces, if we're not inside a paragraph or other inline code
    if( t->id == ID_TEXT && t->text ) {
        if(inBody && !skipMode() && current->id() != ID_STYLE 
            && current->id() != ID_TITLE && current->id() != ID_SCRIPT &&
            !t->text->containsOnlyWhitespace()) 
            haveContent = true;
#ifdef PARSER_DEBUG
        kdDebug(6035) << "length="<< t->text->l << " text='" << QConstString(t->text->s, t->text->l).string() << "'" << endl;
#endif
    }

    NodeImpl *n = getElement(t);
    // just to be sure, and to catch currently unimplemented stuff
    if(!n)
        return;

    Node protectNode(n);

    // set attributes
    if(n->isElementNode())
    {
        ElementImpl *e = static_cast<ElementImpl *>(n);
        e->setAttributeMap(t->attrs);

        // take care of optional close tags
        if(endTagRequirement(e->id()) == DOM::OPTIONAL)
            popBlock(t->id);
            
        if (isHeaderTag(t->id))
            // Do not allow two header tags to be nested if the intervening tags are inlines.
            popNestedHeaderTag();
    }

    // if this tag is forbidden inside the current context, pop
    // blocks until we are allowed to add it...
    while (blockStack && t->id <= ID_LAST_TAG && forbiddenTag[t->id]) {
#ifdef PARSER_DEBUG
        kdDebug( 6035 ) << "t->id: " << t->id << " is forbidden :-( " << endl;
#endif
        popOneBlock();
    }

    if (!insertNode(n, t->flat)) 
    {
        // we couldn't insert the node...
        
        if(n->isElementNode())
        {
            ElementImpl *e = static_cast<ElementImpl *>(n);
            e->setAttributeMap(0);
        }
            
#ifdef PARSER_DEBUG
        kdDebug( 6035 ) << "insertNode failed current=" << current->id() << ", new=" << n->id() << "!" << endl;
#endif
        if (map == n)
        {
#ifdef PARSER_DEBUG
            kdDebug( 6035 ) << "  --> resetting map!" << endl;
#endif
            map = 0;
        }
        if (form == n)
        {
#ifdef PARSER_DEBUG
            kdDebug( 6035 ) << "   --> resetting form!" << endl;
#endif
            form = 0;
        }
    }
}

static bool isTableRelatedTag(int id)
{
    return (id == ID_TR || id == ID_TD || id == ID_TABLE || id == ID_TBODY || id == ID_TFOOT || id == ID_THEAD ||
            id == ID_TH);
}

bool KHTMLParser::insertNode(NodeImpl *n, bool flat)
{
    Node protectNode(n);

    int id = n->id();

    // let's be stupid and just try to insert it.
    // this should work if the document is wellformed
#ifdef PARSER_DEBUG
    NodeImpl *tmp = current;
#endif
    NodeImpl *newNode = current->addChild(n);
    if ( newNode ) {
#ifdef PARSER_DEBUG
        kdDebug( 6035 ) << "added " << n->nodeName().string() << " to " << tmp->nodeName().string() << ", new current=" << newNode->nodeName().string() << endl;
#endif
        // don't push elements without end tag on the stack
        if(tagPriority(id) != 0 && !flat)
        {
            pushBlock(id, tagPriority(id));
            if (newNode == current)
                popBlock(id);
            else
                setCurrent(newNode);
#if SPEED_DEBUG < 2
            if(!n->attached() && HTMLWidget)
                n->attach();
#endif
        }
        else {
#if SPEED_DEBUG < 2
            if(!n->attached() && HTMLWidget)
                n->attach();
            if (n->maintainsState()) {
                doc()->registerMaintainsState(n);
                QStringList &states = doc()->restoreState();
                if (!states.isEmpty())
                    n->restoreState(states);
            }
            n->closeRenderer();
#endif
        }

        return true;
    } else {
#ifdef PARSER_DEBUG
        kdDebug( 6035 ) << "ADDING NODE FAILED!!!! current = " << current->nodeName().string() << ", new = " << n->nodeName().string() << endl;
#endif
        // error handling...
        HTMLElementImpl *e;
        bool handled = false;

        // switch according to the element to insert
        switch(id)
        {
        case ID_TR:
        case ID_TH:
        case ID_TD:
            if (inStrayTableContent && !isTableRelatedTag(current->id())) {
                // pop out to the nearest enclosing table-related tag.
                while (blockStack && !isTableRelatedTag(current->id()))
                    popOneBlock();
                return insertNode(n);
            }
            break;
        case ID_COMMENT:
            break;
        case ID_HEAD:
            // ### alllow not having <HTML> in at all, as per HTML spec
            if (!current->isDocumentNode() && current->id() != ID_HTML )
                return false;
            break;
            // We can deal with a base, meta and link element in the body, by just adding the element to head.
        case ID_META:
        case ID_LINK:
        case ID_BASE:
            if( !head )
                createHead();
            if( head ) {
                if ( head->addChild(n) ) {
#if SPEED_DEBUG < 2
                    if(!n->attached() && HTMLWidget)
                        n->attach();
#endif
                    return true;
		} else {
		    return false;
		}
            }
            break;
        case ID_HTML:
            if (!current->isDocumentNode() ) {
		if ( doc()->firstChild()->id() == ID_HTML) {
		    // we have another <HTML> element.... apply attributes to existing one
		    // make sure we don't overwrite already existing attributes
		    NamedAttrMapImpl *map = static_cast<ElementImpl*>(n)->attributes(true);
		    NamedAttrMapImpl *bmap = static_cast<ElementImpl*>(doc()->firstChild())->attributes(false);
		    bool changed = false;
		    for (unsigned long l = 0; map && l < map->length(); ++l) {
			AttributeImpl* it = map->attributeItem(l);
			changed = !bmap->getAttributeItem(it->id());
			bmap->insertAttribute(it->clone(false));
		    }
		    if ( changed )
			doc()->recalcStyle( NodeImpl::Inherit );
		}
                return false;
	    }
            break;
        case ID_TITLE:
        case ID_STYLE:
            if ( !head )
                createHead();
            if ( head ) {
                DOM::NodeImpl *newNode = head->addChild(n);
                if ( newNode ) {
                    pushBlock(id, tagPriority(id));
                    setCurrent(newNode);
#if SPEED_DEBUG < 2
		    if(!n->attached() && HTMLWidget)
                        n->attach();
#endif
                } else {
#ifdef PARSER_DEBUG
                    kdDebug( 6035 ) << "adding style before to body failed!!!!" << endl;
#endif
                    discard_until = ID_STYLE + ID_CLOSE_TAG;
                    return false;
                }
                return true;
            } else if(inBody) {
                discard_until = ID_STYLE + ID_CLOSE_TAG;
                return false;
            }
            break;
            // SCRIPT and OBJECT are allowed in the body.
        case ID_BODY:
            if(inBody && doc()->body()) {
                // we have another <BODY> element.... apply attributes to existing one
                // make sure we don't overwrite already existing attributes
                // some sites use <body bgcolor=rightcolor>...<body bgcolor=wrongcolor>
                NamedAttrMapImpl *map = static_cast<ElementImpl*>(n)->attributes(true);
                NamedAttrMapImpl *bmap = doc()->body()->attributes(false);
                bool changed = false;
                for (unsigned long l = 0; map && l < map->length(); ++l) {
                    AttributeImpl* it = map->attributeItem(l);
                    changed = !bmap->getAttributeItem(it->id());
                    bmap->insertAttribute(it->clone(false));
                }
                if ( changed )
                    doc()->recalcStyle( NodeImpl::Inherit );
            } else if ( current->isDocumentNode() )
                break;
            return false;
            break;

            // the following is a hack to move non rendered elements
            // outside of tables.
            // needed for broken constructs like <table><form ...><tr>....
        case ID_INPUT:
        {
            ElementImpl *e = static_cast<ElementImpl *>(n);
            DOMString type = e->getAttribute(ATTR_TYPE);

            if ( strcasecmp( type, "hidden" ) == 0 && form) {
                form->addChild(n);
#if SPEED_DEBUG < 2
                if(!n->attached() && HTMLWidget)
                    n->attach();
#endif
                return true;
            }
            break;
        }
        case ID_TEXT:
            // ignore text inside the following elements.
            switch(current->id())
            {
            case ID_SELECT:
                return false;
            default:
                ;
                // fall through!!
            };
            break;
        case ID_DD:
        case ID_DT:
            e = new HTMLDListElementImpl(document);
            if ( insertNode(e) ) {
                insertNode(n);
                return true;
            }
            break;
        case ID_AREA:
        {
            if(map)
            {
                map->addChild(n);
#if SPEED_DEBUG < 2
                if(!n->attached() && HTMLWidget)
                    n->attach();
#endif
                handled = true;
            }
            else
                return false;
            return true;
        }
        case ID_CAPTION: {
            switch (current->id()) {
                case ID_THEAD:
                case ID_TBODY:
                case ID_TFOOT:
                case ID_TR:
                case ID_TH:
                case ID_TD: {
                    NodeImpl* tsection = current;
                    if (current->id() == ID_TR)
                        tsection = current->parent();
                    else if (current->id() == ID_TD || current->id() == ID_TH)
                        tsection = current->parent()->parent();
                    NodeImpl* table = tsection->parent();
                    int exceptioncode = 0;
                    table->insertBefore(n, tsection, exceptioncode);
                    pushBlock(id, tagPriority(id));
                    setCurrent(n);
                    inStrayTableContent++;
                    blockStack->strayTableContent = true;
                    return true;
                }
                default:
                    break;
            }
            break;
        }
        case ID_THEAD:
        case ID_TBODY:
        case ID_TFOOT:
        case ID_COLGROUP: {
            if (isTableRelatedTag(current->id())) {
                while (blockStack && current->id() != ID_TABLE && isTableRelatedTag(current->id()))
                    popOneBlock();
                return insertNode(n);
            }
        }
        default:
            break;
        }

        // switch on the currently active element
        switch(current->id())
        {
        case ID_HTML:
            switch(id)
            {
            case ID_SCRIPT:
            case ID_STYLE:
            case ID_META:
            case ID_LINK:
            case ID_OBJECT:
            case ID_EMBED:
            case ID_TITLE:
            case ID_ISINDEX:
            case ID_BASE:
                if(!head) {
                    head = new HTMLHeadElementImpl(document);
                    e = head;
                    insertNode(e);
                    handled = true;
                }
                break;
            case ID_TEXT: {
                TextImpl *t = static_cast<TextImpl *>(n);
                if (t->containsOnlyWhitespace())
                    return false;
                /* Fall through to default */
            }
            default:
                if ( haveFrameSet ) break;
                e = new HTMLBodyElementImpl(document);
                startBody();
                insertNode(e);
                handled = true;
                break;
            }
            break;
        case ID_HEAD:
            // we can get here only if the element is not allowed in head.
            if (id == ID_HTML)
                return false;
            else {
                // This means the body starts here...
                if ( haveFrameSet ) break;
                popBlock(ID_HEAD);
                e = new HTMLBodyElementImpl(document);
                startBody();
                insertNode(e);
                handled = true;
            }
            break;
        case ID_BODY:
            break;
        case ID_CAPTION:
            // Illegal content in a caption. Close the caption and try again.
            popBlock(ID_CAPTION);
            switch( id ) {
            case ID_THEAD:
            case ID_TFOOT:
            case ID_TBODY:
            case ID_TR:
            case ID_TD:
            case ID_TH:
                return insertNode(n, flat);
            }
            break;
        case ID_TABLE:
        case ID_THEAD:
        case ID_TFOOT:
        case ID_TBODY:
        case ID_TR:
            switch(id)
            {
            case ID_TABLE:
                popBlock(ID_TABLE); // end the table
                handled = true;      // ...and start a new one
                break;
	    case ID_TEXT:
            {
                TextImpl *t = static_cast<TextImpl *>(n);
                if (t->containsOnlyWhitespace())
                    return false;
                DOMStringImpl *i = t->string();
                unsigned int pos = 0;
                while(pos < i->l && ( *(i->s+pos) == QChar(' ') ||
                                      *(i->s+pos) == QChar(0xa0))) pos++;
                if(pos == i->l)
                    break;
            }
            default:
            {
                NodeImpl *node = current;
                NodeImpl *parent = node->parentNode();

                NodeImpl *parentparent = parent->parentNode();

                if (n->isTextNode() ||
                    ( node->id() == ID_TR &&
                     ( parent->id() == ID_THEAD ||
                      parent->id() == ID_TBODY ||
                      parent->id() == ID_TFOOT ) && parentparent->id() == ID_TABLE ) ||
                    ( !checkChild(ID_TR, id, !doc()->inCompatMode()) && ( node->id() == ID_THEAD || node->id() == ID_TBODY || node->id() == ID_TFOOT ) &&
                     parent->id() == ID_TABLE ))
                {
                    node = (node->id() == ID_TABLE) ? node :
                            ((node->id() == ID_TR) ? parentparent : parent);
                    NodeImpl *parent = node->parentNode();
                    int exceptioncode = 0;
                    parent->insertBefore( n, node, exceptioncode );
                    if ( exceptioncode ) {
#ifdef PARSER_DEBUG
                        kdDebug( 6035 ) << "adding content before table failed!" << endl;
#endif
                        break;
                    }
                    if (n->isElementNode() && tagPriority(id) != 0 && 
                        !flat && endTagRequirement(id) != DOM::FORBIDDEN)
                    {
                        pushBlock(id, tagPriority(id));
                        setCurrent(n);
                        inStrayTableContent++;
                        blockStack->strayTableContent = true;
                    }
                    return true;
                }

                if ( current->id() == ID_TR )
                    e = new HTMLTableCellElementImpl(document, ID_TD);
                else if ( current->id() == ID_TABLE )
                    e = new HTMLTableSectionElementImpl( document, ID_TBODY, true /* implicit */ );
                else
                    e = new HTMLTableRowElementImpl( document );
                
                insertNode(e);
                handled = true;
                break;
            } // end default
            } // end switch
            break;
        case ID_OBJECT:
            discard_until = ID_OBJECT + ID_CLOSE_TAG;
            return false;
        case ID_UL:
        case ID_OL:
        case ID_DIR:
        case ID_MENU:
            e = new HTMLDivElementImpl(document);
            insertNode(e);
            handled = true;
            break;
            case ID_DL:
                popBlock(ID_DL);
                handled = true;
                break;
            case ID_DT:
                popBlock(ID_DT);
                handled = true;
                break;
        case ID_SELECT:
            if( n->isInline() )
                return false;
            break;
        case ID_P:
        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
            if(!n->isInline())
            {
                popBlock(current->id());
                handled = true;
            }
            break;
        case ID_OPTION:
        case ID_OPTGROUP:
            if (id == ID_OPTGROUP)
            {
                popBlock(current->id());
                handled = true;
            }
            else if(id == ID_SELECT)
            {
                // IE treats a nested select as </select>. Let's do the same
                popBlock( ID_SELECT );
                break;
            }
            break;
            // head elements in the body should be ignored.
        case ID_ADDRESS:
            popBlock(ID_ADDRESS);
            handled = true;
            break;
        case ID_COLGROUP:
            if (id != ID_TEXT) {
                popBlock(ID_COLGROUP);
                handled = true;
            }
            break;
        case ID_FONT:
            popBlock(ID_FONT);
            handled = true;
            break;
        default:
            if(current->isDocumentNode())
            {
                if(current->firstChild() == 0) {
                    e = new HTMLHtmlElementImpl(document);
                    insertNode(e);
                    handled = true;
                }
            }
            else if(current->isInline())
            {
                popInlineBlocks();
                handled = true;
            }
        }

        // if we couldn't handle the error, just rethrow the exception...
        if(!handled)
        {
            //kdDebug( 6035 ) << "Exception handler failed in HTMLPArser::insertNode()" << endl;
            return false;
        }

        return insertNode(n);
    }
}

NodeImpl *KHTMLParser::getElement(Token* t)
{
    switch (t->id)
    {
    case ID_HEAD:
        if (!head && current->id() == ID_HTML) {
            head = new HTMLHeadElementImpl(document);
            return head;
        }
        return 0;
    case ID_BODY:
        // body no longer allowed if we have a frameset
        if (haveFrameSet)
            return 0;
        popBlock(ID_HEAD);
        startBody();
        return new HTMLBodyElementImpl(document);

// frames
    case ID_FRAMESET:
        popBlock(ID_HEAD);
        if (inBody && !haveFrameSet && !haveContent) {
            popBlock(ID_BODY);
            // ### actually for IE document.body returns the now hidden "body" element
            // we can't implement that behaviour now because it could cause too many
            // regressions and the headaches are not worth the work as long as there is
            // no site actually relying on that detail (Dirk)
            if (doc()->body())
                doc()->body()->setAttribute(ATTR_STYLE, "display:none");
            inBody = false;
        }
        if ((haveContent || haveFrameSet) && current->id() == ID_HTML)
            return 0;
        haveFrameSet = true;
        startBody();
        return new HTMLFrameSetElementImpl(document);

    // a bit of a special case, since the frame is inlined
    case ID_IFRAME:
        discard_until = ID_IFRAME + ID_CLOSE_TAG;
        break;

// form elements
    case ID_FORM:
        // Only create a new form if we're not already inside one.
        // This is consistent with other browsers' behavior.
        if (form)
            return 0;
        form = new HTMLFormElementImpl(document);
        return form;
    case ID_BUTTON:
        return new HTMLButtonElementImpl(document, form);
    case ID_FIELDSET:
        return new HTMLFieldSetElementImpl(document, form);
    case ID_INPUT:
        return new HTMLInputElementImpl(document, form);
    case ID_ISINDEX: {
        NodeImpl *n = handleIsindex(t);
        if (!inBody) {
            if (isindex)
                isindex->deref();
            isindex = n;
            isindex->ref();
            return 0;
        }
        t->flat = true;
        return n;
    }
    case ID_KEYGEN:
        return new HTMLKeygenElementImpl(document, form);
    case ID_LEGEND:
        return new HTMLLegendElementImpl(document, form);
    case ID_OPTGROUP:
        return new HTMLOptGroupElementImpl(document, form);
    case ID_OPTION:
        return new HTMLOptionElementImpl(document, form);
    case ID_SELECT:
        inSelect = true;
        return new HTMLSelectElementImpl(document, form);
    case ID_TEXTAREA:
        return new HTMLTextAreaElementImpl(document, form);

// lists
    case ID_DD:
        popBlock(ID_DT);
        popBlock(ID_DD);
        break;
    case ID_DT:
        popBlock(ID_DD);
        popBlock(ID_DT);
        break;
    case ID_LI:
        popBlock(ID_LI);
        break;

// anchor
    case ID_A:
        // Never allow nested <a>s.
        popBlock(ID_A);
        break;

// images
    case ID_IMG:
        return new HTMLImageElementImpl(document, form);
    case ID_MAP:
        map = new HTMLMapElementImpl(document);
        return map;

// tables
    case ID_TR:
        popBlock(ID_TR);
        break;
    case ID_TD:
    case ID_TH:
        popBlock(ID_TH);
        popBlock(ID_TD);
        break;
    case ID_TBODY:
    case ID_THEAD:
    case ID_TFOOT:
        popBlock(ID_THEAD);
        popBlock(ID_TBODY);
        popBlock(ID_TFOOT);
        break;

// elements with no special representation in the DOM
    case ID_TT:
    case ID_U:
    case ID_B:
    case ID_I:
    case ID_S:
    case ID_STRIKE:
    case ID_BIG:
    case ID_SMALL:
        if (!allowNestedRedundantTag(t->id))
            return 0;
        break;

    case ID_NOBR:
    case ID_WBR:
        popBlock(t->id); // Don't allow nested <nobr> or <wbr>
        break;

// these are special, and normally not rendered
    case ID_NOEMBED:
        discard_until = ID_NOEMBED + ID_CLOSE_TAG;
        return 0;
    case ID_NOFRAMES:
        discard_until = ID_NOFRAMES + ID_CLOSE_TAG;
        return 0;
    case ID_NOSCRIPT:
        if (HTMLWidget && HTMLWidget->part()->jScriptEnabled())
            discard_until = ID_NOSCRIPT + ID_CLOSE_TAG;
        return 0;
    case ID_NOLAYER:
        //discard_until = ID_NOLAYER + ID_CLOSE_TAG;
        return 0;
    case ID_TEXT:
        return new TextImpl(document, t->text);
    case ID_COMMENT:
        if (!includesCommentsInDOM)
            return 0;
        break;
    }

    return document->document()->createHTMLElement(t->id);
}

#define MAX_REDUNDANT 20

bool KHTMLParser::allowNestedRedundantTag(int _id)
{
    // www.liceo.edu.mx is an example of a site that achieves a level of nesting of
    // about 1500 tags, all from a bunch of <b>s.  We will only allow at most 20
    // nested tags of the same type before just ignoring them all together.
    int i = 0;
    for (HTMLStackElem* curr = blockStack;
         i < MAX_REDUNDANT && curr && curr->id == _id;
         curr = curr->next, i++);
    return i != MAX_REDUNDANT;
}

void KHTMLParser::processCloseTag(Token *t)
{
    // support for really broken html. Can't believe I'm supporting such crap (lars)
    switch(t->id)
    {
    case ID_HTML+ID_CLOSE_TAG:
    case ID_BODY+ID_CLOSE_TAG:
        // we never close the body tag, since some stupid web pages close it before the actual end of the doc.
        // let's rely on the end() call to close things.
        return;
    case ID_FORM+ID_CLOSE_TAG:
        form = 0;
        // this one is to get the right style on the body element
        break;
    case ID_MAP+ID_CLOSE_TAG:
        map = 0;
        break;
    case ID_SELECT+ID_CLOSE_TAG:
        inSelect = false;
        break;
    default:
        break;
    }

#ifdef PARSER_DEBUG
    kdDebug( 6035 ) << "added the following childs to " << current->nodeName().string() << endl;
    NodeImpl *child = current->firstChild();
    while(child != 0)
    {
        kdDebug( 6035 ) << "    " << child->nodeName().string() << endl;
        child = child->nextSibling();
    }
#endif
    HTMLStackElem* oldElem = blockStack;
    popBlock(t->id-ID_CLOSE_TAG);
    if (oldElem == blockStack && t->id == ID_P+ID_CLOSE_TAG) {
        // We encountered a stray </p>.  Amazingly Gecko, WinIE, and MacIE all treat
        // this as a valid break, i.e., <p></p>.  So go ahead and make the empty
        // paragraph.
        t->id-=ID_CLOSE_TAG;
        parseToken(t);
        popBlock(ID_P);
    }
#ifdef PARSER_DEBUG
    kdDebug( 6035 ) << "closeTag --> current = " << current->nodeName().string() << endl;
#endif
}

bool KHTMLParser::isHeaderTag(int _id)
{
    switch (_id) {
        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
            return true;
        default:
            return false;
    }
}

void KHTMLParser::popNestedHeaderTag()
{
    // This function only cares about checking for nested headers that have only inlines in between them.
    NodeImpl* currNode = current;
    for (HTMLStackElem* curr = blockStack; curr; curr = curr->next) {
        if (isHeaderTag(curr->id)) {
            popBlock(curr->id);
            return;
        }
        if (currNode && !currNode->isInline())
            return;
        currNode = curr->node;
    }
}

bool KHTMLParser::isResidualStyleTag(int _id)
{
    switch (_id) {
        case ID_A:
        case ID_FONT:
        case ID_TT:
        case ID_U:
        case ID_B:
        case ID_I:
        case ID_S:
        case ID_STRIKE:
        case ID_BIG:
        case ID_SMALL:
        case ID_EM:
        case ID_STRONG:
        case ID_DFN:
        case ID_CODE:
        case ID_SAMP:
        case ID_KBD:
        case ID_VAR:
            return true;
        default:
            return false;
    }
}

bool KHTMLParser::isAffectedByResidualStyle(int _id)
{
    if (isResidualStyleTag(_id))
        return true;
    
    switch (_id) {
        case ID_P:
        case ID_DIV:
        case ID_BLOCKQUOTE:
        case ID_ADDRESS:
        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
        case ID_CENTER:
        case ID_UL:
        case ID_OL:
        case ID_LI:
        case ID_DL:
        case ID_DT:
        case ID_DD:
        case ID_PRE:
            return true;
        default:
            return false;
    }
}

void KHTMLParser::handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem* elem)
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

    if (!curr || !maxElem || !isAffectedByResidualStyle(maxElem->id)) return;

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
            if (!isResidualStyleTag(currElem->id)) {
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
            if (isResidualStyleTag(currElem->node->id())) {
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
        if (isResidualStyleTag(curr->id)) {
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

void KHTMLParser::reopenResidualStyleTags(HTMLStackElem* elem, DOM::NodeImpl* malformedTableParent)
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
        pushBlock(elem->id, elem->level);

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

void KHTMLParser::pushBlock(int _id, int _level)
{
    HTMLStackElem *Elem = new HTMLStackElem(_id, _level, current, blockStack);

    blockStack = Elem;
    addForbidden(_id, forbiddenTag);
}

void KHTMLParser::popBlock( int _id )
{
    HTMLStackElem *Elem = blockStack;
    
    int maxLevel = 0;

#ifdef PARSER_DEBUG
    kdDebug( 6035 ) << "popBlock(" << getTagName(_id).string() << ")" << endl;
    while(Elem) {
        kdDebug( 6035) << "   > " << getTagName(Elem->id).string() << endl;
        Elem = Elem->next;
    }
    Elem = blockStack;
#endif

    while( Elem && (Elem->id != _id))
    {
        if (maxLevel < Elem->level)
        {
            maxLevel = Elem->level;
        }
        Elem = Elem->next;
    }

    if (!Elem)
        return;

    if (maxLevel > Elem->level) {
        // We didn't match because the tag is in a different scope, e.g.,
        // <b><p>Foo</b>.  Try to correct the problem.
        if (!isResidualStyleTag(_id))
            return;
        return handleResidualStyleCloseTagAcrossBlocks(Elem);
    }

    bool isAffectedByStyle = isAffectedByResidualStyle(Elem->id);
    HTMLStackElem* residualStyleStack = 0;
    NodeImpl* malformedTableParent = 0;
    
    Elem = blockStack;
    while (Elem)
    {
        if (Elem->id == _id)
        {
            int strayTable = inStrayTableContent;
            popOneBlock();
            Elem = 0;

            // This element was the root of some malformed content just inside an implicit or
            // explicit <tbody> or <tr>.
            // If we end up needing to reopen residual style tags, the root of the reopened chain
            // must also know that it is the root of malformed content inside a <tbody>/<tr>.
            if (strayTable && (inStrayTableContent < strayTable) && residualStyleStack) {
                NodeImpl* curr = current;
                while (curr && curr->id() != ID_TABLE)
                    curr = curr->parentNode();
                malformedTableParent = curr ? curr->parentNode() : 0;
            }
        }
        else
        {
            if (Elem->id == ID_FORM && form)
                // A <form> is being closed prematurely (and this is
                // malformed HTML).  Set an attribute on the form to clear out its
                // bottom margin.
                form->setMalformed(true);

            // Schedule this tag for reopening
            // after we complete the close of this entire block.
            NodeImpl* currNode = current;
            if (isAffectedByStyle && isResidualStyleTag(Elem->id)) {
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

void KHTMLParser::popOneBlock(bool delBlock)
{
    HTMLStackElem *Elem = blockStack;

    // we should never get here, but some bad html might cause it.
#ifndef PARSER_DEBUG
    if(!Elem) return;
#else
    kdDebug( 6035 ) << "popping block: " << getTagName(Elem->id).string() << "(" << Elem->id << ")" << endl;
#endif

#if SPEED_DEBUG < 1
    if((Elem->node != current)) {
        if (current->maintainsState() && doc()){
            doc()->registerMaintainsState(current);
            QStringList &states = doc()->restoreState();
            if (!states.isEmpty())
                current->restoreState(states);
        }
        
        // A few elements (<applet>, <object>) need to know when all child elements (<param>s) are available:
        current->closeRenderer();
    }
#endif

    removeForbidden(Elem->id, forbiddenTag);

    blockStack = Elem->next;
    setCurrent(Elem->node);

    if (Elem->strayTableContent)
        inStrayTableContent--;
    
    if (delBlock)
        delete Elem;
}

void KHTMLParser::popInlineBlocks()
{
    while (blockStack && current->isInline())
        popOneBlock();
}

void KHTMLParser::freeBlock()
{
    while (blockStack)
        popOneBlock();
}

void KHTMLParser::createHead()
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

NodeImpl *KHTMLParser::handleIsindex( Token *t )
{
    NodeImpl *n;
    HTMLFormElementImpl *myform = form;
    if ( !myform ) {
        myform = new HTMLFormElementImpl(document);
        n = myform;
    } else
        n = new HTMLDivElementImpl( document );
    NodeImpl *child = new HTMLHRElementImpl( document );
    n->addChild( child );
    AttributeImpl* a = t->attrs ? t->attrs->getAttributeItem(ATTR_PROMPT) : 0;
#if APPLE_CHANGES
    DOMString text = searchableIndexIntroduction();
#else
    DOMString text = i18n("This is a searchable index. Enter search keywords: ");
#endif
    if (a)
        text = DOMString(a->value()) + " ";
    child = new TextImpl(document, text);
    n->addChild( child );
    child = new HTMLIsIndexElementImpl(document, myform);
    static_cast<ElementImpl *>(child)->setAttribute(ATTR_TYPE, "khtml_isindex");
    n->addChild( child );
    child = new HTMLHRElementImpl( document );
    n->addChild( child );

    return n;
}

void KHTMLParser::startBody()
{
    if(inBody) return;

    inBody = true;

    if( isindex ) {
        insertNode( isindex, true /* don't decend into this node */ );
        isindex = 0;
    }
}

void KHTMLParser::finished()
{
    // In the case of a completely empty document, here's the place to create the HTML element.
    if (current->isDocumentNode() && current->firstChild() == 0) {
        insertNode(new HTMLHtmlElementImpl(document));
    }

    // This ensures that "current" is not left pointing to a node when the document is destroyed.
    freeBlock();
    setCurrent(0);
}
