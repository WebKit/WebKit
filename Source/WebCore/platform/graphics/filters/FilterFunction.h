/*
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

#include "FilterEffectGeometry.h"
#include "FilterImage.h"
#include "FilterImageVector.h"
#include "FilterRenderingMode.h"
#include "FilterStyle.h"
#include "FloatRect.h"
#include "LengthBox.h"
#include <wtf/RefCounted.h>
#include <wtf/text/AtomString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Filter;
class FilterResults;

enum class FilterRepresentation : uint8_t {
    TestOutput,
    Debugging
};

class FilterFunction : public RefCounted<FilterFunction> {
public:
    enum class Type : uint8_t {
        CSSFilter,
        SVGFilter,

        FEFirst,

        FEBlend = FEFirst,
        FEColorMatrix,
        FEComponentTransfer,
        FEComposite,
        FEConvolveMatrix,
        FEDiffuseLighting,
        FEDisplacementMap,
        FEDropShadow,
        FEFlood,
        FEGaussianBlur,
        FEImage,
        FEMerge,
        FEMorphology,
        FEOffset,
        FESpecularLighting,
        FETile,
        FETurbulence,
        SourceAlpha,
        SourceGraphic,

        FELast = SourceGraphic
    };

    FilterFunction(Type);
    virtual ~FilterFunction() = default;

    Type filterType() const { return m_filterType; }

    bool isCSSFilter() const { return m_filterType == Type::CSSFilter; }
    bool isSVGFilter() const { return m_filterType == Type::SVGFilter; }
    bool isFilter() const { return m_filterType == Type::CSSFilter || m_filterType == Type::SVGFilter; }
    bool isFilterEffect() const { return m_filterType >= Type::FEFirst && m_filterType <= Type::FELast; }

    static AtomString filterName(Type);
    static AtomString sourceAlphaName() { return filterName(Type::SourceAlpha); }
    static AtomString sourceGraphicName() { return filterName(Type::SourceGraphic); }
    AtomString filterName() const { return filterName(m_filterType); }

    virtual OptionSet<FilterRenderingMode> supportedFilterRenderingModes() const { return FilterRenderingMode::Software; }
    virtual RefPtr<FilterImage> apply(const Filter&, FilterImage&, FilterResults&) { return nullptr; }
    virtual FilterStyleVector createFilterStyles(const Filter&, const FilterStyle&) const { return { }; }

    virtual WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation = FilterRepresentation::TestOutput) const = 0;

private:
    Type m_filterType;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const FilterFunction&);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::FilterFunction::Type> {
    using values = EnumValues<
        WebCore::FilterFunction::Type,

        WebCore::FilterFunction::Type::CSSFilter,
        WebCore::FilterFunction::Type::SVGFilter,

        WebCore::FilterFunction::Type::FEBlend,
        WebCore::FilterFunction::Type::FEColorMatrix,
        WebCore::FilterFunction::Type::FEComponentTransfer,
        WebCore::FilterFunction::Type::FEComposite,
        WebCore::FilterFunction::Type::FEConvolveMatrix,
        WebCore::FilterFunction::Type::FEDiffuseLighting,
        WebCore::FilterFunction::Type::FEDisplacementMap,
        WebCore::FilterFunction::Type::FEDropShadow,
        WebCore::FilterFunction::Type::FEFlood,
        WebCore::FilterFunction::Type::FEGaussianBlur,
        WebCore::FilterFunction::Type::FEImage,
        WebCore::FilterFunction::Type::FEMerge,
        WebCore::FilterFunction::Type::FEMorphology,
        WebCore::FilterFunction::Type::FEOffset,
        WebCore::FilterFunction::Type::FESpecularLighting,
        WebCore::FilterFunction::Type::FETile,
        WebCore::FilterFunction::Type::FETurbulence,
        WebCore::FilterFunction::Type::SourceAlpha,
        WebCore::FilterFunction::Type::SourceGraphic
    >;
};

} // namespace WTF
