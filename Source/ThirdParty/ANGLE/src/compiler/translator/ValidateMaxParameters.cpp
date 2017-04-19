//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ValidateMaxParameters checks if function definitions have more than a set number of parameters.

#include "compiler/translator/ValidateMaxParameters.h"

namespace sh
{

ValidateMaxParameters::ValidateMaxParameters(unsigned int maxParameters)
    : TIntermTraverser(true, false, false), mMaxParameters(maxParameters), mValid(true)
{
}

bool ValidateMaxParameters::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    if (!mValid)
    {
        return false;
    }

    if (node->getFunctionPrototype()->getSequence()->size() > mMaxParameters)
    {
        mValid = false;
    }

    return mValid;
}

bool ValidateMaxParameters::validate(TIntermNode *root, unsigned int maxParameters)
{
    ValidateMaxParameters argsTraverser(maxParameters);
    root->traverse(&argsTraverser);
    return argsTraverser.mValid;
}

}  // namespace sh
