//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DriverUniform.h: Add code to support driver uniforms
//

#ifndef COMPILER_TRANSLATOR_TREEUTIL_DRIVERUNIFORM_H_
#define COMPILER_TRANSLATOR_TREEUTIL_DRIVERUNIFORM_H_

#include "common/angleutils.h"
#include "compiler/translator/Types.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
class TIntermNode;
class TSymbolTable;
class TIntermTyped;
class TIntermSwizzle;
class TIntermBinary;

enum class DriverUniformMode
{
    // Define the driver uniforms as an interface block. Used by the
    // Vulkan and Metal/SPIR-V backends.
    InterfaceBlock,

    // Define the driver uniforms as a structure. Used by the
    // direct-to-MSL Metal backend.
    Structure
};

class DriverUniform
{
  public:
    DriverUniform(DriverUniformMode mode)
        : mMode(mode), mDriverUniforms(nullptr), mEmulatedDepthRangeType(nullptr)
    {}
    virtual ~DriverUniform() = default;

    bool addComputeDriverUniformsToShader(TIntermBlock *root, TSymbolTable *symbolTable);
    bool addGraphicsDriverUniformsToShader(TIntermBlock *root, TSymbolTable *symbolTable);

    TIntermTyped *getViewportRef() const;
    TIntermTyped *getAbcBufferOffsets() const;
    TIntermTyped *getXfbVerticesPerInstance() const;
    TIntermTyped *getXfbBufferOffsets() const;
    TIntermTyped *getClipDistancesEnabled() const;
    TIntermTyped *getDepthRangeRef() const;
    TIntermTyped *getDepthRangeReservedFieldRef() const;
    TIntermTyped *getNumSamplesRef() const;

    virtual TIntermTyped *getFlipXYRef() const { return nullptr; }
    virtual TIntermTyped *getNegFlipXYRef() const { return nullptr; }
    virtual TIntermTyped *getPreRotationMatrixRef() const { return nullptr; }
    virtual TIntermTyped *getFragRotationMatrixRef() const { return nullptr; }
    virtual TIntermTyped *getHalfRenderAreaRef() const { return nullptr; }
    virtual TIntermTyped *getNegFlipYRef() const { return nullptr; }
    virtual TIntermTyped *getDitherRef() const { return nullptr; }

    const TVariable *getDriverUniformsVariable() const { return mDriverUniforms; }

  protected:
    TIntermTyped *createDriverUniformRef(const char *fieldName) const;
    virtual TFieldList *createUniformFields(TSymbolTable *symbolTable);
    TType *createEmulatedDepthRangeType(TSymbolTable *symbolTable);

    const DriverUniformMode mMode;
    const TVariable *mDriverUniforms;
    TType *mEmulatedDepthRangeType;
};

class DriverUniformExtended : public DriverUniform
{
  public:
    DriverUniformExtended(DriverUniformMode mode) : DriverUniform(mode) {}
    virtual ~DriverUniformExtended() override {}

    TIntermTyped *getFlipXYRef() const override;
    TIntermTyped *getNegFlipXYRef() const override;
    TIntermTyped *getPreRotationMatrixRef() const override;
    TIntermTyped *getFragRotationMatrixRef() const override;
    TIntermTyped *getHalfRenderAreaRef() const override;
    TIntermTyped *getNegFlipYRef() const override;
    TIntermTyped *getDitherRef() const override;

  protected:
    virtual TFieldList *createUniformFields(TSymbolTable *symbolTable) override;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEUTIL_DRIVERUNIFORM_H_
