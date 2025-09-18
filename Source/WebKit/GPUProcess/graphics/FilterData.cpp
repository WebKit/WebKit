/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(GPU_PROCESS)
#include "FilterData.h"

#include "RemoteRenderingBackend.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteResourceCache.h"
#include "RemoteResourceCacheProxy.h"
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
#include <WebCore/FEOffset.h>
#include <WebCore/FESpecularLighting.h>
#include <WebCore/FETile.h>
#include <WebCore/SourceAlpha.h>
#include <WebCore/SourceGraphic.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

namespace {
struct ToDataContext {
    RemoteRenderingBackendProxy& renderingBackend;
    const DestinationColorSpace& fallbackColorSpace;
};
struct FromDataContext {
    RemoteResourceCache& resourceCache;
    RemoteRenderingBackend& renderingBackend;
};

}

static LightSourceData toData(LightSource& value)
{
    return LightSourceData {

    };
}

static std::optional<Vector<FilterEffectData>> serializeFilterEffectsToFilterEffectDatas(std::span<const Ref<FilterEffect>> effects, const ToDataContext&);

static std::optional<SVGFilterData> toData(const SVGFilter& svgFilter, const ToDataContext& context)
{
    auto effects = serializeFilterEffectsToFilterEffectDatas(svgFilter.effects(), context);
    if (!effects)
        return std::nullopt;
    return SVGFilterData {
        .filterRenderingModes = svgFilter.filterRenderingModes(),
        .filterScale = svgFilter.filterScale(),
        .filterRegion = svgFilter.filterRegion(),
        .targetBoundingBox = svgFilter.targetBoundingBox(),
        .primitiveUnits = svgFilter.primitiveUnits(),
        .expression = svgFilter.expression(),
        .effects = WTFMove(*effects),
        .renderingResourceIdentifier = svgFilter.renderingResourceIdentifier()
    };
}

template<typename FilterFunctionDataVariant>
static std::optional<FilterFunctionDataVariant> toData(const FilterFunction& function, const ToDataContext& context)
{
    switch (function.filterType()) {
    case FilterFunction::Type::FEBlend: {
        RefPtr f = dynamicDowncast<FEBlend>(function);
        if (!f)
            return std::nullopt;
        return FEBlendData {
            .blendMode = f->blendMode(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEColorMatrix: {
        RefPtr f = dynamicDowncast<FEColorMatrix>(function);
        if (!f)
            return std::nullopt;
        return FEColorMatrixData {
            .type = f->type(),
            .values = f->values(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEComponentTransfer: {
        RefPtr f = dynamicDowncast<FEComponentTransfer>(function);
        if (!f)
            return std::nullopt;
        return FEComponentTransferData {
            .redFunction = f->redFunction(),
            .greenFunction = f->greenFunction(),
            .blueFunction = f->blueFunction(),
            .alphaFunction = f->alphaFunction(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEComposite: {
        RefPtr f = dynamicDowncast<FEComposite>(function);
        if (!f)
            return std::nullopt;
        return FECompositeData {
            .operation = f->operation(),
            .k1 = f->k1(),
            .k2 = f->k2(),
            .k3 = f->k3(),
            .k4 = f->k4(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEConvolveMatrix: {
        RefPtr f = dynamicDowncast<FEConvolveMatrix>(function);
        if (!f)
            return std::nullopt;
        return FEConvolveMatrixData {
            .kernelSize = f->kernelSize(),
            .divisor = f->divisor(),
            .bias = f->bias(),
            .targetOffset = f->targetOffset(),
            .edgeMode = f->edgeMode(),
            .kernelUnitLength = f->kernelUnitLength(),
            .preserveAlpha = f->preserveAlpha(),
            .kernel = f->kernel(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEDiffuseLighting: {
        RefPtr f = dynamicDowncast<FEDiffuseLighting>(function);
        if (!f)
            return std::nullopt;
        return FEDiffuseLightingData {
            .lightingColor = f->lightingColor(),
            .surfaceScale = f->surfaceScale(),
            .diffuseConstant = f->diffuseConstant(),
            .kernelUnitLengthX = f->kernelUnitLengthX(),
            .kernelUnitLengthY = f->kernelUnitLengthY(),
            .lightSource = toData(f->lightSource()),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEDisplacementMap: {
        RefPtr f = dynamicDowncast<FEDisplacementMap>(function);
        if (!f)
            return std::nullopt;
        return FEDisplacementMapData {
            .xChannelSelector = f->xChannelSelector(),
            .yChannelSelector = f->yChannelSelector(),
            .scale = f->scale(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEDropShadow: {
        RefPtr f = dynamicDowncast<FEDropShadow>(function);
        if (!f)
            return std::nullopt;
        return FEDropShadowData {
            .stdDeviationX = f->stdDeviationX(),
            .stdDeviationY = f->stdDeviationY(),
            .dx = f->dx(),
            .dy = f->dy(),
            .shadowColor = f->shadowColor(),
            .shadowOpacity = f->shadowOpacity(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEFlood: {
        RefPtr f = dynamicDowncast<FEFlood>(function);
        if (!f)
            return std::nullopt;
        return FEFloodData {
            .floodColor = f->floodColor(),
            .floodOpacity = f->floodOpacity(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEGaussianBlur: {
        RefPtr f = dynamicDowncast<FEGaussianBlur>(function);
        if (!f)
            return std::nullopt;
        return FEGaussianBlurData {
            .stdDeviationX = f->stdDeviationX(),
            .stdDeviationY = f->stdDeviationY(),
            .edgeMode = f->edgeMode(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEImage: {
        RefPtr f = dynamicDowncast<FEImage>(function);
        if (!f)
            return std::nullopt;
        const auto& sourceImage = f->sourceImage();
        std::optional<RenderingResourceIdentifier> sourceImageIdentifier;
        if (RefPtr nativeImage = sourceImage.nativeImageIfExists()) {
            context.renderingBackend.remoteResourceCacheProxy().recordNativeImageUse(*nativeImage, context.fallbackColorSpace);
            sourceImageIdentifier = nativeImage->renderingResourceIdentifier();
        } else if (RefPtr imageBuffer = sourceImage.imageBufferIfExists()) {
            if (context.renderingBackend.isCached(*imageBuffer))
                sourceImageIdentifier = imageBuffer->renderingResourceIdentifier();
        }
        if (!sourceImageIdentifier)
            return std::nullopt;
        return FEImageData {
            .sourceImageIdentifier = *sourceImageIdentifier,
            .sourceImageRect = f->sourceImageRect()
        };
    }
    case FilterFunction::Type::FEMerge: {
        RefPtr f = dynamicDowncast<FEMerge>(function);
        if (!f)
            return std::nullopt;
        return FEMergeData {
            .numberOfEffectInputs = f->numberOfEffectInputs(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEMorphology: {
        RefPtr f = dynamicDowncast<FEMorphology>(function);
        if (!f)
            return std::nullopt;
        return FEMorphologyData {
            .morphologyOperator = f->morphologyOperator(),
            .radiusX = f->radiusX(),
            .radiusY = f->radiusY(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FEOffset: {
        RefPtr f = dynamicDowncast<FEOffset>(function);
        if (!f)
            return std::nullopt;
        return FEOffsetData {
            .dx = f->dx(),
            .dy = f->dy(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FETile: {
        RefPtr f = dynamicDowncast<FETile>(function);
        if (!f)
            return std::nullopt;
        return FETileData {
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FESpecularLighting: {
        RefPtr f = dynamicDowncast<FESpecularLighting>(function);
        if (!f)
            return std::nullopt;
        return FESpecularLightingData {
            .lightingColor = f->lightingColor(),
            .surfaceScale = f->surfaceScale(),
            .specularConstant = f->specularConstant(),
            .specularExponent = f->specularExponent(),
            .kernelUnitLengthX = f->kernelUnitLengthX(),
            .kernelUnitLengthY = f->kernelUnitLengthY(),
            .lightSource = toData(f->lightSource()),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::FETurbulence: {
        RefPtr f = dynamicDowncast<FETurbulence>(function);
        if (!f)
            return std::nullopt;
        return FETurbulenceData {
            .type = f->type(),
            .baseFrequencyX = f->baseFrequencyX(),
            .baseFrequencyY = f->baseFrequencyY(),
            .numOctaves = f->numOctaves(),
            .seed = f->seed(),
            .stitchTiles = f->stitchTiles(),
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::SourceAlpha: {
        RefPtr f = dynamicDowncast<SourceAlpha>(function);
        if (!f)
            return std::nullopt;
        return SourceAlphaData {
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    case FilterFunction::Type::SourceGraphic: {
        RefPtr f = dynamicDowncast<SourceGraphic>(function);
        if (!f)
            return std::nullopt;
        return SourceGraphicData {
            .operatingColorSpace = f->operatingColorSpace()
        };
    }
    default:
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

std::optional<Vector<FilterEffectData>> serializeFilterEffectsToFilterEffectDatas(std::span<const Ref<FilterEffect>> effects, const ToDataContext& context)
{
    Vector<FilterEffectData> result;
    result.reserveCapacity(effects.size());
    for (auto& effect : effects) {
        auto effectData = toData<FilterEffectData>(effect, context);
        if (!effectData)
            return std::nullopt;
        result.append(WTFMove(*effectData));
    }
    return result;
}

static std::optional<Vector<FilterFunctionData>> serializeFilterFunctionsToFilterFunctionDatas(std::span<const Ref<FilterFunction>> functions, const ToDataContext& context)
{
    Vector<FilterFunctionData> result;
    result.reserveCapacity(functions.size());
    for (auto& function : functions) {
        std::optional<FilterFunctionData> functionData;
        if (RefPtr svgFilter = dynamicDowncast<SVGFilter>(function))
            functionData = toData(*svgFilter, context);
        else
            functionData = toData<FilterFunctionData>(function, context);
        if (!functionData)
            return std::nullopt;
        result.append(WTFMove(*functionData));
    }
    return result;
}

std::optional<FilterData> serializeFilterToFilterData(Filter& filter, RemoteRenderingBackendProxy& renderingBackend, const DestinationColorSpace& fallbackColorSpace)
{
    ToDataContext context { renderingBackend, fallbackColorSpace };
    if (RefPtr svgFilter = dynamicDowncast<SVGFilter>(filter))
        return toData(*svgFilter, context);
    if (RefPtr cssFilter = dynamicDowncast<CSSFilter>(filter)) {
        auto functions = serializeFilterFunctionsToFilterFunctionDatas(cssFilter->functions(), context);
        if (!functions)
            return std::nullopt;
        return CSSFilterData {
            .filterRenderingModes = cssFilter->filterRenderingModes(),
            .filterScale = cssFilter->filterScale(),
            .filterRegion = cssFilter->filterRegion(),
            .functions = WTFMove(*functions)
        };
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

#if 0
static Vector<Ref<FilterEffect>> fromData(std::span<const FilterEffectData> effectDatas, const FromDataContext& context)
{
    Vector<Ref<FilterEffect>> result;
    return result;
}

static Vector<Ref<FilterFunction>> fromData(std::span<const FilterFunctionData> functionDatas, const FromDataContext& context)
{
    Vector<Ref<FilterFunction>> result;
    return result;
}
#endif

RefPtr<Filter> createFilterFromFilterData(FilterData&& filterData, RemoteRenderingBackend& renderingBackend)
{
#if 0
    FromDataContext context { resourceCache, renderingBackend };
    return WTF::switchOn(filterData,
        [&](CSSFilterData&& data) -> RefPtr<Filter> {
            return CSSFilter::create(fromData(data.functions, context), data.filterRenderingModes, data.filterScale, data.filterRegion);
        },
        [&](SVGFilterData&& data) -> RefPtr<Filter> {
            return SVGFilter::create(data.targetBoundingBox, data.primitiveUnits, WTFMove(data.expression), fromData(data.effects, context), data.renderingResourceIdentifier, data.filterRenderingModes, data.filterScale, data.filterRegion);
        });
#else
    return nullptr;
#endif
}

bool isValidSVGFilterExpression(const WebCore::SVGFilterExpression&, const Vector<FilterEffectData>&)
{
    // FIXME: implement.
    return false;
}

}

#endif
