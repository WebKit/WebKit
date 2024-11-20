/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include "TransformFunctions.h"
#include <optional>
#include <wtf/Vector.h>

namespace WebCore {

struct TransformList {
    using List = Vector<TransformFunction>;

    using const_iterator = List::const_iterator;
    using const_reverse_iterator = List::const_reverse_iterator;
    using value_type = List::value_type;

    const_iterator begin() const { return list.begin(); }
    const_iterator end() const { return list.end(); }

    bool isEmpty() const { return list.isEmpty(); }
    size_t size() const { return list.size(); }

    std::optional<TransformFunction> at(size_t index) const { return index < list.size() ? std::make_optional(list[index]) : std::nullopt; }
    const TransformFunction& operator[](size_t i) const { return list[i]; }

    template<typename... Ts> bool hasTransformOfType() const;
    bool isRepresentableIn2D() const;

    void apply(TransformationMatrix&, unsigned start = 0) const;

    bool operator==(const TransformList&) const = default;

    List list;
};

template<typename... Ts> bool TransformList::hasTransformOfType() const
{
    for (const auto& operation : *this) {
        if ((std::holds_alternative<Ts>(operation) || ...))
            return true;
    }
    return false;
}

// MARK: - Blending

TransformList blend(const TransformList&, const TransformList&, const BlendingContext&, std::optional<unsigned> prefixLength = std::nullopt);

} // namespace WebCore
