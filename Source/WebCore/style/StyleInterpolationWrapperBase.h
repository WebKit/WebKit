/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class RenderStyle;

enum class CompositeOperation : uint8_t;
enum CSSPropertyID : uint16_t;

namespace Style::Interpolation {

struct Context;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleInterpolationWrapperBase);
class WrapperBase {
    WTF_MAKE_NONCOPYABLE(WrapperBase);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(WrapperBase, StyleInterpolationWrapperBase);
public:
    explicit WrapperBase(CSSPropertyID property)
        : m_property(property)
    {
    }
    virtual ~WrapperBase() = default;

    CSSPropertyID property() const { return m_property; }

    virtual bool equals(const RenderStyle&, const RenderStyle&) const = 0;
    virtual bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const { return true; }
    virtual bool requiresInterpolationForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const { return false; }
    virtual void interpolate(RenderStyle&, const RenderStyle&, const RenderStyle&, const Context&) const = 0;
#if !LOG_DISABLED
    virtual void log(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const = 0;
#endif

private:
    CSSPropertyID m_property;
};

} // namespace Style::Interpolation
} // namespace WebCore
