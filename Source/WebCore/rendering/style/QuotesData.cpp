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
#include "QuotesData.h"
#include <wtf/StdLibExtras.h>
#include <wtf/ZippedRange.h>

namespace WebCore {

static size_t sizeForQuotesDataWithQuoteCount(unsigned count)
{
    return sizeof(QuotesData) + sizeof(std::pair<String, String>) * count;
}

Ref<QuotesData> QuotesData::create(const Vector<std::pair<String, String>>& quotes)
{
    void* slot = fastMalloc(sizeForQuotesDataWithQuoteCount(quotes.size()));
    return adoptRef(*new (NotNull, slot) QuotesData(quotes));
}

QuotesData::QuotesData(const Vector<std::pair<String, String>>& quotes)
    : m_quoteCount(quotes.size())
{
    for (auto [quotePair, quote] : zippedRange(quotePairs(), quotes))
        new (NotNull, &quotePair) std::pair<String, String>(quote);
}

QuotesData::~QuotesData()
{
    for (auto& quotePair : quotePairs())
        quotePair.~pair<String, String>();
}

std::span<const std::pair<String, String>> QuotesData::quotePairs() const
{
    return unsafeMakeSpan(m_quotePairs, m_quoteCount);
}

std::span<std::pair<String, String>> QuotesData::quotePairs()
{
    return unsafeMakeSpan(m_quotePairs, m_quoteCount);
}

const String& QuotesData::openQuote(unsigned index) const
{
    auto quotePairs = this->quotePairs();
    if (quotePairs.empty())
        return emptyString();

    if (index < quotePairs.size())
        return quotePairs[index].first;
    return quotePairs.back().first;
}

const String& QuotesData::closeQuote(unsigned index) const
{
    auto quotePairs = this->quotePairs();
    if (quotePairs.empty())
        return emptyString();

    if (index < quotePairs.size())
        return quotePairs[index].second;
    return quotePairs.back().second;
}

bool operator==(const QuotesData& a, const QuotesData& b)
{
    if (a.m_quoteCount != b.m_quoteCount)
        return false;

    for (auto [aPair, bPair] : zippedRange(a.quotePairs(), b.quotePairs())) {
        if (aPair != bPair)
            return false;
    }

    return true;
}

} // namespace WebCore
