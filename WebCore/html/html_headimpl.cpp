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
#include "html_headimpl.h"

#include "CachedCSSStyleSheet.h"
#include "CachedScript.h"
#include "DocLoader.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "KURL.h"
#include "Text.h"
#include "css_stylesheetimpl.h"
#include "csshelper.h"
#include "cssstyleselector.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "kjs_proxy.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

HTMLBaseElement::HTMLBaseElement(Document *doc)
    : HTMLElement(baseTag, doc)
{
}

HTMLBaseElement::~HTMLBaseElement()
{
}

void HTMLBaseElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == hrefAttr) {
	m_href = parseURL(attr->value());
	process();
    } else if (attr->name() == targetAttr) {
    	m_target = attr->value();
	process();
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLBaseElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    process();
}

void HTMLBaseElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();

    // Since the document doesn't have a base element...
    // (This will break in the case of multiple base elements, but that's not valid anyway (?))
    getDocument()->setBaseURL( DeprecatedString::null );
    getDocument()->setBaseTarget( DeprecatedString::null );
}

void HTMLBaseElement::process()
{
    if (!inDocument())
	return;

    if(!m_href.isEmpty() && getDocument()->frame())
	getDocument()->setBaseURL( KURL( getDocument()->frame()->url(), m_href.deprecatedString() ).url() );

    if(!m_target.isEmpty())
	getDocument()->setBaseTarget( m_target.deprecatedString() );

    // ### should changing a document's base URL dynamically automatically update all images, stylesheets etc?
}

void HTMLBaseElement::setHref(const String &value)
{
    setAttribute(hrefAttr, value);
}

void HTMLBaseElement::setTarget(const String &value)
{
    setAttribute(targetAttr, value);
}

// -------------------------------------------------------------------------

HTMLLinkElement::HTMLLinkElement(Document *doc)
    : HTMLElement(linkTag, doc)
{
    m_loading = false;
    m_cachedSheet = 0;
    m_isStyleSheet = m_isIcon = m_alternate = false;
    m_disabledState = 0;
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void HTMLLinkElement::setDisabledState(bool _disabled)
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

void HTMLLinkElement::parseMappedAttribute(MappedAttribute *attr)
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
        m_media = attr->value().deprecatedString().lower();
        process();
    } else if (attr->name() == disabledAttr) {
        setDisabledState(!attr->isNull());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLLinkElement::tokenizeRelAttribute(const AtomicString& relStr)
{
    m_isStyleSheet = m_isIcon = m_alternate = false;
    DeprecatedString rel = relStr.deprecatedString().lower();
    if (rel == "stylesheet")
        m_isStyleSheet = true;
    else if (rel == "icon" || rel == "shortcut icon")
        m_isIcon = true;
    else if (rel == "alternate stylesheet" || rel == "stylesheet alternate")
        m_isStyleSheet = m_alternate = true;
    else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        rel.replace('\n', ' ');
        DeprecatedStringList list = DeprecatedStringList::split(' ', rel);        
        for (DeprecatedStringList::Iterator i = list.begin(); i != list.end(); ++i) {
            if (*i == "stylesheet")
                m_isStyleSheet = true;
            else if (*i == "alternate")
                m_alternate = true;
            else if (*i == "icon")
                m_isIcon = true;
        }
    }
}

void HTMLLinkElement::process()
{
    if (!inDocument())
        return;

    DeprecatedString type = m_type.deprecatedString().lower();
    
    Frame *frame = getDocument()->frame();

    // IE extension: location of small icon for locationbar / bookmarks
    if (frame && m_isIcon && !m_url.isEmpty() && !frame->tree()->parent()) {
        if (!type.isEmpty()) // Mozilla extension to IE extension: icon specified with type
            frame->browserExtension()->setTypedIconURL(KURL(m_url.deprecatedString()), type);
        else 
            frame->browserExtension()->setIconURL(KURL(m_url.deprecatedString()));
    }

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if (m_disabledState != 2 && (type.contains("text/css") || m_isStyleSheet) && getDocument()->frame()) {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
	// also, don't load style sheets for standalone documents
        if (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print")) {

            // Add ourselves as a pending sheet, but only if we aren't an alternate 
            // stylesheet.  Alternate stylesheets don't hold up render tree construction.
            if (!isAlternate())
                getDocument()->addPendingSheet();
            
            DeprecatedString chset = getAttribute(charsetAttr).deprecatedString();
            if (m_cachedSheet) {
                if (m_loading) {
                    getDocument()->stylesheetLoaded();
                }
                m_cachedSheet->deref(this);
            }
            m_loading = true;
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

void HTMLLinkElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    process();
}

void HTMLLinkElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    process();
}

void HTMLLinkElement::setStyleSheet(const String &url, const String &sheetStr)
{
    m_sheet = new CSSStyleSheet(this, url);
    m_sheet->parseString(sheetStr, !getDocument()->inCompatMode());

    MediaList *media = new MediaList(m_sheet.get(), m_media);
    m_sheet->setMedia( media );

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet && !isDisabled() && !isAlternate())
        getDocument()->stylesheetLoaded();
}

bool HTMLLinkElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

void HTMLLinkElement::sheetLoaded()
{
    if (!isLoading() && !isDisabled() && !isAlternate())
        getDocument()->stylesheetLoaded();
}

bool HTMLLinkElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == hrefAttr;
}

bool HTMLLinkElement::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLLinkElement::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

String HTMLLinkElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLLinkElement::setCharset(const String &value)
{
    setAttribute(charsetAttr, value);
}

String HTMLLinkElement::href() const
{
    return getDocument()->completeURL(getAttribute(hrefAttr));
}

void HTMLLinkElement::setHref(const String &value)
{
    setAttribute(hrefAttr, value);
}

String HTMLLinkElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLLinkElement::setHreflang(const String &value)
{
    setAttribute(hreflangAttr, value);
}

String HTMLLinkElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLLinkElement::setMedia(const String &value)
{
    setAttribute(mediaAttr, value);
}

String HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLLinkElement::setRel(const String &value)
{
    setAttribute(relAttr, value);
}

String HTMLLinkElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLLinkElement::setRev(const String &value)
{
    setAttribute(revAttr, value);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLLinkElement::setTarget(const String &value)
{
    setAttribute(targetAttr, value);
}

String HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLinkElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

// -------------------------------------------------------------------------

HTMLMetaElement::HTMLMetaElement(Document *doc) : HTMLElement(metaTag, doc)
{
}

HTMLMetaElement::~HTMLMetaElement()
{
}

void HTMLMetaElement::parseMappedAttribute(MappedAttribute *attr)
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
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLMetaElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    process();
}

void HTMLMetaElement::process()
{
    // Get the document to process the tag, but only if we're actually part of DOM tree (changing a meta tag while
    // it's not in the tree shouldn't have any effect on the document)
    if (inDocument() && !m_equiv.isNull() && !m_content.isNull())
	getDocument()->processHttpEquiv(m_equiv,m_content);
}

String HTMLMetaElement::content() const
{
    return getAttribute(contentAttr);
}

void HTMLMetaElement::setContent(const String &value)
{
    setAttribute(contentAttr, value);
}

String HTMLMetaElement::httpEquiv() const
{
    return getAttribute(http_equivAttr);
}

void HTMLMetaElement::setHttpEquiv(const String &value)
{
    setAttribute(http_equivAttr, value);
}

String HTMLMetaElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLMetaElement::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

String HTMLMetaElement::scheme() const
{
    return getAttribute(schemeAttr);
}

void HTMLMetaElement::setScheme(const String &value)
{
    setAttribute(schemeAttr, value);
}

// -------------------------------------------------------------------------

HTMLScriptElement::HTMLScriptElement(Document *doc)
    : HTMLElement(scriptTag, doc), m_cachedScript(0), m_createdByParser(false), m_evaluated(false)
{
}

HTMLScriptElement::~HTMLScriptElement()
{
    if (m_cachedScript)
        m_cachedScript->deref(this);
}

bool HTMLScriptElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
}

void HTMLScriptElement::childrenChanged()
{
    // If a node is inserted as a child of the script element
    // and the script element has been inserted in the document
    // we evaluate the script.
    if (!m_createdByParser && inDocument() && firstChild())
        evaluateScript(getDocument()->URL(), text());
}

void HTMLScriptElement::parseMappedAttribute(MappedAttribute *attr)
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
            DeprecatedString charset = getAttribute(charsetAttr).deprecatedString();
            m_cachedScript = getDocument()->docLoader()->requestScript(url, charset);
            m_cachedScript->ref(this);
        }
    } else if (attrName == onerrorAttr) {
        setHTMLEventListener(errorEvent, attr);
    } else if (attrName == onloadAttr) {
        setHTMLEventListener(loadEvent, attr);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLScriptElement::closeRenderer()
{
    // The parser just reached </script>. If we have no src and no text,
    // allow dynamic loading later.
    if (getAttribute(srcAttr).isEmpty() && text().isEmpty())
        setCreatedByParser(false);
    HTMLElement::closeRenderer();
}

void HTMLScriptElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

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
        DeprecatedString charset = getAttribute(charsetAttr).deprecatedString();
        m_cachedScript = getDocument()->docLoader()->requestScript(url, charset);
        m_cachedScript->ref(this);
        return;
    }

    // If there's an empty script node, we shouldn't evaluate the script
    // because if a script is inserted afterwards (by setting text or innerText)
    // it should be evaluated, and evaluateScript only evaluates a script once.
    String scriptString = text();    
    if (!scriptString.isEmpty())
        evaluateScript(getDocument()->URL(), scriptString);
}

void HTMLScriptElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();

    if (m_cachedScript) {
        m_cachedScript->deref(this);
        m_cachedScript = 0;
    }
}

void HTMLScriptElement::notifyFinished(CachedObject* o)
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

void HTMLScriptElement::evaluateScript(const String& URL, const String& script)
{
    if (m_evaluated)
        return;
    
    Frame *frame = getDocument()->frame();
    if (frame) {
        KJSProxy *proxy = frame->jScript();
        if (proxy) {
            m_evaluated = true;
            proxy->evaluate(URL, 0, script, 0);
            Document::updateDocumentsRendering();
        }
    }
}

String HTMLScriptElement::text() const
{
    String val = "";
    
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            val += static_cast<Text *>(n)->data();
    }
    
    return val;
}

void HTMLScriptElement::setText(const String &value)
{
    ExceptionCode ec = 0;
    int numChildren = childNodeCount();
    
    if (numChildren == 1 && firstChild()->isTextNode()) {
        static_cast<Text *>(firstChild())->setData(value, ec);
        return;
    }
    
    if (numChildren > 0) {
        removeChildren();
    }
    
    appendChild(getDocument()->createTextNode(value.impl()), ec);
}

String HTMLScriptElement::htmlFor() const
{
    // DOM Level 1 says: reserved for future use.
    return String();
}

void HTMLScriptElement::setHtmlFor(const String &/*value*/)
{
    // DOM Level 1 says: reserved for future use.
}

String HTMLScriptElement::event() const
{
    // DOM Level 1 says: reserved for future use.
    return String();
}

void HTMLScriptElement::setEvent(const String &/*value*/)
{
    // DOM Level 1 says: reserved for future use.
}

String HTMLScriptElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLScriptElement::setCharset(const String &value)
{
    setAttribute(charsetAttr, value);
}

bool HTMLScriptElement::defer() const
{
    return !getAttribute(deferAttr).isNull();
}

void HTMLScriptElement::setDefer(bool defer)
{
    setAttribute(deferAttr, defer ? "" : 0);
}

String HTMLScriptElement::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

void HTMLScriptElement::setSrc(const String &value)
{
    setAttribute(srcAttr, value);
}

String HTMLScriptElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLScriptElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

// -------------------------------------------------------------------------

HTMLStyleElement::HTMLStyleElement(Document *doc) : HTMLElement(styleTag, doc)
{
    m_loading = false;
}

// other stuff...
void HTMLStyleElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == typeAttr)
        m_type = attr->value().domString().lower();
    else if (attr->name() == mediaAttr)
        m_media = attr->value().deprecatedString().lower();
    else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLStyleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    if (m_sheet)
        getDocument()->updateStyleSelector();
}

void HTMLStyleElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    if (m_sheet)
        getDocument()->updateStyleSelector();
}

void HTMLStyleElement::childrenChanged()
{
    String text = "";

    for (Node* c = firstChild(); c; c = c->nextSibling())
	if (c->nodeType() == TEXT_NODE || c->nodeType() == CDATA_SECTION_NODE || c->nodeType() == COMMENT_NODE)
	    text += c->nodeValue();

    if (m_sheet) {
        if (static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading())
            getDocument()->stylesheetLoaded(); // Remove ourselves from the sheet list.
        m_sheet = 0;
    }

    m_loading = false;
    if ((m_type.isEmpty() || m_type == "text/css") // Type must be empty or CSS
         && (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print"))) {
        getDocument()->addPendingSheet();
        m_loading = true;
        m_sheet = new CSSStyleSheet(this);
        m_sheet->parseString(text, !getDocument()->inCompatMode());
        MediaList *media = new MediaList(m_sheet.get(), m_media);
        m_sheet->setMedia(media);
        m_loading = false;
    }

    if (!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

bool HTMLStyleElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

void HTMLStyleElement::sheetLoaded()
{
    if (!isLoading())
        getDocument()->stylesheetLoaded();
}

bool HTMLStyleElement::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLStyleElement::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

String HTMLStyleElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLStyleElement::setMedia(const String &value)
{
    setAttribute(mediaAttr, value);
}

String HTMLStyleElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLStyleElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

// -------------------------------------------------------------------------

HTMLTitleElement::HTMLTitleElement(Document *doc)
    : HTMLElement(titleTag, doc), m_title("")
{
}

HTMLTitleElement::~HTMLTitleElement()
{
}

void HTMLTitleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    getDocument()->setTitle(m_title, this);
}

void HTMLTitleElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    getDocument()->removeTitle(this);
}

void HTMLTitleElement::childrenChanged()
{
    HTMLElement::childrenChanged();
    m_title = "";
    for (Node* c = firstChild(); c != 0; c = c->nextSibling())
	if (c->nodeType() == TEXT_NODE || c->nodeType() == CDATA_SECTION_NODE)
	    m_title += c->nodeValue();
    if (inDocument())
        getDocument()->setTitle(m_title, this);
}

String HTMLTitleElement::text() const
{
    String val = "";
    
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            val += static_cast<Text *>(n)->data();
    }
    
    return val;
}

void HTMLTitleElement::setText(const String &value)
{
    ExceptionCode ec = 0;
    int numChildren = childNodeCount();
    
    if (numChildren == 1 && firstChild()->isTextNode()) {
        static_cast<Text *>(firstChild())->setData(value, ec);
    } else {  
        if (numChildren > 0) {
            removeChildren();
        }
    
        appendChild(getDocument()->createTextNode(value.impl()), ec);
    }
}

}
