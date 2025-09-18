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

#pragma once

#if ENABLE(GPU_PROCESS)

#include <WebCore/Color.h>
#include <WebCore/FEColorMatrix.h>
#include <WebCore/FEComponentTransfer.h>
#include <WebCore/FEComposite.h>
#include <WebCore/FEConvolveMatrix.h>
#include <WebCore/FEDisplacementMap.h>
#include <WebCore/FEMorphology.h>
#include <WebCore/FETurbulence.h>
#include <WebCore/LightSource.h>
#include <WebCore/SVGFilter.h>
#include <WebCore/SVGPreserveAspectRatioValue.h>

namespace WebCore {

class Filter;

}

namespace WebKit {

class RemoteResourceCache;
class RemoteResourceCacheProxy;
class RemoteRenderingBackend;
class RemoteRenderingBackendProxy;

struct DistantLightSourceData {
    float azimuth;
    float elevation;
};

struct PointLightSourceData {
    WebCore::FloatPoint3D position;
};

struct SpotLightSourceData {
    WebCore::FloatPoint3D position;
    WebCore::FloatPoint3D direction;
    float specularExponent;
    float limitingConeAngle;
};

using LightSourceData = Variant<DistantLightSourceData, PointLightSourceData, SpotLightSourceData>;

struct FEBlendData {
    WebCore::BlendMode blendMode;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEColorMatrixData {
    WebCore::ColorMatrixType type;
    Vector<float> values;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct ComponentTransferFunctionData {
    WebCore::ComponentTransferType type;
    float slope;
    float intercept;
    float amplitude;
    float exponent;
    float offset;
    Vector<float> tableValues;
};

struct FEComponentTransferData {
    WebCore::ComponentTransferFunction redFunction;
    WebCore::ComponentTransferFunction greenFunction;
    WebCore::ComponentTransferFunction blueFunction;
    WebCore::ComponentTransferFunction alphaFunction;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FECompositeData {
    WebCore::CompositeOperationType operation;
    float k1;
    float k2;
    float k3;
    float k4;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEConvolveMatrixData {
    WebCore::IntSize kernelSize;
    float divisor;
    float bias;
    WebCore::IntPoint targetOffset;
    WebCore::EdgeModeType edgeMode;
    WebCore::FloatPoint kernelUnitLength;
    bool preserveAlpha;
    Vector<float> kernel;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEDiffuseLightingData {
    WebCore::Color lightingColor;
    float surfaceScale;
    float diffuseConstant;
    float kernelUnitLengthX;
    float kernelUnitLengthY;
    LightSourceData lightSource;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEDisplacementMapData {
    WebCore::ChannelSelectorType xChannelSelector;
    WebCore::ChannelSelectorType yChannelSelector;
    float scale;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEDropShadowData {
    float stdDeviationX;
    float stdDeviationY;
    float dx;
    float dy;
    WebCore::Color shadowColor;
    float shadowOpacity;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEFloodData {
    WebCore::Color floodColor;
    float floodOpacity;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEGaussianBlurData {
    float stdDeviationX;
    float stdDeviationY;
    WebCore::EdgeModeType edgeMode;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEImageData {
    WebCore::RenderingResourceIdentifier sourceImageIdentifier;
    WebCore::FloatRect sourceImageRect;
    WebCore::SVGPreserveAspectRatioValue preserveAspectRatio;
};

struct FEMergeData {
    unsigned numberOfEffectInputs;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEMorphologyData {
    WebCore::MorphologyOperatorType morphologyOperator;
    float radiusX;
    float radiusY;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FEOffsetData {
    float dx;
    float dy;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FETileData {
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FESpecularLightingData {
    WebCore::Color lightingColor;
    float surfaceScale;
    float specularConstant;
    float specularExponent;
    float kernelUnitLengthX;
    float kernelUnitLengthY;
    LightSourceData lightSource;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct FETurbulenceData {
    WebCore::TurbulenceType type;
    float baseFrequencyX;
    float baseFrequencyY;
    int numOctaves;
    float seed;
    bool stitchTiles;
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct SourceAlphaData {
    WebCore::DestinationColorSpace operatingColorSpace;
};

struct SourceGraphicData {
    WebCore::DestinationColorSpace operatingColorSpace;
};

using FilterEffectData = Variant<FEBlendData, FEColorMatrixData, FEComponentTransferData, FECompositeData, FEConvolveMatrixData, FEDiffuseLightingData, FEDisplacementMapData, FEDropShadowData, FEFloodData, FEGaussianBlurData, FEImageData, FEMergeData, FEMorphologyData, FEOffsetData, FETileData, FESpecularLightingData, FETurbulenceData, SourceAlphaData, SourceGraphicData>;

struct SVGFilterData {
    OptionSet<WebCore::FilterRenderingMode> filterRenderingModes;
    WebCore::FloatSize filterScale;
    WebCore::FloatRect filterRegion;
    WebCore::FloatRect targetBoundingBox;
    WebCore::SVGUnitTypes::SVGUnitType primitiveUnits;
    WebCore::SVGFilterExpression expression;
    Vector<FilterEffectData> effects;
    std::optional<WebCore::RenderingResourceIdentifier> renderingResourceIdentifier;
};

using FilterFunctionData = Variant<SVGFilterData, FEBlendData, FEColorMatrixData, FEComponentTransferData, FECompositeData, FEConvolveMatrixData, FEDiffuseLightingData, FEDisplacementMapData, FEDropShadowData, FEFloodData, FEGaussianBlurData, FEImageData, FEMergeData, FEMorphologyData, FEOffsetData, FESpecularLightingData, FETileData, FETurbulenceData, SourceAlphaData, SourceGraphicData>;

struct CSSFilterData {
    OptionSet<WebCore::FilterRenderingMode> filterRenderingModes;
    WebCore::FloatSize filterScale;
    WebCore::FloatRect filterRegion;
    Vector<FilterFunctionData> functions;
};

using FilterData = Variant<CSSFilterData, SVGFilterData>;

std::optional<FilterData> serializeFilterToFilterData(WebCore::Filter&, RemoteRenderingBackendProxy&, const WebCore::DestinationColorSpace& fallbackColorSpace);

RefPtr<WebCore::Filter> createFilterFromFilterData(FilterData&&, RemoteRenderingBackend&);

bool isValidSVGFilterExpression(const WebCore::SVGFilterExpression&, const Vector<FilterEffectData>&);

}

#endif
