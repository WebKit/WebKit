//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/SimplifyArrayAssignment.h"

bool SimplifyArrayAssignment::visitBinary(Visit visit, TIntermBinary *node)
{
    switch (node->getOp())
    {
      case EOpAssign:
        {
            TIntermNode *parent = getParentNode();
            if (node->getLeft()->isArray() && parent != nullptr)
            {
                TIntermAggregate *parentAgg = parent->getAsAggregate();
                if (parentAgg != nullptr && parentAgg->getOp() == EOpSequence)
                {
                    // This case is fine, the result of the assignment is not used.
                    break;
                }

                // The result of the assignment needs to be stored into a temporary variable,
                // the assignment needs to be replaced with a reference to the temporary variable,
                // and the temporary variable needs to finally be assigned to the target variable.

                // This also needs to interact correctly with unfolding short circuiting operators.
                UNIMPLEMENTED();
            }
        }
        break;
      default:
        break;
    }
    return true;
}
