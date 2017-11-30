//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ValidateMaxParameters checks if function definitions have more than a set number of parameters.

#include "compiler/translator/ValidateMaxParameters.h"

#include "compiler/translator/IntermNode.h"

namespace sh
{

bool ValidateMaxParameters(TIntermBlock *root, unsigned int maxParameters)
{
    for (TIntermNode *node : *root->getSequence())
    {
        TIntermFunctionDefinition *definition = node->getAsFunctionDefinition();
        if (definition != nullptr &&
            definition->getFunctionPrototype()->getSequence()->size() > maxParameters)
        {
            return false;
        }
    }
    return true;
}

}  // namespace sh
