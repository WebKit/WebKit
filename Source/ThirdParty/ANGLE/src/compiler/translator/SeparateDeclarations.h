//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The SeparateArrayDeclarations function processes declarations that contain array declarators. Each declarator in
// such declarations gets its own declaration.
// This is useful as an intermediate step when initialization needs to be separated from declaration.
// Example:
//     int a[1] = int[1](1), b[1] = int[1](2);
// gets transformed when run through this class into the AST equivalent of:
//     int a[1] = int[1](1);
//     int b[1] = int[1](2);

#ifndef COMPILER_TRANSLATOR_SEPARATEDECLARATIONS_H_
#define COMPILER_TRANSLATOR_SEPARATEDECLARATIONS_H_

class TIntermNode;

void SeparateArrayDeclarations(TIntermNode *root);

#endif  // COMPILER_TRANSLATOR_SEPARATEDECLARATIONS_H_
