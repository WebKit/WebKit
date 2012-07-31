/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
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

#ifndef QuotesData_h
#define QuotesData_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class QuotesData : public RefCounted<QuotesData> {
public:
    static PassRefPtr<QuotesData> create() { return adoptRef(new QuotesData()); }
    static PassRefPtr<QuotesData> create(const String open, const String close);
    static PassRefPtr<QuotesData> create(const String open1, const String close1, const String open2, const String close2);

    // FIXME: this should be an operator==.
    static bool equals(const QuotesData*, const QuotesData*);

    void addPair(const std::pair<String, String> quotePair);
    const String getOpenQuote(int index) const;
    const String getCloseQuote(int index) const;

private:
    QuotesData() { }

    Vector<std::pair<String, String> > m_quotePairs;
};

} // namespace WebCore

#endif // QuotesData_h
