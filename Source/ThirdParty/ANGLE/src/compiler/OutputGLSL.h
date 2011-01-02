//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef CROSSCOMPILERGLSL_OUTPUTGLSL_H_
#define CROSSCOMPILERGLSL_OUTPUTGLSL_H_

#include <set>

#include "compiler/intermediate.h"
#include "compiler/ParseHelper.h"

class TOutputGLSL : public TIntermTraverser
{
public:
    TOutputGLSL(TInfoSinkBase& objSink);

protected:
    TInfoSinkBase& objSink() { return mObjSink; }
    void writeTriplet(Visit visit, const char* preStr, const char* inStr, const char* postStr);
    void writeVariableType(const TType& type);
    void writeFunctionParameters(const TIntermSequence& args);
    const ConstantUnion* writeConstantUnion(const TType& type, const ConstantUnion* pConstUnion);

    virtual void visitSymbol(TIntermSymbol* node);
    virtual void visitConstantUnion(TIntermConstantUnion* node);
    virtual bool visitBinary(Visit visit, TIntermBinary* node);
    virtual bool visitUnary(Visit visit, TIntermUnary* node);
    virtual bool visitSelection(Visit visit, TIntermSelection* node);
    virtual bool visitAggregate(Visit visit, TIntermAggregate* node);
    virtual bool visitLoop(Visit visit, TIntermLoop* node);
    virtual bool visitBranch(Visit visit, TIntermBranch* node);

    void visitCodeBlock(TIntermNode* node);

private:
    TInfoSinkBase& mObjSink;
    bool mDeclaringVariables;

    // Structs are declared as the tree is traversed. This set contains all
    // the structs already declared. It is maintained so that a struct is
    // declared only once.
    typedef std::set<TString> DeclaredStructs;
    DeclaredStructs mDeclaredStructs;
};

#endif  // CROSSCOMPILERGLSL_OUTPUTGLSL_H_
