/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSFilter.h>
#include <WebCore/FEBlend.h>
#include <WebCore/FEColorMatrix.h>
#include <WebCore/FEComponentTransfer.h>
#include <WebCore/FEComposite.h>
#include <WebCore/FEConvolveMatrix.h>
#include <WebCore/FEDiffuseLighting.h>
#include <WebCore/FEDisplacementMap.h>
#include <WebCore/FEDropShadow.h>
#include <WebCore/FEFlood.h>
#include <WebCore/FEGaussianBlur.h>
#include <WebCore/FEImage.h>
#include <WebCore/FEMerge.h>
#include <WebCore/FEMorphology.h>
#include <WebCore/FEOffset.h>
#include <WebCore/FESpecularLighting.h>
#include <WebCore/FETile.h>
#include <WebCore/FETurbulence.h>
#include <WebCore/Filter.h>
#include <WebCore/FilterEffectVector.h>
#include <WebCore/SVGFilter.h>
#include <WebCore/SourceAlpha.h>
#include <WebCore/SourceGraphic.h>

namespace IPC {

class FilterReference {
public:
    FilterReference(Ref<WebCore::Filter>&& filter)
        : m_filter(WTFMove(filter))
    {
    }

    Ref<WebCore::Filter> takeFilter() { return WTFMove(m_filter); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FilterReference> decode(Decoder&);

private:
    struct ExpressionReferenceTerm {
        unsigned index;
        std::optional<WebCore::FilterEffectGeometry> geometry;
        unsigned level;
    
        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<ExpressionReferenceTerm> decode(Decoder&);
    };
    
    using ExpressionReference = Vector<ExpressionReferenceTerm>;

    template<class Encoder> static void encodeFilterEffect(const WebCore::FilterEffect&, Encoder&);
    template<class Decoder> static RefPtr<WebCore::FilterEffect> decodeFilterEffect(Decoder&, WebCore::FilterFunction::Type);
    template<class Decoder> static RefPtr<WebCore::FilterEffect> decodeFilterEffect(Decoder&);

    template<class Encoder> static void encodeSVGFilter(const WebCore::SVGFilter&, Encoder&);
    template<class Decoder> static RefPtr<WebCore::SVGFilter> decodeSVGFilter(Decoder&);

    template<class Encoder> static void encodeCSSFilter(const WebCore::CSSFilter&, Encoder&);
    template<class Decoder> static RefPtr<WebCore::CSSFilter> decodeCSSFilter(Decoder&);

    template<class Encoder> static void encodeFilter(const WebCore::Filter&, Encoder&);
    template<class Decoder> static RefPtr<WebCore::Filter> decodeFilter(Decoder&, WebCore::FilterFunction::Type);
    template<class Decoder> static RefPtr<WebCore::Filter> decodeFilter(Decoder&);

    Ref<WebCore::Filter> m_filter;
};

template<class Encoder>
void FilterReference::encodeFilterEffect(const WebCore::FilterEffect& effect, Encoder& encoder)
{
    encoder << effect.filterType();
    encoder << effect.operatingColorSpace();

    switch (effect.filterType()) {
    case WebCore::FilterEffect::Type::FEBlend:
        downcast<WebCore::FEBlend>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEColorMatrix:
        downcast<WebCore::FEColorMatrix>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEComponentTransfer:
        downcast<WebCore::FEComponentTransfer>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEComposite:
        downcast<WebCore::FEComposite>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEConvolveMatrix:
        downcast<WebCore::FEConvolveMatrix>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEDiffuseLighting:
        downcast<WebCore::FEDiffuseLighting>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEDisplacementMap:
        downcast<WebCore::FEDisplacementMap>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEDropShadow:
        downcast<WebCore::FEDropShadow>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEFlood:
        downcast<WebCore::FEFlood>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEGaussianBlur:
        downcast<WebCore::FEGaussianBlur>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEImage:
        downcast<WebCore::FEImage>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEMerge:
        downcast<WebCore::FEMerge>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEMorphology:
        downcast<WebCore::FEMorphology>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FEOffset:
        downcast<WebCore::FEOffset>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FESpecularLighting:
        downcast<WebCore::FESpecularLighting>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::FETile:
        break;

    case WebCore::FilterEffect::Type::FETurbulence:
        downcast<WebCore::FETurbulence>(effect).encode(encoder);
        break;

    case WebCore::FilterEffect::Type::SourceAlpha:
    case WebCore::FilterEffect::Type::SourceGraphic:
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

template<class Decoder>
RefPtr<WebCore::FilterEffect> FilterReference::decodeFilterEffect(Decoder& decoder, WebCore::FilterFunction::Type filterType)
{
    if (!(filterType >= WebCore::FilterFunction::Type::FEFirst && filterType <= WebCore::FilterFunction::Type::FELast))
        return nullptr;

    std::optional<WebCore::DestinationColorSpace> operatingColorSpace;
    decoder >> operatingColorSpace;
    if (!operatingColorSpace)
        return nullptr;

    std::optional<Ref<WebCore::FilterEffect>> effect;

    switch (filterType) {
    case WebCore::FilterEffect::Type::FEBlend:
        effect = WebCore::FEBlend::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEColorMatrix:
        effect = WebCore::FEColorMatrix::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEComponentTransfer:
        effect = WebCore::FEComponentTransfer::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEComposite:
        effect = WebCore::FEComposite::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEConvolveMatrix:
        effect = WebCore::FEConvolveMatrix::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEDiffuseLighting:
        effect = WebCore::FEDiffuseLighting::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEDisplacementMap:
        effect = WebCore::FEDisplacementMap::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEDropShadow:
        effect = WebCore::FEDropShadow::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEFlood:
        effect = WebCore::FEFlood::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEGaussianBlur:
        effect = WebCore::FEGaussianBlur::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEImage:
        effect = WebCore::FEImage::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEMerge:
        effect = WebCore::FEMerge::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEMorphology:
        effect = WebCore::FEMorphology::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FEOffset:
        effect = WebCore::FEOffset::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FESpecularLighting:
        effect = WebCore::FESpecularLighting::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::FETile:
        effect = WebCore::FETile::create();
        break;

    case WebCore::FilterEffect::Type::FETurbulence:
        effect = WebCore::FETurbulence::decode(decoder);
        break;

    case WebCore::FilterEffect::Type::SourceAlpha:
        effect = WebCore::SourceAlpha::create();
        break;

    case WebCore::FilterEffect::Type::SourceGraphic:
        effect = WebCore::SourceGraphic::create();
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    if (effect) {
        (*effect)->setOperatingColorSpace(*operatingColorSpace);
        return { (*effect).ptr() };
    }

    return nullptr;
}

template<class Decoder>
RefPtr<WebCore::FilterEffect> FilterReference::decodeFilterEffect(Decoder& decoder)
{
    std::optional<WebCore::FilterFunction::Type> filterType;
    decoder >> filterType;
    if (!filterType)
        return nullptr;

    return decodeFilterEffect(decoder, *filterType);
}

template<class Encoder>
void FilterReference::ExpressionReferenceTerm::encode(Encoder& encoder) const
{
    encoder << index;
    encoder << geometry;
    encoder << level;
}

template<class Decoder>
std::optional<FilterReference::ExpressionReferenceTerm> FilterReference::ExpressionReferenceTerm::decode(Decoder& decoder)
{
    std::optional<unsigned> index;
    decoder >> index;
    if (!index)
        return std::nullopt;

    std::optional<std::optional<WebCore::FilterEffectGeometry>> geometry;
    decoder >> geometry;
    if (!geometry)
        return std::nullopt;

    std::optional<unsigned> level;
    decoder >> level;
    if (!level)
        return std::nullopt;

    return { { *index, *geometry, *level } };
}

template<class Encoder>
void FilterReference::encodeSVGFilter(const WebCore::SVGFilter& filter, Encoder& encoder)
{
    HashMap<Ref<WebCore::FilterEffect>, unsigned> indicies;
    Vector<Ref<WebCore::FilterEffect>> effects;

    // Get the individual FilterEffects in filter.expression().
    for (auto& term : filter.expression()) {
        if (indicies.contains(term.effect))
            continue;
        indicies.add(term.effect, effects.size());
        effects.append(term.effect);
    }

    // Replace the Ref<FilterEffect> in SVGExpressionTerm with its index in indicies.
    auto expressionReference = WTF::map(filter.expression(), [&indicies] (auto&& term) -> ExpressionReferenceTerm {
        ASSERT(indicies.contains(term.effect));
        unsigned index = indicies.get(term.effect);
        return { index, term.geometry, term.level };
    });

    encoder << filter.targetBoundingBox();
    encoder << filter.primitiveUnits();
    
    encoder << effects.size();
    for (auto& effect : effects)
        encodeFilterEffect(effect, encoder);

    encoder << expressionReference;
}

template<class Decoder>
RefPtr<WebCore::SVGFilter> FilterReference::decodeSVGFilter(Decoder& decoder)
{
    std::optional<WebCore::FloatRect> targetBoundingBox;
    decoder >> targetBoundingBox;
    if (!targetBoundingBox)
        return nullptr;

    std::optional<WebCore::SVGUnitTypes::SVGUnitType> primitiveUnits;
    decoder >> primitiveUnits;
    if (!primitiveUnits)
        return nullptr;

    std::optional<size_t> effectsSize;
    decoder >> effectsSize;
    if (!effectsSize || !*effectsSize)
        return nullptr;

    Vector<Ref<WebCore::FilterEffect>> effects;

    for (size_t i = 0; i < *effectsSize; ++i) {
        auto effect = decodeFilterEffect(decoder);
        if (!effect)
            return nullptr;

        effects.append(effect.releaseNonNull());
    }

    std::optional<ExpressionReference> expressionReference;
    decoder >> expressionReference;
    if (!expressionReference || expressionReference->isEmpty())
        return nullptr;

    WebCore::SVGFilterExpression expression;
    expression.reserveInitialCapacity(expressionReference->size());

    // Replace the index in ExpressionReferenceTerm with its Ref<FilterEffect> in effects.
    for (auto& term : *expressionReference) {
        if (term.index >= effects.size())
            return nullptr;
        expression.uncheckedAppend({ effects[term.index], term.geometry, term.level });
    }
    
    return WebCore::SVGFilter::create(*targetBoundingBox, *primitiveUnits, WTFMove(expression));
}

template<class Encoder>
void FilterReference::encodeCSSFilter(const WebCore::CSSFilter& filter, Encoder& encoder)
{
    encoder << filter.functions().size();

    for (auto& function : filter.functions()) {
        if (is<WebCore::SVGFilter>(function)) {
            encodeFilter(downcast<WebCore::SVGFilter>(function.get()), encoder);
            continue;
        }

        if (is<WebCore::FilterEffect>(function))
            encodeFilterEffect(downcast<WebCore::FilterEffect>(function.get()), encoder);
    }
}

template<class Decoder>
RefPtr<WebCore::CSSFilter> FilterReference::decodeCSSFilter(Decoder& decoder)
{
    std::optional<size_t> size;
    decoder >> size;
    if (!size || !*size)
        return nullptr;

    Vector<Ref<WebCore::FilterFunction>> functions;

    for (size_t i = 0; i < size; ++i) {
        std::optional<WebCore::FilterFunction::Type> filterType;
        decoder >> filterType;
        if (!filterType)
            return nullptr;

        if (*filterType == WebCore::FilterFunction::Type::SVGFilter) {
            if (auto filter = decodeFilter(decoder, *filterType)) {
                functions.append(filter.releaseNonNull());
                continue;
            }
            return nullptr;
        }

        auto effect = decodeFilterEffect(decoder, *filterType);
        if (!effect)
            return nullptr;

        functions.append(effect.releaseNonNull());
    }

    return WebCore::CSSFilter::create(WTFMove(functions));
}

template<class Encoder>
void FilterReference::encodeFilter(const WebCore::Filter& filter, Encoder& encoder)
{
    encoder << filter.filterType();
    encoder << filter.filterRenderingModes();
    encoder << filter.filterScale();
    encoder << filter.clipOperation();
    encoder << filter.filterRegion();

    if (is<WebCore::CSSFilter>(filter))
        encodeCSSFilter(downcast<WebCore::CSSFilter>(filter), encoder);
    else if (is<WebCore::SVGFilter>(filter))
        encodeSVGFilter(downcast<WebCore::SVGFilter>(filter), encoder);
}

template<class Decoder>
RefPtr<WebCore::Filter> FilterReference::decodeFilter(Decoder& decoder, WebCore::FilterFunction::Type filterType)
{
    std::optional<OptionSet<WebCore::FilterRenderingMode>> filterRenderingModes;
    decoder >> filterRenderingModes;
    if (!filterRenderingModes)
        return nullptr;

    std::optional<WebCore::FloatSize> filterScale;
    decoder >> filterScale;
    if (!filterScale)
        return nullptr;

    std::optional<WebCore::Filter::ClipOperation> clipOperation;
    decoder >> clipOperation;
    if (!clipOperation)
        return nullptr;

    std::optional<WebCore::FloatRect> filterRegion;
    decoder >> filterRegion;
    if (!filterRegion)
        return nullptr;

    RefPtr<WebCore::Filter> filter;

    if (filterType == WebCore::FilterFunction::Type::CSSFilter)
        filter = decodeCSSFilter(decoder);
    else if (filterType == WebCore::FilterFunction::Type::SVGFilter)
        filter = decodeSVGFilter(decoder);
    
    if (filter) {
        filter->setFilterRenderingModes(*filterRenderingModes);
        filter->setFilterScale(*filterScale);
        filter->setClipOperation(*clipOperation);
        filter->setFilterRegion(*filterRegion);
    }

    return filter;
}

template<class Decoder>
RefPtr<WebCore::Filter> FilterReference::decodeFilter(Decoder& decoder)
{
    std::optional<WebCore::FilterFunction::Type> filterType;
    decoder >> filterType;
    if (!filterType)
        return nullptr;

    return decodeFilter(decoder, *filterType);
}

template<class Encoder>
void FilterReference::encode(Encoder& encoder) const
{
    encodeFilter(m_filter, encoder);
}

template<class Decoder>
std::optional<FilterReference> FilterReference::decode(Decoder& decoder)
{
    if (auto filter = decodeFilter(decoder))
        return { filter.releaseNonNull() };
    return std::nullopt;
}

} // namespace IPC
