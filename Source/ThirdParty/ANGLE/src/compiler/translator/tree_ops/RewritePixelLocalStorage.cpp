//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/tree_ops/RewritePixelLocalStorage.h"

#include "common/angleutils.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_ops/MonomorphizeUnsupportedFunctions.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

constexpr static TBasicType DataTypeOfPLSType(TBasicType plsType)
{
    switch (plsType)
    {
        case EbtPixelLocalANGLE:
            return EbtFloat;
        case EbtIPixelLocalANGLE:
            return EbtInt;
        case EbtUPixelLocalANGLE:
            return EbtUInt;
        default:
            UNREACHABLE();
            return EbtVoid;
    }
}

constexpr static TBasicType DataTypeOfImageType(TBasicType imageType)
{
    switch (imageType)
    {
        case EbtImage2D:
            return EbtFloat;
        case EbtIImage2D:
            return EbtInt;
        case EbtUImage2D:
            return EbtUInt;
        default:
            UNREACHABLE();
            return EbtVoid;
    }
}

// Delimits the beginning of a per-pixel critical section. Makes pixel local storage coherent.
//
// Either: GL_NV_fragment_shader_interlock
//         GL_INTEL_fragment_shader_ordering
//         GL_ARB_fragment_shader_interlock (also compiles to SPV_EXT_fragment_shader_interlock)
static TIntermNode *CreateBuiltInInterlockBeginCall(const ShCompileOptions &compileOptions,
                                                    TSymbolTable &symbolTable)
{
    switch (compileOptions.pls.fragmentSynchronizationType)
    {
        case ShFragmentSynchronizationType::FragmentShaderInterlock_NV_GL:
            return CreateBuiltInFunctionCallNode("beginInvocationInterlockNV", {}, symbolTable,
                                                 kESSLInternalBackendBuiltIns);
        case ShFragmentSynchronizationType::FragmentShaderOrdering_INTEL_GL:
            return CreateBuiltInFunctionCallNode("beginFragmentShaderOrderingINTEL", {},
                                                 symbolTable, kESSLInternalBackendBuiltIns);
        case ShFragmentSynchronizationType::FragmentShaderInterlock_ARB_GL:
            return CreateBuiltInFunctionCallNode("beginInvocationInterlockARB", {}, symbolTable,
                                                 kESSLInternalBackendBuiltIns);
        default:
            return nullptr;
    }
}

// Delimits the end of a per-pixel critical section. Makes pixel local storage coherent.
//
// Either: GL_NV_fragment_shader_interlock
//         GL_ARB_fragment_shader_interlock (also compiles to SPV_EXT_fragment_shader_interlock)
//
// GL_INTEL_fragment_shader_ordering doesn't have an "end()" delimiter.
static TIntermNode *CreateBuiltInInterlockEndCall(const ShCompileOptions &compileOptions,
                                                  TSymbolTable &symbolTable)
{
    switch (compileOptions.pls.fragmentSynchronizationType)
    {
        case ShFragmentSynchronizationType::FragmentShaderInterlock_NV_GL:
            return CreateBuiltInFunctionCallNode("endInvocationInterlockNV", {}, symbolTable,
                                                 kESSLInternalBackendBuiltIns);
        case ShFragmentSynchronizationType::FragmentShaderOrdering_INTEL_GL:
            return nullptr;  // GL_INTEL_fragment_shader_ordering doesn't have an "end()" call.
        case ShFragmentSynchronizationType::FragmentShaderInterlock_ARB_GL:
            return CreateBuiltInFunctionCallNode("endInvocationInterlockARB", {}, symbolTable,
                                                 kESSLInternalBackendBuiltIns);
        default:
            return nullptr;
    }
}

// Rewrites high level PLS operations to shader image operations.
class RewriteToImagesTraverser : public TIntermTraverser
{
  public:
    RewriteToImagesTraverser(TSymbolTable &symbolTable,
                             const ShCompileOptions &compileOptions,
                             int shaderVersion)
        : TIntermTraverser(true, false, false, &symbolTable),
          mCompileOptions(&compileOptions),
          mShaderVersion(shaderVersion)
    {}

    bool visitDeclaration(Visit, TIntermDeclaration *decl) override
    {
        TIntermTyped *declVariable = (decl->getSequence())->front()->getAsTyped();
        ASSERT(declVariable);

        if (!IsPixelLocal(declVariable->getBasicType()))
        {
            return true;
        }

        // PLS is not allowed in arrays.
        ASSERT(!declVariable->isArray());

        // This visitDeclaration doesn't get called for function arguments, and opaque types can
        // otherwise only be uniforms.
        ASSERT(declVariable->getQualifier() == EvqUniform);

        TIntermSymbol *plsSymbol = declVariable->getAsSymbolNode();
        ASSERT(plsSymbol);

        // Insert a global to hold the pixel coordinate as soon as we see PLS declared. This will be
        // initialized at the beginning of main().
        if (!mGlobalPixelCoord)
        {
            TType *coordType  = new TType(EbtInt, EbpHigh, EvqGlobal, 2);
            mGlobalPixelCoord = CreateTempVariable(mSymbolTable, coordType);
            insertStatementInParentBlock(CreateTempDeclarationNode(mGlobalPixelCoord));
        }

        // Replace the PLS declaration with an image2D.
        TVariable *image2D = createPLSImageReplacement(plsSymbol);
        insertNewPLSImage(plsSymbol, image2D);
        queueReplacement(new TIntermDeclaration({new TIntermSymbol(image2D)}),
                         OriginalNode::IS_DROPPED);

        return false;
    }

    bool visitAggregate(Visit, TIntermAggregate *aggregate) override
    {
        if (!BuiltInGroup::IsPixelLocal(aggregate->getOp()))
        {
            return true;
        }

        const TIntermSequence &args = *aggregate->getSequence();
        ASSERT(args.size() >= 1);
        TIntermSymbol *plsSymbol = args[0]->getAsSymbolNode();
        TVariable *image2D       = findPLSImage(plsSymbol);
        ASSERT(mGlobalPixelCoord);

        // Rewrite pixelLocalLoadANGLE -> imageLoad.
        if (aggregate->getOp() == EOpPixelLocalLoadANGLE)
        {
            // Replace the pixelLocalLoadANGLE with imageLoad.
            TIntermTyped *pls;
            pls = CreateBuiltInFunctionCallNode(
                "imageLoad", {new TIntermSymbol(image2D), new TIntermSymbol(mGlobalPixelCoord)},
                *mSymbolTable, mShaderVersion);
            pls = unpackImageDataIfNecessary(pls, plsSymbol, image2D);
            queueReplacement(pls, OriginalNode::IS_DROPPED);
            return false;  // No need to recurse since this node is being dropped.
        }

        // Rewrite pixelLocalStoreANGLE -> imageStore.
        if (aggregate->getOp() == EOpPixelLocalStoreANGLE)
        {
            // Surround the store with memoryBarrierImage calls in order to ensure dependent stores
            // and loads in a single shader invocation are coherent. From the ES 3.1 spec:
            //
            //   Using variables declared as "coherent" guarantees only that the results of stores
            //   will be immediately visible to shader invocations using similarly-declared
            //   variables; calling MemoryBarrier is required to ensure that the stores are visible
            //   to other operations.
            //
            // Also hoist the 'value' expression into a temp. In the event of
            // "pixelLocalStoreANGLE(..., pixelLocalLoadANGLE(...))", this ensures the load occurs
            // _before_ the memoryBarrierImage.
            //
            // NOTE: It is generally unsafe to hoist function arguments due to short circuiting,
            // e.g., "if (false && function(...))", but pixelLocalStoreANGLE returns type void, so
            // it is safe in this particular case.
            TType *valueType    = new TType(DataTypeOfPLSType(plsSymbol->getBasicType()),
                                            plsSymbol->getPrecision(), EvqTemporary, 4);
            TVariable *valueVar = CreateTempVariable(mSymbolTable, valueType);
            TIntermDeclaration *valueDecl =
                CreateTempInitDeclarationNode(valueVar, args[1]->getAsTyped());
            valueDecl->traverse(this);  // Rewrite any potential pixelLocalLoadANGLEs in valueDecl.

            insertStatementsInParentBlock(
                {valueDecl, CreateBuiltInFunctionCallNode("memoryBarrierImage", {}, *mSymbolTable,
                                                          mShaderVersion)},  // Before.
                {CreateBuiltInFunctionCallNode("memoryBarrierImage", {}, *mSymbolTable,
                                               mShaderVersion)});  // After.

            // Rewrite the pixelLocalStoreANGLE with imageStore.
            queueReplacement(CreateBuiltInFunctionCallNode(
                                 "imageStore",
                                 {new TIntermSymbol(image2D), new TIntermSymbol(mGlobalPixelCoord),
                                  clampAndPackPLSDataIfNecessary(valueVar, plsSymbol, image2D)},
                                 *mSymbolTable, mShaderVersion),
                             OriginalNode::IS_DROPPED);

            return false;  // No need to recurse since this node is being dropped.
        }

        return true;
    }

    TVariable *globalPixelCoord() const { return mGlobalPixelCoord; }

  private:
    // Do all PLS formats need to be packed into r32f, r32i, or r32ui image2Ds?
    bool needsR32Packing() const
    {
        return mCompileOptions->pls.type == ShPixelLocalStorageType::ImageStoreR32PackedFormats;
    }

    // Sets the given image2D as the backing storage for the plsSymbol's binding point. An entry
    // must not already exist in the map for this binding point.
    void insertNewPLSImage(TIntermSymbol *plsSymbol, TVariable *image2D)
    {
        ASSERT(plsSymbol);
        ASSERT(IsPixelLocal(plsSymbol->getBasicType()));
        int binding = plsSymbol->getType().getLayoutQualifier().binding;
        ASSERT(binding >= 0);
        auto result = mPLSImages.insert({binding, image2D});
        ASSERT(result.second);  // Ensure an image didn't already exist for this symbol.
    }

    // Looks up the image2D backing storage for the given plsSymbol's binding point. An entry
    // must already exist in the map for this binding point.
    TVariable *findPLSImage(TIntermSymbol *plsSymbol)
    {
        ASSERT(plsSymbol);
        ASSERT(IsPixelLocal(plsSymbol->getBasicType()));
        int binding = plsSymbol->getType().getLayoutQualifier().binding;
        ASSERT(binding >= 0);
        auto iter = mPLSImages.find(binding);
        ASSERT(iter != mPLSImages.end());  // Ensure PLSImages already exist for this symbol.
        return iter->second;
    }

    // Creates an image2D that replaces a pixel local storage handle.
    TVariable *createPLSImageReplacement(const TIntermSymbol *plsSymbol)
    {
        ASSERT(plsSymbol);
        ASSERT(IsPixelLocal(plsSymbol->getBasicType()));

        TType *imageType = new TType(plsSymbol->getType());

        TLayoutQualifier layoutQualifier = imageType->getLayoutQualifier();
        switch (layoutQualifier.imageInternalFormat)
        {
            case TLayoutImageInternalFormat::EiifRGBA8:
                if (needsR32Packing())
                {
                    layoutQualifier.imageInternalFormat = EiifR32UI;
                    imageType->setPrecision(EbpHigh);
                    imageType->setBasicType(EbtUImage2D);
                }
                else
                {
                    imageType->setBasicType(EbtImage2D);
                }
                break;
            case TLayoutImageInternalFormat::EiifRGBA8I:
                if (needsR32Packing())
                {
                    layoutQualifier.imageInternalFormat = EiifR32I;
                    imageType->setPrecision(EbpHigh);
                }
                imageType->setBasicType(EbtIImage2D);
                break;
            case TLayoutImageInternalFormat::EiifRGBA8UI:
                if (needsR32Packing())
                {
                    layoutQualifier.imageInternalFormat = EiifR32UI;
                    imageType->setPrecision(EbpHigh);
                }
                imageType->setBasicType(EbtUImage2D);
                break;
            case TLayoutImageInternalFormat::EiifR32F:
                imageType->setBasicType(EbtImage2D);
                break;
            case TLayoutImageInternalFormat::EiifR32UI:
                imageType->setBasicType(EbtUImage2D);
                break;
            default:
                UNREACHABLE();
        }
        layoutQualifier.rasterOrdered = mCompileOptions->pls.fragmentSynchronizationType ==
                                        ShFragmentSynchronizationType::RasterizerOrderViews_D3D;
        imageType->setLayoutQualifier(layoutQualifier);

        TMemoryQualifier memoryQualifier{};
        memoryQualifier.coherent          = true;
        memoryQualifier.restrictQualifier = true;
        memoryQualifier.volatileQualifier = false;
        // TODO(anglebug.com/7279): Maybe we could walk the tree first and see which PLS is used
        // how. If the PLS is never loaded, we could add a writeonly qualifier, for example.
        memoryQualifier.readonly  = false;
        memoryQualifier.writeonly = false;
        imageType->setMemoryQualifier(memoryQualifier);

        const TVariable &plsVar = plsSymbol->variable();
        return new TVariable(plsVar.uniqueId(), plsVar.name(), plsVar.symbolType(),
                             plsVar.extensions(), imageType);
    }

    // Unpacks the raw PLS data if the output shader language needs r32* packing.
    TIntermTyped *unpackImageDataIfNecessary(TIntermTyped *data,
                                             TIntermSymbol *plsSymbol,
                                             TVariable *image2D)
    {
        TLayoutImageInternalFormat plsFormat =
            plsSymbol->getType().getLayoutQualifier().imageInternalFormat;
        TLayoutImageInternalFormat imageFormat =
            image2D->getType().getLayoutQualifier().imageInternalFormat;
        if (plsFormat == imageFormat)
        {
            return data;  // This PLS storage isn't packed.
        }
        ASSERT(needsR32Packing());
        switch (plsFormat)
        {
            case EiifRGBA8:
                // Unpack and normalize r,g,b,a from a single 32-bit unsigned int:
                //
                //     unpackUnorm4x8(data.r)
                //
                data = CreateBuiltInFunctionCallNode("unpackUnorm4x8", {CreateSwizzle(data, 0)},
                                                     *mSymbolTable, mShaderVersion);
                break;
            case EiifRGBA8I:
            case EiifRGBA8UI:
            {
                constexpr unsigned shifts[] = {24, 16, 8, 0};
                // Unpack r,g,b,a form a single (signed or unsigned) 32-bit int. Shift left,
                // then right, to preserve the sign for ints. (highp integers are exactly
                // 32-bit, two's compliment.)
                //
                //     data.rrrr << uvec4(24, 16, 8, 0) >> 24u
                //
                data = CreateSwizzle(data, 0, 0, 0, 0);
                data = new TIntermBinary(EOpBitShiftLeft, data, CreateUVecNode(shifts, 4, EbpHigh));
                data = new TIntermBinary(EOpBitShiftRight, data, CreateUIntNode(24));
                break;
            }
            default:
                UNREACHABLE();
        }
        return data;
    }

    // Packs the PLS to raw data if the output shader language needs r32* packing.
    TIntermTyped *clampAndPackPLSDataIfNecessary(TVariable *plsVar,
                                                 TIntermSymbol *plsSymbol,
                                                 TVariable *image2D)
    {
        TLayoutImageInternalFormat plsFormat =
            plsSymbol->getType().getLayoutQualifier().imageInternalFormat;
        // anglebug.com/7524: Storing to integer formats with values larger than can be represented
        // is specified differently on different APIs. Clamp integer formats here to make it uniform
        // and more GL-like.
        switch (plsFormat)
        {
            case EiifRGBA8I:
            {
                // Clamp r,g,b,a to their min/max 8-bit values:
                //
                //     plsVar = clamp(plsVar, -128, 127) & 0xff
                //
                TIntermTyped *newPLSValue = CreateBuiltInFunctionCallNode(
                    "clamp",
                    {new TIntermSymbol(plsVar), CreateIndexNode(-128), CreateIndexNode(127)},
                    *mSymbolTable, mShaderVersion);
                insertStatementInParentBlock(CreateTempAssignmentNode(plsVar, newPLSValue));
                break;
            }
            case EiifRGBA8UI:
            {
                // Clamp r,g,b,a to their max 8-bit values:
                //
                //     plsVar = min(plsVar, 255)
                //
                TIntermTyped *newPLSValue = CreateBuiltInFunctionCallNode(
                    "min", {new TIntermSymbol(plsVar), CreateUIntNode(255)}, *mSymbolTable,
                    mShaderVersion);
                insertStatementInParentBlock(CreateTempAssignmentNode(plsVar, newPLSValue));
                break;
            }
            default:
                break;
        }
        TIntermTyped *result = new TIntermSymbol(plsVar);
        TLayoutImageInternalFormat imageFormat =
            image2D->getType().getLayoutQualifier().imageInternalFormat;
        if (plsFormat == imageFormat)
        {
            return result;  // This PLS storage isn't packed.
        }
        ASSERT(needsR32Packing());
        switch (plsFormat)
        {
            case EiifRGBA8:
            {
                if (mCompileOptions->passHighpToPackUnormSnormBuiltins)
                {
                    // anglebug.com/7527: unpackUnorm4x8 doesn't work on Pixel 4 when passed
                    // a mediump vec4. Use an intermediate highp vec4.
                    //
                    // It's safe to inject a variable here because it happens right before
                    // pixelLocalStoreANGLE, which returns type void. (See visitAggregate.)
                    TType *highpType              = new TType(EbtFloat, EbpHigh, EvqTemporary, 4);
                    TVariable *workaroundHighpVar = CreateTempVariable(mSymbolTable, highpType);
                    insertStatementInParentBlock(
                        CreateTempInitDeclarationNode(workaroundHighpVar, result));
                    result = new TIntermSymbol(workaroundHighpVar);
                }

                // Denormalize and pack r,g,b,a into a single 32-bit unsigned int:
                //
                //     packUnorm4x8(workaroundHighpVar)
                //
                result = CreateBuiltInFunctionCallNode("packUnorm4x8", {result}, *mSymbolTable,
                                                       mShaderVersion);
                break;
            }
            case EiifRGBA8I:
            case EiifRGBA8UI:
            {
                if (plsFormat == EiifRGBA8I)
                {
                    // Mask off extra sign bits beyond 8.
                    //
                    //     plsVar &= 0xff
                    //
                    insertStatementInParentBlock(new TIntermBinary(
                        EOpBitwiseAndAssign, new TIntermSymbol(plsVar), CreateIndexNode(0xff)));
                }
                // Pack r,g,b,a into a single 32-bit (signed or unsigned) int:
                //
                //     r | (g << 8) | (b << 16) | (a << 24)
                //
                auto shiftComponent = [=](int componentIdx) {
                    return new TIntermBinary(EOpBitShiftLeft,
                                             CreateSwizzle(new TIntermSymbol(plsVar), componentIdx),
                                             CreateUIntNode(componentIdx * 8));
                };
                result = CreateSwizzle(result, 0);
                result = new TIntermBinary(EOpBitwiseOr, result, shiftComponent(1));
                result = new TIntermBinary(EOpBitwiseOr, result, shiftComponent(2));
                result = new TIntermBinary(EOpBitwiseOr, result, shiftComponent(3));
                break;
            }
            default:
                UNREACHABLE();
        }
        // Convert the packed data to a {u,i}vec4 for imageStore.
        TType imageStoreType(DataTypeOfImageType(image2D->getType().getBasicType()), 4);
        return TIntermAggregate::CreateConstructor(imageStoreType, {result});
    }

    const ShCompileOptions *const mCompileOptions;
    const int mShaderVersion;

    // Stores the shader invocation's pixel coordinate as "ivec2(floor(gl_FragCoord.xy))".
    TVariable *mGlobalPixelCoord = nullptr;

    // Maps PLS variables to their image2D backing storage.
    angle::HashMap<int, TVariable *> mPLSImages;
};

}  // anonymous namespace

bool RewritePixelLocalStorageToImages(TCompiler *compiler,
                                      TIntermBlock *root,
                                      TSymbolTable &symbolTable,
                                      const ShCompileOptions &compileOptions,
                                      int shaderVersion)
{
    // If any functions take PLS arguments, monomorphize the functions by removing said parameters
    // and making the PLS calls from main() instead, using the global uniform from the call site
    // instead of the function argument. This is necessary because function arguments don't carry
    // the necessary "binding" or "format" layout qualifiers.
    if (!MonomorphizeUnsupportedFunctions(
            compiler, root, &symbolTable, compileOptions,
            UnsupportedFunctionArgsBitSet{UnsupportedFunctionArgs::PixelLocalStorage}))
    {
        return false;
    }

    TIntermBlock *mainBody = FindMainBody(root);

    // Surround the critical section of PLS operations in fragment synchronization calls, if
    // supported. This makes pixel local storage coherent.
    TIntermNode *interlockBeginCall = CreateBuiltInInterlockBeginCall(compileOptions, symbolTable);
    if (interlockBeginCall)
    {
        // TODO(anglebug.com/7279): Inject these functions in a tight critical section, instead of
        // just locking the entire main() function:
        //   - Monomorphize all PLS calls into main().
        //   - Insert begin/end calls around the first/last PLS calls (and outside of flow control).
        mainBody->insertStatement(0, interlockBeginCall);
        TIntermNode *interlockEndCall = CreateBuiltInInterlockEndCall(compileOptions, symbolTable);
        if (interlockEndCall)
        {
            // Not all fragment synchronization extensions have an end() call.
            mainBody->appendStatement(interlockEndCall);
        }
    }

    // Rewrite PLS operations to image operations.
    RewriteToImagesTraverser traverser(symbolTable, compileOptions, shaderVersion);
    root->traverse(&traverser);
    if (!traverser.updateTree(compiler, root))
    {
        return false;
    }

    // Initialize the global pixel coord at the beginning of main():
    //
    //     pixelCoord = ivec2(floor(gl_FragCoord.xy));
    //
    if (traverser.globalPixelCoord())
    {
        TIntermTyped *exp;
        exp = ReferenceBuiltInVariable(ImmutableString("gl_FragCoord"), symbolTable, shaderVersion);
        exp = CreateSwizzle(exp, 0, 1);
        exp = CreateBuiltInFunctionCallNode("floor", {exp}, symbolTable, shaderVersion);
        exp = TIntermAggregate::CreateConstructor(TType(EbtInt, 2), {exp});
        exp = CreateTempAssignmentNode(traverser.globalPixelCoord(), exp);
        mainBody->insertStatement(0, exp);
    }

    return compiler->validateAST(root);
}

}  // namespace sh
