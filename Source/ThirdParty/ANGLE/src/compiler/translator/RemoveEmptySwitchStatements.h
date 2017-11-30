//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RemoveEmptySwitchStatements.h: Remove switch statements that have an empty statement list.

#ifndef COMPILER_TRANSLATOR_REMOVEEMPTYSWITCHSTATEMENTS_H_
#define COMPILER_TRANSLATOR_REMOVEEMPTYSWITCHSTATEMENTS_H_

namespace sh
{
class TIntermBlock;

void RemoveEmptySwitchStatements(TIntermBlock *root);
}

#endif  // COMPILER_TRANSLATOR_REMOVEEMPTYSWITCHSTATEMENTS_H_