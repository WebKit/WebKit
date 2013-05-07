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

namespace WebCore {

PassRefPtr<QuotesData> QuotesData::create(const String& open1, const String& close1, const String& open2, const String& close2)
{
    Vector<std::pair<String, String> > quotes;
    quotes.reserveInitialCapacity(2);
    quotes.uncheckedAppend(std::make_pair(open1, close1));
    quotes.uncheckedAppend(std::make_pair(open2, close2));

    return QuotesData::create(quotes);
}

PassRefPtr<QuotesData> QuotesData::create(const Vector<std::pair<String, String> >& quotes)
{
    RefPtr<QuotesData> quotesData = adoptRef(new QuotesData);
    quotesData->m_quotePairs = quotes;

    return quotesData.release();
}

const String& QuotesData::openQuote(unsigned index) const
{
    if (!m_quotePairs.isEmpty())
        return emptyString();

    if (index >= m_quotePairs.size())
        return m_quotePairs.last().first;

    return m_quotePairs[index].first;
}

const String& QuotesData::closeQuote(unsigned index) const
{
    if (m_quotePairs.isEmpty())
        return emptyString();

    if (index >= m_quotePairs.size())
        return m_quotePairs.last().second;

    return m_quotePairs.at(index).second;
}

bool operator==(const QuotesData& a, const QuotesData& b)
{
    return a.m_quotePairs == b.m_quotePairs;
}

} // namespace WebCore
