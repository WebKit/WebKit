/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#include "html_headimpl.h"
#include "dom_textimpl.h"
#include "html_documentimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "htmlhashes.h"

#include "misc/loader.h"
#include "misc/helper.h"

#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/csshelper.h"
using namespace khtml;

#include <kurl.h>
#include <kstringhandler.h>
#include <kio/job.h>

#include <kdebug.h>

HTMLBaseElementImpl::HTMLBaseElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLBaseElementImpl::~HTMLBaseElementImpl()
{
}

const DOMString HTMLBaseElementImpl::nodeName() const
{
    return "BASE";
}

ushort HTMLBaseElementImpl::id() const
{
    return ID_BASE;
}

void HTMLBaseElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_HREF:
      _href = khtml::parseURL(attr->value());
      break;
    case ATTR_TARGET:
      _target = attr->value();
      break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}


void HTMLBaseElementImpl::attach()
{
    KHTMLView* v = ownerDocument()->view();
    setStyle(ownerDocument()->styleSelector()->styleForElement(this));
    if(_href.length())
    {
      v->part()->setBaseURL( KURL( v->part()->url(), _href.string() ) );
    }
    if(_target.length())
    {
      v->part()->setBaseTarget(_target.string());
    }
    HTMLElementImpl::attach();
}

// -------------------------------------------------------------------------

HTMLLinkElementImpl::HTMLLinkElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
    m_sheet = 0;
    m_loading = false;
    m_cachedSheet = 0;
}

HTMLLinkElementImpl::~HTMLLinkElementImpl()
{
    if(m_sheet) m_sheet->deref();
    if(m_cachedSheet) m_cachedSheet->deref(this);
}

const DOMString HTMLLinkElementImpl::nodeName() const
{
    return "LINK";
}

ushort HTMLLinkElementImpl::id() const
{
    return ID_LINK;
}

// other stuff...
void HTMLLinkElementImpl::attach()
{
    setStyle(ownerDocument()->styleSelector()->styleForElement(this));

    QString type = m_type.string().lower();
    QString rel = m_rel.string().lower();

    if((type.contains("text/css") || rel == "stylesheet") && !rel.contains("alternate"))
    {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
        if( m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print") )
        {
            m_loading = true;
            HTMLDocumentImpl *doc = static_cast<HTMLDocumentImpl *>(ownerDocument());
            // we must have a doc->docLoader() !
            QString chset = getAttribute( ATTR_CHARSET ).string();
            m_cachedSheet = doc->docLoader()->requestStyleSheet(m_url, doc->baseURL(), chset);
            if(m_cachedSheet) m_cachedSheet->ref(this);
        }
    }
    HTMLElementImpl::attach();

    getDocument()->createSelector();
}

void HTMLLinkElementImpl::detach()
{
    if ( sheet() )
        getDocument()->createSelector();

    HTMLElementImpl::detach();
}


void HTMLLinkElementImpl::parseAttribute(AttrImpl *attr)
{
    switch (attr->attrId)
    {
    case ATTR_REL:
        m_rel = attr->value(); break;
    case ATTR_HREF:
        m_url = khtml::parseURL(attr->value()); break;
    case ATTR_TYPE:
        m_type = attr->value(); break;
    case ATTR_MEDIA:
        m_media = attr->value().string().lower(); break;
    case ATTR_DISABLED:
        // ###
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLLinkElementImpl::setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheetStr)
{
//    kdDebug( 6030 ) << "HTMLLinkElement::setStyleSheet()" << endl;
    if( m_sheet ) return;
    m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->ref();
    m_sheet->parseString(sheetStr);
    m_loading = false;

    if ( sheet() )  getDocument()->createSelector();
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
    getDocument()->createSelector();
}

StyleSheetImpl *HTMLLinkElementImpl::sheet() const
{
    if ( khtml::printpainter ) {
        // we're currently printing, return all and print style sheets
        if( m_media.isNull() || m_media.contains("all") || m_media.contains("print") )
            return m_sheet;
    } else {
        if( m_media.isNull() || m_media.contains("screen") || m_media.contains("all") )
            return m_sheet;
    }
    return 0;
}


// -------------------------------------------------------------------------

HTMLMetaElementImpl::HTMLMetaElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLMetaElementImpl::~HTMLMetaElementImpl()
{
}

const DOMString HTMLMetaElementImpl::nodeName() const
{
    return "META";
}

ushort HTMLMetaElementImpl::id() const
{
    return ID_META;
}

void HTMLMetaElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_HTTP_EQUIV:
      _equiv = attr->value();
      break;
    case ATTR_CONTENT:
      _content = attr->value();
      break;
    case ATTR_NAME:
      break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}


void HTMLMetaElementImpl::attach()
{
    KHTMLView *v = ownerDocument()->view();
    setStyle(ownerDocument()->styleSelector()->styleForElement(this));
//    kdDebug() << "meta::attach() equiv=" << _equiv.string() << ", content=" << _content.string() << endl;
    if(strcasecmp(_equiv, "refresh") == 0 && !_content.isNull() && v->part()->metaRefreshEnabled())
    {
        // get delay and url
        QString str = _content.string().stripWhiteSpace();
        int pos = str.find(QRegExp("[;,]"));
        if ( pos == -1 )
            pos = str.find(QRegExp("[ \t]"));

        if (pos == -1) // There can be no url (David)
        {
            bool ok = false;
            int delay = 0;
            if(!_content.isNull())
                delay = _content.implementation()->toInt(&ok);
//             kdDebug( ) << "delay = " << delay << " ok = " << ok << endl;
//             kdDebug( ) << "====> scheduling redirect to " << v->part()->url().url() << endl;
            if(ok) v->part()->scheduleRedirection(delay, v->part()->url().url());
        } else {
            int delay = 0;
            bool ok = false;
            if(!_content.isNull()) {
                DOMStringImpl* s = _content.implementation()->substring(0, pos);
                delay = s->toInt(&ok);
                delete s;
            }
            pos++;
            while(pos < (int)str.length() && str[pos].isSpace()) pos++;
            str = str.mid(pos);
            if(str.find("url", 0,  false ) == 0)  str = str.mid(3);
            str = str.stripWhiteSpace();
            if ( str.length() && str[0] == '=' ) str = str.mid( 1 ).stripWhiteSpace();
            str = parseURL( DOMString(str) ).string();
            if ( ok )
                v->part()->scheduleRedirection(delay, v->part()->completeURL( str ).url());
        }
    }
    else if(strcasecmp(_equiv, "expires") == 0 && !_content.isNull())
    {
        QString str = _content.string().stripWhiteSpace();
        time_t expire_date = str.toLong();
        KURL url = v->part()->url();
        if (url.protocol().startsWith("http"))
        {
            // KIO::http_update_cache(m_url, false, d->m_doc->docLoader()->expire_date);
            // moved to khtml_part.cpp::slotFinished(KIO::Job *) (Tobias)
            if (ownerDocument()->docLoader())
                ownerDocument()->docLoader()->setExpireDate(expire_date);
        }
    }
    else if( (strcasecmp(_equiv, "pragma") == 0 ||  strcasecmp(_equiv, "cache-control") == 0 ) && !_content.isNull() )
    {
        QString str = _content.string().lower().stripWhiteSpace();
        KURL url = v->part()->url();
        if ((str == "no-cache") && url.protocol().startsWith("http"))
        {
           KIO::http_update_cache(url, true, 0);
        }
    }
    HTMLElementImpl::attach();
}

// -------------------------------------------------------------------------

HTMLScriptElementImpl::HTMLScriptElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLScriptElementImpl::~HTMLScriptElementImpl()
{
}

const DOMString HTMLScriptElementImpl::nodeName() const
{
    return "SCRIPT";
}

ushort HTMLScriptElementImpl::id() const
{
    return ID_SCRIPT;
}

// -------------------------------------------------------------------------

HTMLStyleElementImpl::HTMLStyleElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
    m_sheet = 0;
}

HTMLStyleElementImpl::~HTMLStyleElementImpl()
{
    if(m_sheet) m_sheet->deref();
}

const DOMString HTMLStyleElementImpl::nodeName() const
{
    return "STYLE";
}

ushort HTMLStyleElementImpl::id() const
{
    return ID_STYLE;
}

// other stuff...
void HTMLStyleElementImpl::parseAttribute(AttrImpl *attr)
{
    switch (attr->attrId)
    {
    case ATTR_TYPE:
        m_type = attr->value(); break;
    case ATTR_MEDIA:
        m_media = attr->value(); break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

NodeImpl *HTMLStyleElementImpl::addChild(NodeImpl *child)
{
    NodeImpl *r = NodeBaseImpl::addChild(child);
    reparseSheet();
    sheetLoaded();
    return r;
}

void HTMLStyleElementImpl::setChanged(bool b)
{
    // TextImpl sets it's parent to be changed when appendData() or similar is called (hack)
    // ### make this work properly in all situations
    if (b)
        reparseSheet();

    HTMLElementImpl::setChanged(b);
}

bool HTMLStyleElementImpl::isLoading() const
{
    if(!m_sheet) return false;
    //if(!m_sheet->isCSSStyleSheet()) return false;
    return static_cast<CSSStyleSheetImpl *>(m_sheet)->isLoading();
}

void HTMLStyleElementImpl::sheetLoaded()
{
    getDocument()->createSelector();
}

void HTMLStyleElementImpl::reparseSheet()
{
    DOMString text = "";
    NodeImpl *n;
    for (n = _first; n; n = n->nextSibling()) {
        if (n->nodeType() == Node::TEXT_NODE ||
            n->nodeType() == Node::CDATA_SECTION_NODE ||
            n->nodeType() == Node::COMMENT_NODE)
        text += static_cast<CharacterDataImpl*>(n)->data();
    }

    if(m_sheet)
	m_sheet->deref();
    m_sheet = new CSSStyleSheetImpl(this);
    m_sheet->ref();
    m_sheet->parseString( text, (ownerDocument()->parseMode() == DocumentImpl::Strict) );
    getDocument()->createSelector();
}

void HTMLStyleElementImpl::attach()
{
    if (m_sheet) getDocument()->createSelector();
    HTMLElementImpl::attach();
}

void HTMLStyleElementImpl::detach()
{
    if (m_sheet) getDocument()->createSelector();
    HTMLElementImpl::detach();
}

// -------------------------------------------------------------------------

HTMLTitleElementImpl::HTMLTitleElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLTitleElementImpl::~HTMLTitleElementImpl()
{
}

const DOMString HTMLTitleElementImpl::nodeName() const
{
    return "TITLE";
}

ushort HTMLTitleElementImpl::id() const
{
    return ID_TITLE;
}

void HTMLTitleElementImpl::setTitle()
{
    if(!_first || _first->id() != ID_TEXT) return;
    TextImpl *t = static_cast<TextImpl *>(_first);
    QString s = t->data().string();
    s.compose();
    s = KStringHandler::csqueeze( s.visual(), 128 );

    HTMLDocumentImpl *d = static_cast<HTMLDocumentImpl *>(ownerDocument());
    if ( !d->view()->part()->parentPart() )
        emit d->view()->part()->setWindowCaption( s.visual() );
}
