//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReswizzleYUVOps: Adjusts swizzles for YUV channel order difference between
//   GLES and Vulkan
//
//

#include "compiler/translator/tree_ops/spirv/EmulateYUVBuiltIns.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"

namespace sh
{
namespace
{
// A traverser that adjusts channel order for various yuv ops.
class ReswizzleYUVOpsTraverser : public TIntermTraverser
{
  public:
    ReswizzleYUVOpsTraverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable)
    {}

    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitSwizzle(Visit visit, TIntermSwizzle *node) override;

  private:
    TIntermSwizzle *transformTextureOp(TIntermAggregate *node);
};

// OpenGLES and Vulkan has different color component mapping for YUV. OpenGL spec maps R_gl=y,
// G_gl=u, B_gl=v, but Vulkan wants R_vulkan=v, G_vulkan=y, B_vulkan=u. We want all calculation to
// be in OpenGLES mapping during shader execution, but the actual buffer/image will be stored as
// vulkan mapping. This means when we sample from VkImage, we need to map from vulkan order back to
// GL order, which comes out to be R_gl=y=G_vulkan=1, G_gl=u=B_vulkan=2, B_gl=v=R_vulkan=0. i.e, {1,
// 2, 0, 3}. This function will check if the aggregate is a texture{proj|fetch}(samplerExternal,...)
// and if yes it will compose and return a swizzle node.
TIntermSwizzle *ReswizzleYUVOpsTraverser::transformTextureOp(TIntermAggregate *node)
{
    const TOperator op = node->getOp();
    if (op == EOpTexture || op == EOpTextureProj || op == EOpTexelFetch)
    {
        const TIntermSequence &arguments = *node->getSequence();
        TType const &samplerType         = arguments[0]->getAsTyped()->getType();

        if (samplerType.getBasicType() != EbtSamplerExternal2DY2YEXT)
        {
            return nullptr;
        }

        // texture(...).gbra
        return new TIntermSwizzle(node, {1, 2, 0, 3});
    }

    return nullptr;
}

bool ReswizzleYUVOpsTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    TIntermSwizzle *yuvSwizzle = transformTextureOp(node);
    if (yuvSwizzle != nullptr)
    {
        ASSERT(!getParentNode()->getAsSwizzleNode());
        queueReplacement(yuvSwizzle, OriginalNode::IS_DROPPED);
        return false;
    }

    return true;
}

bool ReswizzleYUVOpsTraverser::visitSwizzle(Visit visit, TIntermSwizzle *node)
{
    TIntermAggregate *aggregate = node->getOperand()->getAsAggregate();
    if (aggregate == nullptr)
    {
        return true;
    }

    // There is swizzle on YUV texture sampler, and we need to apply YUV swizzle first and
    // then followed by the original swizzle. Finally we fold the two swizzles into one.
    TIntermSwizzle *yuvSwizzle = transformTextureOp(aggregate);
    if (yuvSwizzle != nullptr)
    {
        TIntermTyped *replacement = new TIntermSwizzle(yuvSwizzle, node->getSwizzleOffsets());
        replacement               = replacement->fold(nullptr);
        queueReplacement(replacement, OriginalNode::IS_DROPPED);
        return false;
    }

    return true;
}
}  // anonymous namespace

// OpenGLES and Vulkan has different color component mapping for YUV. When we write YUV data, we
// need to convert OpenGL mapping to vulkan's mapping, which comes out to be {2, 0, 1, 3}.
bool AdjustYUVOutput(TCompiler *compiler,
                     TIntermBlock *root,
                     TSymbolTable *symbolTable,
                     const TIntermSymbol &yuvOutput)
{
    TIntermBlock *block = new TIntermBlock;

    // output = output.brga
    TVector<uint32_t> swizzle = {2, 0, 1, 3};
    swizzle.resize(yuvOutput.getType().getNominalSize());

    TIntermTyped *assignment = new TIntermBinary(EOpAssign, yuvOutput.deepCopy(),
                                                 new TIntermSwizzle(yuvOutput.deepCopy(), swizzle));
    block->appendStatement(assignment);

    return RunAtTheEndOfShader(compiler, root, block, symbolTable);
}

bool ReswizzleYUVTextureAccess(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    ReswizzleYUVOpsTraverser traverser(symbolTable);
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}
}  // namespace sh
