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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

// Used to store one filter value in a keyframe list.
class GraphicsLayerFilterAnimationValue : public GraphicsLayerAnimationValue {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GraphicsLayerFilterAnimationValue);
public:
    GraphicsLayerFilterAnimationValue(double keyTime, const FilterOperations& value, TimingFunction* timingFunction = nullptr)
        : GraphicsLayerAnimationValue(keyTime, timingFunction)
        , m_value(value)
    {
    }

    std::unique_ptr<GraphicsLayerAnimationValue> clone() const override
    {
        return makeUnique<GraphicsLayerFilterAnimationValue>(*this);
    }

    explicit GraphicsLayerFilterAnimationValue(const GraphicsLayerFilterAnimationValue& other)
        : GraphicsLayerAnimationValue(other)
        , m_value(other.m_value.clone())
    {
    }

    GraphicsLayerFilterAnimationValue(GraphicsLayerFilterAnimationValue&&) = default;

    const FilterOperations& value() const { return m_value; }

private:
    bool isGraphicsLayerFilterAnimationValue() const final { return true; }

    FilterOperations m_value;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::GraphicsLayerFilterAnimationValue)
    static bool isType(const WebCore::GraphicsLayerAnimationValue& value) { return value.isGraphicsLayerFilterAnimationValue(); }
SPECIALIZE_TYPE_TRAITS_END()
