/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
#include "rendering/render_html.h"
#include "rendering/render_body.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/csshelper.h"
#include "misc/loader.h"
#include "misc/htmlhashes.h"
#include "dom/dom_string.h"
#include "dom/dom_doc.h"
#include "xml/dom2_eventsimpl.h"

#include <kurl.h>
#include <kdebug.h>

using namespace DOM;
using namespace khtml;

HTMLBodyElementImpl::HTMLBodyElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc),
    m_bgSet( false ), m_fgSet( false )
{
    m_styleSheet = 0;
}

HTMLBodyElementImpl::~HTMLBodyElementImpl()
{
    if(m_styleSheet) m_styleSheet->deref();
}

NodeImpl::Id HTMLBodyElementImpl::id() const
{
    return ID_BODY;
}

void HTMLBodyElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {

    case ATTR_BACKGROUND:
    {
        QString url = khtml::parseURL( attr->value() ).string();
        if (!url.isEmpty()) {
            url = getDocument()->completeURL( url );
            addCSSProperty(CSS_PROP_BACKGROUND_IMAGE, "url('"+url+"')" );
            m_bgSet = true;
        }
        else
            m_bgSet = false;
        break;
    }
    case ATTR_MARGINWIDTH:
        addCSSLength(CSS_PROP_MARGIN_RIGHT, attr->value() );
        /* nobreak; */
    case ATTR_LEFTMARGIN:
        addCSSLength(CSS_PROP_MARGIN_LEFT, attr->value() );
        break;
    case ATTR_MARGINHEIGHT:
        addCSSLength(CSS_PROP_MARGIN_BOTTOM, attr->value());
        /* nobreak */
    case ATTR_TOPMARGIN:
        addCSSLength(CSS_PROP_MARGIN_TOP, attr->value());
        break;
    case ATTR_BGCOLOR:
        addCSSProperty(CSS_PROP_BACKGROUND_COLOR, attr->value());
        m_bgSet = !attr->value().isNull();
        break;
    case ATTR_TEXT:
        addCSSProperty(CSS_PROP_COLOR, attr->value());
        m_fgSet = !attr->value().isNull();
        break;
    case ATTR_BGPROPERTIES:
        if ( strcasecmp( attr->value(), "fixed" ) == 0)
            addCSSProperty(CSS_PROP_BACKGROUND_ATTACHMENT, CSS_VAL_FIXED);
        break;
    case ATTR_VLINK:
    case ATTR_ALINK:
    case ATTR_LINK:
    {
        if(!m_styleSheet) {
            m_styleSheet = new CSSStyleSheetImpl(this,DOMString(),true);
            m_styleSheet->ref();
        }
        QString aStr;
	if ( attr->id() == ATTR_LINK )
	    aStr = "a:link";
	else if ( attr->id() == ATTR_VLINK )
	    aStr = "a:visited";
	else if ( attr->id() == ATTR_ALINK )
	    aStr = "a:active";
	aStr += " { color: " + attr->value().string() + "; }";
        m_styleSheet->parseString(aStr);
        m_styleSheet->setNonCSSHints();
        if (attached())
            getDocument()->updateStyleSelector();
        break;
    }
    case ATTR_ONLOAD:
        getDocument()->setWindowEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONUNLOAD:
        getDocument()->setWindowEventListener(EventImpl::UNLOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        getDocument()->setWindowEventListener(EventImpl::BLUR_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONFOCUS:
        getDocument()->setWindowEventListener(EventImpl::FOCUS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONRESIZE:
        getDocument()->setWindowEventListener(EventImpl::RESIZE_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_NOSAVE:
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLBodyElementImpl::init()
{
    HTMLElementImpl::init();

    KHTMLView* w = getDocument()->view();
    if(w->marginWidth() != -1) {
        QString s;
        s.sprintf( "%d", w->marginWidth() );
        addCSSLength(CSS_PROP_MARGIN_LEFT, s);
        addCSSLength(CSS_PROP_MARGIN_RIGHT, s);
    }
    if(w->marginHeight() != -1) {
        QString s;
        s.sprintf( "%d", w->marginHeight() );
        addCSSLength(CSS_PROP_MARGIN_TOP, s);
        addCSSLength(CSS_PROP_MARGIN_BOTTOM, s);
    }

    if ( m_bgSet && !m_fgSet )
        addCSSProperty(CSS_PROP_COLOR, "#000000");

    getDocument()->updateStyleSelector();
}

void HTMLBodyElementImpl::attach()
{
    assert(!m_render);
    assert(parentNode());
    assert(parentNode()->renderer());

    RenderStyle* style = getDocument()->styleSelector()->styleForElement(this);
    style->ref();
    if (style->display() != NONE) {
        m_render = new RenderBody(this);
        m_render->setStyle(style);
        parentNode()->renderer()->addChild(m_render, nextRenderer());
    }
    style->deref();

    NodeBaseImpl::attach();
}

// -------------------------------------------------------------------------

HTMLFrameElementImpl::HTMLFrameElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    parentWidget = 0;

    frameBorder = true;
    frameBorderSet = false;
    marginWidth = -1;
    marginHeight = -1;
    scrolling = QScrollView::Auto;
    noresize = false;
    url = "about:blank";
}

HTMLFrameElementImpl::~HTMLFrameElementImpl()
{
}

NodeImpl::Id HTMLFrameElementImpl::id() const
{
    return ID_FRAME;
}

void HTMLFrameElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SRC:
        url = khtml::parseURL(attr->val());
        break;
    case ATTR_ID:
    case ATTR_NAME:
        name = attr->value();
        break;
    case ATTR_FRAMEBORDER:
    {
        frameBorder = attr->value().toInt();
        frameBorderSet = ( attr->val() != 0 );
    }
    break;
    case ATTR_MARGINWIDTH:
        marginWidth = attr->val()->toInt();
        break;
    case ATTR_MARGINHEIGHT:
        marginHeight = attr->val()->toInt();
        break;
    case ATTR_NORESIZE:
        noresize = true;
        break;
    case ATTR_SCROLLING:
        kdDebug( 6031 ) << "set scroll mode" << endl;
        if( strcasecmp( attr->value(), "auto" ) == 0 )
            scrolling = QScrollView::Auto;
        else if( strcasecmp( attr->value(), "yes" ) == 0 )
            scrolling = QScrollView::AlwaysOn;
        else if( strcasecmp( attr->value(), "no" ) == 0 )
            scrolling = QScrollView::AlwaysOff;

    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLFrameElementImpl::init()
{
    HTMLElementImpl::init();

    // we should first look up via id, then via name.
    // this shortterm hack fixes the ugly case. ### rewrite needed for next release
    name = getAttribute(ATTR_NAME);
    if (name.isNull())
        name = getAttribute(ATTR_ID);

    // inherit default settings from parent frameset
    HTMLElementImpl* node = static_cast<HTMLElementImpl*>(parentNode());
    while(node)
    {
        if(node->id() == ID_FRAMESET)
        {
            HTMLFrameSetElementImpl* frameset = static_cast<HTMLFrameSetElementImpl*>(node);
            if(!frameBorderSet)  frameBorder = frameset->frameBorder();
            if(!noresize)  noresize = frameset->noResize();
            break;
        }
        node = static_cast<HTMLElementImpl*>(node->parentNode());
    }
}

void HTMLFrameElementImpl::attach()
{
    assert(!attached());
    assert(parentNode());
    assert(parentNode()->renderer());

    // ignore display: none for this element!
    KHTMLView* w = getDocument()->view();
    // avoid endless recursion
    KURL u;
    if (!url.isEmpty()) u = getDocument()->completeURL( url.string() );
    bool selfreference = false;
    for (KHTMLPart* part = w->part(); part; part = part->parentPart())
        if (part->url() == u) {
            selfreference = true;
            break;
        }

    if (!selfreference)  {
        m_render = new RenderFrame(this);
        m_render->setStyle(getDocument()->styleSelector()->styleForElement(this));
        parentNode()->renderer()->addChild(m_render, nextRenderer());
    }

    NodeBaseImpl::attach();

    if (!m_render)
        return;

    // we need a unique name for every frame in the frameset. Hope that's unique enough.
    if(name.isEmpty() || w->part()->frameExists( name.string() ) )
      name = DOMString(w->part()->requestFrameName());

    // load the frame contents
    if ( !url.isEmpty() && !(w->part()->onlyLocalReferences() && u.protocol() != "file"))
        w->part()->requestFrame( static_cast<RenderFrame*>(m_render), url.string(), name.string() );
}

void HTMLFrameElementImpl::detach()
{
    HTMLElementImpl::detach();
    parentWidget = 0;
}

void HTMLFrameElementImpl::setLocation( const DOMString& str )
{
    url = str;
    KHTMLView* w = getDocument()->view();
    if ( m_render && w && w->part() )
        // don't call this for an iframe
        w->part()->requestFrame( static_cast<khtml::RenderFrame *>(m_render), url.string(), name.string() );
}

bool HTMLFrameElementImpl::isSelectable() const
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

DocumentImpl* HTMLFrameElementImpl::contentDocument() const
{
    if ( !m_render ) return 0;

    RenderPart* render = static_cast<RenderPart*>( m_render );

    if(render->widget() && render->widget()->inherits("KHTMLView"))
        return static_cast<KHTMLView*>( render->widget() )->part()->xmlDocImpl();

    return 0;
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

void HTMLFrameSetElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ROWS:
        if (!attr->val()) break;
        if (m_rows) delete [] m_rows;
        m_rows = attr->val()->toLengthArray(m_totalRows);
        setChanged();
    break;
    case ATTR_COLS:
        if (!attr->val()) break;
        delete [] m_cols;
        m_cols = attr->val()->toLengthArray(m_totalCols);
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
        m_border = attr->val()->toInt();
        if(!m_border)
            frameborder = false;
        break;
    case ATTR_ONLOAD:
        setHTMLEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONUNLOAD:
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLFrameSetElementImpl::init()
{
    HTMLElementImpl::init();

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
}

void HTMLFrameSetElementImpl::attach()
{
    assert(!m_render);
    assert(parentNode());
    assert(parentNode()->renderer());

    // ignore display: none
    m_render = new RenderFrameSet(this);
    m_render->setStyle(getDocument()->styleSelector()->styleForElement(this));
    parentNode()->renderer()->addChild(m_render, nextRenderer());

    NodeBaseImpl::attach();
}

void HTMLFrameSetElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (evt->isMouseEvent() && !noresize && m_render)
        static_cast<khtml::RenderFrameSet *>(m_render)->userResize(static_cast<MouseEventImpl*>(evt));

    evt->setDefaultHandled();
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
        m_render->setLayouted(false);
//         m_render->layout();
        setChanged(false);
    }
    HTMLElementImpl::recalcStyle( ch );
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

void HTMLHtmlElementImpl::attach()
{
    assert(!m_render);
    assert(parentNode());
    assert(parentNode()->renderer());

    m_render = new RenderHtml(this);
    m_render->setStyle(getDocument()->styleSelector()->styleForElement(this));
    parentNode()->renderer()->addChild(m_render, nextRenderer());

    NodeBaseImpl::attach();
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


// -------------------------------------------------------------------------

HTMLIFrameElementImpl::HTMLIFrameElementImpl(DocumentPtr *doc) : HTMLFrameElementImpl(doc)
{
    frameBorder = false;
    marginWidth = 0;
    marginHeight = 0;
    needWidgetUpdate = false;
}

HTMLIFrameElementImpl::~HTMLIFrameElementImpl()
{
}

NodeImpl::Id HTMLIFrameElementImpl::id() const
{
    return ID_IFRAME;
}

void HTMLIFrameElementImpl::parseAttribute(AttributeImpl *attr )
{
  switch (  attr->id() )
  {
    case ATTR_WIDTH:
      addCSSLength( CSS_PROP_WIDTH, attr->value());
      break;
    case ATTR_HEIGHT:
      addCSSLength( CSS_PROP_HEIGHT, attr->value() );
      break;
    case ATTR_SRC:
      needWidgetUpdate = true; // ### do this for scrolling, margins etc?
      HTMLFrameElementImpl::parseAttribute( attr );
      break;
    default:
      HTMLFrameElementImpl::parseAttribute( attr );
  }
}

void HTMLIFrameElementImpl::attach()
{
    assert(!attached());
    assert(!m_render);
    assert(parentNode());

    KHTMLView* w = getDocument()->view();
    // avoid endless recursion
    KURL u;
    if (!url.isEmpty()) u = getDocument()->completeURL( url.string() );
    bool selfreference = false;
    for (KHTMLPart* part = w->part(); part; part = part->parentPart())
        if (part->url() == u) {
            selfreference = true;
            break;
        }

    KHTMLPart *part = w->part();
    int depth = 0;
    while ((part = part->parentPart()))
	depth++;

    RenderStyle* _style = getDocument()->styleSelector()->styleForElement(this);
    _style->ref();
    if (!selfreference && !(w->part()->onlyLocalReferences() && u.protocol() != "file") &&
        parentNode()->renderer() && _style->display() != NONE) {
        m_render = new RenderPartObject(this);
        m_render->setStyle(_style);
        parentNode()->renderer()->addChild(m_render, nextRenderer());
    }
    _style->deref();

    NodeBaseImpl::attach();

    if (m_render) {
        // we need a unique name for every frame in the frameset. Hope that's unique enough.
        KHTMLView* w = getDocument()->view();
        if(name.isEmpty() || w->part()->frameExists( name.string() ))
            name = DOMString(w->part()->requestFrameName());

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

