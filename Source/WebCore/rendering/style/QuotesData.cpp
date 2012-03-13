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
#include "QuotesData.h"

namespace WebCore {

QuotesData* QuotesData::create(int stringCount)
{
    char* tmp = new char[sizeof(QuotesData)+sizeof(String)*stringCount];
    if (!tmp)
        return 0;
    QuotesData* ret = new (tmp) QuotesData(stringCount);
    for (int i = 0; i < stringCount; ++i)
        new (tmp +sizeof(QuotesData) + sizeof(String)*i) String();
    return ret;
}

bool QuotesData::equal(const QuotesData* quotesData1, const QuotesData* quotesData2)
{
    if (quotesData1 == quotesData2)
        return true;
    if (!quotesData1 || !quotesData2)
        return false;
    if (quotesData1->length != quotesData2->length)
        return false;
    const String* data1 = quotesData1->data();
    const String* data2 = quotesData2->data();
    for (int i = quotesData1->length - 1; i >= 0; --i)
        if (data1[i] != data2[i])
            return false;
    return true;
}

QuotesData::~QuotesData()
{
    String* p = data();
    for (int i = 0; i < length; ++i)
        p[i].~String();
}

} // namespace WebCore
