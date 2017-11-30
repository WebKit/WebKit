//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorHLSL.h"

#include "compiler/translator/AddDefaultReturnStatements.h"
#include "compiler/translator/ArrayReturnValueToOutParameter.h"
#include "compiler/translator/BreakVariableAliasingInInnerLoops.h"
#include "compiler/translator/EmulatePrecision.h"
#include "compiler/translator/ExpandIntegerPowExpressions.h"
#include "compiler/translator/IntermNodePatternMatcher.h"
#include "compiler/translator/OutputHLSL.h"
#include "compiler/translator/RemoveDynamicIndexing.h"
#include "compiler/translator/RemoveNoOpCasesFromEndOfSwitchStatements.h"
#include "compiler/translator/RewriteElseBlocks.h"
#include "compiler/translator/RewriteTexelFetchOffset.h"
#include "compiler/translator/RewriteUnaryMinusOperatorInt.h"
#include "compiler/translator/SeparateArrayInitialization.h"
#include "compiler/translator/SeparateDeclarations.h"
#include "compiler/translator/SeparateExpressionsReturningArrays.h"
#include "compiler/translator/SimplifyLoopConditions.h"
#include "compiler/translator/SplitSequenceOperator.h"
#include "compiler/translator/UnfoldShortCircuitToIf.h"
#include "compiler/translator/WrapSwitchStatementsInBlocks.h"

namespace sh
{

TranslatorHLSL::TranslatorHLSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : TCompiler(type, spec, output)
{
}

void TranslatorHLSL::translate(TIntermBlock *root,
                               ShCompileOptions compileOptions,
                               PerformanceDiagnostics *perfDiagnostics)
{
    const ShBuiltInResources &resources = getResources();
    int numRenderTargets                = resources.EXT_draw_buffers ? resources.MaxDrawBuffers : 1;

    sh::AddDefaultReturnStatements(root);

    // Note that SimplifyLoopConditions needs to be run before any other AST transformations that
    // may need to generate new statements from loop conditions or loop expressions.
    // Note that SeparateDeclarations has already been run in TCompiler::compileTreeImpl().
    SimplifyLoopConditions(root,
                           IntermNodePatternMatcher::kExpressionReturningArray |
                               IntermNodePatternMatcher::kUnfoldedShortCircuitExpression |
                               IntermNodePatternMatcher::kDynamicIndexingOfVectorOrMatrixInLValue,
                           &getSymbolTable(), getShaderVersion());

    SplitSequenceOperator(root,
                          IntermNodePatternMatcher::kExpressionReturningArray |
                              IntermNodePatternMatcher::kUnfoldedShortCircuitExpression |
                              IntermNodePatternMatcher::kDynamicIndexingOfVectorOrMatrixInLValue,
                          &getSymbolTable(), getShaderVersion());

    // Note that SeparateDeclarations needs to be run before UnfoldShortCircuitToIf.
    UnfoldShortCircuitToIf(root, &getSymbolTable());

    SeparateExpressionsReturningArrays(root, &getSymbolTable());

    // Note that SeparateDeclarations needs to be run before SeparateArrayInitialization.
    SeparateArrayInitialization(root);

    // HLSL doesn't support arrays as return values, we'll need to make functions that have an array
    // as a return value to use an out parameter to transfer the array data instead.
    ArrayReturnValueToOutParameter(root, &getSymbolTable());

    if (!shouldRunLoopAndIndexingValidation(compileOptions))
    {
        // HLSL doesn't support dynamic indexing of vectors and matrices.
        RemoveDynamicIndexing(root, &getSymbolTable(), getShaderVersion(), perfDiagnostics);
    }

    // Work around D3D9 bug that would manifest in vertex shaders with selection blocks which
    // use a vertex attribute as a condition, and some related computation in the else block.
    if (getOutputType() == SH_HLSL_3_0_OUTPUT && getShaderType() == GL_VERTEX_SHADER)
    {
        sh::RewriteElseBlocks(root, &getSymbolTable());
    }

    // Work around an HLSL compiler frontend aliasing optimization bug.
    // TODO(cwallez) The date is 2016-08-25, Microsoft said the bug would be fixed
    // in the next release of d3dcompiler.dll, it would be nice to detect the DLL
    // version and only apply the workaround if it is too old.
    sh::BreakVariableAliasingInInnerLoops(root);

    // WrapSwitchStatementsInBlocks should be called after any AST transformations that might
    // introduce variable declarations inside the main scope of any switch statement.
    if (WrapSwitchStatementsInBlocks(root))
    {
        // The WrapSwitchStatementsInBlocks step might introduce new no-op cases to the end of
        // switch statements, so make sure to clean up the AST.
        RemoveNoOpCasesFromEndOfSwitchStatements(root, &getSymbolTable());
    }

    bool precisionEmulation =
        getResources().WEBGL_debug_shader_precision && getPragma().debugShaderPrecision;

    if (precisionEmulation)
    {
        EmulatePrecision emulatePrecision(&getSymbolTable(), getShaderVersion());
        root->traverse(&emulatePrecision);
        emulatePrecision.updateTree();
        emulatePrecision.writeEmulationHelpers(getInfoSink().obj, getShaderVersion(),
                                               getOutputType());
    }

    if ((compileOptions & SH_EXPAND_SELECT_HLSL_INTEGER_POW_EXPRESSIONS) != 0)
    {
        sh::ExpandIntegerPowExpressions(root, &getSymbolTable());
    }

    if ((compileOptions & SH_REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH) != 0)
    {
        sh::RewriteTexelFetchOffset(root, getSymbolTable(), getShaderVersion());
    }

    if (((compileOptions & SH_REWRITE_INTEGER_UNARY_MINUS_OPERATOR) != 0) &&
        getShaderType() == GL_VERTEX_SHADER)
    {
        sh::RewriteUnaryMinusOperatorInt(root);
    }

    sh::OutputHLSL outputHLSL(getShaderType(), getShaderVersion(), getExtensionBehavior(),
                              getSourcePath(), getOutputType(), numRenderTargets, getUniforms(),
                              compileOptions, &getSymbolTable(), perfDiagnostics);

    outputHLSL.output(root, getInfoSink().obj);

    mUniformBlockRegisterMap   = outputHLSL.getUniformBlockRegisterMap();
    mUniformRegisterMap        = outputHLSL.getUniformRegisterMap();
}

bool TranslatorHLSL::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary when translating to HLSL.
    return false;
}

bool TranslatorHLSL::hasUniformBlock(const std::string &uniformBlockName) const
{
    return (mUniformBlockRegisterMap.count(uniformBlockName) > 0);
}

unsigned int TranslatorHLSL::getUniformBlockRegister(const std::string &uniformBlockName) const
{
    ASSERT(hasUniformBlock(uniformBlockName));
    return mUniformBlockRegisterMap.find(uniformBlockName)->second;
}

const std::map<std::string, unsigned int> *TranslatorHLSL::getUniformRegisterMap() const
{
    return &mUniformRegisterMap;
}

}  // namespace sh
