//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_UNFOLDSELECT_H_
#define COMPILER_UNFOLDSELECT_H_

#include "compiler/intermediate.h"
#include "compiler/ParseHelper.h"

namespace sh
{
class OutputHLSL;

class UnfoldSelect : public TIntermTraverser
{
  public:
    UnfoldSelect(TParseContext &context, OutputHLSL *outputHLSL);

    void traverse(TIntermNode *node);
    bool visitSelection(Visit visit, TIntermSelection *node);

    int getTemporaryIndex();

  protected:
    TParseContext &mContext;
    OutputHLSL *const mOutputHLSL;

    int mTemporaryIndex;
};
}

#endif   // COMPILER_UNFOLDSELECT_H_
