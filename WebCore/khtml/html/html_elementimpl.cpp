/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#include "editing/visible_text.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "dom/dom_exception.h"
#include "rendering/render_object.h"
#include "rendering/render_replaced.h"
#include "css/css_valueimpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/css_ruleimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "editing/markup.h"

#include <kdebug.h>

using namespace DOM;
using namespace khtml;

// ------------------------------------------------------------------

HTMLElementImpl::HTMLElementImpl(DocumentPtr *doc)
    : StyledElementImpl(doc)
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

NodeImpl *HTMLElementImpl::cloneNode(bool deep)
{
    HTMLElementImpl *clone = static_cast<HTMLElementImpl *>(getDocument()->createHTMLElement(localNamePart(id())));
    if (!clone)
        return 0;

    if (namedAttrMap)
        *clone->attributes() = *namedAttrMap;

    if (m_inlineStyleDecl)
        *clone->getInlineStyleDecl() = *m_inlineStyleDecl;

    if (deep)
        cloneChildNodes(clone);

    return clone;
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

    return StyledElementImpl::mapToEntry(attr, result);
}
    
void HTMLElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->id() == ATTR_ID || attr->id() == ATTR_CLASS || attr->id() == ATTR_STYLE)
        return StyledElementImpl::parseMappedAttribute(attr);

    DOMString indexstring;
    switch (attr->id())
    {
    case ATTR_ALIGN:
        if (strcasecmp(attr->value(), "middle" ) == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, "center");
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, attr->value());
        break;
    case ATTR_CONTENTEDITABLE:
        setContentEditable(attr);
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
            getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONCONTEXTMENU:
	setHTMLEventListener(EventImpl::CONTEXTMENU_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDBLCLICK:
	setHTMLEventListener(EventImpl::KHTML_DBLCLICK_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONMOUSEDOWN:
        setHTMLEventListener(EventImpl::MOUSEDOWN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONMOUSEMOVE:
        setHTMLEventListener(EventImpl::MOUSEMOVE_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONMOUSEOUT:
        setHTMLEventListener(EventImpl::MOUSEOUT_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONMOUSEOVER:
        setHTMLEventListener(EventImpl::MOUSEOVER_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONMOUSEUP:
        setHTMLEventListener(EventImpl::MOUSEUP_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONMOUSEWHEEL:
        setHTMLEventListener(EventImpl::MOUSEWHEEL_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONFOCUS:
        setHTMLEventListener(EventImpl::DOMFOCUSIN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONKEYDOWN:
        setHTMLEventListener(EventImpl::KEYDOWN_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
	break;
    case ATTR_ONKEYPRESS:
        setHTMLEventListener(EventImpl::KEYPRESS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
	break;
    case ATTR_ONKEYUP:
        setHTMLEventListener(EventImpl::KEYUP_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONSCROLL:
        setHTMLEventListener(EventImpl::SCROLL_EVENT,
            getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONBEFORECUT:
        setHTMLEventListener(EventImpl::BEFORECUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONCUT:
        setHTMLEventListener(EventImpl::CUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONBEFORECOPY:
        setHTMLEventListener(EventImpl::BEFORECOPY_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONCOPY:
        setHTMLEventListener(EventImpl::COPY_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONBEFOREPASTE:
        setHTMLEventListener(EventImpl::BEFOREPASTE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONPASTE:
        setHTMLEventListener(EventImpl::PASTE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;        
    case ATTR_ONDRAGENTER:
        setHTMLEventListener(EventImpl::DRAGENTER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDRAGOVER:
        setHTMLEventListener(EventImpl::DRAGOVER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDRAGLEAVE:
        setHTMLEventListener(EventImpl::DRAGLEAVE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDROP:
        setHTMLEventListener(EventImpl::DROP_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDRAGSTART:
        setHTMLEventListener(EventImpl::DRAGSTART_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDRAG:
        setHTMLEventListener(EventImpl::DRAG_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONDRAGEND:
        setHTMLEventListener(EventImpl::DRAGEND_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONSELECTSTART:
        setHTMLEventListener(EventImpl::SELECTSTART_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
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

DOMString HTMLElementImpl::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

DOMString HTMLElementImpl::outerHTML() const
{
    return createMarkup(this);
}

DOMString HTMLElementImpl::innerText() const
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    getDocument()->updateLayout();
    return plainText(rangeOfContents(const_cast<HTMLElementImpl *>(this)).get());
}

DOMString HTMLElementImpl::outerText() const
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't in WinIE, and we want to match that.
    return innerText();
}

DocumentFragmentImpl *HTMLElementImpl::createContextualFragment(const DOMString &html)
{
    // the following is in accordance with the definition as used by IE
    if( endTagRequirement(id()) == FORBIDDEN )
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
        HTMLTokenizer tok(docPtr(), fragment);
        tok.setForceSynchronous(true);            // disable asynchronous parsing
        tok.write( html.string(), true );
        tok.finish();
        assert(!tok.processingData());            // make sure we're done (see 3963151)
    }

    // Exceptions are ignored because none ought to happen here.
    int ignoredExceptionCode;

    // we need to pop <html> and <body> elements and remove <head> to
    // accomadate folks passing complete HTML documents to make the
    // child of an element.

    NodeImpl *nextNode;
    for (NodeImpl *node = fragment->firstChild(); node != NULL; node = nextNode) {
        nextNode = node->nextSibling();
	node->ref();
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
	} else if (node->id() == ID_HEAD) {
	    fragment->removeChild(node, ignoredExceptionCode);
	}
        // Important to do this deref after removeChild, because if the only thing
        // keeping a node around is a parent that is non-0, removeChild will not
        // delete the node. This works fine in JavaScript because there's always
        // a ref of the node, but here in C++ we need to do it explicitly.
        node->deref();
    }

    // Trick to get the fragment back to the floating state, with 0
    // refs but not destroyed.
    fragment->setParent(this);
    fragment->deref();
    fragment->setParent(0);

    return fragment;
}

void HTMLElementImpl::setInnerHTML(const DOMString &html, int &exception)
{
    DocumentFragmentImpl *fragment = createContextualFragment( html );
    if (fragment == NULL) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    removeChildren();
    appendChild(fragment, exception);
}

void HTMLElementImpl::setOuterHTML(const DOMString &html, int &exception)
{
    NodeImpl *p = parent();
    if (!p || !p->isHTMLElement()) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    HTMLElementImpl *parent = static_cast<HTMLElementImpl *>(p);
    DocumentFragmentImpl *fragment = parent->createContextualFragment( html );

    if (!fragment) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    
    if (parentNode()) {
        parentNode()->replaceChild(fragment, this, exception);
    }
}


void HTMLElementImpl::setInnerText(const DOMString &text, int &exception)
{
    // following the IE specs.
    if (endTagRequirement(id()) == FORBIDDEN) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

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
            exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
            return;
        default:
            break;
    }

    removeChildren();
    appendChild(new TextImpl(docPtr(), text), exception);
}

void HTMLElementImpl::setOuterText(const DOMString &text, int &exception)
{
    // following the IE specs.
    if( endTagRequirement(id()) == FORBIDDEN ) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
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
            exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
            return;
        default:
            break;
    }

    NodeImpl *parent = parentNode();

    if (!parent) {
        exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    TextImpl *t = new TextImpl(docPtr(), text);
    parent->replaceChild(t, this, exception);
    if (exception)
        return;

    // is previous node a text node? if so, merge into it
    NodeImpl *prev = t->previousSibling();
    if (prev && prev->isTextNode()) {
	TextImpl *textPrev = static_cast<TextImpl *>(prev);
	textPrev->appendData(t->data(), exception);
        if (exception)
            return;
	t->parentNode()->removeChild(t, exception);
        if (exception)
            return;
	t = textPrev;
    }

    // is next node a text node? if so, merge it in
    NodeImpl *next = t->nextSibling();
    if (next && next->isTextNode()) {
	TextImpl *textNext = static_cast<TextImpl *>(next);
	t->appendData(textNext->data(), exception);
        if (exception)
            return;
	textNext->parentNode()->removeChild(textNext, exception);
        if (exception)
            return;
    }
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

void HTMLElementImpl::addHTMLAlignment(MappedAttributeImpl* attr)
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

void HTMLElementImpl::setContentEditable(MappedAttributeImpl* attr) 
{
    KHTMLPart *part = getDocument()->part();
    const AtomicString& enabled = attr->value();
    if (enabled.isEmpty() || strcasecmp(enabled, "true") == 0) {
        addCSSProperty(attr, CSS_PROP__KHTML_USER_MODIFY, CSS_VAL_READ_WRITE);
        if (part)
            part->applyEditingStyleToElement(this);    
    }
    else if (strcasecmp(enabled, "false") == 0) {
        addCSSProperty(attr, CSS_PROP__KHTML_USER_MODIFY, CSS_VAL_READ_ONLY);
        if (part)
            part->removeEditingStyleFromElement(this);    
    }
    else if (strcasecmp(enabled, "inherit") == 0) {
        addCSSProperty(attr, CSS_PROP__KHTML_USER_MODIFY, CSS_VAL_INHERIT);
        if (part)
            part->removeEditingStyleFromElement(this);    
    }
}

void HTMLElementImpl::setContentEditable(const DOMString &enabled) {
    if (enabled == "inherit") {
        int exceptionCode;
        removeAttribute(ATTR_CONTENTEDITABLE, exceptionCode);
    }
    else
        setAttribute(ATTR_CONTENTEDITABLE, enabled.isEmpty() ? "true" : enabled);
}

void HTMLElementImpl::click(bool sendMouseEvents)
{
    int x = 0;
    int y = 0;
    RenderObject *r = renderer();
    if (r)
        r->absolutePosition(x,y);

    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents) {
        QMouseEvent pressEvt(QEvent::MouseButtonPress, QPoint(x,y), Qt::LeftButton, 0);
        dispatchMouseEvent(&pressEvt, EventImpl::MOUSEDOWN_EVENT);
        QMouseEvent upEvent(QEvent::MouseButtonRelease, QPoint(x,y), Qt::LeftButton, 0);
        dispatchMouseEvent(&upEvent, EventImpl::MOUSEUP_EVENT);
    }
    
    // always send click
    QMouseEvent clickEvent(QEvent::MouseButtonRelease, QPoint(x,y), Qt::LeftButton, 0);
    dispatchMouseEvent(&clickEvent, EventImpl::KHTML_CLICK_EVENT);
}

// accessKeyAction is used by the accessibility support code
// to send events to elements that our JavaScript caller does
// does not.  The elements JS is interested in have subclasses
// that override this method to direct the click appropriately.
// Here in the base class, then, we only send the click if
// the caller wants it to go to any HTMLElementImpl, and we say
// to send the mouse events in addition to the click.
void HTMLElementImpl::accessKeyAction(bool sendToAnyElement)
{
    if (sendToAnyElement)
        click(true);
}

DOMString HTMLElementImpl::toString() const
{
    if (!hasChildNodes()) {
	DOMString result = openTagStartToString();
	result += ">";

	if (endTagRequirement(id()) == REQUIRED) {
	    result += "</";
	    result += tagName();
	    result += ">";
	}

	return result;
    }

    return ElementImpl::toString();
}

DOMString HTMLElementImpl::idDOM() const
{
    return getAttribute(ATTR_ID);
}

void HTMLElementImpl::setId(const DOMString &value)
{
    setAttribute(ATTR_ID, value);
}

DOMString HTMLElementImpl::title() const
{
    return getAttribute(ATTR_TITLE);
}

void HTMLElementImpl::setTitle(const DOMString &value)
{
    setAttribute(ATTR_TITLE, value);
}

DOMString HTMLElementImpl::lang() const
{
    return getAttribute(ATTR_LANG);
}

void HTMLElementImpl::setLang(const DOMString &value)
{
    setAttribute(ATTR_LANG, value);
}

DOMString HTMLElementImpl::dir() const
{
    return getAttribute(ATTR_DIR);
}

void HTMLElementImpl::setDir(const DOMString &value)
{
    setAttribute(ATTR_DIR, value);
}

DOMString HTMLElementImpl::className() const
{
    return getAttribute(ATTR_CLASS);
}

void HTMLElementImpl::setClassName(const DOMString &value)
{
    setAttribute(ATTR_CLASS, value);
}

SharedPtr<HTMLCollectionImpl> HTMLElementImpl::children()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::NODE_CHILDREN));
}

// -------------------------------------------------------------------------

HTMLGenericElementImpl::HTMLGenericElementImpl(DocumentPtr *doc, ushort i)
    : HTMLElementImpl(doc)
{
    m_elementId = i;
}

NodeImpl::Id HTMLGenericElementImpl::id() const
{
    return m_elementId;
}
