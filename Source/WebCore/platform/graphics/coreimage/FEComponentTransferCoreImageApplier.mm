/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "FEComponentTransferCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEComponentTransfer.h"
#import "Filter.h"
#import "FilterImage.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEComponentTransferCoreImageApplier);

FEComponentTransferCoreImageApplier::FEComponentTransferCoreImageApplier(const FEComponentTransfer& effect)
    : Base(effect)
{
}

template<ComponentTransferType... Types>
static bool isNullOr(const ComponentTransferFunction& function)
{
    if (function.type == ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN)
        return true;
    return ((function.type == Types) || ...);
}

template<ComponentTransferType... Types>
static bool isType(const ComponentTransferFunction& function)
{
    return ((function.type == Types) || ...);
}

template<typename Predicate>
static bool allChannelsMatch(const FEComponentTransfer& effect, Predicate predicate)
{
    return predicate(effect.redFunction())
        && predicate(effect.greenFunction())
        && predicate(effect.blueFunction())
        && predicate(effect.alphaFunction());
}

static CIKernel* compontentTransferKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
extern "C" {
namespace coreimage {

enum class TransferType : uint32_t {
    Identity,
    Table,
    Discrete,
    Linear,
    Gamma,
};

enum ParamIndex {
    LinearSlope = 0,
    LinearIntercept = 1,
    GammaAmplitude = 0,
    GammaExponent = 1,
    GammaOffset = 2,
};

struct TransferFunction {
    TransferType functionType;
    unsigned tableLength; // For table and discrete, number of entries in table.
    unsigned tableStart; // For table and discrete, start of table in the table buffer.

    float params[3];
};

struct ComponentTransferConstants {
    TransferFunction data[4];
};

float applyTransferFunction(float component, constant TransferFunction& function, constant float* tables)
{
    switch (function.functionType) {
    case TransferType::Identity:
        return component;

    case TransferType::Table: {
        constant float* tableStart = tables + function.tableStart;
        int tableLength = function.tableLength;
        int n = tableLength - 1;

        int k = min((int)(component * n), n);
        float v1 = tableStart[k];
        float v2 = tableStart[min(k + 1, n)];
        return v1 + ((component * n) - k) * (v2 - v1);
    }
    case TransferType::Discrete: {
        constant float* tableStart = tables + function.tableStart;
        int n = function.tableLength;
        int k = min((int)(component * n), n - 1);
        return tableStart[k];
    }
    case TransferType::Linear:
        return component * function.params[ParamIndex::LinearSlope] + function.params[ParamIndex::LinearIntercept];

    case TransferType::Gamma:
        return function.params[ParamIndex::GammaAmplitude] * pow(component, function.params[ParamIndex::GammaExponent]) + function.params[ParamIndex::GammaOffset];
    }

    return component;
}

[[stitchable]] float4 componentTransfer(sampler src,
    constant ComponentTransferConstants* constants,
    constant float* tables,
    destination dest)
{
    float2 samplePosition = src.transform(dest.coord());
    float4 srcPixel = unpremultiply(src.sample(samplePosition));

    float4 resultPixel = { };

    resultPixel.r = applyTransferFunction(srcPixel.r, constants->data[0], tables);
    resultPixel.g = applyTransferFunction(srcPixel.g, constants->data[1], tables);
    resultPixel.b = applyTransferFunction(srcPixel.b, constants->data[2], tables);
    resultPixel.a = applyTransferFunction(srcPixel.a, constants->data[3], tables);

    return premultiply(clamp(resultPixel, 0, 1));
}

} // namespace coreimage {
} // extern "C"

        )" error:&error]; /* NOLINT */

        if (error || !kernels || !kernels.count) {
            LOG(Filters, "ComponentTransfer kernel compilation failed: %@", error);
            return;
        }

        kernel.get() = kernels[0];
    });

    return kernel.get().get();
}

RetainPtr<CIImage> FEComponentTransferCoreImageApplier::applyLinear(RetainPtr<CIImage>&& inputImage) const
{
    auto componentTransferFilter = [CIFilter filterWithName:@"CIColorPolynomial"];
    [componentTransferFilter setValue:inputImage.get() forKey:kCIInputImageKey];

    auto setCoefficients = [&] (NSString *key, const ComponentTransferFunction& function) {
        if (function.type == ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR)
            [componentTransferFilter setValue:[CIVector vectorWithX:function.intercept Y:function.slope Z:0 W:0] forKey:key];
    };

    setCoefficients(@"inputRedCoefficients", m_effect->redFunction());
    setCoefficients(@"inputGreenCoefficients", m_effect->greenFunction());
    setCoefficients(@"inputBlueCoefficients", m_effect->blueFunction());
    setCoefficients(@"inputAlphaCoefficients", m_effect->alphaFunction());

    return componentTransferFilter.outputImage;
}

RetainPtr<CIImage> FEComponentTransferCoreImageApplier::applyOther(RetainPtr<CIImage>&& inputImage) const
{
    RetainPtr kernel = compontentTransferKernel();
    if (!kernel)
        return nil;

    enum class TransferType : uint32_t {
        Identity,
        Table,
        Discrete,
        Linear,
        Gamma,
    };

    auto transferType = [](ComponentTransferType type) {
        switch (type) {
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN: return TransferType::Identity;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY: return TransferType::Identity;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_TABLE: return TransferType::Table;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_DISCRETE: return TransferType::Discrete;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR: return TransferType::Linear;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA: return TransferType::Gamma;
        }
    };

    struct TransferFunction {
        TransferType functionType;
        unsigned tableLength; // For table and discrete, number of entries in table.
        unsigned tableStart; // For table and discrete, start of table in the table buffer.

        std::array<float, 3> params;
    };

    struct ComponentTransferConstants {
        TransferFunction data[4];
    };

    auto transferFunction = [&](const ComponentTransferFunction& function, Vector<float>& tableData) -> TransferFunction {
        auto result = TransferFunction {
            transferType(function.type),
            0,
            0,
            { 0, 0, 0 }
        };

        switch (function.type) {
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY:
            break;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR:
            result.params[0] = function.slope;
            result.params[1] = function.intercept;
            break;
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA:
            result.params[0] = function.amplitude;
            result.params[1] = function.exponent;
            result.params[2] = function.offset;
            break;

        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_TABLE:
        case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_DISCRETE:
            result.tableLength = function.tableValues.size();
            result.tableStart = tableData.size();
            tableData.appendVector(function.tableValues);
            break;
        }

        return result;
    };

    Vector<float> tableData;
    auto constants = ComponentTransferConstants {
        {
            transferFunction(m_effect->redFunction(), tableData),
            transferFunction(m_effect->greenFunction(), tableData),
            transferFunction(m_effect->blueFunction(), tableData),
            transferFunction(m_effect->alphaFunction(), tableData),
        }
    };

    RetainPtr<NSArray> arguments = @[
        inputImage.get(),
        [NSData dataWithBytes:&constants length:sizeof(ComponentTransferConstants)],
        [NSData dataWithBytes:tableData.span().data() length:tableData.sizeInBytes()],
    ];

    RetainPtr<CIImage> outputImage = [kernel applyWithExtent:inputImage.get().extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];

    return outputImage;
}

bool FEComponentTransferCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    RetainPtr<CIImage> resultImage;
    if (allChannelsMatch(m_effect, isType<ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR>))
        resultImage = applyLinear(WTF::move(inputImage));
    else
        resultImage = applyOther(WTF::move(inputImage));

    if (!resultImage)
        return false;

    auto extent = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    resultImage = [resultImage imageByCroppingToRect:extent];

    result.setCIImage(WTF::move(resultImage));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
