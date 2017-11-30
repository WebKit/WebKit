//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AddDefaultReturnStatements.h: Add default return statements to functions that do not end in a
//                               return.
//

#ifndef COMPILER_TRANSLATOR_ADDDEFAULTRETURNSTATEMENTS_H_
#define COMPILER_TRANSLATOR_ADDDEFAULTRETURNSTATEMENTS_H_

class TIntermBlock;

namespace sh
{

void AddDefaultReturnStatements(TIntermBlock *root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_ADDDEFAULTRETURNSTATEMENTS_H_
