/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

#include "html/html_headimpl.h"
#include "html/html_documentimpl.h"
#include "xml/dom_textimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "misc/htmlhashes.h"
#include "misc/loader.h"
#include "misc/helper.h"

#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/csshelper.h"

#include <kurl.h>
#include <kdebug.h>

using namespace khtml;

HTMLBaseElementImpl::HTMLBaseElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLBaseElementImpl::~HTMLBaseElementImpl()
{
}

NodeImpl::Id HTMLBaseElementImpl::id() const
{
    return ID_BASE;
}

void HTMLBaseElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_HREF:
	m_href = khtml::parseURL(attr->value());
	process();
	break;
    case ATTR_TARGET:
	m_target = attr->value();
	process();
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLBaseElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();
    process();
}

void HTMLBaseElementImpl::removedFromDocument()
{
    HTMLElementImpl::removedFromDocument();

    // Since the document doesn't have a base element...
    // (This will break in the case of multiple base elements, but that's not valid anyway (?))
    getDocument()->setBaseURL( QString::null );
    getDocument()->setBaseTarget( QString::null );
}

void HTMLBaseElementImpl::process()
{
    if (!inDocument())
	return;

    if(!m_href.isEmpty())
	getDocument()->setBaseURL( KURL( getDocument()->view()->part()->url(), m_href.string() ).url() );

    if(!m_target.isEmpty())
	getDocument()->setBaseTarget( m_target.string() );

    // ### should changing a document's base URL dynamically automatically update all images, stylesheets etc?
}

// -------------------------------------------------------------------------

HTMLLinkElementImpl::HTMLLinkElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_sheet = 0;
    m_loading = false;
    m_cachedSheet = 0;
    m_alternate = false;
}

HTMLLinkElementImpl::~HTMLLinkElementImpl()
{
    if(m_sheet) m_sheet->deref();
    if(m_cachedSheet) m_cachedSheet->deref(this);
}

NodeImpl::Id HTMLLinkElementImpl::id() const
{
    return ID_LINK;
}

void HTMLLinkElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_REL:
        m_rel = attr->value();
	process();
        break;
    case ATTR_HREF:
        m_url = getDocument()->completeURL( khtml::parseURL(attr->value()).string() );
	process();
        break;
    case ATTR_TYPE:
        m_type = attr->value();
	process();
        break;
    case ATTR_MEDIA:
        m_media = attr->value().string().lower();
	process();
        break;
    case ATTR_DISABLED:
        // ###
        break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLLinkElementImpl::process()
{
    if (!inDocument())
	return;

    QString type = m_type.string().lower();
    QString rel = m_rel.string().lower();

    KHTMLPart* part = getDocument()->view() ? getDocument()->view()->part() : 0;

    // IE extension: location of small icon for locationbar / bookmarks
#ifdef APPLE_CHANGES
    if ( part && rel == "shortcut icon" && !m_url.isEmpty() && !part->parentPart())
    	part->browserExtension()->setIconURL( KURL(m_url.string()) );

    // Mozilla extension to IE extension: icon specified with type
    if ( part && rel == "icon" && !m_url.isEmpty() && !part->parentPart())
    	part->browserExtension()->setTypedIconURL( KURL(m_url.string()), type );
#else
    // Uses both "shortcut icon" and "icon"
   
    if ( part && rel.contains("icon") && !m_url.isEmpty() && !part->parentPart())
        part->browserExtension()->setIconURL( KURL(m_url.string()) );
#endif

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if(type.contains("text/css") || rel == "stylesheet" || (rel.contains("alternate") && rel.contains("stylesheet"))) {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
        if( m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print") ) {
            m_loading = true;

            // Add ourselves as a pending sheet, but only if we aren't an alternate 
            // stylesheet.  Alternate stylesheets don't hold up render tree construction.
            m_alternate = rel.contains("alternate");
            if (!isAlternate())
                getDocument()->addPendingSheet();
            
            QString chset = getAttribute( ATTR_CHARSET ).string();
            if (m_cachedSheet)
                m_cachedSheet->deref(this);
            m_cachedSheet = getDocument()->docLoader()->requestStyleSheet(m_url, chset);
            if (m_cachedSheet)
                m_cachedSheet->ref(this);
        }
    }
    else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        m_sheet->deref();
        m_sheet = 0;
        getDocument()->updateStyleSelector();
    }
}

void HTMLLinkElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();
    process();
}

void HTMLLinkElementImpl::removedFromDocument()
{
    HTMLElementImpl::removedFromDocument();
    process();
}

void HTMLLinkElementImpl::setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheetStr)
{
//    kdDebug( 6030 ) << "HTMLLinkElement::setStyleSheet()" << endl;
//    kdDebug( 6030 ) << "**** current medium: " << m_media << endl;

    if (m_sheet)
        m_sheet->deref();
    m_sheet = new CSSStyleSheetImpl(this, url);
    kdDebug( 6030 ) << "style sheet parse mode strict = " << ( getDocument()->parseMode() == DocumentImpl::Strict ) << endl;
    m_sheet->ref();
    m_sheet->parseString( sheetStr, getDocument()->parseMode() == DocumentImpl::Strict );

    MediaListImpl *media = new MediaListImpl( m_sheet, m_media );
    m_sheet->setMedia( media );

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet && !isAlternate())
        getDocument()->stylesheetLoaded();
}

bool HTMLLinkElementImpl::isLoading() const
{
//    kdDebug( 6030 ) << "link: checking if loading!" << endl;
    if(m_loading) return true;
    if(!m_sheet) return false;
    //if(!m_sheet->isCSSStyleSheet()) return false;
    return static_cast<CSSStyleSheetImpl *>(m_sheet)->isLoading();
}

void HTMLLinkElementImpl::sheetLoaded()
{
    if (!isLoading() && !isAlternate())
        getDocument()->stylesheetLoaded();
}

// -------------------------------------------------------------------------

HTMLMetaElementImpl::HTMLMetaElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLMetaElementImpl::~HTMLMetaElementImpl()
{
}

NodeImpl::Id HTMLMetaElementImpl::id() const
{
    return ID_META;
}

void HTMLMetaElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_HTTP_EQUIV:
	m_equiv = attr->value();
	process();
	break;
    case ATTR_CONTENT:
	m_content = attr->value();
	process();
	break;
    case ATTR_NAME:
      break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLMetaElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();
    process();
}

void HTMLMetaElementImpl::process()
{
    // Get the document to process the tag, but only if we're actually part of DOM tree (changing a meta tag while
    // it's not in the tree shouldn't have any effect on the document)
    if (inDocument() && !m_equiv.isNull() && !m_content.isNull())
	getDocument()->processHttpEquiv(m_equiv,m_content);
}

// -------------------------------------------------------------------------

HTMLScriptElementImpl::HTMLScriptElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLScriptElementImpl::~HTMLScriptElementImpl()
{
}

NodeImpl::Id HTMLScriptElementImpl::id() const
{
    return ID_SCRIPT;
}

// -------------------------------------------------------------------------

HTMLStyleElementImpl::HTMLStyleElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
    m_sheet = 0;
    m_loading = false;
}

HTMLStyleElementImpl::~HTMLStyleElementImpl()
{
    if(m_sheet) m_sheet->deref();
}

NodeImpl::Id HTMLStyleElementImpl::id() const
{
    return ID_STYLE;
}

// other stuff...
void HTMLStyleElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_TYPE:
        m_type = attr->value();
        break;
    case ATTR_MEDIA:
        m_media = attr->value().string().lower();
        break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLStyleElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();
    if (m_sheet)
        getDocument()->updateStyleSelector();
}

void HTMLStyleElementImpl::removedFromDocument()
{
    HTMLElementImpl::removedFromDocument();
    if (m_sheet)
        getDocument()->updateStyleSelector();
}

void HTMLStyleElementImpl::childrenChanged()
{
    DOMString text = "";

    for (NodeImpl *c = firstChild(); c != 0; c = c->nextSibling()) {
	if ((c->nodeType() == Node::TEXT_NODE) ||
	    (c->nodeType() == Node::CDATA_SECTION_NODE) ||
	    (c->nodeType() == Node::COMMENT_NODE))
	    text += c->nodeValue();
    }

    if (m_sheet) {
        m_sheet->deref();
        m_sheet = 0;
    }
    
    m_loading = false;
    if ((m_type.isEmpty() || m_type == "text/css") // Type must be empty or CSS
         && (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print"))) {
        getDocument()->addPendingSheet();
        m_loading = true;
        m_sheet = new CSSStyleSheetImpl(this);
        m_sheet->ref();
        m_sheet->parseString( text, (getDocument()->parseMode() == DocumentImpl::Strict) );
        m_loading = false;
    }

    if (!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

bool HTMLStyleElementImpl::isLoading() const
{
    if (m_loading) return true;
    if(!m_sheet) return false;
    return static_cast<CSSStyleSheetImpl *>(m_sheet)->isLoading();
}

void HTMLStyleElementImpl::sheetLoaded()
{
    if (!isLoading())
        getDocument()->stylesheetLoaded();
}

// -------------------------------------------------------------------------

HTMLTitleElementImpl::HTMLTitleElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLTitleElementImpl::~HTMLTitleElementImpl()
{
}

NodeImpl::Id HTMLTitleElementImpl::id() const
{
    return ID_TITLE;
}

void HTMLTitleElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();
    getDocument()->setTitle(m_title);
}

void HTMLTitleElementImpl::removedFromDocument()
{
    HTMLElementImpl::removedFromDocument();
    // Title element removed, so we have no title... we ignore the case of multiple title elements, as it's invalid
    // anyway (?)
    getDocument()->setTitle(DOMString());
}

void HTMLTitleElementImpl::childrenChanged()
{
    HTMLElementImpl::childrenChanged();
    m_title = "";
    for (NodeImpl *c = firstChild(); c != 0; c = c->nextSibling()) {
	if ((c->nodeType() == Node::TEXT_NODE) || (c->nodeType() == Node::CDATA_SECTION_NODE))
	    m_title += c->nodeValue();
    }
    if (inDocument())
	getDocument()->setTitle(m_title);
}
