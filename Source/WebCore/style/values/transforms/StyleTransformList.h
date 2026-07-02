/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleTransformFunction.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

class FloatSize;
class LayoutSize;
class TransformationMatrix;

namespace Style {

struct Transform;

// <transform-list> = <transform-function>+
// https://drafts.csswg.org/css-transforms-1/#typedef-transform-list
struct TransformList {
    using Container = SpaceSeparatedFixedVector<TransformFunction>;
    using iterator = typename Container::iterator;
    using reverse_iterator = typename Container::reverse_iterator;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    TransformList() = default;
    TransformList(Container&& value)
        : m_value { WTF::move(value) }
    {
    }

    TransformList(TransformFunction&& transformFunction)
        : m_value { WTF::move(transformFunction) }
    {
    }

    iterator begin() LIFETIME_BOUND { return m_value.begin(); }
    iterator end() LIFETIME_BOUND { return m_value.end(); }
    reverse_iterator rbegin() LIFETIME_BOUND { return m_value.rbegin(); }
    reverse_iterator rend() LIFETIME_BOUND { return m_value.rend(); }

    const_iterator begin() const LIFETIME_BOUND { return m_value.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_value.end(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_value.rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_value.rend(); }

    bool isEmpty() const { return m_value.isEmpty(); }
    size_t size() const { return m_value.size(); }
    const TransformFunction& operator[](size_t i) const LIFETIME_BOUND { return m_value[i]; }

    bool operator==(const TransformList&) const = default;

    template<TransformFunctionType>
    bool hasTransformOfType() const;

    void apply(TransformationMatrix&, const FloatSize&, unsigned start = 0) const;

    // Return true if any of the operation types are 3D operation types (even if the
    // values describe affine transforms)
    bool has3DOperation() const;
    bool isRepresentableIn2D() const;
    bool affectedByTransformOrigin() const;
    bool containsNonInvertibleMatrix(const LayoutSize&) const;

    TransformFunctionSizeDependencies computeSizeDependencies() const;

private:
    friend struct Blending<TransformList>;
    friend struct Transform;

    bool isInvertible(const LayoutSize&) const;

    Container m_value;
};

template<TransformFunctionType operationType>
bool TransformList::hasTransformOfType() const
{
    return std::ranges::any_of(m_value, [](auto& op) { return op->type() == operationType; });
}

template<> struct Blending<TransformList> {
    auto canBlend(const TransformList&, const TransformList&, CompositeOperation) -> bool;
    auto blend(const TransformList&, const TransformList&, const Interpolation::Context&) -> TransformList;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_RANGE_LIKE_CONFORMANCE(WebCore::Style::TransformList)
