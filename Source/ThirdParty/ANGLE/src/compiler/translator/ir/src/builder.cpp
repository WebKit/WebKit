//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ir/src/builder.h"

#include "compiler/translator/ConstantUnion.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/Types.h"

namespace sh
{
namespace ir
{
namespace
{
template <typename T, template <typename> typename Span>
rust::Slice<const T> Slice(const Span<T> &span)
{
    return rust::Slice(const_cast<const T *>(span.data()), span.size());
}

template <typename T, typename U, template <typename> typename Span>
rust::Slice<const T> SliceCast(const Span<U> &span)
{
    return rust::Slice(reinterpret_cast<const T *>(span.data()), span.size());
}

template <typename T, template <typename> typename Span>
rust::Slice<T> SliceMut(const Span<T> &span)
{
    return rust::Slice(span.data(), span.size());
}

rust::Str Str(const ImmutableString &str)
{
    return rust::Str(str.data(), str.length());
}

ffi::ASTLayoutQualifier MakeASTLayoutQualifier(const TLayoutQualifier &qualifier)
{
    return ffi::ASTLayoutQualifier{
        qualifier.location,
        static_cast<ffi::ASTLayoutMatrixPacking>(qualifier.matrixPacking),
        static_cast<ffi::ASTLayoutBlockStorage>(qualifier.blockStorage),
        qualifier.binding,
        qualifier.offset,
        static_cast<ffi::ASTLayoutDepth>(qualifier.depth),
        static_cast<ffi::ASTLayoutImageInternalFormat>(qualifier.imageInternalFormat),
        qualifier.numViews,
        qualifier.yuv,
        qualifier.index,
        qualifier.noncoherent,
    };
}

ffi::ASTMemoryQualifier MakeASTMemoryQualifier(const TMemoryQualifier &qualifier)
{
    return ffi::ASTMemoryQualifier{
        qualifier.readonly,          qualifier.writeonly,         qualifier.coherent,
        qualifier.restrictQualifier, qualifier.volatileQualifier,
    };
}

ffi::ASTType MakeASTType(const TType &type, TypeId typeId)
{
    ASSERT(!type.isTypeIdSet() || type.typeId().id == typeId.id);
    ASSERT(typeId.id != kInvalidTypeId.id);

    return ffi::ASTType{
        typeId,
        static_cast<ffi::ASTQualifier>(type.getQualifier()),
        static_cast<ffi::ASTPrecision>(type.getPrecision()),
        MakeASTLayoutQualifier(type.getLayoutQualifier()),
        MakeASTMemoryQualifier(type.getMemoryQualifier()),
        type.isInvariant(),
        type.isPrecise(),
        type.isInterpolant(),
    };
}
}  // anonymous namespace

Builder::Builder(gl::ShaderType shaderType)
    :  // C++ and Rust enums are identically defined.
      mBuilder(ffi::builder_new(static_cast<ffi::ASTShaderType>(shaderType)))

{}

// static
IR Builder::destroy(Builder &&builder)
{
    if (builder.mHasError)
    {
        // Discard everything if the shader failed to build.
        return ffi::builder_fail(std::move(builder.mBuilder));
    }

    return ffi::builder_finish(std::move(builder.mBuilder));
}

TypeId Builder::getBasicTypeId(TBasicType basicType, uint32_t primarySize, uint32_t secondarySize)
{
    return mBuilder->get_basic_type_id(static_cast<ffi::ASTBasicType>(basicType), primarySize,
                                       secondarySize);
}

TypeId Builder::getStructTypeId(const ImmutableString &name,
                                const angle::Span<const TField *const> &fields,
                                const angle::Span<TypeId> &fieldTypeIds,
                                bool isInterfaceBlock,
                                bool isBuiltIn,
                                bool isAtGlobalScope)
{
    if (mHasError)
    {
        return {};
    }

    TVector<ffi::ASTField> astFields;

    for (size_t i = 0; i < fields.size(); ++i)
    {
        const TField *field = fields[i];
        astFields.emplace_back(ffi::ASTField{
            Str(field->name()),
            MakeASTType(*field->type(),
                        fieldTypeIds.empty() ? field->type()->typeId() : fieldTypeIds[i]),
        });
    }

    ffi::ASTStruct astStruct = {
        Str(name), Slice(astFields), isInterfaceBlock, isBuiltIn, isAtGlobalScope,
    };

    return mBuilder->get_struct_type_id(astStruct);
}

TypeId Builder::getArrayTypeId(TypeId elementTypeId,
                               const angle::Span<const unsigned int> &arraySizes)
{
    if (mHasError)
    {
        return {};
    }

    return mBuilder->get_array_type_id(elementTypeId, Slice(arraySizes));
}

void Builder::setEarlyFragmentTests(bool value)
{
    mBuilder->set_early_fragment_tests(value);
}

void Builder::setAdvancedBlendEquations(uint32_t value)
{
    mBuilder->set_advanced_blend_equations(value);
}

void Builder::setTcsVertices(uint32_t value)
{
    mBuilder->set_tcs_vertices(value);
}

void Builder::setTesPrimitive(TLayoutTessEvaluationType value)
{
    mBuilder->set_tes_primitive(static_cast<ffi::ASTLayoutTessEvaluationType>(value));
}

void Builder::setTesVertexSpacing(TLayoutTessEvaluationType value)
{
    mBuilder->set_tes_vertex_spacing(static_cast<ffi::ASTLayoutTessEvaluationType>(value));
}

void Builder::setTesOrdering(TLayoutTessEvaluationType value)
{
    mBuilder->set_tes_ordering(static_cast<ffi::ASTLayoutTessEvaluationType>(value));
}

void Builder::setTesPointMode(TLayoutTessEvaluationType value)
{
    mBuilder->set_tes_point_mode(static_cast<ffi::ASTLayoutTessEvaluationType>(value));
}

void Builder::setGsPrimitiveIn(TLayoutPrimitiveType value)
{
    mBuilder->set_gs_primitive_in(static_cast<ffi::ASTLayoutPrimitiveType>(value));
}

void Builder::setGsPrimitiveOut(TLayoutPrimitiveType value)
{
    mBuilder->set_gs_primitive_out(static_cast<ffi::ASTLayoutPrimitiveType>(value));
}

void Builder::setGsInvocations(uint32_t value)
{
    mBuilder->set_gs_invocations(value);
}

void Builder::setGsMaxVertices(uint32_t value)
{
    mBuilder->set_gs_max_vertices(value);
}

VariableId Builder::declareInterfaceVariable(const ImmutableString &name,
                                             TypeId typeId,
                                             const TType &type,
                                             DeclarationSource source)
{
    if (mHasError)
    {
        return {};
    }

    return mBuilder->declare_interface_variable(Str(name), MakeASTType(type, typeId),
                                                source == DeclarationSource::Internal);
}

VariableId Builder::declareTempVariable(const ImmutableString &name,
                                        TypeId typeId,
                                        const TType &type)
{
    if (mHasError)
    {
        return {};
    }

    return mBuilder->declare_temp_variable(Str(name), MakeASTType(type, typeId));
}

void Builder::markVariableInvariant(VariableId id)
{
    if (!mHasError)
    {
        mBuilder->mark_variable_invariant(id);
    }
}

void Builder::markVariablePrecise(VariableId id)
{
    if (!mHasError)
    {
        mBuilder->mark_variable_precise(id);
    }
}

void Builder::initialize(VariableId id)
{
    if (!mHasError)
    {
        mBuilder->initialize(id);
    }
}

FunctionId Builder::newFunction(const ImmutableString &name,
                                const angle::Span<VariableId> &params,
                                const angle::Span<TQualifier> &paramDirections,
                                TypeId returnTypeId,
                                const TType &returnType)
{
    if (mHasError)
    {
        return {};
    }

    return mBuilder->new_function(Str(name), Slice(params),
                                  SliceCast<ffi::ASTQualifier>(paramDirections), returnTypeId,
                                  MakeASTType(returnType, returnTypeId));
}

void Builder::updateFunctionParamNames(FunctionId id,
                                       const angle::Span<ImmutableString> &paramNames,
                                       const angle::Span<VariableId> &paramIdsOut)
{
    if (mHasError)
    {
        return;
    }

    std::vector<rust::Str> paramNameStrs;
    paramNameStrs.reserve(paramNames.size());

    for (const ImmutableString &param : paramNames)
    {
        paramNameStrs.push_back(Str(param));
    }

    mBuilder->update_function_param_names(id, Slice(paramNameStrs), SliceMut(paramIdsOut));
}
VariableId Builder::declareFunctionParam(const ImmutableString &name,
                                         TypeId typeId,
                                         const TType &type)
{
    if (mHasError)
    {
        return {};
    }

    return mBuilder->declare_function_param(Str(name), typeId, MakeASTType(type, typeId));
}

void Builder::beginFunction(FunctionId id)
{
    if (!mHasError)
    {
        mBuilder->begin_function(id);
    }
}

void Builder::endFunction()
{
    if (!mHasError)
    {
        mBuilder->end_function();
    }
}

void Builder::beginIfTrueBlock()
{
    if (!mHasError)
    {
        mBuilder->begin_if_true_block();
    }
}

void Builder::endIfTrueBlock()
{
    if (!mHasError)
    {
        mBuilder->end_if_true_block();
    }
}

void Builder::beginIfFalseBlock()
{
    if (!mHasError)
    {
        mBuilder->begin_if_false_block();
    }
}

void Builder::endIfFalseBlock()
{
    if (!mHasError)
    {
        mBuilder->end_if_false_block();
    }
}

void Builder::endIf()
{
    if (!mHasError)
    {
        mBuilder->end_if();
    }
}

void Builder::beginTernaryTrueExpression()
{
    if (!mHasError)
    {
        mBuilder->begin_ternary_true_expression();
    }
}

void Builder::endTernaryTrueExpression(TBasicType basicType)
{
    if (!mHasError)
    {
        mBuilder->end_ternary_true_expression(basicType == EbtVoid);
    }
}

void Builder::beginTernaryFalseExpression()
{
    if (!mHasError)
    {
        mBuilder->begin_ternary_false_expression();
    }
}

void Builder::endTernaryFalseExpression(TBasicType basicType)
{
    if (!mHasError)
    {
        mBuilder->end_ternary_false_expression(basicType == EbtVoid);
    }
}

void Builder::endTernary(TBasicType basicType)
{
    if (!mHasError)
    {
        mBuilder->end_ternary(basicType == EbtVoid);
    }
}

void Builder::beginShortCircuitOr()
{
    if (!mHasError)
    {
        mBuilder->begin_short_circuit_or();
    }
}

void Builder::endShortCircuitOr()
{
    if (!mHasError)
    {
        mBuilder->end_short_circuit_or();
    }
}

void Builder::beginShortCircuitAnd()
{
    if (!mHasError)
    {
        mBuilder->begin_short_circuit_and();
    }
}

void Builder::endShortCircuitAnd()
{
    if (!mHasError)
    {
        mBuilder->end_short_circuit_and();
    }
}

void Builder::beginLoopCondition()
{
    if (!mHasError)
    {
        mBuilder->begin_loop_condition();
    }
}

void Builder::endLoopCondition()
{
    if (!mHasError)
    {
        mBuilder->end_loop_condition();
    }
}

void Builder::endLoopContinue()
{
    if (!mHasError)
    {
        mBuilder->end_loop_continue();
    }
}

void Builder::endLoop()
{
    if (!mHasError)
    {
        mBuilder->end_loop();
    }
}

void Builder::beginDoLoop()
{
    if (!mHasError)
    {
        mBuilder->begin_do_loop();
    }
}

void Builder::beginDoLoopCondition()
{
    if (!mHasError)
    {
        mBuilder->begin_do_loop_condition();
    }
}

void Builder::endDoLoop()
{
    if (!mHasError)
    {
        mBuilder->end_do_loop();
    }
}

void Builder::beginSwitch()
{
    if (!mHasError)
    {
        mBuilder->begin_switch();
    }
}

void Builder::beginCase()
{
    if (!mHasError)
    {
        mBuilder->begin_case();
    }
}

void Builder::beginDefault()
{
    if (!mHasError)
    {
        mBuilder->begin_default();
    }
}

void Builder::endSwitch()
{
    if (!mHasError)
    {
        mBuilder->end_switch();
    }
}

void Builder::branchDiscard()
{
    if (!mHasError)
    {
        mBuilder->branch_discard();
    }
}

void Builder::branchReturn()
{
    if (!mHasError)
    {
        mBuilder->branch_return();
    }
}

void Builder::branchReturnValue()
{
    if (!mHasError)
    {
        mBuilder->branch_return_value();
    }
}

void Builder::branchBreak()
{
    if (!mHasError)
    {
        mBuilder->branch_break();
    }
}

void Builder::branchContinue()
{
    if (!mHasError)
    {
        mBuilder->branch_continue();
    }
}

uint32_t Builder::popArraySize()
{
    if (!mHasError)
    {
        return mBuilder->pop_array_size();
    }
    return 0xEE;
}

void Builder::endStatementWithValue()
{
    if (!mHasError)
    {
        mBuilder->end_statement_with_value();
    }
}

void Builder::pushConstantFloat(float value)
{
    if (!mHasError)
    {
        mBuilder->push_constant_float(value);
    }
}

void Builder::pushConstantInt(int32_t value)
{
    if (!mHasError)
    {
        mBuilder->push_constant_int(value);
    }
}

void Builder::pushConstantUint(uint32_t value)
{
    if (!mHasError)
    {
        mBuilder->push_constant_uint(value);
    }
}

void Builder::pushConstantBool(bool value)
{
    if (!mHasError)
    {
        mBuilder->push_constant_bool(value);
    }
}

void Builder::pushConstantYuvCscStandard(TYuvCscStandardEXT value)
{
    if (!mHasError)
    {
        mBuilder->push_constant_yuv_csc_standard(static_cast<ffi::ASTYuvCscStandardEXT>(value));
    }
}

void Builder::pushVariable(VariableId id)
{
    if (!mHasError)
    {
        mBuilder->push_variable(id);
    }
}

void Builder::callFunction(FunctionId id)
{
    if (!mHasError)
    {
        mBuilder->call_function(id);
    }
}

void Builder::vectorComponent(uint32_t component)
{
    if (!mHasError)
    {
        mBuilder->vector_component(component);
    }
}

void Builder::vectorComponentMulti(const angle::Span<uint32_t> &components)
{
    if (!mHasError)
    {
        mBuilder->vector_component_multi(Slice(components));
    }
}

void Builder::index()
{
    if (!mHasError)
    {
        mBuilder->index();
    }
}

void Builder::structField(uint32_t fieldIndex)
{
    if (!mHasError)
    {
        mBuilder->struct_field(fieldIndex);
    }
}

void Builder::construct(TypeId typeId, size_t argCount)
{
    if (!mHasError)
    {
        mBuilder->construct(typeId, argCount);
    }
}

void Builder::onGlClipDistanceSized(VariableId id, uint32_t length)
{
    if (!mHasError)
    {
        mBuilder->on_gl_clip_distance_sized(id, length);
    }
}

void Builder::onGlCullDistanceSized(VariableId id, uint32_t length)
{
    if (!mHasError)
    {
        mBuilder->on_gl_cull_distance_sized(id, length);
    }
}

void Builder::arrayLength()
{
    if (!mHasError)
    {
        mBuilder->array_length();
    }
}

void Builder::builtIn(TOperator op, size_t argCount)
{
    if (mHasError)
    {
        return;
    }
    switch (op)
    {
        case EOpNegative:
            mBuilder->negate();
            return;
        case EOpPositive:
            // Nothing, this is a no-op.
            return;
        case EOpPostIncrement:
            mBuilder->postfix_increment();
            return;
        case EOpPostDecrement:
            mBuilder->postfix_decrement();
            return;
        case EOpPreIncrement:
            mBuilder->prefix_increment();
            return;
        case EOpPreDecrement:
            mBuilder->prefix_decrement();
            return;
        case EOpAssign:
            mBuilder->store();
            return;
        case EOpAdd:
            mBuilder->add();
            return;
        case EOpAddAssign:
            mBuilder->add_assign();
            return;
        case EOpSub:
            mBuilder->sub();
            return;
        case EOpSubAssign:
            mBuilder->sub_assign();
            return;
        case EOpMul:
            mBuilder->mul();
            return;
        case EOpMulAssign:
            mBuilder->mul_assign();
            return;
        case EOpVectorTimesScalar:
            mBuilder->vector_times_scalar();
            return;
        case EOpVectorTimesScalarAssign:
            mBuilder->vector_times_scalar_assign();
            return;
        case EOpMatrixTimesScalar:
            mBuilder->matrix_times_scalar();
            return;
        case EOpMatrixTimesScalarAssign:
            mBuilder->matrix_times_scalar_assign();
            return;
        case EOpVectorTimesMatrix:
            mBuilder->vector_times_matrix();
            return;
        case EOpVectorTimesMatrixAssign:
            mBuilder->vector_times_matrix_assign();
            return;
        case EOpMatrixTimesVector:
            mBuilder->matrix_times_vector();
            return;
        case EOpMatrixTimesMatrix:
            mBuilder->matrix_times_matrix();
            return;
        case EOpMatrixTimesMatrixAssign:
            mBuilder->matrix_times_matrix_assign();
            return;
        case EOpDiv:
            mBuilder->div();
            return;
        case EOpDivAssign:
            mBuilder->div_assign();
            return;
        case EOpIMod:
            mBuilder->imod();
            return;
        case EOpIModAssign:
            mBuilder->imod_assign();
            return;
        case EOpLogicalNot:
            mBuilder->logical_not();
            return;
        case EOpLogicalXor:
            mBuilder->logical_xor();
            return;
        case EOpEqual:
            mBuilder->equal();
            return;
        case EOpNotEqual:
            mBuilder->not_equal();
            return;
        case EOpLessThan:
            mBuilder->less_than();
            return;
        case EOpGreaterThan:
            mBuilder->greater_than();
            return;
        case EOpLessThanEqual:
            mBuilder->less_than_equal();
            return;
        case EOpGreaterThanEqual:
            mBuilder->greater_than_equal();
            return;
        case EOpBitwiseNot:
            mBuilder->bitwise_not();
            return;
        case EOpBitShiftLeft:
            mBuilder->bit_shift_left();
            return;
        case EOpBitShiftLeftAssign:
            mBuilder->bit_shift_left_assign();
            return;
        case EOpBitShiftRight:
            mBuilder->bit_shift_right();
            return;
        case EOpBitShiftRightAssign:
            mBuilder->bit_shift_right_assign();
            return;
        case EOpBitwiseOr:
            mBuilder->bitwise_or();
            return;
        case EOpBitwiseOrAssign:
            mBuilder->bitwise_or_assign();
            return;
        case EOpBitwiseXor:
            mBuilder->bitwise_xor();
            return;
        case EOpBitwiseXorAssign:
            mBuilder->bitwise_xor_assign();
            return;
        case EOpBitwiseAnd:
            mBuilder->bitwise_and();
            return;
        case EOpBitwiseAndAssign:
            mBuilder->bitwise_and_assign();
            return;
        case EOpRadians:
            mBuilder->built_in_radians();
            return;
        case EOpDegrees:
            mBuilder->built_in_degrees();
            return;
        case EOpSin:
            mBuilder->built_in_sin();
            return;
        case EOpCos:
            mBuilder->built_in_cos();
            return;
        case EOpTan:
            mBuilder->built_in_tan();
            return;
        case EOpAsin:
            mBuilder->built_in_asin();
            return;
        case EOpAcos:
            mBuilder->built_in_acos();
            return;
        case EOpAtan:
            if (argCount == 2)
            {
                mBuilder->built_in_atan_binary();
            }
            else
            {
                mBuilder->built_in_atan();
            }
            return;
        case EOpSinh:
            mBuilder->built_in_sinh();
            return;
        case EOpCosh:
            mBuilder->built_in_cosh();
            return;
        case EOpTanh:
            mBuilder->built_in_tanh();
            return;
        case EOpAsinh:
            mBuilder->built_in_asinh();
            return;
        case EOpAcosh:
            mBuilder->built_in_acosh();
            return;
        case EOpAtanh:
            mBuilder->built_in_atanh();
            return;
        case EOpPow:
            mBuilder->built_in_pow();
            return;
        case EOpExp:
            mBuilder->built_in_exp();
            return;
        case EOpLog:
            mBuilder->built_in_log();
            return;
        case EOpExp2:
            mBuilder->built_in_exp2();
            return;
        case EOpLog2:
            mBuilder->built_in_log2();
            return;
        case EOpSqrt:
            mBuilder->built_in_sqrt();
            return;
        case EOpInversesqrt:
            mBuilder->built_in_inversesqrt();
            return;
        case EOpAbs:
            mBuilder->built_in_abs();
            return;
        case EOpSign:
            mBuilder->built_in_sign();
            return;
        case EOpFloor:
            mBuilder->built_in_floor();
            return;
        case EOpTrunc:
            mBuilder->built_in_trunc();
            return;
        case EOpRound:
            mBuilder->built_in_round();
            return;
        case EOpRoundEven:
            mBuilder->built_in_roundeven();
            return;
        case EOpCeil:
            mBuilder->built_in_ceil();
            return;
        case EOpFract:
            mBuilder->built_in_fract();
            return;
        case EOpMod:
            mBuilder->built_in_mod();
            return;
        case EOpMin:
            mBuilder->built_in_min();
            return;
        case EOpMax:
            mBuilder->built_in_max();
            return;
        case EOpClamp:
            mBuilder->built_in_clamp();
            return;
        case EOpMix:
            mBuilder->built_in_mix();
            return;
        case EOpStep:
            mBuilder->built_in_step();
            return;
        case EOpSmoothstep:
            mBuilder->built_in_smoothstep();
            return;
        case EOpModf:
            mBuilder->built_in_modf();
            return;
        case EOpIsnan:
            mBuilder->built_in_isnan();
            return;
        case EOpIsinf:
            mBuilder->built_in_isinf();
            return;
        case EOpFloatBitsToInt:
            mBuilder->built_in_floatbitstoint();
            return;
        case EOpFloatBitsToUint:
            mBuilder->built_in_floatbitstouint();
            return;
        case EOpIntBitsToFloat:
            mBuilder->built_in_intbitstofloat();
            return;
        case EOpUintBitsToFloat:
            mBuilder->built_in_uintbitstofloat();
            return;
        case EOpFma:
            mBuilder->built_in_fma();
            return;
        case EOpFrexp:
            mBuilder->built_in_frexp();
            return;
        case EOpLdexp:
            mBuilder->built_in_ldexp();
            return;
        case EOpPackSnorm2x16:
            mBuilder->built_in_packsnorm2x16();
            return;
        case EOpPackHalf2x16:
            mBuilder->built_in_packhalf2x16();
            return;
        case EOpUnpackSnorm2x16:
            mBuilder->built_in_unpacksnorm2x16();
            return;
        case EOpUnpackHalf2x16:
            mBuilder->built_in_unpackhalf2x16();
            return;
        case EOpPackUnorm2x16:
            mBuilder->built_in_packunorm2x16();
            return;
        case EOpUnpackUnorm2x16:
            mBuilder->built_in_unpackunorm2x16();
            return;
        case EOpPackUnorm4x8:
            mBuilder->built_in_packunorm4x8();
            return;
        case EOpPackSnorm4x8:
            mBuilder->built_in_packsnorm4x8();
            return;
        case EOpUnpackUnorm4x8:
            mBuilder->built_in_unpackunorm4x8();
            return;
        case EOpUnpackSnorm4x8:
            mBuilder->built_in_unpacksnorm4x8();
            return;
        case EOpLength:
            mBuilder->built_in_length();
            return;
        case EOpDistance:
            mBuilder->built_in_distance();
            return;
        case EOpDot:
            mBuilder->built_in_dot();
            return;
        case EOpCross:
            mBuilder->built_in_cross();
            return;
        case EOpNormalize:
            mBuilder->built_in_normalize();
            return;
        case EOpFaceforward:
            mBuilder->built_in_faceforward();
            return;
        case EOpReflect:
            mBuilder->built_in_reflect();
            return;
        case EOpRefract:
            mBuilder->built_in_refract();
            return;
        case EOpMatrixCompMult:
            mBuilder->built_in_matrixcompmult();
            return;
        case EOpOuterProduct:
            mBuilder->built_in_outerproduct();
            return;
        case EOpTranspose:
            mBuilder->built_in_transpose();
            return;
        case EOpDeterminant:
            mBuilder->built_in_determinant();
            return;
        case EOpInverse:
            mBuilder->built_in_inverse();
            return;
        case EOpLessThanComponentWise:
            mBuilder->built_in_lessthan();
            return;
        case EOpLessThanEqualComponentWise:
            mBuilder->built_in_lessthanequal();
            return;
        case EOpGreaterThanComponentWise:
            mBuilder->built_in_greaterthan();
            return;
        case EOpGreaterThanEqualComponentWise:
            mBuilder->built_in_greaterthanequal();
            return;
        case EOpEqualComponentWise:
            mBuilder->built_in_equal();
            return;
        case EOpNotEqualComponentWise:
            mBuilder->built_in_notequal();
            return;
        case EOpAny:
            mBuilder->built_in_any();
            return;
        case EOpAll:
            mBuilder->built_in_all();
            return;
        case EOpNotComponentWise:
            mBuilder->built_in_not();
            return;
        case EOpBitfieldExtract:
            mBuilder->built_in_bitfieldextract();
            return;
        case EOpBitfieldInsert:
            mBuilder->built_in_bitfieldinsert();
            return;
        case EOpBitfieldReverse:
            mBuilder->built_in_bitfieldreverse();
            return;
        case EOpBitCount:
            mBuilder->built_in_bitcount();
            return;
        case EOpFindLSB:
            mBuilder->built_in_findlsb();
            return;
        case EOpFindMSB:
            mBuilder->built_in_findmsb();
            return;
        case EOpUaddCarry:
            mBuilder->built_in_uaddcarry();
            return;
        case EOpUsubBorrow:
            mBuilder->built_in_usubborrow();
            return;
        case EOpUmulExtended:
            mBuilder->built_in_umulextended();
            return;
        case EOpImulExtended:
            mBuilder->built_in_imulextended();
            return;
        case EOpTextureSize:
        {
            // textureSize() takes the sampler and possibly lod.
            const bool withLod = argCount > 1;
            mBuilder->built_in_texturesize(withLod);
            return;
        }
        case EOpTextureQueryLOD:
            mBuilder->built_in_texturequerylod();
            return;
        case EOpTexelFetch:
        {
            // texelFetch() takes the sampler, coordinates and either the lod or sample index,
            // except for samplerBuffers.
            const bool withLodOrSample = argCount > 2;
            mBuilder->built_in_texelfetch(withLodOrSample);
            return;
        }
        case EOpTexelFetchOffset:
            mBuilder->built_in_texelfetchoffset();
            return;
        case EOpRgb_2_yuv:
            mBuilder->built_in_rgb_2_yuv();
            return;
        case EOpYuv_2_rgb:
            mBuilder->built_in_yuv_2_rgb();
            return;
        case EOpDFdx:
            mBuilder->built_in_dfdx();
            return;
        case EOpDFdy:
            mBuilder->built_in_dfdy();
            return;
        case EOpFwidth:
            mBuilder->built_in_fwidth();
            return;
        case EOpInterpolateAtCentroid:
            mBuilder->built_in_interpolateatcentroid();
            return;
        case EOpInterpolateAtSample:
            mBuilder->built_in_interpolateatsample();
            return;
        case EOpInterpolateAtOffset:
            mBuilder->built_in_interpolateatoffset();
            return;
        case EOpAtomicCounter:
            mBuilder->built_in_atomiccounter();
            return;
        case EOpAtomicCounterIncrement:
            mBuilder->built_in_atomiccounterincrement();
            return;
        case EOpAtomicCounterDecrement:
            mBuilder->built_in_atomiccounterdecrement();
            return;
        case EOpAtomicAdd:
            mBuilder->built_in_atomicadd();
            return;
        case EOpAtomicMin:
            mBuilder->built_in_atomicmin();
            return;
        case EOpAtomicMax:
            mBuilder->built_in_atomicmax();
            return;
        case EOpAtomicAnd:
            mBuilder->built_in_atomicand();
            return;
        case EOpAtomicOr:
            mBuilder->built_in_atomicor();
            return;
        case EOpAtomicXor:
            mBuilder->built_in_atomicxor();
            return;
        case EOpAtomicExchange:
            mBuilder->built_in_atomicexchange();
            return;
        case EOpAtomicCompSwap:
            mBuilder->built_in_atomiccompswap();
            return;
        case EOpImageSize:
            mBuilder->built_in_imagesize();
            return;
        case EOpImageStore:
            mBuilder->built_in_imagestore();
            return;
        case EOpImageLoad:
            mBuilder->built_in_imageload();
            return;
        case EOpImageAtomicAdd:
            mBuilder->built_in_imageatomicadd();
            return;
        case EOpImageAtomicMin:
            mBuilder->built_in_imageatomicmin();
            return;
        case EOpImageAtomicMax:
            mBuilder->built_in_imageatomicmax();
            return;
        case EOpImageAtomicAnd:
            mBuilder->built_in_imageatomicand();
            return;
        case EOpImageAtomicOr:
            mBuilder->built_in_imageatomicor();
            return;
        case EOpImageAtomicXor:
            mBuilder->built_in_imageatomicxor();
            return;
        case EOpImageAtomicExchange:
            mBuilder->built_in_imageatomicexchange();
            return;
        case EOpImageAtomicCompSwap:
            mBuilder->built_in_imageatomiccompswap();
            return;
        case EOpPixelLocalLoadANGLE:
            mBuilder->built_in_pixellocalloadangle();
            return;
        case EOpPixelLocalStoreANGLE:
            mBuilder->built_in_pixellocalstoreangle();
            return;
        case EOpMemoryBarrier:
            mBuilder->built_in_memorybarrier();
            return;
        case EOpMemoryBarrierAtomicCounter:
            mBuilder->built_in_memorybarrieratomiccounter();
            return;
        case EOpMemoryBarrierBuffer:
            mBuilder->built_in_memorybarrierbuffer();
            return;
        case EOpMemoryBarrierImage:
            mBuilder->built_in_memorybarrierimage();
            return;
        case EOpBarrier:
        case EOpBarrierTCS:
            mBuilder->built_in_barrier();
            return;
        case EOpMemoryBarrierShared:
            mBuilder->built_in_memorybarriershared();
            return;
        case EOpGroupMemoryBarrier:
            mBuilder->built_in_groupmemorybarrier();
            return;
        case EOpEmitVertex:
            mBuilder->built_in_emitvertex();
            return;
        case EOpEndPrimitive:
            mBuilder->built_in_endprimitive();
            return;
        case EOpTexture:
        case EOpTexture2D:
        case EOpShadow2DEXT:
        case EOpTexture2DRect:
        case EOpTexture3D:
        case EOpTextureCube:
        case EOpTextureVideoWEBGL:
        {
            // texture() takes the sampler, coordinates and possibly a compare parameter.
            // Note that the variant with a bias parameter is given a different Op.
            const bool withCompare = argCount > 2;
            mBuilder->built_in_texture(withCompare);
            return;
        }
        case EOpTextureProj:
        case EOpShadow2DProjEXT:
        case EOpTexture2DProj:
        case EOpTexture2DRectProj:
        case EOpTexture3DProj:
            mBuilder->built_in_textureproj();
            return;
        case EOpTextureLod:
        case EOpTexture2DLodEXTFS:
        case EOpTexture2DLodVS:
        case EOpTexture3DLod:
        case EOpTextureCubeLodEXTFS:
        case EOpTextureCubeLodVS:
        {
            // textureLod() takes the sampler, coordinates and lod.  The EXT_texture_shadow_lod
            // extension introduces a variant that possibly takes a compare parameter.
            const bool withCompare = argCount > 3;
            mBuilder->built_in_texturelod(withCompare);
            return;
        }
        case EOpTextureProjLod:
        case EOpTexture2DProjLodEXTFS:
        case EOpTexture2DProjLodVS:
        case EOpTexture3DProjLod:
            mBuilder->built_in_textureprojlod();
            return;
        case EOpTextureBias:
        case EOpTexture2DBias:
        case EOpTexture3DBias:
        case EOpTextureCubeBias:
        {
            // The bias-variant of texture() takes the sampler, coordinates and the bias parameter.
            // The EXT_texture_shadow_lod extension introduces a variant that possibly takes a
            // compare parameter.
            const bool withCompare = argCount > 3;
            mBuilder->built_in_texturebias(withCompare);
            return;
        }
        case EOpTextureProjBias:
        case EOpTexture2DProjBias:
        case EOpTexture3DProjBias:
            mBuilder->built_in_textureprojbias();
            return;
        case EOpTextureOffset:
            mBuilder->built_in_textureoffset();
            return;
        case EOpTextureProjOffset:
            mBuilder->built_in_textureprojoffset();
            return;
        case EOpTextureLodOffset:
            mBuilder->built_in_texturelodoffset();
            return;
        case EOpTextureProjLodOffset:
            mBuilder->built_in_textureprojlodoffset();
            return;
        case EOpTextureOffsetBias:
            mBuilder->built_in_textureoffsetbias();
            return;
        case EOpTextureProjOffsetBias:
            mBuilder->built_in_textureprojoffsetbias();
            return;
        case EOpTextureGrad:
        case EOpTexture2DGradEXT:
        case EOpTextureCubeGradEXT:
            mBuilder->built_in_texturegrad();
            return;
        case EOpTextureProjGrad:
        case EOpTexture2DProjGradEXT:
            mBuilder->built_in_textureprojgrad();
            return;
        case EOpTextureGradOffset:
            mBuilder->built_in_texturegradoffset();
            return;
        case EOpTextureProjGradOffset:
            mBuilder->built_in_textureprojgradoffset();
            return;
        case EOpTextureGather:
            mBuilder->built_in_texturegather();
            return;
        case EOpTextureGatherComp:
            mBuilder->built_in_texturegathercomp();
            return;
        case EOpTextureGatherRef:
            mBuilder->built_in_texturegatherref();
            return;
        case EOpTextureGatherOffset:
        case EOpTextureGatherOffsets:
            mBuilder->built_in_texturegatheroffset();
            return;
        case EOpTextureGatherOffsetComp:
        case EOpTextureGatherOffsetsComp:
            mBuilder->built_in_texturegatheroffsetcomp();
            return;
        case EOpTextureGatherOffsetRef:
        case EOpTextureGatherOffsetsRef:
            mBuilder->built_in_texturegatheroffsetref();
            return;
        default:
            UNREACHABLE();
    }
}
}  // namespace ir
}  // namespace sh
