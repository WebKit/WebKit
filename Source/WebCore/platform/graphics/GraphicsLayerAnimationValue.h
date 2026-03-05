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

#include <WebCore/TimingFunction.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

// Base class for animation values (also used for transitions). Here to
// represent values for properties being animated via the GraphicsLayer,
// without pulling in style-related data from outside of the platform directory.
class GraphicsLayerAnimationValue {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(GraphicsLayerAnimationValue, WEBCORE_EXPORT);
public:
    virtual ~GraphicsLayerAnimationValue() = default;

    double keyTime() const { return m_keyTime; }
    const TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    virtual std::unique_ptr<GraphicsLayerAnimationValue> clone() const = 0;

    virtual bool isGraphicsLayerFilterAnimationValue() const { return false; }
    virtual bool isGraphicsLayerFloatAnimationValue() const { return false; }
    virtual bool isGraphicsLayerTransformAnimationValue() const { return false; }

protected:
    GraphicsLayerAnimationValue(double keyTime, TimingFunction* timingFunction = nullptr)
        : m_keyTime(keyTime)
        , m_timingFunction(timingFunction)
    {
    }

    explicit GraphicsLayerAnimationValue(const GraphicsLayerAnimationValue& other)
        : m_keyTime(other.m_keyTime)
        , m_timingFunction(other.m_timingFunction ? RefPtr { other.m_timingFunction->clone() } : nullptr)
    {
    }

    GraphicsLayerAnimationValue(GraphicsLayerAnimationValue&&) = default;

private:
    void operator=(const GraphicsLayerAnimationValue&) = delete;

    double m_keyTime;
    const RefPtr<TimingFunction> m_timingFunction;
};

} // namespace WebCore
