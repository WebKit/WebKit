/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
// -------------------------------------------------------------------------
//#define DEBUG
//#define DEBUG_LAYOUT
//#define PAR_DEBUG
//#define EVENT_DEBUG
//#define UNSUPPORTED_ATTR

#include "html/dtd.h"
#include "html/html_elementimpl.h"
#include "html/html_documentimpl.h"
#include "html/htmltokenizer.h"

#include "misc/htmlhashes.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "rendering/render_object.h"
#include "rendering/render_replaced.h"
#include "css/css_valueimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"

#include <kdebug.h>

using namespace DOM;
using namespace khtml;

HTMLElementImpl::HTMLElementImpl(DocumentPtr *doc)
    : ElementImpl(doc)
{
}

HTMLElementImpl::~HTMLElementImpl()
{
}

bool HTMLElementImpl::isInline() const
{
    if (renderer())
        return ElementImpl::isInline();
    
    switch(id()) {
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
    
            // %phrase
        case ID_EM:
        case ID_STRONG:
        case ID_DFN:
        case ID_CODE:
        case ID_SAMP:
        case ID_KBD:
        case ID_VAR:
        case ID_CITE:
        case ID_ABBR:
        case ID_ACRONYM:
    
            // %special
        case ID_SUB:
        case ID_SUP:
        case ID_SPAN:
        case ID_NOBR:
        case ID_WBR:
            return true;
            
        default:
            return ElementImpl::isInline();
    }
}

void HTMLElementImpl::parseAttribute(AttributeImpl *attr)
{
    DOMString indexstring;
    switch( attr->id() )
    {
    case ATTR_ALIGN:
        if (attr->val()) {
            if ( strcasecmp(attr->value(), "middle" ) == 0 )
                addCSSProperty( CSS_PROP_TEXT_ALIGN, "center" );
            else
                addCSSProperty(CSS_PROP_TEXT_ALIGN, attr->value());
        }
        else
            removeCSSProperty(CSS_PROP_TEXT_ALIGN);
        break;
// the core attributes...
    case ATTR_ID:
        // unique id
        setHasID(attr->val());
        setChanged();
        break;
    case ATTR_CLASS:
        // class
        setHasClass(attr->val());
        setChanged();
        break;
    case ATTR_STYLE:
        // ### we need to remove old style info in case there was any!
        // ### the inline sheet ay contain more than 1 property!
        // stylesheet info
        setHasStyle();
        if(!m_styleDecls) createDecl();
        m_styleDecls->setProperty(attr->value());
        setChanged();
        break;
    case ATTR_TABINDEX:
        indexstring=getAttribute(ATTR_TABINDEX);
        if (indexstring.length())
            setTabIndex(indexstring.toInt());
        break;
// i18n attributes
    case ATTR_LANG:
        break;
    case ATTR_DIR:
        addCSSProperty(CSS_PROP_DIRECTION, attr->value());
        addCSSProperty(CSS_PROP_UNICODE_BIDI, CSS_VAL_EMBED);
        break;
// standard events
    case ATTR_ONCLICK:
	setHTMLEventListener(EventImpl::KHTML_CLICK_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDBLCLICK:
	setHTMLEventListener(EventImpl::KHTML_DBLCLICK_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONMOUSEDOWN:
        setHTMLEventListener(EventImpl::MOUSEDOWN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONMOUSEMOVE:
        setHTMLEventListener(EventImpl::MOUSEMOVE_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONMOUSEOUT:
        setHTMLEventListener(EventImpl::MOUSEOUT_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONMOUSEOVER:
        setHTMLEventListener(EventImpl::MOUSEOVER_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONMOUSEUP:
        setHTMLEventListener(EventImpl::MOUSEUP_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::DOMFOCUSIN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONKEYDOWN:
        setHTMLEventListener(EventImpl::KHTML_KEYDOWN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
	break;
    case ATTR_ONKEYPRESS:
        setHTMLEventListener(EventImpl::KHTML_KEYPRESS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
	break;
    case ATTR_ONKEYUP:
        setHTMLEventListener(EventImpl::KHTML_KEYUP_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
// other misc attributes
    default:
#ifdef UNSUPPORTED_ATTR
	kdDebug(6030) << "UATTR: <" << this->nodeName().string() << "> ["
		      << attr->name().string() << "]=[" << attr->value().string() << "]" << endl;
#endif
        break;
    }
}

void HTMLElementImpl::addCSSProperty(int id, const DOMString &value)
{
    if(!m_styleDecls) createDecl();
    m_styleDecls->setProperty(id, value, false, true);
    setChanged();
}

void HTMLElementImpl::addCSSProperty(int id, int value)
{
    if(!m_styleDecls) createDecl();
    m_styleDecls->setProperty(id, value, false, true);
    setChanged();
}

void HTMLElementImpl::addCSSLength(int id, const DOMString &value)
{
    if(!m_styleDecls) createDecl();

    // strip attribute garbage..
    DOMStringImpl* v = value.implementation();
    if ( v ) {
        unsigned int l = 0;

        while ( l < v->l && v->s[l].unicode() <= ' ') l++;

        for ( ;l < v->l; l++ ) {
            char cc = v->s[l].latin1();
            if ( cc > '9' || ( cc < '0' && cc != '*' && cc != '%' && cc != '.') )
                break;
        }
        if ( l != v->l ) {
            m_styleDecls->setLengthProperty( id, DOMString( v->s, l ), false, true );
            setChanged();
            return;
        }
    }

    m_styleDecls->setLengthProperty(id, value, false, true);
    setChanged();
}

static inline bool isHexDigit( const QChar &c ) {
    return ( c >= '0' && c <= '9' ) ||
	   ( c >= 'a' && c <= 'f' ) ||
	   ( c >= 'A' && c <= 'F' );
}

static inline int toHex( const QChar &c ) {
    return ( (c >= '0' && c <= '9')
             ? (c.unicode() - '0')
             : ( ( c >= 'a' && c <= 'f' )
                 ? (c.unicode() - 'a' + 10)
                 : ( ( c >= 'A' && c <= 'F' )
                     ? (c.unicode() - 'A' + 10)
                     : -1 ) ) );
}

/* color parsing that tries to match as close as possible IE 6. */
void HTMLElementImpl::addHTMLColor( int id, const DOMString &c )
{
    if(!m_styleDecls) createDecl();

    // this is the only case no color gets applied in IE.
    if ( !c.length() )
        return;

    if ( m_styleDecls->setProperty(id, c, false, true) )
        return;

    QString color = c.string();
    // not something that fits the specs.

    // we're emulating IEs color parser here. It maps transparent to black, otherwise it tries to build a rgb value
    // out of everyhting you put in. The algorithm is experimentally determined, but seems to work for all test cases I have.

    // the length of the color value is rounded up to the next
    // multiple of 3. each part of the rgb triple then gets one third
    // of the length.
    //
    // Each triplet is parsed byte by byte, mapping
    // each number to a hex value (0-9a-fA-F to their values
    // everything else to 0).
    //
    // The highest non zero digit in all triplets is remembered, and
    // used as a normalization point to normalize to values between 0
    // and 255.

    if ( color.lower() != "transparent" ) {
        if ( color[0] == '#' )
            color.remove( 0,  1 );
        int basicLength = (color.length() + 2) / 3;
        if ( basicLength > 1 ) {
            // IE ignores colors with three digits or less
            // 	    qDebug("trying to fix up color '%s'. basicLength=%d, length=%d",
            // 		   color.latin1(), basicLength, color.length() );
            int colors[3] = { 0, 0, 0 };
            int component = 0;
            int pos = 0;
            int maxDigit = basicLength-1;
            while ( component < 3 ) {
                // search forward for digits in the string
                int numDigits = 0;
                while ( pos < (int)color.length() && numDigits < basicLength ) {
                    int hex = toHex( color[pos] );
                    colors[component] = (colors[component] << 4);
                    if ( hex > 0 ) {
                        colors[component] += hex;
                        maxDigit = QMIN( maxDigit, numDigits );
                    }
                    numDigits++;
                    pos++;
                }
                while ( numDigits++ < basicLength )
                    colors[component] <<= 4;
                component++;
            }
            maxDigit = basicLength - maxDigit;
            // 	    qDebug("color is %x %x %x, maxDigit=%d",  colors[0], colors[1], colors[2], maxDigit );

            // normalize to 00-ff. The highest filled digit counts, minimum is 2 digits
            maxDigit -= 2;
            colors[0] >>= 4*maxDigit;
            colors[1] >>= 4*maxDigit;
            colors[2] >>= 4*maxDigit;
            // 	    qDebug("normalized color is %x %x %x",  colors[0], colors[1], colors[2] );
            // 	assert( colors[0] < 0x100 && colors[1] < 0x100 && colors[2] < 0x100 );

            color.sprintf("#%02x%02x%02x", colors[0], colors[1], colors[2] );
            // 	    qDebug( "trying to add fixed color string '%s'", color.latin1() );
            if ( m_styleDecls->setProperty(id, DOMString(color), false, true) )
                return;
        }
    }
    m_styleDecls->setProperty(id, CSS_VAL_BLACK, false, true);
}

void HTMLElementImpl::removeCSSProperty(int id)
{
    if(!m_styleDecls)
        return;
    m_styleDecls->parent()->deref();
    m_styleDecls->setParent(getDocument()->elementSheet());
    m_styleDecls->parent()->ref();
    m_styleDecls->removeProperty(id);
    setChanged();
}

DOMString HTMLElementImpl::innerHTML() const
{
    return toHTML();
}

DOMString HTMLElementImpl::innerText() const
{
    DOMString text;

    const NodeImpl *n = this;
    // find the next text/image after the anchor, to get a position
    while(n) {
        if(n->firstChild())
            n = n->firstChild();
        else if(n->nextSibling())
            n = n->nextSibling();
        else {
            NodeImpl *next = 0;
            while(!next) {
                n = n->parentNode();
                if(!n || n == (NodeImpl *)this ) goto end;
                next = n->nextSibling();
            }
            n = next;
        }
        if(n->isTextNode() ) {
            text += static_cast<const TextImpl *>(n)->data();
        }
    }
 end:
    return text;
}

DocumentFragmentImpl *HTMLElementImpl::createContextualFragment( const DOMString &html )
{
    // the following is in accordance with the definition as used by IE
    if( endTag[id()] == FORBIDDEN )
        return NULL;
    // IE disallows innerHTML on inline elements. I don't see why we should have this restriction, as our
    // dhtml engine can cope with it. Lars
    //if ( isInline() ) return false;
    switch( id() ) {
        case ID_COL:
        case ID_COLGROUP:
        case ID_FRAMESET:
        case ID_HEAD:
        case ID_STYLE:
        case ID_TABLE:
        case ID_TBODY:
        case ID_TFOOT:
        case ID_THEAD:
        case ID_TITLE:
            return NULL;
        default:
            break;
    }
    if ( !getDocument()->isHTMLDocument() )
        return NULL;

    DocumentFragmentImpl *fragment = new DocumentFragmentImpl( docPtr() );
    {
        HTMLTokenizer tok( docPtr(), fragment );
        tok.begin();
        tok.write( html.string(), true );
        tok.end();
    }

    // Exceptions are ignored because none ought to happen here.
    int ignoredExceptionCode;

    // we need to pop <html> and <body> elements and remove <head> to
    // accomadate folks passing complete HTML documents to make the
    // child of an element.

    NodeImpl *node = fragment->firstChild(); 
    while (node != NULL) {
	if (node->id() == ID_HTML || node->id() == ID_BODY) {
	    NodeImpl *firstChild = node->firstChild();
	    NodeImpl *child = firstChild; 
	    while (child != NULL) {
		NodeImpl *nextChild = child->nextSibling();
		fragment->insertBefore(child, node, ignoredExceptionCode);
                // FIXME: Does node leak here?
		child = nextChild;
	    }
	    if (firstChild == NULL) {
		NodeImpl *nextNode = node->nextSibling();
		fragment->removeChild(node, ignoredExceptionCode);
                // FIXME: Does node leak here?
                node = nextNode;
	    } else {
		fragment->removeChild(node, ignoredExceptionCode);
                // FIXME: Does node leak here?
		node = firstChild;
	    }
	} else if (node->id() == ID_HEAD) {
	    NodeImpl *nextNode = node->nextSibling();
	    fragment->removeChild(node, ignoredExceptionCode);
            // FIXME: Does node leak here?
	    node = nextNode;
	} else {
	    node = node->nextSibling();
	}
    }

    return fragment;
}

bool HTMLElementImpl::setInnerHTML( const DOMString &html )
{
    DocumentFragmentImpl *fragment = createContextualFragment( html );
    if (fragment == NULL) {
	return false;
    }

    removeChildren();
    int ec = 0;
    appendChild( fragment, ec );
    delete fragment;
    return !ec;
}

bool HTMLElementImpl::setInnerText( const DOMString &text )
{
    // following the IE specs.
    if( endTag[id()] == FORBIDDEN )
        return false;
    // IE disallows innerHTML on inline elements. I don't see why we should have this restriction, as our
    // dhtml engine can cope with it. Lars
    //if ( isInline() ) return false;
    switch( id() ) {
        case ID_COL:
        case ID_COLGROUP:
        case ID_FRAMESET:
        case ID_HEAD:
        case ID_HTML:
        case ID_TABLE:
        case ID_TBODY:
        case ID_TFOOT:
        case ID_THEAD:
        case ID_TR:
            return false;
        default:
            break;
    }

    removeChildren();

    TextImpl *t = new TextImpl( docPtr(), text );
    int ec = 0;
    appendChild( t, ec );
    if ( !ec )
        return true;
    return false;
}

DOMString HTMLElementImpl::namespaceURI() const
{
    // For HTML documents, we treat HTML elements as having no namespace. But for XML documents
    // the elements have the namespace defined in the XHTML spec
    if (getDocument()->isHTMLDocument())
        return DOMString();
    else
        return XHTML_NAMESPACE;
}

void HTMLElementImpl::addHTMLAlignment( DOMString alignment )
{
    //qDebug("alignment is %s", alignment.string().latin1() );
    // vertical alignment with respect to the current baseline of the text
    // right or left means floating images
    int propfloat = -1;
    int propvalign = -1;
    if ( strcasecmp( alignment, "absmiddle" ) == 0 ) {
        propvalign = CSS_VAL_MIDDLE;
    } else if ( strcasecmp( alignment, "absbottom" ) == 0 ) {
        propvalign = CSS_VAL_BOTTOM;
    } else if ( strcasecmp( alignment, "left" ) == 0 ) {
	propfloat = CSS_VAL_LEFT;
	propvalign = CSS_VAL_TOP;
    } else if ( strcasecmp( alignment, "right" ) == 0 ) {
	propfloat = CSS_VAL_RIGHT;
	propvalign = CSS_VAL_TOP;
    } else if ( strcasecmp( alignment, "top" ) == 0 ) {
	propvalign = CSS_VAL_TOP;
    } else if ( strcasecmp( alignment, "middle" ) == 0 ) {
	propvalign = CSS_VAL__KONQ_BASELINE_MIDDLE;
    } else if ( strcasecmp( alignment, "center" ) == 0 ) {
	propvalign = CSS_VAL_MIDDLE;
    } else if ( strcasecmp( alignment, "bottom" ) == 0 ) {
	propvalign = CSS_VAL_BASELINE;
    } else if ( strcasecmp ( alignment, "texttop") == 0 ) {
	propvalign = CSS_VAL_TEXT_TOP;
    }

    if ( propfloat != -1 )
	addCSSProperty( CSS_PROP_FLOAT, propfloat );
    if ( propvalign != -1 )
	addCSSProperty( CSS_PROP_VERTICAL_ALIGN, propvalign );
}


// -------------------------------------------------------------------------
HTMLGenericElementImpl::HTMLGenericElementImpl(DocumentPtr *doc, ushort i)
    : HTMLElementImpl(doc)
{
    _id = i;
}

HTMLGenericElementImpl::~HTMLGenericElementImpl()
{
}

