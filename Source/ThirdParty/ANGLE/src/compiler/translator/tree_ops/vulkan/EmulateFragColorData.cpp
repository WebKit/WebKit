//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateFragColorData: Emulate gl_FragColor and gl_FragData.
//

#include "compiler/translator/tree_ops/vulkan/EmulateFragColorData.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"

namespace sh
{
namespace
{
// Traverser that:
//
// 1. Declares outputs corresponding to gl_FragColor and gl_FragData (and their corresponding
//    Secondary versions for framebuffer fetch).
// 2. Replaces built-in references with these variables.
class EmulateFragColorDataTraverser : public TIntermTraverser
{
  public:
    EmulateFragColorDataTraverser(TCompiler *compiler, TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable), mResources(compiler->getResources())
    {}

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TVariable *variable = &symbol->variable();
        const TType &type         = variable->getType();

        // If this built-in was already visited, reuse the variable defined for it.
        auto replacement = mVariableMap.find(variable);
        if (replacement != mVariableMap.end())
        {
            queueReplacement(replacement->second->deepCopy(), OriginalNode::IS_DROPPED);
            return;
        }

        int index        = 0;
        const char *name = "";

        // Replace the built-ins being emulated with a variable of the appropriate type.
        switch (type.getQualifier())
        {
            case EvqFragColor:
                name = "webgl_FragColor";
                break;
            case EvqFragData:
                name = "webgl_FragData";
                break;
            case EvqSecondaryFragColorEXT:
                name  = "webgl_SecondaryFragColor";
                index = 1;
                break;
            case EvqSecondaryFragDataEXT:
                name  = "webgl_SecondaryFragData";
                index = 1;
                break;
            default:
                // Not the built-in we are looking for.
                return;
        }

        TType *outputType = new TType(type);
        outputType->setQualifier(EvqFragmentOut);
        if (index > 0)
        {
            TLayoutQualifier layoutQualifier = outputType->getLayoutQualifier();
            layoutQualifier.index            = index;
            outputType->setLayoutQualifier(layoutQualifier);
        }

        TVariable *replacementVar = new TVariable(mSymbolTable, ImmutableString(name), outputType,
                                                  SymbolType::AngleInternal);

        TIntermSymbol *newSymbol = new TIntermSymbol(replacementVar);
        mVariableMap[variable]   = newSymbol;

        queueReplacement(newSymbol, OriginalNode::IS_DROPPED);
    }

    void addDeclarations(TIntermBlock *root)
    {
        // Insert the declaration before the first function.
        size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
        TIntermSequence declarations;

        for (auto &replaced : mVariableMap)
        {
            TIntermDeclaration *decl = new TIntermDeclaration;
            TIntermSymbol *symbol    = replaced.second->deepCopy()->getAsSymbolNode();
            decl->appendDeclarator(symbol);
            declarations.push_back(decl);
        }

        root->insertChildNodes(firstFunctionIndex, declarations);
    }

  private:
    const ShBuiltInResources &mResources;

    // A map of already replaced built-in variables.
    VariableReplacementMap mVariableMap;
};

bool EmulateFragColorDataImpl(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    EmulateFragColorDataTraverser traverser(compiler, symbolTable);
    root->traverse(&traverser);
    if (!traverser.updateTree(compiler, root))
    {
        return false;
    }

    traverser.addDeclarations(root);
    return true;
}
}  // anonymous namespace

bool EmulateFragColorData(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    if (compiler->getShaderType() != GL_FRAGMENT_SHADER)
    {
        return true;
    }

    // This transformation adds variable declarations after the fact and so some validation is
    // momentarily disabled.
    bool enableValidateVariableReferences = compiler->disableValidateVariableReferences();

    bool result = EmulateFragColorDataImpl(compiler, root, symbolTable);

    compiler->restoreValidateVariableReferences(enableValidateVariableReferences);
    return result && compiler->validateAST(root);
}
}  // namespace sh
