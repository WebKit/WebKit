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
#include "misc/khtml_text_operations.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "rendering/render_object.h"
#include "rendering/render_replaced.h"
#include "css/css_valueimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/css_ruleimpl.h"
#include "xml/dom_selection.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"

#include <kdebug.h>

using namespace DOM;
using namespace khtml;

CSSMappedAttributeDeclarationImpl::~CSSMappedAttributeDeclarationImpl() {
    if (m_entryType != ePersistent)
        HTMLElementImpl::removeMappedAttributeDecl(m_entryType, m_attrName, m_attrValue);
}

QPtrDict<QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> > >* HTMLElementImpl::m_mappedAttributeDecls = 0;

CSSMappedAttributeDeclarationImpl* HTMLElementImpl::getMappedAttributeDecl(MappedAttributeEntry entryType, AttributeImpl* attr)
{
    if (!m_mappedAttributeDecls)
        return 0;
    
    QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> >* attrNameDict = m_mappedAttributeDecls->find((void*)entryType);
    if (attrNameDict) {
        QPtrDict<CSSMappedAttributeDeclarationImpl>* attrValueDict = attrNameDict->find((void*)attr->id());
        if (attrValueDict)
            return attrValueDict->find(attr->value().implementation());
    }
    return 0;
}

void HTMLElementImpl::setMappedAttributeDecl(MappedAttributeEntry entryType, AttributeImpl* attr, CSSMappedAttributeDeclarationImpl* decl)
{
    if (!m_mappedAttributeDecls)
        m_mappedAttributeDecls = new QPtrDict<QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> > >;
    
    QPtrDict<CSSMappedAttributeDeclarationImpl>* attrValueDict = 0;
    QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> >* attrNameDict = m_mappedAttributeDecls->find((void*)entryType);
    if (!attrNameDict) {
        attrNameDict = new QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> >;
        attrNameDict->setAutoDelete(true);
        m_mappedAttributeDecls->insert((void*)entryType, attrNameDict);
    }
    else
        attrValueDict = attrNameDict->find((void*)attr->id());
    if (!attrValueDict) {
        attrValueDict = new QPtrDict<CSSMappedAttributeDeclarationImpl>;
        if (entryType == ePersistent)
            attrValueDict->setAutoDelete(true);
        attrNameDict->insert((void*)attr->id(), attrValueDict);
    }
    attrValueDict->replace(attr->value().implementation(), decl);
}

void HTMLElementImpl::removeMappedAttributeDecl(MappedAttributeEntry entryType, NodeImpl::Id attrName, const AtomicString& attrValue)
{
    if (!m_mappedAttributeDecls)
        return;
    
    QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> >* attrNameDict = m_mappedAttributeDecls->find((void*)entryType);
    if (!attrNameDict)
        return;
    QPtrDict<CSSMappedAttributeDeclarationImpl>* attrValueDict = attrNameDict->find((void*)attrName);
    if (!attrValueDict)
        return;
    attrValueDict->remove(attrValue.implementation());
}

HTMLAttributeImpl::~HTMLAttributeImpl()
{
    if (m_styleDecl)
        m_styleDecl->deref();
}

AttributeImpl* HTMLAttributeImpl::clone(bool preserveDecl) const
{
    return new HTMLAttributeImpl(m_id, _value, preserveDecl ? m_styleDecl : 0);
}

HTMLNamedAttrMapImpl::HTMLNamedAttrMapImpl(ElementImpl *e)
:NamedAttrMapImpl(e), m_mappedAttributeCount(0)
{}

void HTMLNamedAttrMapImpl::clearAttributes()
{
    m_classList.clear();
    m_mappedAttributeCount = 0;
    NamedAttrMapImpl::clearAttributes();
}

bool HTMLNamedAttrMapImpl::isHTMLAttributeMap() const
{
    return true;
}

int HTMLNamedAttrMapImpl::declCount() const
{
    int result = 0;
    for (uint i = 0; i < length(); i++) {
        HTMLAttributeImpl* attr = attributeItem(i);
        if (attr->decl())
            result++;
    }
    return result;
}

bool HTMLNamedAttrMapImpl::mapsEquivalent(const HTMLNamedAttrMapImpl* otherMap) const
{
    // The # of decls must match.
    if (declCount() != otherMap->declCount())
        return false;
    
    // The values for each decl must match.
    for (uint i = 0; i < length(); i++) {
        HTMLAttributeImpl* attr = attributeItem(i);
        if (attr->decl()) {
            AttributeImpl* otherAttr = otherMap->getAttributeItem(attr->id());
            if (!otherAttr || (attr->value() != otherAttr->value()))
                return false;
        }
    }
    return true;
}

void HTMLNamedAttrMapImpl::parseClassAttribute(const DOMString& classStr)
{
    m_classList.clear();
    if (!element->hasClass())
        return;
    
    DOMString classAttr = element->getDocument()->inCompatMode() ? 
        (classStr.implementation()->isLower() ? classStr : DOMString(classStr.implementation()->lower())) :
        classStr;
    
    if (classAttr.find(' ') == -1)
        m_classList.setString(AtomicString(classAttr));
    else {
        QString val = classAttr.string();
        QStringList list = QStringList::split(' ', val);
        
        AtomicStringList* curr = 0;
        for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
        {
            const QString& singleClass = *it;
            if (!singleClass.isEmpty()) {
                if (curr) {
                    curr->setNext(new AtomicStringList(AtomicString(singleClass)));
                    curr = curr->next();
                }
                else {
                    m_classList.setString(AtomicString(singleClass));
                    curr = &m_classList;
                }
            }
        }
    }
}

// ------------------------------------------------------------------

HTMLElementImpl::HTMLElementImpl(DocumentPtr *doc)
    : ElementImpl(doc)
{
    m_inlineStyleDecl = 0;
}

HTMLElementImpl::~HTMLElementImpl()
{
    if (m_inlineStyleDecl) {
        m_inlineStyleDecl->setParent(0);
        m_inlineStyleDecl->deref();
    }
}

AttributeImpl* HTMLElementImpl::createAttribute(NodeImpl::Id id, DOMStringImpl* value)
{
    return new HTMLAttributeImpl(id, value);
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

void HTMLElementImpl::createInlineStyleDecl()
{
    m_inlineStyleDecl = new CSSStyleDeclarationImpl(0);
    m_inlineStyleDecl->ref();
    m_inlineStyleDecl->setParent(getDocument()->elementSheet());
    m_inlineStyleDecl->setNode(this);
    m_inlineStyleDecl->setStrictParsing(!getDocument()->inCompatMode());
}

void HTMLElementImpl::attributeChanged(AttributeImpl* attr, bool preserveDecls)
{
    HTMLAttributeImpl* htmlAttr = static_cast<HTMLAttributeImpl*>(attr);
    if (htmlAttr->decl() && !preserveDecls) {
        htmlAttr->setDecl(0);
        setChanged();
        if (namedAttrMap)
            static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->declRemoved();
    }

    bool checkDecl = true;
    MappedAttributeEntry entry;
    bool needToParse = mapToEntry(attr->id(), entry);
    if (preserveDecls) {
        if (htmlAttr->decl()) {
            setChanged();
            if (namedAttrMap)
                static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->declAdded();
            checkDecl = false;
        }
    }
    else if (!attr->isNull() && entry != eNone) {
        CSSMappedAttributeDeclarationImpl* decl = getMappedAttributeDecl(entry, attr);
        if (decl) {
            htmlAttr->setDecl(decl);
            setChanged();
            if (namedAttrMap)
                static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->declAdded();
            checkDecl = false;
        } else
            needToParse = true;
    }

    if (needToParse)
        parseHTMLAttribute(htmlAttr);
    
    if (checkDecl && htmlAttr->decl()) {
        // Add the decl to the table in the appropriate spot.
        setMappedAttributeDecl(entry, attr, htmlAttr->decl());
        htmlAttr->decl()->setMappedState(entry, attr->id(), attr->value());
        htmlAttr->decl()->setParent(0);
        htmlAttr->decl()->setNode(0);
        if (namedAttrMap)
            static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->declAdded();
    }
}

bool HTMLElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr)
    {
        case ATTR_ALIGN:
        case ATTR_CONTENTEDITABLE:
        case ATTR_DIR:
            result = eUniversal;
            return false;
        default:
            break;
    }
    
    result = eNone;
    return true;
}
    
void HTMLElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    DOMString indexstring;
    switch (attr->id())
    {
    case ATTR_ALIGN:
        if (strcasecmp(attr->value(), "middle" ) == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, "center");
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, attr->value());
        break;
// the core attributes...
    case ATTR_ID:
        // unique id
        setHasID(!attr->isNull());
        if (namedAttrMap) {
            if (attr->isNull())
                namedAttrMap->setID(nullAtom);
            else if (getDocument()->inCompatMode() && !attr->value().implementation()->isLower())
                namedAttrMap->setID(AtomicString(attr->value().implementation()->lower()));
            else
                namedAttrMap->setID(attr->value());
        }
        setChanged();
        break;
    case ATTR_CLASS:
        // class
        setHasClass(!attr->isNull());
        if (namedAttrMap) static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->parseClassAttribute(attr->value());
        setChanged();
        break;
    case ATTR_CONTENTEDITABLE:
        setContentEditable(attr);
        break;
    case ATTR_STYLE:
        // ### we need to remove old style info in case there was any!
        // ### the inline sheet ay contain more than 1 property!
        // stylesheet info
        setHasStyle();
        if (!m_inlineStyleDecl) createInlineStyleDecl();
        m_inlineStyleDecl->setProperty(attr->value());
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
        addCSSProperty(attr, CSS_PROP_DIRECTION, attr->value());
        addCSSProperty(attr, CSS_PROP_UNICODE_BIDI, CSS_VAL_EMBED);
        break;
// standard events
    case ATTR_ONCLICK:
	setHTMLEventListener(EventImpl::KHTML_CLICK_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONCONTEXTMENU:
	setHTMLEventListener(EventImpl::CONTEXTMENU_EVENT,
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
        setHTMLEventListener(EventImpl::KEYDOWN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
	break;
    case ATTR_ONKEYPRESS:
        setHTMLEventListener(EventImpl::KEYPRESS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
	break;
    case ATTR_ONKEYUP:
        setHTMLEventListener(EventImpl::KEYUP_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONSCROLL:
        setHTMLEventListener(EventImpl::SCROLL_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBEFORECUT:
        setHTMLEventListener(EventImpl::BEFORECUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONCUT:
        setHTMLEventListener(EventImpl::CUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBEFORECOPY:
        setHTMLEventListener(EventImpl::BEFORECOPY_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONCOPY:
        setHTMLEventListener(EventImpl::COPY_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBEFOREPASTE:
        setHTMLEventListener(EventImpl::BEFOREPASTE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONPASTE:
        setHTMLEventListener(EventImpl::PASTE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;        
    case ATTR_ONDRAGENTER:
        setHTMLEventListener(EventImpl::DRAGENTER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDRAGOVER:
        setHTMLEventListener(EventImpl::DRAGOVER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDRAGLEAVE:
        setHTMLEventListener(EventImpl::DRAGLEAVE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDROP:
        setHTMLEventListener(EventImpl::DROP_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDRAGSTART:
        setHTMLEventListener(EventImpl::DRAGSTART_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDRAG:
        setHTMLEventListener(EventImpl::DRAG_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONDRAGEND:
        setHTMLEventListener(EventImpl::DRAGEND_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONSELECTSTART:
        setHTMLEventListener(EventImpl::SELECTSTART_EVENT,
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

void HTMLElementImpl::createAttributeMap() const
{
    namedAttrMap = new HTMLNamedAttrMapImpl(const_cast<HTMLElementImpl*>(this));
    namedAttrMap->ref();
}

CSSStyleDeclarationImpl* HTMLElementImpl::getInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        createInlineStyleDecl();
    return m_inlineStyleDecl;
}

CSSStyleDeclarationImpl* HTMLElementImpl::additionalAttributeStyleDecl()
{
    return 0;
}

const AtomicStringList* HTMLElementImpl::getClassList() const
{
    return namedAttrMap ? static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->getClassList() : 0;
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

void HTMLElementImpl::addCSSProperty(HTMLAttributeImpl* attr, int id, const DOMString &value)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setProperty(id, value, false);
}

void HTMLElementImpl::addCSSProperty(HTMLAttributeImpl* attr, int id, int value)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setProperty(id, value, false);
}

void HTMLElementImpl::addCSSStringProperty(HTMLAttributeImpl* attr, int id, const DOMString &value, CSSPrimitiveValue::UnitTypes type)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setStringProperty(id, value, type, false);
}

void HTMLElementImpl::addCSSImageProperty(HTMLAttributeImpl* attr, int id, const DOMString &URL)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setImageProperty(id, URL, false);
}

void HTMLElementImpl::addCSSLength(HTMLAttributeImpl* attr, int id, const DOMString &value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.
    if (!attr->decl()) createMappedDecl(attr);

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
            attr->decl()->setLengthProperty(id, DOMString( v->s, l ), false);
            return;
        }
    }
    
    attr->decl()->setLengthProperty(id, value, false);
}

/* color parsing that tries to match as close as possible IE 6. */
void HTMLElementImpl::addHTMLColor(HTMLAttributeImpl* attr, int id, const DOMString &c)
{
    // this is the only case no color gets applied in IE.
    if ( !c.length() )
        return;

    if (!attr->decl()) createMappedDecl(attr);
    
    if (attr->decl()->setProperty(id, c, false) )
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
            if ( attr->decl()->setProperty(id, DOMString(color), false) )
                return;
        }
    }
    attr->decl()->setProperty(id, CSS_VAL_BLACK, false);
}

void HTMLElementImpl::createMappedDecl(HTMLAttributeImpl* attr)
{
    CSSMappedAttributeDeclarationImpl* decl = new CSSMappedAttributeDeclarationImpl(0);
    attr->setDecl(decl);
    decl->setParent(getDocument()->elementSheet());
    decl->setNode(this);
    decl->setStrictParsing(false); // Mapped attributes are just always quirky.
}

DOMString HTMLElementImpl::innerHTML() const
{
    return toHTML();
}

DOMString HTMLElementImpl::outerHTML() const
{
    return recursive_toHTML(true);
}

DOMString HTMLElementImpl::innerText() const
{
    Node startContainer(const_cast<HTMLElementImpl *>(this));
    long startOffset = 0;
    Node endContainer(const_cast<HTMLElementImpl *>(this));

    long endOffset = 0;

    for (NodeImpl *child = firstChild(); child; child = child->nextSibling()) {
	endOffset++;
    }

    Range innerRange(startContainer, startOffset, endContainer, endOffset);

    return plainText(innerRange);
}

DOMString HTMLElementImpl::outerText() const
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't.
    return innerText();
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
    fragment->ref();
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

    NodeImpl *nextNode;
    for (NodeImpl *node = fragment->firstChild(); node != NULL; node = nextNode) {
        nextNode = node->nextSibling();
	if (node->id() == ID_HTML || node->id() == ID_BODY) {
	    NodeImpl *firstChild = node->firstChild();
            if (firstChild != NULL) {
                nextNode = firstChild;
            }
	    NodeImpl *nextChild;
            for (NodeImpl *child = firstChild; child != NULL; child = nextChild) {
		nextChild = child->nextSibling();
                child->ref();
                node->removeChild(child, ignoredExceptionCode);
		fragment->insertBefore(child, node, ignoredExceptionCode);
                child->deref();
	    }
            fragment->removeChild(node, ignoredExceptionCode);
            // FIXME: Does node leak here?
	} else if (node->id() == ID_HEAD) {
	    fragment->removeChild(node, ignoredExceptionCode);
            // FIXME: Does node leak here?
	}
    }

    // Trick to get the fragment back to the floating state, with 0
    // refs but not destroyed.
    fragment->setParent(this);
    fragment->deref();
    fragment->setParent(0);

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

bool HTMLElementImpl::setOuterHTML( const DOMString &html )
{
    DocumentFragmentImpl *fragment = createContextualFragment( html );
    if (fragment == NULL) {
	return false;
    }
    
    int ec = 0;
    parentNode()->replaceChild(fragment, this, ec);
    return !ec;
}


bool HTMLElementImpl::setInnerText( const DOMString &text )
{
    // following the IE specs.
    if( endTag[id()] == FORBIDDEN )
        return false;
    // IE disallows innerText on inline elements. I don't see why we should have this restriction, as our
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

bool HTMLElementImpl::setOuterText( const DOMString &text )
{
    // following the IE specs.
    if( endTag[id()] == FORBIDDEN )
        return false;
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

    NodeBaseImpl *parent = static_cast<NodeBaseImpl *>(parentNode());

    if (!parent) {
	return false;
    }

    TextImpl *t = new TextImpl( docPtr(), text );
    int ec = 0;
    parent->replaceChild(t, this, ec);

    if ( ec )
        return false;

    // is previous node a text node? if so, merge into it
    NodeImpl *prev = t->previousSibling();
    if (prev && prev->isTextNode()) {
	TextImpl *textPrev = static_cast<TextImpl *>(prev);
	textPrev->appendData(t->data(), ec);
	t->parentNode()->removeChild(t, ec);
	t = textPrev;
    }

    if ( ec )
        return false;

    // is next node a text node? if so, merge it in
    NodeImpl *next = t->nextSibling();
    if (next && next->isTextNode()) {
	TextImpl *textNext = static_cast<TextImpl *>(next);
	t->appendData(textNext->data(), ec);
	textNext->parentNode()->removeChild(textNext, ec);
    }

    if ( ec )
        return false;

    return true;
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

void HTMLElementImpl::addHTMLAlignment(HTMLAttributeImpl* attr)
{
    //qDebug("alignment is %s", alignment.string().latin1() );
    // vertical alignment with respect to the current baseline of the text
    // right or left means floating images
    int propfloat = -1;
    int propvalign = -1;
    const AtomicString& alignment = attr->value();
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
	propvalign = CSS_VAL__KHTML_BASELINE_MIDDLE;
    } else if ( strcasecmp( alignment, "center" ) == 0 ) {
	propvalign = CSS_VAL_MIDDLE;
    } else if ( strcasecmp( alignment, "bottom" ) == 0 ) {
	propvalign = CSS_VAL_BASELINE;
    } else if ( strcasecmp ( alignment, "texttop") == 0 ) {
	propvalign = CSS_VAL_TEXT_TOP;
    }
    
    if ( propfloat != -1 )
	addCSSProperty( attr, CSS_PROP_FLOAT, propfloat );
    if ( propvalign != -1 )
	addCSSProperty( attr, CSS_PROP_VERTICAL_ALIGN, propvalign );
}

bool HTMLElementImpl::isFocusable() const
{
    return isContentEditable() && parent() && !parent()->isContentEditable();
}

bool HTMLElementImpl::isContentEditable() const 
{
    if (getDocument()->part() && getDocument()->part()->isContentEditable())
        return true;

    getDocument()->updateRendering();

    if (!renderer()) {
        if (parentNode())
            return parentNode()->isContentEditable();
        else
            return false;
    }
    
    return renderer()->style()->userModify() == READ_WRITE;
}

DOMString HTMLElementImpl::contentEditable() const 
{
    getDocument()->updateRendering();

    if (!renderer())
        return "false";
    
    switch (renderer()->style()->userModify()) {
        case READ_WRITE:
            return "true";
        case READ_ONLY:
            return "false";
        default:
            return "inherit";
    }
    return "inherit";
}

void HTMLElementImpl::setContentEditable(HTMLAttributeImpl* attr) 
{
    const AtomicString& enabled = attr->value();
    if (enabled.isEmpty() || strcasecmp(enabled, "true") == 0)
        addCSSProperty(attr, CSS_PROP__KHTML_USER_MODIFY, CSS_VAL_READ_WRITE);
    else if (strcasecmp(enabled, "false") == 0)
        addCSSProperty(attr, CSS_PROP__KHTML_USER_MODIFY, CSS_VAL_READ_ONLY);
    else if (strcasecmp(enabled, "inherit") == 0)
        addCSSProperty(attr, CSS_PROP__KHTML_USER_MODIFY, CSS_VAL_INHERIT);
}

void HTMLElementImpl::setContentEditable(const DOMString &enabled) {
    if (enabled == "inherit") {
        int exceptionCode;
        removeAttribute(ATTR_CONTENTEDITABLE, exceptionCode);
    }
    else
        setAttribute(ATTR_CONTENTEDITABLE, enabled.isEmpty() ? "true" : enabled);
}

void HTMLElementImpl::click()
{
    int x = 0;
    int y = 0;
    if (renderer()) {
        renderer()->absolutePosition(x,y);
        x += renderer()->width() / 2;
        y += renderer()->height() / 2;
    }
    // always send click
    QMouseEvent evt(QEvent::MouseButtonRelease, QPoint(x,y), Qt::LeftButton, 0);
    dispatchMouseEvent(&evt, EventImpl::KHTML_CLICK_EVENT);
}

DOMString HTMLElementImpl::toString() const
{
    if (!hasChildNodes()) {
	DOMString result = openTagStartToString();
	result += ">";

	if (endTag[id()] == REQUIRED) {
	    result += "</";
	    result += tagName();
	    result += ">";
	}

	return result;
    }

    return ElementImpl::toString();
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

