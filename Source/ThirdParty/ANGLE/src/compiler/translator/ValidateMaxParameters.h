//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ValidateMaxParameters checks if function definitions have more than a set number of parameters.

#ifndef COMPILER_TRANSLATOR_VALIDATEMAXPARAMETERS_H_
#define COMPILER_TRANSLATOR_VALIDATEMAXPARAMETERS_H_

#include "compiler/translator/IntermNode.h"

class ValidateMaxParameters : public TIntermTraverser
{
  public:
    // Returns false if maxParameters is exceeded.
    static bool validate(TIntermNode *root, unsigned int maxParameters);

  protected:
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;

  private:
    ValidateMaxParameters(unsigned int maxParameters);

    unsigned int mMaxParameters;
    bool mValid;
};

#endif  // COMPILER_TRANSLATOR_VALIDATEMAXPARAMETERS_H_
