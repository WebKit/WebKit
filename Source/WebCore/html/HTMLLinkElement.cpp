/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLLinkElement.h"

#include "Attribute.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "ResourceHandle.h"
#include "ScriptEventListener.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

inline HTMLLinkElement::HTMLLinkElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLElement(tagName, document)
    , m_linkLoader(this)
    , m_sizes(DOMSettableTokenList::create())
    , m_disabledState(Unset)
    , m_loading(false)
    , m_createdByParser(createdByParser)
    , m_isInShadowTree(false)
    , m_pendingSheetType(None)
{
    ASSERT(hasTagName(linkTag));
}

PassRefPtr<HTMLLinkElement> HTMLLinkElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLLinkElement(tagName, document, createdByParser));
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_cachedSheet) {
        m_cachedSheet->removeClient(this);
        removePendingSheet();
    }

    if (inDocument())
        document()->removeStyleSheetCandidateNode(this);
}

void HTMLLinkElement::setDisabledState(bool disabled)
{
    DisabledState oldDisabledState = m_disabledState;
    m_disabledState = disabled ? Disabled : EnabledViaScript;
    if (oldDisabledState != m_disabledState) {
        // If we change the disabled state while the sheet is still loading, then we have to
        // perform three checks:
        if (styleSheetIsLoading()) {
            // Check #1: The sheet becomes disabled while loading.
            if (m_disabledState == Disabled)
                removePendingSheet();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_relAttribute.m_isAlternate && m_disabledState == EnabledViaScript)
                addPendingSheet(Blocking);

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script. It takes really terrible code to make this
            // happen (a double toggle for no reason essentially). This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_relAttribute.m_isAlternate && m_disabledState == EnabledViaScript && oldDisabledState == Disabled)
                addPendingSheet(Blocking);

            // If the sheet is already loading just bail.
            return;
        }

        // Load the sheet, since it's never been loaded before.
        if (!m_sheet && m_disabledState == EnabledViaScript)
            process();
        else
            document()->styleSelectorChanged(DeferRecalcStyle); // Update the style selector.
    }
}

void HTMLLinkElement::parseAttribute(Attribute* attr)
{
    if (attr->name() == relAttr) {
        m_relAttribute = LinkRelAttribute(attr->value());
        process();
    } else if (attr->name() == hrefAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(attr->value());
        m_url = url.isEmpty() ? KURL() : document()->completeURL(url);
        process();
    } else if (attr->name() == typeAttr) {
        m_type = attr->value();
        process();
    } else if (attr->name() == sizesAttr) {
        setSizes(attr->value());
        process();
    } else if (attr->name() == mediaAttr) {
        m_media = attr->value().string().lower();
        process();
    } else if (attr->name() == disabledAttr)
        setDisabledState(!attr->isNull());
    else if (attr->name() == onbeforeloadAttr)
        setAttributeEventListener(eventNames().beforeloadEvent, createAttributeEventListener(this, attr));
#if ENABLE(LINK_PREFETCH)
    else if (attr->name() == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onerrorAttr)
        setAttributeEventListener(eventNames().errorEvent, createAttributeEventListener(this, attr));
#endif
    else {
        if (attr->name() == titleAttr && m_sheet)
            m_sheet->setTitle(attr->value());
        HTMLElement::parseAttribute(attr);
    }
}

bool HTMLLinkElement::shouldLoadLink()
{
    RefPtr<Document> originalDocument = document();
    if (!dispatchBeforeLoadEvent(m_url))
        return false;
    // A beforeload handler might have removed us from the document or changed the document.
    if (!inDocument() || document() != originalDocument)
        return false;
    return true;
}

void HTMLLinkElement::process()
{
    if (!inDocument() || m_isInShadowTree) {
        ASSERT(!m_sheet);
        return;
    }

    String type = m_type.lower();

    if (!m_linkLoader.loadLink(m_relAttribute, type, m_sizes->toString(), m_url, document()))
        return;

    bool acceptIfTypeContainsTextCSS = document()->page() && document()->page()->settings() && document()->page()->settings()->treatsAnyTextCSSLinkAsStylesheet();

    if (m_disabledState != Disabled && (m_relAttribute.m_isStyleSheet || (acceptIfTypeContainsTextCSS && type.contains("text/css")))
        && document()->frame() && m_url.isValid()) {
        
        String charset = getAttribute(charsetAttr);
        if (charset.isEmpty() && document()->frame())
            charset = document()->charset();
        
        if (m_cachedSheet) {
            removePendingSheet();
            m_cachedSheet->removeClient(this);
            m_cachedSheet = 0;
        }

        if (!shouldLoadLink())
            return;

        m_loading = true;

        bool mediaQueryMatches = true;
        if (!m_media.isEmpty()) {
            RefPtr<RenderStyle> documentStyle = CSSStyleSelector::styleForDocument(document());
            RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(m_media);
            MediaQueryEvaluator evaluator(document()->frame()->view()->mediaType(), document()->frame(), documentStyle.get());
            mediaQueryMatches = evaluator.eval(media.get());
        }

        // Don't hold up render tree construction and script execution on stylesheets
        // that are not needed for the rendering at the moment.
        bool blocking = mediaQueryMatches && !isAlternate();
        addPendingSheet(blocking ? Blocking : NonBlocking);

        // Load stylesheets that are not needed for the rendering immediately with low priority.
        ResourceLoadPriority priority = blocking ? ResourceLoadPriorityUnresolved : ResourceLoadPriorityVeryLow;
        ResourceRequest request(document()->completeURL(m_url));
        m_cachedSheet = document()->cachedResourceLoader()->requestCSSStyleSheet(request, charset, priority);
        
        if (m_cachedSheet)
            m_cachedSheet->addClient(this);
        else {
            // The request may have been denied if (for example) the stylesheet is local and the document is remote.
            m_loading = false;
            removePendingSheet();
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        m_sheet = 0;
        document()->styleSelectorChanged(DeferRecalcStyle);
    }
}

void HTMLLinkElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    m_isInShadowTree = isInShadowTree();
    if (m_isInShadowTree)
        return;

    document()->addStyleSheetCandidateNode(this, m_createdByParser);

    process();
}

void HTMLLinkElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();

    if (m_isInShadowTree) {
        ASSERT(!m_sheet);
        return;
    }
    document()->removeStyleSheetCandidateNode(this);

    if (m_sheet) {
        ASSERT(m_sheet->ownerNode() == this);
        m_sheet->clearOwnerNode();
        m_sheet = 0;
    }

    if (document()->renderer())
        document()->styleSelectorChanged(DeferRecalcStyle);
}

void HTMLLinkElement::finishParsingChildren()
{
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLLinkElement::setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet)
{
    if (!inDocument()) {
        ASSERT(!m_sheet);
        return;
    }

    m_sheet = CSSStyleSheet::create(this, href, baseURL, charset);

    bool strictParsing = !document()->inQuirksMode();
    bool enforceMIMEType = strictParsing;
    bool crossOriginCSS = false;
    bool validMIMEType = false;
    bool needsSiteSpecificQuirks = document()->page() && document()->page()->settings()->needsSiteSpecificQuirks();

    // Check to see if we should enforce the MIME type of the CSS resource in strict mode.
    // Running in iWeb 2 is one example of where we don't want to - <rdar://problem/6099748>
    if (enforceMIMEType && document()->page() && !document()->page()->settings()->enforceCSSMIMETypeInNoQuirksMode())
        enforceMIMEType = false;

#ifdef BUILDING_ON_LEOPARD
    if (enforceMIMEType && needsSiteSpecificQuirks) {
        // Covers both http and https, with or without "www."
        if (baseURL.string().contains("mcafee.com/japan/", false))
            enforceMIMEType = false;
    }
#endif

    String sheetText = sheet->sheetText(enforceMIMEType, &validMIMEType);
    m_sheet->parseString(sheetText, strictParsing);

    // If we're loading a stylesheet cross-origin, and the MIME type is not
    // standard, require the CSS to at least start with a syntactically
    // valid CSS rule.
    // This prevents an attacker playing games by injecting CSS strings into
    // HTML, XML, JSON, etc. etc.
    if (!document()->securityOrigin()->canRequest(baseURL))
        crossOriginCSS = true;

    if (crossOriginCSS && !validMIMEType && !m_sheet->hasSyntacticallyValidCSSHeader())
        m_sheet = CSSStyleSheet::create(this, href, baseURL, charset);

    if (strictParsing && needsSiteSpecificQuirks) {
        // Work around <https://bugs.webkit.org/show_bug.cgi?id=28350>.
        DEFINE_STATIC_LOCAL(const String, slashKHTMLFixesDotCss, ("/KHTMLFixes.css"));
        DEFINE_STATIC_LOCAL(const String, mediaWikiKHTMLFixesStyleSheet, ("/* KHTML fix stylesheet */\n/* work around the horizontal scrollbars */\n#column-content { margin-left: 0; }\n\n"));
        // There are two variants of KHTMLFixes.css. One is equal to mediaWikiKHTMLFixesStyleSheet,
        // while the other lacks the second trailing newline.
        if (baseURL.string().endsWith(slashKHTMLFixesDotCss) && !sheetText.isNull() && mediaWikiKHTMLFixesStyleSheet.startsWith(sheetText)
                && sheetText.length() >= mediaWikiKHTMLFixesStyleSheet.length() - 1) {
            ASSERT(m_sheet->length() == 1);
            ExceptionCode ec;
            m_sheet->deleteRule(0, ec);
        }
    }

    m_sheet->setTitle(title());

    RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(m_media);
    m_sheet->setMedia(media.get());

    m_loading = false;
    m_sheet->checkLoaded();
}

bool HTMLLinkElement::styleSheetIsLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->isLoading();
}

void HTMLLinkElement::linkLoaded()
{
    dispatchEvent(Event::create(eventNames().loadEvent, false, false));
}

void HTMLLinkElement::linkLoadingErrored()
{
    dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

bool HTMLLinkElement::sheetLoaded()
{
    if (!styleSheetIsLoading()) {
        removePendingSheet();
        return true;
    }
    return false;
}

void HTMLLinkElement::startLoadingDynamicSheet()
{
    // We don't support multiple blocking sheets.
    ASSERT(m_pendingSheetType < Blocking);
    addPendingSheet(Blocking);
}

bool HTMLLinkElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == hrefAttr || HTMLElement::isURLAttribute(attr);
}

KURL HTMLLinkElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

String HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

String HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLinkElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    // Favicons are handled by a special case in LegacyWebArchive::create()
    if (m_relAttribute.m_iconType != InvalidIcon)
        return;

    if (!m_relAttribute.m_isStyleSheet)
        return;
    
    // Append the URL of this link element.
    addSubresourceURL(urls, href());
    
    // Walk the URLs linked by the linked-to stylesheet.
    if (CSSStyleSheet* styleSheet = const_cast<HTMLLinkElement*>(this)->sheet())
        styleSheet->addSubresourceStyleURLs(urls);
}

void HTMLLinkElement::addPendingSheet(PendingSheetType type)
{
    if (type <= m_pendingSheetType)
        return;
    m_pendingSheetType = type;

    if (m_pendingSheetType == NonBlocking)
        return;
    document()->addPendingSheet();
}

void HTMLLinkElement::removePendingSheet()
{
    PendingSheetType type = m_pendingSheetType;
    m_pendingSheetType = None;

    if (type == None)
        return;
    if (type == NonBlocking) {
        // Document::removePendingSheet() triggers the style selector recalc for blocking sheets.
        document()->styleSelectorChanged(RecalcStyleImmediately);
        return;
    }
    document()->removePendingSheet();
}

DOMSettableTokenList* HTMLLinkElement::sizes() const
{
    return m_sizes.get();
}

void HTMLLinkElement::setSizes(const String& value)
{
    m_sizes->setValue(value);
}

#if ENABLE(MICRODATA)
String HTMLLinkElement::itemValueText() const
{
    return getURLAttribute(hrefAttr);
}

void HTMLLinkElement::setItemValueText(const String& value, ExceptionCode&)
{
    setAttribute(hrefAttr, value);
}
#endif

} // namespace WebCore
