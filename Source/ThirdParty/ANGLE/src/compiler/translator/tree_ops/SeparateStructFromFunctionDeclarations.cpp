//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SeparateStructFromFunctionDeclarations: Separate struct declarations from function declaration
// return type.
//

#include "compiler/translator/tree_ops/SeparateStructFromFunctionDeclarations.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{
class Traverser : public TIntermTraverser
{
  public:
    explicit Traverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable)
    {}

    void visitFunctionPrototype(TIntermFunctionPrototype *node) override
    {
        const TFunction *function = node->getFunction();
        if (mFunctionsToReplace.count(function) > 0)
        {
            TIntermFunctionPrototype *newFuncProto =
                new TIntermFunctionPrototype(mFunctionsToReplace[function]);
            queueReplacement(newFuncProto, OriginalNode::IS_DROPPED);
        }
        else
        {
            const TType &type = node->getType();
            if (type.isStructSpecifier())
            {
                doReplacement(function, type);
            }
        }
    }

    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        const TFunction *function = node->getFunction();
        if (mFunctionsToReplace.count(function) > 0)
        {
            TIntermAggregate *replacementNode = TIntermAggregate::CreateFunctionCall(
                *mFunctionsToReplace[function], node->getSequence());
            queueReplacement(replacementNode, OriginalNode::IS_DROPPED);
        }
        return true;
    }

  private:
    void doReplacement(const TFunction *function, const TType &oldType)
    {
        const TStructure *structure = oldType.getStruct();
        // Name unnamed inline structs
        if (structure->symbolType() == SymbolType::Empty)
        {
            structure = new TStructure(mSymbolTable, kEmptyImmutableString, &structure->fields(),
                                       SymbolType::AngleInternal);
        }

        TVariable *structVar = new TVariable(mSymbolTable, ImmutableString(""),
                                             new TType(structure, true), SymbolType::Empty);
        insertStatementInParentBlock(new TIntermDeclaration({structVar}));

        TType *returnType = new TType(structure, false);
        if (oldType.isArray())
        {
            returnType->makeArrays(oldType.getArraySizes());
        }
        returnType->setQualifier(oldType.getQualifier());

        const TFunction *oldFunc = function;
        ASSERT(oldFunc->symbolType() == SymbolType::UserDefined);

        const TFunction *newFunc =
            cloneFunctionAndChangeReturnType(mSymbolTable, oldFunc, returnType);

        TIntermFunctionPrototype *newFuncProto = new TIntermFunctionPrototype(newFunc);
        queueReplacement(newFuncProto, OriginalNode::IS_DROPPED);

        mFunctionsToReplace[oldFunc] = newFunc;
    }

    const TFunction *cloneFunctionAndChangeReturnType(TSymbolTable *symbolTable,
                                                      const TFunction *oldFunc,
                                                      const TType *newReturnType)

    {
        ASSERT(oldFunc->symbolType() == SymbolType::UserDefined);

        TFunction *newFunc = new TFunction(symbolTable, oldFunc->name(), oldFunc->symbolType(),
                                           newReturnType, oldFunc->isKnownToNotHaveSideEffects());

        if (oldFunc->isDefined())
        {
            newFunc->setDefined();
        }

        if (oldFunc->hasPrototypeDeclaration())
        {
            newFunc->setHasPrototypeDeclaration();
        }

        const size_t paramCount = oldFunc->getParamCount();
        for (size_t i = 0; i < paramCount; ++i)
        {
            const TVariable *var = oldFunc->getParam(i);
            newFunc->addParameter(var);
        }

        return newFunc;
    }

    using FunctionReplacement = angle::HashMap<const TFunction *, const TFunction *>;
    FunctionReplacement mFunctionsToReplace;
};

}  // anonymous namespace

bool SeparateStructFromFunctionDeclarations(TCompiler *compiler,
                                            TIntermBlock *root,
                                            TSymbolTable *symbolTable)
{
    Traverser separateStructDecls(symbolTable);
    root->traverse(&separateStructDecls);
    return separateStructDecls.updateTree(compiler, root);
}
}  // namespace sh
