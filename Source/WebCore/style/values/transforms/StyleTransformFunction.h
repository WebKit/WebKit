/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleTransformFunctionBase.h>
#include <WebCore/StyleTransformFunctionWrapper.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/TransformOperation.h>

namespace WebCore {
namespace Style {

// Any <transform-function>.
// https://www.w3.org/TR/css-transforms-1/#typedef-transform-function
struct TransformFunction : TransformFunctionWrapper<TransformFunctionBase> {
    using TransformFunctionWrapper<TransformFunctionBase>::TransformFunctionWrapper;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TransformFunction> { auto operator()(BuilderState&, const CSSValue&) -> TransformFunction; };
template<> struct CSSValueCreation<TransformFunction> { auto operator()(CSSValuePool&, const RenderStyle&, const TransformFunction&) -> Ref<CSSValue>; };
template<> struct CSSValueCreation<TransformationMatrix> { auto operator()(CSSValuePool&, const RenderStyle&, const TransformationMatrix&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<TransformFunction> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const TransformFunction&); };
template<> struct Serialize<TransformationMatrix> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const TransformationMatrix&); };

// MARK: - Blending

template<> struct Blending<TransformFunction> {
    auto blend(const TransformFunction&, const TransformFunction&, const Interpolation::Context&) -> TransformFunction;
};

// MARK: - Platform

template<> struct ToPlatform<TransformFunction> { auto operator()(const TransformFunction&, const FloatSize&) -> Ref<TransformOperation>; };

// MARK: - Logging

TextStream& operator<<(TextStream&, const TransformFunction&);

} // namespace Style
} // namespace WebCore
