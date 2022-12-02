//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A set of transformations that prepare the AST to be compatible with GL_KHR_vulkan_glsl followed
//   by a pass that generates SPIR-V.
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
#include "compiler/translator/StaticType.h"
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
#include "compiler/translator/tree_ops/vulkan/EmulateAdvancedBlendEquations.h"
#include "compiler/translator/tree_ops/vulkan/EmulateDithering.h"
#include "compiler/translator/tree_ops/vulkan/EmulateFragColorData.h"
#include "compiler/translator/tree_ops/vulkan/EmulateYUVBuiltIns.h"
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
#include "compiler/translator/tree_util/RunAtTheBeginningOfShader.h"
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
[[nodiscard]] bool RotateAndFlipBuiltinVariable(TCompiler *compiler,
                                                TIntermBlock *root,
                                                TIntermSequence *insertSequence,
                                                TIntermTyped *swapXY,
                                                TIntermTyped *flipXY,
                                                TSymbolTable *symbolTable,
                                                const TVariable *builtin,
                                                const ImmutableString &flippedVariableName,
                                                TIntermTyped *pivot)
{
    // Create a symbol reference to 'builtin'.
    TIntermSymbol *builtinRef = new TIntermSymbol(builtin);

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

    // Create the expression "(swapXY ? builtin.yx : builtin.xy)"
    TIntermTyped *builtinXY = new TIntermSwizzle(builtinRef, {0, 1});
    TIntermTyped *builtinYX = new TIntermSwizzle(builtinRef->deepCopy(), {1, 0});

    builtinXY = new TIntermTernary(swapXY, builtinYX, builtinXY);

    // Create the expression "(builtin.xy - pivot) * flipXY + pivot
    TIntermBinary *removePivot = new TIntermBinary(EOpSub, builtinXY, pivot);
    TIntermBinary *inverseXY   = new TIntermBinary(EOpMul, removePivot, flipXY);
    TIntermBinary *plusPivot   = new TIntermBinary(EOpAdd, inverseXY, pivot->deepCopy());

    // Create the corrected variable and copy the value of the original builtin.
    TIntermBinary *assignment =
        new TIntermBinary(EOpAssign, flippedBuiltinRef, builtinRef->deepCopy());

    // Create an assignment to the replaced variable's .xy.
    TIntermSwizzle *correctedXY = new TIntermSwizzle(flippedBuiltinRef->deepCopy(), {0, 1});
    TIntermBinary *assignToXY   = new TIntermBinary(EOpAssign, correctedXY, plusPivot);

    // Add this assigment at the beginning of the main function
    insertSequence->insert(insertSequence->begin(), assignToXY);
    insertSequence->insert(insertSequence->begin(), assignment);

    return compiler->validateAST(root);
}

TIntermSequence *GetMainSequence(TIntermBlock *root)
{
    TIntermFunctionDefinition *main = FindMain(root);
    return main->getBody()->getSequence();
}

// Declares a new variable to replace gl_DepthRange, its values are fed from a driver uniform.
[[nodiscard]] bool ReplaceGLDepthRangeWithDriverUniform(TCompiler *compiler,
                                                        TIntermBlock *root,
                                                        const DriverUniform *driverUniforms,
                                                        TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_DepthRange"
    const TVariable *depthRangeVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_DepthRange"), 0));

    // ANGLEUniforms.depthRange
    TIntermTyped *angleEmulatedDepthRangeRef = driverUniforms->getDepthRange();

    // Use this variable instead of gl_DepthRange everywhere.
    return ReplaceVariableWithTyped(compiler, root, depthRangeVar, angleEmulatedDepthRangeRef);
}

// Declares a new variable to replace gl_BoundingBoxEXT, its values are fed from a global temporary
// variable.
[[nodiscard]] bool ReplaceGLBoundingBoxWithGlobal(TCompiler *compiler,
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

[[nodiscard]] bool AddXfbEmulationSupport(TCompiler *compiler,
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

[[nodiscard]] bool AddXfbExtensionSupport(TCompiler *compiler,
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

[[nodiscard]] bool AddVertexTransformationSupport(TCompiler *compiler,
                                                  const ShCompileOptions &compileOptions,
                                                  TIntermBlock *root,
                                                  TSymbolTable *symbolTable,
                                                  SpecConst *specConst,
                                                  const DriverUniform *driverUniforms)
{
    // In GL the viewport transformation is slightly different - see the GL 2.0 spec section "2.12.1
    // Controlling the Viewport".  In Vulkan the corresponding spec section is currently "23.4.
    // Coordinate Transformations".  The following transformation needs to be done:
    //
    //     z_vk = 0.5 * (w_gl + z_gl)
    //
    // where z_vk is the depth output of a Vulkan geometry-stage shader and z_gl is the same for GL.
    //
    // Generate the following function and place it before main().  This function takes
    // gl_Position and rotates xy, and adjusts z (if necessary).
    //
    //     vec4 ANGLETransformPosition(vec4 position)
    //     {
    //         return vec4((swapXY ? position.yx : position.xy) * flipXY,
    //                     transformDepth ? (gl_Position.z + gl_Position.w) / 2 : gl_Position.z,
    //                     gl_Postion.w);
    //     }

    const TType *vec4Type = StaticType::GetBasic<EbtFloat, EbpHigh, 4>();
    TType *positionType   = new TType(*vec4Type);
    positionType->setQualifier(EvqParamConst);

    // Create the parameter variable.
    TVariable *positionVar = new TVariable(symbolTable, ImmutableString("position"), positionType,
                                           SymbolType::AngleInternal);
    TIntermSymbol *positionSymbol = new TIntermSymbol(positionVar);

    // swapXY ? position.yx : position.xy
    TIntermTyped *swapXY = specConst->getSwapXY();
    if (swapXY == nullptr)
    {
        swapXY = driverUniforms->getSwapXY();
    }

    TIntermTyped *xy        = new TIntermSwizzle(positionSymbol, {0, 1});
    TIntermTyped *swappedXY = new TIntermSwizzle(positionSymbol->deepCopy(), {1, 0});
    TIntermTyped *rotatedXY = new TIntermTernary(swapXY, swappedXY, xy);

    // (swapXY ? position.yx : position.xy) * flipXY
    TIntermTyped *flipXY = driverUniforms->getFlipXY(symbolTable, DriverUniformFlip::PreFragment);
    TIntermTyped *rotatedFlippedXY = new TIntermBinary(EOpMul, rotatedXY, flipXY);

    // (gl_Position.z + gl_Position.w) / 2
    TIntermTyped *z = new TIntermSwizzle(positionSymbol->deepCopy(), {2});
    TIntermTyped *w = new TIntermSwizzle(positionSymbol->deepCopy(), {3});

    TIntermTyped *transformedDepth = z;
    if (compileOptions.addVulkanDepthCorrection)
    {
        TIntermBinary *zPlusW = new TIntermBinary(EOpAdd, z, w->deepCopy());
        TIntermBinary *halfZPlusW =
            new TIntermBinary(EOpMul, zPlusW, CreateFloatNode(0.5, EbpMedium));

        // transformDepth ? (gl_Position.z + gl_Position.w) / 2 : gl_Position.z,
        TIntermTyped *transformDepth = driverUniforms->getTransformDepth();
        transformedDepth = new TIntermTernary(transformDepth, halfZPlusW, z->deepCopy());
    }

    // vec4(...);
    TIntermSequence args = {
        rotatedFlippedXY,
        transformedDepth,
        w,
    };
    TIntermTyped *transformedPosition = TIntermAggregate::CreateConstructor(*vec4Type, &args);

    // Create the function body, which has a single return statement.
    TIntermBlock *body = new TIntermBlock;
    body->appendStatement(new TIntermBranch(EOpReturn, transformedPosition));

    // Declare the function
    TFunction *transformPositionFunction =
        new TFunction(symbolTable, ImmutableString(vk::kTransformPositionFunctionName),
                      SymbolType::AngleInternal, vec4Type, true);
    transformPositionFunction->addParameter(positionVar);

    TIntermFunctionDefinition *functionDef =
        CreateInternalFunctionDefinitionNode(*transformPositionFunction, body);

    // Insert the function declaration before main().
    const size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, {functionDef});

    return compiler->validateAST(root);
}

[[nodiscard]] bool InsertFragCoordCorrection(TCompiler *compiler,
                                             const ShCompileOptions &compileOptions,
                                             TIntermBlock *root,
                                             TIntermSequence *insertSequence,
                                             TSymbolTable *symbolTable,
                                             SpecConst *specConst,
                                             const DriverUniform *driverUniforms)
{
    TIntermTyped *flipXY = driverUniforms->getFlipXY(symbolTable, DriverUniformFlip::Fragment);
    TIntermTyped *pivot  = driverUniforms->getHalfRenderArea();

    TIntermTyped *swapXY = specConst->getSwapXY();
    if (swapXY == nullptr)
    {
        swapXY = driverUniforms->getSwapXY();
    }

    const TVariable *fragCoord = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_FragCoord"), compiler->getShaderVersion()));
    return RotateAndFlipBuiltinVariable(compiler, root, insertSequence, swapXY, flipXY, symbolTable,
                                        fragCoord, kFlippedFragCoordName, pivot);
}

bool HasFramebufferFetch(const TExtensionBehavior &extBehavior,
                         const ShCompileOptions &compileOptions)
{
    return IsExtensionEnabled(extBehavior, TExtension::EXT_shader_framebuffer_fetch) ||
           IsExtensionEnabled(extBehavior, TExtension::EXT_shader_framebuffer_fetch_non_coherent) ||
           IsExtensionEnabled(extBehavior, TExtension::ARM_shader_framebuffer_fetch) ||
           IsExtensionEnabled(extBehavior, TExtension::NV_shader_framebuffer_fetch) ||
           (compileOptions.pls.type == ShPixelLocalStorageType::FramebufferFetch &&
            IsExtensionEnabled(extBehavior, TExtension::ANGLE_shader_pixel_local_storage));
}
}  // anonymous namespace

TranslatorVulkan::TranslatorVulkan(sh::GLenum type, ShShaderSpec spec)
    : TCompiler(type, spec, SH_GLSL_450_CORE_OUTPUT)
{}

bool TranslatorVulkan::translateImpl(TIntermBlock *root,
                                     const ShCompileOptions &compileOptions,
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

    // Remove declarations of inactive shader interface variables so SPIR-V transformer doesn't need
    // to replace them.  Note that currently, CollectVariables marks every field of an active
    // uniform that's of struct type as active, i.e. no extracted sampler is inactive, so this can
    // be done before extracting samplers from structs.
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
    UnsupportedFunctionArgsBitSet args{UnsupportedFunctionArgs::StructContainingSamplers,
                                       UnsupportedFunctionArgs::ArrayOfArrayOfSamplerOrImage,
                                       UnsupportedFunctionArgs::AtomicCounter,
                                       UnsupportedFunctionArgs::SamplerCubeEmulation,
                                       UnsupportedFunctionArgs::Image};
    if (!MonomorphizeUnsupportedFunctions(this, root, &getSymbolTable(), compileOptions, args))
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
    if (compileOptions.emulateSeamfulCubeMapSampling)
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
        const TIntermTyped *acbBufferOffsets = driverUniforms->getAcbBufferOffsets();
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
        if (compileOptions.addVulkanXfbExtensionSupportCode)
        {
            // Add support code for transform feedback extension.
            if (!AddXfbExtensionSupport(this, root, &getSymbolTable(), driverUniforms))
            {
                return false;
            }
        }

        // Add support code for pre-rotation and depth correction in the vertex processing stages.
        if (!AddVertexTransformationSupport(this, compileOptions, root, &getSymbolTable(),
                                            specConst, driverUniforms))
        {
            return false;
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
                TIntermTyped *flipNegXY =
                    driverUniforms->getNegFlipXY(&getSymbolTable(), DriverUniformFlip::Fragment);
                TIntermConstantUnion *pivot = CreateFloatNode(0.5f, EbpMedium);
                TIntermTyped *swapXY        = specConst->getSwapXY();
                if (swapXY == nullptr)
                {
                    swapXY = driverUniforms->getSwapXY();
                }
                if (!RotateAndFlipBuiltinVariable(
                        this, root, GetMainSequence(root), swapXY, flipNegXY, &getSymbolTable(),
                        BuiltInVariable::gl_PointCoord(), kFlippedPointCoordName, pivot))
                {
                    return false;
                }
            }

            if (useSamplePosition)
            {
                TIntermTyped *flipXY =
                    driverUniforms->getFlipXY(&getSymbolTable(), DriverUniformFlip::Fragment);
                TIntermConstantUnion *pivot = CreateFloatNode(0.5f, EbpMedium);
                TIntermTyped *swapXY        = specConst->getSwapXY();
                if (swapXY == nullptr)
                {
                    swapXY = driverUniforms->getSwapXY();
                }

                const TVariable *samplePositionBuiltin =
                    static_cast<const TVariable *>(getSymbolTable().findBuiltIn(
                        ImmutableString("gl_SamplePosition"), getShaderVersion()));
                if (!RotateAndFlipBuiltinVariable(this, root, GetMainSequence(root), swapXY, flipXY,
                                                  &getSymbolTable(), samplePositionBuiltin,
                                                  kFlippedPointCoordName, pivot))
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

            if (HasFramebufferFetch(getExtensionBehavior(), compileOptions))
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

            // This should be operated after doing ReplaceLastFragData and ReplaceInOutVariables,
            // because they will create the input attachment variables. AddBlendMainCaller will
            // check the existing input attachment variables and if there is no existing input
            // attachment variable then create a new one.
            if (getAdvancedBlendEquations().any() &&
                compileOptions.addAdvancedBlendEquationsEmulation &&
                !EmulateAdvancedBlendEquations(this, compileOptions, root, &getSymbolTable(),
                                               driverUniforms, &mUniforms,
                                               getAdvancedBlendEquations()))
            {
                return false;
            }

            if (!RewriteDfdy(this, root, &getSymbolTable(), getShaderVersion(), specConst,
                             driverUniforms))
            {
                return false;
            }

            if (!RewriteInterpolateAtOffset(this, root, &getSymbolTable(), getShaderVersion(),
                                            specConst, driverUniforms))
            {
                return false;
            }

            if (usesSampleMaskIn && !RewriteSampleMaskIn(this, root, &getSymbolTable()))
            {
                return false;
            }

            if (hasGLSampleMask)
            {
                TIntermTyped *numSamples = driverUniforms->getNumSamples();
                if (!RewriteSampleMask(this, root, &getSymbolTable(), numSamples))
                {
                    return false;
                }
            }

            {
                const TVariable *numSamplesVar =
                    static_cast<const TVariable *>(getSymbolTable().findBuiltIn(
                        ImmutableString("gl_NumSamples"), getShaderVersion()));
                TIntermTyped *numSamples = driverUniforms->getNumSamples();
                if (!ReplaceVariableWithTyped(this, root, numSamplesVar, numSamples))
                {
                    return false;
                }
            }

            if (IsExtensionEnabled(getExtensionBehavior(), TExtension::EXT_YUV_target))
            {
                if (!EmulateYUVBuiltIns(this, root, &getSymbolTable()))
                {
                    return false;
                }
            }

            if (!EmulateDithering(this, compileOptions, root, &getSymbolTable(), specConst,
                                  driverUniforms))
            {
                return false;
            }

            break;
        }

        case gl::ShaderType::Vertex:
        {
            if (compileOptions.addVulkanXfbEmulationSupportCode)
            {
                // Add support code for transform feedback emulation.  Only applies to vertex shader
                // as tessellation and geometry shader transform feedback capture require
                // VK_EXT_transform_feedback.
                if (!AddXfbEmulationSupport(this, root, &getSymbolTable(), driverUniforms))
                {
                    return false;
                }
            }

            break;
        }

        case gl::ShaderType::Geometry:
            break;

        case gl::ShaderType::TessControl:
        {
            if (!ReplaceGLBoundingBoxWithGlobal(this, root, &getSymbolTable(), getShaderVersion()))
            {
                return false;
            }
            break;
        }

        case gl::ShaderType::TessEvaluation:
            break;

        case gl::ShaderType::Compute:
            break;

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

bool TranslatorVulkan::translate(TIntermBlock *root,
                                 const ShCompileOptions &compileOptions,
                                 PerformanceDiagnostics *perfDiagnostics)
{
    SpecConst specConst(&getSymbolTable(), compileOptions, getShaderType());

    DriverUniform driverUniforms(DriverUniformMode::InterfaceBlock);
    DriverUniformExtended driverUniformsExt(DriverUniformMode::InterfaceBlock);

    const bool useExtendedDriverUniforms = compileOptions.addVulkanXfbEmulationSupportCode;

    DriverUniform *uniforms = useExtendedDriverUniforms ? &driverUniformsExt : &driverUniforms;

    if (!translateImpl(root, compileOptions, perfDiagnostics, &specConst, uniforms))
    {
        return false;
    }

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
}  // namespace sh
