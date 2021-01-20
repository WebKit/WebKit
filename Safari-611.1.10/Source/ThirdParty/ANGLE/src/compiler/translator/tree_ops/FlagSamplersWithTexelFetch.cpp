//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FlagSamplersForTexelFetch.cpp: finds all instances of texelFetch used with a static reference to
// a sampler uniform, and flag that uniform as having been used with texelFetch
//

#include "compiler/translator/tree_ops/FlagSamplersWithTexelFetch.h"

#include "angle_gl.h"
#include "common/utilities.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"

namespace sh
{
namespace
{

// gvec4 texelFetch(gsamplerXD sampler, ivecX P, int lod)
constexpr uint32_t kTexelFetchSequenceLength = 3u;

// gvec4 texelFetchOffset(gsamplerXD sampler, ivecX P, int lod, ivecX offset)
constexpr uint32_t kTexelFetchOffsetSequenceLength = 4u;

class FlagSamplersWithTexelFetchTraverser : public TIntermTraverser
{
  public:
    FlagSamplersWithTexelFetchTraverser(TSymbolTable *symbolTable,
                                        std::vector<ShaderVariable> *uniforms)
        : TIntermTraverser(true, true, true, symbolTable), mUniforms(uniforms)
    {}

    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        // Decide if the node is a call to texelFetch[Offset]
        if (node->getOp() != EOpCallBuiltInFunction)
        {
            return true;
        }

        ASSERT(node->getFunction()->symbolType() == SymbolType::BuiltIn);
        if (node->getFunction()->name() != "texelFetch" &&
            node->getFunction()->name() != "texelFetchOffset")
        {
            return true;
        }

        const TIntermSequence *sequence = node->getSequence();

        // This must be a valid texelFetch invokation with the required number of arguments
        ASSERT(sequence->size() == (node->getFunction()->name() == "texelFetch"
                                        ? kTexelFetchSequenceLength
                                        : kTexelFetchOffsetSequenceLength));

        TIntermSymbol *samplerSymbol = sequence->at(0)->getAsSymbolNode();
        ASSERT(samplerSymbol != nullptr);

        const TVariable &samplerVariable = samplerSymbol->variable();

        for (ShaderVariable &uniform : *mUniforms)
        {
            if (samplerVariable.name() == uniform.name)
            {
                ASSERT(gl::IsSamplerType(uniform.type));
                uniform.texelFetchStaticUse = true;
                break;
            }
        }

        return true;
    }

  private:
    std::vector<ShaderVariable> *mUniforms;
};

}  // anonymous namespace

bool FlagSamplersForTexelFetch(TCompiler *compiler,
                               TIntermBlock *root,
                               TSymbolTable *symbolTable,
                               std::vector<ShaderVariable> *uniforms)
{
    ASSERT(uniforms != nullptr);
    if (uniforms->size() > 0)
    {
        FlagSamplersWithTexelFetchTraverser traverser(symbolTable, uniforms);
        root->traverse(&traverser);
    }

    return true;
}

}  // namespace sh
