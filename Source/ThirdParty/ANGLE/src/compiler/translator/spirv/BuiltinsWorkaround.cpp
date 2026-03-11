//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/spirv/BuiltinsWorkaround.h"

#include "angle_gl.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/BuiltIn.h"

namespace sh
{

namespace
{
constexpr const ImmutableString kGlInstanceIDString("gl_InstanceID");
constexpr const ImmutableString kGlVertexIDString("gl_VertexID");

class TBuiltinsWorkaround : public TIntermTraverser
{
  public:
    TBuiltinsWorkaround(TSymbolTable *symbolTable,
                        const ShCompileOptions &options,
                        const TVariable *emulatedBaseInstance)
        : TIntermTraverser(true, false, false, symbolTable),
          mCompileOptions(options),
          mEmulatedBaseInstance(emulatedBaseInstance)
    {}

    void visitSymbol(TIntermSymbol *node) override;

  private:
    void ensureVersionIsAtLeast(int version);

    const ShCompileOptions &mCompileOptions;
    const TVariable *mEmulatedBaseInstance;
};

void TBuiltinsWorkaround::visitSymbol(TIntermSymbol *node)
{
    if (node->variable().symbolType() == SymbolType::BuiltIn &&
        node->getName() == kGlInstanceIDString)
    {
        TIntermSymbol *instanceIndexRef = new TIntermSymbol(BuiltInVariable::gl_InstanceIndex());

        if (mEmulatedBaseInstance != nullptr)
        {
            TIntermSymbol *baseInstanceRef = new TIntermSymbol(mEmulatedBaseInstance);

            TIntermBinary *subBaseInstance =
                new TIntermBinary(EOpSub, instanceIndexRef, baseInstanceRef);
            queueReplacement(subBaseInstance, OriginalNode::IS_DROPPED);
        }
        else
        {
            queueReplacement(instanceIndexRef, OriginalNode::IS_DROPPED);
        }
    }
    else if (node->getName() == kGlVertexIDString)
    {
        TIntermSymbol *vertexIndexRef = new TIntermSymbol(BuiltInVariable::gl_VertexIndex());
        queueReplacement(vertexIndexRef, OriginalNode::IS_DROPPED);
    }
}

const TVariable *FindEmulatedBaseInstance(TIntermBlock *root)
{
    for (TIntermNode *node : *root->getSequence())
    {
        TIntermDeclaration *decl = node->getAsDeclarationNode();
        if (decl == nullptr)
        {
            continue;
        }

        const TIntermSequence &sequence = *(decl->getSequence());
        ASSERT(!sequence.empty());

        TIntermSymbol *symbol = sequence[0]->getAsSymbolNode();
        if (symbol == nullptr)
        {
            continue;
        }

        if (symbol->variable().symbolType() == SymbolType::AngleInternal &&
            symbol->variable().name() == "angle_BaseInstance")
        {
            return &symbol->variable();
        }
    }
    return nullptr;
}
}  // anonymous namespace

[[nodiscard]] bool ShaderBuiltinsWorkaround(TCompiler *compiler,
                                            TIntermBlock *root,
                                            TSymbolTable *symbolTable,
                                            const ShCompileOptions &compileOptions)
{
    TBuiltinsWorkaround builtins(symbolTable, compileOptions, FindEmulatedBaseInstance(root));
    root->traverse(&builtins);
    if (!builtins.updateTree(compiler, root))
    {
        return false;
    }
    return true;
}

}  // namespace sh
