//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl and feeds them into
//   glslang to spit out SPIR-V.
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/TranslatorVulkan.h"

#include "angle_gl.h"
#include "common/PackedEnums.h"
#include "common/utilities.h"
#include "compiler/translator/BuiltinsWorkaroundGLSL.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/OutputSPIRV.h"
#include "compiler/translator/OutputVulkanGLSL.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/glslang_wrapper.h"
#include "compiler/translator/tree_ops/MonomorphizeUnsupportedFunctions.h"
#include "compiler/translator/tree_ops/RecordConstantPrecision.h"
#include "compiler/translator/tree_ops/RemoveAtomicCounterBuiltins.h"
#include "compiler/translator/tree_ops/RemoveInactiveInterfaceVariables.h"
#include "compiler/translator/tree_ops/RewriteArrayOfArrayOfOpaqueUniforms.h"
#include "compiler/translator/tree_ops/RewriteAtomicCounters.h"
#include "compiler/translator/tree_ops/RewriteCubeMapSamplersAs2DArray.h"
#include "compiler/translator/tree_ops/RewriteDfdy.h"
#include "compiler/translator/tree_ops/RewriteStructSamplers.h"
#include "compiler/translator/tree_ops/SeparateStructFromUniformDeclarations.h"
#include "compiler/translator/tree_ops/vulkan/DeclarePerVertexBlocks.h"
#include "compiler/translator/tree_ops/vulkan/EmulateDithering.h"
#include "compiler/translator/tree_ops/vulkan/EmulateFragColorData.h"
#include "compiler/translator/tree_ops/vulkan/FlagSamplersWithTexelFetch.h"
#include "compiler/translator/tree_ops/vulkan/ReplaceForShaderFramebufferFetch.h"
#include "compiler/translator/tree_ops/vulkan/RewriteInterpolateAtOffset.h"
#include "compiler/translator/tree_ops/vulkan/RewriteR32fImages.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/FindFunction.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceClipCullDistanceVariable.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RewriteSampleMaskVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"
#include "compiler/translator/tree_util/SpecializationConstant.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{
constexpr ImmutableString kFlippedPointCoordName = ImmutableString("flippedPointCoord");
constexpr ImmutableString kFlippedFragCoordName  = ImmutableString("flippedFragCoord");

constexpr gl::ShaderMap<const char *> kDefaultUniformNames = {
    {gl::ShaderType::Vertex, vk::kDefaultUniformsNameVS},
    {gl::ShaderType::TessControl, vk::kDefaultUniformsNameTCS},
    {gl::ShaderType::TessEvaluation, vk::kDefaultUniformsNameTES},
    {gl::ShaderType::Geometry, vk::kDefaultUniformsNameGS},
    {gl::ShaderType::Fragment, vk::kDefaultUniformsNameFS},
    {gl::ShaderType::Compute, vk::kDefaultUniformsNameCS},
};

bool IsDefaultUniform(const TType &type)
{
    return type.getQualifier() == EvqUniform && type.getInterfaceBlock() == nullptr &&
           !IsOpaqueType(type.getBasicType());
}

class ReplaceDefaultUniformsTraverser : public TIntermTraverser
{
  public:
    ReplaceDefaultUniformsTraverser(const VariableReplacementMap &variableMap)
        : TIntermTraverser(true, false, false), mVariableMap(variableMap)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();

        if (IsDefaultUniform(type))
        {
            // Remove the uniform declaration.
            TIntermSequence emptyReplacement;
            mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                            std::move(emptyReplacement));

            return false;
        }

        return true;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TVariable &variable = symbol->variable();
        const TType &type         = variable.getType();

        if (!IsDefaultUniform(type) || gl::IsBuiltInName(variable.name().data()))
        {
            return;
        }

        ASSERT(mVariableMap.count(&variable) > 0);

        queueReplacement(mVariableMap.at(&variable)->deepCopy(), OriginalNode::IS_DROPPED);
    }

  private:
    const VariableReplacementMap &mVariableMap;
};

bool DeclareDefaultUniforms(TCompiler *compiler,
                            TIntermBlock *root,
                            TSymbolTable *symbolTable,
                            gl::ShaderType shaderType)
{
    // First, collect all default uniforms and declare a uniform block.
    TFieldList *uniformList = new TFieldList;
    TVector<const TVariable *> uniformVars;

    for (TIntermNode *node : *root->getSequence())
    {
        TIntermDeclaration *decl = node->getAsDeclarationNode();
        if (decl == nullptr)
        {
            continue;
        }

        const TIntermSequence &sequence = *(decl->getSequence());

        TIntermSymbol *symbol = sequence.front()->getAsSymbolNode();
        if (symbol == nullptr)
        {
            continue;
        }

        const TType &type = symbol->getType();
        if (IsDefaultUniform(type))
        {
            TType *fieldType = new TType(type);

            uniformList->push_back(new TField(fieldType, symbol->getName(), symbol->getLine(),
                                              symbol->variable().symbolType()));
            uniformVars.push_back(&symbol->variable());
        }
    }

    TLayoutQualifier layoutQualifier = TLayoutQualifier::Create();
    layoutQualifier.blockStorage     = EbsStd140;
    const TVariable *uniformBlock    = DeclareInterfaceBlock(
        root, symbolTable, uniformList, EvqUniform, layoutQualifier, TMemoryQualifier::Create(), 0,
        ImmutableString(kDefaultUniformNames[shaderType]), ImmutableString(""));

    // Create a map from the uniform variables to new variables that reference the fields of the
    // block.
    VariableReplacementMap variableMap;
    for (size_t fieldIndex = 0; fieldIndex < uniformVars.size(); ++fieldIndex)
    {
        const TVariable *variable = uniformVars[fieldIndex];

        TType *replacementType = new TType(variable->getType());
        replacementType->setInterfaceBlockField(uniformBlock->getType().getInterfaceBlock(),
                                                fieldIndex);

        TVariable *replacementVariable =
            new TVariable(symbolTable, variable->name(), replacementType, variable->symbolType());

        variableMap[variable] = new TIntermSymbol(replacementVariable);
    }

    // Finally transform the AST and make sure references to the uniforms are replaced with the new
    // variables.
    ReplaceDefaultUniformsTraverser defaultTraverser(variableMap);
    root->traverse(&defaultTraverser);
    return defaultTraverser.updateTree(compiler, root);
}

// Replaces a builtin variable with a version that is rotated and corrects the X and Y coordinates.
ANGLE_NO_DISCARD bool RotateAndFlipBuiltinVariable(TCompiler *compiler,
                                                   TIntermBlock *root,
                                                   TIntermSequence *insertSequence,
                                                   TIntermTyped *flipXY,
                                                   TSymbolTable *symbolTable,
                                                   const TVariable *builtin,
                                                   const ImmutableString &flippedVariableName,
                                                   TIntermTyped *pivot,
                                                   TIntermTyped *fragRotation)
{
    // Create a symbol reference to 'builtin'.
    TIntermSymbol *builtinRef = new TIntermSymbol(builtin);

    // Create a swizzle to "builtin.xy"
    TVector<int> swizzleOffsetXY = {0, 1};
    TIntermSwizzle *builtinXY    = new TIntermSwizzle(builtinRef, swizzleOffsetXY);

    // Create a symbol reference to our new variable that will hold the modified builtin.
    TType *type = new TType(builtin->getType());
    type->setQualifier(EvqGlobal);
    type->setPrimarySize(builtin->getType().getNominalSize());
    TVariable *replacementVar =
        new TVariable(symbolTable, flippedVariableName, type, SymbolType::AngleInternal);
    DeclareGlobalVariable(root, replacementVar);
    TIntermSymbol *flippedBuiltinRef = new TIntermSymbol(replacementVar);

    // Use this new variable instead of 'builtin' everywhere.
    if (!ReplaceVariable(compiler, root, builtin, replacementVar))
    {
        return false;
    }

    // Create the expression "(builtin.xy * fragRotation)"
    TIntermTyped *rotatedXY;
    if (fragRotation)
    {
        rotatedXY = new TIntermBinary(EOpMatrixTimesVector, fragRotation, builtinXY);
    }
    else
    {
        // No rotation applied, use original variable.
        rotatedXY = builtinXY;
    }

    // Create the expression "(builtin.xy - pivot) * flipXY + pivot
    TIntermBinary *removePivot = new TIntermBinary(EOpSub, rotatedXY, pivot);
    TIntermBinary *inverseXY   = new TIntermBinary(EOpMul, removePivot, flipXY);
    TIntermBinary *plusPivot   = new TIntermBinary(EOpAdd, inverseXY, pivot->deepCopy());

    // Create the corrected variable and copy the value of the original builtin.
    TIntermBinary *assignment =
        new TIntermBinary(EOpAssign, flippedBuiltinRef, builtinRef->deepCopy());

    // Create an assignment to the replaced variable's .xy.
    TIntermSwizzle *correctedXY =
        new TIntermSwizzle(flippedBuiltinRef->deepCopy(), swizzleOffsetXY);
    TIntermBinary *assignToY = new TIntermBinary(EOpAssign, correctedXY, plusPivot);

    // Add this assigment at the beginning of the main function
    insertSequence->insert(insertSequence->begin(), assignToY);
    insertSequence->insert(insertSequence->begin(), assignment);

    return compiler->validateAST(root);
}

TIntermSequence *GetMainSequence(TIntermBlock *root)
{
    TIntermFunctionDefinition *main = FindMain(root);
    return main->getBody()->getSequence();
}

// Declares a new variable to replace gl_DepthRange, its values are fed from a driver uniform.
ANGLE_NO_DISCARD bool ReplaceGLDepthRangeWithDriverUniform(TCompiler *compiler,
                                                           TIntermBlock *root,
                                                           const DriverUniform *driverUniforms,
                                                           TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_DepthRange"
    const TVariable *depthRangeVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_DepthRange"), 0));

    // ANGLEUniforms.depthRange
    TIntermTyped *angleEmulatedDepthRangeRef = driverUniforms->getDepthRangeRef();

    // Use this variable instead of gl_DepthRange everywhere.
    return ReplaceVariableWithTyped(compiler, root, depthRangeVar, angleEmulatedDepthRangeRef);
}

// Declares a new variable to replace gl_BoundingBoxEXT, its values are fed from a global temporary
// variable.
ANGLE_NO_DISCARD bool ReplaceGLBoundingBoxWithGlobal(TCompiler *compiler,
                                                     TIntermBlock *root,
                                                     TSymbolTable *symbolTable,
                                                     int shaderVersion)
{
    // Declare the replacement bounding box variable type
    TType *emulatedBoundingBoxDeclType = new TType(EbtFloat, EbpHigh, EvqGlobal, 4);
    emulatedBoundingBoxDeclType->makeArray(2u);

    TVariable *ANGLEBoundingBoxVar = new TVariable(
        symbolTable->nextUniqueId(), ImmutableString("ANGLEBoundingBox"), SymbolType::AngleInternal,
        TExtension::EXT_primitive_bounding_box, emulatedBoundingBoxDeclType);

    DeclareGlobalVariable(root, ANGLEBoundingBoxVar);

    const TVariable *builtinBoundingBoxVar;
    bool replacementResult = true;

    // Create a symbol reference to "gl_BoundingBoxEXT"
    builtinBoundingBoxVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_BoundingBoxEXT"), shaderVersion));
    if (builtinBoundingBoxVar != nullptr)
    {
        // Use the replacement variable instead of builtin gl_BoundingBoxEXT everywhere.
        replacementResult &=
            ReplaceVariable(compiler, root, builtinBoundingBoxVar, ANGLEBoundingBoxVar);
    }

    // Create a symbol reference to "gl_BoundingBoxOES"
    builtinBoundingBoxVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_BoundingBoxOES"), shaderVersion));
    if (builtinBoundingBoxVar != nullptr)
    {
        // Use the replacement variable instead of builtin gl_BoundingBoxOES everywhere.
        replacementResult &=
            ReplaceVariable(compiler, root, builtinBoundingBoxVar, ANGLEBoundingBoxVar);
    }

    if (shaderVersion >= 320)
    {
        // Create a symbol reference to "gl_BoundingBox"
        builtinBoundingBoxVar = static_cast<const TVariable *>(
            symbolTable->findBuiltIn(ImmutableString("gl_BoundingBox"), shaderVersion));
        if (builtinBoundingBoxVar != nullptr)
        {
            // Use the replacement variable instead of builtin gl_BoundingBox everywhere.
            replacementResult &=
                ReplaceVariable(compiler, root, builtinBoundingBoxVar, ANGLEBoundingBoxVar);
        }
    }
    return replacementResult;
}

TVariable *AddANGLEPositionVaryingDeclaration(TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              TQualifier qualifier)
{
    // Define a vec2 driver varying to hold the line rasterization emulation position.
    TType *varyingType = new TType(EbtFloat, EbpMedium, qualifier, 2);
    TVariable *varyingVar =
        new TVariable(symbolTable, ImmutableString(vk::kLineRasterEmulationPosition), varyingType,
                      SymbolType::AngleInternal);
    TIntermSymbol *varyingDeclarator = new TIntermSymbol(varyingVar);
    TIntermDeclaration *varyingDecl  = new TIntermDeclaration;
    varyingDecl->appendDeclarator(varyingDeclarator);

    TIntermSequence insertSequence;
    insertSequence.push_back(varyingDecl);

    // Insert the declarations before Main.
    size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, insertSequence);

    return varyingVar;
}

ANGLE_NO_DISCARD bool AddBresenhamEmulationVS(TCompiler *compiler,
                                              TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              SpecConst *specConst,
                                              const DriverUniform *driverUniforms)
{
    TVariable *anglePosition = AddANGLEPositionVaryingDeclaration(root, symbolTable, EvqVaryingOut);

    // Clamp position to subpixel grid.
    // Do perspective divide (get normalized device coords)
    // "vec2 ndc = gl_Position.xy / gl_Position.w"
    const TType *vec2Type        = StaticType::GetTemporary<EbtFloat, EbpHigh, 2>();
    TIntermTyped *viewportRef    = driverUniforms->getViewportRef();
    TIntermSymbol *glPos         = new TIntermSymbol(BuiltInVariable::gl_Position());
    TIntermSwizzle *glPosXY      = CreateSwizzle(glPos, 0, 1);
    TIntermSwizzle *glPosW       = CreateSwizzle(glPos->deepCopy(), 3);
    TVariable *ndc               = CreateTempVariable(symbolTable, vec2Type);
    TIntermBinary *noPerspective = new TIntermBinary(EOpDiv, glPosXY, glPosW);
    TIntermDeclaration *ndcDecl  = CreateTempInitDeclarationNode(ndc, noPerspective);

    // Convert NDC to window coordinates. According to Vulkan spec.
    // "vec2 window = 0.5 * viewport.wh * (ndc + 1) + viewport.xy"
    TIntermBinary *ndcPlusOne =
        new TIntermBinary(EOpAdd, CreateTempSymbolNode(ndc), CreateFloatNode(1.0f, EbpMedium));
    TIntermSwizzle *viewportZW = CreateSwizzle(viewportRef, 2, 3);
    TIntermBinary *ndcViewport = new TIntermBinary(EOpMul, viewportZW, ndcPlusOne);
    TIntermBinary *ndcViewportHalf =
        new TIntermBinary(EOpVectorTimesScalar, ndcViewport, CreateFloatNode(0.5f, EbpMedium));
    TIntermSwizzle *viewportXY     = CreateSwizzle(viewportRef->deepCopy(), 0, 1);
    TIntermBinary *ndcToWindow     = new TIntermBinary(EOpAdd, ndcViewportHalf, viewportXY);
    TVariable *windowCoords        = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *windowDecl = CreateTempInitDeclarationNode(windowCoords, ndcToWindow);

    // Clamp to subpixel grid.
    // "vec2 clamped = round(window * 2^{subpixelBits}) / 2^{subpixelBits}"
    int subpixelBits = compiler->getResources().SubPixelBits;
    TIntermConstantUnion *scaleConstant =
        CreateFloatNode(static_cast<float>(1 << subpixelBits), EbpHigh);
    TIntermBinary *windowScaled =
        new TIntermBinary(EOpVectorTimesScalar, CreateTempSymbolNode(windowCoords), scaleConstant);
    TIntermTyped *windowRounded =
        CreateBuiltInUnaryFunctionCallNode("round", windowScaled, *symbolTable, 300);
    TIntermBinary *windowRoundedBack =
        new TIntermBinary(EOpDiv, windowRounded, scaleConstant->deepCopy());
    TVariable *clampedWindowCoords = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *clampedDecl =
        CreateTempInitDeclarationNode(clampedWindowCoords, windowRoundedBack);

    // Set varying.
    // "ANGLEPosition = 2 * (clamped - viewport.xy) / viewport.wh - 1"
    TIntermBinary *clampedOffset = new TIntermBinary(
        EOpSub, CreateTempSymbolNode(clampedWindowCoords), viewportXY->deepCopy());
    TIntermBinary *clampedOff2x =
        new TIntermBinary(EOpVectorTimesScalar, clampedOffset, CreateFloatNode(2.0f, EbpMedium));
    TIntermBinary *clampedDivided = new TIntermBinary(EOpDiv, clampedOff2x, viewportZW->deepCopy());
    TIntermBinary *clampedNDC =
        new TIntermBinary(EOpSub, clampedDivided, CreateFloatNode(1.0f, EbpMedium));
    TIntermSymbol *varyingRef    = new TIntermSymbol(anglePosition);
    TIntermBinary *varyingAssign = new TIntermBinary(EOpAssign, varyingRef, clampedNDC);

    TIntermBlock *emulationBlock = new TIntermBlock;
    emulationBlock->appendStatement(ndcDecl);
    emulationBlock->appendStatement(windowDecl);
    emulationBlock->appendStatement(clampedDecl);
    emulationBlock->appendStatement(varyingAssign);
    TIntermIfElse *ifEmulation =
        new TIntermIfElse(specConst->getLineRasterEmulation(), emulationBlock, nullptr);

    // Ensure the statements run at the end of the main() function.
    return RunAtTheEndOfShader(compiler, root, ifEmulation, symbolTable);
}

ANGLE_NO_DISCARD bool AddXfbEmulationSupport(TCompiler *compiler,
                                             TIntermBlock *root,
                                             TSymbolTable *symbolTable,
                                             const DriverUniform *driverUniforms)
{
    // Generate the following function and place it before main().  This function takes a "strides"
    // parameter that is determined at link time, and calculates for each transform feedback buffer
    // (of which there are a maximum of four) what the starting index is to write to the output
    // buffer.
    //
    //     ivec4 ANGLEGetXfbOffsets(ivec4 strides)
    //     {
    //         int xfbIndex = gl_VertexIndex
    //                      + gl_InstanceIndex * ANGLEUniforms.xfbVerticesPerInstance;
    //         return ANGLEUniforms.xfbBufferOffsets + xfbIndex * strides;
    //     }

    constexpr uint32_t kMaxXfbBuffers = 4;

    const TType *ivec4Type = StaticType::GetBasic<EbtInt, EbpHigh, kMaxXfbBuffers>();
    TType *stridesType     = new TType(*ivec4Type);
    stridesType->setQualifier(EvqParamConst);

    // Create the parameter variable.
    TVariable *stridesVar = new TVariable(symbolTable, ImmutableString("strides"), stridesType,
                                          SymbolType::AngleInternal);
    TIntermSymbol *stridesSymbol = new TIntermSymbol(stridesVar);

    // Create references to gl_VertexIndex, gl_InstanceIndex, ANGLEUniforms.xfbVerticesPerInstance
    // and ANGLEUniforms.xfbBufferOffsets.
    TIntermSymbol *vertexIndex           = new TIntermSymbol(BuiltInVariable::gl_VertexIndex());
    TIntermSymbol *instanceIndex         = new TIntermSymbol(BuiltInVariable::gl_InstanceIndex());
    TIntermTyped *xfbVerticesPerInstance = driverUniforms->getXfbVerticesPerInstance();
    TIntermTyped *xfbBufferOffsets       = driverUniforms->getXfbBufferOffsets();

    // gl_InstanceIndex * ANGLEUniforms.xfbVerticesPerInstance
    TIntermBinary *xfbInstanceIndex =
        new TIntermBinary(EOpMul, instanceIndex, xfbVerticesPerInstance);

    // gl_VertexIndex + |xfbInstanceIndex|
    TIntermBinary *xfbIndex = new TIntermBinary(EOpAdd, vertexIndex, xfbInstanceIndex);

    // |xfbIndex| * |strides|
    TIntermBinary *xfbStrides = new TIntermBinary(EOpVectorTimesScalar, xfbIndex, stridesSymbol);

    // ANGLEUniforms.xfbBufferOffsets + |xfbStrides|
    TIntermBinary *xfbOffsets = new TIntermBinary(EOpAdd, xfbBufferOffsets, xfbStrides);

    // Create the function body, which has a single return statement.  Note that the `xfbIndex`
    // variable declared in the comment at the beginning of this function is simply replaced in the
    // return statement for brevity.
    TIntermBlock *body = new TIntermBlock;
    body->appendStatement(new TIntermBranch(EOpReturn, xfbOffsets));

    // Declare the function
    TFunction *getOffsetsFunction =
        new TFunction(symbolTable, ImmutableString(vk::kXfbEmulationGetOffsetsFunctionName),
                      SymbolType::AngleInternal, ivec4Type, true);
    getOffsetsFunction->addParameter(stridesVar);

    TIntermFunctionDefinition *functionDef =
        CreateInternalFunctionDefinitionNode(*getOffsetsFunction, body);

    // Insert the function declaration before main().
    const size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, {functionDef});

    // Generate the following function and place it before main().  This function will be filled
    // with transform feedback capture code at link time.
    //
    //     void ANGLECaptureXfb()
    //     {
    //     }
    const TType *voidType = StaticType::GetBasic<EbtVoid, EbpUndefined>();

    // Create the function body, which is empty.
    body = new TIntermBlock;

    // Declare the function
    TFunction *xfbCaptureFunction =
        new TFunction(symbolTable, ImmutableString(vk::kXfbEmulationCaptureFunctionName),
                      SymbolType::AngleInternal, voidType, false);

    // Insert the function declaration before main().
    root->insertChildNodes(mainIndex,
                           {CreateInternalFunctionDefinitionNode(*xfbCaptureFunction, body)});

    // Create the following logic and add it at the end of main():
    //
    //     ANGLECaptureXfb();
    //

    // Create the function call
    TIntermAggregate *captureXfbCall =
        TIntermAggregate::CreateFunctionCall(*xfbCaptureFunction, {});

    TIntermBlock *captureXfbBlock = new TIntermBlock;
    captureXfbBlock->appendStatement(captureXfbCall);

    // Create a call to ANGLEGetXfbOffsets too, for the sole purpose of preventing it from being
    // culled as unused by glslang.
    TIntermSequence ivec4Zero;
    ivec4Zero.push_back(CreateZeroNode(*ivec4Type));
    TIntermAggregate *getOffsetsCall =
        TIntermAggregate::CreateFunctionCall(*getOffsetsFunction, &ivec4Zero);
    captureXfbBlock->appendStatement(getOffsetsCall);

    // Run it at the end of the shader.
    if (!RunAtTheEndOfShader(compiler, root, captureXfbBlock, symbolTable))
    {
        return false;
    }

    // Additionally, generate the following storage buffer declarations used to capture transform
    // feedback output.  Again, there's a maximum of four buffers.
    //
    //     buffer ANGLEXfbBuffer0
    //     {
    //         float xfbOut[];
    //     } ANGLEXfb0;
    //     buffer ANGLEXfbBuffer1
    //     {
    //         float xfbOut[];
    //     } ANGLEXfb1;
    //     ...

    for (uint32_t bufferIndex = 0; bufferIndex < kMaxXfbBuffers; ++bufferIndex)
    {
        TFieldList *fieldList = new TFieldList;
        TType *xfbOutType     = new TType(EbtFloat, EbpHigh, EvqGlobal);
        xfbOutType->makeArray(0);

        TField *field = new TField(xfbOutType, ImmutableString(vk::kXfbEmulationBufferFieldName),
                                   TSourceLoc(), SymbolType::AngleInternal);

        fieldList->push_back(field);

        static_assert(
            kMaxXfbBuffers < 10,
            "ImmutableStringBuilder memory size below needs to accomodate the number of buffers");

        ImmutableStringBuilder blockName(strlen(vk::kXfbEmulationBufferBlockName) + 2);
        blockName << vk::kXfbEmulationBufferBlockName;
        blockName.appendDecimal(bufferIndex);

        ImmutableStringBuilder varName(strlen(vk::kXfbEmulationBufferName) + 2);
        varName << vk::kXfbEmulationBufferName;
        varName.appendDecimal(bufferIndex);

        TLayoutQualifier layoutQualifier = TLayoutQualifier::Create();
        layoutQualifier.blockStorage     = EbsStd430;

        DeclareInterfaceBlock(root, symbolTable, fieldList, EvqBuffer, layoutQualifier,
                              TMemoryQualifier::Create(), 0, blockName, varName);
    }

    return compiler->validateAST(root);
}

ANGLE_NO_DISCARD bool AddXfbExtensionSupport(TCompiler *compiler,
                                             TIntermBlock *root,
                                             TSymbolTable *symbolTable,
                                             const DriverUniform *driverUniforms)
{
    // Generate the following output varying declaration used to capture transform feedback output
    // from gl_Position, as it can't be captured directly due to changes that are applied to it for
    // clip-space correction and pre-rotation.
    //
    //     out vec4 ANGLEXfbPosition;

    const TType *vec4Type = nullptr;

    switch (compiler->getShaderType())
    {
        case GL_VERTEX_SHADER:
            vec4Type = StaticType::Get<EbtFloat, EbpHigh, EvqVertexOut, 4, 1>();
            break;
        case GL_TESS_EVALUATION_SHADER_EXT:
            vec4Type = StaticType::Get<EbtFloat, EbpHigh, EvqTessEvaluationOut, 4, 1>();
            break;
        case GL_GEOMETRY_SHADER_EXT:
            vec4Type = StaticType::Get<EbtFloat, EbpHigh, EvqGeometryOut, 4, 1>();
            break;
        default:
            UNREACHABLE();
    }

    TVariable *varyingVar =
        new TVariable(symbolTable, ImmutableString(vk::kXfbExtensionPositionOutName), vec4Type,
                      SymbolType::AngleInternal);

    TIntermDeclaration *varyingDecl = new TIntermDeclaration();
    varyingDecl->appendDeclarator(new TIntermSymbol(varyingVar));

    // Insert the varying declaration before the first function.
    const size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
    root->insertChildNodes(firstFunctionIndex, {varyingDecl});

    return compiler->validateAST(root);
}

ANGLE_NO_DISCARD bool InsertFragCoordCorrection(TCompiler *compiler,
                                                ShCompileOptions compileOptions,
                                                TIntermBlock *root,
                                                TIntermSequence *insertSequence,
                                                TSymbolTable *symbolTable,
                                                SpecConst *specConst,
                                                const DriverUniform *driverUniforms)
{
    TIntermTyped *flipXY = specConst->getFlipXY();
    if (!flipXY)
    {
        flipXY = driverUniforms->getFlipXYRef();
    }

    TIntermTyped *pivot = specConst->getHalfRenderArea();
    if (!pivot)
    {
        pivot = driverUniforms->getHalfRenderAreaRef();
    }

    TIntermTyped *fragRotation = nullptr;
    if ((compileOptions & SH_ADD_PRE_ROTATION) != 0)
    {
        fragRotation = specConst->getFragRotationMatrix();
        if (!fragRotation)
        {
            fragRotation = driverUniforms->getFragRotationMatrixRef();
        }
    }
    const TVariable *fragCoord = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_FragCoord"), compiler->getShaderVersion()));
    return RotateAndFlipBuiltinVariable(compiler, root, insertSequence, flipXY, symbolTable,
                                        fragCoord, kFlippedFragCoordName, pivot, fragRotation);
}

// This block adds OpenGL line segment rasterization emulation behind a specialization constant
// guard.  OpenGL's simple rasterization algorithm is a strict subset of the pixels generated by the
// Vulkan algorithm. Thus we can implement a shader patch that rejects pixels if they would not be
// generated by the OpenGL algorithm. OpenGL's algorithm is similar to Bresenham's line algorithm.
// It is implemented for each pixel by testing if the line segment crosses a small diamond inside
// the pixel. See the OpenGL ES 2.0 spec section "3.4.1 Basic Line Segment Rasterization". Also
// see the Vulkan spec section "24.6.1. Basic Line Segment Rasterization":
// https://khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#primsrast-lines-basic
//
// Using trigonometric math and the fact that we know the size of the diamond we can derive a
// formula to test if the line segment crosses the pixel center. gl_FragCoord is used along with an
// internal position varying to determine the inputs to the formula.
//
// The implementation of the test is similar to the following pseudocode:
//
// void main()
// {
//    vec2 p  = (((((ANGLEPosition.xy) * 0.5) + 0.5) * viewport.zw) + viewport.xy);
//    vec2 d  = dFdx(p) + dFdy(p);
//    vec2 f  = gl_FragCoord.xy;
//    vec2 p_ = p.yx;
//    vec2 d_ = d.yx;
//    vec2 f_ = f.yx;
//
//    vec2 i = abs(p - f + (d / d_) * (f_ - p_));
//
//    if (i.x > (0.5 + e) && i.y > (0.5 + e))
//        discard;
//     <otherwise run fragment shader main>
// }
//
// Note this emulation can not provide fully correct rasterization. See the docs more more info.

ANGLE_NO_DISCARD bool AddBresenhamEmulationFS(TCompiler *compiler,
                                              ShCompileOptions compileOptions,
                                              TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              SpecConst *specConst,
                                              const DriverUniform *driverUniforms,
                                              bool usesFragCoord)
{
    TVariable *anglePosition = AddANGLEPositionVaryingDeclaration(root, symbolTable, EvqVaryingIn);
    const TType *vec2Type    = StaticType::GetTemporary<EbtFloat, EbpHigh, 2>();

    TIntermTyped *viewportRef = driverUniforms->getViewportRef();

    // vec2 p = ((ANGLEPosition * 0.5) + 0.5) * viewport.zw + viewport.xy
    TIntermSwizzle *viewportXY    = CreateSwizzle(viewportRef->deepCopy(), 0, 1);
    TIntermSwizzle *viewportZW    = CreateSwizzle(viewportRef, 2, 3);
    TIntermSymbol *position       = new TIntermSymbol(anglePosition);
    TIntermConstantUnion *oneHalf = CreateFloatNode(0.5f, EbpMedium);
    TIntermBinary *halfPosition   = new TIntermBinary(EOpVectorTimesScalar, position, oneHalf);
    TIntermBinary *offsetHalfPosition =
        new TIntermBinary(EOpAdd, halfPosition, oneHalf->deepCopy());
    TIntermBinary *scaledPosition = new TIntermBinary(EOpMul, offsetHalfPosition, viewportZW);
    TIntermBinary *windowPosition = new TIntermBinary(EOpAdd, scaledPosition, viewportXY);
    TVariable *p                  = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *pDecl     = CreateTempInitDeclarationNode(p, windowPosition);

    // vec2 d = dFdx(p) + dFdy(p)
    TIntermTyped *dfdx =
        CreateBuiltInUnaryFunctionCallNode("dFdx", new TIntermSymbol(p), *symbolTable, 300);
    TIntermTyped *dfdy =
        CreateBuiltInUnaryFunctionCallNode("dFdy", new TIntermSymbol(p), *symbolTable, 300);
    TIntermBinary *dfsum      = new TIntermBinary(EOpAdd, dfdx, dfdy);
    TVariable *d              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *dDecl = CreateTempInitDeclarationNode(d, dfsum);

    // vec2 f = gl_FragCoord.xy
    const TVariable *fragCoord = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_FragCoord"), compiler->getShaderVersion()));
    TIntermSwizzle *fragCoordXY = CreateSwizzle(new TIntermSymbol(fragCoord), 0, 1);
    TVariable *f                = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *fDecl   = CreateTempInitDeclarationNode(f, fragCoordXY);

    // vec2 p_ = p.yx
    TIntermSwizzle *pyx        = CreateSwizzle(new TIntermSymbol(p), 1, 0);
    TVariable *p_              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *p_decl = CreateTempInitDeclarationNode(p_, pyx);

    // vec2 d_ = d.yx
    TIntermSwizzle *dyx        = CreateSwizzle(new TIntermSymbol(d), 1, 0);
    TVariable *d_              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *d_decl = CreateTempInitDeclarationNode(d_, dyx);

    // vec2 f_ = f.yx
    TIntermSwizzle *fyx        = CreateSwizzle(new TIntermSymbol(f), 1, 0);
    TVariable *f_              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *f_decl = CreateTempInitDeclarationNode(f_, fyx);

    // vec2 i = abs(p - f + (d/d_) * (f_ - p_))
    TIntermBinary *dd   = new TIntermBinary(EOpDiv, new TIntermSymbol(d), new TIntermSymbol(d_));
    TIntermBinary *fp   = new TIntermBinary(EOpSub, new TIntermSymbol(f_), new TIntermSymbol(p_));
    TIntermBinary *ddfp = new TIntermBinary(EOpMul, dd, fp);
    TIntermBinary *pf   = new TIntermBinary(EOpSub, new TIntermSymbol(p), new TIntermSymbol(f));
    TIntermBinary *expr = new TIntermBinary(EOpAdd, pf, ddfp);

    TIntermTyped *absd        = CreateBuiltInUnaryFunctionCallNode("abs", expr, *symbolTable, 100);
    TVariable *i              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *iDecl = CreateTempInitDeclarationNode(i, absd);

    // Using a small epsilon value ensures that we don't suffer from numerical instability when
    // lines are exactly vertical or horizontal.
    static constexpr float kEpsilon   = 0.0001f;
    static constexpr float kThreshold = 0.5 + kEpsilon;
    TIntermConstantUnion *threshold   = CreateFloatNode(kThreshold, EbpHigh);

    // if (i.x > (0.5 + e) && i.y > (0.5 + e))
    TIntermSwizzle *ix     = CreateSwizzle(new TIntermSymbol(i), 0);
    TIntermBinary *checkX  = new TIntermBinary(EOpGreaterThan, ix, threshold);
    TIntermSwizzle *iy     = CreateSwizzle(new TIntermSymbol(i), 1);
    TIntermBinary *checkY  = new TIntermBinary(EOpGreaterThan, iy, threshold->deepCopy());
    TIntermBinary *checkXY = new TIntermBinary(EOpLogicalAnd, checkX, checkY);

    // discard
    TIntermBranch *discard     = new TIntermBranch(EOpKill, nullptr);
    TIntermBlock *discardBlock = new TIntermBlock;
    discardBlock->appendStatement(discard);
    TIntermIfElse *ifStatement = new TIntermIfElse(checkXY, discardBlock, nullptr);

    TIntermBlock *emulationBlock       = new TIntermBlock;
    TIntermSequence *emulationSequence = emulationBlock->getSequence();

    std::array<TIntermNode *, 8> nodes = {
        {pDecl, dDecl, fDecl, p_decl, d_decl, f_decl, iDecl, ifStatement}};
    emulationSequence->insert(emulationSequence->begin(), nodes.begin(), nodes.end());

    TIntermIfElse *ifEmulation =
        new TIntermIfElse(specConst->getLineRasterEmulation(), emulationBlock, nullptr);

    // Ensure the line raster code runs at the beginning of main().
    TIntermFunctionDefinition *main = FindMain(root);
    TIntermSequence *mainSequence   = main->getBody()->getSequence();
    ASSERT(mainSequence);

    mainSequence->insert(mainSequence->begin(), ifEmulation);

    // If the shader does not use frag coord, we should insert it inside the emulation if.
    if (!usesFragCoord)
    {
        if (!InsertFragCoordCorrection(compiler, compileOptions, root, emulationSequence,
                                       symbolTable, specConst, driverUniforms))
        {
            return false;
        }
    }

    return compiler->validateAST(root);
}

bool HasFramebufferFetch(const TExtensionBehavior &extBehavior)
{
    return IsExtensionEnabled(extBehavior, TExtension::EXT_shader_framebuffer_fetch) ||
           IsExtensionEnabled(extBehavior, TExtension::EXT_shader_framebuffer_fetch_non_coherent) ||
           IsExtensionEnabled(extBehavior, TExtension::ARM_shader_framebuffer_fetch) ||
           IsExtensionEnabled(extBehavior, TExtension::NV_shader_framebuffer_fetch);
}
}  // anonymous namespace

TranslatorVulkan::TranslatorVulkan(sh::GLenum type, ShShaderSpec spec)
    : TCompiler(type, spec, SH_GLSL_450_CORE_OUTPUT)
{}

bool TranslatorVulkan::translateImpl(TInfoSinkBase &sink,
                                     TIntermBlock *root,
                                     ShCompileOptions compileOptions,
                                     PerformanceDiagnostics * /*perfDiagnostics*/,
                                     SpecConst *specConst,
                                     DriverUniform *driverUniforms)
{
    if (getShaderType() == GL_VERTEX_SHADER)
    {
        if (!ShaderBuiltinsWorkaround(this, root, &getSymbolTable(), compileOptions))
        {
            return false;
        }
    }

    sink << "#version 450 core\n";
    writeExtensionBehavior(compileOptions, sink);
    WritePragma(sink, compileOptions, getPragma());

    // Write out default uniforms into a uniform block assigned to a specific set/binding.
    int defaultUniformCount           = 0;
    int aggregateTypesUsedForUniforms = 0;
    int r32fImageCount                = 0;
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

        if (uniform.active && gl::IsImageType(uniform.type) && uniform.imageUnitFormat == GL_R32F)
        {
            ++r32fImageCount;
        }

        if (uniform.active && gl::IsAtomicCounterType(uniform.type))
        {
            ++atomicCounterCount;
        }
    }

    // Remove declarations of inactive shader interface variables so glslang wrapper doesn't need to
    // replace them.  Note: this is done before extracting samplers from structs, as removing such
    // inactive samplers is not yet supported.  Note also that currently, CollectVariables marks
    // every field of an active uniform that's of struct type as active, i.e. no extracted sampler
    // is inactive.
    if (!RemoveInactiveInterfaceVariables(this, root, &getSymbolTable(), getAttributes(),
                                          getInputVaryings(), getOutputVariables(), getUniforms(),
                                          getInterfaceBlocks(), true))
    {
        return false;
    }

    // If there are any function calls that take array-of-array of opaque uniform parameters, or
    // other opaque uniforms that need special handling in Vulkan, such as atomic counters,
    // monomorphize the functions by removing said parameters and replacing them in the function
    // body with the call arguments.
    //
    // This has a few benefits:
    //
    // - It dramatically simplifies future transformations w.r.t to samplers in structs, array of
    //   arrays of opaque types, atomic counters etc.
    // - Avoids the need for shader*ArrayDynamicIndexing Vulkan features.
    if (!MonomorphizeUnsupportedFunctions(this, root, &getSymbolTable(), compileOptions))
    {
        return false;
    }

    if (aggregateTypesUsedForUniforms > 0)
    {
        if (!SeparateStructFromUniformDeclarations(this, root, &getSymbolTable()))
        {
            return false;
        }

        int removedUniformsCount;

        if (!RewriteStructSamplers(this, root, &getSymbolTable(), &removedUniformsCount))
        {
            return false;
        }
        defaultUniformCount -= removedUniformsCount;
    }

    // Replace array of array of opaque uniforms with a flattened array.  This is run after
    // MonomorphizeUnsupportedFunctions and RewriteStructSamplers so that it's not possible for an
    // array of array of opaque type to be partially subscripted and passed to a function.
    if (!RewriteArrayOfArrayOfOpaqueUniforms(this, root, &getSymbolTable()))
    {
        return false;
    }

    // Rewrite samplerCubes as sampler2DArrays.  This must be done after rewriting struct samplers
    // as it doesn't expect that.
    if ((compileOptions & SH_EMULATE_SEAMFUL_CUBE_MAP_SAMPLING) != 0)
    {
        if (!RewriteCubeMapSamplersAs2DArray(this, root, &getSymbolTable(),
                                             getShaderType() == GL_FRAGMENT_SHADER))
        {
            return false;
        }
    }

    if (!FlagSamplersForTexelFetch(this, root, &getSymbolTable(), &mUniforms))
    {
        return false;
    }

    gl::ShaderType packedShaderType = gl::FromGLenum<gl::ShaderType>(getShaderType());

    if (defaultUniformCount > 0)
    {
        if (!DeclareDefaultUniforms(this, root, &getSymbolTable(), packedShaderType))
        {
            return false;
        }
    }

    if (getShaderType() == GL_COMPUTE_SHADER)
    {
        driverUniforms->addComputeDriverUniformsToShader(root, &getSymbolTable());
    }
    else
    {
        driverUniforms->addGraphicsDriverUniformsToShader(root, &getSymbolTable());
    }

    if (r32fImageCount > 0)
    {
        if (!RewriteR32fImages(this, root, &getSymbolTable()))
        {
            return false;
        }
    }

    if (atomicCounterCount > 0)
    {
        // ANGLEUniforms.acbBufferOffsets
        const TIntermTyped *acbBufferOffsets = driverUniforms->getAbcBufferOffsets();
        if (!RewriteAtomicCounters(this, root, &getSymbolTable(), acbBufferOffsets))
        {
            return false;
        }
    }
    else if (getShaderVersion() >= 310)
    {
        // Vulkan doesn't support Atomic Storage as a Storage Class, but we've seen
        // cases where builtins are using it even with no active atomic counters.
        // This pass simply removes those builtins in that scenario.
        if (!RemoveAtomicCounterBuiltins(this, root))
        {
            return false;
        }
    }

    if (packedShaderType != gl::ShaderType::Compute)
    {
        if (!ReplaceGLDepthRangeWithDriverUniform(this, root, driverUniforms, &getSymbolTable()))
        {
            return false;
        }

        // Search for the gl_ClipDistance/gl_CullDistance usage, if its used, we need to do some
        // replacements.
        bool useClipDistance = false;
        bool useCullDistance = false;
        for (const ShaderVariable &outputVarying : mOutputVaryings)
        {
            if (outputVarying.name == "gl_ClipDistance")
            {
                useClipDistance = true;
            }
            else if (outputVarying.name == "gl_CullDistance")
            {
                useCullDistance = true;
            }
        }
        for (const ShaderVariable &inputVarying : mInputVaryings)
        {
            if (inputVarying.name == "gl_ClipDistance")
            {
                useClipDistance = true;
            }
            else if (inputVarying.name == "gl_CullDistance")
            {
                useCullDistance = true;
            }
        }

        if (useClipDistance &&
            !ReplaceClipDistanceAssignments(this, root, &getSymbolTable(), getShaderType(),
                                            driverUniforms->getClipDistancesEnabled()))
        {
            return false;
        }
        if (useCullDistance &&
            !ReplaceCullDistanceAssignments(this, root, &getSymbolTable(), getShaderType()))
        {
            return false;
        }
    }

    if (gl::ShaderTypeSupportsTransformFeedback(packedShaderType))
    {
        if ((compileOptions & SH_ADD_VULKAN_XFB_EXTENSION_SUPPORT_CODE) != 0)
        {
            // Add support code for transform feedback extension.
            if (!AddXfbExtensionSupport(this, root, &getSymbolTable(), driverUniforms))
            {
                return false;
            }
        }
    }

    switch (packedShaderType)
    {
        case gl::ShaderType::Fragment:
        {
            bool usesPointCoord    = false;
            bool usesFragCoord     = false;
            bool usesSampleMaskIn  = false;
            bool useSamplePosition = false;

            // Search for the gl_PointCoord usage, if its used, we need to flip the y coordinate.
            for (const ShaderVariable &inputVarying : mInputVaryings)
            {
                if (!inputVarying.isBuiltIn())
                {
                    continue;
                }

                if (inputVarying.name == "gl_SampleMaskIn")
                {
                    usesSampleMaskIn = true;
                    continue;
                }

                if (inputVarying.name == "gl_SamplePosition")
                {
                    useSamplePosition = true;
                    continue;
                }

                if (inputVarying.name == "gl_PointCoord")
                {
                    usesPointCoord = true;
                    break;
                }

                if (inputVarying.name == "gl_FragCoord")
                {
                    usesFragCoord = true;
                    break;
                }
            }

            if ((compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION) != 0)
            {
                if (!AddBresenhamEmulationFS(this, compileOptions, root, &getSymbolTable(),
                                             specConst, driverUniforms, usesFragCoord))
                {
                    return false;
                }
            }

            bool usePreRotation  = (compileOptions & SH_ADD_PRE_ROTATION) != 0;
            bool hasGLSampleMask = false;

            for (const ShaderVariable &outputVar : mOutputVariables)
            {
                if (outputVar.name == "gl_SampleMask")
                {
                    ASSERT(!hasGLSampleMask);
                    hasGLSampleMask = true;
                    continue;
                }
            }

            if (usesPointCoord)
            {
                TIntermTyped *flipNegXY = specConst->getNegFlipXY();
                if (!flipNegXY)
                {
                    flipNegXY = driverUniforms->getNegFlipXYRef();
                }
                TIntermConstantUnion *pivot = CreateFloatNode(0.5f, EbpMedium);
                TIntermTyped *fragRotation  = nullptr;
                if (usePreRotation)
                {
                    fragRotation = specConst->getFragRotationMatrix();
                    if (!fragRotation)
                    {
                        fragRotation = driverUniforms->getFragRotationMatrixRef();
                    }
                }
                if (!RotateAndFlipBuiltinVariable(this, root, GetMainSequence(root), flipNegXY,
                                                  &getSymbolTable(),
                                                  BuiltInVariable::gl_PointCoord(),
                                                  kFlippedPointCoordName, pivot, fragRotation))
                {
                    return false;
                }
            }

            if (useSamplePosition)
            {
                TIntermTyped *flipXY = specConst->getFlipXY();
                if (!flipXY)
                {
                    flipXY = driverUniforms->getFlipXYRef();
                }
                TIntermConstantUnion *pivot = CreateFloatNode(0.5f, EbpMedium);
                TIntermTyped *fragRotation  = nullptr;
                if (usePreRotation)
                {
                    fragRotation = specConst->getFragRotationMatrix();
                    if (!fragRotation)
                    {
                        fragRotation = driverUniforms->getFragRotationMatrixRef();
                    }
                }

                const TVariable *samplePositionBuiltin =
                    static_cast<const TVariable *>(getSymbolTable().findBuiltIn(
                        ImmutableString("gl_SamplePosition"), getShaderVersion()));
                if (!RotateAndFlipBuiltinVariable(this, root, GetMainSequence(root), flipXY,
                                                  &getSymbolTable(), samplePositionBuiltin,
                                                  kFlippedPointCoordName, pivot, fragRotation))
                {
                    return false;
                }
            }

            if (usesFragCoord)
            {
                if (!InsertFragCoordCorrection(this, compileOptions, root, GetMainSequence(root),
                                               &getSymbolTable(), specConst, driverUniforms))
                {
                    return false;
                }
            }

            if (HasFramebufferFetch(getExtensionBehavior()))
            {
                if (getShaderVersion() == 100)
                {
                    if (!ReplaceLastFragData(this, root, &getSymbolTable(), &mUniforms))
                    {
                        return false;
                    }
                }
                else
                {
                    if (!ReplaceInOutVariables(this, root, &getSymbolTable(), &mUniforms))
                    {
                        return false;
                    }
                }
            }

            // Emulate gl_FragColor and gl_FragData with normal output variables.
            if (!EmulateFragColorData(this, root, &getSymbolTable()))
            {
                return false;
            }

            if (!RewriteDfdy(this, compileOptions, root, getSymbolTable(), getShaderVersion(),
                             specConst, driverUniforms))
            {
                return false;
            }

            if (!RewriteInterpolateAtOffset(this, compileOptions, root, getSymbolTable(),
                                            getShaderVersion(), specConst, driverUniforms))
            {
                return false;
            }

            if (usesSampleMaskIn && !RewriteSampleMaskIn(this, root, &getSymbolTable()))
            {
                return false;
            }

            if (hasGLSampleMask)
            {
                TIntermTyped *numSamples = driverUniforms->getNumSamplesRef();
                if (!RewriteSampleMask(this, root, &getSymbolTable(), numSamples))
                {
                    return false;
                }
            }

            {
                const TVariable *numSamplesVar =
                    static_cast<const TVariable *>(getSymbolTable().findBuiltIn(
                        ImmutableString("gl_NumSamples"), getShaderVersion()));
                TIntermTyped *numSamples = driverUniforms->getNumSamplesRef();
                if (!ReplaceVariableWithTyped(this, root, numSamplesVar, numSamples))
                {
                    return false;
                }
            }

            if (!EmulateDithering(this, root, &getSymbolTable(), specConst, driverUniforms))
            {
                return false;
            }

            EmitEarlyFragmentTestsGLSL(*this, sink);
            break;
        }

        case gl::ShaderType::Vertex:
        {
            if ((compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION) != 0)
            {
                if (!AddBresenhamEmulationVS(this, root, &getSymbolTable(), specConst,
                                             driverUniforms))
                {
                    return false;
                }
            }

            if ((compileOptions & SH_ADD_VULKAN_XFB_EMULATION_SUPPORT_CODE) != 0)
            {
                // Add support code for transform feedback emulation.  Only applies to vertex shader
                // as tessellation and geometry shader transform feedback capture require
                // VK_EXT_transform_feedback.
                if (!AddXfbEmulationSupport(this, root, &getSymbolTable(), driverUniforms))
                {
                    return false;
                }
            }

            // Append depth range translation to main.
            if (!transformDepthBeforeCorrection(root, driverUniforms))
            {
                return false;
            }

            break;
        }

        case gl::ShaderType::Geometry:
        {
            int maxVertices = getGeometryShaderMaxVertices();

            // max_vertices=0 is not valid in Vulkan
            maxVertices = std::max(1, maxVertices);

            WriteGeometryShaderLayoutQualifiers(
                sink, getGeometryShaderInputPrimitiveType(), getGeometryShaderInvocations(),
                getGeometryShaderOutputPrimitiveType(), maxVertices);
            break;
        }

        case gl::ShaderType::TessControl:
        {
            if (!ReplaceGLBoundingBoxWithGlobal(this, root, &getSymbolTable(), getShaderVersion()))
            {
                return false;
            }
            WriteTessControlShaderLayoutQualifiers(sink, getTessControlShaderOutputVertices());
            break;
        }

        case gl::ShaderType::TessEvaluation:
        {
            WriteTessEvaluationShaderLayoutQualifiers(
                sink, getTessEvaluationShaderInputPrimitiveType(),
                getTessEvaluationShaderInputVertexSpacingType(),
                getTessEvaluationShaderInputOrderingType(),
                getTessEvaluationShaderInputPointType());
            break;
        }

        case gl::ShaderType::Compute:
        {
            EmitWorkGroupSizeGLSL(*this, sink);
            break;
        }

        default:
            UNREACHABLE();
            break;
    }

    specConst->declareSpecConsts(root);
    mValidateASTOptions.validateSpecConstReferences = true;

    // Gather specialization constant usage bits so that we can feedback to context.
    mSpecConstUsageBits = specConst->getSpecConstUsageBits();

    if (!validateAST(root))
    {
        return false;
    }

    // Make sure function call validation is not accidentally left off anywhere.
    ASSERT(mValidateASTOptions.validateFunctionCall);
    ASSERT(mValidateASTOptions.validateNoRawFunctionCalls);

    return true;
}

void TranslatorVulkan::writeExtensionBehavior(ShCompileOptions compileOptions, TInfoSinkBase &sink)
{
    const TExtensionBehavior &extBehavior = getExtensionBehavior();
    TBehavior multiviewBehavior           = EBhUndefined;
    TBehavior multiview2Behavior          = EBhUndefined;
    for (const auto &iter : extBehavior)
    {
        if (iter.second == EBhUndefined || iter.second == EBhDisable)
        {
            continue;
        }

        switch (iter.first)
        {
            case TExtension::OVR_multiview:
                multiviewBehavior = iter.second;
                break;
            case TExtension::OVR_multiview2:
                multiviewBehavior = iter.second;
                break;
            default:
                break;
        }
    }

    if (multiviewBehavior != EBhUndefined || multiview2Behavior != EBhUndefined)
    {
        // Only either OVR_multiview or OVR_multiview2 should be emitted.
        TExtension ext     = TExtension::OVR_multiview;
        TBehavior behavior = multiviewBehavior;
        if (multiview2Behavior != EBhUndefined)
        {
            ext      = TExtension::OVR_multiview2;
            behavior = multiview2Behavior;
        }
        EmitMultiviewGLSL(*this, compileOptions, ext, behavior, sink);
    }
}

bool TranslatorVulkan::translate(TIntermBlock *root,
                                 ShCompileOptions compileOptions,
                                 PerformanceDiagnostics *perfDiagnostics)
{
    TInfoSinkBase sink;

    SpecConst specConst(&getSymbolTable(), compileOptions, getShaderType());

    if ((compileOptions & SH_USE_SPECIALIZATION_CONSTANT) != 0)
    {
        DriverUniform driverUniforms(DriverUniformMode::InterfaceBlock);
        if (!translateImpl(sink, root, compileOptions, perfDiagnostics, &specConst,
                           &driverUniforms))
        {
            return false;
        }
    }
    else
    {
        DriverUniformExtended driverUniformsExt(DriverUniformMode::InterfaceBlock);
        if (!translateImpl(sink, root, compileOptions, perfDiagnostics, &specConst,
                           &driverUniformsExt))
        {
            return false;
        }
    }

#if defined(ANGLE_ENABLE_SPIRV_GENERATION_THROUGH_GLSLANG)
    if ((compileOptions & SH_GENERATE_SPIRV_THROUGH_GLSLANG) != 0)
    {
        // When generating text, glslang cannot know the precision of folded constants so it may
        // infer the wrong precisions.  The following transformation gives constants names with
        // precision to guide glslang.  This is not an issue for SPIR-V generation because the
        // precision information is present in the tree already.
        if (!RecordConstantPrecision(this, root, &getSymbolTable()))
        {
            return false;
        }

        const bool enablePrecision = (compileOptions & SH_IGNORE_PRECISION_QUALIFIERS) == 0;

        // Write translated shader.
        TOutputVulkanGLSL outputGLSL(this, sink, enablePrecision, compileOptions);
        root->traverse(&outputGLSL);

        return compileToSpirv(sink);
    }
#endif

    // Declare the implicitly defined gl_PerVertex I/O blocks if not already.  This will help SPIR-V
    // generation treat them mostly like usual I/O blocks.
    if (!DeclarePerVertexBlocks(this, root, &getSymbolTable()))
    {
        return false;
    }

    return OutputSPIRV(this, root, compileOptions);
}

bool TranslatorVulkan::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary.
    return false;
}

bool TranslatorVulkan::compileToSpirv(const TInfoSinkBase &glsl)
{
    angle::spirv::Blob spirvBlob;
    if (!GlslangCompileToSpirv(getResources(), getShaderType(), glsl.str(), &spirvBlob))
    {
        return false;
    }

    getInfoSink().obj.setBinary(std::move(spirvBlob));
    return true;
}
}  // namespace sh
