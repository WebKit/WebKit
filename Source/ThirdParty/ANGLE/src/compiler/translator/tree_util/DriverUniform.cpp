//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DriverUniform.cpp: Add code to support driver uniforms
//

#include "compiler/translator/tree_util/DriverUniform.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{
constexpr ImmutableString kEmulatedDepthRangeParams = ImmutableString("ANGLEDepthRangeParams");

constexpr const char kViewport[]               = "viewport";
constexpr const char kClipDistancesEnabled[]   = "clipDistancesEnabled";
constexpr const char kUnused[]                 = "_unused";
constexpr const char kUnused2[]                = "_unused2";
constexpr const char kXfbVerticesPerInstance[] = "xfbVerticesPerInstance";
constexpr const char kXfbBufferOffsets[]       = "xfbBufferOffsets";
constexpr const char kAcbBufferOffsets[]       = "acbBufferOffsets";
constexpr const char kDepthRange[]             = "depthRange";
constexpr const char kNumSamples[]             = "numSamples";
constexpr const char kHalfRenderArea[]         = "halfRenderArea";
constexpr const char kFlipXY[]                 = "flipXY";
constexpr const char kNegFlipXY[]              = "negFlipXY";
constexpr const char kPreRotation[]            = "preRotation";
constexpr const char kFragRotation[]           = "fragRotation";
constexpr const char kDither[]                 = "dither";

}  // anonymous namespace

// Class DriverUniform
bool DriverUniform::addComputeDriverUniformsToShader(TIntermBlock *root, TSymbolTable *symbolTable)
{
    constexpr size_t kNumComputeDriverUniforms                                               = 1;
    constexpr std::array<const char *, kNumComputeDriverUniforms> kComputeDriverUniformNames = {
        {kAcbBufferOffsets}};

    ASSERT(!mDriverUniforms);
    // This field list mirrors the structure of ComputeDriverUniforms in ContextVk.cpp.
    TFieldList *driverFieldList = new TFieldList;

    const std::array<TType *, kNumComputeDriverUniforms> kDriverUniformTypes = {{
        new TType(EbtUInt, EbpHigh, EvqGlobal, 4),
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumComputeDriverUniforms; ++uniformIndex)
    {
        TField *driverUniformField =
            new TField(kDriverUniformTypes[uniformIndex],
                       ImmutableString(kComputeDriverUniformNames[uniformIndex]), TSourceLoc(),
                       SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    // Define a driver uniform block "ANGLEUniformBlock" with instance name "ANGLEUniforms".
    TLayoutQualifier layoutQualifier = TLayoutQualifier::Create();
    layoutQualifier.blockStorage     = EbsStd140;

    mDriverUniforms = DeclareInterfaceBlock(root, symbolTable, driverFieldList, EvqUniform,
                                            layoutQualifier, TMemoryQualifier::Create(), 0,
                                            ImmutableString(vk::kDriverUniformsBlockName),
                                            ImmutableString(vk::kDriverUniformsVarName));
    return mDriverUniforms != nullptr;
}

TFieldList *DriverUniform::createUniformFields(TSymbolTable *symbolTable)
{
    constexpr size_t kNumGraphicsDriverUniforms                                                = 8;
    constexpr std::array<const char *, kNumGraphicsDriverUniforms> kGraphicsDriverUniformNames = {
        {kViewport, kClipDistancesEnabled, kUnused, kXfbVerticesPerInstance, kNumSamples,
         kXfbBufferOffsets, kAcbBufferOffsets, kDepthRange}};

    // This field list mirrors the structure of GraphicsDriverUniforms in ContextVk.cpp.
    TFieldList *driverFieldList = new TFieldList;

    const std::array<TType *, kNumGraphicsDriverUniforms> kDriverUniformTypes = {{
        new TType(EbtFloat, EbpHigh, EvqGlobal, 4),
        new TType(EbtUInt, EbpHigh,
                  EvqGlobal),  // uint clipDistancesEnabled;  // 32 bits for 32 clip distances max
        new TType(EbtUInt, EbpHigh, EvqGlobal),  // Gap usable for future
        new TType(EbtInt, EbpHigh, EvqGlobal),
        new TType(EbtInt, EbpLow, EvqGlobal),  // uint numSamples;         // Up to 16
        new TType(EbtInt, EbpHigh, EvqGlobal, 4),
        new TType(EbtUInt, EbpHigh, EvqGlobal, 4),
        createEmulatedDepthRangeType(symbolTable),
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumGraphicsDriverUniforms; ++uniformIndex)
    {
        TField *driverUniformField =
            new TField(kDriverUniformTypes[uniformIndex],
                       ImmutableString(kGraphicsDriverUniformNames[uniformIndex]), TSourceLoc(),
                       SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    return driverFieldList;
}

TType *DriverUniform::createEmulatedDepthRangeType(TSymbolTable *symbolTable)
{
    // If already defined, return it immediately.
    if (mEmulatedDepthRangeType != nullptr)
    {
        return mEmulatedDepthRangeType;
    }

    // Create the depth range type.
    TFieldList *depthRangeParamsFields = new TFieldList();
    TType *floatType                   = new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1);
    depthRangeParamsFields->push_back(
        new TField(floatType, ImmutableString("near"), TSourceLoc(), SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(
        new TField(floatType, ImmutableString("far"), TSourceLoc(), SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(
        new TField(floatType, ImmutableString("diff"), TSourceLoc(), SymbolType::AngleInternal));
    // This additional field might be used by subclass such as TranslatorMetal.
    depthRangeParamsFields->push_back(new TField(floatType, ImmutableString("reserved"),
                                                 TSourceLoc(), SymbolType::AngleInternal));

    TStructure *emulatedDepthRangeParams = new TStructure(
        symbolTable, kEmulatedDepthRangeParams, depthRangeParamsFields, SymbolType::AngleInternal);

    mEmulatedDepthRangeType = new TType(emulatedDepthRangeParams, false);

    // Note: this should really return a const TType *, but one of its uses is with TField who takes
    // a non-const TType.  See comment on that class.
    return mEmulatedDepthRangeType;
}

// The Add*DriverUniformsToShader operation adds an internal uniform block to a shader. The driver
// block is used to implement Vulkan-specific features and workarounds. Returns the driver uniforms
// variable.
//
// There are Graphics and Compute variations as they require different uniforms.
bool DriverUniform::addGraphicsDriverUniformsToShader(TIntermBlock *root, TSymbolTable *symbolTable)
{
    ASSERT(!mDriverUniforms);

    // Declare the depth range struct type.
    TType *emulatedDepthRangeType     = createEmulatedDepthRangeType(symbolTable);
    TType *emulatedDepthRangeDeclType = new TType(emulatedDepthRangeType->getStruct(), true);

    TVariable *depthRangeVar =
        new TVariable(symbolTable->nextUniqueId(), kEmptyImmutableString, SymbolType::Empty,
                      TExtension::UNDEFINED, emulatedDepthRangeDeclType);

    DeclareGlobalVariable(root, depthRangeVar);

    TFieldList *driverFieldList = createUniformFields(symbolTable);
    if (mMode == DriverUniformMode::InterfaceBlock)
    {
        // Define a driver uniform block "ANGLEUniformBlock" with instance name "ANGLEUniforms".
        TLayoutQualifier layoutQualifier = TLayoutQualifier::Create();
        layoutQualifier.blockStorage     = EbsStd140;

        mDriverUniforms = DeclareInterfaceBlock(root, symbolTable, driverFieldList, EvqUniform,
                                                layoutQualifier, TMemoryQualifier::Create(), 0,
                                                ImmutableString(vk::kDriverUniformsBlockName),
                                                ImmutableString(vk::kDriverUniformsVarName));
    }
    else
    {
        // Declare a structure "ANGLEUniformBlock" with instance name "ANGLE_angleUniforms".
        // This code path is taken only by the direct-to-Metal backend, and the assumptions
        // about the naming conventions of ANGLE-internal variables run too deeply to rename
        // this one.
        auto varName    = ImmutableString("ANGLE_angleUniforms");
        auto result     = DeclareStructure(root, symbolTable, driverFieldList, EvqUniform,
                                       TMemoryQualifier::Create(), 0,
                                       ImmutableString(vk::kDriverUniformsBlockName), &varName);
        mDriverUniforms = result.second;
    }

    return mDriverUniforms != nullptr;
}

TIntermTyped *DriverUniform::createDriverUniformRef(const char *fieldName) const
{
    size_t fieldIndex = 0;
    if (mMode == DriverUniformMode::InterfaceBlock)
    {
        fieldIndex =
            FindFieldIndex(mDriverUniforms->getType().getInterfaceBlock()->fields(), fieldName);
    }
    else
    {
        fieldIndex = FindFieldIndex(mDriverUniforms->getType().getStruct()->fields(), fieldName);
    }

    TIntermSymbol *angleUniformsRef = new TIntermSymbol(mDriverUniforms);
    TConstantUnion *uniformIndex    = new TConstantUnion;
    uniformIndex->setIConst(static_cast<int>(fieldIndex));
    TIntermConstantUnion *indexRef =
        new TIntermConstantUnion(uniformIndex, *StaticType::GetBasic<EbtInt, EbpLow>());
    if (mMode == DriverUniformMode::InterfaceBlock)
    {
        return new TIntermBinary(EOpIndexDirectInterfaceBlock, angleUniformsRef, indexRef);
    }
    return new TIntermBinary(EOpIndexDirectStruct, angleUniformsRef, indexRef);
}

TIntermTyped *DriverUniform::getViewportRef() const
{
    return createDriverUniformRef(kViewport);
}

TIntermTyped *DriverUniform::getAbcBufferOffsets() const
{
    return createDriverUniformRef(kAcbBufferOffsets);
}

TIntermTyped *DriverUniform::getXfbVerticesPerInstance() const
{
    return createDriverUniformRef(kXfbVerticesPerInstance);
}

TIntermTyped *DriverUniform::getXfbBufferOffsets() const
{
    return createDriverUniformRef(kXfbBufferOffsets);
}

TIntermTyped *DriverUniform::getClipDistancesEnabled() const
{
    return createDriverUniformRef(kClipDistancesEnabled);
}

TIntermTyped *DriverUniform::getDepthRangeRef() const
{
    return createDriverUniformRef(kDepthRange);
}

TIntermTyped *DriverUniform::getDepthRangeReservedFieldRef() const
{
    TIntermTyped *depthRange = createDriverUniformRef(kDepthRange);

    return new TIntermBinary(EOpIndexDirectStruct, depthRange, CreateIndexNode(3));
}

TIntermTyped *DriverUniform::getNumSamplesRef() const
{
    return createDriverUniformRef(kNumSamples);
}

//
// Class DriverUniformExtended
//
TFieldList *DriverUniformExtended::createUniformFields(TSymbolTable *symbolTable)
{
    TFieldList *driverFieldList = DriverUniform::createUniformFields(symbolTable);

    constexpr size_t kNumGraphicsDriverUniformsExt = 7;
    constexpr std::array<const char *, kNumGraphicsDriverUniformsExt>
        kGraphicsDriverUniformNamesExt = {
            {kHalfRenderArea, kFlipXY, kNegFlipXY, kDither, kUnused2, kFragRotation, kPreRotation}};

    const std::array<TType *, kNumGraphicsDriverUniformsExt> kDriverUniformTypesExt = {{
        new TType(EbtFloat, EbpHigh, EvqGlobal, 2),
        new TType(EbtFloat, EbpLow, EvqGlobal, 2),
        new TType(EbtFloat, EbpLow, EvqGlobal, 2),
        new TType(EbtUInt, EbpHigh, EvqGlobal),
        new TType(EbtUInt, EbpHigh, EvqGlobal),
        new TType(EbtFloat, EbpLow, EvqGlobal, 2, 2),
        new TType(EbtFloat, EbpLow, EvqGlobal, 2, 2),
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumGraphicsDriverUniformsExt; ++uniformIndex)
    {
        TField *driverUniformField =
            new TField(kDriverUniformTypesExt[uniformIndex],
                       ImmutableString(kGraphicsDriverUniformNamesExt[uniformIndex]), TSourceLoc(),
                       SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    return driverFieldList;
}

TIntermTyped *DriverUniformExtended::getFlipXYRef() const
{
    return createDriverUniformRef(kFlipXY);
}

TIntermTyped *DriverUniformExtended::getNegFlipXYRef() const
{
    return createDriverUniformRef(kNegFlipXY);
}

TIntermTyped *DriverUniformExtended::getNegFlipYRef() const
{
    // Create a swizzle to "negFlipXY.y"
    TIntermTyped *negFlipXY     = createDriverUniformRef(kNegFlipXY);
    TVector<int> swizzleOffsetY = {1};
    TIntermSwizzle *negFlipY    = new TIntermSwizzle(negFlipXY, swizzleOffsetY);
    return negFlipY;
}

TIntermTyped *DriverUniformExtended::getPreRotationMatrixRef() const
{
    return createDriverUniformRef(kPreRotation);
}

TIntermTyped *DriverUniformExtended::getFragRotationMatrixRef() const
{
    return createDriverUniformRef(kFragRotation);
}

TIntermTyped *DriverUniformExtended::getHalfRenderAreaRef() const
{
    return createDriverUniformRef(kHalfRenderArea);
}

TIntermTyped *DriverUniformExtended::getDitherRef() const
{
    return createDriverUniformRef(kDither);
}

}  // namespace sh
