//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/DetectRecursion.h"

DetectRecursion::FunctionNode::FunctionNode(const TString& fname)
    : name(fname),
      visit(PreVisit)
{
}

const TString& DetectRecursion::FunctionNode::getName() const
{
    return name;
}

void DetectRecursion::FunctionNode::addCallee(
    DetectRecursion::FunctionNode* callee)
{
    for (size_t i = 0; i < callees.size(); ++i) {
        if (callees[i] == callee)
            return;
    }
    callees.push_back(callee);
}

bool DetectRecursion::FunctionNode::detectRecursion()
{
    ASSERT(visit == PreVisit);
    visit = InVisit;
    for (size_t i = 0; i < callees.size(); ++i) {
        switch (callees[i]->visit) {
            case InVisit:
                // cycle detected, i.e., recursion detected.
                return true;
            case PostVisit:
                break;
            case PreVisit: {
                bool recursion = callees[i]->detectRecursion();
                if (recursion)
                    return true;
                break;
            }
            default:
                UNREACHABLE();
                break;
        }
    }
    visit = PostVisit;
    return false;
}

DetectRecursion::DetectRecursion()
    : currentFunction(NULL)
{
}

DetectRecursion::~DetectRecursion()
{
    for (size_t i = 0; i < functions.size(); ++i)
        delete functions[i];
}

bool DetectRecursion::visitAggregate(Visit visit, TIntermAggregate* node)
{
    switch (node->getOp())
    {
        case EOpPrototype:
            // Function declaration.
            // Don't add FunctionNode here because node->getName() is the
            // unmangled function name.
            break;
        case EOpFunction: {
            // Function definition.
            if (visit == PreVisit) {
                currentFunction = findFunctionByName(node->getName());
                if (currentFunction == NULL) {
                    currentFunction = new FunctionNode(node->getName());
                    functions.push_back(currentFunction);
                }
            }
            break;
        }
        case EOpFunctionCall: {
            // Function call.
            if (visit == PreVisit) {
                ASSERT(currentFunction != NULL);
                FunctionNode* func = findFunctionByName(node->getName());
                if (func == NULL) {
                    func = new FunctionNode(node->getName());
                    functions.push_back(func);
                }
                currentFunction->addCallee(func);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

DetectRecursion::ErrorCode DetectRecursion::detectRecursion()
{
    FunctionNode* main = findFunctionByName("main(");
    if (main == NULL)
        return kErrorMissingMain;
    if (main->detectRecursion())
        return kErrorRecursion;
    return kErrorNone;
}

DetectRecursion::FunctionNode* DetectRecursion::findFunctionByName(
    const TString& name)
{
    for (size_t i = 0; i < functions.size(); ++i) {
        if (functions[i]->getName() == name)
            return functions[i];
    }
    return NULL;
}

