//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateGLDrawID is an AST traverser to convert the gl_DrawID builtin
// to a uniform int
//
// EmulateGLBaseVertex is an AST traverser to convert the gl_BaseVertex builtin
// to a uniform int
//
// EmulateGLBaseInstance is an AST traverser to convert the gl_BaseInstance builtin
// to a uniform int
//

#include "compiler/translator/tree_ops/EmulateMultiDrawShaderBuiltins.h"

#include "angle_gl.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/BuiltIn.h"
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
        if (node->getQualifier() == EvqDrawID)
        {
            mVariable = &node->variable();
        }
    }

  private:
    const TVariable *mVariable;
};

class AddBaseVertexToGLVertexIDTraverser : public TIntermTraverser
{
  public:
    AddBaseVertexToGLVertexIDTraverser() : TIntermTraverser(true, false, false) {}

  protected:
    void visitSymbol(TIntermSymbol *node) override
    {
        if (&node->variable() == BuiltInVariable::gl_VertexID())
        {

            TIntermSymbol *baseVertexRef = new TIntermSymbol(BuiltInVariable::gl_BaseVertex());

            TIntermBinary *addBaseVertex = new TIntermBinary(EOpAdd, node, baseVertexRef);
            queueReplacement(addBaseVertex, OriginalNode::BECOMES_CHILD);
        }
    }
};

constexpr const ImmutableString kEmulatedGLBaseVertexName("angle_BaseVertex");
constexpr const ImmutableString kEmulatedGLBaseInstanceName("angle_BaseInstance");

class FindGLBaseVertexBaseInstanceTraverser : public TIntermTraverser
{
  public:
    FindGLBaseVertexBaseInstanceTraverser()
        : TIntermTraverser(true, false, false),
          mBaseVertexVariable(nullptr),
          mBaseInstanceVariable(nullptr)
    {}

    const TVariable *getGLBaseVertexBuiltinVariable() { return mBaseVertexVariable; }
    const TVariable *getGLBaseInstanceBuiltinVariable() { return mBaseInstanceVariable; }

  protected:
    void visitSymbol(TIntermSymbol *node) override
    {
        switch (node->getQualifier())
        {
            case EvqBaseVertex:
                mBaseVertexVariable = &node->variable();
                break;
            case EvqBaseInstance:
                mBaseInstanceVariable = &node->variable();
                break;
            default:
                break;
        }
    }

  private:
    const TVariable *mBaseVertexVariable;
    const TVariable *mBaseInstanceVariable;
};

bool EmulateBuiltIn(TCompiler *compiler,
                    TIntermBlock *root,
                    TSymbolTable *symbolTable,
                    const TVariable *builtInVariable,
                    const TType *type,
                    const ImmutableString &name,
                    std::vector<sh::ShaderVariable> *uniforms)
{
    const TVariable *emulatedVar =
        new TVariable(symbolTable, name, type, SymbolType::AngleInternal);
    const TIntermSymbol *emulatedSymbol = new TIntermSymbol(emulatedVar);

    // AngleInternal variables don't get collected
    ShaderVariable uniform;
    uniform.name          = name.data();
    uniform.mappedName    = name.data();
    uniform.type          = GLVariableType(*type);
    uniform.precision     = GLVariablePrecision(*type);
    uniform.staticUse     = symbolTable->isStaticallyUsed(*builtInVariable);
    uniform.active        = true;
    uniform.binding       = type->getLayoutQualifier().binding;
    uniform.location      = type->getLayoutQualifier().location;
    uniform.offset        = type->getLayoutQualifier().offset;
    uniform.rasterOrdered = type->getLayoutQualifier().rasterOrdered;
    uniform.readonly      = type->getMemoryQualifier().readonly;
    uniform.writeonly     = type->getMemoryQualifier().writeonly;
    uniforms->push_back(uniform);

    DeclareGlobalVariable(root, emulatedVar);
    return ReplaceVariableWithTyped(compiler, root, builtInVariable, emulatedSymbol);
}

}  // namespace

bool EmulateGLDrawID(TCompiler *compiler,
                     TIntermBlock *root,
                     TSymbolTable *symbolTable,
                     std::vector<sh::ShaderVariable> *uniforms)
{
    FindGLDrawIDTraverser traverser;
    root->traverse(&traverser);
    const TVariable *builtInVariable = traverser.getGLDrawIDBuiltinVariable();
    if (builtInVariable)
    {
        const TType *type = StaticType::Get<EbtInt, EbpHigh, EvqUniform, 1, 1>();
        if (!EmulateBuiltIn(compiler, root, symbolTable, builtInVariable, type,
                            kEmulatedGLDrawIDName, uniforms))
        {
            return false;
        }
    }

    return true;
}

bool EmulateGLBaseVertexBaseInstance(TCompiler *compiler,
                                     TIntermBlock *root,
                                     TSymbolTable *symbolTable,
                                     std::vector<sh::ShaderVariable> *uniforms,
                                     bool addBaseVertexToVertexID)
{
    if (addBaseVertexToVertexID)
    {
        // This is a workaround for Mac AMD GPU
        // Replace gl_VertexID with (gl_VertexID + gl_BaseVertex)
        AddBaseVertexToGLVertexIDTraverser traverserVertexID;
        root->traverse(&traverserVertexID);
        if (!traverserVertexID.updateTree(compiler, root))
        {
            return false;
        }
    }

    FindGLBaseVertexBaseInstanceTraverser traverser;
    root->traverse(&traverser);
    const TVariable *builtInVariableBaseVertex   = traverser.getGLBaseVertexBuiltinVariable();
    const TVariable *builtInVariableBaseInstance = traverser.getGLBaseInstanceBuiltinVariable();

    if (builtInVariableBaseVertex)
    {
        const TType *type = StaticType::Get<EbtInt, EbpHigh, EvqUniform, 1, 1>();
        if (!EmulateBuiltIn(compiler, root, symbolTable, builtInVariableBaseVertex, type,
                            kEmulatedGLBaseVertexName, uniforms))
        {
            return false;
        }
    }

    if (builtInVariableBaseInstance)
    {
        const TType *type = StaticType::Get<EbtInt, EbpHigh, EvqUniform, 1, 1>();
        if (!EmulateBuiltIn(compiler, root, symbolTable, builtInVariableBaseInstance, type,
                            kEmulatedGLBaseInstanceName, uniforms))
        {
            return false;
        }
    }

    // DeclareGlobalVariable prepends to the declarations, but the uniforms are appended.  So if
    // both base vertex and instance variables are added, the order doesn't match.  Fix that here.
    if (builtInVariableBaseVertex && builtInVariableBaseInstance)
    {
        const size_t count = uniforms->size();
        ASSERT(count >= 2);
        std::swap((*uniforms)[count - 1], (*uniforms)[count - 2]);
    }

    return true;
}

}  // namespace sh
