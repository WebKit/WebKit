//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RemoveDynamicIndexing is an AST traverser to remove dynamic indexing of non-SSBO vectors and
// matrices, replacing them with calls to functions that choose which component to return or write.
// We don't need to consider dynamic indexing in SSBO since it can be directly as part of the offset
// of RWByteAddressBuffer.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_REMOVEDYNAMICINDEXING_H_
#define COMPILER_TRANSLATOR_TREEOPS_REMOVEDYNAMICINDEXING_H_

namespace sh
{

class TIntermNode;
class TSymbolTable;
class PerformanceDiagnostics;

void RemoveDynamicIndexing(TIntermNode *root,
                           TSymbolTable *symbolTable,
                           PerformanceDiagnostics *perfDiagnostics);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REMOVEDYNAMICINDEXING_H_
