/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include "StyleRuleImport.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "SecurityOrigin.h"
#include "StyleSheetContents.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

StyleRuleImport::LoadContext::LoadContext(CSSStyleSheet* rootStyleSheet, const CSSParserContext& parentParserContext)
    : rootStyleSheet(rootStyleSheet)
    , parentParserContext(parentParserContext)
{
}

PassRefPtr<StyleRuleImport> StyleRuleImport::create(const String& href, PassRefPtr<MediaQuerySet> media)
{
    return adoptRef(new StyleRuleImport(href, media));
}

StyleRuleImport::StyleRuleImport(const String& href, PassRefPtr<MediaQuerySet> media)
    : StyleRuleBase(Import, 0)
    , m_strHref(href)
    , m_mediaQueries(media)
    , m_cachedSheet(0)
{
    if (!m_mediaQueries)
        m_mediaQueries = MediaQuerySet::create(String());
}

StyleRuleImport::~StyleRuleImport()
{
    if (m_cachedSheet)
        m_cachedSheet->removeClient(this);
}

void StyleRuleImport::setCSSStyleSheet(const String& url, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet*)
{
    ASSERT(m_loadContext);
    ASSERT(!m_styleSheet);
    ASSERT(m_cachedSheet);

    OwnPtr<LoadContext> loadContext = m_loadContext.release();

    CSSParserContext parserContext = loadContext->parentParserContext;
    if (!baseURL.isNull())
        parserContext.baseURL = baseURL;
    parserContext.charset = charset;

    m_styleSheet = StyleSheetContents::create(url, parserContext);
    m_styleSheet->parseAuthorStyleSheet(m_cachedSheet.get(), loadContext->rootStyleSheet.get());

    loadContext->rootStyleSheet->contents()->checkLoadCompleted();
}

bool StyleRuleImport::isLoading() const
{
    return m_loadContext || (m_styleSheet && m_styleSheet->isLoading());
}

bool StyleRuleImport::hadLoadError() const
{
    return m_cachedSheet && m_cachedSheet->errorOccurred();
}

void StyleRuleImport::requestStyleSheet(CSSStyleSheet* rootSheet, const CSSParserContext& parserContext)
{
    ASSERT(!rootSheet->parentStyleSheet());
    ASSERT(!m_cachedSheet);

    Node* ownerNode = rootSheet->ownerNode();
    if (!ownerNode)
        return;
    Document* document = ownerNode->document();

    KURL resolvedURL;
    if (!parserContext.baseURL.isNull())
        resolvedURL = KURL(parserContext.baseURL, m_strHref);
    else
        resolvedURL = document->completeURL(m_strHref);

    StyleSheetContents* rootSheetContents = rootSheet->contents();
    if (rootSheetContents->hasImportCycle(this, resolvedURL, document->baseURL()))
        return;

    ResourceRequest request(resolvedURL);
    CachedResourceLoader* cachedResourceLoader = document->cachedResourceLoader();
    if (rootSheetContents->isUserStyleSheet())
        m_cachedSheet = cachedResourceLoader->requestUserCSSStyleSheet(request, parserContext.charset);
    else
        m_cachedSheet = cachedResourceLoader->requestCSSStyleSheet(request, parserContext.charset);

    if (!m_cachedSheet)
        return;
    // if the import rule is issued dynamically, the sheet may be
    // removed from the pending sheet count, so let the doc know
    // the sheet being imported is pending.
    if (rootSheetContents->loadCompleted())
        ownerNode->startLoadingDynamicSheet();

    m_loadContext = adoptPtr(new LoadContext(rootSheet, parserContext));
    m_cachedSheet->addClient(this);
}

void StyleRuleImport::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedMember(m_strHref);
    info.addInstrumentedMember(m_mediaQueries);
    info.addInstrumentedMember(m_styleSheet);
}

} // namespace WebCore
