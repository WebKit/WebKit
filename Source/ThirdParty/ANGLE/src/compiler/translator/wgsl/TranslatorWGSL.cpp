//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/TranslatorWGSL.h"

#include "GLSLANG/ShaderLang.h"
#include "common/log_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Diagnostics.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{

// When emitting a list of statements, this determines whether a semicolon follows the statement.
bool RequiresSemicolonTerminator(TIntermNode &node)
{
    if (node.getAsBlock())
    {
        return false;
    }
    if (node.getAsLoopNode())
    {
        return false;
    }
    if (node.getAsSwitchNode())
    {
        return false;
    }
    if (node.getAsIfElseNode())
    {
        return false;
    }
    if (node.getAsFunctionDefinition())
    {
        return false;
    }
    if (node.getAsCaseNode())
    {
        return false;
    }

    return true;
}

// For pretty formatting of the resulting WGSL text.
bool NewlinePad(TIntermNode &node)
{
    if (node.getAsFunctionDefinition())
    {
        return true;
    }
    if (TIntermDeclaration *declNode = node.getAsDeclarationNode())
    {
        ASSERT(declNode->getChildCount() == 1);
        TIntermNode &childNode = *declNode->getChildNode(0);
        if (TIntermSymbol *symbolNode = childNode.getAsSymbolNode())
        {
            const TVariable &var = symbolNode->variable();
            return var.getType().isStructSpecifier();
        }
        return false;
    }
    return false;
}

// A traverser that generates WGSL as it walks the AST.
class OutputWGSLTraverser : public TIntermTraverser
{
  public:
    OutputWGSLTraverser(TCompiler *compiler);
    ~OutputWGSLTraverser() override;

  protected:
    void visitSymbol(TIntermSymbol *node) override;
    void visitConstantUnion(TIntermConstantUnion *node) override;
    bool visitSwizzle(Visit visit, TIntermSwizzle *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitUnary(Visit visit, TIntermUnary *node) override;
    bool visitTernary(Visit visit, TIntermTernary *node) override;
    bool visitIfElse(Visit visit, TIntermIfElse *node) override;
    bool visitSwitch(Visit visit, TIntermSwitch *node) override;
    bool visitCase(Visit visit, TIntermCase *node) override;
    void visitFunctionPrototype(TIntermFunctionPrototype *node) override;
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitBlock(Visit visit, TIntermBlock *node) override;
    bool visitGlobalQualifierDeclaration(Visit visit,
                                         TIntermGlobalQualifierDeclaration *node) override;
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitLoop(Visit visit, TIntermLoop *node) override;
    bool visitBranch(Visit visit, TIntermBranch *node) override;
    void visitPreprocessorDirective(TIntermPreprocessorDirective *node) override;

  private:
    void groupedTraverse(TIntermNode &node);
    template <typename T>
    void emitNameOf(const T &namedObject);
    void emitIndentation();
    void emitOpenBrace();
    void emitCloseBrace();
    void emitFunctionSignature(const TFunction &func);
    void emitFunctionReturn(const TFunction &func);
    void emitFunctionParameter(const TFunction &func, const TVariable &param);

    TInfoSinkBase &mSink;

    int mIndentLevel        = -1;
    int mLastIndentationPos = -1;
};

OutputWGSLTraverser::OutputWGSLTraverser(TCompiler *compiler)
    : TIntermTraverser(true, false, false), mSink(compiler->getInfoSink().obj)
{}

OutputWGSLTraverser::~OutputWGSLTraverser() = default;

void OutputWGSLTraverser::groupedTraverse(TIntermNode &node)
{
    // TODO(anglebug.com/8662): to make generated code more readable, do not always
    // emit parentheses like WGSL is some Lisp dialect.
    const bool emitParens = true;

    if (emitParens)
    {
        mSink << "(";
    }

    node.traverse(this);

    if (emitParens)
    {
        mSink << ")";
    }
}

// Can be used with TSymbol or TField. Must have a .name() and a .symbolType().
template <typename T>
void OutputWGSLTraverser::emitNameOf(const T &namedObject)
{
    switch (namedObject.symbolType())
    {
        case SymbolType::BuiltIn:
        {
            mSink << namedObject.name();
        }
        break;
        case SymbolType::UserDefined:
        {
            mSink << kUserDefinedNamePrefix << namedObject.name();
        }
        break;
        case SymbolType::AngleInternal:
        case SymbolType::Empty:
            // TODO(anglebug.com/8662): support these if necessary
            UNREACHABLE();
    }
}

void OutputWGSLTraverser::emitIndentation()
{
    ASSERT(mIndentLevel >= 0);

    if (mLastIndentationPos == mSink.size())
    {
        return;  // Line is already indented.
    }

    for (int i = 0; i < mIndentLevel; ++i)
    {
        mSink << "  ";
    }

    mLastIndentationPos = mSink.size();
}

void OutputWGSLTraverser::emitOpenBrace()
{
    ASSERT(mIndentLevel >= 0);

    emitIndentation();
    mSink << "{\n";
    ++mIndentLevel;
}

void OutputWGSLTraverser::emitCloseBrace()
{
    ASSERT(mIndentLevel >= 1);

    --mIndentLevel;
    emitIndentation();
    mSink << "}";
}

void OutputWGSLTraverser::visitSymbol(TIntermSymbol *symbolNode)
{

    const TVariable &var = symbolNode->variable();
    const TType &type    = var.getType();
    ASSERT(var.symbolType() != SymbolType::Empty);

    if (type.getBasicType() == TBasicType::EbtVoid)
    {
        UNREACHABLE();
    }
    else
    {
        emitNameOf(var);
    }
}

void OutputWGSLTraverser::visitConstantUnion(TIntermConstantUnion *constValueNode)
{
    // TODO(anglebug.com/8662): support emitting constants..
}

bool OutputWGSLTraverser::visitSwizzle(Visit, TIntermSwizzle *swizzleNode)
{
    // TODO(anglebug.com/8662): support swizzle statements.
    return false;
}

bool OutputWGSLTraverser::visitBinary(Visit, TIntermBinary *binaryNode)
{
    // TODO(anglebug.com/8662): support binary statements.
    return false;
}

bool OutputWGSLTraverser::visitUnary(Visit, TIntermUnary *unaryNode)
{
    // TODO(anglebug.com/8662): support unary statements.
    return false;
}

bool OutputWGSLTraverser::visitTernary(Visit, TIntermTernary *conditionalNode)
{
    // TODO(anglebug.com/8662): support ternaries.
    return false;
}

bool OutputWGSLTraverser::visitIfElse(Visit, TIntermIfElse *ifThenElseNode)
{
    // TODO(anglebug.com/8662): support basic control flow.
    return false;
}

bool OutputWGSLTraverser::visitSwitch(Visit, TIntermSwitch *switchNode)
{
    // TODO(anglebug.com/8662): support switch statements.
    return false;
}

bool OutputWGSLTraverser::visitCase(Visit, TIntermCase *caseNode)
{
    // TODO(anglebug.com/8662): support switch statements.
    return false;
}

void OutputWGSLTraverser::emitFunctionReturn(const TFunction &func)
{
    const TType &returnType = func.getReturnType();
    if (returnType.getBasicType() == EbtVoid)
    {
        return;
    }
    mSink << " -> ";
    mSink << "FAKE_RETURN_TYPE";
}

// TODO(anglebug.com/42267100): Function overloads are not supported in WGSL, so function names
// should either be emitted mangled or overloaded functions should be renamed in the AST as a
// pre-pass. As of Apr 2024, WGSL function overloads are "not coming soon"
// (https://github.com/gpuweb/gpuweb/issues/876).
void OutputWGSLTraverser::emitFunctionSignature(const TFunction &func)
{
    // TODO(anglebug.com/42267100): main functions should be renamed and labeled with @vertex or
    // @fragment.
    mSink << "fn ";

    emitNameOf(func);
    mSink << "(";

    bool emitComma          = false;
    const size_t paramCount = func.getParamCount();
    for (size_t i = 0; i < paramCount; ++i)
    {
        if (emitComma)
        {
            mSink << ", ";
        }
        emitComma = true;

        const TVariable &param = *func.getParam(i);
        emitFunctionParameter(func, param);
    }

    mSink << ")";

    emitFunctionReturn(func);
}

void OutputWGSLTraverser::emitFunctionParameter(const TFunction &func, const TVariable &param)
{
    // TODO(anglebug.com/8662): actually emit function parameters.

    mSink << "FAKE_FUNCTION_PARAMETER";
}

void OutputWGSLTraverser::visitFunctionPrototype(TIntermFunctionPrototype *funcProtoNode)
{
    const TFunction &func = *funcProtoNode->getFunction();

    emitIndentation();
    emitFunctionSignature(func);
}

bool OutputWGSLTraverser::visitFunctionDefinition(Visit, TIntermFunctionDefinition *funcDefNode)
{
    const TFunction &func = *funcDefNode->getFunction();
    TIntermBlock &body    = *funcDefNode->getBody();
    emitIndentation();
    emitFunctionSignature(func);
    mSink << "\n";
    body.traverse(this);
    return false;
}

bool OutputWGSLTraverser::visitAggregate(Visit, TIntermAggregate *aggregateNode)
{
    // TODO(anglebug.com/8662): support aggregate statements.
    return false;
}

bool OutputWGSLTraverser::visitBlock(Visit, TIntermBlock *blockNode)
{
    ASSERT(mIndentLevel >= -1);
    const bool isGlobalScope = mIndentLevel == -1;

    if (isGlobalScope)
    {
        ++mIndentLevel;
    }
    else
    {
        emitOpenBrace();
    }

    TIntermNode *prevStmtNode = nullptr;

    const size_t stmtCount = blockNode->getChildCount();
    for (size_t i = 0; i < stmtCount; ++i)
    {
        TIntermNode &stmtNode = *blockNode->getChildNode(i);

        if (isGlobalScope && prevStmtNode && (NewlinePad(*prevStmtNode) || NewlinePad(stmtNode)))
        {
            mSink << "\n";
        }
        const bool isCase = stmtNode.getAsCaseNode();
        mIndentLevel -= isCase;
        emitIndentation();
        mIndentLevel += isCase;
        stmtNode.traverse(this);
        if (RequiresSemicolonTerminator(stmtNode))
        {
            mSink << ";";
        }
        mSink << "\n";

        prevStmtNode = &stmtNode;
    }

    if (isGlobalScope)
    {
        ASSERT(mIndentLevel == 0);
        --mIndentLevel;
    }
    else
    {
        emitCloseBrace();
    }

    return false;
}

bool OutputWGSLTraverser::visitGlobalQualifierDeclaration(Visit,
                                                          TIntermGlobalQualifierDeclaration *)
{
    return false;
}

bool OutputWGSLTraverser::visitDeclaration(Visit, TIntermDeclaration *declNode)
{
    // TODO(anglebug.com/8662): support variable declarations.
    mSink << "FAKE_DECLARATION";
    return false;
}

bool OutputWGSLTraverser::visitLoop(Visit, TIntermLoop *loopNode)
{
    // TODO(anglebug.com/8662): emit loops.
    return false;
}

bool OutputWGSLTraverser::visitBranch(Visit, TIntermBranch *branchNode)
{
    // TODO(anglebug.com/8662): emit branch instructions.
    return false;
}

void OutputWGSLTraverser::visitPreprocessorDirective(TIntermPreprocessorDirective *node)
{
    // No preprocessor directives expected at this point.
    UNREACHABLE();
}

}  // namespace

TranslatorWGSL::TranslatorWGSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : TCompiler(type, spec, output)
{}

bool TranslatorWGSL::translate(TIntermBlock *root,
                               const ShCompileOptions &compileOptions,
                               PerformanceDiagnostics *perfDiagnostics)
{
    // TODO(anglebug.com/8662): until the translator is ready to translate most basic shaders, emit
    // the code commented out.
    TInfoSinkBase &sink = getInfoSink().obj;
    sink << "/*\n";
    OutputWGSLTraverser traverser(this);
    root->traverse(&traverser);
    sink << "*/\n";

    std::cout << getInfoSink().obj.str();

    // TODO(anglebug.com/8662): delete this.
    if (getShaderType() == GL_VERTEX_SHADER)
    {
        constexpr const char *kVertexShader = R"(@vertex
fn main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f
{
    const pos = array(
        vec2( 0.0,  0.5),
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5)
    );

    return vec4f(pos[vertex_index % 3], 0, 1);
})";
        sink << kVertexShader;
    }
    else if (getShaderType() == GL_FRAGMENT_SHADER)
    {
        constexpr const char *kFragmentShader = R"(@fragment
fn main() -> @location(0) vec4f
{
    return vec4(1, 0, 0, 1);
})";
        sink << kFragmentShader;
    }
    else
    {
        UNREACHABLE();
        return false;
    }

    return true;
}

bool TranslatorWGSL::shouldFlattenPragmaStdglInvariantAll()
{
    // Not neccesary for WGSL transformation.
    return false;
}
}  // namespace sh
