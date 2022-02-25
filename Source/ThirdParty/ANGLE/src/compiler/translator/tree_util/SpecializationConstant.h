//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SpecializationConst.h: Add code to generate AST node for specialization constant.
//

#ifndef COMPILER_TRANSLATOR_TREEUTIL_SPECIALIZATIONCONSTANT_H_
#define COMPILER_TRANSLATOR_TREEUTIL_SPECIALIZATIONCONSTANT_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/SymbolTable.h"

class TIntermBlock;
class TIntermTyped;
class TIntermSymbol;
class TVariable;

namespace sh
{

class SpecConst
{
  public:
    SpecConst(TSymbolTable *symbolTable, ShCompileOptions compileOptions, GLenum shaderType);
    virtual ~SpecConst();

    // Line rasterizaton emulation
    TIntermSymbol *getLineRasterEmulation();

    // Flip/rotation
    TIntermTyped *getMultiplierXForDFdx();
    TIntermTyped *getMultiplierYForDFdx();
    TIntermTyped *getMultiplierXForDFdy();
    TIntermTyped *getMultiplierYForDFdy();
    TIntermTyped *getPreRotationMatrix();
    TIntermTyped *getFragRotationMatrix();
    TIntermTyped *getFlipXY();
    TIntermTyped *getNegFlipXY();
    TIntermTyped *getFlipY();
    TIntermTyped *getFragRotationMultiplyFlipXY();

    // Half render area
    TIntermTyped *getHalfRenderArea();

    // Dither emulation
    TIntermTyped *getDither();

    void declareSpecConsts(TIntermBlock *root);
    SpecConstUsageBits getSpecConstUsageBits() const { return mUsageBits; }

  private:
    TIntermSymbol *getFlipRotation();
    TIntermTyped *getNegFlipY();
    TIntermSymbol *getDrawableWidth();
    TIntermSymbol *getDrawableHeight();
    TIntermTyped *getHalfRenderAreaRotationMatrix();

    // If unsupported, this should be set to null.
    TSymbolTable *mSymbolTable;
    ShCompileOptions mCompileOptions;

    TVariable *mLineRasterEmulationVar;
    TVariable *mSurfaceRotationVar;
    TVariable *mDrawableWidthVar;
    TVariable *mDrawableHeightVar;
    TVariable *mDitherVar;

    // Bit is set if YFlip or Rotation has been used
    SpecConstUsageBits mUsageBits;
};
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEUTIL_SPECIALIZATIONCONSTANT_H_
