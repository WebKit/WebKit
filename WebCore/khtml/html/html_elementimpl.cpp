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

#include "html/html_elementimpl.h"
#include "html/html_documentimpl.h"
#include "html/htmltokenizer.h"
#include "htmlfactory.h"

#include "misc/hashset.h"
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

HTMLElementImpl::HTMLElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
    : StyledElementImpl(tagName, doc)
{
}

HTMLElementImpl::~HTMLElementImpl()
{
}

DOMString HTMLElementImpl::nodeName() const
{
    // FIXME: Would be nice to have an atomicstring lookup based off uppercase chars that does not have to copy
    // the string on a hit in the hash.
    if (getDocument()->isHTMLDocument())
        return m_tagName.localName().implementation()->upper();
    return ElementImpl::nodeName();
}
    
HTMLTagStatus HTMLElementImpl::endTagRequirement() const
{
    if (hasLocalName(HTMLTags::dt()) || hasLocalName(HTMLTags::dd()))
        return TagStatusOptional;

    // Same values as <span>.  This way custom tag name elements will behave like inline spans.
    return TagStatusRequired;
}

int HTMLElementImpl::tagPriority() const
{
    if (hasLocalName(HTMLTags::address()) || hasLocalName(HTMLTags::dd()) || hasLocalName(HTMLTags::dt()) || hasLocalName(HTMLTags::noscript()))
        return 3;
    if (hasLocalName(HTMLTags::center()) || hasLocalName(HTMLTags::nobr()))
        return 5;
    if (hasLocalName(HTMLTags::noembed()) || hasLocalName(HTMLTags::noframes()))
        return 10;

    // Same values as <span>.  This way custom tag name elements will behave like inline spans.
    return 1;
}

NodeImpl *HTMLElementImpl::cloneNode(bool deep)
{
    HTMLElementImpl *clone = HTMLElementFactory::createHTMLElement(m_tagName.localName(), getDocument(), 0, false);
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

bool HTMLElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLAttributes::align() ||
        attrName == HTMLAttributes::contenteditable() ||
        attrName == HTMLAttributes::dir()) {
        result = eUniversal;
        return false;
    }

    return StyledElementImpl::mapToEntry(attrName, result);
}
    
void HTMLElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::idAttr() || attr->name() == HTMLAttributes::classAttr() || attr->name() == HTMLAttributes::style())
        return StyledElementImpl::parseMappedAttribute(attr);

    DOMString indexstring;
    if (attr->name() == HTMLAttributes::align()) {
        if (strcasecmp(attr->value(), "middle" ) == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, "center");
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, attr->value());
    } else if (attr->name() == HTMLAttributes::contenteditable()) {
        setContentEditable(attr);
    } else if (attr->name() == HTMLAttributes::tabindex()) {
        indexstring = getAttribute(HTMLAttributes::tabindex());
        if (indexstring.length())
            setTabIndex(indexstring.toInt());
    } else if (attr->name() == HTMLAttributes::lang()) {
        // FIXME: Implement
    } else if (attr->name() == HTMLAttributes::direction()) {
        addCSSProperty(attr, CSS_PROP_DIRECTION, attr->value());
        addCSSProperty(attr, CSS_PROP_UNICODE_BIDI, CSS_VAL_EMBED);
    }
// standard events
    else if (attr->name() == HTMLAttributes::onclick()) {
        setHTMLEventListener(EventImpl::KHTML_CLICK_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::oncontextmenu()) {
    	setHTMLEventListener(EventImpl::CONTEXTMENU_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondblclick()) {
	setHTMLEventListener(EventImpl::KHTML_DBLCLICK_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onmousedown()) {
        setHTMLEventListener(EventImpl::MOUSEDOWN_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onmousemove()) {
        setHTMLEventListener(EventImpl::MOUSEMOVE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onmouseout()) {
        setHTMLEventListener(EventImpl::MOUSEOUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onmouseover()) {
        setHTMLEventListener(EventImpl::MOUSEOVER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onmouseup()) {
        setHTMLEventListener(EventImpl::MOUSEUP_EVENT,
	                     getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onmousewheel()) {
        setHTMLEventListener(EventImpl::MOUSEWHEEL_EVENT,
                            getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onfocus()) {
        setHTMLEventListener(EventImpl::DOMFOCUSIN_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onkeydown()) {
        setHTMLEventListener(EventImpl::KEYDOWN_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onkeypress()) {
        setHTMLEventListener(EventImpl::KEYPRESS_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onkeyup()) {
        setHTMLEventListener(EventImpl::KEYUP_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onscroll()) {
        setHTMLEventListener(EventImpl::SCROLL_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onbeforecut()) {
        setHTMLEventListener(EventImpl::BEFORECUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::oncut()) {
        setHTMLEventListener(EventImpl::CUT_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onbeforecopy()) {
        setHTMLEventListener(EventImpl::BEFORECOPY_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::oncopy()) {
        setHTMLEventListener(EventImpl::COPY_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onbeforepaste()) {
        setHTMLEventListener(EventImpl::BEFOREPASTE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onpaste()) {
        setHTMLEventListener(EventImpl::PASTE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondragenter()) {
        setHTMLEventListener(EventImpl::DRAGENTER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondragover()) {
        setHTMLEventListener(EventImpl::DRAGOVER_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondragleave()) {
        setHTMLEventListener(EventImpl::DRAGLEAVE_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondrop()) {
        setHTMLEventListener(EventImpl::DROP_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondragstart()) {
        setHTMLEventListener(EventImpl::DRAGSTART_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondrag()) {
        setHTMLEventListener(EventImpl::DRAG_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::ondragend()) {
        setHTMLEventListener(EventImpl::DRAGEND_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onselectstart()) {
        setHTMLEventListener(EventImpl::SELECTSTART_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
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
    if (endTagRequirement() == TagStatusForbidden)
        return 0;

    if (hasLocalName(HTMLTags::col()) || hasLocalName(HTMLTags::colgroup()) || hasLocalName(HTMLTags::frameset()) ||
        hasLocalName(HTMLTags::head()) || hasLocalName(HTMLTags::style()) || hasLocalName(HTMLTags::title()))
        return 0;

    if (!getDocument()->isHTMLDocument())
        return 0;

    DocumentFragmentImpl *fragment = new DocumentFragmentImpl(docPtr());
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
    // accommodate folks passing complete HTML documents to make the
    // child of an element.

    NodeImpl *nextNode;
    for (NodeImpl *node = fragment->firstChild(); node != NULL; node = nextNode) {
        nextNode = node->nextSibling();
	node->ref();
        if (node->hasTagName(HTMLTags::html()) || node->hasTagName(HTMLTags::body())) {
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
	} else if (node->hasTagName(HTMLTags::head()))
	    fragment->removeChild(node, ignoredExceptionCode);

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
    DocumentFragmentImpl *fragment = createContextualFragment(html);
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
    DocumentFragmentImpl *fragment = parent->createContextualFragment(html);

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
    if (endTagRequirement() == TagStatusForbidden) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if (hasLocalName(HTMLTags::col()) || hasLocalName(HTMLTags::colgroup()) || hasLocalName(HTMLTags::frameset()) ||
        hasLocalName(HTMLTags::head()) || hasLocalName(HTMLTags::html()) || hasLocalName(HTMLTags::table()) || 
        hasLocalName(HTMLTags::tbody()) || hasLocalName(HTMLTags::tfoot()) || hasLocalName(HTMLTags::thead()) ||
        hasLocalName(HTMLTags::tr())) {
        exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    removeChildren();
    appendChild(new TextImpl(docPtr(), text), exception);
}

void HTMLElementImpl::setOuterText(const DOMString &text, int &exception)
{
    // following the IE specs.
    if (endTagRequirement() == TagStatusForbidden) {
	exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if (hasLocalName(HTMLTags::col()) || hasLocalName(HTMLTags::colgroup()) || hasLocalName(HTMLTags::frameset()) ||
        hasLocalName(HTMLTags::head()) || hasLocalName(HTMLTags::html()) || hasLocalName(HTMLTags::table()) || 
        hasLocalName(HTMLTags::tbody()) || hasLocalName(HTMLTags::tfoot()) || hasLocalName(HTMLTags::thead()) ||
        hasLocalName(HTMLTags::tr())) {
        exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
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
        removeAttribute(HTMLAttributes::contenteditable(), exceptionCode);
    }
    else
        setAttribute(HTMLAttributes::contenteditable(), enabled.isEmpty() ? "true" : enabled);
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

	if (endTagRequirement() == TagStatusRequired) {
	    result += "</";
	    result += nodeName();
	    result += ">";
	}

	return result;
    }

    return ElementImpl::toString();
}

DOMString HTMLElementImpl::id() const
{
    return getAttribute(HTMLAttributes::idAttr());
}

void HTMLElementImpl::setId(const DOMString &value)
{
    setAttribute(HTMLAttributes::idAttr(), value);
}

DOMString HTMLElementImpl::title() const
{
    return getAttribute(HTMLAttributes::title());
}

void HTMLElementImpl::setTitle(const DOMString &value)
{
    setAttribute(HTMLAttributes::title(), value);
}

DOMString HTMLElementImpl::lang() const
{
    return getAttribute(HTMLAttributes::lang());
}

void HTMLElementImpl::setLang(const DOMString &value)
{
    setAttribute(HTMLAttributes::lang(), value);
}

DOMString HTMLElementImpl::dir() const
{
    return getAttribute(HTMLAttributes::dir());
}

void HTMLElementImpl::setDir(const DOMString &value)
{
    setAttribute(HTMLAttributes::dir(), value);
}

DOMString HTMLElementImpl::className() const
{
    return getAttribute(HTMLAttributes::classAttr());
}

void HTMLElementImpl::setClassName(const DOMString &value)
{
    setAttribute(HTMLAttributes::classAttr(), value);
}

SharedPtr<HTMLCollectionImpl> HTMLElementImpl::children()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::NODE_CHILDREN));
}

// DOM Section 1.1.1
bool HTMLElementImpl::childAllowed(NodeImpl *newChild)
{
    if (!ElementImpl::childAllowed(newChild))
        return false;

    // For XML documents, we are non-validating and do not check against a DTD, even for HTML elements.
    if (!getDocument()->isHTMLDocument())
        return true;

    // Future-proof for XML content inside HTML documents (we may allow this some day).
    if (newChild->isElementNode() && !newChild->isHTMLElement())
        return true;

    // Elements with forbidden tag status can never have children
    if (endTagRequirement() == TagStatusForbidden)
        return false;

    // Comment nodes are always allowed.
    if (newChild->isCommentNode())
        return true;

    // Now call checkDTD.
    return checkDTD(newChild);
}

// DTD Stuff
// This unfortunate function is only needed when checking against the DTD.  Other languages (like SVG) won't need this.
bool HTMLElementImpl::isRecognizedTagName(const QualifiedName& tagName)
{
    static HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > tagList;
    if (tagList.isEmpty()) {
        tagList.insert(HTMLTags::a().localName().implementation());
        tagList.insert(HTMLTags::abbr().localName().implementation());
        tagList.insert(HTMLTags::acronym().localName().implementation());
        tagList.insert(HTMLTags::address().localName().implementation());
        tagList.insert(HTMLTags::applet().localName().implementation());
        tagList.insert(HTMLTags::area().localName().implementation());
        tagList.insert(HTMLTags::b().localName().implementation());
        tagList.insert(HTMLTags::base().localName().implementation());
        tagList.insert(HTMLTags::basefont().localName().implementation());
        tagList.insert(HTMLTags::bdo().localName().implementation());
        tagList.insert(HTMLTags::big().localName().implementation());
        tagList.insert(HTMLTags::blockquote().localName().implementation());
        tagList.insert(HTMLTags::body().localName().implementation());
        tagList.insert(HTMLTags::br().localName().implementation());
        tagList.insert(HTMLTags::button().localName().implementation());
        tagList.insert(HTMLTags::canvas().localName().implementation());
        tagList.insert(HTMLTags::caption().localName().implementation());
        tagList.insert(HTMLTags::center().localName().implementation());
        tagList.insert(HTMLTags::cite().localName().implementation());
        tagList.insert(HTMLTags::code().localName().implementation());
        tagList.insert(HTMLTags::col().localName().implementation());
        tagList.insert(HTMLTags::colgroup().localName().implementation());
        tagList.insert(HTMLTags::dd().localName().implementation());
        tagList.insert(HTMLTags::del().localName().implementation());
        tagList.insert(HTMLTags::dfn().localName().implementation());
        tagList.insert(HTMLTags::dir().localName().implementation());
        tagList.insert(HTMLTags::div().localName().implementation());
        tagList.insert(HTMLTags::dl().localName().implementation());
        tagList.insert(HTMLTags::dt().localName().implementation());
        tagList.insert(HTMLTags::em().localName().implementation());
        tagList.insert(HTMLTags::embed().localName().implementation());
        tagList.insert(HTMLTags::fieldset().localName().implementation());
        tagList.insert(HTMLTags::font().localName().implementation());
        tagList.insert(HTMLTags::form().localName().implementation());
        tagList.insert(HTMLTags::frame().localName().implementation());
        tagList.insert(HTMLTags::frameset().localName().implementation());
        tagList.insert(HTMLTags::head().localName().implementation());
        tagList.insert(HTMLTags::h1().localName().implementation());
        tagList.insert(HTMLTags::h2().localName().implementation());
        tagList.insert(HTMLTags::h3().localName().implementation());
        tagList.insert(HTMLTags::h4().localName().implementation());
        tagList.insert(HTMLTags::h5().localName().implementation());
        tagList.insert(HTMLTags::h6().localName().implementation());
        tagList.insert(HTMLTags::hr().localName().implementation());
        tagList.insert(HTMLTags::html().localName().implementation());
        tagList.insert(HTMLTags::i().localName().implementation());
        tagList.insert(HTMLTags::iframe().localName().implementation());
        tagList.insert(HTMLTags::img().localName().implementation());
        tagList.insert(HTMLTags::input().localName().implementation());
        tagList.insert(HTMLTags::ins().localName().implementation());
        tagList.insert(HTMLTags::isindex().localName().implementation());
        tagList.insert(HTMLTags::kbd().localName().implementation());
        tagList.insert(HTMLTags::keygen().localName().implementation());
        tagList.insert(HTMLTags::label().localName().implementation());
        tagList.insert(HTMLTags::layer().localName().implementation());
        tagList.insert(HTMLTags::legend().localName().implementation());
        tagList.insert(HTMLTags::li().localName().implementation());
        tagList.insert(HTMLTags::link().localName().implementation());
        tagList.insert(HTMLTags::map().localName().implementation());
        tagList.insert(HTMLTags::marquee().localName().implementation());
        tagList.insert(HTMLTags::menu().localName().implementation());
        tagList.insert(HTMLTags::meta().localName().implementation());
        tagList.insert(HTMLTags::nobr().localName().implementation());
        tagList.insert(HTMLTags::noembed().localName().implementation());
        tagList.insert(HTMLTags::noframes().localName().implementation());
        tagList.insert(HTMLTags::nolayer().localName().implementation());
        tagList.insert(HTMLTags::noscript().localName().implementation());
        tagList.insert(HTMLTags::object().localName().implementation());
        tagList.insert(HTMLTags::ol().localName().implementation());
        tagList.insert(HTMLTags::optgroup().localName().implementation());
        tagList.insert(HTMLTags::option().localName().implementation());
        tagList.insert(HTMLTags::p().localName().implementation());
        tagList.insert(HTMLTags::param().localName().implementation());
        tagList.insert(HTMLTags::plaintext().localName().implementation());
        tagList.insert(HTMLTags::pre().localName().implementation());
        tagList.insert(HTMLTags::q().localName().implementation());
        tagList.insert(HTMLTags::s().localName().implementation());
        tagList.insert(HTMLTags::samp().localName().implementation());
        tagList.insert(HTMLTags::script().localName().implementation());
        tagList.insert(HTMLTags::select().localName().implementation());
        tagList.insert(HTMLTags::small().localName().implementation());
        tagList.insert(HTMLTags::span().localName().implementation());
        tagList.insert(HTMLTags::strike().localName().implementation());
        tagList.insert(HTMLTags::strong().localName().implementation());
        tagList.insert(HTMLTags::style().localName().implementation());
        tagList.insert(HTMLTags::sub().localName().implementation());
        tagList.insert(HTMLTags::sup().localName().implementation());
        tagList.insert(HTMLTags::table().localName().implementation());
        tagList.insert(HTMLTags::tbody().localName().implementation());
        tagList.insert(HTMLTags::td().localName().implementation());
        tagList.insert(HTMLTags::textarea().localName().implementation());
        tagList.insert(HTMLTags::tfoot().localName().implementation());
        tagList.insert(HTMLTags::th().localName().implementation());
        tagList.insert(HTMLTags::thead().localName().implementation());
        tagList.insert(HTMLTags::title().localName().implementation());
        tagList.insert(HTMLTags::tr().localName().implementation());
        tagList.insert(HTMLTags::tt().localName().implementation());
        tagList.insert(HTMLTags::u().localName().implementation());
        tagList.insert(HTMLTags::ul().localName().implementation());
        tagList.insert(HTMLTags::var().localName().implementation());
        tagList.insert(HTMLTags::wbr().localName().implementation());
        tagList.insert(HTMLTags::xmp().localName().implementation());
    }

    return tagList.contains(tagName.localName().implementation());
}

// The terms inline and block are used here loosely.  Don't make the mistake of assuming all inlines or all blocks
// need to be in these two lists.
HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> >* inlineTagList() {
    static HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > tagList;
    if (tagList.isEmpty()) {
        tagList.insert(HTMLTags::tt().localName().implementation());
        tagList.insert(HTMLTags::i().localName().implementation());
        tagList.insert(HTMLTags::b().localName().implementation());
        tagList.insert(HTMLTags::u().localName().implementation());
        tagList.insert(HTMLTags::s().localName().implementation());
        tagList.insert(HTMLTags::strike().localName().implementation());
        tagList.insert(HTMLTags::big().localName().implementation());
        tagList.insert(HTMLTags::small().localName().implementation());
        tagList.insert(HTMLTags::em().localName().implementation());
        tagList.insert(HTMLTags::strong().localName().implementation());
        tagList.insert(HTMLTags::dfn().localName().implementation());
        tagList.insert(HTMLTags::code().localName().implementation());
        tagList.insert(HTMLTags::samp().localName().implementation());
        tagList.insert(HTMLTags::kbd().localName().implementation());
        tagList.insert(HTMLTags::var().localName().implementation());
        tagList.insert(HTMLTags::cite().localName().implementation());
        tagList.insert(HTMLTags::abbr().localName().implementation());
        tagList.insert(HTMLTags::acronym().localName().implementation());
        tagList.insert(HTMLTags::a().localName().implementation());
        tagList.insert(HTMLTags::canvas().localName().implementation());
        tagList.insert(HTMLTags::img().localName().implementation());
        tagList.insert(HTMLTags::applet().localName().implementation());
        tagList.insert(HTMLTags::object().localName().implementation());
        tagList.insert(HTMLTags::embed().localName().implementation());
        tagList.insert(HTMLTags::font().localName().implementation());
        tagList.insert(HTMLTags::basefont().localName().implementation());
        tagList.insert(HTMLTags::br().localName().implementation());
        tagList.insert(HTMLTags::script().localName().implementation());
        tagList.insert(HTMLTags::map().localName().implementation());
        tagList.insert(HTMLTags::q().localName().implementation());
        tagList.insert(HTMLTags::sub().localName().implementation());
        tagList.insert(HTMLTags::sup().localName().implementation());
        tagList.insert(HTMLTags::span().localName().implementation());
        tagList.insert(HTMLTags::bdo().localName().implementation());
        tagList.insert(HTMLTags::iframe().localName().implementation());
        tagList.insert(HTMLTags::input().localName().implementation());
        tagList.insert(HTMLTags::keygen().localName().implementation());
        tagList.insert(HTMLTags::select().localName().implementation());
        tagList.insert(HTMLTags::textarea().localName().implementation());
        tagList.insert(HTMLTags::label().localName().implementation());
        tagList.insert(HTMLTags::button().localName().implementation());
        tagList.insert(HTMLTags::ins().localName().implementation());
        tagList.insert(HTMLTags::del().localName().implementation());
        tagList.insert(HTMLTags::nobr().localName().implementation());
        tagList.insert(HTMLTags::wbr().localName().implementation());
    }
    return &tagList;
}

HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> >* blockTagList() {
    static HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > tagList;
    if (tagList.isEmpty()) {
        tagList.insert(HTMLTags::p().localName().implementation());
        tagList.insert(HTMLTags::h1().localName().implementation());
        tagList.insert(HTMLTags::h2().localName().implementation());
        tagList.insert(HTMLTags::h3().localName().implementation());
        tagList.insert(HTMLTags::h4().localName().implementation());
        tagList.insert(HTMLTags::h5().localName().implementation());
        tagList.insert(HTMLTags::h6().localName().implementation());
        tagList.insert(HTMLTags::ul().localName().implementation());
        tagList.insert(HTMLTags::ol().localName().implementation());
        tagList.insert(HTMLTags::dir().localName().implementation());
        tagList.insert(HTMLTags::menu().localName().implementation());
        tagList.insert(HTMLTags::pre().localName().implementation());
        tagList.insert(HTMLTags::plaintext().localName().implementation());
        tagList.insert(HTMLTags::xmp().localName().implementation());
        tagList.insert(HTMLTags::dl().localName().implementation());
        tagList.insert(HTMLTags::div().localName().implementation());
        tagList.insert(HTMLTags::layer().localName().implementation());
        tagList.insert(HTMLTags::center().localName().implementation());
        tagList.insert(HTMLTags::noscript().localName().implementation());
        tagList.insert(HTMLTags::noframes().localName().implementation());
        tagList.insert(HTMLTags::blockquote().localName().implementation());
        tagList.insert(HTMLTags::form().localName().implementation());
        tagList.insert(HTMLTags::isindex().localName().implementation());
        tagList.insert(HTMLTags::hr().localName().implementation());
        tagList.insert(HTMLTags::table().localName().implementation());
        tagList.insert(HTMLTags::fieldset().localName().implementation());
        tagList.insert(HTMLTags::address().localName().implementation());
        tagList.insert(HTMLTags::li().localName().implementation());
        tagList.insert(HTMLTags::dd().localName().implementation());
        tagList.insert(HTMLTags::dt().localName().implementation());
        tagList.insert(HTMLTags::marquee().localName().implementation());
    }
    return &tagList;
}

bool HTMLElementImpl::inEitherTagList(const NodeImpl* newChild)
{
    if (newChild->isTextNode())
        return true;
        
    if (newChild->isHTMLElement()) {
        const HTMLElementImpl* child = static_cast<const HTMLElementImpl*>(newChild);
        if (inlineTagList()->contains(child->tagName().localName().implementation()))
            return true;
        if (blockTagList()->contains(child->tagName().localName().implementation()))
            return true;
        return !isRecognizedTagName(child->tagName()); // Accept custom html tags
    }

    return false;
}

bool HTMLElementImpl::inInlineTagList(const NodeImpl* newChild)
{
    if (newChild->isTextNode())
        return true;

    if (newChild->isHTMLElement()) {
        const HTMLElementImpl* child = static_cast<const HTMLElementImpl*>(newChild);
        if (inlineTagList()->contains(child->tagName().localName().implementation()))
            return true;
        return !isRecognizedTagName(child->tagName()); // Accept custom html tags
    }

    return false;
}

bool HTMLElementImpl::inBlockTagList(const NodeImpl* newChild)
{
    if (newChild->isTextNode())
        return true;
            
    if (newChild->isHTMLElement()) {
        const HTMLElementImpl* child = static_cast<const HTMLElementImpl*>(newChild);
        return (blockTagList()->contains(child->tagName().localName().implementation()));
    }

    return false;
}

bool HTMLElementImpl::checkDTD(const NodeImpl* newChild)
{
    if (hasTagName(HTMLTags::address()) && newChild->hasTagName(HTMLTags::p()))
        return true;
    return inEitherTagList(newChild);
}
