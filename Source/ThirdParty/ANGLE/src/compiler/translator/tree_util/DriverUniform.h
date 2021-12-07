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

    TIntermBinary *getViewportRef() const;
    TIntermBinary *getAbcBufferOffsets() const;
    TIntermBinary *getXfbActiveUnpaused() const;
    TIntermBinary *getXfbVerticesPerInstance() const;
    TIntermBinary *getXfbBufferOffsets() const;
    TIntermBinary *getClipDistancesEnabled() const;
    TIntermBinary *getDepthRangeRef() const;
    TIntermBinary *getDepthRangeReservedFieldRef() const;
    TIntermBinary *getNumSamplesRef() const;

    virtual TIntermBinary *getFlipXYRef() const { return nullptr; }
    virtual TIntermBinary *getNegFlipXYRef() const { return nullptr; }
    virtual TIntermBinary *getPreRotationMatrixRef() const { return nullptr; }
    virtual TIntermBinary *getFragRotationMatrixRef() const { return nullptr; }
    virtual TIntermBinary *getHalfRenderAreaRef() const { return nullptr; }
    virtual TIntermSwizzle *getNegFlipYRef() const { return nullptr; }
    virtual TIntermBinary *getEmulatedInstanceId() const { return nullptr; }
    virtual TIntermBinary *getCoverageMask() const { return nullptr; }

    const TVariable *getDriverUniformsVariable() const { return mDriverUniforms; }

  protected:
    TIntermBinary *createDriverUniformRef(const char *fieldName) const;
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

    TIntermBinary *getFlipXYRef() const override;
    TIntermBinary *getNegFlipXYRef() const override;
    TIntermBinary *getPreRotationMatrixRef() const override;
    TIntermBinary *getFragRotationMatrixRef() const override;
    TIntermBinary *getHalfRenderAreaRef() const override;
    TIntermSwizzle *getNegFlipYRef() const override;
    TIntermBinary *getEmulatedInstanceId() const override;
    TIntermBinary *getCoverageMask() const override;

  protected:
    virtual TFieldList *createUniformFields(TSymbolTable *symbolTable) override;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEUTIL_DRIVERUNIFORM_H_
