/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
        addHTMLColor(CSS_PROP_BACKGROUND_COLOR, attr->value());
        m_bgSet = !attr->value().isNull();
        break;
    case ATTR_TEXT:
        addHTMLColor(CSS_PROP_COLOR, attr->value());
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
        m_styleSheet->parseString(aStr, !getDocument()->inCompatMode());
        m_styleSheet->setNonCSSHints();
        if (attached())
            getDocument()->updateStyleSelector();
        break;
    }
    case ATTR_ONLOAD:
        getDocument()->setHTMLWindowEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONUNLOAD:
        getDocument()->setHTMLWindowEventListener(EventImpl::UNLOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONBLUR:
        getDocument()->setHTMLWindowEventListener(EventImpl::BLUR_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONFOCUS:
        getDocument()->setHTMLWindowEventListener(EventImpl::FOCUS_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONRESIZE:
        getDocument()->setHTMLWindowEventListener(EventImpl::RESIZE_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_NOSAVE:
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLBodyElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();

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

// -------------------------------------------------------------------------

HTMLFrameElementImpl::HTMLFrameElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
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

NodeImpl::Id HTMLFrameElementImpl::id() const
{
    return ID_FRAME;
}

bool HTMLFrameElementImpl::isURLAllowed(const DOMString &URLString) const
{
    if (URLString.isEmpty()) {
        return true;
    }
    
    KHTMLView *w = getDocument()->view();

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
    
    DOMString relativeURL = url;
    if (relativeURL.isEmpty()) {
        relativeURL = "about:blank";
    }

    if (!isURLAllowed(relativeURL)) {
        return;
    }

    KHTMLView *w = getDocument()->view();
    if (!w) {
        return;
    }
    
    // Load the frame contents.
    KHTMLPart *part = w->part();
    KHTMLPart *framePart = part->findFrame(name.string());
    KURL kurl = getDocument()->completeURL(relativeURL.string());

    // Temporarily treat javascript: URLs as about:blank, until we can
    // properly support them as frame sources.
    if (kurl.protocol() == "javascript") {
	relativeURL = "about:blank";
	kurl = "about:blank";
    }

    if (framePart) {
        framePart->openURL(kurl);
    } else {
        part->requestFrame(static_cast<RenderFrame *>(m_render), relativeURL.string(), name.string());
    }
}


void HTMLFrameElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SRC:
        url = khtml::parseURL(attr->val());
        updateForNewURL();
        break;
    case ATTR_ID:
    case ATTR_NAME:
        name = attr->value();
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
        break;
    case ATTR_FRAMEBORDER:
    {
        frameBorder = attr->value().toInt();
        frameBorderSet = ( attr->val() != 0 );
        // FIXME: If we are already attached, this has no effect.
    }
    break;
    case ATTR_MARGINWIDTH:
        marginWidth = attr->val()->toInt();
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_MARGINHEIGHT:
        marginHeight = attr->val()->toInt();
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_NORESIZE:
        noresize = true;
        // FIXME: If we are already attached, this has no effect.
        break;
    case ATTR_SCROLLING:
        kdDebug( 6031 ) << "set scroll mode" << endl;
        if( strcasecmp( attr->value(), "auto" ) == 0 )
            scrolling = QScrollView::Auto;
        else if( strcasecmp( attr->value(), "yes" ) == 0 )
            scrolling = QScrollView::AlwaysOn;
        else if( strcasecmp( attr->value(), "no" ) == 0 )
            scrolling = QScrollView::AlwaysOff;
        // FIXME: If we are already attached, this has no effect.
        // FIXME: Is this falling through on purpose, or do we want a break here?
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

bool HTMLFrameElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none.
    return isURLAllowed(url);
}

RenderObject *HTMLFrameElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrame(this);
}

void HTMLFrameElementImpl::attach()
{
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

    createRendererIfNeeded();
    NodeBaseImpl::attach();

    if (!m_render)
        return;

    KHTMLView* w = getDocument()->view();

    w->part()->incrementFrameCount();
    
    DOMString relativeURL = url;
    if (relativeURL.isEmpty()) {
        relativeURL = "about:blank";
    }

    // Temporarily treat javascript: URLs as about:blank, until we can
    // properly support them as frame sources.
    if (KURL(getDocument()->completeURL(relativeURL.string())).protocol() == "javascript") {
	relativeURL = "about:blank";
    }

    // we need a unique name for every frame in the frameset. Hope that's unique enough.
    if(name.isEmpty() || w->part()->frameExists( name.string() ) )
      name = DOMString(w->part()->requestFrameName());

    // load the frame contents
    w->part()->requestFrame( static_cast<RenderFrame*>(m_render), relativeURL.string(), name.string() );
}

void HTMLFrameElementImpl::detach()
{
    if (m_render) {
	KHTMLView* w = getDocument()->view();
	w->part()->decrementFrameCount();
        KHTMLPart *framePart = w->part()->findFrame( name.string() );
        if (framePart)
            framePart->frameDetached();
    }

    HTMLElementImpl::detach();
}

void HTMLFrameElementImpl::setLocation( const DOMString& str )
{
    url = str;
    updateForNewURL();
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
    KHTMLView* w = getDocument()->view();

    if (w) {
        KHTMLPart *part = w->part()->findFrame( name.string() );
        if (part) {
            return part->xmlDocImpl();
        }
    }

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

    createRendererIfNeeded();
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
        m_render->setNeedsLayout(true);
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
    marginWidth = -1;
    marginHeight = -1;
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
    case ATTR_ALIGN:
      addHTMLAlignment( attr->value() );
      break;
    default:
      HTMLFrameElementImpl::parseAttribute( attr );
  }
}

bool HTMLIFrameElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Don't ignore display: none the way frame does.
    return isURLAllowed(url) && style->display() != NONE;
}

RenderObject *HTMLIFrameElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLIFrameElementImpl::attach()
{
    createRendererIfNeeded();
    NodeBaseImpl::attach();

    if (m_render) {
        // we need a unique name for every frame in the frameset. Hope that's unique enough.
        KHTMLView* w = getDocument()->view();
	w->part()->incrementFrameCount();
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

