//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_MSL_NODETYPE_H_
#define COMPILER_TRANSLATOR_MSL_NODETYPE_H_

#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

enum class NodeType
{
    Unknown,
    Symbol,
    ConstantUnion,
    FunctionPrototype,
    PreprocessorDirective,
    Unary,
    Binary,
    Ternary,
    Swizzle,
    IfElse,
    Switch,
    Case,
    FunctionDefinition,
    Aggregate,
    Block,
    GlobalQualifierDeclaration,
    Declaration,
    Loop,
    Branch,
};

// This is a function like object instead of a function that stack allocates this because
// TIntermTraverser is a heavy object to construct.
class GetNodeType : private TIntermTraverser
{
    NodeType nodeType;

  public:
    GetNodeType() : TIntermTraverser(true, false, false) {}

    NodeType operator()(TIntermNode &node)
    {
        node.visit(Visit::PreVisit, this);
        return nodeType;
    }

  private:
    void visitSymbol(TIntermSymbol *) override { nodeType = NodeType::Symbol; }

    void visitConstantUnion(TIntermConstantUnion *) override { nodeType = NodeType::ConstantUnion; }

    void visitFunctionPrototype(TIntermFunctionPrototype *) override
    {
        nodeType = NodeType::FunctionPrototype;
    }

    void visitPreprocessorDirective(TIntermPreprocessorDirective *) override
    {
        nodeType = NodeType::PreprocessorDirective;
    }

    bool visitSwizzle(Visit, TIntermSwizzle *) override
    {
        nodeType = NodeType::Swizzle;
        return false;
    }

    bool visitBinary(Visit, TIntermBinary *) override
    {
        nodeType = NodeType::Binary;
        return false;
    }

    bool visitUnary(Visit, TIntermUnary *) override
    {
        nodeType = NodeType::Unary;
        return false;
    }

    bool visitTernary(Visit, TIntermTernary *) override
    {
        nodeType = NodeType::Ternary;
        return false;
    }

    bool visitIfElse(Visit, TIntermIfElse *) override
    {
        nodeType = NodeType::IfElse;
        return false;
    }

    bool visitSwitch(Visit, TIntermSwitch *) override
    {
        nodeType = NodeType::Switch;
        return false;
    }

    bool visitCase(Visit, TIntermCase *) override
    {
        nodeType = NodeType::Case;
        return false;
    }

    bool visitFunctionDefinition(Visit, TIntermFunctionDefinition *) override
    {
        nodeType = NodeType::FunctionDefinition;
        return false;
    }

    bool visitAggregate(Visit, TIntermAggregate *) override
    {
        nodeType = NodeType::Aggregate;
        return false;
    }

    bool visitBlock(Visit, TIntermBlock *) override
    {
        nodeType = NodeType::Block;
        return false;
    }

    bool visitGlobalQualifierDeclaration(Visit, TIntermGlobalQualifierDeclaration *) override
    {
        nodeType = NodeType::GlobalQualifierDeclaration;
        return false;
    }

    bool visitDeclaration(Visit, TIntermDeclaration *) override
    {
        nodeType = NodeType::Declaration;
        return false;
    }

    bool visitLoop(Visit, TIntermLoop *) override
    {
        nodeType = NodeType::Loop;
        return false;
    }

    bool visitBranch(Visit, TIntermBranch *) override
    {
        nodeType = NodeType::Branch;
        return false;
    }
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_MSL_NODETYPE_H_
