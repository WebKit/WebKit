//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_FragColor needs to broadcast to all color buffers in ES2 if
// GL_EXT_draw_buffers is explicitly enabled in a fragment shader.
//
// We emulate this by replacing all gl_FragColor with gl_FragData[0], and in the end
// of main() function, assigning gl_FragData[1], ..., gl_FragData[maxDrawBuffers-1]
// with gl_FragData[0].
//

#include "compiler/translator/tree_ops/EmulateGLFragColorBroadcast.h"

#include "compiler/translator/Symbol.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"

namespace sh
{

namespace
{

constexpr const ImmutableString kGlFragDataString("gl_FragData");

class GLFragColorBroadcastTraverser : public TIntermTraverser
{
  public:
    GLFragColorBroadcastTraverser(int maxDrawBuffers, TSymbolTable *symbolTable, int shaderVersion)
        : TIntermTraverser(true, false, false, symbolTable),
          mGLFragColorUsed(false),
          mMaxDrawBuffers(maxDrawBuffers),
          mShaderVersion(shaderVersion)
    {}

    void broadcastGLFragColor(TIntermBlock *root);

    bool isGLFragColorUsed() const { return mGLFragColorUsed; }

  protected:
    void visitSymbol(TIntermSymbol *node) override;

    TIntermBinary *constructGLFragDataNode(int index) const;
    TIntermBinary *constructGLFragDataAssignNode(int index) const;

  private:
    bool mGLFragColorUsed;
    int mMaxDrawBuffers;
    const int mShaderVersion;
};

TIntermBinary *GLFragColorBroadcastTraverser::constructGLFragDataNode(int index) const
{
    TIntermSymbol *symbol =
        ReferenceBuiltInVariable(kGlFragDataString, *mSymbolTable, mShaderVersion);
    TIntermTyped *indexNode = CreateIndexNode(index);

    TIntermBinary *binary = new TIntermBinary(EOpIndexDirect, symbol, indexNode);
    return binary;
}

TIntermBinary *GLFragColorBroadcastTraverser::constructGLFragDataAssignNode(int index) const
{
    TIntermTyped *fragDataIndex = constructGLFragDataNode(index);
    TIntermTyped *fragDataZero  = constructGLFragDataNode(0);

    return new TIntermBinary(EOpAssign, fragDataIndex, fragDataZero);
}

void GLFragColorBroadcastTraverser::visitSymbol(TIntermSymbol *node)
{
    if (node->variable().symbolType() == SymbolType::BuiltIn && node->getName() == "gl_FragColor")
    {
        queueReplacement(constructGLFragDataNode(0), OriginalNode::IS_DROPPED);
        mGLFragColorUsed = true;
    }
}

void GLFragColorBroadcastTraverser::broadcastGLFragColor(TIntermBlock *root)
{
    ASSERT(mMaxDrawBuffers > 1);
    if (!mGLFragColorUsed)
    {
        return;
    }

    TIntermBlock *broadcastBlock = new TIntermBlock();
    // Now insert statements
    //   gl_FragData[1] = gl_FragData[0];
    //   ...
    //   gl_FragData[maxDrawBuffers - 1] = gl_FragData[0];
    for (int colorIndex = 1; colorIndex < mMaxDrawBuffers; ++colorIndex)
    {
        broadcastBlock->appendStatement(constructGLFragDataAssignNode(colorIndex));
    }
    RunAtTheEndOfShader(root, broadcastBlock, mSymbolTable);
}

}  // namespace

void EmulateGLFragColorBroadcast(TIntermBlock *root,
                                 int maxDrawBuffers,
                                 std::vector<sh::OutputVariable> *outputVariables,
                                 TSymbolTable *symbolTable,
                                 int shaderVersion)
{
    ASSERT(maxDrawBuffers > 1);
    GLFragColorBroadcastTraverser traverser(maxDrawBuffers, symbolTable, shaderVersion);
    root->traverse(&traverser);
    if (traverser.isGLFragColorUsed())
    {
        traverser.updateTree();
        traverser.broadcastGLFragColor(root);
        for (auto &var : *outputVariables)
        {
            if (var.name == "gl_FragColor")
            {
                // TODO(zmo): Find a way to keep the original variable information.
                var.name       = "gl_FragData";
                var.mappedName = "gl_FragData";
                var.arraySizes.push_back(maxDrawBuffers);
                ASSERT(var.arraySizes.size() == 1u);
            }
        }
    }
}

}  // namespace sh
