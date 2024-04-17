//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_SEPARATEDECLARATIONS_H_
#define COMPILER_TRANSLATOR_TREEOPS_SEPARATEDECLARATIONS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;

// Transforms declarations so that in the end each declaration contains only one declarator.
// This is useful as an intermediate step when initialization needs to be separated from
// declaration, or when things need to be unfolded out of the initializer.
// Examples:
// Input:
//     int a[1] = int[1](1), b[1] = int[1](2);
// Output:
//     int a[1] = int[1](1);
//     int b[1] = int[1](2);
// Input:
//    struct S { vec3 d; } a, b;
// Output:
//    struct S { vec3 d; } a;
//    S b;
// Input:
//    struct { vec3 d; } a, b;
// Output:
//    struct s1234 { vec3 d; } a;
//    s1234 b;
[[nodiscard]] bool SeparateDeclarations(TCompiler &compiler, TIntermBlock &root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_SEPARATEDECLARATIONS_H_
