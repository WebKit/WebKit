//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ir/src/output/ir_to_legacy.h"

#include "compiler/translator/ir/src/output/legacy.rs.h"

#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Types.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/util.h"

#include <vector>

namespace sh
{
namespace ir
{
namespace ffi
{

namespace
{
ImmutableString Str(rust::Str str)
{
    ImmutableStringBuilder builder(str.length());
    builder << ImmutableString(str.data(), str.length());
    return builder;
}

ImmutableString Str(const SymbolName &name)
{
    constexpr uint32_t kNoId = 0xFFFF'FFFF;
    const bool appendId      = name.id != kNoId;
    ImmutableStringBuilder builder(name.name.length() + (appendId ? 1 + 11 : 0));
    builder << ImmutableString(name.name.data(), name.name.length());
    if (appendId)
    {
        builder << '_';
        builder << name.id;
    }
    return builder;
}

TType *Type(const TType *baseType, const ASTType &astType)
{
    TType *completeType = new TType(*baseType);

    TLayoutQualifier layoutQualifier             = completeType->getLayoutQualifier();
    const ASTLayoutQualifier &astLayoutQualifier = astType.layout_qualifier;
    layoutQualifier.location                     = astLayoutQualifier.location;
    layoutQualifier.locationsSpecified           = layoutQualifier.location >= 0;
    layoutQualifier.matrixPacking =
        static_cast<TLayoutMatrixPacking>(astLayoutQualifier.matrix_packing);
    layoutQualifier.blockStorage =
        static_cast<TLayoutBlockStorage>(astLayoutQualifier.block_storage);
    layoutQualifier.binding      = astLayoutQualifier.binding;
    layoutQualifier.offset       = astLayoutQualifier.offset;
    layoutQualifier.pushConstant = astLayoutQualifier.push_constant;
    layoutQualifier.depth        = static_cast<TLayoutDepth>(astLayoutQualifier.depth);
    layoutQualifier.imageInternalFormat =
        static_cast<TLayoutImageInternalFormat>(astLayoutQualifier.image_internal_format);
    layoutQualifier.numViews             = astLayoutQualifier.num_views;
    layoutQualifier.yuv                  = astLayoutQualifier.yuv;
    layoutQualifier.index                = astLayoutQualifier.index;
    layoutQualifier.inputAttachmentIndex = astLayoutQualifier.input_attachment_index;
    layoutQualifier.noncoherent          = astLayoutQualifier.noncoherent;
    layoutQualifier.rasterOrdered        = astLayoutQualifier.raster_ordered;

    TMemoryQualifier memoryQualifier             = completeType->getMemoryQualifier();
    const ASTMemoryQualifier &astMemoryQualifier = astType.memory_qualifier;
    memoryQualifier.readonly                     = astMemoryQualifier.readonly;
    memoryQualifier.writeonly                    = astMemoryQualifier.writeonly;
    memoryQualifier.coherent                     = astMemoryQualifier.coherent;
    memoryQualifier.restrictQualifier            = astMemoryQualifier.restrict_qualifier;
    memoryQualifier.volatileQualifier            = astMemoryQualifier.volatile_qualifier;

    completeType->setQualifier(static_cast<TQualifier>(astType.qualifier));
    completeType->setPrecision(static_cast<TPrecision>(astType.precision));
    completeType->setInvariant(astType.invariant);
    completeType->setPrecise(astType.precise);
    completeType->setInterpolant(astType.interpolant);
    completeType->setLayoutQualifier(layoutQualifier);
    completeType->setMemoryQualifier(memoryQualifier);

    return completeType;
}

TIntermTyped *Expr(const Expression &expr)
{
    return expr.needs_copy ? expr.node->deepCopy() : expr.node;
}

TIntermSequence Exprs(rust::Slice<const Expression> exprs)
{
    TIntermSequence seq;
    for (const Expression &expr : exprs)
    {
        seq.push_back(Expr(expr));
    }
    return seq;
}

TIntermTyped *UnaryBuiltIn(TCompiler *compiler, const char *name, const Expression &operand)
{
    TIntermTyped *operandNode = Expr(operand);
    return CreateBuiltInUnaryFunctionCallNode(name, operandNode, compiler->getSymbolTable(),
                                              compiler->getShaderVersion());
}

TIntermTyped *BinaryBuiltIn(TCompiler *compiler,
                            const char *name,
                            const Expression &lhs,
                            const Expression &rhs)
{
    TIntermTyped *lhsNode = Expr(lhs);
    TIntermTyped *rhsNode = Expr(rhs);
    return CreateBuiltInFunctionCallNode(name, {lhsNode, rhsNode}, compiler->getSymbolTable(),
                                         compiler->getShaderVersion());
}

TIntermTyped *BuiltIn(TCompiler *compiler, const char *name, rust::Slice<const Expression> args)
{
    TIntermSequence argsNodes = Exprs(args);
    return CreateBuiltInFunctionCallNode(name, &argsNodes, compiler->getSymbolTable(),
                                         compiler->getShaderVersion());
}
}  // namespace

TType *make_basic_type(ASTBasicType basicType)
{
    return new TType(static_cast<TBasicType>(basicType));
}

TType *make_vector_type(const TType *scalarType, uint32_t count)
{
    return new TType(scalarType->getBasicType(), EbpHigh, EvqTemporary, count);
}

TType *make_matrix_type(const TType *vectorType, uint32_t count)
{
    return new TType(vectorType->getBasicType(), EbpHigh, EvqTemporary, count,
                     vectorType->getNominalSize());
}

TType *make_array_type(const TType *elementType, uint32_t count)
{
    TType *arrayType = new TType(*elementType);
    arrayType->makeArray(count);
    return arrayType;
}

TType *make_unsized_array_type(const TType *elementType)
{
    return make_array_type(elementType, 0);
}

TType *make_struct_type(TCompiler *compiler,
                        const SymbolName &name,
                        rust::Slice<const ASTFieldInfo> fields,
                        bool isInterfaceBlock)
{
    const SymbolType structSymbolType = static_cast<SymbolType>(name.symbol_type);
    // The field symbol types usually inherit from the struct itself.  Except when the symbol type
    // is empty (nameless struct), the fields are still user-defined.
    const SymbolType fieldSymbolType = Str(name) == "" ? SymbolType::UserDefined : structSymbolType;

    TFieldList *fieldList = new TFieldList();
    fieldList->reserve(fields.size());
    for (const ASTFieldInfo &fieldInfo : fields)
    {
        TType *fieldType = Type(fieldInfo.base_type, fieldInfo.ast_type);
        if (fieldSymbolType == SymbolType::BuiltIn)
        {
            if (fieldInfo.name == "gl_Position")
            {
                fieldType->setQualifier(EvqPosition);
            }
            if (fieldInfo.name == "gl_PointSize")
            {
                fieldType->setQualifier(EvqPointSize);
            }
            if (fieldInfo.name == "gl_ClipDistance")
            {
                fieldType->setQualifier(EvqClipDistance);
            }
            if (fieldInfo.name == "gl_CullDistance")
            {
                fieldType->setQualifier(EvqCullDistance);
            }
        }

        fieldList->push_back(
            new TField(fieldType, Str(fieldInfo.name), TSourceLoc(), fieldSymbolType));
    }

    if (isInterfaceBlock)
    {
        // Note: Information expected in TLayoutQualifier is set when the interface block type is
        // used to declare a variable, where the decorations are.
        TInterfaceBlock *interfaceBlock =
            new TInterfaceBlock(&compiler->getSymbolTable(), Str(name), fieldList,
                                TLayoutQualifier{}, structSymbolType);
        compiler->getSymbolTable().redeclare(interfaceBlock);
        // Same with qualifier and layoutQualifier.
        return new TType(interfaceBlock, EvqTemporary, TLayoutQualifier{});
    }
    else
    {
        TStructure *structure =
            new TStructure(&compiler->getSymbolTable(), Str(name), fieldList, structSymbolType);
        return new TType(structure, false);
    }
}

TIntermNode *declare_struct(TCompiler *compiler, const TType *structType)
{
    TType *structSpecifierType = new TType(structType->getStruct(), true);
    structSpecifierType->setQualifier(EvqGlobal);

    TVariable *structVariable = new TVariable(&compiler->getSymbolTable(), kEmptyImmutableString,
                                              structSpecifierType, SymbolType::Empty);
    TIntermSymbol *structDeclarator       = new TIntermSymbol(structVariable);
    TIntermDeclaration *structDeclaration = new TIntermDeclaration;
    structDeclaration->appendDeclarator(structDeclarator);

    return structDeclaration;
}

TIntermTyped *make_float_constant(float f)
{
    return CreateFloatNode(f, EbpUndefined);
}

TIntermTyped *make_int_constant(int32_t i)
{
    return CreateIndexNode(i);
}

TIntermTyped *make_uint_constant(uint32_t u)
{
    return CreateUIntNode(u);
}

TIntermTyped *make_bool_constant(bool b)
{
    return CreateBoolNode(b);
}

TIntermTyped *make_yuv_csc_constant(ASTYuvCscStandardEXT yuvCsc)
{
    return CreateYuvCscNode(static_cast<TYuvCscStandardEXT>(yuvCsc));
}

TIntermTyped *make_composite_constant(rust::Slice<TIntermTyped *const> elements,
                                      const TType *constantType)
{
    TInfoSinkBase unusedSink;
    TDiagnostics unused(unusedSink);

    TType constantTypeQualified = TType(*constantType);
    constantTypeQualified.setQualifier(EvqConst);

    TIntermSequence args(elements.begin(), elements.end());
    return TIntermAggregate::CreateConstructor(constantTypeQualified, &args)->fold(&unused);
}

TIntermTyped *make_constant_variable(TCompiler *compiler,
                                     const TType *constantType,
                                     TIntermTyped *value)
{
    ASSERT(value->hasConstantValue());

    TType *constantTypeQualified = new TType(*constantType);
    constantTypeQualified->setQualifier(EvqConst);

    // If the IR was unable to assign a precision to the constant, it means it was not used in any
    // context that needed a precision, for example `(variable ? const1 : const2) < const3`
    if (IsPrecisionApplicableToType(constantType->getBasicType()) &&
        constantType->getPrecision() == EbpUndefined)
    {
        constantTypeQualified->setPrecision(EbpHigh);
    }

    TVariable *variable = new TVariable(&compiler->getSymbolTable(), kEmptyImmutableString,
                                        constantTypeQualified, SymbolType::AngleInternal);

    const TConstantUnion *constArray = value->getConstantValue();
    if (constArray)
    {
        variable->shareConstPointer(constArray);
    }

    return new TIntermSymbol(variable);
}

TIntermTyped *make_variable(TCompiler *compiler,
                            const SymbolName &name,
                            const TType *baseType,
                            const ASTType &astType,
                            bool isRedeclaredBuiltIn,
                            bool isStaticUse)
{
    SymbolType symbolType     = static_cast<SymbolType>(name.symbol_type);
    const TVariable *variable = nullptr;

    if (symbolType == SymbolType::BuiltIn && !isRedeclaredBuiltIn)
    {
        variable = static_cast<const TVariable *>(
            compiler->getSymbolTable().findBuiltIn(Str(name.name), compiler->getShaderVersion()));
        ASSERT(variable != nullptr);
    }
    else
    {
        // If the variable is an interface block, AST specifies some variable information in
        // TInterfaceBlock too.  Adjust the TInterfaceBlock right now, overriding const-ness knowing
        // that the interface block is paired with a single variable anyway.
        TType *varType = Type(baseType, astType);
        if (varType->isInterfaceBlock())
        {
            TInterfaceBlock *block = const_cast<TInterfaceBlock *>(varType->getInterfaceBlock());
            block->setBlockStorage(varType->getLayoutQualifier().blockStorage);
            block->setBlockBinding(varType->getLayoutQualifier().binding);
        }

        TVariable *newVariable =
            new TVariable(&compiler->getSymbolTable(), Str(name), varType, symbolType);

        if (symbolType != SymbolType::Empty &&
            (varType->isInterfaceBlock() || IsShaderIn(varType->getQualifier()) ||
             IsShaderOut(varType->getQualifier())))
        {
            compiler->getSymbolTable().redeclare(newVariable);
        }
        variable = newVariable;
    }
    if (isStaticUse)
    {
        compiler->getSymbolTable().markStaticUse(*variable);
    }
    return new TIntermSymbol(variable);
}

TIntermTyped *make_nameless_block_field_variable(TCompiler *compiler,
                                                 TIntermTyped *variable,
                                                 uint32_t fieldIndex,
                                                 const SymbolName &name,
                                                 const TType *baseType,
                                                 const ASTType &astType)
{
    SymbolType symbolType                 = static_cast<SymbolType>(name.symbol_type);
    const TInterfaceBlock *interfaceBlock = variable->getType().getInterfaceBlock();

    // If the variable is an interface block, AST specifies some variable information in
    // TInterfaceBlock too.  Adjust the TInterfaceBlock right now, overriding const-ness knowing
    // that the interface block is paired with a single variable anyway.
    TType *fieldType = Type(baseType, astType);
    fieldType->setInterfaceBlockField(interfaceBlock, fieldIndex);
    fieldType->setQualifier(variable->getType().getQualifier());

    TVariable *field_variable =
        new TVariable(&compiler->getSymbolTable(), Str(name), fieldType, symbolType);
    compiler->getSymbolTable().redeclare(field_variable);
    return new TIntermSymbol(field_variable);
}

TIntermNode *declare_variable(TIntermTyped *variable)
{
    TIntermDeclaration *declaration = new TIntermDeclaration;
    declaration->appendDeclarator(variable->getAsSymbolNode());
    return declaration;
}

TIntermNode *declare_variable_with_initializer(TIntermTyped *variable, TIntermTyped *value)
{
    ASSERT(value->hasConstantValue());

    TIntermDeclaration *declaration = new TIntermDeclaration;
    TIntermBinary *init =
        new TIntermBinary(EOpInitialize, variable->getAsSymbolNode(), value->deepCopy());
    declaration->appendDeclarator(init);
    return declaration;
}

TIntermNode *globally_qualify_built_in_invariant(TIntermTyped *variable)
{
    return new TIntermGlobalQualifierDeclaration(variable->getAsSymbolNode()->deepCopy(), false,
                                                 TSourceLoc());
}

TIntermNode *globally_qualify_built_in_precise(TIntermTyped *variable)
{
    return new TIntermGlobalQualifierDeclaration(variable->getAsSymbolNode()->deepCopy(), true,
                                                 TSourceLoc());
}

TFunction *make_function(TCompiler *compiler,
                         const SymbolName &name,
                         const TType *returnType,
                         const ASTType &returnAstType,
                         rust::Slice<TIntermTyped *const> params,
                         rust::Slice<const ffi::ASTQualifier> paramDirections)
{
    TFunction *function = new TFunction(&compiler->getSymbolTable(), Str(name),
                                        static_cast<SymbolType>(name.symbol_type),
                                        Type(returnType, returnAstType), false);

    ASSERT(params.size() == paramDirections.size());
    for (size_t paramIndex = 0; paramIndex < params.size(); ++paramIndex)
    {
        const TVariable *param = &params[paramIndex]->getAsSymbolNode()->variable();

        // Update the qualifier of function param variables to the correct EvqParamIn/Out/InOut,
        // ignoring const-ness since each param is given its own TType.
        const TQualifier qualifier = static_cast<TQualifier>(paramDirections[paramIndex]);
        const_cast<TType &>(param->getType()).setQualifier(qualifier);

        function->addParameter(param);
    }

    return function;
}

TIntermNode *declare_function(const TFunction *function, TIntermBlock *body)
{
    return new TIntermFunctionDefinition(new TIntermFunctionPrototype(function), body);
}

TIntermBlock *make_interm_block()
{
    return new TIntermBlock;
}

void append_instructions_to_block(TIntermBlock *block, rust::Slice<TIntermNode *const> nodes)
{
    block->getSequence()->insert(block->getSequence()->end(), nodes.begin(), nodes.end());
}

void append_blocks_to_block(TIntermBlock *block, rust::Slice<TIntermBlock *const> blocksToAppend)
{
    for (const TIntermBlock *toAppend : blocksToAppend)
    {
        block->getSequence()->insert(block->getSequence()->end(), toAppend->getSequence()->begin(),
                                     toAppend->getSequence()->end());
    }
}

TIntermTyped *swizzle(const Expression &operand, rust::Slice<const uint32_t> indices)
{
    TVector<uint32_t> indicesVec(indices.begin(), indices.end());
    return new TIntermSwizzle(Expr(operand), indicesVec);
}

TIntermTyped *index(const Expression &operand, const Expression &index)
{
    TIntermTyped *operandNode              = Expr(operand);
    TIntermTyped *indexNode                = Expr(index);
    TIntermConstantUnion *indexNodeAsConst = indexNode->getAsConstantUnion();
    const TOperator op = indexNodeAsConst != nullptr ? EOpIndexDirect : EOpIndexIndirect;

    // The AST expects indices to be signed integers for some reason.
    if (op == EOpIndexDirect && indexNodeAsConst->getConstantValue()->getType() == EbtUInt)
    {
        indexNode = CreateIndexNode(indexNodeAsConst->getConstantValue()->getUConst());
    }

    return new TIntermBinary(op, operandNode, indexNode);
}

TIntermTyped *select_field(const Expression &operand, uint32_t fieldIndex)
{
    TIntermTyped *operandNode = Expr(operand);
    const TOperator op = operandNode->getType().isInterfaceBlock() ? EOpIndexDirectInterfaceBlock
                                                                   : EOpIndexDirectStruct;

    return new TIntermBinary(op, operandNode, CreateIndexNode(fieldIndex));
}

TIntermTyped *construct(const TType *type, rust::Slice<const Expression> operands)
{
    TIntermSequence operandsSequence = Exprs(operands);
    return TIntermAggregate::CreateConstructor(*type, &operandsSequence);
}

TIntermNode *store(const Expression &pointer, const Expression &value)
{
    return new TIntermBinary(EOpAssign, Expr(pointer), Expr(value));
}

TIntermTyped *call(const TFunction *function, rust::Slice<const Expression> args)
{
    TIntermSequence argsSequence = Exprs(args);
    return TIntermAggregate::CreateFunctionCall(*function, &argsSequence);
}

TIntermNode *call_void(const TFunction *function, rust::Slice<const Expression> args)
{
    return call(function, args);
}

TIntermTyped *array_length(const Expression &operand)
{
    return new TIntermUnary(EOpArrayLength, Expr(operand), nullptr);
}

TIntermTyped *negate(const Expression &operand)
{
    return new TIntermUnary(EOpNegative, Expr(operand), nullptr);
}

TIntermTyped *postfix_increment(const Expression &operand)
{
    return new TIntermUnary(EOpPostIncrement, Expr(operand), nullptr);
}

TIntermTyped *postfix_decrement(const Expression &operand)
{
    return new TIntermUnary(EOpPostDecrement, Expr(operand), nullptr);
}

TIntermTyped *prefix_increment(const Expression &operand)
{
    return new TIntermUnary(EOpPreIncrement, Expr(operand), nullptr);
}

TIntermTyped *prefix_decrement(const Expression &operand)
{
    return new TIntermUnary(EOpPreDecrement, Expr(operand), nullptr);
}

TIntermTyped *logical_not(const Expression &operand)
{
    return new TIntermUnary(EOpLogicalNot, Expr(operand), nullptr);
}

TIntermTyped *bitwise_not(const Expression &operand)
{
    return new TIntermUnary(EOpBitwiseNot, Expr(operand), nullptr);
}

TIntermTyped *built_in_radians(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "radians", operand);
}

TIntermTyped *built_in_degrees(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "degrees", operand);
}

TIntermTyped *built_in_sin(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "sin", operand);
}

TIntermTyped *built_in_cos(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "cos", operand);
}

TIntermTyped *built_in_tan(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "tan", operand);
}

TIntermTyped *built_in_asin(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "asin", operand);
}

TIntermTyped *built_in_acos(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "acos", operand);
}

TIntermTyped *built_in_atan(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "atan", operand);
}

TIntermTyped *built_in_sinh(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "sinh", operand);
}

TIntermTyped *built_in_cosh(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "cosh", operand);
}

TIntermTyped *built_in_tanh(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "tanh", operand);
}

TIntermTyped *built_in_asinh(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "asinh", operand);
}

TIntermTyped *built_in_acosh(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "acosh", operand);
}

TIntermTyped *built_in_atanh(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "atanh", operand);
}

TIntermTyped *built_in_exp(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "exp", operand);
}

TIntermTyped *built_in_log(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "log", operand);
}

TIntermTyped *built_in_exp2(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "exp2", operand);
}

TIntermTyped *built_in_log2(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "log2", operand);
}

TIntermTyped *built_in_sqrt(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "sqrt", operand);
}

TIntermTyped *built_in_inversesqrt(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "inversesqrt", operand);
}

TIntermTyped *built_in_abs(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "abs", operand);
}

TIntermTyped *built_in_sign(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "sign", operand);
}

TIntermTyped *built_in_floor(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "floor", operand);
}

TIntermTyped *built_in_trunc(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "trunc", operand);
}

TIntermTyped *built_in_round(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "round", operand);
}

TIntermTyped *built_in_roundeven(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "roundEven", operand);
}

TIntermTyped *built_in_ceil(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "ceil", operand);
}

TIntermTyped *built_in_fract(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "fract", operand);
}

TIntermTyped *built_in_isnan(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "isnan", operand);
}

TIntermTyped *built_in_isinf(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "isinf", operand);
}

TIntermTyped *built_in_floatbitstoint(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "floatBitsToInt", operand);
}

TIntermTyped *built_in_floatbitstouint(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "floatBitsToUint", operand);
}

TIntermTyped *built_in_intbitstofloat(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "intBitsToFloat", operand);
}

TIntermTyped *built_in_uintbitstofloat(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "uintBitsToFloat", operand);
}

TIntermTyped *built_in_packsnorm2x16(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "packSnorm2x16", operand);
}

TIntermTyped *built_in_packhalf2x16(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "packHalf2x16", operand);
}

TIntermTyped *built_in_unpacksnorm2x16(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "unpackSnorm2x16", operand);
}

TIntermTyped *built_in_unpackhalf2x16(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "unpackHalf2x16", operand);
}

TIntermTyped *built_in_packunorm2x16(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "packUnorm2x16", operand);
}

TIntermTyped *built_in_unpackunorm2x16(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "unpackUnorm2x16", operand);
}

TIntermTyped *built_in_packunorm4x8(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "packUnorm4x8", operand);
}

TIntermTyped *built_in_packsnorm4x8(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "packSnorm4x8", operand);
}

TIntermTyped *built_in_unpackunorm4x8(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "unpackUnorm4x8", operand);
}

TIntermTyped *built_in_unpacksnorm4x8(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "unpackSnorm4x8", operand);
}

TIntermTyped *built_in_length(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "length", operand);
}

TIntermTyped *built_in_normalize(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "normalize", operand);
}

TIntermTyped *built_in_transpose(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "transpose", operand);
}

TIntermTyped *built_in_determinant(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "determinant", operand);
}

TIntermTyped *built_in_inverse(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "inverse", operand);
}

TIntermTyped *built_in_any(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "any", operand);
}

TIntermTyped *built_in_all(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "all", operand);
}

TIntermTyped *built_in_not(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "not", operand);
}

TIntermTyped *built_in_bitfieldreverse(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "bitfieldReverse", operand);
}

TIntermTyped *built_in_bitcount(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "bitCount", operand);
}

TIntermTyped *built_in_findlsb(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "findLSB", operand);
}

TIntermTyped *built_in_findmsb(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "findMSB", operand);
}

TIntermTyped *built_in_dfdx(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "dFdx", operand);
}

TIntermTyped *built_in_dfdy(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "dFdy", operand);
}

TIntermTyped *built_in_fwidth(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "fwidth", operand);
}

TIntermTyped *built_in_interpolateatcentroid(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "interpolateAtCentroid", operand);
}

TIntermTyped *built_in_atomiccounter(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "atomicCounter", operand);
}

TIntermTyped *built_in_atomiccounterincrement(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "atomicCounterIncrement", operand);
}

TIntermTyped *built_in_atomiccounterdecrement(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "atomicCounterDecrement", operand);
}

TIntermTyped *built_in_imagesize(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "imageSize", operand);
}

TIntermTyped *built_in_pixellocalload(TCompiler *compiler, const Expression &operand)
{
    return UnaryBuiltIn(compiler, "pixelLocalLoadANGLE", operand);
}

TIntermTyped *add(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpAdd, Expr(lhs), Expr(rhs));
}

TIntermTyped *sub(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpSub, Expr(lhs), Expr(rhs));
}

TIntermTyped *mul(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpMul, Expr(lhs), Expr(rhs));
}

TIntermTyped *vector_times_scalar(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpVectorTimesScalar, Expr(lhs), Expr(rhs));
}

TIntermTyped *matrix_times_scalar(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpMatrixTimesScalar, Expr(lhs), Expr(rhs));
}

TIntermTyped *vector_times_matrix(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpVectorTimesMatrix, Expr(lhs), Expr(rhs));
}

TIntermTyped *matrix_times_vector(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpMatrixTimesVector, Expr(lhs), Expr(rhs));
}

TIntermTyped *matrix_times_matrix(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpMatrixTimesMatrix, Expr(lhs), Expr(rhs));
}

TIntermTyped *div(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpDiv, Expr(lhs), Expr(rhs));
}

TIntermTyped *imod(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpIMod, Expr(lhs), Expr(rhs));
}

TIntermTyped *logical_xor(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpLogicalXor, Expr(lhs), Expr(rhs));
}

TIntermTyped *equal(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpEqual, Expr(lhs), Expr(rhs));
}

TIntermTyped *not_equal(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpNotEqual, Expr(lhs), Expr(rhs));
}

TIntermTyped *less_than(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpLessThan, Expr(lhs), Expr(rhs));
}

TIntermTyped *greater_than(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpGreaterThan, Expr(lhs), Expr(rhs));
}

TIntermTyped *less_than_equal(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpLessThanEqual, Expr(lhs), Expr(rhs));
}

TIntermTyped *greater_than_equal(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpGreaterThanEqual, Expr(lhs), Expr(rhs));
}

TIntermTyped *bit_shift_left(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpBitShiftLeft, Expr(lhs), Expr(rhs));
}

TIntermTyped *bit_shift_right(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpBitShiftRight, Expr(lhs), Expr(rhs));
}

TIntermTyped *bitwise_or(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpBitwiseOr, Expr(lhs), Expr(rhs));
}

TIntermTyped *bitwise_xor(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpBitwiseXor, Expr(lhs), Expr(rhs));
}

TIntermTyped *bitwise_and(const Expression &lhs, const Expression &rhs)
{
    return new TIntermBinary(EOpBitwiseAnd, Expr(lhs), Expr(rhs));
}

TIntermTyped *built_in_atan_binary(TCompiler *compiler,
                                   const Expression &lhs,
                                   const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atan", lhs, rhs);
}

TIntermTyped *built_in_pow(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "pow", lhs, rhs);
}

TIntermTyped *built_in_mod(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "mod", lhs, rhs);
}

TIntermTyped *built_in_min(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "min", lhs, rhs);
}

TIntermTyped *built_in_max(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "max", lhs, rhs);
}

TIntermTyped *built_in_step(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "step", lhs, rhs);
}

TIntermTyped *built_in_modf(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "modf", lhs, rhs);
}

TIntermTyped *built_in_frexp(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "frexp", lhs, rhs);
}

TIntermTyped *built_in_ldexp(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "ldexp", lhs, rhs);
}

TIntermTyped *built_in_distance(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "distance", lhs, rhs);
}

TIntermTyped *built_in_dot(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "dot", lhs, rhs);
}

TIntermTyped *built_in_cross(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "cross", lhs, rhs);
}

TIntermTyped *built_in_reflect(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "reflect", lhs, rhs);
}

TIntermTyped *built_in_matrixcompmult(TCompiler *compiler,
                                      const Expression &lhs,
                                      const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "matrixCompMult", lhs, rhs);
}

TIntermTyped *built_in_outerproduct(TCompiler *compiler,
                                    const Expression &lhs,
                                    const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "outerProduct", lhs, rhs);
}

TIntermTyped *built_in_lessthanvec(TCompiler *compiler,
                                   const Expression &lhs,
                                   const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "lessThan", lhs, rhs);
}

TIntermTyped *built_in_lessthanequalvec(TCompiler *compiler,
                                        const Expression &lhs,
                                        const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "lessThanEqual", lhs, rhs);
}

TIntermTyped *built_in_greaterthanvec(TCompiler *compiler,
                                      const Expression &lhs,
                                      const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "greaterThan", lhs, rhs);
}

TIntermTyped *built_in_greaterthanequalvec(TCompiler *compiler,
                                           const Expression &lhs,
                                           const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "greaterThanEqual", lhs, rhs);
}

TIntermTyped *built_in_equalvec(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "equal", lhs, rhs);
}

TIntermTyped *built_in_notequalvec(TCompiler *compiler,
                                   const Expression &lhs,
                                   const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "notEqual", lhs, rhs);
}

TIntermTyped *built_in_interpolateatsample(TCompiler *compiler,
                                           const Expression &lhs,
                                           const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "interpolateAtSample", lhs, rhs);
}

TIntermTyped *built_in_interpolateatoffset(TCompiler *compiler,
                                           const Expression &lhs,
                                           const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "interpolateAtOffset", lhs, rhs);
}

TIntermTyped *built_in_atomicadd(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicAdd", lhs, rhs);
}

TIntermTyped *built_in_atomicmin(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicMin", lhs, rhs);
}

TIntermTyped *built_in_atomicmax(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicMax", lhs, rhs);
}

TIntermTyped *built_in_atomicand(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicAnd", lhs, rhs);
}

TIntermTyped *built_in_atomicor(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicOr", lhs, rhs);
}

TIntermTyped *built_in_atomicxor(TCompiler *compiler, const Expression &lhs, const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicXor", lhs, rhs);
}

TIntermTyped *built_in_atomicexchange(TCompiler *compiler,
                                      const Expression &lhs,
                                      const Expression &rhs)
{
    return BinaryBuiltIn(compiler, "atomicExchange", lhs, rhs);
}

TIntermTyped *built_in_clamp(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "clamp", args);
}

TIntermTyped *built_in_mix(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "mix", args);
}

TIntermTyped *built_in_smoothstep(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "smoothstep", args);
}

TIntermTyped *built_in_fma(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "fma", args);
}

TIntermTyped *built_in_faceforward(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "faceforward", args);
}

TIntermTyped *built_in_refract(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "refract", args);
}

TIntermTyped *built_in_bitfieldextract(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "bitfieldExtract", args);
}

TIntermTyped *built_in_bitfieldinsert(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "bitfieldInsert", args);
}

TIntermTyped *built_in_uaddcarry(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "uaddCarry", args);
}

TIntermTyped *built_in_usubborrow(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "usubBorrow", args);
}

TIntermNode *built_in_umulextended(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "umulExtended", args);
}

TIntermNode *built_in_imulextended(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imulExtended", args);
}

TIntermTyped *built_in_texturesize(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "textureSize", args);
}

TIntermTyped *built_in_texturequerylod(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "textureQueryLOD", args);
}

TIntermTyped *built_in_texelfetch(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "texelFetch", args);
}

TIntermTyped *built_in_texelfetchoffset(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "texelFetchOffset", args);
}

TIntermTyped *built_in_rgb_2_yuv(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "rgb_2_yuv", args);
}

TIntermTyped *built_in_yuv_2_rgb(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "yuv_2_rgb", args);
}

TIntermTyped *built_in_atomiccompswap(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "atomicCompSwap", args);
}

TIntermNode *built_in_imagestore(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageStore", args);
}

TIntermTyped *built_in_imageload(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageLoad", args);
}

TIntermTyped *built_in_imageatomicadd(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicAdd", args);
}

TIntermTyped *built_in_imageatomicmin(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicMin", args);
}

TIntermTyped *built_in_imageatomicmax(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicMax", args);
}

TIntermTyped *built_in_imageatomicand(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicAnd", args);
}

TIntermTyped *built_in_imageatomicor(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicOr", args);
}

TIntermTyped *built_in_imageatomicxor(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicXor", args);
}

TIntermTyped *built_in_imageatomicexchange(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicExchange", args);
}

TIntermTyped *built_in_imageatomiccompswap(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "imageAtomicCompSwap", args);
}

TIntermNode *built_in_pixellocalstore(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "pixelLocalStoreANGLE", args);
}

TIntermNode *built_in_memorybarrier(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "memoryBarrier", args);
}

TIntermNode *built_in_memorybarrieratomiccounter(TCompiler *compiler,
                                                 rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "memoryBarrierAtomicCounter", args);
}

TIntermNode *built_in_memorybarrierbuffer(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "memoryBarrierBuffer", args);
}

TIntermNode *built_in_memorybarrierimage(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "memoryBarrierImage", args);
}

TIntermNode *built_in_barrier(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "barrier", args);
}

TIntermNode *built_in_memorybarriershared(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "memoryBarrierShared", args);
}

TIntermNode *built_in_groupmemorybarrier(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "groupMemoryBarrier", args);
}

TIntermNode *built_in_emitvertex(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "EmitVertex", args);
}

TIntermNode *built_in_endprimitive(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "EndPrimitive", args);
}

TIntermTyped *built_in_subpassload(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "subpassLoad", args);
}

TIntermNode *built_in_begininvocationinterlocknv(TCompiler *compiler,
                                                 rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "beginInvocationInterlockNV", args);
}

TIntermNode *built_in_endinvocationinterlocknv(TCompiler *compiler,
                                               rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "endInvocationInterlockNV", args);
}

TIntermNode *built_in_beginfragmentshaderorderingintel(TCompiler *compiler,
                                                       rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "beginFragmentShaderOrderingINTEL", args);
}

TIntermNode *built_in_begininvocationinterlockarb(TCompiler *compiler,
                                                  rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "beginInvocationInterlockARB", args);
}

TIntermNode *built_in_endinvocationinterlockarb(TCompiler *compiler,
                                                rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "endInvocationInterlockARB", args);
}

TIntermTyped *built_in_numsamples(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "numSamples", args);
}

TIntermTyped *built_in_sampleposition(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "samplePosition", args);
}

TIntermTyped *built_in_interpolateatcenter(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "interpolateAtCenter", args);
}

TIntermNode *built_in_loopforwardprogress(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "loopForwardProgress", args);
}

TIntermTyped *built_in_saturate(TCompiler *compiler, rust::Slice<const Expression> args)
{
    return BuiltIn(compiler, "saturate", args);
}

TIntermTyped *built_in_texture(TCompiler *compiler,
                               rust::Slice<const Expression> args,
                               ASTBasicType samplerType,
                               bool isProj)
{
    const char *builtIn = isProj ? "textureProj" : "texture";
    if (compiler->getShaderVersion() == 100)
    {
        switch (static_cast<TBasicType>(samplerType))
        {
            case EbtSampler2D:
            case EbtSamplerExternalOES:
                builtIn = isProj ? "texture2DProj" : "texture2D";
                break;
            case EbtSampler3D:
                builtIn = isProj ? "texture3DProj" : "texture3D";
                break;
            case EbtSamplerCube:
                ASSERT(!isProj);
                builtIn = "textureCube";
                break;
            case EbtSampler2DRect:
                builtIn = isProj ? "texture2DRectProj" : "texture2DRect";
                break;
            case EbtSampler2DShadow:
                builtIn = isProj ? "shadow2DProjEXT" : "shadow2DEXT";
                break;
            case EbtSamplerVideoWEBGL:
                ASSERT(!isProj);
                builtIn = "textureVideoWEBGL";
                break;
            default:
                ASSERT(false);
        }
    }
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_textureoffset(TCompiler *compiler,
                                     rust::Slice<const Expression> args,
                                     bool isProj)
{
    const char *builtIn = isProj ? "textureProjOffset" : "textureOffset";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texture_with_compare(TCompiler *compiler, rust::Slice<const Expression> args)
{
    const char *builtIn = "texture";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturelod(TCompiler *compiler,
                                  rust::Slice<const Expression> args,
                                  ASTBasicType samplerType,
                                  bool isProj)
{
    const char *builtIn = isProj ? "textureProjLod" : "textureLod";
    if (compiler->getShaderVersion() == 100)
    {
        switch (static_cast<TBasicType>(samplerType))
        {
            case EbtSampler2D:
                if (compiler->getShaderType() == GL_FRAGMENT_SHADER)
                {
                    builtIn = isProj ? "texture2DProjLodEXT" : "texture2DLodEXT";
                }
                else
                {
                    builtIn = isProj ? "texture2DProjLod" : "texture2DLod";
                }
                break;
            case EbtSampler3D:
                builtIn = isProj ? "texture3DProjLod" : "texture3DLod";
                break;
            case EbtSamplerCube:
                ASSERT(!isProj);
                if (compiler->getShaderType() == GL_FRAGMENT_SHADER)
                {
                    builtIn = "textureCubeLodEXT";
                }
                else
                {
                    builtIn = "textureCubeLod";
                }
                break;
            default:
                ASSERT(false);
        }
    }
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturelodoffset(TCompiler *compiler,
                                        rust::Slice<const Expression> args,
                                        bool isProj)
{
    const char *builtIn = isProj ? "textureProjLodOffset" : "textureLodOffset";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturelod_with_compare(TCompiler *compiler,
                                               rust::Slice<const Expression> args)
{
    const char *builtIn = "textureLod";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturegrad(TCompiler *compiler,
                                   rust::Slice<const Expression> args,
                                   ASTBasicType samplerType,
                                   bool isProj)
{
    const char *builtIn = isProj ? "textureProjGrad" : "textureGrad";
    if (compiler->getShaderVersion() == 100)
    {
        switch (static_cast<TBasicType>(samplerType))
        {
            case EbtSampler2D:
                builtIn = isProj ? "texture2DProjGradEXT" : "texture2DGradEXT";
                break;
            case EbtSamplerCube:
                ASSERT(!isProj);
                builtIn = "textureCubeGradEXT";
                break;
            default:
                ASSERT(false);
        }
    }
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturegradoffset(TCompiler *compiler,
                                         rust::Slice<const Expression> args,
                                         bool isProj)
{
    const char *builtIn = isProj ? "textureProjGradOffset" : "textureGradOffset";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturegather(TCompiler *compiler, rust::Slice<const Expression> args)
{
    const char *builtIn = "textureGather";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

TIntermTyped *built_in_texturegatheroffset(TCompiler *compiler,
                                           rust::Slice<const Expression> args,
                                           bool isOffsetArray)
{
    const char *builtIn = isOffsetArray ? "textureGatherOffsets" : "textureGatherOffset";
    ASSERT(compiler->getShaderVersion() >= 300);
    return BuiltIn(compiler, builtIn, args);
}

void branch_discard(TIntermBlock *block)
{
    block->appendStatement(new TIntermBranch(EOpKill, nullptr));
}

void branch_return_value(TIntermBlock *block, const Expression &value)
{
    block->appendStatement(new TIntermBranch(EOpReturn, Expr(value)));
}

void branch_return(TIntermBlock *block)
{
    block->appendStatement(new TIntermBranch(EOpReturn, nullptr));
}

void branch_break(TIntermBlock *block)
{
    block->appendStatement(new TIntermBranch(EOpBreak, nullptr));
}

void branch_continue(TIntermBlock *block)
{
    block->appendStatement(new TIntermBranch(EOpContinue, nullptr));
}

void branch_if(TIntermBlock *block, const Expression &condition, TIntermBlock *trueBlock)
{
    branch_if_else(block, condition, trueBlock, nullptr);
}

void branch_if_else(TIntermBlock *block,
                    const Expression &condition,
                    TIntermBlock *trueBlock,
                    TIntermBlock *falseBlock)
{
    block->appendStatement(new TIntermIfElse(Expr(condition), trueBlock, falseBlock));
}

void branch_loop(TIntermBlock *block, TIntermBlock *loopConditionBlock, TIntermBlock *bodyBlock)
{
    TIntermBlock *loopBody = new TIntermBlock;
    loopBody->appendStatement(loopConditionBlock);
    loopBody->appendStatement(bodyBlock);

    block->appendStatement(new TIntermLoop(ELoopFor, nullptr, nullptr, nullptr, loopBody));
}

void branch_do_loop(TIntermBlock *block, TIntermBlock *bodyBlock)
{
    block->appendStatement(new TIntermLoop(ELoopFor, nullptr, nullptr, nullptr, bodyBlock));
}

void branch_loop_if(TIntermBlock *block, const Expression &condition)
{
    TIntermTyped *notCondition = new TIntermUnary(EOpLogicalNot, Expr(condition), nullptr);

    TIntermBlock *breakBlock = new TIntermBlock;
    breakBlock->appendStatement(new TIntermBranch(EOpBreak, nullptr));

    block->appendStatement(new TIntermIfElse(notCondition, breakBlock, nullptr));
}

void branch_switch(TIntermBlock *block,
                   const Expression &value,
                   rust::Slice<TIntermTyped *const> caseLabels,
                   rust::Slice<TIntermBlock *const> caseBlocks)
{
    ASSERT(caseLabels.size() == caseBlocks.size());

    TIntermBlock *switchBody = new TIntermBlock;
    for (size_t caseIndex = 0; caseIndex < caseLabels.size(); ++caseIndex)
    {
        TIntermBlock *caseBlock = caseBlocks[caseIndex];
        TIntermTyped *label     = caseLabels[caseIndex];
        if (label != nullptr)
        {
            ASSERT(label->getAsConstantUnion());
            label = label->deepCopy();
        }

        switchBody->appendStatement(new TIntermCase(label));
        switchBody->appendStatement(caseBlock);
    }

    block->appendStatement(new TIntermSwitch(Expr(value), switchBody));
}

TIntermBlock *finalize(TCompiler *compiler,
                       rust::Slice<TIntermNode *const> typeDeclarations,
                       rust::Slice<TIntermNode *const> globalVariables,
                       rust::Slice<TIntermNode *const> functionDeclarations)
{
    TIntermBlock *root = new TIntermBlock;
    root->setIsTreeRoot();

    append_instructions_to_block(root, typeDeclarations);
    append_instructions_to_block(root, globalVariables);
    append_instructions_to_block(root, functionDeclarations);

    return root;
}

}  // namespace ffi
}  // namespace ir
}  // namespace sh
