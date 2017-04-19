//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_VALIDATEOUTPUTS_H_
#define COMPILER_TRANSLATOR_VALIDATEOUTPUTS_H_

#include "compiler/translator/ExtensionBehavior.h"
#include "compiler/translator/IntermNode.h"

#include <set>

namespace sh
{

class TInfoSinkBase;

class ValidateOutputs : public TIntermTraverser
{
  public:
    ValidateOutputs(const TExtensionBehavior &extBehavior, int maxDrawBuffers);

    void validate(TDiagnostics *diagnostics) const;

    void visitSymbol(TIntermSymbol *) override;

  private:
    int mMaxDrawBuffers;
    bool mAllowUnspecifiedOutputLocationResolution;
    bool mUsesFragDepth;

    typedef std::vector<TIntermSymbol *> OutputVector;
    OutputVector mOutputs;
    OutputVector mUnspecifiedLocationOutputs;
    OutputVector mYuvOutputs;
    std::set<std::string> mVisitedSymbols;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_VALIDATEOUTPUTS_H_
