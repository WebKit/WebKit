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
 */

#pragma once

#include <WebCore/LayoutSize.h>
#include <WebCore/TransformOperation.h>
#include <algorithm>
#include <wtf/ArgumentCoder.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

struct BlendingContext;

class TransformOperations {
    WTF_MAKE_TZONE_ALLOCATED(TransformOperations);
public:
    using const_iterator = Vector<Ref<TransformOperation>>::const_iterator;
    using const_reverse_iterator = Vector<Ref<TransformOperation>>::const_reverse_iterator;
    using value_type = Vector<Ref<TransformOperation>>::value_type;

    TransformOperations() = default;

    explicit TransformOperations(Ref<TransformOperation>&&);
    WEBCORE_EXPORT explicit TransformOperations(Vector<Ref<TransformOperation>>&&);

    bool operator==(const TransformOperations&) const;

    WEBCORE_EXPORT TransformOperations clone() const;

    const_iterator begin() const LIFETIME_BOUND { return m_operations.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_operations.end(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_operations.rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_operations.rend(); }

    bool isEmpty() const { return m_operations.isEmpty(); }
    size_t size() const { return m_operations.size(); }
    const TransformOperation* at(size_t index) const LIFETIME_BOUND { return index < m_operations.size() ? m_operations[index].ptr() : nullptr; }

    const Ref<TransformOperation>& operator[](size_t i) const LIFETIME_BOUND { return m_operations[i]; }
    const Ref<TransformOperation>& first() const LIFETIME_BOUND { return m_operations.first(); }
    const Ref<TransformOperation>& last() const LIFETIME_BOUND { return m_operations.last(); }

    void apply(TransformationMatrix&, unsigned start = 0) const;

    template<TransformOperation::Type operationType>
    bool hasTransformOfType() const;

    bool isInvertible() const;
    bool containsNonInvertibleMatrix() const;

private:
    friend struct IPC::ArgumentCoder<TransformOperations>;
    friend WTF::TextStream& operator<<(WTF::TextStream&, const TransformOperations&);

    Vector<Ref<TransformOperation>> m_operations;
};

template<TransformOperation::Type operationType>
bool TransformOperations::hasTransformOfType() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return op->type() == operationType; });
}

TransformOperations blend(const TransformOperations& from, const TransformOperations& to, const BlendingContext&);

WTF::TextStream& operator<<(WTF::TextStream&, const TransformOperations&);

} // namespace WebCore
