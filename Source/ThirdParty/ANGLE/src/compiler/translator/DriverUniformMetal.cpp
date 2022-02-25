//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DriverUniformMetal:
//   Struct defining the default driver uniforms for direct and SpirV based ANGLE translation
//

#include "compiler/translator/DriverUniformMetal.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/IntermNode_util.h"

namespace sh
{

namespace
{

// Metal specific driver uniforms
constexpr const char kHalfRenderArea[] = "halfRenderArea";
constexpr const char kFlipXY[]         = "flipXY";
constexpr const char kNegFlipXY[]      = "negFlipXY";
constexpr const char kCoverageMask[]   = "coverageMask";
constexpr const char kUnusedMetal[]    = "unusedMetal";

}  // namespace

// class DriverUniformMetal
// The fields here must match the DriverUniforms structure defined in ContextMtl.h.
TFieldList *DriverUniformMetal::createUniformFields(TSymbolTable *symbolTable)
{
    TFieldList *driverFieldList = DriverUniform::createUniformFields(symbolTable);

    constexpr size_t kNumGraphicsDriverUniformsMetal = 5;
    constexpr std::array<const char *, kNumGraphicsDriverUniformsMetal>
        kGraphicsDriverUniformNamesMetal = {
            {kHalfRenderArea, kFlipXY, kNegFlipXY, kCoverageMask, kUnusedMetal}};

    const std::array<TType *, kNumGraphicsDriverUniformsMetal> kDriverUniformTypesMetal = {{
        new TType(EbtFloat, EbpHigh, EvqGlobal, 2),  // halfRenderArea
        new TType(EbtFloat, EbpLow, EvqGlobal, 2),   // flipXY
        new TType(EbtFloat, EbpLow, EvqGlobal, 2),   // negFlipXY
        new TType(EbtUInt, EbpHigh, EvqGlobal),      // kCoverageMask
        new TType(EbtUInt, EbpHigh, EvqGlobal),      // kUnusedMetal
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumGraphicsDriverUniformsMetal; ++uniformIndex)
    {
        TField *driverUniformField =
            new TField(kDriverUniformTypesMetal[uniformIndex],
                       ImmutableString(kGraphicsDriverUniformNamesMetal[uniformIndex]),
                       TSourceLoc(), SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    return driverFieldList;
}

TIntermTyped *DriverUniformMetal::getHalfRenderAreaRef() const
{
    return createDriverUniformRef(kHalfRenderArea);
}

TIntermTyped *DriverUniformMetal::getFlipXYRef() const
{
    return createDriverUniformRef(kFlipXY);
}

TIntermTyped *DriverUniformMetal::getNegFlipXYRef() const
{
    return createDriverUniformRef(kNegFlipXY);
}

TIntermTyped *DriverUniformMetal::getNegFlipYRef() const
{
    // Create a swizzle to "negFlipXY.y"
    TIntermTyped *negFlipXY     = createDriverUniformRef(kNegFlipXY);
    TVector<int> swizzleOffsetY = {1};
    TIntermSwizzle *negFlipY    = new TIntermSwizzle(negFlipXY, swizzleOffsetY);
    return negFlipY;
}

TIntermTyped *DriverUniformMetal::getCoverageMaskFieldRef() const
{
    return createDriverUniformRef(kCoverageMask);
}

}  // namespace sh
