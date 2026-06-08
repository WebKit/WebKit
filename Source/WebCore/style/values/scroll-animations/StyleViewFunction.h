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

#include <WebCore/ScrollAxis.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/StyleViewTimelineInsetItem.h>

namespace WebCore {

class CSSViewValue;

namespace Style {

// <view()> = view( [ <axis> || <'view-timeline-inset'> ]? )
// https://www.w3.org/TR/scroll-animations-1/#funcdef-view
struct ViewFunctionParameters {
    ScrollAxis axis;
    ViewTimelineInsetItem insets;

    bool operator==(const ViewFunctionParameters&) const = default;
};
using ViewFunction = FunctionNotation<CSSValueView, ViewFunctionParameters>;

// MARK: - Conversion

template<> struct CSSValueConversion<ViewFunction> {
    auto operator()(BuilderState&, const CSSValue&) -> ViewFunction;
    auto operator()(BuilderState&, const CSSViewValue&) -> ViewFunction;
};

template<> struct CSSValueCreation<ViewFunction> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const ViewFunction&); };

// MARK: - Serialization

template<> struct Serialize<ViewFunctionParameters> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const ViewFunctionParameters&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const ViewFunctionParameters&);

} // namespace Style
} // namespace WebCore
