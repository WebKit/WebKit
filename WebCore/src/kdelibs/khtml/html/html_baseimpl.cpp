/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org))
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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
 * $Id$
 */
// -------------------------------------------------------------------------
#include "html_baseimpl.h"

#include "html_documentimpl.h"

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
#include "xml/dom2_eventsimpl.h"

#include <kurl.h>
#include <kdebug.h>

using namespace DOM;
using namespace khtml;

HTMLBodyElementImpl::HTMLBodyElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_styleSheet = 0;
}

HTMLBodyElementImpl::~HTMLBodyElementImpl()
{
    if(m_styleSheet) m_styleSheet->deref();
}

const DOMString HTMLBodyElementImpl::nodeName() const
{
    return "BODY";
}

ushort HTMLBodyElementImpl::id() const
{
    return ID_BODY;
}

void HTMLBodyElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {

    case ATTR_BACKGROUND:
    {
        KURL u = khtml::Cache::completeURL(attr->value(), static_cast<HTMLDocumentImpl *>(ownerDocument())->baseURL());
        bgImage = u.url();
        addCSSProperty(CSS_PROP_BACKGROUND_IMAGE, "url('"+u.url()+"')" );
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
        bgColor = attr->value();
        addCSSProperty(CSS_PROP_BACKGROUND_COLOR, attr->value());
        break;
    case ATTR_TEXT:
        addCSSProperty(CSS_PROP_COLOR, attr->value());
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
            m_styleSheet = new CSSStyleSheetImpl(this,0,true);
            m_styleSheet->ref();
        }
        QString aStr;
	if ( attr->attrId == ATTR_LINK )
	    aStr = "a:link";
	else if ( attr->attrId == ATTR_VLINK )
	    aStr = "a:visited";
	else if ( attr->attrId == ATTR_ALINK )
	    aStr = "a:active";
	aStr += " { color: " + attr->value().string() + "; }";
        m_styleSheet->parseString(aStr);
        m_styleSheet->setNonCSSHints();
        if (attached())
            getDocument()->createSelector();
        break;
    }
    case ATTR_ONLOAD:
        ownerDocument()->setWindowEventListener(EventImpl::LOAD_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONUNLOAD:
        ownerDocument()->setWindowEventListener(EventImpl::UNLOAD_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        ownerDocument()->setWindowEventListener(EventImpl::BLUR_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONFOCUS:
        ownerDocument()->setWindowEventListener(EventImpl::FOCUS_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_NOSAVE:
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}


void HTMLBodyElementImpl::attach()
{
    KHTMLView* w = ownerDocument()->view();
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

    ownerDocument()->createSelector();

    setStyle(ownerDocument()->styleSelector()->styleForElement( this ));

    khtml::RenderObject *r = _parent->renderer();

    // ignore display: none for this element!
    if ( !r )
      return;

    m_render = new RenderBody(this);
    m_render->setStyle(m_style);
    r->addChild( m_render, nextRenderer() );

    HTMLElementImpl::attach();
}

bool HTMLBodyElementImpl::prepareMouseEvent(int _x, int _y, int _tx, int _ty, MouseEvent *ev)
{
    bool inside = HTMLElementImpl::prepareMouseEvent(_x, _y, _tx, _ty, ev);
    if(!inside)
	ev->innerNode = this;
    return !inside;
}

// -------------------------------------------------------------------------

HTMLFrameElementImpl::HTMLFrameElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    view = 0;
    parentWidget = 0;

    frameBorder = true;
    frameBorderSet = false;
    marginWidth = -1;
    marginHeight = -1;
    scrolling = QScrollView::Auto;
    noresize = false;
}

HTMLFrameElementImpl::~HTMLFrameElementImpl()
{
}

const DOMString HTMLFrameElementImpl::nodeName() const
{
    return "FRAME";
}

ushort HTMLFrameElementImpl::id() const
{
    return ID_FRAME;
}

void HTMLFrameElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_SRC:
        url = khtml::parseURL(attr->val());
        break;
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

void HTMLFrameElementImpl::attach()
{
    setStyle(ownerDocument()->styleSelector()->styleForElement( this ));

    KHTMLView* w = ownerDocument()->view();
    // limit to how deep we can nest frames
    KHTMLPart *part = w->part();
    int depth = 0;
    while ((part = part->parentPart()))
        depth++;

    if (depth > 6 || url.isNull()) {
        style()->setDisplay( NONE );
        return;
    }

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

    khtml::RenderObject *r = _parent->renderer();

    // ignore display: none for this element!
    if ( !r )
      return;

    khtml::RenderFrame *renderFrame = new khtml::RenderFrame( w, this );
    m_render = renderFrame;
    m_render->setStyle(m_style);
    r->addChild( m_render, nextRenderer() );

    // we need a unique name for every frame in the frameset. Hope that's unique enough.
    if(name.isEmpty() || w->part()->frameExists( name.string() ) )
    {
      name = DOMString(w->part()->requestFrameName());
      kdDebug( 6030 ) << "creating frame name: " << name.string() << endl;
    }

    w->part()->requestFrame( renderFrame, url.string(), name.string() );

    HTMLElementImpl::attach();
    return;
}

void HTMLFrameElementImpl::detach()
{
    HTMLElementImpl::detach();
    delete view;
    parentWidget = 0;
}

bool HTMLFrameElementImpl::isSelectable() const
{
    return m_render!=0;
}

void HTMLFrameElementImpl::setFocus(bool received)
{
    HTMLElementImpl::setFocus(received);
    khtml::RenderFrame *renderFrame = static_cast<khtml::RenderFrame *>(m_render);
    if (!renderFrame || !renderFrame->m_widget)
        return;
    if (received)
        renderFrame->m_widget->setFocus();
    else
        renderFrame->m_widget->clearFocus();
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

    view = 0;
}

HTMLFrameSetElementImpl::~HTMLFrameSetElementImpl()
{
    delete m_rows;
    delete m_cols;
}

const DOMString HTMLFrameSetElementImpl::nodeName() const
{
    return "FRAMESET";
}

ushort HTMLFrameSetElementImpl::id() const
{
    return ID_FRAMESET;
}

void HTMLFrameSetElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_ROWS:
        m_rows = attr->val()->toLengthList();
        m_totalRows = m_rows->count();
        break;
    case ATTR_COLS:
        m_cols = attr->val()->toLengthList();
        m_totalCols = m_cols->count();
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
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONUNLOAD:
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
	    ownerDocument()->createHTMLEventListener(attr->value().string()));
        break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLFrameSetElementImpl::attach()
{
    KHTMLView* w = ownerDocument()->view();
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

    setStyle(ownerDocument()->styleSelector()->styleForElement( this ));
    view = w;
    khtml::RenderObject *r = _parent->renderer();

    // ignore display: none for this element!
    if ( !r )
      return;

    khtml::RenderFrameSet *renderFrameSet = new khtml::RenderFrameSet( this, w, m_rows, m_cols );
    m_render = renderFrameSet;
    m_render->setStyle(m_style);
    r->addChild( m_render, nextRenderer() );

    HTMLElementImpl::attach();
}

// verifies that we have enough m_rows/m_cols entries for the actual document structure
// if there are not enough, we add fake ones.
// ### investigate IE's behaviour in such cases more closely
bool HTMLFrameSetElementImpl::verifyLayout()
{
    QList<khtml::Length>* layoutAttr = 0;

    if(m_cols) layoutAttr = m_cols;
    if(m_rows) layoutAttr = m_rows;

    if(!layoutAttr) {
        layoutAttr = new QList<Length>;
        layoutAttr->setAutoDelete(true);
        m_rows = layoutAttr;
        static_cast<khtml::RenderFrameSet*>(m_render)->m_rows = layoutAttr;
    }

    bool changed = false;
    unsigned int l = layoutAttr->count();
    unsigned int i = 0;

    for(HTMLElementImpl* node = static_cast<HTMLElementImpl*>(firstChild());
        node;
        node = static_cast<HTMLElementImpl*>(node->nextSibling()), i++) {
        if(i < l) continue;

        if(node->id() == ID_FRAMESET)
            layoutAttr->append(new Length(1, khtml::Relative));
        else
            layoutAttr->append(new Length(0, khtml::Fixed));
        changed = true;
    }
    if(m_rows)
        m_totalRows = i;
    else
        m_totalCols = i;

    return changed;
}

bool HTMLFrameSetElementImpl::prepareMouseEvent( int _x, int _y,
                                                 int _tx, int _ty,
                                                 MouseEvent *ev )
{
    _x-=_tx;
    _y-=_ty;

    NodeImpl *child = _first;
    while(child)
    {
        if(child->id() == ID_FRAMESET)
            if(child->prepareMouseEvent( _x, _y, _tx, _ty, ev )) return true;
        child = child->nextSibling();
    }

    if(noresize) return true;

    if ( !m_render || !m_render->layouted() )
    {
      kdDebug( 6030 ) << "ugh, not layouted or not attached?!" << endl;
      return true;
    }
    if ( (m_render->style() && m_render->style()->visiblity() == HIDDEN) )
      return true;

    if (static_cast<khtml::RenderFrameSet *>(m_render)->canResize( _x, _y, ev->type )) {
        ev->innerNode = Node(this);
        return true;
    }
    else
        return false;
}

void HTMLFrameSetElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (evt->isMouseEvent())
        static_cast<khtml::RenderFrameSet *>(m_render)->userResize(static_cast<MouseEventImpl*>(evt));
}

khtml::FindSelectionResult HTMLFrameSetElementImpl::findSelectionNode( int _x, int _y, int _tx, int _ty,
                                                                DOM::Node & node, int & offset )
{
    _x-=_tx;
    _y-=_ty;
    NodeImpl *child = _first;
    while(child)
    {
        if(child->id() == ID_FRAMESET)
            return child->findSelectionNode( _x, _y, _tx, _ty, node, offset ); // to be checked
        child = child->nextSibling();
    }
    return SelectionPointAfter;
}

void HTMLFrameSetElementImpl::detach()
{
    HTMLElementImpl::detach();
    // ### send the event when we actually get removed from the doc instead of here
    dispatchHTMLEvent(EventImpl::UNLOAD_EVENT,false,false);
}


// -------------------------------------------------------------------------

HTMLHeadElementImpl::HTMLHeadElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLHeadElementImpl::~HTMLHeadElementImpl()
{
}

const DOMString HTMLHeadElementImpl::nodeName() const
{
    return "HEAD";
}

ushort HTMLHeadElementImpl::id() const
{
    return ID_HEAD;
}

void HTMLHtmlElementImpl::attach()
{
    setStyle(ownerDocument()->styleSelector()->styleForElement( this ));
    khtml::RenderObject *r = _parent->renderer();

    // ignore display: none for this element!
    if ( !r )
      return;

    m_render = new khtml::RenderHtml();
    m_render->setStyle(m_style);
    r->addChild( m_render, nextRenderer() );

    HTMLElementImpl::attach();
}


// -------------------------------------------------------------------------

HTMLHtmlElementImpl::HTMLHtmlElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLHtmlElementImpl::~HTMLHtmlElementImpl()
{
}

const DOMString HTMLHtmlElementImpl::nodeName() const
{
    return "HTML";
}

ushort HTMLHtmlElementImpl::id() const
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

const DOMString HTMLIFrameElementImpl::nodeName() const
{
    return "IFRAME";
}

ushort HTMLIFrameElementImpl::id() const
{
    return ID_IFRAME;
}

void HTMLIFrameElementImpl::parseAttribute(AttrImpl *attr )
{
  DOM::DOMStringImpl *stringImpl = attr->value().implementation();
  QString val = QConstString( stringImpl->s, stringImpl->l ).string();
  switch (  attr->attrId )
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
  setStyle(ownerDocument()->styleSelector()->styleForElement( this ));

  KHTMLView* w = ownerDocument()->view();
  // limit to how deep we can nest frames
  KHTMLPart *part = w->part();
  int depth = 0;
  while ((part = part->parentPart()))
    depth++;
  if (depth > 6) {
      style()->setDisplay( NONE );
      return;
  }

  khtml::RenderObject *r = _parent->renderer();

  if(r && m_style->display() != NONE) {

      // we need a unique name for every frame in the frameset. Hope that's unique enough.
      if(name.isEmpty())
      {
          name = DOMString(w->part()->requestFrameName());
          kdDebug( 6030 ) << "creating frame name: " << name.string() << endl;
      }

      khtml::RenderPartObject *renderFrame = new khtml::RenderPartObject( w, this );
      m_render = renderFrame;
      m_render->setStyle(m_style);
      r->addChild( m_render, nextRenderer() );
      renderFrame->updateWidget();
  }

  HTMLElementImpl::attach();
}

void HTMLIFrameElementImpl::applyChanges(bool top, bool force)
{
    if (needWidgetUpdate) {
        if(m_render)  static_cast<RenderPartObject*>(m_render)->updateWidget();
        needWidgetUpdate = false;
    }
    HTMLElementImpl::applyChanges(top,force);
}

DocumentImpl* HTMLIFrameElementImpl::frameDocument() const
{
    if ( !m_render ) return 0;

    RenderPartObject* render = static_cast<RenderPartObject*>( m_render );

    if(render->m_widget && render->m_widget->inherits("KHTMLView"))
        return static_cast<KHTMLView*>( render->m_widget )->part()->xmlDocImpl();

    return 0;
}
