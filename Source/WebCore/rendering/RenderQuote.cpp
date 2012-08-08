/**
 * Copyright (C) 2011 Nokia Inc.  All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderQuote.h"

#include <wtf/text/AtomicString.h>

#define U(x) ((const UChar*)L##x)

namespace WebCore {

RenderQuote::RenderQuote(Document* node, QuoteType quote)
    : RenderText(node, StringImpl::empty())
    , m_type(quote)
    , m_depth(0)
    , m_next(0)
    , m_previous(0)
    , m_attached(false)
{
}

RenderQuote::~RenderQuote()
{
    ASSERT(!m_attached);
    ASSERT(!m_next && !m_previous);
}

void RenderQuote::willBeDestroyed()
{
    detachQuote();
    RenderText::willBeDestroyed();
}

typedef HashMap<AtomicString, const QuotesData*, CaseFoldingHash> QuotesMap;

static const QuotesMap& quotesDataLanguageMap()
{
    DEFINE_STATIC_LOCAL(QuotesMap, staticQuotesMap, ());
    if (staticQuotesMap.size())
        return staticQuotesMap;
    // FIXME: Expand this table to include all the languages in https://bug-3234-attachments.webkit.org/attachment.cgi?id=2135
    staticQuotesMap.set("en", QuotesData::create(U("\x201C"), U("\x201D"), U("\x2018"), U("\x2019")).leakRef());
    staticQuotesMap.set("no", QuotesData::create(U("\x00AB"), U("\x00BB"), U("\x2039"), U("\x203A")).leakRef());
    staticQuotesMap.set("ro", QuotesData::create(U("\x201E"), U("\x201D")).leakRef());
    staticQuotesMap.set("ru", QuotesData::create(U("\x00AB"), U("\x00BB"), U("\x201E"), U("\x201C")).leakRef());
    return staticQuotesMap;
}

static const QuotesData* basicQuotesData()
{
    static const QuotesData* staticBasicQuotes = QuotesData::create(U("\""), U("\""), U("'"), U("'")).leakRef();
    return staticBasicQuotes;
}

PassRefPtr<StringImpl> RenderQuote::originalText() const
{
    switch (m_type) {
    case NO_OPEN_QUOTE:
    case NO_CLOSE_QUOTE:
        return StringImpl::empty();
    case CLOSE_QUOTE:
        // FIXME: When m_depth is 0 we should return empty string.
        return quotesData()->getCloseQuote(std::max(m_depth - 1, 0)).impl();
    case OPEN_QUOTE:
        return quotesData()->getOpenQuote(m_depth).impl();
    }
    ASSERT_NOT_REACHED();
    return StringImpl::empty();
}

void RenderQuote::computePreferredLogicalWidths(float lead)
{
    if (!m_attached)
        attachQuote();
    setTextInternal(originalText());
    RenderText::computePreferredLogicalWidths(lead);
}

const QuotesData* RenderQuote::quotesData() const
{
    if (QuotesData* customQuotes = style()->quotes())
        return customQuotes;

    AtomicString language = style()->locale();
    if (language.isNull())
        return basicQuotesData();
    const QuotesData* quotes = quotesDataLanguageMap().get(language);
    if (!quotes)
        return basicQuotesData();
    return quotes;
}

void RenderQuote::attachQuote()
{
    ASSERT(view());
    ASSERT(!m_attached);
    ASSERT(!m_next && !m_previous);

    // FIXME: Don't set pref widths dirty during layout. See updateDepth() for
    // more detail.
    if (!isRooted()) {
        setNeedsLayoutAndPrefWidthsRecalc();
        return;
    }

    if (!view()->renderQuoteHead()) {
        view()->setRenderQuoteHead(this);
        m_attached = true;
        return;
    }

    for (RenderObject* predecessor = previousInPreOrder(); predecessor; predecessor = predecessor->previousInPreOrder()) {
        if (!predecessor->isQuote())
            continue;
        m_previous = toRenderQuote(predecessor);
        m_next = m_previous->m_next;
        m_previous->m_next = this;
        if (m_next)
            m_next->m_previous = this;
        break;
    }

    if (!m_previous) {
        m_next = view()->renderQuoteHead();
        view()->setRenderQuoteHead(this);
    }
    m_attached = true;
    for (RenderQuote* quote = this; quote; quote = quote->m_next)
        quote->updateDepth();

    ASSERT(!m_next || m_next->m_attached);
    ASSERT(!m_previous || m_previous->m_attached);
}

void RenderQuote::detachQuote()
{
    ASSERT(!m_next || m_next->m_attached);
    ASSERT(!m_previous || m_previous->m_attached);
    if (!m_attached)
        return;
    if (m_previous)
        m_previous->m_next = m_next;
    else if (view())
        view()->setRenderQuoteHead(m_next);
    if (m_next)
        m_next->m_previous = m_previous;
    if (!documentBeingDestroyed()) {
        for (RenderQuote* quote = m_next; quote; quote = quote->m_next)
            quote->updateDepth();
    }
    m_attached = false;
    m_next = 0;
    m_previous = 0;
    m_depth = 0;
}

void RenderQuote::updateDepth()
{
    ASSERT(m_attached);
    int oldDepth = m_depth;
    m_depth = 0;
    if (m_previous) {
        m_depth = m_previous->m_depth;
        switch (m_previous->m_type) {
        case OPEN_QUOTE:
        case NO_OPEN_QUOTE:
            m_depth++;
            break;
        case CLOSE_QUOTE:
        case NO_CLOSE_QUOTE:
            if (m_depth)
                m_depth--;
            break;
        }
    }
    // FIXME: Don't call setNeedsLayout or dirty our preferred widths during layout.
    // This is likely to fail anyway as one of our ancestor will call setNeedsLayout(false),
    // preventing the future layout to occur on |this|. The solution is to move that to a
    // pre-layout phase.
    if (oldDepth != m_depth)
        setNeedsLayoutAndPrefWidthsRecalc();
}

} // namespace WebCore
