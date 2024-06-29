/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_FactoryFunctionsPriv_DEFINED
#define skgpu_graphite_FactoryFunctionsPriv_DEFINED

#include "src/base/SkEnumBitMask.h"
#include "src/gpu/graphite/FactoryFunctions.h"

namespace skgpu::graphite {

class PrecompileShader;

enum class PrecompileImageShaderFlags {
    kNone         = 0b00,
    kExcludeAlpha = 0b01,
    kExcludeCubic = 0b10
};
SK_MAKE_BITMASK_OPS(PrecompileImageShaderFlags)

//--------------------------------------------------------------------------------------------------
namespace PrecompileShadersPriv {
    sk_sp<PrecompileShader> Blur(sk_sp<PrecompileShader> child);

    // TODO: This, technically, doesn't need to take an SkSpan since it is only called from
    // PrecompileShader::makeWithCTM. Leaving it be for now in case the usage is revised.
    sk_sp<PrecompileShader> CTM(SkSpan<const sk_sp<PrecompileShader>> wrapped);

    sk_sp<PrecompileShader> Image(SkEnumBitMask<PrecompileImageShaderFlags>);

    // These factory variants should be used when the existence or non-existence of the local matrix
    // is known. If 'withLM' is true only the LMShader-wrapped shader will be created while, when
    // 'withLM' is false, no LMShader will wrap the base shader.
    sk_sp<PrecompileShader> LinearGradient(bool withLM);
    sk_sp<PrecompileShader> RadialGradient(bool withLM);
    sk_sp<PrecompileShader> TwoPointConicalGradient(bool withLM);
    sk_sp<PrecompileShader> SweepGradient(bool withLM);
    sk_sp<PrecompileShader> Picture(bool withLM);

    // TODO: this factory function should go away (c.f. b/336810091)
    sk_sp<PrecompileShader> LocalMatrixBothVariants(SkSpan<const sk_sp<PrecompileShader>> wrapped);

} // namespace PrecompileShadersPriv

namespace PrecompileColorFiltersPriv {
    // These three match those in src/core/SkColorFilterPriv
    sk_sp<PrecompileColorFilter> Gaussian();

    sk_sp<PrecompileColorFilter> ColorSpaceXform();

    sk_sp<PrecompileColorFilter> WithWorkingFormat(
            SkSpan<const sk_sp<PrecompileColorFilter>> childOptions);

} // namespace PrecompileColorFiltersPriv

} // namespace skgpu::graphite

#endif // skgpu_graphite_FactoryFunctionsPriv_DEFINED
