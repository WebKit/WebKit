//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RewriteDoWhile.h: rewrite do-while loops as while loops to work around
// driver bugs

#ifndef COMPILER_TRANSLATOR_REWRITEDOWHILE_H_
#define COMPILER_TRANSLATOR_REWRITEDOWHILE_H_

class TIntermNode;
void RewriteDoWhile(TIntermNode *root, unsigned int *temporaryIndex);

#endif  // COMPILER_TRANSLATOR_REWRITEDOWHILE_H_
