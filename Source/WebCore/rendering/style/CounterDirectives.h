/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

#include <memory>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

struct CounterDirectives {
    std::optional<int> resetValue;
    std::optional<int> incrementValue;

    void addIncrementValue(int additionalIncrement) { incrementValue = addClamped(incrementValue.value_or(0), additionalIncrement); }
    static int addClamped(int a, int b) { return clampToInteger(static_cast<double>(a) + b);  }
};

bool operator==(const CounterDirectives&, const CounterDirectives&);
inline bool operator!=(const CounterDirectives& a, const CounterDirectives& b) { return !(a == b); }

using CounterDirectiveMap = HashMap<AtomicString, CounterDirectives>;

inline bool operator==(const CounterDirectives& a, const CounterDirectives& b)
{
    return a.incrementValue == b.incrementValue && a.resetValue == b.resetValue;
}

} // namespace WebCore
