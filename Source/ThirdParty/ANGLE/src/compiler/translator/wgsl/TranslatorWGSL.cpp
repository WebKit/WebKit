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
#include "compiler/translator/OutputTree.h"
#include "compiler/translator/SymbolUniqueId.h"
#include "compiler/translator/Types.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{

constexpr bool kOutputTreeBeforeTranslation = false;
constexpr bool kOutputTranslatedShader      = false;

struct VarDecl
{
    const SymbolType symbolType = SymbolType::Empty;
    const ImmutableString &symbolName;
    const TType &type;
};

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
    struct EmitVariableDeclarationConfig
    {
        bool isParameter            = false;
        bool disableStructSpecifier = false;
    };

    void groupedTraverse(TIntermNode &node);
    void emitNameOf(const VarDecl &decl);
    template <typename T>
    void emitNameOf(const T &namedObject);
    void emitNameOf(SymbolType symbolType, const ImmutableString &name);
    void emitBareTypeName(const TType &type);
    void emitType(const TType &type);
    void emitIndentation();
    void emitOpenBrace();
    void emitCloseBrace();
    void emitFunctionSignature(const TFunction &func);
    void emitFunctionReturn(const TFunction &func);
    void emitFunctionParameter(const TFunction &func, const TVariable &param);
    void emitStructDeclaration(const TType &type);
    void emitVariableDeclaration(const VarDecl &decl,
                                 const EmitVariableDeclarationConfig &evdConfig);

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
    // TODO(anglebug.com/42267100): to make generated code more readable, do not always
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

void OutputWGSLTraverser::emitNameOf(const VarDecl &decl)
{
    emitNameOf(decl.symbolType, decl.symbolName);
}

// Can be used with TSymbol or TField or TFunc.
template <typename T>
void OutputWGSLTraverser::emitNameOf(const T &namedObject)
{
    emitNameOf(namedObject.symbolType(), namedObject.name());
}

void OutputWGSLTraverser::emitNameOf(SymbolType symbolType, const ImmutableString &name)
{
    switch (symbolType)
    {
        case SymbolType::BuiltIn:
        {
            mSink << name;
        }
        break;
        case SymbolType::UserDefined:
        {
            mSink << kUserDefinedNamePrefix << name;
        }
        break;
        case SymbolType::AngleInternal:
        case SymbolType::Empty:
            // TODO(anglebug.com/42267100): support these if necessary
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
    // TODO(anglebug.com/42267100): support emitting constants..
}

bool OutputWGSLTraverser::visitSwizzle(Visit, TIntermSwizzle *swizzleNode)
{
    // TODO(anglebug.com/42267100): support swizzle statements.
    return false;
}

bool OutputWGSLTraverser::visitBinary(Visit, TIntermBinary *binaryNode)
{
    // TODO(anglebug.com/42267100): support binary statements.
    return false;
}

bool OutputWGSLTraverser::visitUnary(Visit, TIntermUnary *unaryNode)
{
    // TODO(anglebug.com/42267100): support unary statements.
    return false;
}

bool OutputWGSLTraverser::visitTernary(Visit, TIntermTernary *conditionalNode)
{
    // TODO(anglebug.com/42267100): support ternaries.
    return false;
}

bool OutputWGSLTraverser::visitIfElse(Visit, TIntermIfElse *ifThenElseNode)
{
    // TODO(anglebug.com/42267100): support basic control flow.
    return false;
}

bool OutputWGSLTraverser::visitSwitch(Visit, TIntermSwitch *switchNode)
{
    // TODO(anglebug.com/42267100): support switch statements.
    return false;
}

bool OutputWGSLTraverser::visitCase(Visit, TIntermCase *caseNode)
{
    // TODO(anglebug.com/42267100): support switch statements.
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
    emitType(returnType);
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
    // TODO(anglebug.com/42267100): function parameters are immutable and will need to be renamed if
    // they are mutated.
    EmitVariableDeclarationConfig evdConfig;
    evdConfig.isParameter = true;
    emitVariableDeclaration({param.symbolType(), param.name(), param.getType()}, evdConfig);
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
    // TODO(anglebug.com/42267100): support aggregate statements.
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

void OutputWGSLTraverser::emitStructDeclaration(const TType &type)
{
    ASSERT(type.getBasicType() == TBasicType::EbtStruct);
    ASSERT(type.isStructSpecifier());

    mSink << "struct ";
    emitBareTypeName(type);

    mSink << "\n";
    emitOpenBrace();

    const TStructure &structure = *type.getStruct();

    for (const TField *field : structure.fields())
    {
        emitIndentation();
        // TODO(anglebug.com/42267100): emit qualifiers.
        EmitVariableDeclarationConfig evdConfig;
        evdConfig.disableStructSpecifier = true;
        emitVariableDeclaration({.symbolType = field->symbolType(),
                                 .symbolName = field->name(),
                                 .type       = *field->type()},
                                evdConfig);
        mSink << ",\n";
    }

    emitCloseBrace();
}

void OutputWGSLTraverser::emitVariableDeclaration(const VarDecl &decl,
                                                  const EmitVariableDeclarationConfig &evdConfig)
{
    const TBasicType basicType = decl.type.getBasicType();

    if (basicType == TBasicType::EbtStruct && decl.type.isStructSpecifier() &&
        !evdConfig.disableStructSpecifier)
    {
        // TODO(anglebug.com/42267100): in WGSL structs probably can't be declared in
        // function parameters or in uniform declarations or in variable declarations, or
        // anonymously either within other structs or within a variable declaration. Handle
        // these with the same AST pre-passes as other shader translators.
        ASSERT(!evdConfig.isParameter);
        emitStructDeclaration(decl.type);
        if (decl.symbolType != SymbolType::Empty)
        {
            mSink << " ";
            emitNameOf(decl);
        }
        return;
    }

    ASSERT(basicType == TBasicType::EbtStruct || decl.symbolType != SymbolType::Empty ||
           evdConfig.isParameter);

    if (decl.symbolType != SymbolType::Empty)
    {
        emitNameOf(decl);
    }
    mSink << " : ";
    emitType(decl.type);
}

bool OutputWGSLTraverser::visitDeclaration(Visit, TIntermDeclaration *declNode)
{
    ASSERT(declNode->getChildCount() == 1);
    TIntermNode &node = *declNode->getChildNode(0);

    EmitVariableDeclarationConfig evdConfig;

    // TODO(anglebug.com/42267100): emit let or var for function-local variables. (GLSL const or
    // default).
    if (TIntermSymbol *symbolNode = node.getAsSymbolNode())
    {
        const TVariable &var = symbolNode->variable();
        emitVariableDeclaration({var.symbolType(), var.name(), var.getType()}, evdConfig);
    }
    else if (TIntermBinary *initNode = node.getAsBinaryNode())
    {
        ASSERT(initNode->getOp() == TOperator::EOpInitialize);
        TIntermSymbol *leftSymbolNode = initNode->getLeft()->getAsSymbolNode();
        TIntermTyped *valueNode       = initNode->getRight()->getAsTyped();
        ASSERT(leftSymbolNode && valueNode);

        const TVariable &var = leftSymbolNode->variable();

        emitVariableDeclaration({var.symbolType(), var.name(), var.getType()}, evdConfig);
        mSink << " = ";
        groupedTraverse(*valueNode);
    }
    else
    {
        UNREACHABLE();
    }

    return false;
}

bool OutputWGSLTraverser::visitLoop(Visit, TIntermLoop *loopNode)
{
    // TODO(anglebug.com/42267100): emit loops.
    return false;
}

bool OutputWGSLTraverser::visitBranch(Visit, TIntermBranch *branchNode)
{
    // TODO(anglebug.com/42267100): emit branch instructions.
    return false;
}

void OutputWGSLTraverser::visitPreprocessorDirective(TIntermPreprocessorDirective *node)
{
    // No preprocessor directives expected at this point.
    UNREACHABLE();
}

void OutputWGSLTraverser::emitBareTypeName(const TType &type)
{
    const TBasicType basicType = type.getBasicType();

    switch (basicType)
    {
        case TBasicType::EbtVoid:
        case TBasicType::EbtBool:
            mSink << type.getBasicString();
            break;
        // TODO(anglebug.com/42267100): is there double precision (f64) in GLSL? It doesn't really
        // exist in WGSL (i.e. f64 does not exist but AbstractFloat can handle 64 bits???) Metal
        // does not have 64 bit double precision types. It's being implemented in WGPU:
        // https://github.com/gpuweb/gpuweb/issues/2805
        case TBasicType::EbtFloat:
            mSink << "f32";
            break;
        case TBasicType::EbtInt:
            mSink << "i32";
            break;
        case TBasicType::EbtUInt:
            mSink << "u32";
            break;

        case TBasicType::EbtStruct:
            emitNameOf(*type.getStruct());
            break;

        case TBasicType::EbtInterfaceBlock:
            emitNameOf(*type.getInterfaceBlock());
            break;

        default:
            if (IsSampler(basicType))
            {
                //  TODO(anglebug.com/42267100): possibly emit both a sampler and a texture2d. WGSL
                //  has sampler variables for the sampler configuration, whereas GLSL has sampler2d
                //  and other sampler* variables for an actual texture.
                mSink << "texture2d<";
                switch (type.getBasicType())
                {
                    case EbtSampler2D:
                        mSink << "f32";
                        break;
                    case EbtISampler2D:
                        mSink << "i32";
                        break;
                    case EbtUSampler2D:
                        mSink << "u32";
                        break;
                    default:
                        // TODO(anglebug.com/42267100): are any of the other sampler types necessary
                        // to translate?
                        UNIMPLEMENTED();
                        break;
                }
                if (type.getMemoryQualifier().readonly || type.getMemoryQualifier().writeonly)
                {
                    // TODO(anglebug.com/42267100): implement memory qualifiers.
                    UNIMPLEMENTED();
                }
                mSink << ">";
            }
            else if (IsImage(basicType))
            {
                // TODO(anglebug.com/42267100): does texture2d also correspond to GLSL's image type?
                mSink << "texture2d<";
                switch (type.getBasicType())
                {
                    case EbtImage2D:
                        mSink << "f32";
                        break;
                    case EbtIImage2D:
                        mSink << "i32";
                        break;
                    case EbtUImage2D:
                        mSink << "u32";
                        break;
                    default:
                        // TODO(anglebug.com/42267100): are any of the other image types necessary
                        // to translate?
                        UNIMPLEMENTED();
                        break;
                }
                if (type.getMemoryQualifier().readonly || type.getMemoryQualifier().writeonly)
                {
                    // TODO(anglebug.com/42267100): implement memory qualifiers.
                    UNREACHABLE();
                }
                mSink << ">";
            }
            else
            {
                UNREACHABLE();
            }
            break;
    }
}

void OutputWGSLTraverser::emitType(const TType &type)
{
    if (type.isArray())
    {
        // Examples:
        // array<f32, 5>
        // array<array<u32, 5>, 10>
        mSink << "array<";
        TType innerType = type;
        innerType.toArrayElementType();
        emitType(innerType);
        mSink << ", " << type.getOutermostArraySize() << ">";
    }
    else if (type.isVector())
    {
        mSink << "vec" << static_cast<uint32_t>(type.getNominalSize()) << "<";
        emitBareTypeName(type);
        mSink << ">";
    }
    else if (type.isMatrix())
    {
        mSink << "mat" << static_cast<uint32_t>(type.getCols()) << "x"
              << static_cast<uint32_t>(type.getRows()) << "<";
        emitBareTypeName(type);
        mSink << ">";
    }
    else
    {
        // This type has no dimensions and is equivalent to its bare type.
        emitBareTypeName(type);
    }
}

}  // namespace

TranslatorWGSL::TranslatorWGSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : TCompiler(type, spec, output)
{}

bool TranslatorWGSL::translate(TIntermBlock *root,
                               const ShCompileOptions &compileOptions,
                               PerformanceDiagnostics *perfDiagnostics)
{

    if (kOutputTreeBeforeTranslation)
    {
        OutputTree(root, getInfoSink().info);
        std::cout << getInfoSink().info.c_str();
    }

    // TODO(anglebug.com/8662): until the translator is ready to translate most basic shaders, emit
    // the code commented out.
    TInfoSinkBase &sink = getInfoSink().obj;
    sink << "/*\n";
    OutputWGSLTraverser traverser(this);
    root->traverse(&traverser);
    sink << "*/\n";

    if (kOutputTranslatedShader)
    {
        std::cout << getInfoSink().obj.str();
    }
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
