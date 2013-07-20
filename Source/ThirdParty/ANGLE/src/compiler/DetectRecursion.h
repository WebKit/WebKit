//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_DETECT_RECURSION_H_
#define COMPILER_DETECT_RECURSION_H_

#include "GLSLANG/ShaderLang.h"

#include "compiler/intermediate.h"
#include "compiler/VariableInfo.h"

// Traverses intermediate tree to detect function recursion.
class DetectRecursion : public TIntermTraverser {
public:
    enum ErrorCode {
        kErrorMissingMain,
        kErrorRecursion,
        kErrorNone
    };

    DetectRecursion();
    ~DetectRecursion();

    virtual bool visitAggregate(Visit, TIntermAggregate*);

    ErrorCode detectRecursion();

private:
    class FunctionNode {
    public:
        FunctionNode(const TString& fname);

        const TString& getName() const;

        // If a function is already in the callee list, this becomes a no-op.
        void addCallee(FunctionNode* callee);

        // Return true if recursive function calls are detected.
        bool detectRecursion();

    private:
        // mangled function name is unique.
        TString name;

        // functions that are directly called by this function.
        TVector<FunctionNode*> callees;

        Visit visit;
    };

    FunctionNode* findFunctionByName(const TString& name);

    TVector<FunctionNode*> functions;
    FunctionNode* currentFunction;
};

#endif  // COMPILER_DETECT_RECURSION_H_
