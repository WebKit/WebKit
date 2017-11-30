//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RemoveArrayLengthMethod.h:
//   Fold array length expressions, including cases where the "this" node has side effects.
//   Example:
//     int i = (a = b).length();
//     int j = (func()).length();
//   becomes:
//     (a = b);
//     int i = <constant array length>;
//     func();
//     int j = <constant array length>;
//
//   Must be run after SplitSequenceOperator, SimplifyLoopConditions and SeparateDeclarations steps
//   have been done to expressions containing calls of the array length method.
//
//   Does nothing to length method calls done on runtime-sized arrays.

namespace sh
{

class TIntermBlock;

void RemoveArrayLengthMethod(TIntermBlock *root);

}  // namespace sh
