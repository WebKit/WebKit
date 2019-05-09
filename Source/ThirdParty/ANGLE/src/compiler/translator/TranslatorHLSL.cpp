//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorHLSL.h"

#include "compiler/translator/OutputHLSL.h"
#include "compiler/translator/tree_ops/AddDefaultReturnStatements.h"
#include "compiler/translator/tree_ops/ArrayReturnValueToOutParameter.h"
#include "compiler/translator/tree_ops/BreakVariableAliasingInInnerLoops.h"
#include "compiler/translator/tree_ops/EmulatePrecision.h"
#include "compiler/translator/tree_ops/ExpandIntegerPowExpressions.h"
#include "compiler/translator/tree_ops/PruneEmptyCases.h"
#include "compiler/translator/tree_ops/RemoveDynamicIndexing.h"
#include "compiler/translator/tree_ops/RewriteAtomicFunctionExpressions.h"
#include "compiler/translator/tree_ops/RewriteElseBlocks.h"
#include "compiler/translator/tree_ops/RewriteExpressionsWithShaderStorageBlock.h"
#include "compiler/translator/tree_ops/RewriteTexelFetchOffset.h"
#include "compiler/translator/tree_ops/RewriteUnaryMinusOperatorInt.h"
#include "compiler/translator/tree_ops/SeparateArrayConstructorStatements.h"
#include "compiler/translator/tree_ops/SeparateArrayInitialization.h"
#include "compiler/translator/tree_ops/SeparateDeclarations.h"
#include "compiler/translator/tree_ops/SeparateExpressionsReturningArrays.h"
#include "compiler/translator/tree_ops/SimplifyLoopConditions.h"
#include "compiler/translator/tree_ops/SplitSequenceOperator.h"
#include "compiler/translator/tree_ops/UnfoldShortCircuitToIf.h"
#include "compiler/translator/tree_ops/WrapSwitchStatementsInBlocks.h"
#include "compiler/translator/tree_util/IntermNodePatternMatcher.h"

namespace sh
{

TranslatorHLSL::TranslatorHLSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : TCompiler(type, spec, output)
{}

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
                           &getSymbolTable());

    SplitSequenceOperator(root,
                          IntermNodePatternMatcher::kExpressionReturningArray |
                              IntermNodePatternMatcher::kUnfoldedShortCircuitExpression |
                              IntermNodePatternMatcher::kDynamicIndexingOfVectorOrMatrixInLValue,
                          &getSymbolTable());

    // Note that SeparateDeclarations needs to be run before UnfoldShortCircuitToIf.
    UnfoldShortCircuitToIf(root, &getSymbolTable());

    SeparateArrayConstructorStatements(root);

    SeparateExpressionsReturningArrays(root, &getSymbolTable());

    // Note that SeparateDeclarations needs to be run before SeparateArrayInitialization.
    SeparateArrayInitialization(root);

    // HLSL doesn't support arrays as return values, we'll need to make functions that have an array
    // as a return value to use an out parameter to transfer the array data instead.
    ArrayReturnValueToOutParameter(root, &getSymbolTable());

    if (!shouldRunLoopAndIndexingValidation(compileOptions))
    {
        // HLSL doesn't support dynamic indexing of vectors and matrices.
        RemoveDynamicIndexing(root, &getSymbolTable(), perfDiagnostics);
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
    // introduce variable declarations inside the main scope of any switch statement. It cannot
    // result in no-op cases at the end of switch statements, because unreferenced variables
    // have already been pruned.
    WrapSwitchStatementsInBlocks(root);

    bool precisionEmulation =
        getResources().WEBGL_debug_shader_precision && getPragma().debugShaderPrecision;

    if (precisionEmulation)
    {
        EmulatePrecision emulatePrecision(&getSymbolTable());
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

    if (getShaderVersion() >= 310)
    {
        // Due to ssbo also can be used as the argument of atomic memory functions, we should put
        // RewriteExpressionsWithShaderStorageBlock before RewriteAtomicFunctionExpressions.
        sh::RewriteExpressionsWithShaderStorageBlock(root, &getSymbolTable());
        sh::RewriteAtomicFunctionExpressions(root, &getSymbolTable(), getShaderVersion());
    }

    sh::OutputHLSL outputHLSL(getShaderType(), getShaderVersion(), getExtensionBehavior(),
                              getSourcePath(), getOutputType(), numRenderTargets, getUniforms(),
                              compileOptions, getComputeShaderLocalSize(), &getSymbolTable(),
                              perfDiagnostics, mShaderStorageBlocks);

    outputHLSL.output(root, getInfoSink().obj);

    mShaderStorageBlockRegisterMap = outputHLSL.getShaderStorageBlockRegisterMap();
    mUniformBlockRegisterMap       = outputHLSL.getUniformBlockRegisterMap();
    mUniformRegisterMap            = outputHLSL.getUniformRegisterMap();
    mReadonlyImage2DRegisterIndex  = outputHLSL.getReadonlyImage2DRegisterIndex();
    mImage2DRegisterIndex          = outputHLSL.getImage2DRegisterIndex();
    mUsedImage2DFunctionNames      = outputHLSL.getUsedImage2DFunctionNames();
}

bool TranslatorHLSL::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary when translating to HLSL.
    return false;
}

bool TranslatorHLSL::hasShaderStorageBlock(const std::string &uniformBlockName) const
{
    return (mShaderStorageBlockRegisterMap.count(uniformBlockName) > 0);
}

unsigned int TranslatorHLSL::getShaderStorageBlockRegister(
    const std::string &shaderStorageBlockName) const
{
    ASSERT(hasShaderStorageBlock(shaderStorageBlockName));
    return mShaderStorageBlockRegisterMap.find(shaderStorageBlockName)->second;
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

unsigned int TranslatorHLSL::getReadonlyImage2DRegisterIndex() const
{
    return mReadonlyImage2DRegisterIndex;
}

unsigned int TranslatorHLSL::getImage2DRegisterIndex() const
{
    return mImage2DRegisterIndex;
}

const std::set<std::string> *TranslatorHLSL::getUsedImage2DFunctionNames() const
{
    return &mUsedImage2DFunctionNames;
}

}  // namespace sh
