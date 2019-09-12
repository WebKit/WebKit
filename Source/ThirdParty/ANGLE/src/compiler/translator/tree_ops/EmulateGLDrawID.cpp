//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateGLDrawID is an AST traverser to convert the gl_DrawID builtin
// to a uniform int
//

#include "compiler/translator/tree_ops/EmulateGLDrawID.h"

#include "angle_gl.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/BuiltIn_autogen.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

constexpr const ImmutableString kEmulatedGLDrawIDName("angle_DrawID");

class FindGLDrawIDTraverser : public TIntermTraverser
{
  public:
    FindGLDrawIDTraverser() : TIntermTraverser(true, false, false), mVariable(nullptr) {}

    const TVariable *getGLDrawIDBuiltinVariable() { return mVariable; }

  protected:
    void visitSymbol(TIntermSymbol *node) override
    {
        if (&node->variable() == BuiltInVariable::gl_DrawID() ||
            &node->variable() == BuiltInVariable::gl_DrawIDESSL1())
        {
            mVariable = &node->variable();
        }
    }

  private:
    const TVariable *mVariable;
};

}  // namespace

void EmulateGLDrawID(TIntermBlock *root,
                     TSymbolTable *symbolTable,
                     std::vector<sh::Uniform> *uniforms,
                     bool shouldCollect)
{
    FindGLDrawIDTraverser traverser;
    root->traverse(&traverser);
    const TVariable *builtInVariable = traverser.getGLDrawIDBuiltinVariable();
    if (builtInVariable)
    {
        const TType *type = StaticType::Get<EbtInt, EbpHigh, EvqUniform, 1, 1>();
        const TVariable *drawID =
            new TVariable(symbolTable, kEmulatedGLDrawIDName, type, SymbolType::AngleInternal);

        // AngleInternal variables don't get collected
        if (shouldCollect)
        {
            Uniform uniform;
            uniform.name       = kEmulatedGLDrawIDName.data();
            uniform.mappedName = kEmulatedGLDrawIDName.data();
            uniform.type       = GLVariableType(*type);
            uniform.precision  = GLVariablePrecision(*type);
            uniform.staticUse  = symbolTable->isStaticallyUsed(*builtInVariable);
            uniform.active     = true;
            uniform.binding    = type->getLayoutQualifier().binding;
            uniform.location   = type->getLayoutQualifier().location;
            uniform.offset     = type->getLayoutQualifier().offset;
            uniform.readonly   = type->getMemoryQualifier().readonly;
            uniform.writeonly  = type->getMemoryQualifier().writeonly;
            uniforms->push_back(uniform);
        }

        DeclareGlobalVariable(root, drawID);
        ReplaceVariable(root, builtInVariable, drawID);
    }
}

}  // namespace sh
