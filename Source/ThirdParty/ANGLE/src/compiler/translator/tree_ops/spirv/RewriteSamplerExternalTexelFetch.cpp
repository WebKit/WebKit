//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteSamplerExternalTexelFetch: Rewrite texelFetch() for external samplers to texture() so that
// yuv decoding happens according to the sampler.
//

#include "compiler/translator/tree_ops/spirv/RewriteSamplerExternalTexelFetch.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{
// In GLES, texelFetch decodes YUV according to the sampler, while the SPIR-V equivalent
// (OpImageFetch) does not take a sampler and cannot do that.  The texelFetch() call is changed to
// texture() here to get the GLES behavior.
class Traverser : public TIntermTraverser
{
  public:
    Traverser(TSymbolTable *symbolTable) : TIntermTraverser(true, false, false, symbolTable) {}

    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
};

bool Traverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (node->getOp() != EOpTexelFetch)
    {
        return true;
    }

    const TIntermSequence &arguments = *node->getSequence();
    ASSERT(arguments.size() == 3);

    TIntermTyped *sampler = arguments[0]->getAsTyped();
    TIntermTyped *coords  = arguments[1]->getAsTyped();
    TIntermTyped *lod     = arguments[2]->getAsTyped();

    const TBasicType samplerType = sampler->getType().getBasicType();
    if (samplerType != EbtSamplerExternalOES && samplerType != EbtSamplerExternal2DY2YEXT)
    {
        return true;
    }

    // Change
    //
    //     texelFetch(externalSampler, coords, lod)
    //
    // to
    //
    //     texture(externalSampler, (vec2(coords) + vec2(0.5))
    //                              / vec2(textureSize(externalSampler, lod)))
    //
    // Note that |texelFetch| takes integer coordinates, while |texture| takes normalized
    // coordinates.  The division by |textureSize| achieves normalization.  Additionally, an offset
    // of half a pixel (vec2(0.5)) is added to the coordinates such that |texture|'s sampling is
    // done at the center of the pixel, returning only the value of that pixel and not an
    // interpolation with its neighboring pixels.
    //
    const TPrecision coordsPrecision = coords->getType().getPrecision();
    TType *vec2Type                  = new TType(EbtFloat, coordsPrecision, EvqTemporary, 2);

    // textureSize(externalSampler, lod)
    TIntermTyped *textureSizeCall =
        CreateBuiltInFunctionCallNode("textureSize", {sampler, lod}, *mSymbolTable, 300);
    // vec2(textureSize(externalSampler, lod))
    textureSizeCall = TIntermAggregate::CreateConstructor(*vec2Type, {textureSizeCall});

    constexpr float kHalfPixelOffset[2] = {0.5, 0.5};
    TIntermTyped *halfPixel             = CreateVecNode(kHalfPixelOffset, 2, coordsPrecision);

    // vec2(coords)
    TIntermTyped *scaledCoords = TIntermAggregate::CreateConstructor(*vec2Type, {coords});
    // vec2(coords) + vec2(0.5)
    scaledCoords = new TIntermBinary(EOpAdd, scaledCoords, halfPixel);
    // (vec2(coords) + vec2(0.5)) / vec2(textureSize(externalSampler, lod)))
    scaledCoords = new TIntermBinary(EOpDiv, scaledCoords, textureSizeCall);

    TIntermTyped *textureCall = CreateBuiltInFunctionCallNode(
        "texture", {sampler->deepCopy(), scaledCoords}, *mSymbolTable, 300);
    queueReplacement(textureCall, OriginalNode::IS_DROPPED);
    return true;
}
}  // anonymous namespace

bool RewriteSamplerExternalTexelFetch(TCompiler *compiler,
                                      TIntermBlock *root,
                                      TSymbolTable *symbolTable)
{
    Traverser traverser(symbolTable);
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}
}  // namespace sh
