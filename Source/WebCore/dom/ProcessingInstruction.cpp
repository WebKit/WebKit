/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "ProcessingInstruction.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedXSLStyleSheet.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "XSLStyleSheet.h"
#include "XMLDocumentParser.h" // for parseAttributes()
#include "MediaList.h"
#include "StyleSheetContents.h"

namespace WebCore {

inline ProcessingInstruction::ProcessingInstruction(Document& document, const String& target, const String& data)
    : CharacterData(document, data, CreateOther)
    , m_target(target)
    , m_cachedSheet(0)
    , m_loading(false)
    , m_alternate(false)
    , m_createdByParser(false)
    , m_isCSS(false)
#if ENABLE(XSLT)
    , m_isXSL(false)
#endif
{
}

PassRefPtr<ProcessingInstruction> ProcessingInstruction::create(Document& document, const String& target, const String& data)
{
    return adoptRef(new ProcessingInstruction(document, target, data));
}

ProcessingInstruction::~ProcessingInstruction()
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_cachedSheet)
        m_cachedSheet->removeClient(this);

    if (inDocument())
        document().styleSheetCollection().removeStyleSheetCandidateNode(*this);
}

String ProcessingInstruction::nodeName() const
{
    return m_target;
}

Node::NodeType ProcessingInstruction::nodeType() const
{
    return PROCESSING_INSTRUCTION_NODE;
}

PassRefPtr<Node> ProcessingInstruction::cloneNode(bool /*deep*/)
{
    // FIXME: Is it a problem that this does not copy m_localHref?
    // What about other data members?
    return create(document(), m_target, data());
}

void ProcessingInstruction::checkStyleSheet()
{
    if (m_target == "xml-stylesheet" && document().frame() && parentNode() == &document()) {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        bool attrsOk;
        const HashMap<String, String> attrs = parseAttributes(data(), attrsOk);
        if (!attrsOk)
            return;
        HashMap<String, String>::const_iterator i = attrs.find("type");
        String type;
        if (i != attrs.end())
            type = i->value;

        m_isCSS = type.isEmpty() || type == "text/css";
#if ENABLE(XSLT)
        m_isXSL = (type == "text/xml" || type == "text/xsl" || type == "application/xml" ||
                   type == "application/xhtml+xml" || type == "application/rss+xml" || type == "application/atom+xml");
        if (!m_isCSS && !m_isXSL)
#else
        if (!m_isCSS)
#endif
            return;

        String href = attrs.get("href");
        String alternate = attrs.get("alternate");
        m_alternate = alternate == "yes";
        m_title = attrs.get("title");
        m_media = attrs.get("media");

        if (m_alternate && m_title.isEmpty())
            return;

        if (href.length() > 1 && href[0] == '#') {
            m_localHref = href.substring(1);
#if ENABLE(XSLT)
            // We need to make a synthetic XSLStyleSheet that is embedded.  It needs to be able
            // to kick off import/include loads that can hang off some parent sheet.
            if (m_isXSL) {
                URL finalURL(ParsedURLString, m_localHref);
                m_sheet = XSLStyleSheet::createEmbedded(this, finalURL);
                m_loading = false;
            }
#endif
        } else {
            if (m_cachedSheet) {
                m_cachedSheet->removeClient(this);
                m_cachedSheet = 0;
            }
            
            String url = document().completeURL(href).string();
            if (!dispatchBeforeLoadEvent(url))
                return;
            
            m_loading = true;
            document().styleSheetCollection().addPendingSheet();
            
            CachedResourceRequest request(ResourceRequest(document().completeURL(href)));
#if ENABLE(XSLT)
            if (m_isXSL)
                m_cachedSheet = document().cachedResourceLoader()->requestXSLStyleSheet(request);
            else
#endif
            {
                String charset = attrs.get("charset");
                if (charset.isEmpty())
                    charset = document().charset();
                request.setCharset(charset);

                m_cachedSheet = document().cachedResourceLoader()->requestCSSStyleSheet(request);
            }
            if (m_cachedSheet)
                m_cachedSheet->addClient(this);
            else {
                // The request may have been denied if (for example) the stylesheet is local and the document is remote.
                m_loading = false;
                document().styleSheetCollection().removePendingSheet();
            }
        }
    }
}

bool ProcessingInstruction::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->isLoading();
}

bool ProcessingInstruction::sheetLoaded()
{
    if (!isLoading()) {
        document().styleSheetCollection().removePendingSheet();
        return true;
    }
    return false;
}

void ProcessingInstruction::setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet)
{
    if (!inDocument()) {
        ASSERT(!m_sheet);
        return;
    }

    ASSERT(m_isCSS);
    CSSParserContext parserContext(document(), baseURL, charset);

    auto cssSheet = CSSStyleSheet::create(StyleSheetContents::create(href, parserContext), this);
    cssSheet.get().setDisabled(m_alternate);
    cssSheet.get().setTitle(m_title);
    cssSheet.get().setMediaQueries(MediaQuerySet::create(m_media));

    m_sheet = std::move(cssSheet);

    // We don't need the cross-origin security check here because we are
    // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
    // type.
    parseStyleSheet(sheet->sheetText(true));
}

#if ENABLE(XSLT)
void ProcessingInstruction::setXSLStyleSheet(const String& href, const URL& baseURL, const String& sheet)
{
    ASSERT(m_isXSL);
    m_sheet = XSLStyleSheet::create(this, href, baseURL);
    parseStyleSheet(sheet);
}
#endif

void ProcessingInstruction::parseStyleSheet(const String& sheet)
{
    if (m_isCSS)
        static_cast<CSSStyleSheet*>(m_sheet.get())->contents().parseString(sheet);
#if ENABLE(XSLT)
    else if (m_isXSL)
        static_cast<XSLStyleSheet*>(m_sheet.get())->parseString(sheet);
#endif

    if (m_cachedSheet)
        m_cachedSheet->removeClient(this);
    m_cachedSheet = 0;

    m_loading = false;

    if (m_isCSS)
        static_cast<CSSStyleSheet*>(m_sheet.get())->contents().checkLoaded();
#if ENABLE(XSLT)
    else if (m_isXSL)
        static_cast<XSLStyleSheet*>(m_sheet.get())->checkLoaded();
#endif
}

void ProcessingInstruction::setCSSStyleSheet(PassRefPtr<CSSStyleSheet> sheet)
{
    ASSERT(!m_cachedSheet);
    ASSERT(!m_loading);
    m_sheet = sheet;
    sheet->setTitle(m_title);
    sheet->setDisabled(m_alternate);
}

void ProcessingInstruction::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    if (!sheet())
        return;
    
    addSubresourceURL(urls, sheet()->baseURL());
}

Node::InsertionNotificationRequest ProcessingInstruction::insertedInto(ContainerNode& insertionPoint)
{
    CharacterData::insertedInto(insertionPoint);
    if (!insertionPoint.inDocument())
        return InsertionDone;
    document().styleSheetCollection().addStyleSheetCandidateNode(*this, m_createdByParser);
    checkStyleSheet();
    return InsertionDone;
}

void ProcessingInstruction::removedFrom(ContainerNode& insertionPoint)
{
    CharacterData::removedFrom(insertionPoint);
    if (!insertionPoint.inDocument())
        return;
    
    document().styleSheetCollection().removeStyleSheetCandidateNode(*this);

    if (m_sheet) {
        ASSERT(m_sheet->ownerNode() == this);
        m_sheet->clearOwnerNode();
        m_sheet = 0;
    }

    // If we're in document teardown, then we don't need to do any notification of our sheet's removal.
    if (document().hasLivingRenderTree())
        document().styleResolverChanged(DeferRecalcStyle);
}

void ProcessingInstruction::finishParsingChildren()
{
    m_createdByParser = false;
    CharacterData::finishParsingChildren();
}

} // namespace
