/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include "StyleCounterStyle.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CounterContent {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CounterContent);
public:
    CounterContent(const AtomString& identifier, const AtomString& separator, Style::CounterStyle&& style)
        : m_identifier(identifier)
        , m_separator(separator)
        , m_style(WTFMove(style))
    {
    }

    const AtomString& identifier() const { return m_identifier; }
    const AtomString& separator() const { return m_separator; }
    const Style::CounterStyle& style() const { return m_style; }

    bool operator==(const CounterContent&) const = default;

private:
    AtomString m_identifier;
    AtomString m_separator;
    Style::CounterStyle m_style;
};

} // namespace WebCore
