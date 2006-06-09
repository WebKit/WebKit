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
#include "config.h"
#include "HTMLLinkElement.h"

#include "CachedCSSStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "csshelper.h"
#include "MediaQueryEvaluator.h"

namespace WebCore {

using namespace HTMLNames;

HTMLLinkElement::HTMLLinkElement(Document *doc)
    : HTMLElement(linkTag, doc)
    , m_cachedSheet(0)
    , m_disabledState(0)
    , m_loading(false)
    , m_alternate(false)
    , m_isStyleSheet(false)
    , m_isIcon(false)
{
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_cachedSheet) {
        m_cachedSheet->deref(this);
        if (m_loading && !isDisabled() && !isAlternate())
            document()->stylesheetLoaded();
    }
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
                document()->stylesheetLoaded();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_alternate && m_disabledState == 1)
                document()->addPendingSheet();

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script.  It takes really terrible code to make this
            // happen (a double toggle for no reason essentially).  This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_alternate && m_disabledState == 1 && oldDisabledState == 2)
                document()->addPendingSheet();

            // If the sheet is already loading just bail.
            return;
        }

        // Load the sheet, since it's never been loaded before.
        if (!m_sheet && m_disabledState == 1)
            process();
        else
            document()->updateStyleSelector(); // Update the style selector.
    }
}

StyleSheet* HTMLLinkElement::sheet() const
{
    return m_sheet.get();
}

void HTMLLinkElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == relAttr) {
        tokenizeRelAttribute(attr->value());
        process();
    } else if (attr->name() == hrefAttr) {
        m_url = document()->completeURL(parseURL(attr->value()));
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

    String type = m_type.lower();
    
    Frame* frame = document()->frame();

    // IE extension: location of small icon for locationbar / bookmarks
    if (frame && m_isIcon && !m_url.isEmpty() && !frame->tree()->parent()) {
        if (!type.isEmpty()) // Mozilla extension to IE extension: icon specified with type
            frame->browserExtension()->setTypedIconURL(KURL(m_url.deprecatedString()), type);
        else 
            frame->browserExtension()->setIconURL(KURL(m_url.deprecatedString()));
    }

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if (m_disabledState != 2 && (type.contains("text/css") || m_isStyleSheet) && document()->frame()) {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
        // also, don't load style sheets for standalone documents
        MediaQueryEvaluator allEval(true);
        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        RefPtr<MediaList> media = new MediaList((CSSStyleSheet*)0, m_media, true);
        if (allEval.eval(media.get()) || screenEval.eval(media.get()) || printEval.eval(media.get())) {

            // Add ourselves as a pending sheet, but only if we aren't an alternate 
            // stylesheet.  Alternate stylesheets don't hold up render tree construction.
            if (!isAlternate())
                document()->addPendingSheet();
            
            DeprecatedString chset = getAttribute(charsetAttr).deprecatedString();
            if (m_cachedSheet) {
                if (m_loading) {
                    document()->stylesheetLoaded();
                }
                m_cachedSheet->deref(this);
            }
            m_loading = true;
            m_cachedSheet = document()->docLoader()->requestStyleSheet(m_url, chset);
            if (m_cachedSheet)
                m_cachedSheet->ref(this);
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        m_sheet = 0;
        document()->updateStyleSelector();
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

void HTMLLinkElement::setStyleSheet(const String& url, const String& sheetStr)
{
    m_sheet = new CSSStyleSheet(this, url);
    m_sheet->parseString(sheetStr, !document()->inCompatMode());

    RefPtr<MediaList> media = new MediaList((CSSStyleSheet*)0, m_media, true);
    m_sheet->setMedia(media.get());

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet && !isDisabled() && !isAlternate())
        document()->stylesheetLoaded();
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
        document()->stylesheetLoaded();
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

void HTMLLinkElement::setCharset(const String& value)
{
    setAttribute(charsetAttr, value);
}

String HTMLLinkElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

void HTMLLinkElement::setHref(const String& value)
{
    setAttribute(hrefAttr, value);
}

String HTMLLinkElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLLinkElement::setHreflang(const String& value)
{
    setAttribute(hreflangAttr, value);
}

String HTMLLinkElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLLinkElement::setMedia(const String& value)
{
    setAttribute(mediaAttr, value);
}

String HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLLinkElement::setRel(const String& value)
{
    setAttribute(relAttr, value);
}

String HTMLLinkElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLLinkElement::setRev(const String& value)
{
    setAttribute(revAttr, value);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLLinkElement::setTarget(const String& value)
{
    setAttribute(targetAttr, value);
}

String HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLinkElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

}
