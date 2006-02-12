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

#include "config.h"
#include "html/html_headimpl.h"

#include "html/html_documentimpl.h"
#include "TextImpl.h"
#include "EventNames.h"
#include "dom_node.h"

#include "Frame.h"
#include "FrameTree.h"
#include "kjs_proxy.h"

#include "CachedCSSStyleSheet.h"
#include "CachedScript.h"
#include "DocLoader.h"
#include "helper.h"

#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/csshelper.h"
#include "htmlnames.h"

#include <kurl.h>

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

HTMLBaseElementImpl::HTMLBaseElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(baseTag, doc)
{
}

HTMLBaseElementImpl::~HTMLBaseElementImpl()
{
}

void HTMLBaseElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == hrefAttr) {
	m_href = parseURL(attr->value());
	process();
    } else if (attr->name() == targetAttr) {
    	m_target = attr->value();
	process();
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
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

    if(!m_href.isEmpty() && getDocument()->frame())
	getDocument()->setBaseURL( KURL( getDocument()->frame()->url(), m_href.qstring() ).url() );

    if(!m_target.isEmpty())
	getDocument()->setBaseTarget( m_target.qstring() );

    // ### should changing a document's base URL dynamically automatically update all images, stylesheets etc?
}

void HTMLBaseElementImpl::setHref(const DOMString &value)
{
    setAttribute(hrefAttr, value);
}

void HTMLBaseElementImpl::setTarget(const DOMString &value)
{
    setAttribute(targetAttr, value);
}

// -------------------------------------------------------------------------

HTMLLinkElementImpl::HTMLLinkElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(linkTag, doc)
{
    m_loading = false;
    m_cachedSheet = 0;
    m_isStyleSheet = m_isIcon = m_alternate = false;
    m_disabledState = 0;
}

HTMLLinkElementImpl::~HTMLLinkElementImpl()
{
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
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
    if (attr->name() == relAttr) {
        tokenizeRelAttribute(attr->value());
        process();
    } else if (attr->name() == hrefAttr) {
        m_url = getDocument()->completeURL(parseURL(attr->value()));
	process();
    } else if (attr->name() == typeAttr) {
        m_type = attr->value();
	process();
    } else if (attr->name() == mediaAttr) {
        m_media = attr->value().qstring().lower();
        process();
    } else if (attr->name() == disabledAttr) {
        setDisabledState(!attr->isNull());
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

void HTMLLinkElementImpl::tokenizeRelAttribute(const AtomicString& relStr)
{
    m_isStyleSheet = m_isIcon = m_alternate = false;
    QString rel = relStr.qstring().lower();
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

    QString type = m_type.qstring().lower();
    
    Frame *frame = getDocument()->frame();

    // IE extension: location of small icon for locationbar / bookmarks
    if (frame && m_isIcon && !m_url.isEmpty() && !frame->tree()->parent()) {
        if (!type.isEmpty()) // Mozilla extension to IE extension: icon specified with type
            frame->browserExtension()->setTypedIconURL(KURL(m_url.qstring()), type);
        else 
            frame->browserExtension()->setIconURL(KURL(m_url.qstring()));
    }

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if (m_disabledState != 2 && (type.contains("text/css") || m_isStyleSheet) && getDocument()->frame()) {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
	// also, don't load style sheets for standalone documents
        if (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print")) {
            m_loading = true;

            // Add ourselves as a pending sheet, but only if we aren't an alternate 
            // stylesheet.  Alternate stylesheets don't hold up render tree construction.
            if (!isAlternate())
                getDocument()->addPendingSheet();
            
            QString chset = getAttribute(charsetAttr).qstring();
            if (m_cachedSheet)
                m_cachedSheet->deref(this);
            m_cachedSheet = getDocument()->docLoader()->requestStyleSheet(m_url, chset);
            if (m_cachedSheet)
                m_cachedSheet->ref(this);
        }
    }
    else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
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
    m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->parseString(sheetStr, !getDocument()->inCompatMode());

    MediaListImpl *media = new MediaListImpl(m_sheet.get(), m_media);
    m_sheet->setMedia( media );

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet && !isDisabled() && !isAlternate())
        getDocument()->stylesheetLoaded();
}

bool HTMLLinkElementImpl::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheetImpl *>(m_sheet.get())->isLoading();
}

void HTMLLinkElementImpl::sheetLoaded()
{
    if (!isLoading() && !isDisabled() && !isAlternate())
        getDocument()->stylesheetLoaded();
}

bool HTMLLinkElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == hrefAttr;
}

bool HTMLLinkElementImpl::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLLinkElementImpl::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

DOMString HTMLLinkElementImpl::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLLinkElementImpl::setCharset(const DOMString &value)
{
    setAttribute(charsetAttr, value);
}

DOMString HTMLLinkElementImpl::href() const
{
    return getDocument()->completeURL(getAttribute(hrefAttr));
}

void HTMLLinkElementImpl::setHref(const DOMString &value)
{
    setAttribute(hrefAttr, value);
}

DOMString HTMLLinkElementImpl::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLLinkElementImpl::setHreflang(const DOMString &value)
{
    setAttribute(hreflangAttr, value);
}

DOMString HTMLLinkElementImpl::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLLinkElementImpl::setMedia(const DOMString &value)
{
    setAttribute(mediaAttr, value);
}

DOMString HTMLLinkElementImpl::rel() const
{
    return getAttribute(relAttr);
}

void HTMLLinkElementImpl::setRel(const DOMString &value)
{
    setAttribute(relAttr, value);
}

DOMString HTMLLinkElementImpl::rev() const
{
    return getAttribute(revAttr);
}

void HTMLLinkElementImpl::setRev(const DOMString &value)
{
    setAttribute(revAttr, value);
}

DOMString HTMLLinkElementImpl::target() const
{
    return getAttribute(targetAttr);
}

void HTMLLinkElementImpl::setTarget(const DOMString &value)
{
    setAttribute(targetAttr, value);
}

DOMString HTMLLinkElementImpl::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLinkElementImpl::setType(const DOMString &value)
{
    setAttribute(typeAttr, value);
}

// -------------------------------------------------------------------------

HTMLMetaElementImpl::HTMLMetaElementImpl(DocumentImpl *doc) : HTMLElementImpl(metaTag, doc)
{
}

HTMLMetaElementImpl::~HTMLMetaElementImpl()
{
}

void HTMLMetaElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == http_equivAttr) {
	m_equiv = attr->value();
	process();
    } else if (attr->name() == contentAttr) {
	m_content = attr->value();
	process();
    } else if (attr->name() == nameAttr) {
        // Do nothing.
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
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
    return getAttribute(contentAttr);
}

void HTMLMetaElementImpl::setContent(const DOMString &value)
{
    setAttribute(contentAttr, value);
}

DOMString HTMLMetaElementImpl::httpEquiv() const
{
    return getAttribute(http_equivAttr);
}

void HTMLMetaElementImpl::setHttpEquiv(const DOMString &value)
{
    setAttribute(http_equivAttr, value);
}

DOMString HTMLMetaElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLMetaElementImpl::setName(const DOMString &value)
{
    setAttribute(nameAttr, value);
}

DOMString HTMLMetaElementImpl::scheme() const
{
    return getAttribute(schemeAttr);
}

void HTMLMetaElementImpl::setScheme(const DOMString &value)
{
    setAttribute(schemeAttr, value);
}

// -------------------------------------------------------------------------

HTMLScriptElementImpl::HTMLScriptElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(scriptTag, doc), m_cachedScript(0), m_createdByParser(false), m_evaluated(false)
{
}

HTMLScriptElementImpl::~HTMLScriptElementImpl()
{
    if (m_cachedScript)
        m_cachedScript->deref(this);
}

bool HTMLScriptElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == srcAttr;
}

void HTMLScriptElementImpl::childrenChanged()
{
    // If a node is inserted as a child of the script element
    // and the script element has been inserted in the document
    // we evaluate the script.
    if (!m_createdByParser && inDocument() && firstChild())
        evaluateScript(getDocument()->URL(), text());
}

void HTMLScriptElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == srcAttr) {
        if (m_evaluated || m_cachedScript || m_createdByParser || !inDocument())
            return;

        // FIXME: Evaluate scripts in viewless documents.
        // See http://bugzilla.opendarwin.org/show_bug.cgi?id=5727
        if (!getDocument()->frame())
            return;
    
        const AtomicString& url = attr->value();
        if (!url.isEmpty()) {
            QString charset = getAttribute(charsetAttr).qstring();
            m_cachedScript = getDocument()->docLoader()->requestScript(url, charset);
            m_cachedScript->ref(this);
        }
    } else if (attrName == onerrorAttr) {
        setHTMLEventListener(errorEvent, attr);
    } else if (attrName == onloadAttr) {
        setHTMLEventListener(loadEvent, attr);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

void HTMLScriptElementImpl::closeRenderer()
{
    // The parser just reached </script>. If we have no src and no text,
    // allow dynamic loading later.
    if (getAttribute(srcAttr).isEmpty() && text().isEmpty())
        setCreatedByParser(false);
    HTMLElementImpl::closeRenderer();
}

void HTMLScriptElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();

    assert(!m_cachedScript);

    if (m_createdByParser)
        return;
    
    // FIXME: Eventually we'd like to evaluate scripts which are inserted into a 
    // viewless document but this'll do for now.
    // See http://bugzilla.opendarwin.org/show_bug.cgi?id=5727
    if (!getDocument()->frame())
        return;
    
    const AtomicString& url = getAttribute(srcAttr);
    if (!url.isEmpty()) {
        QString charset = getAttribute(charsetAttr).qstring();
        m_cachedScript = getDocument()->docLoader()->requestScript(url, charset);
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

    if (cs->errorOccurred())
        dispatchHTMLEvent(errorEvent, false, false);
    else {
        evaluateScript(cs->url(), cs->script());
        dispatchHTMLEvent(loadEvent, false, false);
    }

    cs->deref(this);
    m_cachedScript = 0;
}

void HTMLScriptElementImpl::evaluateScript(const DOMString& URL, const DOMString& script)
{
    if (m_evaluated)
        return;
    
    Frame *frame = getDocument()->frame();
    if (frame) {
        KJSProxyImpl *proxy = frame->jScript();
        if (proxy) {
            m_evaluated = true;
            proxy->evaluate(URL, 0, script, 0);
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
    
    appendChild(getDocument()->createTextNode(value.impl()), exceptioncode);
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
    return getAttribute(charsetAttr);
}

void HTMLScriptElementImpl::setCharset(const DOMString &value)
{
    setAttribute(charsetAttr, value);
}

bool HTMLScriptElementImpl::defer() const
{
    return !getAttribute(deferAttr).isNull();
}

void HTMLScriptElementImpl::setDefer(bool defer)
{
    setAttribute(deferAttr, defer ? "" : 0);
}

DOMString HTMLScriptElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

void HTMLScriptElementImpl::setSrc(const DOMString &value)
{
    setAttribute(srcAttr, value);
}

DOMString HTMLScriptElementImpl::type() const
{
    return getAttribute(typeAttr);
}

void HTMLScriptElementImpl::setType(const DOMString &value)
{
    setAttribute(typeAttr, value);
}

// -------------------------------------------------------------------------

HTMLStyleElementImpl::HTMLStyleElementImpl(DocumentImpl *doc) : HTMLElementImpl(styleTag, doc)
{
    m_loading = false;
}

// other stuff...
void HTMLStyleElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == typeAttr)
        m_type = attr->value().domString().lower();
    else if (attr->name() == mediaAttr)
        m_media = attr->value().qstring().lower();
    else
        HTMLElementImpl::parseMappedAttribute(attr);
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
        if (static_cast<CSSStyleSheetImpl *>(m_sheet.get())->isLoading())
            getDocument()->stylesheetLoaded(); // Remove ourselves from the sheet list.
        m_sheet = 0;
    }

    m_loading = false;
    if ((m_type.isEmpty() || m_type == "text/css") // Type must be empty or CSS
         && (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print"))) {
        getDocument()->addPendingSheet();
        m_loading = true;
        m_sheet = new CSSStyleSheetImpl(this);
        m_sheet->parseString(text, !getDocument()->inCompatMode());
        MediaListImpl *media = new MediaListImpl(m_sheet.get(), m_media);
        m_sheet->setMedia(media);
        m_loading = false;
    }

    if (!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

bool HTMLStyleElementImpl::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheetImpl *>(m_sheet.get())->isLoading();
}

void HTMLStyleElementImpl::sheetLoaded()
{
    if (!isLoading())
        getDocument()->stylesheetLoaded();
}

bool HTMLStyleElementImpl::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLStyleElementImpl::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

DOMString HTMLStyleElementImpl::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLStyleElementImpl::setMedia(const DOMString &value)
{
    setAttribute(mediaAttr, value);
}

DOMString HTMLStyleElementImpl::type() const
{
    return getAttribute(typeAttr);
}

void HTMLStyleElementImpl::setType(const DOMString &value)
{
    setAttribute(typeAttr, value);
}

// -------------------------------------------------------------------------

HTMLTitleElementImpl::HTMLTitleElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(titleTag, doc), m_title("")
{
}

HTMLTitleElementImpl::~HTMLTitleElementImpl()
{
}

void HTMLTitleElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();
    getDocument()->setTitle(m_title, this);
}

void HTMLTitleElementImpl::removedFromDocument()
{
    HTMLElementImpl::removedFromDocument();
    getDocument()->removeTitle(this);
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
        getDocument()->setTitle(m_title, this);
}

DOMString HTMLTitleElementImpl::text() const
{
    DOMString val = "";
    
    for (NodeImpl *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            val += static_cast<TextImpl *>(n)->data();
    }
    
    return val;
}

void HTMLTitleElementImpl::setText(const DOMString &value)
{
    int exceptioncode = 0;
    int numChildren = childNodeCount();
    
    if (numChildren == 1 && firstChild()->isTextNode()) {
        static_cast<DOM::TextImpl *>(firstChild())->setData(value, exceptioncode);
    } else {  
        if (numChildren > 0) {
            removeChildren();
        }
    
        appendChild(getDocument()->createTextNode(value.impl()), exceptioncode);
    }
}

}
