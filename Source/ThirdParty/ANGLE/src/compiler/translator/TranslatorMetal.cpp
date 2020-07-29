//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorMetal:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl.
//   It takes into account some considerations for Metal backend also.
//   The shaders are then fed into glslang to spit out SPIR-V (libANGLE-side).
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//
//   The SPIR-V will then be translated to Metal Shading Language later in Metal backend.
//

#include "compiler/translator/TranslatorMetal.h"

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/OutputVulkanGLSLForMetal.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/tree_ops/InitializeVariables.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/FindSymbolNode.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace mtl
{
/** extern */
const char kCoverageMaskEnabledConstName[] = "ANGLECoverageMaskEnabled";
}  // namespace mtl

namespace
{

constexpr ImmutableString kCoverageMaskField       = ImmutableString("coverageMask");
constexpr ImmutableString kSampleMaskWriteFuncName = ImmutableString("ANGLEWriteSampleMask");

TIntermBinary *CreateDriverUniformRef(const TVariable *driverUniforms, const char *fieldName)
{
    size_t fieldIndex =
        FindFieldIndex(driverUniforms->getType().getInterfaceBlock()->fields(), fieldName);

    TIntermSymbol *angleUniformsRef = new TIntermSymbol(driverUniforms);
    TConstantUnion *uniformIndex    = new TConstantUnion;
    uniformIndex->setIConst(static_cast<int>(fieldIndex));
    TIntermConstantUnion *indexRef =
        new TIntermConstantUnion(uniformIndex, *StaticType::GetBasic<EbtInt>());
    return new TIntermBinary(EOpIndexDirectInterfaceBlock, angleUniformsRef, indexRef);
}

// Unlike Vulkan having auto viewport flipping extension, in Metal we have to flip gl_Position.y
// manually.
// This operation performs flipping the gl_Position.y using this expression:
// gl_Position.y = gl_Position.y * negViewportScaleY
ANGLE_NO_DISCARD bool AppendVertexShaderPositionYCorrectionToMain(TCompiler *compiler,
                                                                  TIntermBlock *root,
                                                                  TSymbolTable *symbolTable,
                                                                  TIntermSwizzle *negFlipY)
{
    // Create a symbol reference to "gl_Position"
    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    // Create a swizzle to "gl_Position.y"
    TVector<int> swizzleOffsetY;
    swizzleOffsetY.push_back(1);
    TIntermSwizzle *positionY = new TIntermSwizzle(positionRef, swizzleOffsetY);

    // Create the expression "gl_Position.y * negFlipY"
    TIntermBinary *inverseY = new TIntermBinary(EOpMul, positionY->deepCopy(), negFlipY);

    // Create the assignment "gl_Position.y = gl_Position.y * negViewportScaleY
    TIntermTyped *positionYLHS = positionY->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionYLHS, inverseY);

    // Append the assignment as a statement at the end of the shader.
    return RunAtTheEndOfShader(compiler, root, assignment, symbolTable);
}

// Initialize unused varying outputs.
ANGLE_NO_DISCARD bool InitializeUnusedOutputs(TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              const InitVariableList &unusedVars)
{
    if (unusedVars.empty())
    {
        return true;
    }

    TIntermSequence *insertSequence = new TIntermSequence;

    for (const sh::ShaderVariable &var : unusedVars)
    {
        ASSERT(!var.active);
        const TIntermSymbol *symbol = FindSymbolNode(root, var.name);
        ASSERT(symbol);

        TIntermSequence *initCode = CreateInitCode(symbol, false, false, symbolTable);

        insertSequence->insert(insertSequence->end(), initCode->begin(), initCode->end());
    }

    if (insertSequence)
    {
        TIntermFunctionDefinition *main = FindMain(root);
        TIntermSequence *mainSequence   = main->getBody()->getSequence();

        // Insert init code at the start of main()
        mainSequence->insert(mainSequence->begin(), insertSequence->begin(), insertSequence->end());
    }

    return true;
}

}  // anonymous namespace

TranslatorMetal::TranslatorMetal(sh::GLenum type, ShShaderSpec spec) : TranslatorVulkan(type, spec)
{}

bool TranslatorMetal::translate(TIntermBlock *root,
                                ShCompileOptions compileOptions,
                                PerformanceDiagnostics *perfDiagnostics)
{
    TInfoSinkBase &sink = getInfoSink().obj;

    TOutputVulkanGLSL outputGLSL(sink, getArrayIndexClampingStrategy(), getHashFunction(),
                                 getNameMap(), &getSymbolTable(), getShaderType(),
                                 getShaderVersion(), getOutputType(), false, true, compileOptions);

    const TVariable *driverUniforms = nullptr;
    if (!TranslatorVulkan::translateImpl(root, compileOptions, perfDiagnostics, &driverUniforms,
                                         &outputGLSL))
    {
        return false;
    }

    if (getShaderType() == GL_VERTEX_SHADER)
    {
        auto negFlipY = getDriverUniformNegFlipYRef(driverUniforms);

        // Append gl_Position.y correction to main
        if (!AppendVertexShaderPositionYCorrectionToMain(this, root, &getSymbolTable(), negFlipY))
        {
            return false;
        }
    }
    else if (getShaderType() == GL_FRAGMENT_SHADER)
    {
        if (!insertSampleMaskWritingLogic(root, driverUniforms))
        {
            return false;
        }
    }

    // Initialize unused varying outputs to avoid spirv-cross dead-code removing them in later
    // stage. Only do this if SH_INIT_OUTPUT_VARIABLES is not specified.
    if ((getShaderType() == GL_VERTEX_SHADER || getShaderType() == GL_GEOMETRY_SHADER_EXT) &&
        !(compileOptions & SH_INIT_OUTPUT_VARIABLES))
    {
        InitVariableList list;
        for (const sh::ShaderVariable &var : mOutputVaryings)
        {
            if (!var.active)
            {
                list.push_back(var);
            }
        }

        if (!InitializeUnusedOutputs(root, &getSymbolTable(), list))
        {
            return false;
        }
    }

    // Write translated shader.
    root->traverse(&outputGLSL);

    return true;
}

// Metal needs to inverse the depth if depthRange is is reverse order, i.e. depth near > depth far
// This is achieved by multiply the depth value with scale value stored in
// driver uniform's depthRange.reserved
bool TranslatorMetal::transformDepthBeforeCorrection(TIntermBlock *root,
                                                     const TVariable *driverUniforms)
{
    // Create a symbol reference to "gl_Position"
    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    // Create a swizzle to "gl_Position.z"
    TVector<int> swizzleOffsetZ = {2};
    TIntermSwizzle *positionZ   = new TIntermSwizzle(positionRef, swizzleOffsetZ);

    // Create a ref to "depthRange.reserved"
    TIntermBinary *viewportZScale = getDriverUniformDepthRangeReservedFieldRef(driverUniforms);

    // Create the expression "gl_Position.z * depthRange.reserved".
    TIntermBinary *zScale = new TIntermBinary(EOpMul, positionZ->deepCopy(), viewportZScale);

    // Create the assignment "gl_Position.z = gl_Position.z * depthRange.reserved"
    TIntermTyped *positionZLHS = positionZ->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionZLHS, zScale);

    // Append the assignment as a statement at the end of the shader.
    return RunAtTheEndOfShader(this, root, assignment, &getSymbolTable());
}

void TranslatorMetal::createAdditionalGraphicsDriverUniformFields(std::vector<TField *> *fieldsOut)
{
    // Add coverage mask to driver uniform. Metal doesn't have built-in GL_SAMPLE_COVERAGE_VALUE
    // equivalent functionality, needs to emulate it using fragment shader's [[sample_mask]] output
    // value.
    TField *coverageMaskField =
        new TField(new TType(EbtUInt), kCoverageMaskField, TSourceLoc(), SymbolType::AngleInternal);
    fieldsOut->push_back(coverageMaskField);
}

// Add sample_mask writing to main, guarded by the specialization constant
// kCoverageMaskEnabledConstName
ANGLE_NO_DISCARD bool TranslatorMetal::insertSampleMaskWritingLogic(TIntermBlock *root,
                                                                    const TVariable *driverUniforms)
{
    TInfoSinkBase &sink       = getInfoSink().obj;
    TSymbolTable *symbolTable = &getSymbolTable();

    // Insert coverageMaskEnabled specialization constant and sample_mask writing function.
    sink << "layout (constant_id=0) const bool " << mtl::kCoverageMaskEnabledConstName;
    sink << " = false;\n";
    sink << "void " << kSampleMaskWriteFuncName << "(uint mask)\n";
    sink << "{\n";
    sink << "   if (" << mtl::kCoverageMaskEnabledConstName << ")\n";
    sink << "   {\n";
    sink << "       gl_SampleMask[0] = int(mask);\n";
    sink << "   }\n";
    sink << "}\n";

    // Create kCoverageMaskEnabledConstName and kSampleMaskWriteFuncName variable references.
    TType *boolType = new TType(EbtBool);
    boolType->setQualifier(EvqConst);
    TVariable *coverageMaskEnabledVar =
        new TVariable(symbolTable, ImmutableString(mtl::kCoverageMaskEnabledConstName), boolType,
                      SymbolType::AngleInternal);

    TFunction *sampleMaskWriteFunc =
        new TFunction(symbolTable, kSampleMaskWriteFuncName, SymbolType::AngleInternal,
                      StaticType::GetBasic<EbtVoid>(), false);

    TType *uintType = new TType(EbtUInt);
    TVariable *maskArg =
        new TVariable(symbolTable, ImmutableString("mask"), uintType, SymbolType::AngleInternal);
    sampleMaskWriteFunc->addParameter(maskArg);

    // coverageMask
    TIntermBinary *coverageMask = CreateDriverUniformRef(driverUniforms, kCoverageMaskField.data());

    // Insert this code to the end of main()
    // if (ANGLECoverageMaskEnabled)
    // {
    //      ANGLEWriteSampleMask(ANGLEUniforms.coverageMask);
    // }
    TIntermSequence *args = new TIntermSequence;
    args->push_back(coverageMask);
    TIntermAggregate *callSampleMaskWriteFunc =
        TIntermAggregate::CreateFunctionCall(*sampleMaskWriteFunc, args);
    TIntermBlock *callBlock = new TIntermBlock;
    callBlock->appendStatement(callSampleMaskWriteFunc);

    TIntermSymbol *coverageMaskEnabled = new TIntermSymbol(coverageMaskEnabledVar);
    TIntermIfElse *ifCall              = new TIntermIfElse(coverageMaskEnabled, callBlock, nullptr);

    return RunAtTheEndOfShader(this, root, ifCall, symbolTable);
}

}  // namespace sh
