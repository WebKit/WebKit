/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/GraphicsLayerAnimationValue.h>
#include <WebCore/GraphicsLayerClient.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

// Used to store a series of values in a keyframe list.
// Values will all be of the same type, which can be inferred from the property.
class GraphicsLayerKeyframeValueList {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(GraphicsLayerKeyframeValueList, WEBCORE_EXPORT);
public:
    explicit GraphicsLayerKeyframeValueList(AnimatedProperty property)
        : m_property(property)
    {
    }

    explicit GraphicsLayerKeyframeValueList(const GraphicsLayerKeyframeValueList& other)
        : m_property(other.property())
    {
        m_values = WTF::map(other.m_values, [](auto& value) -> std::unique_ptr<const GraphicsLayerAnimationValue> {
            return value->clone();
        });
    }

    GraphicsLayerKeyframeValueList(GraphicsLayerKeyframeValueList&&) = default;

    GraphicsLayerKeyframeValueList& operator=(const GraphicsLayerKeyframeValueList& other)
    {
        GraphicsLayerKeyframeValueList copy(other);
        swap(copy);
        return *this;
    }

    GraphicsLayerKeyframeValueList& operator=(GraphicsLayerKeyframeValueList&&) = default;

    void swap(GraphicsLayerKeyframeValueList& other)
    {
        std::swap(m_property, other.m_property);
        m_values.swap(other.m_values);
    }

    AnimatedProperty property() const { return m_property; }

    size_t size() const { return m_values.size(); }
    const GraphicsLayerAnimationValue& at(size_t i) const { return *m_values.at(i); }

    // Insert, sorted by keyTime.
    WEBCORE_EXPORT void insert(std::unique_ptr<const GraphicsLayerAnimationValue>);

protected:
    Vector<std::unique_ptr<const GraphicsLayerAnimationValue>> m_values;
    AnimatedProperty m_property;
};

} // namespace WebCore
