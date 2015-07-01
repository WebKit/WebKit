//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_VALIDATEOUTPUTS_H_
#define COMPILER_TRANSLATOR_VALIDATEOUTPUTS_H_

#include "compiler/translator/IntermNode.h"

#include <set>

class TInfoSinkBase;

class ValidateOutputs : public TIntermTraverser
{
  public:
    ValidateOutputs(TInfoSinkBase& sink, int maxDrawBuffers);

    int numErrors() const { return mNumErrors; }

    virtual void visitSymbol(TIntermSymbol*);

  private:
    TInfoSinkBase& mSink;
    int mMaxDrawBuffers;
    int mNumErrors;
    bool mHasUnspecifiedOutputLocation;

    typedef std::map<int, TIntermSymbol*> OutputMap;
    OutputMap mOutputMap;
    std::set<TString> mVisitedSymbols;

    void error(TSourceLoc loc, const char *reason, const char* token);
};

#endif // COMPILER_TRANSLATOR_VALIDATEOUTPUTS_H_
