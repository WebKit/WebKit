// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Rewrite "-float" to "0.0 - float" to work around unary minus operator on float issue on Intel Mac
// OSX 10.11.

#ifndef COMPILER_TRANSLATOR_REWRITEUNARYMINUSOPERATORFLOAT_H_
#define COMPILER_TRANSLATOR_REWRITEUNARYMINUSOPERATORFLOAT_H_

class TIntermNode;
namespace sh
{

void RewriteUnaryMinusOperatorFloat(TIntermNode *root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_REWRITEUNARYMINUSOPERATORFLOAT_H_
