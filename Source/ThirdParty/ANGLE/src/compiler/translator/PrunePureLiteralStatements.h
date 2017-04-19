//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PrunePureLiteralStatements.h: Removes statements that are literals and nothing else.

#ifndef COMPILER_TRANSLATOR_PRUNEPURELITERALSTATEMENTS_H_
#define COMPILER_TRANSLATOR_PRUNEPURELITERALSTATEMENTS_H_

namespace sh
{
class TIntermNode;

void PrunePureLiteralStatements(TIntermNode *root);
}

#endif  // COMPILER_TRANSLATOR_PRUNEPURELITERALSTATEMENTS_H_
