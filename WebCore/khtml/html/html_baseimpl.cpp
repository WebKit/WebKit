/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
 */
// -------------------------------------------------------------------------

#include "html/html_baseimpl.h"
#include "html/html_documentimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "rendering/render_frames.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/csshelper.h"
#include "misc/loader.h"
#include "misc/htmlhashes.h"
#include "dom/dom_string.h"
#include "xml/dom2_eventsimpl.h"

#include <kurl.h>
#include <kdebug.h>

using namespace DOM;
using namespace khtml;

HTMLBodyElementImpl::HTMLBodyElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc), m_linkDecl(0)
{
}

HTMLBodyElementImpl::~HTMLBodyElementImpl()
{
    if (m_linkDecl) {
        m_linkDecl->setNode(0);
        m_linkDecl->setParent(0);
        m_linkDecl->deref();
    }
}

NodeImpl::Id HTMLBodyElementImpl::id() const
{
    return ID_BODY;
}

void HTMLBodyElementImpl::createLinkDecl()
{
    m_linkDecl = new CSSMutableStyleDeclarationImpl;
    m_linkDecl->ref();
    m_linkDecl->setParent(getDocument()->elementSheet());
    m_linkDecl->setNode(this);
    m_linkDecl->setStrictParsing(!getDocument()->inCompatMode());
}

bool HTMLBodyElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch(attr)
    {
        case ATTR_BACKGROUND:
            result = (MappedAttributeEntry)(eLastEntry + getDocument()->docID());
            return false;
        case ATTR_BGCOLOR:
        case ATTR_TEXT:
        case ATTR_MARGINWIDTH:
        case ATTR_LEFTMARGIN:
        case ATTR_MARGINHEIGHT:
        case ATTR_TOPMARGIN:
        case ATTR_BGPROPERTIES:
            result = eUniversal;
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLBodyElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_BACKGROUND:
    {
        QString url = khtml::parseURL( attr->value() ).string();
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, getDocument()->completeURL(url));
        break;
    }
    case ATTR_MARGINWIDTH:
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value() );
        /* nobreak; */
    case ATTR_LEFTMARGIN:
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value() );
        break;
    case ATTR_MARGINHEIGHT:
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        /* nobreak */
    case ATTR_TOPMARGIN:
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        break;
    case ATTR_BGCOLOR:
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
        break;
    case ATTR_TEXT:
        addCSSColor(attr, CSS_PROP_COLOR, attr->value());
        break;
    case ATTR_BGPROPERTIES:
        if ( strcasecmp( attr->value(), "fixed" ) == 0)
            addCSSProperty(attr, CSS_PROP_BACKGROUND_ATTACHMENT, CSS_VAL_FIXED);
        break;
    case ATTR_VLINK:
    case ATTR_ALINK:
    case ATTR_LINK:
    {
        if (attr->isNull()) {
            if (attr->id() == ATTR_LINK)
                getDocument()->resetLinkColor();
            else if (attr->id() == ATTR_VLINK)
                getDocument()->resetVisitedLinkColor();
            else
                getDocument()->resetActiveLinkColor();
        }
        else {
            if (!m_linkDecl)
                createLinkDecl();
            m_linkDecl->setProperty(CSS_PROP_COLOR, attr->value(), false, false);
            CSSValueImpl* val = m_linkDecl->getPropertyCSSValue(CSS_PROP_COLOR);
            if (val) {
                val->ref();
                if (val->isPrimitiveValue()) {
                    QColor col = getDocument()->styleSelector()->getColorFromPrimitiveValue(static_cast<CSSPrimitiveValueImpl*>(val));
                    if (attr->id() == ATTR_LINK)
                        getDocument()->setLinkColor(col);
                    else if (attr->id() == ATTR_VLINK)
                        getDocument()->setVisitedLinkColor(col);
                    else
                        getDocument()->setActiveLinkColor(col);
                }
                val->deref();
            }
        }
        
        if (attached())
            getDocument()->recalcStyle(Force);
        break;
    }
    case ATTR_ONLOAD:
        getDocument()->setHTMLWindowEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), NULL));
        break;
    case ATTR_ONUNLOAD:
        getDocument()->setHTMLWindowEventListener(EventImpl::UNLOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), NULL));
        break;
    case ATTR_ONBLUR:
        getDocument()->setHTMLWindowEventListener(EventImpl::BLUR_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), NULL));
        break;
    case ATTR_ONFOCUS:
        getDocument()->setHTMLWindowEventListener(EventImpl::FOCUS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), NULL));
        break;
    case ATTR_ONRESIZE:
        getDocument()->setHTMLWindowEventListener(EventImpl::RESIZE_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), NULL));
        break;
    case ATTR_ONSCROLL:
        getDocument()->setHTMLWindowEventListener(EventImpl::SCROLL_EVENT,
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
        break;
    case ATTR_NOSAVE:
	break;
    default:
        HTMLElementImpl::parseMappedAttribute(attr);
    }
}

void HTMLBodyElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();

    // FIXME: perhaps this code should be in attach() instead of here

    DocumentImpl *d = getDocument();
    KHTMLView *w = d ? d->view() : 0;
    if (w && w->marginWidth() != -1) {
        QString s;
        s.sprintf( "%d", w->marginWidth() );
        setAttribute(ATTR_MARGINWIDTH, s);
    }
    if (w && w->marginHeight() != -1) {
        QString s;
        s.sprintf( "%d", w->marginHeight() );
        setAttribute(ATTR_MARGINHEIGHT, s);
    }

    if (w)
        w->scheduleRelayout();
}

bool HTMLBodyElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_BACKGROUND;
}

DOMString HTMLBodyElementImpl::aLink() const
{
    return getAttribute(ATTR_ALINK);
}

void HTMLBodyElementImpl::setALink(const DOMString &value)
{
    setAttribute(ATTR_ALINK, value);
}

DOMString HTMLBodyElementImpl::background() const
{
    return getAttribute(ATTR_BACKGROUND);
}

void HTMLBodyElementImpl::setBackground(const DOMString &value)
{
    setAttribute(ATTR_BACKGROUND, value);
}

DOMString HTMLBodyElementImpl::bgColor() const
{
    return getAttribute(ATTR_BGCOLOR);
}

void HTMLBodyElementImpl::setBgColor(const DOMString &value)
{
    setAttribute(ATTR_BGCOLOR, value);
}

DOMString HTMLBodyElementImpl::link() const
{
    return getAttribute(ATTR_LINK);
}

void HTMLBodyElementImpl::setLink(const DOMString &value)
{
    setAttribute(ATTR_LINK, value);
}

DOMString HTMLBodyElementImpl::text() const
{
    return getAttribute(ATTR_TEXT);
}

void HTMLBodyElementImpl::setText(const DOMString &value)
{
    setAttribute(ATTR_TEXT, value);
}

DOMString HTMLBodyElementImpl::vLink() const
{
    return getAttribute(ATTR_VLINK);
}

void HTMLBodyElementImpl::setVLink(const DOMString &value)
{
    setAttribute(ATTR_VLINK, value);
}

// -------------------------------------------------------------------------

HTMLFrameElementImpl::HTMLFrameElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_frameBorder = true;
    m_frameBorderSet = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
    m_scrolling = QScrollView::Auto;
    m_noResize = false;
}

HTMLFrameElementImpl::~HTMLFrameElementImpl()
{
}

NodeImpl::Id HTMLFrameElementImpl::id() const
{
    return ID_FRAME;
}

bool HTMLFrameElementImpl::isURLAllowed(const AtomicString &URLString) const
{
    if (URLString.isEmpty()) {
        return true;
    }
    
    DocumentImpl *d = getDocument();
    KHTMLView *w = d ? d->view() : 0;

    if (!w) {
	return false;
    }

    KURL newURL(getDocument()->completeURL(URLString.string()));
    newURL.setRef(QString::null);

    // Don't allow more than 1000 total frames in a set. This seems
    // like a reasonable upper bound, and otherwise mutually recursive
    // frameset pages can quickly bring the program to its knees with
    // exponential growth in the number of frames.

    // FIXME: This limit could be higher, but WebKit has some
    // algorithms that happen while loading which appear to be N^2 or
    // worse in the number of frames
    if (w->part()->topLevelFrameCount() >= 200) {
	return false;
    }

    // Prohibit non-file URLs if we are asked to.
    if (w->part()->onlyLocalReferences() && newURL.protocol().lower() != "file") {
        return false;
    }

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (KHTMLPart *part = w->part(); part; part = part->parentPart()) {
        KURL partURL = part->url();
        partURL.setRef(QString::null);
        if (partURL == newURL) {
            if (foundSelfReference) {
                return false;
            }
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElementImpl::updateForNewURL()
{
    if (!attached()) {
        return;
    }
    
    // Handle the common case where we decided not to make a frame the first time.
    // Detach and the let attach() decide again whether to make the frame for this URL.
    if (!m_render) {
        detach();
        attach();
        return;
    }
    
    if (!isURLAllowed(m_URL)) {
        return;
    }

    openURL();
}

void HTMLFrameElementImpl::openURL()
{
    DocumentImpl *d = getDocument();
    KHTMLView *w = d ? d->view() : 0;
    if (!w) {
        return;
    }
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty()) {
        relativeURL = "about:blank";
    }

    // Load the frame contents.
    KHTMLPart *part = w->part();
    KHTMLPart *framePart = part->findFrame(m_name.string());
    if (framePart) {
        framePart->openURL(getDocument()->completeURL(relativeURL.string()));
    } else {
        part->requestFrame(static_cast<RenderFrame *>(m_render), relativeURL.string(), m_name.string());
    }
}


void HTMLFrameElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SRC:
        setLocation(khtml::parseURL(attr->value()));
        break;
    case ATTR_ID:
        // Important to call through to base for ATTR_ID so the hasID bit gets set.
        HTMLElementImpl::parseMappedAttribute(attr);
        // fall through
    case ATTR_NAME:
        m_name = attr->value();
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
        break;
    case ATTR_FRAMEBORDER:
        m_frameBorder = attr->value().toInt();
        m_frameBorderSet = !attr->isNull();
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_MARGINWIDTH:
        m_marginWidth = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_MARGINHEIGHT:
        m_marginHeight = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_NORESIZE:
        m_noResize = true;
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_SCROLLING:
        kdDebug( 6031 ) << "set scroll mode" << endl;
	// Auto and yes both simply mean "allow scrolling."  No means
	// "don't allow scrolling."
        if( strcasecmp( attr->value(), "auto" ) == 0 ||
            strcasecmp( attr->value(), "yes" ) == 0 )
            m_scrolling = QScrollView::Auto;
        else if( strcasecmp( attr->value(), "no" ) == 0 )
            m_scrolling = QScrollView::AlwaysOff;
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_ONLOAD:
        setHTMLEventListener(EventImpl::LOAD_EVENT,
                                getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONUNLOAD:
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
                                getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    default:
        HTMLElementImpl::parseMappedAttribute(attr);
    }
}

bool HTMLFrameElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none.
    return isURLAllowed(m_URL);
}

RenderObject *HTMLFrameElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrame(this);
}

void HTMLFrameElementImpl::attach()
{
    // we should first look up via id, then via name.
    // this shortterm hack fixes the ugly case. ### rewrite needed for next release
    m_name = getAttribute(ATTR_NAME);
    if (m_name.isNull())
        m_name = getAttribute(ATTR_ID);

    // inherit default settings from parent frameset
    for (NodeImpl *node = parentNode(); node; node = node->parentNode())
        if (node->id() == ID_FRAMESET) {
            HTMLFrameSetElementImpl* frameset = static_cast<HTMLFrameSetElementImpl*>(node);
            if (!m_frameBorderSet)
                m_frameBorder = frameset->frameBorder();
            if (!m_noResize)
                m_noResize = frameset->noResize();
            break;
        }

    HTMLElementImpl::attach();

    if (!m_render)
        return;

    KHTMLPart *part = getDocument()->part();

    if (!part)
	return;

    part->incrementFrameCount();
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty()) {
        relativeURL = "about:blank";
    }

    // we need a unique name for every frame in the frameset. Hope that's unique enough.
    if (m_name.isEmpty() || part->frameExists(m_name.string()))
        m_name = AtomicString(part->requestFrameName());

    // load the frame contents
    part->requestFrame(static_cast<RenderFrame*>(m_render), relativeURL.string(), m_name.string());
}

void HTMLFrameElementImpl::detach()
{
    KHTMLPart *part = getDocument()->part();

    if (m_render && part) {
	part->decrementFrameCount();
        KHTMLPart *framePart = part->findFrame(m_name.string());
        if (framePart)
            framePart->frameDetached();
    }

    HTMLElementImpl::detach();
}

void HTMLFrameElementImpl::setLocation( const DOMString& str )
{
    if (m_URL == str)
        return;
    m_URL = AtomicString(str);
    updateForNewURL();
}

bool HTMLFrameElementImpl::isFocusable() const
{
    return m_render!=0;
}

void HTMLFrameElementImpl::setFocus(bool received)
{
    HTMLElementImpl::setFocus(received);
    khtml::RenderFrame *renderFrame = static_cast<khtml::RenderFrame *>(m_render);
    if (!renderFrame || !renderFrame->widget())
	return;
    if (received)
	renderFrame->widget()->setFocus();
    else
	renderFrame->widget()->clearFocus();
}

KHTMLPart* HTMLFrameElementImpl::contentPart() const
{
    // Start with the part that contains this element, our ownerDocument.
    KHTMLPart* ownerDocumentPart = getDocument()->part();
    if (!ownerDocumentPart) {
        return 0;
    }

    // Find the part for the subframe that this element represents.
    return ownerDocumentPart->findFrame(m_name.string());
}

DocumentImpl* HTMLFrameElementImpl::contentDocument() const
{
    KHTMLPart* part = contentPart();
    if (!part) {
        return 0;
    }

    // Return the document for that part, which is our contentDocument.
    return part->xmlDocImpl();
}

bool HTMLFrameElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_SRC;
}

DOMString HTMLFrameElementImpl::frameBorder() const
{
    return getAttribute(ATTR_FRAMEBORDER);
}

void HTMLFrameElementImpl::setFrameBorder(const DOMString &value)
{
    setAttribute(ATTR_FRAMEBORDER, value);
}

DOMString HTMLFrameElementImpl::longDesc() const
{
    return getAttribute(ATTR_LONGDESC);
}

void HTMLFrameElementImpl::setLongDesc(const DOMString &value)
{
    setAttribute(ATTR_LONGDESC, value);
}

DOMString HTMLFrameElementImpl::marginHeight() const
{
    return getAttribute(ATTR_MARGINHEIGHT);
}

void HTMLFrameElementImpl::setMarginHeight(const DOMString &value)
{
    setAttribute(ATTR_MARGINHEIGHT, value);
}

DOMString HTMLFrameElementImpl::marginWidth() const
{
    return getAttribute(ATTR_MARGINWIDTH);
}

void HTMLFrameElementImpl::setMarginWidth(const DOMString &value)
{
    setAttribute(ATTR_MARGINWIDTH, value);
}

DOMString HTMLFrameElementImpl::name() const
{
    return getAttribute(ATTR_NAME);
}

void HTMLFrameElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

void HTMLFrameElementImpl::setNoResize(bool noResize)
{
    setAttribute(ATTR_NORESIZE, noResize ? "" : 0);
}

DOMString HTMLFrameElementImpl::scrolling() const
{
    return getAttribute(ATTR_SCROLLING);
}

void HTMLFrameElementImpl::setScrolling(const DOMString &value)
{
    setAttribute(ATTR_SCROLLING, value);
}

DOMString HTMLFrameElementImpl::src() const
{
    return getAttribute(ATTR_SRC);
}

void HTMLFrameElementImpl::setSrc(const DOMString &value)
{
    setAttribute(ATTR_SRC, value);
}

// -------------------------------------------------------------------------

HTMLFrameSetElementImpl::HTMLFrameSetElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    // default value for rows and cols...
    m_totalRows = 1;
    m_totalCols = 1;

    m_rows = m_cols = 0;

    frameborder = true;
    frameBorderSet = false;
    m_border = 4;
    noresize = false;

    m_resizing = false;
}

HTMLFrameSetElementImpl::~HTMLFrameSetElementImpl()
{
    if (m_rows)
        delete [] m_rows;
    if (m_cols)
        delete [] m_cols;
}

NodeImpl::Id HTMLFrameSetElementImpl::id() const
{
    return ID_FRAMESET;
}

void HTMLFrameSetElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ROWS:
        if (attr->isNull()) break;
        if (m_rows) delete [] m_rows;
        m_rows = attr->value().toLengthArray(m_totalRows);
        setChanged();
    break;
    case ATTR_COLS:
        if (attr->isNull()) break;
        delete [] m_cols;
        m_cols = attr->value().toLengthArray(m_totalCols);
        setChanged();
    break;
    case ATTR_FRAMEBORDER:
        // false or "no" or "0"..
        if ( attr->value().toInt() == 0 ) {
            frameborder = false;
            m_border = 0;
        }
        frameBorderSet = true;
        break;
    case ATTR_NORESIZE:
        noresize = true;
        break;
    case ATTR_BORDER:
        m_border = attr->value().toInt();
        if(!m_border)
            frameborder = false;
        break;
    case ATTR_ONLOAD:
        setHTMLEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONUNLOAD:
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    default:
        HTMLElementImpl::parseMappedAttribute(attr);
    }
}

bool HTMLFrameSetElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none but do pay attention if a stylesheet has caused us to delay our loading.
    return style->isStyleAvailable();
}

RenderObject *HTMLFrameSetElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrameSet(this);
}

void HTMLFrameSetElementImpl::attach()
{
    // inherit default settings from parent frameset
    HTMLElementImpl* node = static_cast<HTMLElementImpl*>(parentNode());
    while(node)
    {
        if(node->id() == ID_FRAMESET)
        {
            HTMLFrameSetElementImpl* frameset = static_cast<HTMLFrameSetElementImpl*>(node);
            if(!frameBorderSet)  frameborder = frameset->frameBorder();
            if(!noresize)  noresize = frameset->noResize();
            break;
        }
        node = static_cast<HTMLElementImpl*>(node->parentNode());
    }

    HTMLElementImpl::attach();
}

void HTMLFrameSetElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (evt->isMouseEvent() && !noresize && m_render) {
        static_cast<khtml::RenderFrameSet *>(m_render)->userResize(static_cast<MouseEventImpl*>(evt));
        evt->setDefaultHandled();
    }

    HTMLElementImpl::defaultEventHandler(evt);
}

void HTMLFrameSetElementImpl::detach()
{
    if(attached())
        // ### send the event when we actually get removed from the doc instead of here
        dispatchHTMLEvent(EventImpl::UNLOAD_EVENT,false,false);

    HTMLElementImpl::detach();
}

void HTMLFrameSetElementImpl::recalcStyle( StyleChange ch )
{
    if (changed() && m_render) {
        m_render->setNeedsLayout(true);
//         m_render->layout();
        setChanged(false);
    }
    HTMLElementImpl::recalcStyle( ch );
}

DOMString HTMLFrameSetElementImpl::cols() const
{
    return getAttribute(ATTR_COLS);
}

void HTMLFrameSetElementImpl::setCols(const DOMString &value)
{
    setAttribute(ATTR_COLS, value);
}

DOMString HTMLFrameSetElementImpl::rows() const
{
    return getAttribute(ATTR_ROWS);
}

void HTMLFrameSetElementImpl::setRows(const DOMString &value)
{
    setAttribute(ATTR_ROWS, value);
}

// -------------------------------------------------------------------------

HTMLHeadElementImpl::HTMLHeadElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLHeadElementImpl::~HTMLHeadElementImpl()
{
}

NodeImpl::Id HTMLHeadElementImpl::id() const
{
    return ID_HEAD;
}

DOMString HTMLHeadElementImpl::profile() const
{
    return getAttribute(ATTR_PROFILE);
}

void HTMLHeadElementImpl::setProfile(const DOMString &value)
{
    setAttribute(ATTR_PROFILE, value);
}

// -------------------------------------------------------------------------

HTMLHtmlElementImpl::HTMLHtmlElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLHtmlElementImpl::~HTMLHtmlElementImpl()
{
}

NodeImpl::Id HTMLHtmlElementImpl::id() const
{
    return ID_HTML;
}

DOMString HTMLHtmlElementImpl::version() const
{
    return getAttribute(ATTR_VERSION);
}

void HTMLHtmlElementImpl::setVersion(const DOMString &value)
{
    setAttribute(ATTR_VERSION, value);
}

// -------------------------------------------------------------------------

HTMLIFrameElementImpl::HTMLIFrameElementImpl(DocumentPtr *doc) : HTMLFrameElementImpl(doc)
{
    m_frameBorder = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
    needWidgetUpdate = false;
}

HTMLIFrameElementImpl::~HTMLIFrameElementImpl()
{
}

NodeImpl::Id HTMLIFrameElementImpl::id() const
{
    return ID_IFRAME;
}

bool HTMLIFrameElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
            result = eUniversal;
            return false;
        case ATTR_ALIGN:
            result = eReplaced; // Share with <img> since the alignment behavior is the same.
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLIFrameElementImpl::parseMappedAttribute(MappedAttributeImpl *attr )
{
  switch (  attr->id() )
  {
    case ATTR_WIDTH:
      addCSSLength( attr, CSS_PROP_WIDTH, attr->value());
      break;
    case ATTR_HEIGHT:
      addCSSLength( attr, CSS_PROP_HEIGHT, attr->value() );
      break;
    case ATTR_ALIGN:
      addHTMLAlignment( attr );
      break;
    default:
      HTMLFrameElementImpl::parseMappedAttribute( attr );
  }
}

bool HTMLIFrameElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Don't ignore display: none the way frame does.
    return isURLAllowed(m_URL) && style->display() != NONE;
}

RenderObject *HTMLIFrameElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLIFrameElementImpl::attach()
{
    HTMLElementImpl::attach();

    KHTMLPart *part = getDocument()->part();
    if (m_render && part) {
        // we need a unique name for every frame in the frameset. Hope that's unique enough.
	part->incrementFrameCount();
        if (m_name.isEmpty() || part->frameExists(m_name.string()))
            m_name = AtomicString(part->requestFrameName());

        static_cast<RenderPartObject*>(m_render)->updateWidget();
        needWidgetUpdate = false;
    }
}

void HTMLIFrameElementImpl::recalcStyle( StyleChange ch )
{
    if (needWidgetUpdate) {
        if(m_render)  static_cast<RenderPartObject*>(m_render)->updateWidget();
        needWidgetUpdate = false;
    }
    HTMLElementImpl::recalcStyle( ch );
}

void HTMLIFrameElementImpl::openURL()
{
    needWidgetUpdate = true;
    setChanged();
}

bool HTMLIFrameElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_SRC;
}

DOMString HTMLIFrameElementImpl::align() const
{
    return getAttribute(ATTR_ALIGN);
}

void HTMLIFrameElementImpl::setAlign(const DOMString &value)
{
    setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLIFrameElementImpl::height() const
{
    return getAttribute(ATTR_HEIGHT);
}

void HTMLIFrameElementImpl::setHeight(const DOMString &value)
{
    setAttribute(ATTR_HEIGHT, value);
}

DOMString HTMLIFrameElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(ATTR_SRC));
}

DOMString HTMLIFrameElementImpl::width() const
{
    return getAttribute(ATTR_WIDTH);
}

void HTMLIFrameElementImpl::setWidth(const DOMString &value)
{
    setAttribute(ATTR_WIDTH, value);
}
