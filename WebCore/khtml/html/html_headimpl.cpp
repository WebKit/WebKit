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
#include "kjs_proxy.h"

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

void HTMLBaseElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
        HTMLElementImpl::parseMappedAttribute(attr);
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

void HTMLBaseElementImpl::setHref(const DOMString &value)
{
    setAttribute(ATTR_HREF, value);
}

void HTMLBaseElementImpl::setTarget(const DOMString &value)
{
    setAttribute(ATTR_TARGET, value);
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

void HTMLLinkElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
        HTMLElementImpl::parseMappedAttribute(attr);
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

bool HTMLLinkElementImpl::disabled() const
{
    return !getAttribute(ATTR_DISABLED).isNull();
}

void HTMLLinkElementImpl::setDisabled(bool disabled)
{
    setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

DOMString HTMLLinkElementImpl::charset() const
{
    return getAttribute(ATTR_CHARSET);
}

void HTMLLinkElementImpl::setCharset(const DOMString &value)
{
    setAttribute(ATTR_CHARSET, value);
}

DOMString HTMLLinkElementImpl::href() const
{
    return getDocument()->completeURL(getAttribute(ATTR_HREF));
}

void HTMLLinkElementImpl::setHref(const DOMString &value)
{
    setAttribute(ATTR_HREF, value);
}

DOMString HTMLLinkElementImpl::hreflang() const
{
    return getAttribute(ATTR_HREFLANG);
}

void HTMLLinkElementImpl::setHreflang(const DOMString &value)
{
    setAttribute(ATTR_HREFLANG, value);
}

DOMString HTMLLinkElementImpl::media() const
{
    return getAttribute(ATTR_MEDIA);
}

void HTMLLinkElementImpl::setMedia(const DOMString &value)
{
    setAttribute(ATTR_MEDIA, value);
}

DOMString HTMLLinkElementImpl::rel() const
{
    return getAttribute(ATTR_REL);
}

void HTMLLinkElementImpl::setRel(const DOMString &value)
{
    setAttribute(ATTR_REL, value);
}

DOMString HTMLLinkElementImpl::rev() const
{
    return getAttribute(ATTR_REV);
}

void HTMLLinkElementImpl::setRev(const DOMString &value)
{
    setAttribute(ATTR_REV, value);
}

DOMString HTMLLinkElementImpl::target() const
{
    return getAttribute(ATTR_TARGET);
}

void HTMLLinkElementImpl::setTarget(const DOMString &value)
{
    setAttribute(ATTR_TARGET, value);
}

DOMString HTMLLinkElementImpl::type() const
{
    return getAttribute(ATTR_TYPE);
}

void HTMLLinkElementImpl::setType(const DOMString &value)
{
    setAttribute(ATTR_TYPE, value);
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

void HTMLMetaElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
        HTMLElementImpl::parseMappedAttribute(attr);
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

DOMString HTMLMetaElementImpl::content() const
{
    return getAttribute(ATTR_CONTENT);
}

void HTMLMetaElementImpl::setContent(const DOMString &value)
{
    setAttribute(ATTR_CONTENT, value);
}

DOMString HTMLMetaElementImpl::httpEquiv() const
{
    return getAttribute(ATTR_HTTP_EQUIV);
}

void HTMLMetaElementImpl::setHttpEquiv(const DOMString &value)
{
    setAttribute(ATTR_HTTP_EQUIV, value);
}

DOMString HTMLMetaElementImpl::name() const
{
    return getAttribute(ATTR_NAME);
}

void HTMLMetaElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

DOMString HTMLMetaElementImpl::scheme() const
{
    return getAttribute(ATTR_SCHEME);
}

void HTMLMetaElementImpl::setScheme(const DOMString &value)
{
    setAttribute(ATTR_SCHEME, value);
}

// -------------------------------------------------------------------------

HTMLScriptElementImpl::HTMLScriptElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc), m_cachedScript(0), m_createdByParser(false), m_evaluated(false)
{
}

HTMLScriptElementImpl::~HTMLScriptElementImpl()
{
    if (m_cachedScript)
        m_cachedScript->deref(this);
}

NodeImpl::Id HTMLScriptElementImpl::id() const
{
    return ID_SCRIPT;
}

bool HTMLScriptElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_SRC;
}

void HTMLScriptElementImpl::childrenChanged()
{
    // If a node is inserted as a child of the script element
    // and the script element has been inserted in the document
    // we evaluate the script.
    if (!m_createdByParser && inDocument() && firstChild())
        evaluateScript(getDocument()->URL(), text());
}

void HTMLScriptElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();

    assert(!m_cachedScript);

    if (m_createdByParser)
        return;
    
    QString url = getAttribute(ATTR_SRC).string();
    if (!url.isEmpty()) {
        QString charset = getAttribute(ATTR_CHARSET).string();
        m_cachedScript = getDocument()->docLoader()->requestScript(DOMString(url), charset);
        m_cachedScript->ref(this);
        return;
    }

    // If there's an empty script node, we shouldn't evaluate the script
    // because if a script is inserted afterwards (by setting text or innerText)
    // it should be evaluated, and evaluateScript only evaluates a script once.
    DOMString scriptString = text();    
    if (!scriptString.isEmpty())
        evaluateScript(getDocument()->URL(), scriptString);
}

void HTMLScriptElementImpl::removedFromDocument()
{
    HTMLElementImpl::removedFromDocument();

    if (m_cachedScript) {
        m_cachedScript->deref(this);
        m_cachedScript = 0;
    }
}

void HTMLScriptElementImpl::notifyFinished(CachedObject* o)
{
    CachedScript *cs = static_cast<CachedScript *>(o);

    assert(cs == m_cachedScript);

    evaluateScript(cs->url().string(), cs->script());

    cs->deref(this);
    m_cachedScript = 0;
}

void HTMLScriptElementImpl::evaluateScript(const QString &URL, const DOMString &script)
{
    if (m_evaluated)
        return;
    
    KHTMLPart *part = getDocument()->part();
    if (part) {
        KJSProxy *proxy = KJSProxy::proxy(part);
        if (proxy) {
            m_evaluated = true;
            proxy->evaluate(URL, 0, script.string(), 0);
            DocumentImpl::updateDocumentsRendering();
        }
    }
}

DOMString HTMLScriptElementImpl::text() const
{
    DOMString val = "";
    
    for (NodeImpl *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            val += static_cast<TextImpl *>(n)->data();
    }
    
    return val;
}

void HTMLScriptElementImpl::setText(const DOMString &value)
{
    int exceptioncode = 0;
    int numChildren = childNodeCount();
    
    if (numChildren == 1 && firstChild()->isTextNode()) {
        static_cast<DOM::TextImpl *>(firstChild())->setData(value, exceptioncode);
        return;
    }
    
    if (numChildren > 0) {
        removeChildren();
    }
    
    appendChild(getDocument()->createTextNode(value.implementation()), exceptioncode);
}

DOMString HTMLScriptElementImpl::htmlFor() const
{
    // DOM Level 1 says: reserved for future use.
    return DOMString();
}

void HTMLScriptElementImpl::setHtmlFor(const DOMString &/*value*/)
{
    // DOM Level 1 says: reserved for future use.
}

DOMString HTMLScriptElementImpl::event() const
{
    // DOM Level 1 says: reserved for future use.
    return DOMString();
}

void HTMLScriptElementImpl::setEvent(const DOMString &/*value*/)
{
    // DOM Level 1 says: reserved for future use.
}

DOMString HTMLScriptElementImpl::charset() const
{
    return getAttribute(ATTR_CHARSET);
}

void HTMLScriptElementImpl::setCharset(const DOMString &value)
{
    setAttribute(ATTR_CHARSET, value);
}

bool HTMLScriptElementImpl::defer() const
{
    return !getAttribute(ATTR_DEFER).isNull();
}

void HTMLScriptElementImpl::setDefer(bool defer)
{
    setAttribute(ATTR_DEFER, defer ? "" : 0);
}

DOMString HTMLScriptElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(ATTR_SRC));
}

void HTMLScriptElementImpl::setSrc(const DOMString &value)
{
    setAttribute(ATTR_SRC, value);
}

DOMString HTMLScriptElementImpl::type() const
{
    return getAttribute(ATTR_TYPE);
}

void HTMLScriptElementImpl::setType(const DOMString &value)
{
    setAttribute(ATTR_TYPE, value);
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
void HTMLStyleElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
        HTMLElementImpl::parseMappedAttribute(attr);
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

bool HTMLStyleElementImpl::disabled() const
{
    return !getAttribute(ATTR_DISABLED).isNull();
}

void HTMLStyleElementImpl::setDisabled(bool disabled)
{
    setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

DOMString HTMLStyleElementImpl::media() const
{
    return getAttribute(ATTR_MEDIA);
}

void HTMLStyleElementImpl::setMedia(const DOMString &value)
{
    setAttribute(ATTR_MEDIA, value);
}

DOMString HTMLStyleElementImpl::type() const
{
    return getAttribute(ATTR_TYPE);
}

void HTMLStyleElementImpl::setType(const DOMString &value)
{
    setAttribute(ATTR_TYPE, value);
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
    // Only allow title to be set by first <title> encountered.
    if (getDocument()->title().isEmpty())
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
    // Only allow title to be set by first <title> encountered.
    if (inDocument() && getDocument()->title().isEmpty())
	getDocument()->setTitle(m_title);
}

DOMString HTMLTitleElementImpl::text() const
{
    // FIXME: Obviously wrong! There's no "text" attribute on a title element.
    // Need to do something with the children perhaps?
    return getAttribute(ATTR_TEXT);
}

void HTMLTitleElementImpl::setText(const DOMString &value)
{
    // FIXME: Obviously wrong! There's no "text" attribute on a title element.
    // Need to do something with the children perhaps?
    setAttribute(ATTR_TEXT, value);
}
