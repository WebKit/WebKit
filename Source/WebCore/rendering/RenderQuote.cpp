/**
 * Copyright (C) 2011 Nokia Inc.  All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "RenderView.h"
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

void RenderQuote::willBeRemovedFromTree()
{
    RenderText::willBeRemovedFromTree();
    detachQuote();
}

void RenderQuote::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);
    setText(originalText());
}

typedef HashMap<AtomicString, const QuotesData*, CaseFoldingHash> QuotesMap;

static const QuotesMap& quotesDataLanguageMap()
{
    DEFINE_STATIC_LOCAL(QuotesMap, staticQuotesMap, ());
    if (staticQuotesMap.size())
        return staticQuotesMap;

    // Table of quotes from http://www.whatwg.org/specs/web-apps/current-work/multipage/rendering.html#quotes
    #define QUOTES_LANG(lang, o1, c1, o2, c2) staticQuotesMap.set(lang, QuotesData::create(U(o1), U(c1), U(o2), U(c2)).leakRef())
    QUOTES_LANG("af",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("agq",           "\x201e", "\x201d", "\x201a", "\x2019");
    QUOTES_LANG("ak",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("am",            "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("ar",            "\x201d", "\x201c", "\x2019", "\x2018");
    QUOTES_LANG("asa",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("az-Cyrl",       "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("bas",           "\x00ab", "\x00bb", "\x201e", "\x201c");
    QUOTES_LANG("bem",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("bez",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("bg",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("bm",            "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("bn",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("br",            "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("brx",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("bs-Cyrl",       "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("ca",            "\x201c", "\x201d", "\x00ab", "\x00bb");
    QUOTES_LANG("cgg",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("chr",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("cs",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("da",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("dav",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("de",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("de-CH",         "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("dje",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("dua",           "\x00ab", "\x00bb", "\x2018", "\x2019");
    QUOTES_LANG("dyo",           "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("dz",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ebu",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ee",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("el",            "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("en",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("en-GB",         "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("es",            "\x201c", "\x201d", "\x00ab", "\x00bb");
    QUOTES_LANG("et",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("eu",            "\x201c", "\x201d", "\x00ab", "\x00bb");
    QUOTES_LANG("ewo",           "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("fa",            "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("ff",            "\x201e", "\x201d", "\x201a", "\x2019");
    QUOTES_LANG("fi",            "\x201d", "\x201d", "\x2019", "\x2019");
    QUOTES_LANG("fr",            "\x00ab", "\x00bb", "\x00ab", "\x00bb");
    QUOTES_LANG("fr-CA",         "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("fr-CH",         "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("gsw",           "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("gu",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("guz",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ha",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("he",            "\x0022", "\x0022", "\x0027", "\x0027");
    QUOTES_LANG("hi",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("hr",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("hu",            "\x201e", "\x201d", "\x00bb", "\x00ab");
    QUOTES_LANG("id",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ig",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("it",            "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("ja",            "\x300c", "\x300d", "\x300e", "\x300f");
    QUOTES_LANG("jgo",           "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("jmc",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("kab",           "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("kam",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("kde",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("kea",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("khq",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ki",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("kkj",           "\x00ab", "\x00bb", "\x2039", "\x203a");
    QUOTES_LANG("kln",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("km",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("kn",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ko",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ksb",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ksf",           "\x00ab", "\x00bb", "\x2018", "\x2019");
    QUOTES_LANG("lag",           "\x201d", "\x201d", "\x2019", "\x2019");
    QUOTES_LANG("lg",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ln",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("lo",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("lt",            "\x201e", "\x201c", "\x201e", "\x201c");
    QUOTES_LANG("lu",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("luo",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("luy",           "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("lv",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mas",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mer",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mfe",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mg",            "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("mgo",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mk",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("ml",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mr",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ms",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("mua",           "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("my",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("naq",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("nb",            "\x00ab", "\x00bb", "\x2018", "\x2019");
    QUOTES_LANG("nd",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("nl",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("nmg",           "\x201e", "\x201d", "\x00ab", "\x00bb");
    QUOTES_LANG("nn",            "\x00ab", "\x00bb", "\x2018", "\x2019");
    QUOTES_LANG("nnh",           "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("nus",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("nyn",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("pl",            "\x201e", "\x201d", "\x00ab", "\x00bb");
    QUOTES_LANG("pt",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("pt-PT",         "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("rn",            "\x201d", "\x201d", "\x2019", "\x2019");
    QUOTES_LANG("ro",            "\x201e", "\x201d", "\x00ab", "\x00bb");
    QUOTES_LANG("rof",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ru",            "\x00ab", "\x00bb", "\x201e", "\x201c");
    QUOTES_LANG("rw",            "\x00ab", "\x00bb", "\x2018", "\x2019");
    QUOTES_LANG("rwk",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("saq",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("sbp",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("seh",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ses",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("sg",            "\x00ab", "\x00bb", "\x201c", "\x201d");
    QUOTES_LANG("shi",           "\x00ab", "\x00bb", "\x201e", "\x201d");
    QUOTES_LANG("shi-Tfng",      "\x00ab", "\x00bb", "\x201e", "\x201d");
    QUOTES_LANG("si",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("sk",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("sl",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("sn",            "\x201d", "\x201d", "\x2019", "\x2019");
    QUOTES_LANG("so",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("sq",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("sr",            "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("sr-Latn",       "\x201e", "\x201c", "\x201a", "\x2018");
    QUOTES_LANG("sv",            "\x201d", "\x201d", "\x2019", "\x2019");
    QUOTES_LANG("sw",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("swc",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ta",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("te",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("teo",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("th",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("ti-ER",         "\x2018", "\x2019", "\x201c", "\x201d");
    QUOTES_LANG("to",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("tr",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("twq",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("tzm",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("uk",            "\x00ab", "\x00bb", "\x201e", "\x201c");
    QUOTES_LANG("ur",            "\x201d", "\x201c", "\x2019", "\x2018");
    QUOTES_LANG("vai",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("vai-Latn",      "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("vi",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("vun",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("xh",            "\x2018", "\x2019", "\x201c", "\x201d");
    QUOTES_LANG("xog",           "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("yav",           "\x00ab", "\x00bb", "\x00ab", "\x00bb");
    QUOTES_LANG("yo",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("zh",            "\x201c", "\x201d", "\x2018", "\x2019");
    QUOTES_LANG("zh-Hant",       "\x300c", "\x300d", "\x300e", "\x300f");
    QUOTES_LANG("zu",            "\x201c", "\x201d", "\x2018", "\x2019");
    #undef QUOTES_LANG

    return staticQuotesMap;
}

static const QuotesData* basicQuotesData()
{
    // FIXME: The default quotes should be the fancy quotes for "en".
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
        return quotesData()->getCloseQuote(m_depth - 1).impl();
    case OPEN_QUOTE:
        return quotesData()->getOpenQuote(m_depth).impl();
    }
    ASSERT_NOT_REACHED();
    return StringImpl::empty();
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
    ASSERT(isRooted());

    if (!view()->renderQuoteHead()) {
        view()->setRenderQuoteHead(this);
        m_attached = true;
        return;
    }

    for (RenderObject* predecessor = previousInPreOrder(); predecessor; predecessor = predecessor->previousInPreOrder()) {
        // Skip unattached predecessors to avoid having stale m_previous pointers
        // if the previous node is never attached and is then destroyed.
        if (!predecessor->isQuote() || !toRenderQuote(predecessor)->isAttached())
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
        if (m_next)
            m_next->m_previous = this;
    }
    m_attached = true;

    for (RenderQuote* quote = this; quote; quote = quote->m_next)
        quote->updateDepth();

    ASSERT(!m_next || m_next->m_attached);
    ASSERT(!m_next || m_next->m_previous == this);
    ASSERT(!m_previous || m_previous->m_attached);
    ASSERT(!m_previous || m_previous->m_next == this);
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
    if (oldDepth != m_depth)
        setText(originalText());
}

} // namespace WebCore
