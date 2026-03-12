//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Bridge from IR in Rust.  Forward declares callbacks from IR to generate a legacy AST.
//

#ifndef COMPILER_TRANSLATOR_IR_SRC_IR_TO_LEGACY_H_
#define COMPILER_TRANSLATOR_IR_SRC_IR_TO_LEGACY_H_

#include <cstdint>

// There are complex dependencies that can be mostly solved by forward declaring the ffi types,
// except rust::Slice that needs to actually be included.  builder.rs.h includes this type and is
// outside this complex dependency, so that's used here.
#include "compiler/translator/ir/src/builder.rs.h"

namespace sh
{
class TCompiler;
class TType;
class TFunction;
class TVariable;
class TIntermNode;
class TIntermTyped;
class TIntermBlock;

namespace ir
{
namespace ffi
{
struct SymbolName;
struct ASTFieldInfo;
struct Expression;

TType *make_basic_type(ASTBasicType basicType);
TType *make_vector_type(const TType *scalarType, uint32_t count);
TType *make_matrix_type(const TType *vectorType, uint32_t count);
TType *make_array_type(const TType *elementType, uint32_t count);
TType *make_unsized_array_type(const TType *elementType);
TType *make_struct_type(TCompiler *compiler,
                        const SymbolName &name,
                        rust::Slice<const ASTFieldInfo> fields,
                        bool isInterfaceBlock);
TIntermNode *declare_struct(TCompiler *compiler, const TType *structType);

TIntermTyped *make_float_constant(float f);
TIntermTyped *make_int_constant(int32_t i);
TIntermTyped *make_uint_constant(uint32_t u);
TIntermTyped *make_bool_constant(bool b);
TIntermTyped *make_yuv_csc_constant(ASTYuvCscStandardEXT yuvCsc);
TIntermTyped *make_composite_constant(rust::Slice<TIntermTyped *const> elements,
                                      const TType *constantType);
TIntermTyped *make_constant_variable(TCompiler *compiler,
                                     const TType *constantType,
                                     TIntermTyped *value);

TIntermTyped *make_variable(TCompiler *compiler,
                            const SymbolName &name,
                            const TType *baseType,
                            const ASTType &astType,
                            bool isRedeclaredBuiltIn,
                            bool isStaticUse);
TIntermTyped *make_nameless_block_field_variable(TCompiler *compiler,
                                                 TIntermTyped *variable,
                                                 uint32_t fieldIndex,
                                                 const SymbolName &name,
                                                 const TType *baseType,
                                                 const ASTType &astType);
TIntermNode *declare_variable(TIntermTyped *variable);
TIntermNode *declare_variable_with_initializer(TIntermTyped *variable, TIntermTyped *value);
TIntermNode *globally_qualify_built_in_invariant(TIntermTyped *variable);
TIntermNode *globally_qualify_built_in_precise(TIntermTyped *variable);

TFunction *make_function(TCompiler *compiler,
                         const SymbolName &name,
                         const TType *returnType,
                         const ASTType &returnAstType,
                         rust::Slice<TIntermTyped *const> params,
                         rust::Slice<const ffi::ASTQualifier> paramDirections);
TIntermNode *declare_function(const TFunction *function, TIntermBlock *body);

TIntermBlock *make_interm_block();
void append_instructions_to_block(TIntermBlock *block, rust::Slice<TIntermNode *const> nodes);
void append_blocks_to_block(TIntermBlock *block, rust::Slice<TIntermBlock *const> blocksToAppend);

TIntermTyped *swizzle(const Expression &operand, rust::Slice<const uint32_t> indices);
TIntermTyped *index(const Expression &operand, const Expression &index);
TIntermTyped *select_field(const Expression &operand, uint32_t fieldIndex);
TIntermTyped *construct(const TType *type, rust::Slice<const Expression> operands);
TIntermNode *store(const Expression &pointer, const Expression &value);
TIntermTyped *call(const TFunction *function, rust::Slice<const Expression> args);
TIntermNode *call_void(const TFunction *function, rust::Slice<const Expression> args);

TIntermTyped *array_length(const Expression &operand);
TIntermTyped *negate(const Expression &operand);
TIntermTyped *postfix_increment(const Expression &operand);
TIntermTyped *postfix_decrement(const Expression &operand);
TIntermTyped *prefix_increment(const Expression &operand);
TIntermTyped *prefix_decrement(const Expression &operand);
TIntermTyped *logical_not(const Expression &operand);
TIntermTyped *bitwise_not(const Expression &operand);
TIntermTyped *built_in_radians(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_degrees(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_sin(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_cos(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_tan(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_asin(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_acos(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_atan(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_sinh(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_cosh(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_tanh(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_asinh(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_acosh(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_atanh(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_exp(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_log(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_exp2(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_log2(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_sqrt(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_inversesqrt(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_abs(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_sign(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_floor(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_trunc(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_round(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_roundeven(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_ceil(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_fract(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_isnan(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_isinf(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_floatbitstoint(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_floatbitstouint(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_intbitstofloat(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_uintbitstofloat(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_packsnorm2x16(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_packhalf2x16(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_unpacksnorm2x16(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_unpackhalf2x16(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_packunorm2x16(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_unpackunorm2x16(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_packunorm4x8(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_packsnorm4x8(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_unpackunorm4x8(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_unpacksnorm4x8(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_length(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_normalize(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_transpose(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_determinant(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_inverse(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_any(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_all(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_not(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_bitfieldreverse(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_bitcount(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_findlsb(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_findmsb(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_dfdx(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_dfdy(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_fwidth(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_interpolateatcentroid(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_atomiccounter(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_atomiccounterincrement(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_atomiccounterdecrement(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_imagesize(TCompiler *compiler, const Expression &operand);
TIntermTyped *built_in_pixellocalload(TCompiler *compiler, const Expression &operand);

TIntermTyped *add(const Expression &lhs, const Expression &rhs);
TIntermTyped *sub(const Expression &lhs, const Expression &rhs);
TIntermTyped *mul(const Expression &lhs, const Expression &rhs);
TIntermTyped *vector_times_scalar(const Expression &lhs, const Expression &rhs);
TIntermTyped *matrix_times_scalar(const Expression &lhs, const Expression &rhs);
TIntermTyped *vector_times_matrix(const Expression &lhs, const Expression &rhs);
TIntermTyped *matrix_times_vector(const Expression &lhs, const Expression &rhs);
TIntermTyped *matrix_times_matrix(const Expression &lhs, const Expression &rhs);
TIntermTyped *div(const Expression &lhs, const Expression &rhs);
TIntermTyped *imod(const Expression &lhs, const Expression &rhs);
TIntermTyped *logical_xor(const Expression &lhs, const Expression &rhs);
TIntermTyped *equal(const Expression &lhs, const Expression &rhs);
TIntermTyped *not_equal(const Expression &lhs, const Expression &rhs);
TIntermTyped *less_than(const Expression &lhs, const Expression &rhs);
TIntermTyped *greater_than(const Expression &lhs, const Expression &rhs);
TIntermTyped *less_than_equal(const Expression &lhs, const Expression &rhs);
TIntermTyped *greater_than_equal(const Expression &lhs, const Expression &rhs);
TIntermTyped *bit_shift_left(const Expression &lhs, const Expression &rhs);
TIntermTyped *bit_shift_right(const Expression &lhs, const Expression &rhs);
TIntermTyped *bitwise_or(const Expression &lhs, const Expression &rhs);
TIntermTyped *bitwise_xor(const Expression &lhs, const Expression &rhs);
TIntermTyped *bitwise_and(const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atan_binary(TCompiler *compiler,
                                   const Expression &lhs,
                                   const Expression &rhs);
TIntermTyped *built_in_pow(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_mod(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_min(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_max(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_step(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_modf(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_frexp(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_ldexp(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_distance(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_dot(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_cross(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_reflect(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_matrixcompmult(TCompiler *compiler,
                                      const Expression &lhs,
                                      const Expression &rhs);
TIntermTyped *built_in_outerproduct(TCompiler *compiler,
                                    const Expression &lhs,
                                    const Expression &rhs);
TIntermTyped *built_in_lessthanvec(TCompiler *compiler,
                                   const Expression &lhs,
                                   const Expression &rhs);
TIntermTyped *built_in_lessthanequalvec(TCompiler *compiler,
                                        const Expression &lhs,
                                        const Expression &rhs);
TIntermTyped *built_in_greaterthanvec(TCompiler *compiler,
                                      const Expression &lhs,
                                      const Expression &rhs);
TIntermTyped *built_in_greaterthanequalvec(TCompiler *compiler,
                                           const Expression &lhs,
                                           const Expression &rhs);
TIntermTyped *built_in_equalvec(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_notequalvec(TCompiler *compiler,
                                   const Expression &lhs,
                                   const Expression &rhs);
TIntermTyped *built_in_interpolateatsample(TCompiler *compiler,
                                           const Expression &lhs,
                                           const Expression &rhs);
TIntermTyped *built_in_interpolateatoffset(TCompiler *compiler,
                                           const Expression &lhs,
                                           const Expression &rhs);
TIntermTyped *built_in_atomicadd(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atomicmin(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atomicmax(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atomicand(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atomicor(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atomicxor(TCompiler *compiler, const Expression &lhs, const Expression &rhs);
TIntermTyped *built_in_atomicexchange(TCompiler *compiler,
                                      const Expression &lhs,
                                      const Expression &rhs);

TIntermTyped *built_in_clamp(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_mix(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_smoothstep(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_fma(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_faceforward(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_refract(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_bitfieldextract(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_bitfieldinsert(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_uaddcarry(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_usubborrow(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_umulextended(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_imulextended(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_texturesize(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_texturequerylod(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_texelfetch(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_texelfetchoffset(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_rgb_2_yuv(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_yuv_2_rgb(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_atomiccompswap(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_imagestore(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageload(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicadd(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicmin(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicmax(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicand(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicor(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicxor(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomicexchange(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_imageatomiccompswap(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_pixellocalstore(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_memorybarrier(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_memorybarrieratomiccounter(TCompiler *compiler,
                                                 rust::Slice<const Expression> args);
TIntermNode *built_in_memorybarrierbuffer(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_memorybarrierimage(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_barrier(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_memorybarriershared(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_groupmemorybarrier(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_emitvertex(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_endprimitive(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_subpassload(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_begininvocationinterlocknv(TCompiler *compiler,
                                                 rust::Slice<const Expression> args);
TIntermNode *built_in_endinvocationinterlocknv(TCompiler *compiler,
                                               rust::Slice<const Expression> args);
TIntermNode *built_in_beginfragmentshaderorderingintel(TCompiler *compiler,
                                                       rust::Slice<const Expression> args);
TIntermNode *built_in_begininvocationinterlockarb(TCompiler *compiler,
                                                  rust::Slice<const Expression> args);
TIntermNode *built_in_endinvocationinterlockarb(TCompiler *compiler,
                                                rust::Slice<const Expression> args);
TIntermTyped *built_in_numsamples(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_sampleposition(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_interpolateatcenter(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermNode *built_in_loopforwardprogress(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_saturate(TCompiler *compiler, rust::Slice<const Expression> args);

TIntermTyped *built_in_texture(TCompiler *compiler,
                               rust::Slice<const Expression> args,
                               ASTBasicType samplerType,
                               bool isProj);
TIntermTyped *built_in_textureoffset(TCompiler *compiler,
                                     rust::Slice<const Expression> args,
                                     bool isProj);
TIntermTyped *built_in_texture_with_compare(TCompiler *compiler,
                                            rust::Slice<const Expression> args);
TIntermTyped *built_in_texturelod(TCompiler *compiler,
                                  rust::Slice<const Expression> args,
                                  ASTBasicType samplerType,
                                  bool isProj);
TIntermTyped *built_in_texturelodoffset(TCompiler *compiler,
                                        rust::Slice<const Expression> args,
                                        bool isProj);
TIntermTyped *built_in_texturelod_with_compare(TCompiler *compiler,
                                               rust::Slice<const Expression> args);
TIntermTyped *built_in_texturegrad(TCompiler *compiler,
                                   rust::Slice<const Expression> args,
                                   ASTBasicType samplerType,
                                   bool isProj);
TIntermTyped *built_in_texturegradoffset(TCompiler *compiler,
                                         rust::Slice<const Expression> args,
                                         bool isProj);
TIntermTyped *built_in_texturegather(TCompiler *compiler, rust::Slice<const Expression> args);
TIntermTyped *built_in_texturegatheroffset(TCompiler *compiler,
                                           rust::Slice<const Expression> args,
                                           bool isOffsetArray);

void branch_discard(TIntermBlock *block);
void branch_return_value(TIntermBlock *block, const Expression &value);
void branch_return(TIntermBlock *block);
void branch_break(TIntermBlock *block);
void branch_continue(TIntermBlock *block);

void branch_if(TIntermBlock *block, const Expression &condition, TIntermBlock *trueBlock);
void branch_if_else(TIntermBlock *block,
                    const Expression &condition,
                    TIntermBlock *trueBlock,
                    TIntermBlock *falseBlock);
void branch_loop(TIntermBlock *block, TIntermBlock *loopConditionBlock, TIntermBlock *bodyBlock);
void branch_do_loop(TIntermBlock *block, TIntermBlock *bodyBlock);
void branch_loop_if(TIntermBlock *block, const Expression &condition);
void branch_switch(TIntermBlock *block,
                   const Expression &value,
                   rust::Slice<TIntermTyped *const> caseLabels,
                   rust::Slice<TIntermBlock *const> caseBlocks);

TIntermBlock *finalize(TCompiler *compiler,
                       rust::Slice<TIntermNode *const> typeDeclarations,
                       rust::Slice<TIntermNode *const> globalVariables,
                       rust::Slice<TIntermNode *const> functionDeclarations);

}  // namespace ffi
}  // namespace ir
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_IR_SRC_IR_TO_LEGACY_H_
