//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DriverUniformMetal:
//   Struct defining the default driver uniforms for direct and SpirV based ANGLE translation
//

#ifndef LIBANGLE_RENDERER_METAL_DRIVERUNIFORMMETAL_H_
#define LIBANGLE_RENDERER_METAL_DRIVERUNIFORMMETAL_H_

#include "compiler/translator/tree_util/DriverUniform.h"

namespace sh
{

class DriverUniformMetal : public DriverUniform
{
  public:
    DriverUniformMetal(DriverUniformMode mode) : DriverUniform(mode) {}
    DriverUniformMetal() : DriverUniform(DriverUniformMode::InterfaceBlock) {}
    ~DriverUniformMetal() override {}

    TIntermTyped *getHalfRenderAreaRef() const override;
    TIntermTyped *getFlipXYRef() const override;
    TIntermTyped *getNegFlipXYRef() const override;
    TIntermTyped *getNegFlipYRef() const override;
    TIntermTyped *getCoverageMaskFieldRef() const;

  protected:
    TFieldList *createUniformFields(TSymbolTable *symbolTable) override;
};

}  // namespace sh

#endif /* LIBANGLE_RENDERER_METAL_DRIVERUNIFORMMETAL_H_ */
