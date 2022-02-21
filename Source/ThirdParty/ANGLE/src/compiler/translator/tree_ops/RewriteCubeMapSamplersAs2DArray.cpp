//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteCubeMapSamplersAs2DArray: Change samplerCube samplers to sampler2DArray for seamful cube
// map emulation.
//
// Relies on MonomorphizeUnsupportedFunctions to ensure samplerCube variables are not
// passed to functions (for simplicity).
//

#include "compiler/translator/tree_ops/RewriteCubeMapSamplersAs2DArray.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"

namespace sh
{
namespace
{
constexpr ImmutableString kCoordTransformFuncName("ANGLECubeMapCoordTransform");
constexpr ImmutableString kCoordTransformFuncNameImplicit("ANGLECubeMapCoordTransformImplicit");

TIntermTyped *DerivativeQuotient(TIntermTyped *u,
                                 TIntermTyped *du,
                                 TIntermTyped *v,
                                 TIntermTyped *dv,
                                 TIntermTyped *vRecip)
{
    // (du v - dv u) / v^2
    return new TIntermBinary(
        EOpMul,
        new TIntermBinary(EOpSub, new TIntermBinary(EOpMul, du->deepCopy(), v->deepCopy()),
                          new TIntermBinary(EOpMul, dv->deepCopy(), u->deepCopy())),
        new TIntermBinary(EOpMul, vRecip->deepCopy(), vRecip->deepCopy()));
}

TIntermTyped *Swizzle1(TIntermTyped *array, int i)
{
    return new TIntermSwizzle(array, {i});
}

TIntermTyped *IndexDirect(TIntermTyped *array, int i)
{
    return new TIntermBinary(EOpIndexDirect, array, CreateIndexNode(i));
}

// Generated the common transformation in each coord transformation case.  See comment in
// declareCoordTranslationFunction().  Called with P, dPdx and dPdy.
void TransformXMajor(const TSymbolTable &symbolTable,
                     TIntermBlock *block,
                     TIntermTyped *x,
                     TIntermTyped *y,
                     TIntermTyped *z,
                     TIntermTyped *uc,
                     TIntermTyped *vc)
{
    // uc = -sign(x)*z
    // vc = -y
    TIntermTyped *signX =
        CreateBuiltInUnaryFunctionCallNode("sign", x->deepCopy(), symbolTable, 100);

    TIntermTyped *ucValue =
        new TIntermUnary(EOpNegative, new TIntermBinary(EOpMul, signX, z->deepCopy()), nullptr);
    TIntermTyped *vcValue = new TIntermUnary(EOpNegative, y->deepCopy(), nullptr);

    block->appendStatement(new TIntermBinary(EOpAssign, uc->deepCopy(), ucValue));
    block->appendStatement(new TIntermBinary(EOpAssign, vc->deepCopy(), vcValue));
}

void TransformDerivativeXMajor(TIntermBlock *block,
                               TSymbolTable *symbolTable,
                               TIntermTyped *x,
                               TIntermTyped *y,
                               TIntermTyped *z,
                               TIntermTyped *dx,
                               TIntermTyped *dy,
                               TIntermTyped *dz,
                               TIntermTyped *du,
                               TIntermTyped *dv,
                               TIntermTyped *xRecip)
{
    // Only the magnitude of the derivative matters, so we ignore the sign(x)
    // and the negations.
    TIntermTyped *duValue = DerivativeQuotient(z, dz, x, dx, xRecip);
    TIntermTyped *dvValue = DerivativeQuotient(y, dy, x, dx, xRecip);
    duValue               = new TIntermBinary(EOpMul, duValue, CreateFloatNode(0.5f, EbpMedium));
    dvValue               = new TIntermBinary(EOpMul, dvValue, CreateFloatNode(0.5f, EbpMedium));
    block->appendStatement(new TIntermBinary(EOpAssign, du->deepCopy(), duValue));
    block->appendStatement(new TIntermBinary(EOpAssign, dv->deepCopy(), dvValue));
}

void TransformImplicitDerivativeXMajor(TIntermBlock *block,
                                       TIntermTyped *dOuter,
                                       TIntermTyped *du,
                                       TIntermTyped *dv)
{
    block->appendStatement(
        new TIntermBinary(EOpAssign, du->deepCopy(), Swizzle1(dOuter->deepCopy(), 2)));
    block->appendStatement(
        new TIntermBinary(EOpAssign, dv->deepCopy(), Swizzle1(dOuter->deepCopy(), 1)));
}

void TransformYMajor(const TSymbolTable &symbolTable,
                     TIntermBlock *block,
                     TIntermTyped *x,
                     TIntermTyped *y,
                     TIntermTyped *z,
                     TIntermTyped *uc,
                     TIntermTyped *vc)
{
    // uc = x
    // vc = sign(y)*z
    TIntermTyped *signY =
        CreateBuiltInUnaryFunctionCallNode("sign", y->deepCopy(), symbolTable, 100);

    TIntermTyped *ucValue = x->deepCopy();
    TIntermTyped *vcValue = new TIntermBinary(EOpMul, signY, z->deepCopy());

    block->appendStatement(new TIntermBinary(EOpAssign, uc->deepCopy(), ucValue));
    block->appendStatement(new TIntermBinary(EOpAssign, vc->deepCopy(), vcValue));
}

void TransformDerivativeYMajor(TIntermBlock *block,
                               TSymbolTable *symbolTable,
                               TIntermTyped *x,
                               TIntermTyped *y,
                               TIntermTyped *z,
                               TIntermTyped *dx,
                               TIntermTyped *dy,
                               TIntermTyped *dz,
                               TIntermTyped *du,
                               TIntermTyped *dv,
                               TIntermTyped *yRecip)
{
    // Only the magnitude of the derivative matters, so we ignore the sign(x)
    // and the negations.
    TIntermTyped *duValue = DerivativeQuotient(x, dx, y, dy, yRecip);
    TIntermTyped *dvValue = DerivativeQuotient(z, dz, y, dy, yRecip);
    duValue               = new TIntermBinary(EOpMul, duValue, CreateFloatNode(0.5f, EbpMedium));
    dvValue               = new TIntermBinary(EOpMul, dvValue, CreateFloatNode(0.5f, EbpMedium));
    block->appendStatement(new TIntermBinary(EOpAssign, du->deepCopy(), duValue));
    block->appendStatement(new TIntermBinary(EOpAssign, dv->deepCopy(), dvValue));
}

void TransformImplicitDerivativeYMajor(TIntermBlock *block,
                                       TIntermTyped *dOuter,
                                       TIntermTyped *du,
                                       TIntermTyped *dv)
{
    block->appendStatement(
        new TIntermBinary(EOpAssign, du->deepCopy(), Swizzle1(dOuter->deepCopy(), 0)));
    block->appendStatement(
        new TIntermBinary(EOpAssign, dv->deepCopy(), Swizzle1(dOuter->deepCopy(), 2)));
}

void TransformZMajor(const TSymbolTable &symbolTable,
                     TIntermBlock *block,
                     TIntermTyped *x,
                     TIntermTyped *y,
                     TIntermTyped *z,
                     TIntermTyped *uc,
                     TIntermTyped *vc)
{
    // uc = size(z)*x
    // vc = -y
    TIntermTyped *signZ =
        CreateBuiltInUnaryFunctionCallNode("sign", z->deepCopy(), symbolTable, 100);

    TIntermTyped *ucValue = new TIntermBinary(EOpMul, signZ, x->deepCopy());
    TIntermTyped *vcValue = new TIntermUnary(EOpNegative, y->deepCopy(), nullptr);

    block->appendStatement(new TIntermBinary(EOpAssign, uc->deepCopy(), ucValue));
    block->appendStatement(new TIntermBinary(EOpAssign, vc->deepCopy(), vcValue));
}

void TransformDerivativeZMajor(TIntermBlock *block,
                               TSymbolTable *symbolTable,
                               TIntermTyped *x,
                               TIntermTyped *y,
                               TIntermTyped *z,
                               TIntermTyped *dx,
                               TIntermTyped *dy,
                               TIntermTyped *dz,
                               TIntermTyped *du,
                               TIntermTyped *dv,
                               TIntermTyped *zRecip)
{
    // Only the magnitude of the derivative matters, so we ignore the sign(x)
    // and the negations.
    TIntermTyped *duValue = DerivativeQuotient(x, dx, z, dz, zRecip);
    TIntermTyped *dvValue = DerivativeQuotient(y, dy, z, dz, zRecip);
    duValue               = new TIntermBinary(EOpMul, duValue, CreateFloatNode(0.5f, EbpMedium));
    dvValue               = new TIntermBinary(EOpMul, dvValue, CreateFloatNode(0.5f, EbpMedium));
    block->appendStatement(new TIntermBinary(EOpAssign, du->deepCopy(), duValue));
    block->appendStatement(new TIntermBinary(EOpAssign, dv->deepCopy(), dvValue));
}

void TransformImplicitDerivativeZMajor(TIntermBlock *block,
                                       TIntermTyped *dOuter,
                                       TIntermTyped *du,
                                       TIntermTyped *dv)
{
    block->appendStatement(
        new TIntermBinary(EOpAssign, du->deepCopy(), Swizzle1(dOuter->deepCopy(), 0)));
    block->appendStatement(
        new TIntermBinary(EOpAssign, dv->deepCopy(), Swizzle1(dOuter->deepCopy(), 1)));
}

class RewriteCubeMapSamplersAs2DArrayTraverser : public TIntermTraverser
{
  public:
    RewriteCubeMapSamplersAs2DArrayTraverser(TSymbolTable *symbolTable, bool isFragmentShader)
        : TIntermTraverser(true, false, false, symbolTable),
          mCubeXYZToArrayUVL(nullptr),
          mCubeXYZToArrayUVLImplicit(nullptr),
          mIsFragmentShader(isFragmentShader),
          mCoordTranslationFunctionDecl(nullptr),
          mCoordTranslationFunctionImplicitDecl(nullptr)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();
        bool isSamplerCube     = type.getQualifier() == EvqUniform && type.isSamplerCube();

        if (isSamplerCube)
        {
            // Samplers cannot have initializers, so the declaration must necessarily be a symbol.
            TIntermSymbol *samplerVariable = variable->getAsSymbolNode();
            ASSERT(samplerVariable != nullptr);

            declareSampler2DArray(&samplerVariable->variable(), node);
            return false;
        }

        return true;
    }

    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        if (BuiltInGroup::IsBuiltIn(node->getOp()))
        {
            bool converted = convertBuiltinFunction(node);
            return !converted;
        }

        // AST functions don't require modification as samplerCube function parameters are removed
        // by MonomorphizeUnsupportedFunctions.
        return true;
    }

    TIntermFunctionDefinition *getCoordTranslationFunctionDecl()
    {
        return mCoordTranslationFunctionDecl;
    }

    TIntermFunctionDefinition *getCoordTranslationFunctionDeclImplicit()
    {
        return mCoordTranslationFunctionImplicitDecl;
    }

  private:
    void declareSampler2DArray(const TVariable *samplerCubeVar, TIntermDeclaration *node)
    {
        if (mCubeXYZToArrayUVL == nullptr)
        {
            // If not done yet, declare the function that transforms cube map texture sampling
            // coordinates to face index and uv coordinates.
            declareCoordTranslationFunction(false, kCoordTransformFuncName, &mCubeXYZToArrayUVL,
                                            &mCoordTranslationFunctionDecl);
        }
        if (mCubeXYZToArrayUVLImplicit == nullptr && mIsFragmentShader)
        {
            declareCoordTranslationFunction(true, kCoordTransformFuncNameImplicit,
                                            &mCubeXYZToArrayUVLImplicit,
                                            &mCoordTranslationFunctionImplicitDecl);
        }

        TType *newType = new TType(samplerCubeVar->getType());
        newType->setBasicType(EbtSampler2DArray);

        TVariable *sampler2DArrayVar = new TVariable(mSymbolTable, samplerCubeVar->name(), newType,
                                                     samplerCubeVar->symbolType());

        TIntermDeclaration *sampler2DArrayDecl = new TIntermDeclaration();
        sampler2DArrayDecl->appendDeclarator(new TIntermSymbol(sampler2DArrayVar));

        queueReplacement(sampler2DArrayDecl, OriginalNode::IS_DROPPED);

        // Remember the sampler2DArray variable.
        mSamplerMap[samplerCubeVar] = sampler2DArrayVar;
    }

    void declareCoordTranslationFunction(bool implicit,
                                         const ImmutableString &name,
                                         TFunction **functionOut,
                                         TIntermFunctionDefinition **declOut)
    {
        // GLES2.0 (as well as desktop OpenGL 2.0) define the coordination transformation as
        // follows.  Given xyz cube coordinates, where each channel is in [-1, 1], the following
        // table calculates uc, vc and ma as well as the cube map face.
        //
        //    Major    Axis Direction Target     uc  vc  ma
        //     +x   TEXTURE_CUBE_MAP_POSITIVE_X  -z  -y  |x|
        //     -x   TEXTURE_CUBE_MAP_NEGATIVE_X   z  -y  |x|
        //     +y   TEXTURE_CUBE_MAP_POSITIVE_Y   x   z  |y|
        //     -y   TEXTURE_CUBE_MAP_NEGATIVE_Y   x  -z  |y|
        //     +z   TEXTURE_CUBE_MAP_POSITIVE_Z   x  -y  |z|
        //     -z   TEXTURE_CUBE_MAP_NEGATIVE_Z  -x  -y  |z|
        //
        // "Major" is an indication of the axis with the largest value.  The cube map face indicates
        // the layer to sample from.  The uv coordinates to sample from are calculated as,
        // effectively transforming the uv values to [0, 1]:
        //
        //     u = (1 + uc/ma) / 2
        //     v = (1 + vc/ma) / 2
        //
        // The function can be implemented as 6 ifs, though it would be far from efficient.  The
        // following calculations implement the table above in a smaller number of instructions.
        //
        // First, ma can be calculated as the max of the three axes.
        //
        //     ma = max3(|x|, |y|, |z|)
        //
        // We have three cases:
        //
        //     ma == |x|:      uc = -sign(x)*z
        //                     vc = -y
        //                  layer = float(x < 0)
        //
        //     ma == |y|:      uc = x
        //                     vc = sign(y)*z
        //                  layer = 2 + float(y < 0)
        //
        //     ma == |z|:      uc = size(z)*x
        //                     vc = -y
        //                  layer = 4 + float(z < 0)
        //
        // This can be implemented with a number of ?: instructions or 3 ifs. ?: would require all
        // expressions to be evaluated (vector ALU) while if would require exec mask and jumps
        // (scalar operations).  We implement this using ifs as there would otherwise be many vector
        // operations and not much of anything else.
        //
        // If textureCubeGrad is used, we also need to transform the provided dPdx and dPdy (both
        // vec3) to a dUVdx and dUVdy.  Assume P=(r,s,t) and we are investigating dx (note the
        // change from xyz to rst to not confuse with dx and dy):
        //
        //     uv = (f(r,s,t)/ma + 1)/2
        //
        // Where f is one of the transformations above for uc and vc.  Between two neighbors along
        // the x axis, we have P0=(r0,s0,t0) and P1=(r1,s1,t1)
        //
        //     dP = (r1-r0, s1-s0, t1-t0)
        //     dUV = (f(r1,s1,t1)/ma1 - g(r0,s0,t0)/ma0) / 2
        //
        // f and g may not necessarily be the same because the two points may have different major
        // axes.  Even with the same major access, the sign that's used in the formulas may not be
        // the same.  Furthermore, ma0 and ma1 may not be the same.  This makes it impossible to
        // derive dUV from dP exactly.
        //
        // However, gradient transformation is implementation dependant, so we will simplify and
        // assume all the above complications are non-existent.  We therefore have:
        //
        //      dUV = (f(r1,s1,t1)/ma0 - f(r0,s0,t0)/ma0)/2
        //
        // Given that we assumed the sign functions are returning identical results for the two
        // points, f becomes a linear transformation.  Thus:
        //
        //      dUV = f(r1-r0,s1-0,t1-t0)/ma0/2
        //
        // In other words, we use the same formulae that transform XYZ (RST here) to UV to
        // transform the derivatives.
        //
        //     ma == |x|:    dUdx = -sign(x)*dPdx.z / ma / 2
        //                   dVdx = -dPdx.y / ma / 2
        //
        //     ma == |y|:    dUdx = dPdx.x / ma / 2
        //                   dVdx = sign(y)*dPdx.z / ma / 2
        //
        //     ma == |z|:    dUdx = size(z)*dPdx.x / ma / 2
        //                   dVdx = -dPdx.y / ma / 2
        //
        // Similarly for dy.

        // Create the function parameters: vec3 P, vec3 dPdx, vec3 dPdy,
        //                                 out vec2 dUVdx, out vec2 dUVdy
        const TType *vec3Type = StaticType::GetBasic<EbtFloat, EbpHigh, 3>();
        TType *inVec3Type     = new TType(*vec3Type);
        inVec3Type->setQualifier(EvqParamIn);

        TVariable *pVar    = new TVariable(mSymbolTable, ImmutableString("P"), inVec3Type,
                                        SymbolType::AngleInternal);
        TVariable *dPdxVar = new TVariable(mSymbolTable, ImmutableString("dPdx"), inVec3Type,
                                           SymbolType::AngleInternal);
        TVariable *dPdyVar = new TVariable(mSymbolTable, ImmutableString("dPdy"), inVec3Type,
                                           SymbolType::AngleInternal);

        const TType *vec2Type = StaticType::GetBasic<EbtFloat, EbpHigh, 2>();
        TType *outVec2Type    = new TType(*vec2Type);
        outVec2Type->setQualifier(EvqParamOut);

        TVariable *dUVdxVar = new TVariable(mSymbolTable, ImmutableString("dUVdx"), outVec2Type,
                                            SymbolType::AngleInternal);
        TVariable *dUVdyVar = new TVariable(mSymbolTable, ImmutableString("dUVdy"), outVec2Type,
                                            SymbolType::AngleInternal);

        TIntermSymbol *p     = new TIntermSymbol(pVar);
        TIntermSymbol *dPdx  = new TIntermSymbol(dPdxVar);
        TIntermSymbol *dPdy  = new TIntermSymbol(dPdyVar);
        TIntermSymbol *dUVdx = new TIntermSymbol(dUVdxVar);
        TIntermSymbol *dUVdy = new TIntermSymbol(dUVdyVar);

        // Create the function body as statements are generated.
        TIntermBlock *body = new TIntermBlock;

        // Create the swizzle nodes that will be used in multiple expressions:
        TIntermSwizzle *x = new TIntermSwizzle(p->deepCopy(), {0});
        TIntermSwizzle *y = new TIntermSwizzle(p->deepCopy(), {1});
        TIntermSwizzle *z = new TIntermSwizzle(p->deepCopy(), {2});

        // Create abs and "< 0" expressions from the channels.
        const TType *floatType = StaticType::GetBasic<EbtFloat, EbpHigh>();

        TIntermTyped *isNegX = new TIntermBinary(EOpLessThan, x, CreateZeroNode(*floatType));
        TIntermTyped *isNegY = new TIntermBinary(EOpLessThan, y, CreateZeroNode(*floatType));
        TIntermTyped *isNegZ = new TIntermBinary(EOpLessThan, z, CreateZeroNode(*floatType));

        TIntermSymbol *absX = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *absY = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *absZ = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));

        TIntermDeclaration *absXDecl = CreateTempInitDeclarationNode(
            &absX->variable(),
            CreateBuiltInUnaryFunctionCallNode("abs", x->deepCopy(), *mSymbolTable, 100));
        TIntermDeclaration *absYDecl = CreateTempInitDeclarationNode(
            &absY->variable(),
            CreateBuiltInUnaryFunctionCallNode("abs", y->deepCopy(), *mSymbolTable, 100));
        TIntermDeclaration *absZDecl = CreateTempInitDeclarationNode(
            &absZ->variable(),
            CreateBuiltInUnaryFunctionCallNode("abs", z->deepCopy(), *mSymbolTable, 100));

        body->appendStatement(absXDecl);
        body->appendStatement(absYDecl);
        body->appendStatement(absZDecl);

        // Create temporary variable for division outer product matrix and its
        // derivatives.
        // recipOuter[i][j] = 0.5 * P[j] / P[i]
        const TType *mat3Type     = StaticType::GetBasic<EbtFloat, EbpHigh, 3, 3>();
        TIntermSymbol *recipOuter = new TIntermSymbol(CreateTempVariable(mSymbolTable, mat3Type));

        TIntermTyped *pRecip =
            new TIntermBinary(EOpDiv, CreateFloatNode(1.0, EbpMedium), p->deepCopy());
        TIntermSymbol *pRecipVar = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));

        body->appendStatement(CreateTempInitDeclarationNode(&pRecipVar->variable(), pRecip));

        TIntermSequence args = {
            p->deepCopy(), new TIntermBinary(EOpVectorTimesScalar, CreateFloatNode(0.5, EbpMedium),
                                             pRecipVar->deepCopy())};
        TIntermDeclaration *recipOuterDecl = CreateTempInitDeclarationNode(
            &recipOuter->variable(),
            CreateBuiltInFunctionCallNode("outerProduct", &args, *mSymbolTable, 300));
        body->appendStatement(recipOuterDecl);

        TIntermSymbol *dPDXdx = nullptr;
        TIntermSymbol *dPDYdx = nullptr;
        TIntermSymbol *dPDZdx = nullptr;
        TIntermSymbol *dPDXdy = nullptr;
        TIntermSymbol *dPDYdy = nullptr;
        TIntermSymbol *dPDZdy = nullptr;
        if (implicit)
        {
            dPDXdx = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));
            dPDYdx = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));
            dPDZdx = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));
            dPDXdy = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));
            dPDYdy = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));
            dPDZdy = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));

            TIntermDeclaration *dPDXdxDecl = CreateTempInitDeclarationNode(
                &dPDXdx->variable(),
                CreateBuiltInUnaryFunctionCallNode("dFdx", IndexDirect(recipOuter, 0)->deepCopy(),
                                                   *mSymbolTable, 300));
            TIntermDeclaration *dPDYdxDecl = CreateTempInitDeclarationNode(
                &dPDYdx->variable(),
                CreateBuiltInUnaryFunctionCallNode("dFdx", IndexDirect(recipOuter, 1)->deepCopy(),
                                                   *mSymbolTable, 300));
            TIntermDeclaration *dPDZdxDecl = CreateTempInitDeclarationNode(
                &dPDZdx->variable(),
                CreateBuiltInUnaryFunctionCallNode("dFdx", IndexDirect(recipOuter, 2)->deepCopy(),
                                                   *mSymbolTable, 300));
            TIntermDeclaration *dPDXdyDecl = CreateTempInitDeclarationNode(
                &dPDXdy->variable(),
                CreateBuiltInUnaryFunctionCallNode("dFdy", IndexDirect(recipOuter, 0)->deepCopy(),
                                                   *mSymbolTable, 300));
            TIntermDeclaration *dPDYdyDecl = CreateTempInitDeclarationNode(
                &dPDYdy->variable(),
                CreateBuiltInUnaryFunctionCallNode("dFdy", IndexDirect(recipOuter, 1)->deepCopy(),
                                                   *mSymbolTable, 300));
            TIntermDeclaration *dPDZdyDecl = CreateTempInitDeclarationNode(
                &dPDZdy->variable(),
                CreateBuiltInUnaryFunctionCallNode("dFdy", IndexDirect(recipOuter, 2)->deepCopy(),
                                                   *mSymbolTable, 300));

            body->appendStatement(dPDXdxDecl);
            body->appendStatement(dPDYdxDecl);
            body->appendStatement(dPDZdxDecl);
            body->appendStatement(dPDXdyDecl);
            body->appendStatement(dPDYdyDecl);
            body->appendStatement(dPDZdyDecl);
        }

        // Create temporary variables for ma, uc, vc, and l (layer), as well as dUdx, dVdx, dUdy
        // and dVdy.
        TIntermSymbol *ma   = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *l    = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *uc   = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *vc   = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *dUdx = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *dVdx = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *dUdy = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
        TIntermSymbol *dVdy = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));

        body->appendStatement(CreateTempDeclarationNode(&ma->variable()));
        body->appendStatement(CreateTempDeclarationNode(&l->variable()));
        body->appendStatement(CreateTempDeclarationNode(&uc->variable()));
        body->appendStatement(CreateTempDeclarationNode(&vc->variable()));
        body->appendStatement(CreateTempDeclarationNode(&dUdx->variable()));
        body->appendStatement(CreateTempDeclarationNode(&dVdx->variable()));
        body->appendStatement(CreateTempDeclarationNode(&dUdy->variable()));
        body->appendStatement(CreateTempDeclarationNode(&dVdy->variable()));

        // ma = max(|x|, max(|y|, |z|))
        TIntermSequence argsMaxYZ = {absY->deepCopy(), absZ->deepCopy()};
        TIntermTyped *maxYZ = CreateBuiltInFunctionCallNode("max", &argsMaxYZ, *mSymbolTable, 100);
        TIntermSequence argsMaxValue = {absX->deepCopy(), maxYZ};
        TIntermTyped *maValue =
            CreateBuiltInFunctionCallNode("max", &argsMaxValue, *mSymbolTable, 100);
        body->appendStatement(new TIntermBinary(EOpAssign, ma, maValue));

        // ma == |x| and ma == |y| expressions
        TIntermTyped *isXMajor = new TIntermBinary(EOpEqual, ma->deepCopy(), absX->deepCopy());
        TIntermTyped *isYMajor = new TIntermBinary(EOpEqual, ma->deepCopy(), absY->deepCopy());

        // Determine the cube face:

        // The case where x is major:
        //     layer = float(x < 0)
        TIntermSequence argsNegX = {isNegX};
        TIntermTyped *xl         = TIntermAggregate::CreateConstructor(*floatType, &argsNegX);

        TIntermBlock *calculateXL = new TIntermBlock;
        calculateXL->appendStatement(new TIntermBinary(EOpAssign, l->deepCopy(), xl));

        // The case where y is major:
        //     layer = 2 + float(y < 0)
        TIntermSequence argsNegY = {isNegY};
        TIntermTyped *yl =
            new TIntermBinary(EOpAdd, CreateFloatNode(2.0f, EbpMedium),
                              TIntermAggregate::CreateConstructor(*floatType, &argsNegY));

        TIntermBlock *calculateYL = new TIntermBlock;
        calculateYL->appendStatement(new TIntermBinary(EOpAssign, l->deepCopy(), yl));

        // The case where z is major:
        //     layer = 4 + float(z < 0)
        TIntermSequence argsNegZ = {isNegZ};
        TIntermTyped *zl =
            new TIntermBinary(EOpAdd, CreateFloatNode(4.0f, EbpMedium),
                              TIntermAggregate::CreateConstructor(*floatType, &argsNegZ));

        TIntermBlock *calculateZL = new TIntermBlock;
        calculateZL->appendStatement(new TIntermBinary(EOpAssign, l->deepCopy(), zl));

        // Create the if-else paths:
        TIntermIfElse *calculateYZL     = new TIntermIfElse(isYMajor, calculateYL, calculateZL);
        TIntermBlock *calculateYZLBlock = new TIntermBlock;
        calculateYZLBlock->appendStatement(calculateYZL);
        TIntermIfElse *calculateXYZL = new TIntermIfElse(isXMajor, calculateXL, calculateYZLBlock);
        body->appendStatement(calculateXYZL);

        // layer < 1.5 (covering faces 0 and 1, corresponding to major axis being X) and layer < 3.5
        // (covering faces 2 and 3, corresponding to major axis being Y).  Used to determine which
        // of the three transformations to apply.  Previously, ma == |X| and ma == |Y| was used,
        // which is no longer correct for helper invocations.  The value of ma is updated in each
        // case for these invocations.
        isXMajor = new TIntermBinary(EOpLessThan, l->deepCopy(), CreateFloatNode(1.5f, EbpMedium));
        isYMajor = new TIntermBinary(EOpLessThan, l->deepCopy(), CreateFloatNode(3.5f, EbpMedium));

        TIntermSwizzle *dPdxX = new TIntermSwizzle(dPdx->deepCopy(), {0});
        TIntermSwizzle *dPdxY = new TIntermSwizzle(dPdx->deepCopy(), {1});
        TIntermSwizzle *dPdxZ = new TIntermSwizzle(dPdx->deepCopy(), {2});

        TIntermSwizzle *dPdyX = new TIntermSwizzle(dPdy->deepCopy(), {0});
        TIntermSwizzle *dPdyY = new TIntermSwizzle(dPdy->deepCopy(), {1});
        TIntermSwizzle *dPdyZ = new TIntermSwizzle(dPdy->deepCopy(), {2});

        TIntermBlock *calculateXUcVc = new TIntermBlock;
        calculateXUcVc->appendStatement(
            new TIntermBinary(EOpAssign, ma->deepCopy(), absX->deepCopy()));
        TransformXMajor(*mSymbolTable, calculateXUcVc, x, y, z, uc, vc);

        TIntermBlock *calculateYUcVc = new TIntermBlock;
        calculateYUcVc->appendStatement(
            new TIntermBinary(EOpAssign, ma->deepCopy(), absY->deepCopy()));
        TransformYMajor(*mSymbolTable, calculateYUcVc, x, y, z, uc, vc);

        TIntermBlock *calculateZUcVc = new TIntermBlock;
        calculateZUcVc->appendStatement(
            new TIntermBinary(EOpAssign, ma->deepCopy(), absZ->deepCopy()));
        TransformZMajor(*mSymbolTable, calculateZUcVc, x, y, z, uc, vc);

        // Compute derivatives.
        if (implicit)
        {
            TransformImplicitDerivativeXMajor(calculateXUcVc, dPDXdx, dUdx, dVdx);
            TransformImplicitDerivativeXMajor(calculateXUcVc, dPDXdy, dUdy, dVdy);
            TransformImplicitDerivativeYMajor(calculateYUcVc, dPDYdx, dUdx, dVdx);
            TransformImplicitDerivativeYMajor(calculateYUcVc, dPDYdy, dUdy, dVdy);
            TransformImplicitDerivativeZMajor(calculateZUcVc, dPDZdx, dUdx, dVdx);
            TransformImplicitDerivativeZMajor(calculateZUcVc, dPDZdy, dUdy, dVdy);
        }
        else
        {
            TransformDerivativeXMajor(calculateXUcVc, mSymbolTable, x, y, z, dPdxX, dPdxY, dPdxZ,
                                      dUdx, dVdx, Swizzle1(pRecipVar->deepCopy(), 0));
            TransformDerivativeXMajor(calculateXUcVc, mSymbolTable, x, y, z, dPdyX, dPdyY, dPdyZ,
                                      dUdy, dVdy, Swizzle1(pRecipVar->deepCopy(), 0));
            TransformDerivativeYMajor(calculateYUcVc, mSymbolTable, x, y, z, dPdxX, dPdxY, dPdxZ,
                                      dUdx, dVdx, Swizzle1(pRecipVar->deepCopy(), 1));
            TransformDerivativeYMajor(calculateYUcVc, mSymbolTable, x, y, z, dPdyX, dPdyY, dPdyZ,
                                      dUdy, dVdy, Swizzle1(pRecipVar->deepCopy(), 1));
            TransformDerivativeZMajor(calculateZUcVc, mSymbolTable, x, y, z, dPdxX, dPdxY, dPdxZ,
                                      dUdx, dVdx, Swizzle1(pRecipVar->deepCopy(), 2));
            TransformDerivativeZMajor(calculateZUcVc, mSymbolTable, x, y, z, dPdyX, dPdyY, dPdyZ,
                                      dUdy, dVdy, Swizzle1(pRecipVar->deepCopy(), 2));
        }

        // Create the if-else paths:
        TIntermIfElse *calculateYZUcVc =
            new TIntermIfElse(isYMajor, calculateYUcVc, calculateZUcVc);
        TIntermBlock *calculateYZUcVcBlock = new TIntermBlock;
        calculateYZUcVcBlock->appendStatement(calculateYZUcVc);
        TIntermIfElse *calculateXYZUcVc =
            new TIntermIfElse(isXMajor, calculateXUcVc, calculateYZUcVcBlock);
        body->appendStatement(calculateXYZUcVc);

        // u = (1 + uc/|ma|) / 2
        // v = (1 + vc/|ma|) / 2
        TIntermTyped *maTimesTwoRecip = new TIntermBinary(
            EOpAssign, ma->deepCopy(),
            new TIntermBinary(EOpDiv, CreateFloatNode(0.5f, EbpMedium), ma->deepCopy()));
        body->appendStatement(maTimesTwoRecip);

        TIntermTyped *ucDivMa = new TIntermBinary(EOpMul, uc, ma->deepCopy());
        TIntermTyped *vcDivMa = new TIntermBinary(EOpMul, vc, ma->deepCopy());
        TIntermTyped *uNormalized =
            new TIntermBinary(EOpAdd, CreateFloatNode(0.5f, EbpMedium), ucDivMa);
        TIntermTyped *vNormalized =
            new TIntermBinary(EOpAdd, CreateFloatNode(0.5f, EbpMedium), vcDivMa);

        body->appendStatement(new TIntermBinary(EOpAssign, uc->deepCopy(), uNormalized));
        body->appendStatement(new TIntermBinary(EOpAssign, vc->deepCopy(), vNormalized));

        TIntermSequence argsDUVdx = {dUdx, dVdx};
        TIntermTyped *dUVdxValue  = TIntermAggregate::CreateConstructor(*vec2Type, &argsDUVdx);

        TIntermSequence argsDUVdy = {dUdy, dVdy};
        TIntermTyped *dUVdyValue  = TIntermAggregate::CreateConstructor(*vec2Type, &argsDUVdy);

        body->appendStatement(new TIntermBinary(EOpAssign, dUVdx, dUVdxValue));
        body->appendStatement(new TIntermBinary(EOpAssign, dUVdy, dUVdyValue));

        // return vec3(u, v, l)
        TIntermSequence argsUVL = {uc->deepCopy(), vc->deepCopy(), l};
        TIntermBranch *returnStatement =
            new TIntermBranch(EOpReturn, TIntermAggregate::CreateConstructor(*vec3Type, &argsUVL));
        body->appendStatement(returnStatement);

        TFunction *function;
        function = new TFunction(mSymbolTable, name, SymbolType::AngleInternal, vec3Type, true);
        function->addParameter(pVar);
        function->addParameter(dPdxVar);
        function->addParameter(dPdyVar);
        function->addParameter(dUVdxVar);
        function->addParameter(dUVdyVar);

        *functionOut = function;

        *declOut = CreateInternalFunctionDefinitionNode(*function, body);
    }

    TIntermTyped *createCoordTransformationCall(TIntermTyped *P,
                                                TIntermTyped *dPdx,
                                                TIntermTyped *dPdy,
                                                TIntermTyped *dUVdx,
                                                TIntermTyped *dUVdy)
    {
        TIntermSequence args = {P, dPdx, dPdy, dUVdx, dUVdy};
        return TIntermAggregate::CreateFunctionCall(*mCubeXYZToArrayUVL, &args);
    }

    TIntermTyped *createImplicitCoordTransformationCall(TIntermTyped *P,
                                                        TIntermTyped *dUVdx,
                                                        TIntermTyped *dUVdy)
    {
        const TType *vec3Type = StaticType::GetBasic<EbtFloat, EbpHigh, 3>();
        TIntermTyped *dPdx    = CreateZeroNode(*vec3Type);
        TIntermTyped *dPdy    = CreateZeroNode(*vec3Type);
        TIntermSequence args  = {P, dPdx, dPdy, dUVdx, dUVdy};
        return TIntermAggregate::CreateFunctionCall(*mCubeXYZToArrayUVLImplicit, &args);
    }

    TIntermTyped *getMappedSamplerExpression(TIntermNode *samplerCubeExpression)
    {
        // The argument passed to a function can either be the sampler, if not array, or a subscript
        // into the sampler array.
        TIntermSymbol *asSymbol = samplerCubeExpression->getAsSymbolNode();
        TIntermBinary *asBinary = samplerCubeExpression->getAsBinaryNode();

        if (asBinary)
        {
            // Only constant indexing is supported in ES2.0.
            ASSERT(asBinary->getOp() == EOpIndexDirect);
            asSymbol = asBinary->getLeft()->getAsSymbolNode();
        }

        // Arrays of arrays are not available in ES2.0.
        ASSERT(asSymbol != nullptr);
        const TVariable *samplerCubeVar = &asSymbol->variable();

        ASSERT(mSamplerMap.find(samplerCubeVar) != mSamplerMap.end());
        const TVariable *mappedSamplerVar = mSamplerMap.at(samplerCubeVar);

        TIntermTyped *mappedExpression = new TIntermSymbol(mappedSamplerVar);
        if (asBinary)
        {
            mappedExpression =
                new TIntermBinary(asBinary->getOp(), mappedExpression, asBinary->getRight());
        }

        return mappedExpression;
    }

    bool convertBuiltinFunction(TIntermAggregate *node)
    {
        const TFunction *function = node->getFunction();
        if (!function->name().beginsWith("textureCube"))
        {
            return false;
        }

        // All textureCube* functions are in the form:
        //
        //     textureCube??(samplerCube, vec3, ??)
        //
        // They should be converted to:
        //
        //     texture??(sampler2DArray, convertCoords(vec3), ??)
        //
        // We assume the target platform supports texture() functions (currently only used in
        // Vulkan).
        //
        // The intrinsics map as follows:
        //
        //     textureCube -> textureGrad
        //     textureCubeLod -> textureLod
        //     textureCubeLodEXT -> textureLod
        //     textureCubeGrad -> textureGrad
        //     textureCubeGradEXT -> textureGrad
        //
        // Note that dPdx and dPdy in textureCubeGrad* are vec3, while the textureGrad equivalent
        // for sampler2DArray is vec2.  The EXT_shader_texture_lod that introduces thid function
        // says:
        //
        // > For the "Grad" functions, dPdx is the explicit derivative of P with respect
        // > to window x, and similarly dPdy with respect to window y. ...  For a cube map texture,
        // > dPdx and dPdy are vec3.
        // >
        // > Let
        // >
        // >     dSdx = dPdx.s;
        // >     dSdy = dPdy.s;
        // >     dTdx = dPdx.t;
        // >     dTdy = dPdy.t;
        // >
        // > and
        // >
        // >             / 0.0;    for two-dimensional texture
        // >     dRdx = (
        // >             \ dPdx.p; for cube map texture
        // >
        // >             / 0.0;    for two-dimensional texture
        // >     dRdy = (
        // >             \ dPdy.p; for cube map texture
        // >
        // > (See equation 3.12a in The OpenGL ES 2.0 Specification.)
        //
        // It's unclear to me what dRdx and dRdy are.  EXT_gpu_shader4 that promotes this function
        // has the following additional information:
        //
        // > For the "Cube" versions, the partial
        // > derivatives ddx and ddy are assumed to be in the coordinate system used
        // > before texture coordinates are projected onto the appropriate cube
        // > face. The partial derivatives of the post-projection texture coordinates,
        // > which are used for level-of-detail and anisotropic filtering
        // > calculations, are derived from coord, ddx and ddy in an
        // > implementation-dependent manner.
        //
        // The calculation of dPdx and dPdy is declared as implementation-dependent, so we have
        // freedom to calculate it as fit, even if not precisely the same as hardware might.

        const char *substituteFunctionName = "textureGrad";
        bool isGrad                        = false;
        bool isTranslatedGrad              = true;
        bool hasBias                       = false;
        if (function->name().beginsWith("textureCubeLod"))
        {
            substituteFunctionName = "textureLod";
            isTranslatedGrad       = false;
        }
        else if (function->name().beginsWith("textureCubeGrad"))
        {
            isGrad = true;
        }
        else if (!mIsFragmentShader)
        {
            substituteFunctionName = "texture";
            isTranslatedGrad       = false;
        }

        TIntermSequence *arguments = node->getSequence();
        ASSERT(arguments->size() >= 2);

        const TType *vec2Type = StaticType::GetBasic<EbtFloat, EbpHigh, 2>();
        const TType *vec3Type = StaticType::GetBasic<EbtFloat, EbpHigh, 3>();
        TIntermSymbol *uvl    = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec3Type));
        TIntermSymbol *dUVdx  = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec2Type));
        TIntermSymbol *dUVdy  = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec2Type));

        TIntermTyped *dPdx = nullptr;
        TIntermTyped *dPdy = nullptr;
        if (isGrad)
        {
            ASSERT(arguments->size() == 4);
            dPdx = (*arguments)[2]->getAsTyped()->deepCopy();
            dPdy = (*arguments)[3]->getAsTyped()->deepCopy();
        }
        else if (isTranslatedGrad && mIsFragmentShader && arguments->size() == 3)
        {
            hasBias = true;
        }
        else
        {
            dPdx = CreateZeroNode(*vec3Type);
            dPdy = CreateZeroNode(*vec3Type);
        }

        if (isTranslatedGrad && !mIsFragmentShader)
        {
            substituteFunctionName = "texture";
            isTranslatedGrad       = false;
        }

        // The function call to transform the coordinates, dPdx and dPdy.  If not textureCubeGrad,
        // the driver compiler will optimize out the unnecessary calculations.
        TIntermSequence coordTransform;
        coordTransform.push_back(CreateTempDeclarationNode(&dUVdx->variable()));
        coordTransform.push_back(CreateTempDeclarationNode(&dUVdy->variable()));
        TIntermTyped *coordTransformCall;
        if (isGrad || !isTranslatedGrad)
        {
            coordTransformCall = createCoordTransformationCall(
                (*arguments)[1]->getAsTyped()->deepCopy(), dPdx, dPdy, dUVdx, dUVdy);
        }
        else
        {
            coordTransformCall = createImplicitCoordTransformationCall(
                (*arguments)[1]->getAsTyped()->deepCopy(), dUVdx, dUVdy);
        }
        coordTransform.push_back(
            CreateTempInitDeclarationNode(&uvl->variable(), coordTransformCall));

        TIntermTyped *dUVdxArg = dUVdx;
        TIntermTyped *dUVdyArg = dUVdy;
        if (hasBias)
        {
            const TType *floatType   = StaticType::GetBasic<EbtFloat, EbpHigh>();
            TIntermTyped *bias       = (*arguments)[2]->getAsTyped()->deepCopy();
            TIntermSequence exp2Args = {bias};
            TIntermTyped *exp2Call =
                CreateBuiltInFunctionCallNode("exp2", &exp2Args, *mSymbolTable, 100);
            TIntermSymbol *biasFac = new TIntermSymbol(CreateTempVariable(mSymbolTable, floatType));
            coordTransform.push_back(CreateTempInitDeclarationNode(&biasFac->variable(), exp2Call));
            dUVdxArg =
                new TIntermBinary(EOpVectorTimesScalar, biasFac->deepCopy(), dUVdx->deepCopy());
            dUVdyArg =
                new TIntermBinary(EOpVectorTimesScalar, biasFac->deepCopy(), dUVdy->deepCopy());
        }

        insertStatementsInParentBlock(coordTransform);

        TIntermSequence substituteArguments;
        // Replace the first argument (samplerCube) with the sampler2DArray.
        substituteArguments.push_back(getMappedSamplerExpression((*arguments)[0]));
        // Replace the second argument with the coordination transformation.
        substituteArguments.push_back(uvl->deepCopy());
        if (isTranslatedGrad)
        {
            substituteArguments.push_back(dUVdxArg->deepCopy());
            substituteArguments.push_back(dUVdyArg->deepCopy());
        }
        else
        {
            // Pass the rest of the parameters as is.
            for (size_t argIndex = 2; argIndex < arguments->size(); ++argIndex)
            {
                substituteArguments.push_back((*arguments)[argIndex]->getAsTyped()->deepCopy());
            }
        }

        TIntermTyped *substituteCall = CreateBuiltInFunctionCallNode(
            substituteFunctionName, &substituteArguments, *mSymbolTable, 300);

        queueReplacement(substituteCall, OriginalNode::IS_DROPPED);

        return true;
    }

    // A map from the samplerCube variable to the sampler2DArray one.
    angle::HashMap<const TVariable *, const TVariable *> mSamplerMap;

    // A helper function to convert xyz coordinates passed to a cube map sampling function into the
    // array layer (cube map face) and uv coordinates.
    TFunction *mCubeXYZToArrayUVL;
    // A specialized version of the same function which uses implicit derivatives.
    TFunction *mCubeXYZToArrayUVLImplicit;

    bool mIsFragmentShader;

    // Stored to be put before the first function after the pass.
    TIntermFunctionDefinition *mCoordTranslationFunctionDecl;
    TIntermFunctionDefinition *mCoordTranslationFunctionImplicitDecl;
};
}  // anonymous namespace

bool RewriteCubeMapSamplersAs2DArray(TCompiler *compiler,
                                     TIntermBlock *root,
                                     TSymbolTable *symbolTable,
                                     bool isFragmentShader)
{
    RewriteCubeMapSamplersAs2DArrayTraverser traverser(symbolTable, isFragmentShader);
    root->traverse(&traverser);

    TIntermFunctionDefinition *coordTranslationFunctionDecl =
        traverser.getCoordTranslationFunctionDecl();
    TIntermFunctionDefinition *coordTranslationFunctionDeclImplicit =
        traverser.getCoordTranslationFunctionDeclImplicit();
    size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
    if (coordTranslationFunctionDecl)
    {
        root->insertChildNodes(firstFunctionIndex, TIntermSequence({coordTranslationFunctionDecl}));
    }
    if (coordTranslationFunctionDeclImplicit)
    {
        root->insertChildNodes(firstFunctionIndex,
                               TIntermSequence({coordTranslationFunctionDeclImplicit}));
    }

    return traverser.updateTree(compiler, root);
}

}  // namespace sh
