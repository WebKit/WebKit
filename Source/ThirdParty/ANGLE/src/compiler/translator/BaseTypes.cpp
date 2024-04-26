//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "BaseTypes.h"

#include "common/PackedEnums.h"

namespace sh
{
namespace
{
constexpr gl::BlendEquationBitSet kAdvancedBlendBits{
    gl::BlendEquationType::Multiply,      gl::BlendEquationType::Screen,
    gl::BlendEquationType::Overlay,       gl::BlendEquationType::Darken,
    gl::BlendEquationType::Lighten,       gl::BlendEquationType::Colordodge,
    gl::BlendEquationType::Colorburn,     gl::BlendEquationType::Hardlight,
    gl::BlendEquationType::Softlight,     gl::BlendEquationType::Difference,
    gl::BlendEquationType::Exclusion,     gl::BlendEquationType::HslHue,
    gl::BlendEquationType::HslSaturation, gl::BlendEquationType::HslColor,
    gl::BlendEquationType::HslLuminosity,
};

constexpr gl::BlendEquationBitSet kAdvancedBlendHslBits{
    gl::BlendEquationType::HslHue,
    gl::BlendEquationType::HslSaturation,
    gl::BlendEquationType::HslColor,
    gl::BlendEquationType::HslLuminosity,
};

bool IsValidAdvancedBlendBitSet(uint32_t enabledEquations)
{
    return (gl::BlendEquationBitSet(enabledEquations) & ~kAdvancedBlendBits).none();
}
}  // anonymous namespace

bool AdvancedBlendEquations::any() const
{
    ASSERT(IsValidAdvancedBlendBitSet(mEnabledBlendEquations));
    return mEnabledBlendEquations != 0;
}

bool AdvancedBlendEquations::anyHsl() const
{
    ASSERT(IsValidAdvancedBlendBitSet(mEnabledBlendEquations));
    return (gl::BlendEquationBitSet(mEnabledBlendEquations) & kAdvancedBlendHslBits).any();
}

void AdvancedBlendEquations::setAll()
{
    ASSERT(IsValidAdvancedBlendBitSet(mEnabledBlendEquations));
    mEnabledBlendEquations = kAdvancedBlendBits.bits();
}

void AdvancedBlendEquations::set(uint32_t blendEquation)
{
    gl::BlendEquationType eq = static_cast<gl::BlendEquationType>(blendEquation);
    mEnabledBlendEquations   = gl::BlendEquationBitSet(mEnabledBlendEquations).set(eq).bits();
    ASSERT(IsValidAdvancedBlendBitSet(mEnabledBlendEquations));
}

}  // namespace sh
