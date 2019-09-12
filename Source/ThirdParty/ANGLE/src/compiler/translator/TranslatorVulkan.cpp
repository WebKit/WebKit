//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl.
//   The shaders are then fed into glslang to spit out SPIR-V (libANGLE-side).
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/TranslatorVulkan.h"

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/OutputVulkanGLSL.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/tree_ops/NameEmbeddedUniformStructs.h"
#include "compiler/translator/tree_ops/RewriteStructSamplers.h"
#include "compiler/translator/tree_util/BuiltIn_autogen.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{
// This traverses nodes, find the struct ones and add their declarations to the sink. It also
// removes the nodes from the tree as it processes them.
class DeclareStructTypesTraverser : public TIntermTraverser
{
  public:
    explicit DeclareStructTypesTraverser(TOutputVulkanGLSL *outputVulkanGLSL)
        : TIntermTraverser(true, false, false), mOutputVulkanGLSL(outputVulkanGLSL)
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
            mOutputVulkanGLSL->writeStructType(structure);

            TIntermSymbol *symbolNode = declarator->getAsSymbolNode();
            if (symbolNode && symbolNode->variable().symbolType() == SymbolType::Empty)
            {
                // Remove the struct specifier declaration from the tree so it isn't parsed again.
                TIntermSequence emptyReplacement;
                mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                                emptyReplacement);
            }
        }

        return false;
    }

  private:
    TOutputVulkanGLSL *mOutputVulkanGLSL;
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
        bool isUniform = (type.getQualifier() == EvqUniform) && !IsOpaqueType(type.getBasicType());

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

constexpr ImmutableString kFlippedPointCoordName    = ImmutableString("flippedPointCoord");
constexpr ImmutableString kFlippedFragCoordName     = ImmutableString("flippedFragCoord");
constexpr ImmutableString kEmulatedDepthRangeParams = ImmutableString("ANGLEDepthRangeParams");

constexpr const char kViewport[]             = "viewport";
constexpr const char kHalfRenderAreaHeight[] = "halfRenderAreaHeight";
constexpr const char kViewportYScale[]       = "viewportYScale";
constexpr const char kNegViewportYScale[]    = "negViewportYScale";
constexpr const char kDepthRange[]           = "depthRange";

constexpr size_t kNumDriverUniforms                                        = 6;
constexpr std::array<const char *, kNumDriverUniforms> kDriverUniformNames = {
    {kViewport, kHalfRenderAreaHeight, kViewportYScale, kNegViewportYScale, "padding",
     kDepthRange}};

template <TBasicType BasicType = EbtFloat, unsigned char PrimarySize = 1>
TIntermConstantUnion *CreateBasicConstant(float value)
{
    const TType *constantType     = StaticType::GetBasic<BasicType, PrimarySize>();
    TConstantUnion *constantValue = new TConstantUnion[PrimarySize];
    for (unsigned char sizeIndex = 0; sizeIndex < PrimarySize; ++sizeIndex)
    {
        constantValue[sizeIndex].setFConst(value);
    }
    return new TIntermConstantUnion(constantValue, *constantType);
}

size_t FindFieldIndex(const TFieldList &fieldList, const char *fieldName)
{
    for (size_t fieldIndex = 0; fieldIndex < fieldList.size(); ++fieldIndex)
    {
        if (strcmp(fieldList[fieldIndex]->name().data(), fieldName) == 0)
        {
            return fieldIndex;
        }
    }
    UNREACHABLE();
    return 0;
}

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

// Replaces a builtin variable with a version that corrects the Y coordinate.
void FlipBuiltinVariable(TIntermBlock *root,
                         TIntermSequence *insertSequence,
                         TIntermTyped *viewportYScale,
                         TSymbolTable *symbolTable,
                         const TVariable *builtin,
                         const ImmutableString &flippedVariableName,
                         TIntermTyped *pivot)
{
    // Create a symbol reference to 'builtin'.
    TIntermSymbol *builtinRef = new TIntermSymbol(builtin);

    // Create a swizzle to "builtin.y"
    TVector<int> swizzleOffsetY;
    swizzleOffsetY.push_back(1);
    TIntermSwizzle *builtinY = new TIntermSwizzle(builtinRef, swizzleOffsetY);

    // Create a symbol reference to our new variable that will hold the modified builtin.
    const TType *type = StaticType::GetForVec<EbtFloat>(
        EvqGlobal, static_cast<unsigned char>(builtin->getType().getNominalSize()));
    TVariable *replacementVar =
        new TVariable(symbolTable, flippedVariableName, type, SymbolType::AngleInternal);
    DeclareGlobalVariable(root, replacementVar);
    TIntermSymbol *flippedBuiltinRef = new TIntermSymbol(replacementVar);

    // Use this new variable instead of 'builtin' everywhere.
    ReplaceVariable(root, builtin, replacementVar);

    // Create the expression "(builtin.y - pivot) * viewportYScale + pivot
    TIntermBinary *removePivot = new TIntermBinary(EOpSub, builtinY, pivot);
    TIntermBinary *inverseY    = new TIntermBinary(EOpMul, removePivot, viewportYScale);
    TIntermBinary *plusPivot   = new TIntermBinary(EOpAdd, inverseY, pivot->deepCopy());

    // Create the corrected variable and copy the value of the original builtin.
    TIntermSequence *sequence = new TIntermSequence();
    sequence->push_back(builtinRef);
    TIntermAggregate *aggregate = TIntermAggregate::CreateConstructor(builtin->getType(), sequence);
    TIntermBinary *assignment   = new TIntermBinary(EOpInitialize, flippedBuiltinRef, aggregate);

    // Create an assignment to the replaced variable's y.
    TIntermSwizzle *correctedY = new TIntermSwizzle(flippedBuiltinRef, swizzleOffsetY);
    TIntermBinary *assignToY   = new TIntermBinary(EOpAssign, correctedY, plusPivot);

    // Add this assigment at the beginning of the main function
    insertSequence->insert(insertSequence->begin(), assignToY);
    insertSequence->insert(insertSequence->begin(), assignment);
}

TIntermSequence *GetMainSequence(TIntermBlock *root)
{
    TIntermFunctionDefinition *main = FindMain(root);
    return main->getBody()->getSequence();
}

// Declares a new variable to replace gl_DepthRange, its values are fed from a driver uniform.
void ReplaceGLDepthRangeWithDriverUniform(TIntermBlock *root,
                                          const TVariable *driverUniforms,
                                          TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_DepthRange"
    const TVariable *depthRangeVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_DepthRange"), 0));

    // ANGLEUniforms.depthRange
    TIntermBinary *angleEmulatedDepthRangeRef = CreateDriverUniformRef(driverUniforms, kDepthRange);

    // Use this variable instead of gl_DepthRange everywhere.
    ReplaceVariableWithTyped(root, depthRangeVar, angleEmulatedDepthRangeRef);
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
void AppendVertexShaderDepthCorrectionToMain(TIntermBlock *root, TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_Position"
    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    // Create a swizzle to "gl_Position.z"
    TVector<int> swizzleOffsetZ;
    swizzleOffsetZ.push_back(2);
    TIntermSwizzle *positionZ = new TIntermSwizzle(positionRef, swizzleOffsetZ);

    // Create a constant "0.5"
    TIntermConstantUnion *oneHalf = CreateBasicConstant(0.5f);

    // Create a swizzle to "gl_Position.w"
    TVector<int> swizzleOffsetW;
    swizzleOffsetW.push_back(3);
    TIntermSwizzle *positionW = new TIntermSwizzle(positionRef->deepCopy(), swizzleOffsetW);

    // Create the expression "(gl_Position.z + gl_Position.w) * 0.5".
    TIntermBinary *zPlusW = new TIntermBinary(EOpAdd, positionZ->deepCopy(), positionW->deepCopy());
    TIntermBinary *halfZPlusW = new TIntermBinary(EOpMul, zPlusW, oneHalf->deepCopy());

    // Create the assignment "gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5"
    TIntermTyped *positionZLHS = positionZ->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionZLHS, halfZPlusW);

    // Append the assignment as a statement at the end of the shader.
    RunAtTheEndOfShader(root, assignment, symbolTable);
}

// The AddDriverUniformsToShader operation adds an internal uniform block to a shader. The driver
// block is used to implement Vulkan-specific features and workarounds. Returns the driver uniforms
// variable.
const TVariable *AddDriverUniformsToShader(TIntermBlock *root, TSymbolTable *symbolTable)
{
    // Init the depth range type.
    TFieldList *depthRangeParamsFields = new TFieldList();
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("near"), TSourceLoc(),
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("far"), TSourceLoc(),
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("diff"), TSourceLoc(),
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("dummyPacker"), TSourceLoc(),
                                                 SymbolType::AngleInternal));
    TStructure *emulatedDepthRangeParams = new TStructure(
        symbolTable, kEmulatedDepthRangeParams, depthRangeParamsFields, SymbolType::AngleInternal);
    TType *emulatedDepthRangeType = new TType(emulatedDepthRangeParams, false);

    // Declare a global depth range variable.
    TVariable *depthRangeVar =
        new TVariable(symbolTable->nextUniqueId(), kEmptyImmutableString, SymbolType::Empty,
                      TExtension::UNDEFINED, emulatedDepthRangeType);

    DeclareGlobalVariable(root, depthRangeVar);

    // This field list mirrors the structure of ContextVk::DriverUniforms.
    TFieldList *driverFieldList = new TFieldList;

    const std::array<TType *, kNumDriverUniforms> kDriverUniformTypes = {{
        new TType(EbtFloat, 4),
        new TType(EbtFloat),
        new TType(EbtFloat),
        new TType(EbtFloat),
        new TType(EbtFloat),
        emulatedDepthRangeType,
    }};

    for (size_t uniformIndex = 0; uniformIndex < kNumDriverUniforms; ++uniformIndex)
    {
        TField *driverUniformField = new TField(kDriverUniformTypes[uniformIndex],
                                                ImmutableString(kDriverUniformNames[uniformIndex]),
                                                TSourceLoc(), SymbolType::AngleInternal);
        driverFieldList->push_back(driverUniformField);
    }

    // Define a driver uniform block "ANGLEUniformBlock".
    TLayoutQualifier driverLayoutQualifier = TLayoutQualifier::Create();
    TInterfaceBlock *interfaceBlock =
        new TInterfaceBlock(symbolTable, ImmutableString("ANGLEUniformBlock"), driverFieldList,
                            driverLayoutQualifier, SymbolType::AngleInternal);

    // Make the inteface block into a declaration. Use instance name "ANGLEUniforms".
    TType *interfaceBlockType = new TType(interfaceBlock, EvqUniform, driverLayoutQualifier);
    TIntermDeclaration *driverUniformsDecl = new TIntermDeclaration;
    TVariable *driverUniformsVar = new TVariable(symbolTable, ImmutableString("ANGLEUniforms"),
                                                 interfaceBlockType, SymbolType::AngleInternal);
    TIntermSymbol *driverUniformsDeclarator = new TIntermSymbol(driverUniformsVar);
    driverUniformsDecl->appendDeclarator(driverUniformsDeclarator);

    // Insert the declarations before Main.
    TIntermSequence *insertSequence = new TIntermSequence;
    insertSequence->push_back(driverUniformsDecl);

    size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, *insertSequence);

    return driverUniformsVar;
}

TIntermPreprocessorDirective *GenerateLineRasterIfDef()
{
    return new TIntermPreprocessorDirective(
        PreprocessorDirective::Ifdef, ImmutableString("ANGLE_ENABLE_LINE_SEGMENT_RASTERIZATION"));
}

TIntermPreprocessorDirective *GenerateEndIf()
{
    return new TIntermPreprocessorDirective(PreprocessorDirective::Endif, kEmptyImmutableString);
}

TVariable *AddANGLEPositionVaryingDeclaration(TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              TQualifier qualifier)
{
    TIntermSequence *insertSequence = new TIntermSequence;

    insertSequence->push_back(GenerateLineRasterIfDef());

    // Define a driver varying vec2 "ANGLEPosition".
    TType *varyingType               = new TType(EbtFloat, EbpMedium, qualifier, 4);
    TVariable *varyingVar            = new TVariable(symbolTable, ImmutableString("ANGLEPosition"),
                                          varyingType, SymbolType::AngleInternal);
    TIntermSymbol *varyingDeclarator = new TIntermSymbol(varyingVar);
    TIntermDeclaration *varyingDecl  = new TIntermDeclaration;
    varyingDecl->appendDeclarator(varyingDeclarator);
    insertSequence->push_back(varyingDecl);

    insertSequence->push_back(GenerateEndIf());

    // Insert the declarations before Main.
    size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, *insertSequence);

    return varyingVar;
}

void AddANGLEPositionVarying(TIntermBlock *root, TSymbolTable *symbolTable)
{
    TVariable *anglePosition = AddANGLEPositionVaryingDeclaration(root, symbolTable, EvqVaryingOut);

    // Create an assignment "ANGLEPosition = gl_Position".
    const TVariable *position = BuiltInVariable::gl_Position();
    TIntermSymbol *varyingRef = new TIntermSymbol(anglePosition);
    TIntermBinary *assignment =
        new TIntermBinary(EOpAssign, varyingRef, new TIntermSymbol(position));

    // Ensure the assignment runs at the end of the main() function.
    TIntermFunctionDefinition *main = FindMain(root);
    TIntermBlock *mainBody          = main->getBody();
    mainBody->appendStatement(GenerateLineRasterIfDef());
    mainBody->appendStatement(assignment);
    mainBody->appendStatement(GenerateEndIf());
}

void InsertFragCoordCorrection(TIntermBlock *root,
                               TIntermSequence *insertSequence,
                               TSymbolTable *symbolTable,
                               const TVariable *driverUniforms)
{
    TIntermBinary *viewportYScale = CreateDriverUniformRef(driverUniforms, kViewportYScale);
    TIntermBinary *pivot          = CreateDriverUniformRef(driverUniforms, kHalfRenderAreaHeight);
    FlipBuiltinVariable(root, insertSequence, viewportYScale, symbolTable,
                        BuiltInVariable::gl_FragCoord(), kFlippedFragCoordName, pivot);
}

// This block adds OpenGL line segment rasterization emulation behind #ifdef guards.
// OpenGL's simple rasterization algorithm is a strict subset of the pixels generated by the Vulkan
// algorithm. Thus we can implement a shader patch that rejects pixels if they would not be
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
// The implementation of the test code is similar to the following pseudocode:
//
// void main()
// {
//     vec2 b = (((position.xy / position.w) * 0.5) + 0.5) * gl_Viewport.zw + gl_Viewport.xy;
//     vec2 ba = abs(b - gl_FragCoord.xy);
//     vec2 ba2 = 2.0 * (ba * ba);
//     vec2 bp = ba2 + ba2.yx - ba;
//     if (bp.x > epsilon && bp.y > epsilon)
//         discard;
//     <otherwise run fragment shader main>
// }
void AddLineSegmentRasterizationEmulation(TInfoSinkBase &sink,
                                          TIntermBlock *root,
                                          TSymbolTable *symbolTable,
                                          const TVariable *driverUniforms,
                                          bool usesFragCoord)
{
    TVariable *anglePosition = AddANGLEPositionVaryingDeclaration(root, symbolTable, EvqVaryingIn);

    const TType *vec2Type = StaticType::GetBasic<EbtFloat, 2>();

    // Create a swizzle to "ANGLEUniforms.viewport.xy".
    TIntermBinary *viewportRef = CreateDriverUniformRef(driverUniforms, kViewport);
    TVector<int> swizzleOffsetXY;
    swizzleOffsetXY.push_back(0);
    swizzleOffsetXY.push_back(1);
    TIntermSwizzle *viewportXY = new TIntermSwizzle(viewportRef->deepCopy(), swizzleOffsetXY);

    // Create a swizzle to "ANGLEUniforms.viewport.zw".
    TVector<int> swizzleOffsetZW;
    swizzleOffsetZW.push_back(2);
    swizzleOffsetZW.push_back(3);
    TIntermSwizzle *viewportZW = new TIntermSwizzle(viewportRef, swizzleOffsetZW);

    // ANGLEPosition.xy / ANGLEPosition.w
    TIntermSymbol *position    = new TIntermSymbol(anglePosition);
    TIntermSwizzle *positionXY = new TIntermSwizzle(position, swizzleOffsetXY);
    TVector<int> swizzleOffsetW;
    swizzleOffsetW.push_back(3);
    TIntermSwizzle *positionW  = new TIntermSwizzle(position->deepCopy(), swizzleOffsetW);
    TIntermBinary *positionNDC = new TIntermBinary(EOpDiv, positionXY, positionW);

    // ANGLEPosition * 0.5
    TIntermConstantUnion *oneHalf = CreateBasicConstant(0.5f);
    TIntermBinary *halfPosition   = new TIntermBinary(EOpVectorTimesScalar, positionNDC, oneHalf);

    // (ANGLEPosition * 0.5) + 0.5
    TIntermBinary *offsetHalfPosition =
        new TIntermBinary(EOpAdd, halfPosition, oneHalf->deepCopy());

    // ((ANGLEPosition * 0.5) + 0.5) * ANGLEUniforms.viewport.zw
    TIntermBinary *scaledPosition = new TIntermBinary(EOpMul, offsetHalfPosition, viewportZW);

    // ((ANGLEPosition * 0.5) + 0.5) * ANGLEUniforms.viewport + ANGLEUniforms.viewport.xy
    TIntermBinary *windowPosition = new TIntermBinary(EOpAdd, scaledPosition, viewportXY);

    // Assign to a temporary "b".
    TVariable *bTemp          = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *bDecl = CreateTempInitDeclarationNode(bTemp, windowPosition);

    // gl_FragCoord.xy
    const TVariable *fragCoord  = BuiltInVariable::gl_FragCoord();
    TIntermSymbol *fragCoordRef = new TIntermSymbol(fragCoord);
    TIntermSwizzle *fragCoordXY = new TIntermSwizzle(fragCoordRef, swizzleOffsetXY);

    // b - gl_FragCoord.xy
    TIntermSymbol *bRef           = CreateTempSymbolNode(bTemp);
    TIntermBinary *differenceExpr = new TIntermBinary(EOpSub, bRef, fragCoordXY);

    // abs(b - gl_FragCoord.xy)
    TIntermUnary *baAbs = new TIntermUnary(EOpAbs, differenceExpr, nullptr);

    // Assign to a temporary "ba".
    TVariable *baTemp          = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *baDecl = CreateTempInitDeclarationNode(baTemp, baAbs);
    TIntermSymbol *ba          = CreateTempSymbolNode(baTemp);

    // ba * ba
    TIntermBinary *baSq = new TIntermBinary(EOpMul, ba, ba->deepCopy());

    // 2.0 * ba * ba
    TIntermTyped *two      = CreateBasicConstant(2.0f);
    TIntermBinary *twoBaSq = new TIntermBinary(EOpVectorTimesScalar, baSq, two);

    // Assign to a temporary "ba2".
    TVariable *ba2Temp          = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *ba2Decl = CreateTempInitDeclarationNode(ba2Temp, twoBaSq);

    // Create a swizzle to "ba2.yx".
    TVector<int> swizzleOffsetYX;
    swizzleOffsetYX.push_back(1);
    swizzleOffsetYX.push_back(0);
    TIntermSymbol *ba2    = CreateTempSymbolNode(ba2Temp);
    TIntermSwizzle *ba2YX = new TIntermSwizzle(ba2, swizzleOffsetYX);

    // ba2 + ba2.yx - ba
    TIntermBinary *ba2PlusBaYX2 = new TIntermBinary(EOpAdd, ba2->deepCopy(), ba2YX);
    TIntermBinary *bpInit       = new TIntermBinary(EOpSub, ba2PlusBaYX2, ba->deepCopy());

    // Assign to a temporary "bp".
    TVariable *bpTemp          = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *bpDecl = CreateTempInitDeclarationNode(bpTemp, bpInit);
    TIntermSymbol *bp          = CreateTempSymbolNode(bpTemp);

    // Create a swizzle to "bp.x".
    TVector<int> swizzleOffsetX;
    swizzleOffsetX.push_back(0);
    TIntermSwizzle *bpX = new TIntermSwizzle(bp, swizzleOffsetX);

    // Using a small epsilon value ensures that we don't suffer from numerical instability when
    // lines are exactly vertical or horizontal.
    static constexpr float kEpisilon = 0.00001f;
    TIntermConstantUnion *epsilon    = CreateBasicConstant(kEpisilon);

    // bp.x > epsilon
    TIntermBinary *checkX = new TIntermBinary(EOpGreaterThan, bpX, epsilon);

    // Create a swizzle to "bp.y".
    TVector<int> swizzleOffsetY;
    swizzleOffsetY.push_back(1);
    TIntermSwizzle *bpY = new TIntermSwizzle(bp->deepCopy(), swizzleOffsetY);

    // bp.y > epsilon
    TIntermBinary *checkY = new TIntermBinary(EOpGreaterThan, bpY, epsilon->deepCopy());

    // (bp.x > epsilon) && (bp.y > epsilon)
    TIntermBinary *checkXY = new TIntermBinary(EOpLogicalAnd, checkX, checkY);

    // discard
    TIntermBranch *discard     = new TIntermBranch(EOpKill, nullptr);
    TIntermBlock *discardBlock = new TIntermBlock;
    discardBlock->appendStatement(discard);

    // if ((bp.x > epsilon) && (bp.y > epsilon)) discard;
    TIntermIfElse *ifStatement = new TIntermIfElse(checkXY, discardBlock, nullptr);

    // Ensure the line raster code runs at the beginning of main().
    TIntermFunctionDefinition *main = FindMain(root);
    TIntermSequence *mainSequence   = main->getBody()->getSequence();
    ASSERT(mainSequence);

    std::array<TIntermNode *, 6> nodes = {
        {bDecl, baDecl, ba2Decl, bpDecl, ifStatement, GenerateEndIf()}};
    mainSequence->insert(mainSequence->begin(), nodes.begin(), nodes.end());

    // If the shader does not use frag coord, we should insert it inside the ifdef.
    if (!usesFragCoord)
    {
        InsertFragCoordCorrection(root, mainSequence, symbolTable, driverUniforms);
    }

    mainSequence->insert(mainSequence->begin(), GenerateLineRasterIfDef());
}
}  // anonymous namespace

TranslatorVulkan::TranslatorVulkan(sh::GLenum type, ShShaderSpec spec)
    : TCompiler(type, spec, SH_GLSL_450_CORE_OUTPUT)
{}

void TranslatorVulkan::translate(TIntermBlock *root,
                                 ShCompileOptions compileOptions,
                                 PerformanceDiagnostics * /*perfDiagnostics*/)
{
    TInfoSinkBase &sink = getInfoSink().obj;
    TOutputVulkanGLSL outputGLSL(sink, getArrayIndexClampingStrategy(), getHashFunction(),
                                 getNameMap(), &getSymbolTable(), getShaderType(),
                                 getShaderVersion(), getOutputType(), compileOptions);

    sink << "#version 450 core\n";

    // Write out default uniforms into a uniform block assigned to a specific set/binding.
    int defaultUniformCount        = 0;
    int structTypesUsedForUniforms = 0;
    for (const auto &uniform : getUniforms())
    {
        if (!uniform.isBuiltIn() && uniform.staticUse && !gl::IsOpaqueType(uniform.type))
        {
            ++defaultUniformCount;
        }

        if (uniform.isStruct())
        {
            ++structTypesUsedForUniforms;
        }
    }

    // TODO(lucferron): Refactor this function to do less tree traversals.
    // http://anglebug.com/2461
    if (structTypesUsedForUniforms > 0)
    {
        NameEmbeddedStructUniforms(root, &getSymbolTable());

        defaultUniformCount -= RewriteStructSamplers(root, &getSymbolTable());

        // We must declare the struct types before using them.
        DeclareStructTypesTraverser structTypesTraverser(&outputGLSL);
        root->traverse(&structTypesTraverser);
        structTypesTraverser.updateTree();
    }

    if (defaultUniformCount > 0)
    {
        sink << "\nlayout(@@ DEFAULT-UNIFORMS-SET-BINDING @@) uniform defaultUniforms\n{\n";

        DeclareDefaultUniformsTraverser defaultTraverser(&sink, getHashFunction(), &getNameMap());
        root->traverse(&defaultTraverser);
        defaultTraverser.updateTree();

        sink << "};\n";
    }

    const TVariable *driverUniforms = AddDriverUniformsToShader(root, &getSymbolTable());

    ReplaceGLDepthRangeWithDriverUniform(root, driverUniforms, &getSymbolTable());

    // Declare gl_FragColor and glFragData as webgl_FragColor and webgl_FragData
    // if it's core profile shaders and they are used.
    if (getShaderType() == GL_FRAGMENT_SHADER)
    {
        bool usesPointCoord = false;
        bool usesFragCoord  = false;

        // Search for the gl_PointCoord usage, if its used, we need to flip the y coordinate.
        for (const Varying &inputVarying : mInputVaryings)
        {
            if (!inputVarying.isBuiltIn())
            {
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

        AddLineSegmentRasterizationEmulation(sink, root, &getSymbolTable(), driverUniforms,
                                             usesFragCoord);

        bool hasGLFragColor = false;
        bool hasGLFragData  = false;

        for (const OutputVariable &outputVar : mOutputVariables)
        {
            if (outputVar.name == "gl_FragColor")
            {
                ASSERT(!hasGLFragColor);
                hasGLFragColor = true;
                continue;
            }
            else if (outputVar.name == "gl_FragData")
            {
                ASSERT(!hasGLFragData);
                hasGLFragData = true;
                continue;
            }
        }
        ASSERT(!(hasGLFragColor && hasGLFragData));
        if (hasGLFragColor)
        {
            sink << "layout(location = 0) out vec4 webgl_FragColor;\n";
        }
        if (hasGLFragData)
        {
            sink << "layout(location = 0) out vec4 webgl_FragData[gl_MaxDrawBuffers];\n";
        }

        if (usesPointCoord)
        {
            TIntermBinary *viewportYScale =
                CreateDriverUniformRef(driverUniforms, kNegViewportYScale);
            TIntermConstantUnion *pivot = CreateBasicConstant(0.5f);
            FlipBuiltinVariable(root, GetMainSequence(root), viewportYScale, &getSymbolTable(),
                                BuiltInVariable::gl_PointCoord(), kFlippedPointCoordName, pivot);
        }

        if (usesFragCoord)
        {
            InsertFragCoordCorrection(root, GetMainSequence(root), &getSymbolTable(),
                                      driverUniforms);
        }
    }
    else
    {
        ASSERT(getShaderType() == GL_VERTEX_SHADER);

        AddANGLEPositionVarying(root, &getSymbolTable());

        // Append depth range translation to main.
        AppendVertexShaderDepthCorrectionToMain(root, &getSymbolTable());
    }

    // Write translated shader.
    root->traverse(&outputGLSL);
}

bool TranslatorVulkan::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary.
    return false;
}

}  // namespace sh
