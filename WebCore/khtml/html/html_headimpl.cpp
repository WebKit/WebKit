/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

using namespace DOM;
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

void HTMLBaseElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
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
        HTMLElementImpl::parseHTMLAttribute(attr);
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

    if(!m_href.isEmpty() && getDocument()->part())
	getDocument()->setBaseURL( KURL( getDocument()->part()->url(), m_href.string() ).url() );

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
    m_isStyleSheet = m_isIcon = m_alternate = false;
    m_disabledState = 0;
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

void HTMLLinkElementImpl::setDisabledState(bool _disabled)
{
    int oldDisabledState = m_disabledState;
    m_disabledState = _disabled ? 2 : 1;
    if (oldDisabledState != m_disabledState) {
        // If we change the disabled state while the sheet is still loading, then we have to
        // perform three checks:
        if (isLoading()) {
            // Check #1: If the sheet becomes disabled while it was loading, and if it was either
            // a main sheet or a sheet that was previously enabled via script, then we need
            // to remove it from the list of pending sheets.
            if (m_disabledState == 2 && (!m_alternate || oldDisabledState == 1))
                getDocument()->stylesheetLoaded();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_alternate && m_disabledState == 1)
                getDocument()->addPendingSheet();

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script.  It takes really terrible code to make this
            // happen (a double toggle for no reason essentially).  This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_alternate && m_disabledState == 1 && oldDisabledState == 2)
                getDocument()->addPendingSheet();

            // If the sheet is already loading just bail.
            return;
        }

        // Load the sheet, since it's never been loaded before.
        if (!m_sheet && m_disabledState == 1)
            process();
        else
            getDocument()->updateStyleSelector(); // Update the style selector.
    }
}

void HTMLLinkElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_REL:
        tokenizeRelAttribute(attr->value());
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
        setDisabledState(!attr->isNull());
        break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

void HTMLLinkElementImpl::tokenizeRelAttribute(const AtomicString& relStr)
{
    m_isStyleSheet = m_isIcon = m_alternate = false;
    QString rel = relStr.string().lower();
    if (rel == "stylesheet")
        m_isStyleSheet = true;
    else if (rel == "icon" || rel == "shortcut icon")
        m_isIcon = true;
    else if (rel == "alternate stylesheet" || rel == "stylesheet alternate")
        m_isStyleSheet = m_alternate = true;
    else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        rel.replace('\n', ' ');
        QStringList list = QStringList::split(' ', rel);        
        for (QStringList::Iterator i = list.begin(); i != list.end(); ++i) {
            if (*i == "stylesheet")
                m_isStyleSheet = true;
            else if (*i == "alternate")
                m_alternate = true;
            else if (*i == "icon")
                m_isIcon = true;
        }
    }
}

void HTMLLinkElementImpl::process()
{
    if (!inDocument())
        return;

    QString type = m_type.string().lower();
    
    KHTMLPart* part = getDocument()->part();

    // IE extension: location of small icon for locationbar / bookmarks
    if (part && m_isIcon && !m_url.isEmpty() && !part->parentPart()) {
        if (!type.isEmpty()) // Mozilla extension to IE extension: icon specified with type
            part->browserExtension()->setTypedIconURL(KURL(m_url.string()), type);
        else 
            part->browserExtension()->setIconURL(KURL(m_url.string()));
    }

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if (m_disabledState != 2 && (type.contains("text/css") || m_isStyleSheet) && getDocument()->part()) {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
	// also, don't load style sheets for standalone documents
        if (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print")) {
            m_loading = true;

            // Add ourselves as a pending sheet, but only if we aren't an alternate 
            // stylesheet.  Alternate stylesheets don't hold up render tree construction.
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
    kdDebug( 6030 ) << "style sheet parse mode strict = " << ( !getDocument()->inCompatMode() ) << endl;
    m_sheet->ref();
    m_sheet->parseString( sheetStr, !getDocument()->inCompatMode() );

    MediaListImpl *media = new MediaListImpl( m_sheet, m_media );
    m_sheet->setMedia( media );

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet && !isDisabled() && !isAlternate())
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
    if (!isLoading() && !isDisabled() && !isAlternate())
        getDocument()->stylesheetLoaded();
}

bool HTMLLinkElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_HREF;
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

void HTMLMetaElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
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
        HTMLElementImpl::parseHTMLAttribute(attr);
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

bool HTMLScriptElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_SRC;
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
void HTMLStyleElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_TYPE:
        m_type = attr->value().domString().lower();
        break;
    case ATTR_MEDIA:
        m_media = attr->value().string().lower();
        break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
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
        if (static_cast<CSSStyleSheetImpl *>(m_sheet)->isLoading())
            getDocument()->stylesheetLoaded(); // Remove ourselves from the sheet list.
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
        m_sheet->parseString( text, !getDocument()->inCompatMode() );
        MediaListImpl *media = new MediaListImpl( m_sheet, m_media );
        m_sheet->setMedia( media );
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
#if APPLE_CHANGES
    // Only allow title to be set by first <title> encountered.
    if (getDocument()->title().isEmpty())
        getDocument()->setTitle(m_title);
#else
        getDocument()->setTitle(m_title);
#endif
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
#if APPLE_CHANGES
    // Only allow title to be set by first <title> encountered.
    if (inDocument() && getDocument()->title().isEmpty())
#else
    if (inDocument()))
#endif
	getDocument()->setTitle(m_title);
}
