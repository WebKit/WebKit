// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "compiler/translator/tree_ops/glsl/ExpandFragmentOutputsToVec4.h"

#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"

namespace sh
{

namespace
{
void CopyFromGlobal(TIntermSequence *seq,
                    TIntermTyped *global,
                    TIntermTyped *output,
                    const TVector<uint32_t> componentsFromGlobal,
                    TIntermTyped *zero)
{
    // The output takes its components from the original variable (now called "global"), with the
    // rest of the components set to zero.  For example, if global has 2 components, the following
    // is generated:
    //
    //     output.xy = global;
    //     output.z = zero;
    //     output.w = zero;
    seq->push_back(
        new TIntermBinary(EOpAssign, new TIntermSwizzle(output, componentsFromGlobal), global));
    for (uint32_t component = static_cast<uint32_t>(componentsFromGlobal.size()); component < 4;
         ++component)
    {
        seq->push_back(new TIntermBinary(
            EOpAssign, new TIntermSwizzle(output->deepCopy(), {component}), zero->deepCopy()));
    }
}

bool ExpandToVec4(TSymbolTable *symbolTable,
                  TIntermSymbol *symbol,
                  TIntermSequence *declarations,
                  VariableReplacementMap *replacedOutputs,
                  TIntermSequence *postMain)
{
    const TVariable &variable = symbol->variable();
    const TType &type         = variable.getType();

    const uint8_t componentCount = type.getNominalSize();
    if (type.getQualifier() != EvqFragmentOut || componentCount == 4)
    {
        return false;
    }

    // Create a global of the same type to replace the variable
    TVariable *globalVariable      = CreateTempVariable(symbolTable, &type, EvqGlobal);
    TIntermSymbol *global          = new TIntermSymbol(globalVariable);
    TIntermDeclaration *globalDecl = new TIntermDeclaration();
    globalDecl->appendDeclarator(global);
    declarations->push_back(globalDecl);

    (*replacedOutputs)[variable.uniqueId()] = global;

    // Create a fragment output of vec4 size
    TType *expandedType = new TType(type);
    expandedType->setPrimarySize(4);
    TVariable *expandedVariable      = new TVariable(symbolTable, variable.name(), expandedType,
                                                     variable.symbolType(), variable.extensions());
    TIntermSymbol *expanded          = new TIntermSymbol(expandedVariable);
    TIntermDeclaration *expandedDecl = new TIntermDeclaration();
    expandedDecl->appendDeclarator(expanded);
    declarations->push_back(expandedDecl);

    // After main, copy from the global variable into the new output variable and set missing
    // channels to 0 except alpha.  The spec does not specify whether alpha should be 0 or 1, which
    // matters for blending, and implementations are inconsistent.  We set it to 0 for simplicity.
    TVector<uint32_t> swizzle = {0, 1, 2, 3};
    swizzle.resize(componentCount);
    TIntermTyped *zero = CreateZeroNode(TType(type.getBasicType()));
    if (type.isArray())
    {
        for (uint32_t i = 0; i < type.getOutermostArraySize(); ++i)
        {
            TIntermTyped *globalElement =
                new TIntermBinary(EOpIndexDirect, global->deepCopy(), CreateIndexNode(i));
            TIntermTyped *expandedElement =
                new TIntermBinary(EOpIndexDirect, expanded->deepCopy(), CreateIndexNode(i));
            CopyFromGlobal(postMain, globalElement, expandedElement, swizzle, zero);
        }
    }
    else
    {
        CopyFromGlobal(postMain, global->deepCopy(), expanded->deepCopy(), swizzle, zero);
    }

    return true;
}
}  // anonymous namespace

[[nodiscard]] bool ExpandFragmentOutputsToVec4(TCompiler *compiler,
                                               TIntermBlock *root,
                                               TSymbolTable *symbolTable)
{
    // Note: MoveDeclarationsBeforeFunctions() must have been called already, so that when an output
    // variable is replaced, it can be referenced in main().
    VariableReplacementMap replacedOutputs;
    TIntermSequence *original = root->getSequence();
    TIntermSequence replacement;

    TIntermSequence postMain;

    // Replace any non-vec4 output with a global variable of the same type, and add a vec4 output in
    // its stead.
    for (TIntermNode *node : *original)
    {
        TIntermDeclaration *decl = node->getAsDeclarationNode();
        if (decl != nullptr)
        {
            // SeparateDeclarations must already be run
            const TIntermSequence &sequence = *(decl->getSequence());
            ASSERT(sequence.size() == 1);
            TIntermTyped *variable  = sequence.front()->getAsTyped();
            TIntermSymbol *asSymbol = variable->getAsSymbolNode();
            if (asSymbol &&
                ExpandToVec4(symbolTable, asSymbol, &replacement, &replacedOutputs, &postMain))
            {
                continue;
            }
        }

        replacement.push_back(node);
    }

    // Replace root's sequence with |replacement|.
    root->replaceAllChildren(std::move(replacement));

    // Replace the variables throughout the shader.
    if (!replacedOutputs.empty() && !ReplaceVariables(compiler, root, replacedOutputs))
    {
        return false;
    }

    // Run the global->output expansion code at the end of main
    if (!postMain.empty())
    {
        return RunAtTheEndOfShader(compiler, root, new TIntermBlock(std::move(postMain)),
                                   symbolTable);
    }
    return true;
}

}  // namespace sh
