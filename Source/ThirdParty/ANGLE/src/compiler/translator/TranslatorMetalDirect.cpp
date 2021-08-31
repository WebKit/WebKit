//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorMetalDirect.h"

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/BuiltinsWorkaroundGLSL.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/OutputVulkanGLSL.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/TranslatorMetalDirect/AddExplicitTypeCasts.h"
#include "compiler/translator/TranslatorMetalDirect/AstHelpers.h"
#include "compiler/translator/TranslatorMetalDirect/ConstantNames.h"
#include "compiler/translator/TranslatorMetalDirect/EmitMetal.h"
#include "compiler/translator/TranslatorMetalDirect/HoistConstants.h"
#include "compiler/translator/TranslatorMetalDirect/Name.h"
#include "compiler/translator/TranslatorMetalDirect/ReduceInterfaceBlocks.h"
#include "compiler/translator/TranslatorMetalDirect/RewriteCaseDeclarations.h"
#include "compiler/translator/TranslatorMetalDirect/RewriteKeywords.h"
#include "compiler/translator/TranslatorMetalDirect/RewriteOutArgs.h"
#include "compiler/translator/TranslatorMetalDirect/RewritePipelines.h"
#include "compiler/translator/TranslatorMetalDirect/RewriteUnaddressableReferences.h"
#include "compiler/translator/TranslatorMetalDirect/SeparateCompoundExpressions.h"
#include "compiler/translator/TranslatorMetalDirect/SeparateCompoundStructDeclarations.h"
#include "compiler/translator/TranslatorMetalDirect/FixTypeConstructors.h"
#include "compiler/translator/TranslatorMetalDirect/SymbolEnv.h"
#include "compiler/translator/TranslatorMetalDirect/ToposortStructs.h"
#include "compiler/translator/TranslatorMetalDirect/WrapMain.h"
#include "compiler/translator/TranslatorMetalDirect/IntroduceVertexIndexID.h"
#include "compiler/translator/TranslatorMetalUtils.h"
#include "compiler/translator/tree_ops/InitializeVariables.h"
#include "compiler/translator/TranslatorMetalDirect/NameEmbeddedUniformStructsMetal.h"
#include "compiler/translator/tree_ops/NameNamelessUniformBuffers.h"
#include "compiler/translator/tree_ops/RemoveAtomicCounterBuiltins.h"
#include "compiler/translator/tree_ops/RemoveInactiveInterfaceVariables.h"
#include "compiler/translator/tree_ops/RewriteAtomicCounters.h"
#include "compiler/translator/tree_ops/RewriteCubeMapSamplersAs2DArray.h"
#include "compiler/translator/tree_ops/RewriteDfdy.h"
#include "compiler/translator/tree_ops/RewriteStructSamplers.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/FindFunction.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/FindSymbolNode.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceClipDistanceVariable.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"
#include "compiler/translator/tree_ops/RewriteRowMajorMatrices.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

constexpr Name kCoverageMaskField("coverageMask", SymbolType::UserDefined);
constexpr Name kEmuInstanceIDField("emulatedInstanceID", SymbolType::UserDefined);
constexpr Name kSampleMaskWriteFuncName("writeSampleMask", SymbolType::AngleInternal);
#if 0
constexpr Name kDiscardWrapperFuncName("DiscardWrapper");
#endif
constexpr Name kEmulatedDepthRangeParams("DepthRangeParams");
constexpr Name kUniformsBlockName("AngleUniforms");
constexpr Name kUniformsVarName(kUniformsVar);
constexpr Name kFlippedPointCoordName("flippedPointCoord", SymbolType::UserDefined);
constexpr Name kFlippedFragCoordName("flippedFragCoord", SymbolType::UserDefined);

constexpr const TVariable kgl_VertexIDMetal(
    BuiltInId::gl_VertexID,
    ImmutableString("gl_VertexID"),
    SymbolType::BuiltIn,
    TExtension::UNDEFINED,
    StaticType::Get<EbtUInt, EbpHigh, EvqVertexID, 1, 1>());

// Keep this list in sync with ContextMtl::DriverUniforms.
constexpr size_t kNumGraphicsDriverUniforms                                                = 10;
constexpr std::array<const char *, kNumGraphicsDriverUniforms> kGraphicsDriverUniformNames = {
    {kViewport, kHalfRenderArea, kFlipXY, kNegFlipXY, kClipDistancesEnabled, kXfbActiveUnpaused,
     kXfbVerticesPerDraw, kXfbBufferOffsets, kAcbBufferOffsets, kDepthRange}};

constexpr size_t kNumComputeDriverUniforms                                               = 1;
constexpr std::array<const char *, kNumComputeDriverUniforms> kComputeDriverUniformNames = {
    {kAcbBufferOffsets}};

class DeclareStructTypesTraverser : public TIntermTraverser
{
  public:
    explicit DeclareStructTypesTraverser(TOutputMSL *outputMSL)
        : TIntermTraverser(true, false, false), mOutputMSL(outputMSL)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        ASSERT(visit == PreVisit);
        if (!mInGlobalScope)
        {
            return false;
        }

        const TIntermSequence &sequence = *(node->getSequence());
        TIntermTyped *declarator        = sequence.front()->getAsTyped();
        const TType &type               = declarator->getType();

        if (type.isStructSpecifier())
        {
            const TStructure *structure = type.getStruct();

            // Embedded structs should be parsed away by now.
            ASSERT(structure->symbolType() != SymbolType::Empty);
            // outputMSL->writeStructType(structure);

            TIntermSymbol *symbolNode = declarator->getAsSymbolNode();
            if (symbolNode && symbolNode->variable().symbolType() == SymbolType::Empty)
            {
                // Remove the struct specifier declaration from the tree so it isn't parsed again.
                TIntermSequence emptyReplacement;
                mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                                emptyReplacement);
            }
        }
        // TODO: REMOVE, used to remove 'unsued' warning
        mOutputMSL = nullptr;

        return false;
    }

  private:
    TOutputMSL *mOutputMSL;
};

class DeclareDefaultUniformsTraverser : public TIntermTraverser
{
  public:
    DeclareDefaultUniformsTraverser(TInfoSinkBase *sink,
                                    ShHashFunction64 hashFunction,
                                    NameMap *nameMap)
        : TIntermTraverser(true, true, true),
          mSink(sink),
          mHashFunction(hashFunction),
          mNameMap(nameMap),
          mInDefaultUniform(false)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        // TODO(jmadill): Compound declarations.
        ASSERT(sequence.size() == 1);

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();
        bool isUniform         = type.getQualifier() == EvqUniform && !type.isInterfaceBlock() &&
                         !IsOpaqueType(type.getBasicType());

        if (visit == PreVisit)
        {
            if (isUniform)
            {
                (*mSink) << "    " << GetTypeName(type, mHashFunction, mNameMap) << " ";
                mInDefaultUniform = true;
            }
        }
        else if (visit == InVisit)
        {
            mInDefaultUniform = isUniform;
        }
        else if (visit == PostVisit)
        {
            if (isUniform)
            {
                (*mSink) << ";\n";

                // Remove the uniform declaration from the tree so it isn't parsed again.
                TIntermSequence emptyReplacement;
                mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                                emptyReplacement);
            }

            mInDefaultUniform = false;
        }
        return true;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        if (mInDefaultUniform)
        {
            const ImmutableString &name = symbol->variable().name();
            ASSERT(!name.beginsWith("gl_"));
            (*mSink) << HashName(&symbol->variable(), mHashFunction, mNameMap)
                     << ArrayString(symbol->getType());
        }
    }

  private:
    TInfoSinkBase *mSink;
    ShHashFunction64 mHashFunction;
    NameMap *mNameMap;
    bool mInDefaultUniform;
};

TIntermBinary *CreateDriverUniformRef(const TVariable &driverUniforms, const char *fieldName)
{
    size_t fieldIndex = FindFieldIndex(driverUniforms.getType().getStruct()->fields(), fieldName);

    TIntermSymbol *angleUniformsRef = new TIntermSymbol(&driverUniforms);
    TConstantUnion *uniformIndex    = new TConstantUnion;
    uniformIndex->setIConst(static_cast<int>(fieldIndex));
    TIntermConstantUnion *indexRef =
        new TIntermConstantUnion(uniformIndex, *StaticType::GetBasic<EbtInt>());
    return new TIntermBinary(EOpIndexDirectStruct, angleUniformsRef, indexRef);
}

#if 0
// Unlike Vulkan having auto viewport flipping extension, in Metal we have to flip gl_Position.y
// manually.
// This operation performs flipping the gl_Position.y using this expression:
// gl_Position.y = gl_Position.y * negViewportScaleY
ANGLE_NO_DISCARD bool AppendVertexShaderPositionYCorrectionToMain(TCompiler &compiler,
                                                                  TIntermBlock &root,
                                                                  TIntermSwizzle &negFlipY)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();

    // Create a symbol reference to "gl_Position"
    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    // Create a swizzle to "gl_Position.y"
    TVector<int> swizzleOffsetY;
    swizzleOffsetY.push_back(1);
    TIntermSwizzle *positionY = new TIntermSwizzle(positionRef, swizzleOffsetY);

    // Create the expression "gl_Position.y * negFlipY"
    TIntermBinary *inverseY = new TIntermBinary(EOpMul, positionY->deepCopy(), &negFlipY);

    // Create the assignment "gl_Position.y = gl_Position.y * negViewportScaleY
    TIntermTyped *positionYLHS = positionY->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionYLHS, inverseY);

    // Append the assignment as a statement at the end of the shader.
    return RunAtTheEndOfShader(&compiler, &root, assignment, &symbolTable);
}
#endif

#if 0
// Initialize unused varying outputs.
ANGLE_NO_DISCARD bool InitializeUnusedOutputs(TIntermBlock &root,
                                              TSymbolTable &symbolTable,
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
        const TIntermSymbol *symbol = FindSymbolNode(&root, var.name);
        ASSERT(symbol);

        TIntermSequence *initCode = CreateInitCode(symbol, false, false, &symbolTable);

        insertSequence->insert(insertSequence->end(), initCode->begin(), initCode->end());
    }

    if (insertSequence)
    {
        TIntermFunctionDefinition *main = FindMain(&root);
        TIntermSequence *mainSequence   = main->getBody()->getSequence();

        // Insert init code at the start of main()
        mainSequence->insert(mainSequence->begin(), insertSequence->begin(), insertSequence->end());
    }

    return true;
}
#endif

const TVariable *AddComputeDriverUniformsToShader(TIntermBlock &root, TSymbolTable &symbolTable)
{
    // This field list mirrors the structure of ComputeDriverUniforms in ContextVk.cpp.
    TFieldList *driverFieldList = new TFieldList;

    const std::array<TType *, kNumComputeDriverUniforms> kDriverUniformTypes = {{
        new TType(EbtUInt, 4),
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumComputeDriverUniforms; ++uniformIndex)
    {
        TField *driverUniformField =
            new TField(kDriverUniformTypes[uniformIndex],
                       ImmutableString(kComputeDriverUniformNames[uniformIndex]), kNoSourceLoc,
                       SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    return DeclareStructure(&root, &symbolTable, driverFieldList, EvqUniform,
                            TMemoryQualifier::Create(), 0, kUniformsBlockName.rawName(),
                            &kUniformsVarName.rawName())
        .second;
}

// The Add*DriverUniformsToShader operation adds an internal uniform block to a shader. The driver
// block is used to implement Vulkan-specific features and workarounds. Returns the driver uniforms
// variable.
//
// There are Graphics and Compute variations as they require different uniforms.
const TVariable *AddGraphicsDriverUniformsToShader(TIntermBlock &root,
                                                   TSymbolTable &symbolTable,
                                                   const std::vector<TField *> &additionalFields)
{
    // Init the depth range type.
    TFieldList *depthRangeParamsFields = new TFieldList();
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("near"), kNoSourceLoc,
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("far"), kNoSourceLoc,
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("diff"), kNoSourceLoc,
                                                 SymbolType::AngleInternal));
    // This additional field might be used by subclass such as TranslatorMetal.
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("reserved"), kNoSourceLoc,
                                                 SymbolType::AngleInternal));

    const TStructure *emulatedDepthRangeParams =
        DeclareStructure(&root, &symbolTable, depthRangeParamsFields, EvqGlobal,
                         TMemoryQualifier::Create(), 0, kEmulatedDepthRangeParams.rawName(),
                         nullptr)
            .first->getType()
            .getStruct();

    // This field list mirrors the structure of GraphicsDriverUniforms in ContextMtl.cpp.
    TFieldList *driverFieldList = new TFieldList;

    const std::array<TType *, kNumGraphicsDriverUniforms> kDriverUniformTypes = {{
        new TType(EbtFloat, 4),
        new TType(EbtFloat, 2),
        new TType(EbtFloat, 2),
        new TType(EbtFloat, 2),
        new TType(EbtUInt),  // uint clipDistancesEnabled;  // 32 bits for 32 clip distances max
        new TType(EbtUInt),
        new TType(EbtUInt),
        // NOTE: There's a vec3 gap here that can be used in the future
        new TType(EbtInt, 4),
        new TType(EbtUInt, 4),
        new TType(emulatedDepthRangeParams, false),
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumGraphicsDriverUniforms; ++uniformIndex)
    {
        TField *driverUniformField =
            new TField(kDriverUniformTypes[uniformIndex],
                       ImmutableString(kGraphicsDriverUniformNames[uniformIndex]), kNoSourceLoc,
                       SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    // Back-end specific fields
    driverFieldList->insert(driverFieldList->end(), additionalFields.begin(),
                            additionalFields.end());

    // Define a driver uniform block "ANGLEUniformBlock" with instance name "ANGLEUniforms".
    return DeclareStructure(&root, &symbolTable, driverFieldList, EvqUniform,
                            TMemoryQualifier::Create(), 0, kUniformsBlockName.rawName(),
                            &kUniformsVarName.rawName())
        .second;
}

// Declares a new variable to replace gl_DepthRange, its values are fed from a driver uniform.
ANGLE_NO_DISCARD bool ReplaceGLDepthRangeWithDriverUniform(TCompiler &compiler,
                                                           TIntermBlock &root,
                                                           const TVariable &driverUniforms)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();

    // Create a symbol reference to "gl_DepthRange"
    const TVariable *depthRangeVar = static_cast<const TVariable *>(
        symbolTable.findBuiltIn(ImmutableString("gl_DepthRange"), 0));

    // ANGLEUniforms.depthRange
    TIntermBinary *angleEmulatedDepthRangeRef = CreateDriverUniformRef(driverUniforms, kDepthRange);

    // Use this variable instead of gl_DepthRange everywhere.
    return ReplaceVariableWithTyped(&compiler, &root, depthRangeVar, angleEmulatedDepthRangeRef);
}

TIntermSequence *GetMainSequence(TIntermBlock &root)
{
    TIntermFunctionDefinition *main = FindMain(&root);
    return main->getBody()->getSequence();
}

ANGLE_NO_DISCARD bool AppendVertexShaderTransformFeedbackOutputToMain(TCompiler &compiler,
                                                                      SymbolEnv &mSymbolEnv,
                                                                      TIntermBlock &root)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();

    // Append the assignment as a statement at the end of the shader.
    return RunAtTheEndOfShader(&compiler, &root, &(mSymbolEnv.callFunctionOverload(Name("@@XFB-OUT@@"), *new TType(), *new TIntermSequence())), &symbolTable);
}

// This operation performs the viewport depth translation needed by Vulkan. In GL the viewport
// transformation is slightly different - see the GL 2.0 spec section "2.12.1 Controlling the
// Viewport". In Vulkan the corresponding spec section is currently "23.4. Coordinate
// Transformations".
// The equations reduce to an expression:
//
//     z_vk = 0.5 * (w_gl + z_gl)
//
// where z_vk is the depth output of a Vulkan vertex shader and z_gl is the same for GL.
ANGLE_NO_DISCARD bool AppendVertexShaderDepthCorrectionToMain(TCompiler &compiler,
                                                              TIntermBlock &root)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();

    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    TVector<int> swizzleOffsetZ = {2};
    TIntermSwizzle *positionZ   = new TIntermSwizzle(positionRef, swizzleOffsetZ);

    TIntermConstantUnion *oneHalf = CreateFloatNode(0.5f);

    TVector<int> swizzleOffsetW = {3};
    TIntermSwizzle *positionW   = new TIntermSwizzle(positionRef->deepCopy(), swizzleOffsetW);

    // Create the expression "(gl_Position.z + gl_Position.w) * 0.5".
    TIntermBinary *zPlusW = new TIntermBinary(EOpAdd, positionZ->deepCopy(), positionW->deepCopy());
    TIntermBinary *halfZPlusW = new TIntermBinary(EOpMul, zPlusW, oneHalf->deepCopy());

    // Create the assignment "gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5"
    TIntermTyped *positionZLHS = positionZ->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionZLHS, halfZPlusW);

    // Append the assignment as a statement at the end of the shader.
    return RunAtTheEndOfShader(&compiler, &root, assignment, &symbolTable);
}

ANGLE_NO_DISCARD bool RotateAndFlipBuiltinVariable(TCompiler &compiler,
                                                   TIntermBlock &root,
                                                   TIntermSequence &insertSequence,
                                                   TIntermTyped &flipXY,
                                                   const TVariable &builtin,
                                                   const Name &flippedVariableName,
                                                   TIntermTyped &pivot,
                                                   TIntermTyped *fragRotation)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();

    // Create a symbol reference to 'builtin'.
    TIntermSymbol *builtinRef = new TIntermSymbol(&builtin);

    // Create a swizzle to "builtin.xy"
    TVector<int> swizzleOffsetXY = {0, 1};
    TIntermSwizzle *builtinXY    = new TIntermSwizzle(builtinRef, swizzleOffsetXY);

    // Create a symbol reference to our new variable that will hold the modified builtin.
    const TType *type = StaticType::GetForVec<EbtFloat>(
        EvqGlobal, static_cast<unsigned char>(builtin.getType().getNominalSize()));
    TVariable *replacementVar = new TVariable(&symbolTable, flippedVariableName.rawName(), type,
                                              flippedVariableName.symbolType());
    DeclareGlobalVariable(&root, replacementVar);
    TIntermSymbol *flippedBuiltinRef = new TIntermSymbol(replacementVar);

    // Use this new variable instead of 'builtin' everywhere.
    if (!ReplaceVariable(&compiler, &root, &builtin, replacementVar))
    {
        return false;
    }

    // Create the expression "(builtin.xy * fragRotation)"
    TIntermTyped *rotatedXY;
    if (fragRotation)
    {
        rotatedXY = new TIntermBinary(EOpMatrixTimesVector, fragRotation->deepCopy(),
                                      builtinXY->deepCopy());
    }
    else
    {
        // No rotation applied, use original variable.
        rotatedXY = builtinXY->deepCopy();
    }

    // Create the expression "(builtin.xy - pivot) * flipXY + pivot
    TIntermBinary *removePivot = new TIntermBinary(EOpSub, rotatedXY, &pivot);
    TIntermBinary *inverseXY   = new TIntermBinary(EOpMul, removePivot, &flipXY);
    TIntermBinary *plusPivot   = new TIntermBinary(EOpAdd, inverseXY, pivot.deepCopy());

    // Create the corrected variable and copy the value of the original builtin.
    TIntermSequence *sequence = new TIntermSequence();
    sequence->push_back(builtinRef->deepCopy());
    TIntermAggregate *aggregate = TIntermAggregate::CreateConstructor(builtin.getType(), sequence);
    TIntermBinary *assignment   = new TIntermBinary(EOpInitialize, flippedBuiltinRef, aggregate);

    // Create an assignment to the replaced variable's .xy.
    TIntermSwizzle *correctedXY =
        new TIntermSwizzle(flippedBuiltinRef->deepCopy(), swizzleOffsetXY);
    TIntermBinary *assignToY = new TIntermBinary(EOpAssign, correctedXY, plusPivot);

    // Add this assigment at the beginning of the main function
    {
        TIntermBinary *nodes[] = {assignment, assignToY};
        insertSequence.insert(insertSequence.begin(), std::begin(nodes), std::end(nodes));
    }

    return compiler.validateAST(&root);
}

ANGLE_NO_DISCARD bool InsertFragCoordCorrection(TCompiler &compiler,
                                                ShCompileOptions compileOptions,
                                                TIntermBlock &root,
                                                TIntermSequence &insertSequence,
                                                const TVariable &driverUniforms)
{
    TIntermBinary &flipXY       = *CreateDriverUniformRef(driverUniforms, kFlipXY);
    TIntermBinary &pivot        = *CreateDriverUniformRef(driverUniforms, kHalfRenderArea);
    TIntermBinary *fragRotation = nullptr;
    return RotateAndFlipBuiltinVariable(compiler, root, insertSequence, flipXY,
                                        *BuiltInVariable::gl_FragCoord(), kFlippedFragCoordName,
                                        pivot, fragRotation);
}

TIntermBinary *GetDriverUniformDepthRangeReservedFieldRef(const TVariable &driverUniforms)
{
    TIntermBinary *depthRange = CreateDriverUniformRef(driverUniforms, kDepthRange);

    return new TIntermBinary(EOpIndexDirectStruct, depthRange, CreateIndexNode(3));
}

void DeclareRightBeforeMain(TIntermBlock &root, const TVariable &var)
{
    root.insertChildNodes(FindMainIndex(&root), {new TIntermDeclaration{&var}});
}

void AddFragColorDeclaration(TIntermBlock &root, TSymbolTable &symbolTable)
{
    root.insertChildNodes(FindMainIndex(&root),
                          TIntermSequence{new TIntermDeclaration{BuiltInVariable::gl_FragColor()}});
}

void AddFragDepthDeclaration(TIntermBlock &root, TSymbolTable &symbolTable)
{
    root.insertChildNodes(FindMainIndex(&root),
                          TIntermSequence{new TIntermDeclaration{BuiltInVariable::gl_FragDepth()}});
}

void AddFragDepthEXTDeclaration(TCompiler &compiler, TIntermBlock &root, TSymbolTable &symbolTable)
{
    const TIntermSymbol *glFragDepthExt = FindSymbolNode(&root, ImmutableString("gl_FragDepthEXT"));
    ASSERT(glFragDepthExt);
    // Replace gl_FragData with our globally defined fragdata.
    if (!ReplaceVariable(&compiler, &root, &(glFragDepthExt->variable()), BuiltInVariable::gl_FragDepth()))
    {
        return;
    }
    AddFragDepthDeclaration(root, symbolTable);
}
void AddSampleMaskDeclaration(TIntermBlock &root, TSymbolTable &symbolTable)
{
    TType *gl_SampleMaskType    = new TType(EbtUInt, EbpHigh, EvqSampleMask, 1, 1);
    const TVariable *gl_SampleMask =
        new TVariable(&symbolTable, ImmutableString("gl_SampleMask"), gl_SampleMaskType, SymbolType::BuiltIn,
                      TExtension::UNDEFINED);
    root.insertChildNodes(FindMainIndex(&root), TIntermSequence{new TIntermDeclaration{gl_SampleMask}});
}

ANGLE_NO_DISCARD bool AddFragDataDeclaration(TCompiler &compiler, TIntermBlock &root)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();
    const int maxDrawBuffers  = compiler.getResources().MaxDrawBuffers;
    TType *gl_FragDataType    = new TType(EbtFloat, EbpMedium, EvqFragData, 4, 1);
    std::vector<const TVariable *> glFragDataSlots;
    TIntermSequence declareGLFragdataSequence;

    // Create gl_FragData_0,1,2,3
    for (int i = 0; i < maxDrawBuffers; i++)
    {
        ImmutableStringBuilder builder(strlen("gl_FragData_") + 2);
        builder << "gl_FragData_";
        builder.appendDecimal(i);
        const TVariable *glFragData =
            new TVariable(&symbolTable, builder, gl_FragDataType, SymbolType::AngleInternal,
                          TExtension::UNDEFINED);
        glFragDataSlots.push_back(glFragData);
        declareGLFragdataSequence.push_back(new TIntermDeclaration{glFragData});
    }
    root.insertChildNodes(FindMainIndex(&root), declareGLFragdataSequence);

    // Create an internal gl_FragData array type, compatible with indexing syntax.
    TType *gl_FragDataTypeArray = new TType(EbtFloat, EbpMedium, EvqGlobal, 4, 1);
    gl_FragDataTypeArray->makeArray(maxDrawBuffers);
    const TVariable *glFragDataGlobal = new TVariable(&symbolTable, ImmutableString("gl_FragData"),
                                                      gl_FragDataTypeArray, SymbolType::BuiltIn);

    DeclareGlobalVariable(&root, glFragDataGlobal);
    const TIntermSymbol *originalGLFragData = FindSymbolNode(&root, ImmutableString("gl_FragData"));
    ASSERT(originalGLFragData);

    // Replace gl_FragData() with our globally defined fragdata
    if (!ReplaceVariable(&compiler, &root, &(originalGLFragData->variable()), glFragDataGlobal))
    {
        return false;
    }

    // Assign each array attribute to an output
    TIntermBlock *insertSequence = new TIntermBlock();
    for (int i = 0; i < maxDrawBuffers; i++)
    {
        TIntermTyped *glFragDataSlot         = new TIntermSymbol(glFragDataSlots[i]);
        TIntermTyped *glFragDataGlobalSymbol = new TIntermSymbol(glFragDataGlobal);
        auto &access                         = AccessIndex(*glFragDataGlobalSymbol, i);
        TIntermBinary *assignment =
            new TIntermBinary(TOperator::EOpAssign, glFragDataSlot, &access);
        insertSequence->appendStatement(assignment);
    }
    return RunAtTheEndOfShader(&compiler, &root, insertSequence, &symbolTable);
}

ANGLE_NO_DISCARD bool EmulateInstanceID(TCompiler &compiler,
                                        TIntermBlock &root,
                                        const TVariable &driverUniforms)
{
    TIntermBinary *emuInstanceID =
        CreateDriverUniformRef(driverUniforms, kEmuInstanceIDField.rawName().data());
    const TVariable *instanceID = BuiltInVariable::gl_InstanceIndex();
    return ReplaceVariableWithTyped(&compiler, &root, instanceID, emuInstanceID);
}

// Unlike Vulkan having auto viewport flipping extension, in Metal we have to flip gl_Position.y
// manually.
// This operation performs flipping the gl_Position.y using this expression:
// gl_Position.y = gl_Position.y * negViewportScaleY
ANGLE_NO_DISCARD bool AppendVertexShaderPositionYCorrectionToMain(TCompiler &compiler,
                                                                  TIntermBlock &root,
                                                                  TIntermSwizzle &negFlipY)
{
    TSymbolTable &symbolTable = compiler.getSymbolTable();

    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    TVector<int> swizzleOffsetY;
    swizzleOffsetY.push_back(1);
    TIntermSwizzle *positionY = new TIntermSwizzle(positionRef, swizzleOffsetY);

    // Create the expression "gl_Position.y * negFlipY"
    TIntermBinary *inverseY = new TIntermBinary(EOpMul, positionY->deepCopy(), &negFlipY);

    // Create the assignment "gl_Position.y = gl_Position.y * negViewportScaleY
    TIntermTyped *positionYLHS = positionY->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionYLHS, inverseY);

    // Append the assignment as a statement at the end of the shader.
    return RunAtTheEndOfShader(&compiler, &root, assignment, &symbolTable);
}
}  // namespace

TranslatorMetalDirect::TranslatorMetalDirect(sh::GLenum type,
                                             ShShaderSpec spec,
                                             ShShaderOutput output)
    : TCompiler(type, spec, output)
{}

// static
const char *TranslatorMetalDirect::GetCoverageMaskEnabledConstName()
{
    return constant_names::kCoverageMaskEnabled.rawName().data();
}

// static
const char *TranslatorMetalDirect::GetRasterizationDiscardEnabledConstName()
{
    return constant_names::kRasterizationDiscardEnabled.rawName().data();
}

// Add sample_mask writing to main, guarded by the function constant
// kCoverageMaskEnabledName
ANGLE_NO_DISCARD bool TranslatorMetalDirect::insertSampleMaskWritingLogic(TIntermBlock &root,
                                                                          const TVariable &driverUniforms)
{
    TSymbolTable *symbolTable = &getSymbolTable();

    // Create kCoverageMaskEnabled and kSampleMaskWriteFuncName variable references.
    TType *boolType = new TType(EbtBool);
    boolType->setQualifier(EvqConst);
    TVariable *coverageMaskEnabledVar =
        new TVariable(symbolTable, constant_names::kCoverageMaskEnabled.rawName(), boolType,
                      SymbolType::AngleInternal);

    TFunction *sampleMaskWriteFunc =
        new TFunction(symbolTable, kSampleMaskWriteFuncName.rawName(), kSampleMaskWriteFuncName.symbolType(),
                      StaticType::GetBasic<EbtVoid>(), false);

    TType *uintType = new TType(EbtUInt);
    TVariable *maskArg =
        new TVariable(symbolTable, ImmutableString("mask"), uintType, SymbolType::AngleInternal);
    sampleMaskWriteFunc->addParameter(maskArg);

    TVariable *gl_SampleMaskArg =
        new TVariable(symbolTable, ImmutableString("gl_SampleMask"), uintType, SymbolType::AngleInternal);
    sampleMaskWriteFunc->addParameter(gl_SampleMaskArg);

    // Insert this code to the end of main()
    // if (ANGLE_CoverageMaskEnabled)
    // {
    //      ANGLE_writeSampleMask(ANGLE_angleUniforms.coverageMask, ANGLE_fragmentOut.gl_SampleMask);
    // }
    TIntermSequence *args = new TIntermSequence;
    TIntermBinary *coverageMask = CreateDriverUniformRef(driverUniforms, kCoverageMaskField.rawName().data());
    args->push_back(coverageMask);
    const TIntermSymbol *gl_SampleMask = FindSymbolNode(&root, ImmutableString("gl_SampleMask"));
    args->push_back(gl_SampleMask->deepCopy());

    TIntermAggregate *callSampleMaskWriteFunc =
        TIntermAggregate::CreateFunctionCall(*sampleMaskWriteFunc, args);
    TIntermBlock *callBlock = new TIntermBlock;
    callBlock->appendStatement(callSampleMaskWriteFunc);

    TIntermSymbol *coverageMaskEnabled = new TIntermSymbol(coverageMaskEnabledVar);
    TIntermIfElse *ifCall              = new TIntermIfElse(coverageMaskEnabled, callBlock, nullptr);

    return RunAtTheEndOfShader(this, &root, ifCall, symbolTable);
}

ANGLE_NO_DISCARD bool TranslatorMetalDirect::insertRasterizationDiscardLogic(TIntermBlock &root)
{
    TSymbolTable *symbolTable = &getSymbolTable();

    TType *boolType = new TType(EbtBool);
    boolType->setQualifier(EvqConst);
    TVariable *discardEnabledVar =
        new TVariable(symbolTable, constant_names::kRasterizationDiscardEnabled.rawName(), boolType,
                      constant_names::kRasterizationDiscardEnabled.symbolType());

    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    // Create vec4(-3, -3, -3, 1):
    auto vec4Type             = new TType(EbtFloat, 4);
    TIntermSequence *vec4Args = new TIntermSequence();
    vec4Args->push_back(CreateFloatNode(-3.0f));
    vec4Args->push_back(CreateFloatNode(-3.0f));
    vec4Args->push_back(CreateFloatNode(-3.0f));
    vec4Args->push_back(CreateFloatNode(1.0f));
    TIntermAggregate *constVarConstructor =
        TIntermAggregate::CreateConstructor(*vec4Type, vec4Args);

    // Create the assignment "gl_Position = vec4(-3, -3, -3, 1)"
    TIntermBinary *assignment =
        new TIntermBinary(TOperator::EOpAssign, positionRef->deepCopy(), constVarConstructor);

    TIntermBlock *discardBlock = new TIntermBlock;
    discardBlock->appendStatement(assignment);

    TIntermSymbol *discardEnabled = new TIntermSymbol(discardEnabledVar);
    TIntermIfElse *ifCall         = new TIntermIfElse(discardEnabled, discardBlock, nullptr);

    return RunAtTheEndOfShader(this, &root, ifCall, symbolTable);
}

// Metal needs to inverse the depth if depthRange is is reverse order, i.e. depth near > depth far
// This is achieved by multiply the depth value with scale value stored in
// driver uniform's depthRange.reserved
bool TranslatorMetalDirect::transformDepthBeforeCorrection(TIntermBlock &root,
                                                           const TVariable &driverUniforms)
{
    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    TVector<int> swizzleOffsetZ = {2};
    TIntermSwizzle *positionZ   = new TIntermSwizzle(positionRef, swizzleOffsetZ);

    TIntermBinary *viewportZScale = GetDriverUniformDepthRangeReservedFieldRef(driverUniforms);

    // Create the expression "gl_Position.z * depthRange.reserved".
    TIntermBinary *zScale = new TIntermBinary(EOpMul, positionZ->deepCopy(), viewportZScale);

    // Create the assignment "gl_Position.z = gl_Position.z * depthRange.reserved"
    TIntermTyped *positionZLHS = positionZ->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionZLHS, zScale);

    return RunAtTheEndOfShader(this, &root, assignment, &getSymbolTable());
}

void TranslatorMetalDirect::createAdditionalGraphicsDriverUniformFields(
    std::vector<TField *> &fieldsOut)
{
    // Add coverage mask to driver uniform. Metal doesn't have built-in GL_SAMPLE_COVERAGE_VALUE
    // equivalent functionality, needs to emulate it using fragment shader's [[sample_mask]] output
    // value.
    TField *coverageMaskField = new TField(new TType(EbtUInt), kCoverageMaskField.rawName(),
                                           kNoSourceLoc, kCoverageMaskField.symbolType());
    fieldsOut.push_back(coverageMaskField);

    if (mEmulatedInstanceID)
    {
        TField *emuInstanceIDField = new TField(new TType(EbtInt), kEmuInstanceIDField.rawName(),
                                                kNoSourceLoc, kEmuInstanceIDField.symbolType());
        fieldsOut.push_back(emuInstanceIDField);
    }
}

TIntermSwizzle *TranslatorMetalDirect::getDriverUniformNegFlipYRef(
    const TVariable &driverUniforms) const
{
    // Create a swizzle to "negFlipXY.y"
    TIntermBinary *negFlipXY    = CreateDriverUniformRef(driverUniforms, kNegFlipXY);
    TVector<int> swizzleOffsetY = {1};
    TIntermSwizzle *negFlipY    = new TIntermSwizzle(negFlipXY, swizzleOffsetY);
    return negFlipY;
}

static std::set<ImmutableString> GetMslKeywords()
{
    std::set<ImmutableString> keywords;

    keywords.emplace("alignas");
    keywords.emplace("alignof");
    keywords.emplace("as_type");
    keywords.emplace("auto");
    keywords.emplace("catch");
    keywords.emplace("char");
    keywords.emplace("class");
    keywords.emplace("const_cast");
    keywords.emplace("constant");
    keywords.emplace("constexpr");
    keywords.emplace("decltype");
    keywords.emplace("delete");
    keywords.emplace("device");
    keywords.emplace("dynamic_cast");
    keywords.emplace("enum");
    keywords.emplace("explicit");
    keywords.emplace("export");
    keywords.emplace("extern");
    keywords.emplace("fragment");
    keywords.emplace("friend");
    keywords.emplace("goto");
    keywords.emplace("half");
    keywords.emplace("inline");
    keywords.emplace("int16_t");
    keywords.emplace("int32_t");
    keywords.emplace("int64_t");
    keywords.emplace("int8_t");
    keywords.emplace("kernel");
    keywords.emplace("long");
    keywords.emplace("main0");
    keywords.emplace("metal");
    keywords.emplace("mutable");
    keywords.emplace("namespace");
    keywords.emplace("new");
    keywords.emplace("noexcept");
    keywords.emplace("nullptr_t");
    keywords.emplace("nullptr");
    keywords.emplace("operator");
    keywords.emplace("override");
    keywords.emplace("private");
    keywords.emplace("protected");
    keywords.emplace("ptrdiff_t");
    keywords.emplace("public");
    keywords.emplace("ray_data");
    keywords.emplace("register");
    keywords.emplace("short");
    keywords.emplace("signed");
    keywords.emplace("size_t");
    keywords.emplace("sizeof");
    keywords.emplace("stage_in");
    keywords.emplace("static_assert");
    keywords.emplace("static_cast");
    keywords.emplace("static");
    keywords.emplace("template");
    keywords.emplace("this");
    keywords.emplace("thread_local");
    keywords.emplace("thread");
    keywords.emplace("threadgroup_imageblock");
    keywords.emplace("threadgroup");
    keywords.emplace("throw");
    keywords.emplace("try");
    keywords.emplace("typedef");
    keywords.emplace("typeid");
    keywords.emplace("typename");
    keywords.emplace("uchar");
    keywords.emplace("uint16_t");
    keywords.emplace("uint32_t");
    keywords.emplace("uint64_t");
    keywords.emplace("uint8_t");
    keywords.emplace("union");
    keywords.emplace("unsigned");
    keywords.emplace("ushort");
    keywords.emplace("using");
    keywords.emplace("vertex");
    keywords.emplace("virtual");
    keywords.emplace("volatile");
    keywords.emplace("wchar_t");
    keywords.emplace("NAN");
    return keywords;
}

static inline MetalShaderType metalShaderTypeFromGLSL(sh::GLenum shaderType)
{
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
            return MetalShaderType::Vertex;
        case GL_FRAGMENT_SHADER:
            return MetalShaderType::Fragment;
        case GL_COMPUTE_SHADER:
            ASSERT(0 && "compute shaders not currently supported");
            return MetalShaderType::Compute;
        default:
            ASSERT(0 && "Invalid shader type.");
            return MetalShaderType::None;
    }
}

bool TranslatorMetalDirect::translateImpl(TIntermBlock &root, ShCompileOptions compileOptions)
{
    TSymbolTable &symbolTable = getSymbolTable();
    IdGen idGen;
    ProgramPreludeConfig ppc(metalShaderTypeFromGLSL(getShaderType()));

    if (!WrapMain(*this, idGen, root))
    {
        return false;
    }

    TInfoSinkBase &sink = getInfoSink().obj;
#if 0
    if (getShaderType() == GL_VERTEX_SHADER)
    {
        if (!ShaderBuiltinsWorkaround(this, &root, &symbolTable, compileOptions))
        {
            return false;
        }
    }
#endif

    // Strip any inactive variables from the program.
    if (!RemoveInactiveInterfaceVariables(this, &root, getAttributes(), getInputVaryings(),
                                          getOutputVariables(), getUniforms(),
                                          getInterfaceBlocks()))
    {
        return false;
    }

    // Write out default uniforms into a uniform block assigned to a specific set/binding.
    int defaultUniformCount           = 0;
    int aggregateTypesUsedForUniforms = 0;
    int atomicCounterCount            = 0;
    for (const auto &uniform : getUniforms())
    {
        if (!uniform.isBuiltIn() && uniform.active && !gl::IsOpaqueType(uniform.type))
        {
            ++defaultUniformCount;
        }

        if (uniform.isStruct() || uniform.isArrayOfArrays())
        {
            ++aggregateTypesUsedForUniforms;
        }

        if (uniform.active && gl::IsAtomicCounterType(uniform.type))
        {
            ++atomicCounterCount;
        }
    }

    if (aggregateTypesUsedForUniforms > 0)
    {
        if (!NameEmbeddedStructUniformsMetal(this, &root, &symbolTable))
        {
            return false;
        }

        bool rewriteStructSamplersResult;
        int removedUniformsCount;

        if (compileOptions & SH_USE_OLD_REWRITE_STRUCT_SAMPLERS)
        {
            rewriteStructSamplersResult =
                RewriteStructSamplersOld(this, &root, &symbolTable, &removedUniformsCount);
        }
        else
        {
            rewriteStructSamplersResult =
                RewriteStructSamplers(this, &root, &symbolTable, &removedUniformsCount);
        }

        if (!rewriteStructSamplersResult)
        {
            return false;
        }
        defaultUniformCount -= removedUniformsCount;

#if 0
        // We must declare the struct types before using them.
        DeclareStructTypesTraverser structTypesTraverser(outputMSL);
        root->traverse(&structTypesTraverser);
        if (!structTypesTraverser.updateTree(this, root))
        {
            return false;
        }
#endif
    }

    if (compileOptions & SH_EMULATE_SEAMFUL_CUBE_MAP_SAMPLING)
    {
        if (!RewriteCubeMapSamplersAs2DArray(this, &root, &symbolTable,
                                             getShaderType() == GL_FRAGMENT_SHADER))
        {
            return false;
        }
    }

#if 0
    // Write default uniform block
    if (defaultUniformCount > 0)
    {
        gl::ShaderType shaderType = gl::FromGLenum<gl::ShaderType>(getShaderType());
        sink << "\nstruct " << kDefaultUniformNames[shaderType] << "\n{\n";
        DeclareDefaultUniformsTraverser defaultTraverser(&sink, getHashFunction(), &getNameMap());
        root->traverse(&defaultTraverser);
        if (!defaultTraverser.updateTree(this, root))
        {
            return false;
        }

        sink << "};\n";
    }
#endif

    const TVariable &driverUniforms = *[&]() {
        if (getShaderType() == GL_COMPUTE_SHADER)
        {
            return AddComputeDriverUniformsToShader(root, symbolTable);
        }
        else
        {
            std::vector<TField *> additionalFields;
            createAdditionalGraphicsDriverUniformFields(additionalFields);
            return AddGraphicsDriverUniformsToShader(root, symbolTable, additionalFields);
        }
    }();

    if (atomicCounterCount > 0)
    {
        // ANGLEUniforms.acbBufferOffsets
        const TIntermBinary *acbBufferOffsets =
            CreateDriverUniformRef(driverUniforms, kAcbBufferOffsets);

        if (!RewriteAtomicCounters(this, &root, &symbolTable, acbBufferOffsets))
        {
            return false;
        }
    }
    else if (getShaderVersion() >= 310)
    {
        // Vulkan doesn't support Atomic Storage as a Storage Class, but we've seen
        // cases where builtins are using it even with no active atomic counters.
        // This pass simply removes those builtins in that scenario.
        if (!RemoveAtomicCounterBuiltins(this, &root))
        {
            return false;
        }
    }

    if (getShaderType() != GL_COMPUTE_SHADER)
    {
        if (!ReplaceGLDepthRangeWithDriverUniform(*this, root, driverUniforms))
        {
            return false;
        }

#if 0
        // Add specialization constant declarations.  The default value of the specialization
        // constant is irrelevant, as it will be set when creating the pipeline.
        if (compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION)
        {
            sink << "layout(constant_id="
                 << static_cast<uint32_t>(vk::SpecializationConstantId::LineRasterEmulation)
                 << ") const bool " << kLineRasterEmulationSpecConstVarName << " = false;\n\n";
        }
#endif
    }

    {
        bool usesInstanceId = false;
        bool usesVertexId = false;
        for (const ShaderVariable &var : mAttributes)
        {
            if (var.isBuiltIn())
            {
                if (var.name == "gl_InstanceID")
                {
                    usesInstanceId = true;
                }
                if (var.name == "gl_VertexID")
                {
                    usesVertexId = true;
                }
            }
        }

        if (usesInstanceId)
        {
            root.insertChildNodes(
                FindMainIndex(&root),
                TIntermSequence{new TIntermDeclaration{BuiltInVariable::gl_InstanceID()}});
        }
        if (usesVertexId)
        {
            if (!ReplaceVariable(this, &root, BuiltInVariable::gl_VertexID(),
                                 &kgl_VertexIDMetal))
            {
                return false;
            }
            DeclareRightBeforeMain(root, kgl_VertexIDMetal);
        }
    }
    SymbolEnv symbolEnv(*this, root);
    // Declare gl_FragColor and glFragData as webgl_FragColor and webgl_FragData
    // if it's core profile shaders and they are used.
    if (getShaderType() == GL_FRAGMENT_SHADER)
    {
        bool usesPointCoord  = false;
        bool usesFragCoord   = false;
        bool usesFrontFacing = false;
        for (const ShaderVariable &inputVarying : mInputVaryings)
        {
            if (inputVarying.isBuiltIn())
            {
                if (inputVarying.name == "gl_PointCoord")
                {
                    usesPointCoord = true;
                }
                else if (inputVarying.name == "gl_FragCoord")
                {
                    usesFragCoord = true;
                }
                else if (inputVarying.name == "gl_FrontFacing")
                {
                    usesFrontFacing = true;
                }
            }
        }

        bool usesFragColor  = false;
        bool usesFragData   = false;
        bool usesFragDepth  = false;
        bool usesFragDepthEXT = false;
        for (const ShaderVariable &outputVarying : mOutputVariables)
        {
            if (outputVarying.isBuiltIn())
            {
                if (outputVarying.name == "gl_FragColor")
                {
                    usesFragColor = true;
                }
                else if (outputVarying.name == "gl_FragData")
                {
                    usesFragData = true;
                }
                else if (outputVarying.name == "gl_FragDepth")
                {
                    usesFragDepth = true;
                }
                else if (outputVarying.name == "gl_FragDepthEXT")
                {
                    usesFragDepthEXT = true;
                }
            }
        }

        if (usesFragColor && usesFragData)
        {
            return false;
        }

        if (usesFragColor)
        {
            AddFragColorDeclaration(root, symbolTable);
        }

        if (usesFragData)
        {
            if (!AddFragDataDeclaration(*this, root))
            {
                return false;
            }
        }
        if (usesFragDepth)
        {
            AddFragDepthDeclaration(root, symbolTable);
        }
        else if(usesFragDepthEXT)
        {
            AddFragDepthEXTDeclaration(*this, root, symbolTable);
        }
        
        // Always add sample_mask. It will be guarded by a function constant decided at runtime.
        bool usesSampleMask = true;
        if (usesSampleMask)
        {
            AddSampleMaskDeclaration(root, symbolTable);
        }

#if 0
        if (compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION)
        {
            if (!AddBresenhamEmulationFS(this, compileOptions, sink, root, &symbolTable,
                                         driverUniforms, usesFragCoord))
            {
                return false;
            }
        }
#endif

        if (usesPointCoord)
        {
            TIntermBinary &flipXY       = *CreateDriverUniformRef(driverUniforms, kNegFlipXY);
            TIntermConstantUnion &pivot = *CreateFloatNode(0.5f);
            TIntermBinary *fragRotation = nullptr;
            if (!RotateAndFlipBuiltinVariable(*this, root, *GetMainSequence(root), flipXY,
                                              *BuiltInVariable::gl_PointCoord(),
                                              kFlippedPointCoordName, pivot, fragRotation))
            {
                return false;
            }
            DeclareRightBeforeMain(root, *BuiltInVariable::gl_PointCoord());
        }

        if (usesFragCoord)
        {
            if (!InsertFragCoordCorrection(*this, compileOptions, root, *GetMainSequence(root),
                                           driverUniforms))
            {
                return false;
            }
            DeclareRightBeforeMain(root, *BuiltInVariable::gl_FragCoord());
        }

        {
            TIntermBinary *flipXY = CreateDriverUniformRef(driverUniforms, kFlipXY);
            TIntermBinary *fragRotation = nullptr;
            if (!RewriteDfdy(this, &root, symbolTable, getShaderVersion(), flipXY, fragRotation))
            {
                return false;
            }
        }

        if (usesFrontFacing)
        {
            DeclareRightBeforeMain(root, *BuiltInVariable::gl_FrontFacing());
        }

        EmitEarlyFragmentTestsGLSL(*this, sink);
    }
    else if (getShaderType() == GL_VERTEX_SHADER)
    {
        DeclareRightBeforeMain(root, *BuiltInVariable::gl_Position());

        if (FindSymbolNode(&root, BuiltInVariable::gl_PointSize()->name()))
        {
            DeclareRightBeforeMain(root, *BuiltInVariable::gl_PointSize());
        }

        if (FindSymbolNode(&root, BuiltInVariable::gl_VertexIndex()->name()))
        {
            if (!ReplaceVariable(this, &root, BuiltInVariable::gl_VertexIndex(),
                                 &kgl_VertexIDMetal))
            {
                return false;
            }
            DeclareRightBeforeMain(root, kgl_VertexIDMetal);
        }
#if 0
        if (compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION)
        {
            if (!AddBresenhamEmulationVS(this, root, &symbolTable, driverUniforms))
            {
                return false;
            }
        }
#endif
        // Append a macro for transform feedback substitution prior to modifying depth.
        if (!AppendVertexShaderTransformFeedbackOutputToMain(*this, symbolEnv, root))
        {
            return false;
        }

        // Search for the gl_ClipDistance usage, if its used, we need to do some replacements.
        bool useClipDistance = false;
        for (const ShaderVariable &outputVarying : mOutputVaryings)
        {
            if (outputVarying.name == "gl_ClipDistance")
            {
                useClipDistance = true;
                break;
            }
        }

        if (useClipDistance && !ReplaceClipDistanceAssignments(
                                   this, &root, &symbolTable,
                                   CreateDriverUniformRef(driverUniforms, kClipDistancesEnabled)))
        {
            return false;
        }

        if (!transformDepthBeforeCorrection(root, driverUniforms))
        {
            return false;
        }

        if (!AppendVertexShaderDepthCorrectionToMain(*this, root))
        {
            return false;
        }
    }
    else if (getShaderType() == GL_GEOMETRY_SHADER)
    {
        WriteGeometryShaderLayoutQualifiers(
            sink, getGeometryShaderInputPrimitiveType(), getGeometryShaderInvocations(),
            getGeometryShaderOutputPrimitiveType(), getGeometryShaderMaxVertices());
    }
    else
    {
        ASSERT(getShaderType() == GL_COMPUTE_SHADER);
        EmitWorkGroupSizeGLSL(*this, sink);
    }

    if (getShaderType() == GL_VERTEX_SHADER)
    {
        auto &negFlipY = *getDriverUniformNegFlipYRef(driverUniforms);

        if (mEmulatedInstanceID)
        {
            if (!EmulateInstanceID(*this, root, driverUniforms))
            {
                return false;
            }
        }

        if (!AppendVertexShaderPositionYCorrectionToMain(*this, root, negFlipY))
        {
            return false;
        }

        if (!insertRasterizationDiscardLogic(root))
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



    if (!validateAST(&root))
    {
        return false;
    }

    if (!RewriteKeywords(*this, root, idGen, GetMslKeywords()))
    {
        return false;
    }

    if (!ReduceInterfaceBlocks(*this, root, idGen))
    {
        return false;
    }

    if (!SeparateCompoundStructDeclarations(*this, idGen, root))
    {
        return false;
    }
    // This is the largest size required to pass all the tests in
    // (dEQP-GLES3.functional.shaders.large_constant_arrays)
    // This value could in principle be smaller.
    const size_t hoistThresholdSize = 256;
    if (!HoistConstants(*this, root, idGen, hoistThresholdSize))
    {
        return false;
    }

    const bool needsExplicitBoolCasts = (compileOptions & SH_ADD_EXPLICIT_BOOL_CASTS) != 0;
    if (!AddExplicitTypeCasts(*this, root, symbolEnv, needsExplicitBoolCasts))
    {
        return false;
    }

    PipelineStructs pipelineStructs;
    if (!RewritePipelines(*this, root, getInputVaryings(), getOutputVaryings(), idGen,
                          driverUniforms, symbolEnv, pipelineStructs))
    {
        return false;
    }
    if (getShaderType() == GL_VERTEX_SHADER)
    {
        if (!IntroduceVertexAndInstanceIndex(*this, root))
        {
            return false;
        }
    }
    if (!SeparateCompoundExpressions(*this, symbolEnv, idGen, root))
    {
        return false;
    }

    if (!RewriteCaseDeclarations(*this, root))
    {
        return false;
    }

    if (!RewriteUnaddressableReferences(*this, root, symbolEnv))
    {
        return false;
    }

    if (!RewriteOutArgs(*this, root, symbolEnv))
    {
        return false;
    }
    if(!FixTypeConstructors(*this, symbolEnv, root))
    {
        return false;
    }
    if (!ToposortStructs(*this, symbolEnv, root, ppc))
    {
        return false;
    }
    if (!EmitMetal(*this, root, idGen, pipelineStructs, symbolEnv, ppc))
    {
        return false;
    }

    ASSERT(validateAST(&root));

    return true;
}

bool TranslatorMetalDirect::translate(TIntermBlock *root,
                                      ShCompileOptions compileOptions,
                                      PerformanceDiagnostics *perfDiagnostics)
{
    if (!root)
    {
        return false;
    }

    TInfoSinkBase &sink = getInfoSink().obj;

    if ((compileOptions & SH_REWRITE_ROW_MAJOR_MATRICES) != 0 && getShaderVersion() >= 300)
    {
        // "Make sure every uniform buffer variable has a name.  The following transformation relies on this."
        // This pass was removed in e196bc85ac2dda0e9f6664cfc2eca0029e33d2d1, but currently finding it still necessary for MSL.
        // TODO(jcunningham): Look into removing the NameNamelessUniformBuffers and fixing the root cause in RewriteRowMajorMatrices
        if (!NameNamelessUniformBuffers(this, root, &getSymbolTable()))
        {
            return false;
        }
        if (!RewriteRowMajorMatrices(this, root, &getSymbolTable()))
        {
            return false;
        }
    }

    bool precisionEmulation = false;
    if (!emulatePrecisionIfNeeded(root, sink, &precisionEmulation, SH_GLSL_VULKAN_OUTPUT))
    {
        return false;
    }

#if 0
    bool enablePrecision = ((compileOptions & SH_IGNORE_PRECISION_QUALIFIERS) == 0);

    TOutputMSL outputMSL(sink, getArrayIndexClampingStrategy(), getHashFunction(), getNameMap(),
                         &getSymbolTable(), getShaderType(), getShaderVersion(), getOutputType(),
                         precisionEmulation, enablePrecision, compileOptions);
#endif

    if (!translateImpl(*root, compileOptions))
    {
        return false;
    }

#if 0
    // Write translated shader.
    root->traverse(outputMSL);
#endif

    return true;
}
bool TranslatorMetalDirect::shouldFlattenPragmaStdglInvariantAll()
{
    // Not neccesary for MSL transformation.
    return false;
}

}  // namespace sh
