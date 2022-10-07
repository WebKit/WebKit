/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
#include "CommonAtomStrings.h"
#include "DocumentInlines.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MediaList.h"
#include "MediaQueryParserContext.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "XMLDocumentParser.h"
#include "XSLStyleSheet.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ProcessingInstruction);

inline ProcessingInstruction::ProcessingInstruction(Document& document, String&& target, String&& data)
    : CharacterData(document, WTFMove(data))
    , m_target(WTFMove(target))
{
}

Ref<ProcessingInstruction> ProcessingInstruction::create(Document& document, String&& target, String&& data)
{
    return adoptRef(*new ProcessingInstruction(document, WTFMove(target), WTFMove(data)));
}

ProcessingInstruction::~ProcessingInstruction()
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_cachedSheet)
        m_cachedSheet->removeClient(*this);

    if (isConnected())
        document().styleScope().removeStyleSheetCandidateNode(*this);
}

String ProcessingInstruction::nodeName() const
{
    return m_target;
}

Node::NodeType ProcessingInstruction::nodeType() const
{
    return PROCESSING_INSTRUCTION_NODE;
}

Ref<Node> ProcessingInstruction::cloneNodeInternal(Document& targetDocument, CloningOperation)
{
    // FIXME: Is it a problem that this does not copy m_localHref?
    // What about other data members?
    return create(targetDocument, String { m_target }, String { data() });
}

void ProcessingInstruction::checkStyleSheet()
{
    if (m_target == "xml-stylesheet"_s && document().frame() && parentNode() == &document()) {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        auto attributes = parseAttributes(data());
        if (!attributes)
            return;
        String type = attributes->get<HashTranslatorASCIILiteral>("type"_s);

        m_isCSS = type.isEmpty() || type == cssContentTypeAtom();
#if ENABLE(XSLT)
        m_isXSL = type == "text/xml"_s || type == "text/xsl"_s || type == "application/xml"_s || type == "application/xhtml+xml"_s || type == "application/rss+xml"_s || type == "application/atom+xml"_s;
        if (!m_isCSS && !m_isXSL)
#else
        if (!m_isCSS)
#endif
            return;

        String href = attributes->get<HashTranslatorASCIILiteral>("href"_s);
        String alternate = attributes->get<HashTranslatorASCIILiteral>("alternate"_s);
        m_alternate = alternate == "yes"_s;
        m_title = attributes->get<HashTranslatorASCIILiteral>("title"_s);
        m_media = attributes->get<HashTranslatorASCIILiteral>("media"_s);

        if (m_alternate && m_title.isEmpty())
            return;

        if (href.length() > 1 && href[0] == '#') {
            m_localHref = href.substring(1);
#if ENABLE(XSLT)
            // We need to make a synthetic XSLStyleSheet that is embedded.  It needs to be able
            // to kick off import/include loads that can hang off some parent sheet.
            if (m_isXSL) {
                URL finalURL({ }, m_localHref);
                m_sheet = XSLStyleSheet::createEmbedded(*this, finalURL);
                m_loading = false;
                document().scheduleToApplyXSLTransforms();
            }
#endif
        } else {
            if (m_cachedSheet) {
                m_cachedSheet->removeClient(*this);
                m_cachedSheet = nullptr;
            }

            if (m_loading) {
                m_loading = false;
                document().styleScope().removePendingSheet(*this);
            }

            Ref<Document> originalDocument = document();

            String url = document().completeURL(href).string();

            bool didEventListenerDisconnectThisElement = !isConnected() || &document() != originalDocument.ptr();
            if (didEventListenerDisconnectThisElement)
                return;
            
            m_loading = true;
            document().styleScope().addPendingSheet(*this);

            ASSERT_WITH_SECURITY_IMPLICATION(!m_cachedSheet);

#if ENABLE(XSLT)
            if (m_isXSL) {
                auto options = CachedResourceLoader::defaultCachedResourceOptions();
                options.mode = FetchOptions::Mode::SameOrigin;
                m_cachedSheet = document().cachedResourceLoader().requestXSLStyleSheet({ResourceRequest(document().completeURL(href)), options}).value_or(nullptr);
            } else
#endif
            {
                String charset = attributes->get<HashTranslatorASCIILiteral>("charset"_s);
                CachedResourceRequest request(document().completeURL(href), CachedResourceLoader::defaultCachedResourceOptions(), std::nullopt, charset.isEmpty() ? document().charset() : WTFMove(charset));

                m_cachedSheet = document().cachedResourceLoader().requestCSSStyleSheet(WTFMove(request)).value_or(nullptr);
            }
            if (m_cachedSheet)
                m_cachedSheet->addClient(*this);
            else {
                // The request may have been denied if (for example) the stylesheet is local and the document is remote.
                m_loading = false;
                document().styleScope().removePendingSheet(*this);
#if ENABLE(XSLT)
                if (m_isXSL)
                    document().scheduleToApplyXSLTransforms();
#endif
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
        if (document().styleScope().hasPendingSheet(*this))
            document().styleScope().removePendingSheet(*this);
#if ENABLE(XSLT)
        if (m_isXSL)
            document().scheduleToApplyXSLTransforms();
#endif
        return true;
    }
    return false;
}

void ProcessingInstruction::setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet)
{
    if (!isConnected()) {
        ASSERT(!m_sheet);
        return;
    }

    ASSERT(m_isCSS);
    CSSParserContext parserContext(document(), baseURL, charset);

    auto cssSheet = CSSStyleSheet::create(StyleSheetContents::create(href, parserContext), *this);
    cssSheet.get().setDisabled(m_alternate);
    cssSheet.get().setTitle(m_title);
    cssSheet.get().setMediaQueries(MediaQuerySet::create(m_media, MediaQueryParserContext(document())));

    m_sheet = WTFMove(cssSheet);

    // We don't need the cross-origin security check here because we are
    // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
    // type.
    Ref<Document> protect(document());
    parseStyleSheet(sheet->sheetText());
}

#if ENABLE(XSLT)
void ProcessingInstruction::setXSLStyleSheet(const String& href, const URL& baseURL, const String& sheet)
{
    ASSERT(m_isXSL);
    m_sheet = XSLStyleSheet::create(*this, href, baseURL);
    Ref<Document> protect(document());
    parseStyleSheet(sheet);
}
#endif

void ProcessingInstruction::parseStyleSheet(const String& sheet)
{
    if (m_isCSS)
        downcast<CSSStyleSheet>(*m_sheet).contents().parseString(sheet);
#if ENABLE(XSLT)
    else if (m_isXSL)
        downcast<XSLStyleSheet>(*m_sheet).parseString(sheet);
#endif

    if (m_cachedSheet)
        m_cachedSheet->removeClient(*this);
    m_cachedSheet = nullptr;

    m_loading = false;

    if (m_isCSS)
        downcast<CSSStyleSheet>(*m_sheet).contents().checkLoaded();
#if ENABLE(XSLT)
    else if (m_isXSL)
        downcast<XSLStyleSheet>(*m_sheet).checkLoaded();
#endif
}

void ProcessingInstruction::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    if (!sheet())
        return;
    
    addSubresourceURL(urls, sheet()->baseURL());
}

Node::InsertedIntoAncestorResult ProcessingInstruction::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    CharacterData::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    document().styleScope().addStyleSheetCandidateNode(*this, m_createdByParser);
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void ProcessingInstruction::didFinishInsertingNode()
{
    checkStyleSheet();
}

void ProcessingInstruction::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    CharacterData::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (!removalType.disconnectedFromDocument)
        return;
    
    document().styleScope().removeStyleSheetCandidateNode(*this);

    if (m_sheet) {
        ASSERT(m_sheet->ownerNode() == this);
        m_sheet->clearOwnerNode();
        m_sheet = nullptr;
    }

    if (m_loading) {
        m_loading = false;
        document().styleScope().removePendingSheet(*this);
    }

    document().styleScope().didChangeActiveStyleSheetCandidates();
}

void ProcessingInstruction::finishParsingChildren()
{
    m_createdByParser = false;
    CharacterData::finishParsingChildren();
}

} // namespace
