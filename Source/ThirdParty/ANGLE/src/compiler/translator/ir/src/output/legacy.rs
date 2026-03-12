// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Output legacy AST from the IR.  This generator will no longer be needed once the translator is
// entirely converted to IR.

use crate::ir::*;
use crate::*;

const SYMBOL_NAME_NO_ID: u32 = 0xFFFFFFFF;

#[cxx::bridge(namespace = "sh::ir::ffi")]
pub mod ffi {
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTSymbolType {
        BuiltIn,
        UserDefined,
        AngleInternal,
        Empty,
    }

    unsafe extern "C++" {
        include!("compiler/translator/ir/src/builder.rs.h");

        // Forward the legacy AST types from builder since it already defines the same types.
        type ASTPrecision = crate::builder::ffi::ASTPrecision;
        type ASTBasicType = crate::builder::ffi::ASTBasicType;
        type ASTQualifier = crate::builder::ffi::ASTQualifier;
        type ASTLayoutImageInternalFormat = crate::builder::ffi::ASTLayoutImageInternalFormat;
        type ASTLayoutMatrixPacking = crate::builder::ffi::ASTLayoutMatrixPacking;
        type ASTLayoutBlockStorage = crate::builder::ffi::ASTLayoutBlockStorage;
        type ASTLayoutDepth = crate::builder::ffi::ASTLayoutDepth;
        type ASTYuvCscStandardEXT = crate::builder::ffi::ASTYuvCscStandardEXT;
        type ASTLayoutQualifier = crate::builder::ffi::ASTLayoutQualifier;
        type ASTMemoryQualifier = crate::builder::ffi::ASTMemoryQualifier;
        type ASTType = crate::builder::ffi::ASTType;

        #[namespace = "sh"]
        type TCompiler;
        #[namespace = "sh"]
        type TType;
        #[namespace = "sh"]
        type TFunction;
        #[namespace = "sh"]
        type TIntermNode;
        #[namespace = "sh"]
        type TIntermTyped;
        #[namespace = "sh"]
        type TIntermBlock;
    }

    struct ASTFieldInfo {
        name: &'static str,
        base_type: *const TType,
        ast_type: ASTType,
    }

    struct SymbolName {
        name: &'static str,
        symbol_type: ASTSymbolType,
        // In some cases, the IR may generate variables with the same name, such as in
        // duplicate_block().  The ID is appended to temporary names to ensure no duplication.
        // When ID is SYMBOL_NAME_NO_ID, the ID should not be appended.
        id: u32,
    }

    struct Expression {
        node: *mut TIntermTyped,
        needs_copy: bool,
    }

    unsafe extern "C++" {
        include!("compiler/translator/ir/src/output/ir_to_legacy.h");

        // SAFETY: The following functions produce an AST in C++ and use pool-allocated objects of
        // TType, TInterm* etc.  They take `*mut` pointers mirroring the existing legacy C++ AST
        // code.
        unsafe fn make_basic_type(basic_type: ASTBasicType) -> *mut TType;
        unsafe fn make_vector_type(scalar_type: *const TType, count: u32) -> *mut TType;
        unsafe fn make_matrix_type(vector_type: *const TType, count: u32) -> *mut TType;
        unsafe fn make_array_type(element_type: *const TType, count: u32) -> *mut TType;
        unsafe fn make_unsized_array_type(element_type: *const TType) -> *mut TType;
        unsafe fn make_struct_type(
            compiler: *mut TCompiler,
            name: &SymbolName,
            fields: &[ASTFieldInfo],
            is_interface_block: bool,
        ) -> *mut TType;
        unsafe fn declare_struct(
            compiler: *mut TCompiler,
            struct_type: *const TType,
        ) -> *mut TIntermNode;

        unsafe fn make_float_constant(f: f32) -> *mut TIntermTyped;
        unsafe fn make_int_constant(i: i32) -> *mut TIntermTyped;
        unsafe fn make_uint_constant(u: u32) -> *mut TIntermTyped;
        unsafe fn make_bool_constant(b: bool) -> *mut TIntermTyped;
        unsafe fn make_yuv_csc_constant(yuv_csc: ASTYuvCscStandardEXT) -> *mut TIntermTyped;
        unsafe fn make_composite_constant(
            elements: &[*mut TIntermTyped],
            constant_type: *const TType,
        ) -> *mut TIntermTyped;
        unsafe fn make_constant_variable(
            compiler: *mut TCompiler,
            constant_type: *const TType,
            value: *mut TIntermTyped,
        ) -> *mut TIntermTyped;

        unsafe fn make_variable(
            compiler: *mut TCompiler,
            name: &SymbolName,
            base_type: *const TType,
            ast_type: &ASTType,
            is_redeclared_built_in: bool,
            is_static_use: bool,
        ) -> *mut TIntermTyped;
        unsafe fn make_nameless_block_field_variable(
            compiler: *mut TCompiler,
            variable: *mut TIntermTyped,
            field_index: u32,
            name: &SymbolName,
            base_type: *const TType,
            ast_type: &ASTType,
        ) -> *mut TIntermTyped;
        unsafe fn declare_variable(variable: *mut TIntermTyped) -> *mut TIntermNode;
        unsafe fn declare_variable_with_initializer(
            variable: *mut TIntermTyped,
            value: *mut TIntermTyped,
        ) -> *mut TIntermNode;
        unsafe fn globally_qualify_built_in_invariant(
            variable: *mut TIntermTyped,
        ) -> *mut TIntermNode;
        unsafe fn globally_qualify_built_in_precise(
            variable: *mut TIntermTyped,
        ) -> *mut TIntermNode;

        unsafe fn make_function(
            compiler: *mut TCompiler,
            name: &SymbolName,
            return_type: *const TType,
            return_ast_type: &ASTType,
            params: &[*mut TIntermTyped],
            param_directions: &[ASTQualifier],
        ) -> *mut TFunction;
        unsafe fn declare_function(
            function: *const TFunction,
            body: *mut TIntermBlock,
        ) -> *mut TIntermNode;

        unsafe fn make_interm_block() -> *mut TIntermBlock;
        unsafe fn append_instructions_to_block(
            block: *mut TIntermBlock,
            nodes: &[*mut TIntermNode],
        );
        unsafe fn append_blocks_to_block(
            block: *mut TIntermBlock,
            blocks_to_append: &[*mut TIntermBlock],
        );

        // TODO(http://anglebug.com/349994211): the AST automatically derives the type of the
        // intermediate node.  However, the IR may apply transformations that use Precision::High
        // in the middle of low precision operations, so the precision of intermediate nodes must
        // be overridden to what they were originally expected to be; i.e. all the following need
        // to take a precision parameter to override in the node they create.
        //
        // This may not be a problem as ultimately the output will be generated by the IR itself
        // and this file will go away.

        unsafe fn swizzle(operand: &Expression, indices: &[u32]) -> *mut TIntermTyped;
        unsafe fn index(operand: &Expression, index: &Expression) -> *mut TIntermTyped;
        unsafe fn select_field(operand: &Expression, field_index: u32) -> *mut TIntermTyped;
        unsafe fn construct(
            construct_type: *const TType,
            operands: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn store(pointer: &Expression, value: &Expression) -> *mut TIntermNode;
        unsafe fn call(function: *const TFunction, args: &[Expression]) -> *mut TIntermTyped;
        unsafe fn call_void(function: *const TFunction, args: &[Expression]) -> *mut TIntermNode;

        unsafe fn array_length(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn negate(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn postfix_increment(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn postfix_decrement(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn prefix_increment(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn prefix_decrement(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn logical_not(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn bitwise_not(operand: &Expression) -> *mut TIntermTyped;
        unsafe fn built_in_radians(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_degrees(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_sin(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_cos(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_tan(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_asin(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_acos(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atan(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_sinh(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_cosh(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_tanh(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_asinh(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_acosh(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atanh(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_exp(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_log(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_exp2(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_log2(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_sqrt(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_inversesqrt(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_abs(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_sign(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_floor(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_trunc(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_round(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_roundeven(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_ceil(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_fract(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_isnan(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_isinf(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_floatbitstoint(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_floatbitstouint(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_intbitstofloat(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_uintbitstofloat(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_packsnorm2x16(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_packhalf2x16(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_unpacksnorm2x16(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_unpackhalf2x16(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_packunorm2x16(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_unpackunorm2x16(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_packunorm4x8(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_packsnorm4x8(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_unpackunorm4x8(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_unpacksnorm4x8(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_length(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_normalize(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_transpose(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_determinant(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_inverse(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_any(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_all(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_not(compiler: *mut TCompiler, operand: &Expression)
        -> *mut TIntermTyped;
        unsafe fn built_in_bitfieldreverse(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_bitcount(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_findlsb(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_findmsb(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_dfdx(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_dfdy(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_fwidth(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_interpolateatcentroid(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomiccounter(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomiccounterincrement(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomiccounterdecrement(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imagesize(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_pixellocalload(
            compiler: *mut TCompiler,
            operand: &Expression,
        ) -> *mut TIntermTyped;

        unsafe fn add(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn sub(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn mul(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn vector_times_scalar(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn matrix_times_scalar(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn vector_times_matrix(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn matrix_times_vector(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn matrix_times_matrix(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn div(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn imod(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn logical_xor(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn equal(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn not_equal(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn less_than(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn greater_than(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn less_than_equal(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn greater_than_equal(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn bit_shift_left(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn bit_shift_right(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn bitwise_or(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn bitwise_xor(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn bitwise_and(lhs: &Expression, rhs: &Expression) -> *mut TIntermTyped;
        unsafe fn built_in_atan_binary(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_pow(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_mod(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_min(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_max(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_step(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_modf(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_frexp(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_ldexp(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_distance(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_dot(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_cross(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_reflect(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_matrixcompmult(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_outerproduct(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_lessthanvec(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_lessthanequalvec(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_greaterthanvec(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_greaterthanequalvec(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_equalvec(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_notequalvec(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_interpolateatsample(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_interpolateatoffset(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicadd(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicmin(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicmax(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicand(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicor(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicxor(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomicexchange(
            compiler: *mut TCompiler,
            lhs: &Expression,
            rhs: &Expression,
        ) -> *mut TIntermTyped;

        unsafe fn built_in_clamp(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_mix(compiler: *mut TCompiler, args: &[Expression]) -> *mut TIntermTyped;
        unsafe fn built_in_smoothstep(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_fma(compiler: *mut TCompiler, args: &[Expression]) -> *mut TIntermTyped;
        unsafe fn built_in_faceforward(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_refract(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_bitfieldextract(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_bitfieldinsert(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_uaddcarry(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_usubborrow(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_umulextended(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_imulextended(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_texturesize(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturequerylod(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texelfetch(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texelfetchoffset(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_rgb_2_yuv(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_yuv_2_rgb(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_atomiccompswap(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imagestore(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_imageload(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicadd(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicmin(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicmax(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicand(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicor(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicxor(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomicexchange(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_imageatomiccompswap(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_pixellocalstore(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_memorybarrier(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_memorybarrieratomiccounter(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_memorybarrierbuffer(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_memorybarrierimage(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_barrier(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_memorybarriershared(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_groupmemorybarrier(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_emitvertex(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_endprimitive(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_subpassload(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_begininvocationinterlocknv(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_endinvocationinterlocknv(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_beginfragmentshaderorderingintel(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_begininvocationinterlockarb(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_endinvocationinterlockarb(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_numsamples(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_sampleposition(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_interpolateatcenter(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_loopforwardprogress(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermNode;
        unsafe fn built_in_saturate(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;

        unsafe fn built_in_texture(
            compiler: *mut TCompiler,
            args: &[Expression],
            sampler_type: ASTBasicType,
            is_proj: bool,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_textureoffset(
            compiler: *mut TCompiler,
            args: &[Expression],
            is_proj: bool,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texture_with_compare(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturelod(
            compiler: *mut TCompiler,
            args: &[Expression],
            sampler_type: ASTBasicType,
            is_proj: bool,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturelodoffset(
            compiler: *mut TCompiler,
            args: &[Expression],
            is_proj: bool,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturelod_with_compare(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturegrad(
            compiler: *mut TCompiler,
            args: &[Expression],
            sampler_type: ASTBasicType,
            is_proj: bool,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturegradoffset(
            compiler: *mut TCompiler,
            args: &[Expression],
            is_proj: bool,
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturegather(
            compiler: *mut TCompiler,
            args: &[Expression],
        ) -> *mut TIntermTyped;
        unsafe fn built_in_texturegatheroffset(
            compiler: *mut TCompiler,
            args: &[Expression],
            is_offset_array: bool,
        ) -> *mut TIntermTyped;

        unsafe fn branch_discard(block: *mut TIntermBlock);
        unsafe fn branch_return(block: *mut TIntermBlock);
        unsafe fn branch_return_value(block: *mut TIntermBlock, value: &Expression);
        unsafe fn branch_break(block: *mut TIntermBlock);
        unsafe fn branch_continue(block: *mut TIntermBlock);

        unsafe fn branch_if(
            block: *mut TIntermBlock,
            condition: &Expression,
            true_block: *mut TIntermBlock,
        );
        unsafe fn branch_if_else(
            block: *mut TIntermBlock,
            condition: &Expression,
            true_block: *mut TIntermBlock,
            false_block: *mut TIntermBlock,
        );
        unsafe fn branch_loop(
            block: *mut TIntermBlock,
            loop_condition_block: *mut TIntermBlock,
            body_block: *mut TIntermBlock,
        );
        unsafe fn branch_do_loop(block: *mut TIntermBlock, body_block: *mut TIntermBlock);
        unsafe fn branch_loop_if(block: *mut TIntermBlock, condition: &Expression);
        unsafe fn branch_switch(
            block: *mut TIntermBlock,
            value: &Expression,
            case_labels: &[*mut TIntermTyped],
            case_blocks: &[*mut TIntermBlock],
        );

        unsafe fn finalize(
            legacy_compiler: *mut TCompiler,
            type_declarations: &[*mut TIntermNode],
            global_variables: &[*mut TIntermNode],
            function_declarations: &[*mut TIntermNode],
        ) -> *mut TIntermBlock;
    }
}

use ffi::TCompiler;
use ffi::TFunction;
use ffi::TIntermBlock;
use ffi::TIntermNode;
use ffi::TIntermTyped;
use ffi::TType;

impl From<Precision> for ffi::ASTPrecision {
    fn from(precision: Precision) -> Self {
        match precision {
            Precision::Low => ffi::ASTPrecision::Low,
            Precision::Medium => ffi::ASTPrecision::Medium,
            Precision::High => ffi::ASTPrecision::High,
            _ => ffi::ASTPrecision::Undefined,
        }
    }
}

impl From<YuvCscStandard> for ffi::ASTYuvCscStandardEXT {
    fn from(value: YuvCscStandard) -> Self {
        match value {
            YuvCscStandard::Itu601 => ffi::ASTYuvCscStandardEXT::Itu601,
            YuvCscStandard::Itu601FullRange => ffi::ASTYuvCscStandardEXT::Itu601FullRange,
            YuvCscStandard::Itu709 => ffi::ASTYuvCscStandardEXT::Itu709,
        }
    }
}

impl From<BlockStorage> for ffi::ASTLayoutBlockStorage {
    fn from(value: BlockStorage) -> Self {
        match value {
            BlockStorage::Shared => ffi::ASTLayoutBlockStorage::Shared,
            BlockStorage::Packed => ffi::ASTLayoutBlockStorage::Packed,
            BlockStorage::Std140 => ffi::ASTLayoutBlockStorage::Std140,
            BlockStorage::Std430 => ffi::ASTLayoutBlockStorage::Std430,
        }
    }
}

impl From<MatrixPacking> for ffi::ASTLayoutMatrixPacking {
    fn from(value: MatrixPacking) -> Self {
        match value {
            MatrixPacking::RowMajor => ffi::ASTLayoutMatrixPacking::RowMajor,
            MatrixPacking::ColumnMajor => ffi::ASTLayoutMatrixPacking::ColumnMajor,
        }
    }
}

impl From<Depth> for ffi::ASTLayoutDepth {
    fn from(value: Depth) -> Self {
        match value {
            Depth::Any => ffi::ASTLayoutDepth::Any,
            Depth::Greater => ffi::ASTLayoutDepth::Greater,
            Depth::Less => ffi::ASTLayoutDepth::Less,
            Depth::Unchanged => ffi::ASTLayoutDepth::Unchanged,
        }
    }
}

impl From<ImageInternalFormat> for ffi::ASTLayoutImageInternalFormat {
    fn from(value: ImageInternalFormat) -> Self {
        match value {
            ImageInternalFormat::RGBA32F => ffi::ASTLayoutImageInternalFormat::RGBA32F,
            ImageInternalFormat::RGBA16F => ffi::ASTLayoutImageInternalFormat::RGBA16F,
            ImageInternalFormat::R32F => ffi::ASTLayoutImageInternalFormat::R32F,
            ImageInternalFormat::RGBA32UI => ffi::ASTLayoutImageInternalFormat::RGBA32UI,
            ImageInternalFormat::RGBA16UI => ffi::ASTLayoutImageInternalFormat::RGBA16UI,
            ImageInternalFormat::RGBA8UI => ffi::ASTLayoutImageInternalFormat::RGBA8UI,
            ImageInternalFormat::R32UI => ffi::ASTLayoutImageInternalFormat::R32UI,
            ImageInternalFormat::RGBA32I => ffi::ASTLayoutImageInternalFormat::RGBA32I,
            ImageInternalFormat::RGBA16I => ffi::ASTLayoutImageInternalFormat::RGBA16I,
            ImageInternalFormat::RGBA8I => ffi::ASTLayoutImageInternalFormat::RGBA8I,
            ImageInternalFormat::R32I => ffi::ASTLayoutImageInternalFormat::R32I,
            ImageInternalFormat::RGBA8 => ffi::ASTLayoutImageInternalFormat::RGBA8,
            ImageInternalFormat::RGBA8SNORM => ffi::ASTLayoutImageInternalFormat::RGBA8SNORM,
        }
    }
}

pub struct Generator<'options> {
    // The IR types are mapped to a base TType; which are augmented with layout qualifiers etc when
    // variables / functions are declared.
    types: HashMap<TypeId, *mut TType>,
    // Small constants are stored in TIntermConstantUnion.  Larger constants are stored in a global
    // variable (in global_variables, with a TIntermSymbol stashed here).  Fields of nameless
    // interface blocks are stored by their interface block id and field index.
    constants: HashMap<ConstantId, *mut TIntermTyped>,
    variables: HashMap<VariableId, *mut TIntermTyped>,
    nameless_block_field_variables: HashMap<(VariableId, u32), *mut TIntermTyped>,
    functions: HashMap<FunctionId, *mut TFunction>,

    // Global nodes like layout(early_fragment_tests).
    preamble: Vec<*mut TIntermNode>,
    // Struct type declarations
    type_declarations: Vec<*mut TIntermNode>,
    // List of global variables declarations.
    global_variables: Vec<*mut TIntermNode>,
    // The functions (TIntermFunctionDeclaration) in call DAG order
    function_declarations: Vec<*mut TIntermNode>,

    // Every register is a typed value, mapped to a TIntermTyped.  Later when the register is used,
    // the corresponding node is used as child of a new node.  The first use of the node can
    // directly use the pointer, but consecutive uses need to deepCopy() the node.  Unconditional
    // deepCopy() is simpler, but can lead to lots of unnecessary copies.  `needs_deep_copy` tracks
    // this.
    expressions: HashMap<RegisterId, *mut TIntermTyped>,
    needs_deep_copy: HashSet<RegisterId>,

    // Used by legacy code to declare types and variables.
    legacy_compiler: *mut TCompiler,
    // Derived from the GLSL version, used to decide which built-in to use, e.g. texture2D() vs
    // texture() and other ES1 vs ES3 differences.
    options: &'options compile::Options,
}

impl<'options> Generator<'options> {
    pub fn new(
        legacy_compiler: *mut TCompiler,
        options: &'options compile::Options,
    ) -> Generator<'options> {
        Generator {
            types: HashMap::new(),
            constants: HashMap::new(),
            variables: HashMap::new(),
            nameless_block_field_variables: HashMap::new(),
            functions: HashMap::new(),
            preamble: Vec::new(),
            type_declarations: Vec::new(),
            global_variables: Vec::new(),
            function_declarations: Vec::new(),
            expressions: HashMap::new(),
            needs_deep_copy: HashSet::new(),
            legacy_compiler,
            options,
        }
    }

    fn legacy_symbol_type(name: &Name) -> ffi::ASTSymbolType {
        debug_assert!(!name.name.starts_with("gl_"));
        match name.source {
            NameSource::ShaderInterface => {
                if name.name.is_empty() {
                    ffi::ASTSymbolType::Empty
                } else {
                    ffi::ASTSymbolType::UserDefined
                }
            }
            NameSource::Internal => {
                if name.name == "main" {
                    // `main` is considered a user-defined name in the AST, special as it's handled.
                    ffi::ASTSymbolType::UserDefined
                } else {
                    ffi::ASTSymbolType::AngleInternal
                }
            }
            NameSource::Temporary => {
                if name.name.is_empty() {
                    ffi::ASTSymbolType::AngleInternal
                } else {
                    ffi::ASTSymbolType::UserDefined
                }
            }
        }
    }

    fn legacy_struct_symbol_type(name: &Name) -> ffi::ASTSymbolType {
        // For structs, the rules are a little different compared with other symbols:
        //
        // * Structs at global scope are marked as ShaderInterface, but their symbol type is
        //   AngleInternal instead of Empty.
        // * gl_DepthRangeParameters needs to be handled and given a BuiltIn symbol type.
        match name.source {
            NameSource::ShaderInterface | NameSource::Temporary => {
                if name.name.is_empty() {
                    ffi::ASTSymbolType::AngleInternal
                } else {
                    ffi::ASTSymbolType::UserDefined
                }
            }
            NameSource::Internal => {
                if name.name.starts_with("gl_") {
                    ffi::ASTSymbolType::BuiltIn
                } else {
                    ffi::ASTSymbolType::AngleInternal
                }
            }
        }
    }

    fn id_to_append(name: &Name, id: u32) -> u32 {
        if name.source == NameSource::Temporary && !name.name.is_empty() {
            id
        } else {
            SYMBOL_NAME_NO_ID
        }
    }

    fn legacy_param_direction(direction: FunctionParamDirection) -> ffi::ASTQualifier {
        match direction {
            FunctionParamDirection::Input => ffi::ASTQualifier::ParamIn,
            FunctionParamDirection::Output => ffi::ASTQualifier::ParamOut,
            FunctionParamDirection::InputOutput => ffi::ASTQualifier::ParamInOut,
        }
    }

    fn legacy_blend_equation_advanced(equations: &AdvancedBlendEquations) -> u32 {
        // Identical to gl::BlendEquationType, only including advanced blend equations.
        const BLEND_MULTIPLY: u32 = 1 << 6;
        const BLEND_SCREEN: u32 = 1 << 7;
        const BLEND_OVERLAY: u32 = 1 << 8;
        const BLEND_DARKEN: u32 = 1 << 9;
        const BLEND_LIGHTEN: u32 = 1 << 10;
        const BLEND_COLORDODGE: u32 = 1 << 11;
        const BLEND_COLORBURN: u32 = 1 << 12;
        const BLEND_HARDLIGHT: u32 = 1 << 13;
        const BLEND_SOFTLIGHT: u32 = 1 << 14;
        const BLEND_DIFFERENCE: u32 = 1 << 16;
        const BLEND_EXCLUSION: u32 = 1 << 18;
        const BLEND_HSL_HUE: u32 = 1 << 19;
        const BLEND_HSL_SATURATION: u32 = 1 << 20;
        const BLEND_HSL_COLOR: u32 = 1 << 21;
        const BLEND_HSL_LUMINOSITY: u32 = 1 << 22;

        let mut value = 0;

        if equations.multiply {
            value |= BLEND_MULTIPLY;
        }
        if equations.screen {
            value |= BLEND_SCREEN;
        }
        if equations.overlay {
            value |= BLEND_OVERLAY;
        }
        if equations.darken {
            value |= BLEND_DARKEN;
        }
        if equations.lighten {
            value |= BLEND_LIGHTEN;
        }
        if equations.colordodge {
            value |= BLEND_COLORDODGE;
        }
        if equations.colorburn {
            value |= BLEND_COLORBURN;
        }
        if equations.hardlight {
            value |= BLEND_HARDLIGHT;
        }
        if equations.softlight {
            value |= BLEND_SOFTLIGHT;
        }
        if equations.difference {
            value |= BLEND_DIFFERENCE;
        }
        if equations.exclusion {
            value |= BLEND_EXCLUSION;
        }
        if equations.hsl_hue {
            value |= BLEND_HSL_HUE;
        }
        if equations.hsl_saturation {
            value |= BLEND_HSL_SATURATION;
        }
        if equations.hsl_color {
            value |= BLEND_HSL_COLOR;
        }
        if equations.hsl_luminosity {
            value |= BLEND_HSL_LUMINOSITY;
        }

        value
    }

    fn legacy_basic_type(basic_type: BasicType) -> ffi::ASTBasicType {
        match basic_type {
            BasicType::Void => ffi::ASTBasicType::Void,
            BasicType::Float => ffi::ASTBasicType::Float,
            BasicType::Int => ffi::ASTBasicType::Int,
            BasicType::Uint => ffi::ASTBasicType::UInt,
            BasicType::Bool => ffi::ASTBasicType::Bool,
            BasicType::AtomicCounter => ffi::ASTBasicType::AtomicCounter,
            BasicType::YuvCscStandard => ffi::ASTBasicType::YuvCscStandardEXT,
        }
    }

    fn legacy_image_basic_type(
        image_basic_type: ImageBasicType,
        image_type: &ImageType,
    ) -> ffi::ASTBasicType {
        match image_type.dimension {
            ImageDimension::D2 => match image_basic_type {
                ImageBasicType::Float => {
                    if image_type.is_sampled {
                        if image_type.is_shadow {
                            if image_type.is_array {
                                ffi::ASTBasicType::Sampler2DArrayShadow
                            } else {
                                ffi::ASTBasicType::Sampler2DShadow
                            }
                        } else {
                            match (image_type.is_ms, image_type.is_array) {
                                (false, false) => ffi::ASTBasicType::Sampler2D,
                                (false, true) => ffi::ASTBasicType::Sampler2DArray,
                                (true, false) => ffi::ASTBasicType::Sampler2DMS,
                                (true, true) => ffi::ASTBasicType::Sampler2DMSArray,
                            }
                        }
                    } else {
                        // Multisampled storage images are a desktop GLSL feature
                        debug_assert!(!image_type.is_ms);
                        if image_type.is_array {
                            ffi::ASTBasicType::Image2DArray
                        } else {
                            ffi::ASTBasicType::Image2D
                        }
                    }
                }
                ImageBasicType::Int => {
                    if image_type.is_sampled {
                        match (image_type.is_ms, image_type.is_array) {
                            (false, false) => ffi::ASTBasicType::ISampler2D,
                            (false, true) => ffi::ASTBasicType::ISampler2DArray,
                            (true, false) => ffi::ASTBasicType::ISampler2DMS,
                            (true, true) => ffi::ASTBasicType::ISampler2DMSArray,
                        }
                    } else {
                        debug_assert!(!image_type.is_ms);
                        if image_type.is_array {
                            ffi::ASTBasicType::IImage2DArray
                        } else {
                            ffi::ASTBasicType::IImage2D
                        }
                    }
                }
                ImageBasicType::Uint => {
                    if image_type.is_sampled {
                        match (image_type.is_ms, image_type.is_array) {
                            (false, false) => ffi::ASTBasicType::USampler2D,
                            (false, true) => ffi::ASTBasicType::USampler2DArray,
                            (true, false) => ffi::ASTBasicType::USampler2DMS,
                            (true, true) => ffi::ASTBasicType::USampler2DMSArray,
                        }
                    } else {
                        if image_type.is_array {
                            ffi::ASTBasicType::UImage2DArray
                        } else {
                            ffi::ASTBasicType::UImage2D
                        }
                    }
                }
            },
            ImageDimension::D3 => match image_basic_type {
                ImageBasicType::Float => {
                    if image_type.is_sampled {
                        ffi::ASTBasicType::Sampler3D
                    } else {
                        ffi::ASTBasicType::Image3D
                    }
                }
                ImageBasicType::Int => {
                    if image_type.is_sampled {
                        ffi::ASTBasicType::ISampler3D
                    } else {
                        ffi::ASTBasicType::IImage3D
                    }
                }
                ImageBasicType::Uint => {
                    if image_type.is_sampled {
                        ffi::ASTBasicType::USampler3D
                    } else {
                        ffi::ASTBasicType::UImage3D
                    }
                }
            },
            ImageDimension::Cube => match image_basic_type {
                ImageBasicType::Float => {
                    if image_type.is_sampled {
                        if image_type.is_shadow {
                            if image_type.is_array {
                                ffi::ASTBasicType::SamplerCubeArrayShadow
                            } else {
                                ffi::ASTBasicType::SamplerCubeShadow
                            }
                        } else {
                            if image_type.is_array {
                                ffi::ASTBasicType::SamplerCubeArray
                            } else {
                                ffi::ASTBasicType::SamplerCube
                            }
                        }
                    } else {
                        if image_type.is_array {
                            ffi::ASTBasicType::ImageCubeArray
                        } else {
                            ffi::ASTBasicType::ImageCube
                        }
                    }
                }
                ImageBasicType::Int => {
                    if image_type.is_sampled {
                        if image_type.is_array {
                            ffi::ASTBasicType::ISamplerCubeArray
                        } else {
                            ffi::ASTBasicType::ISamplerCube
                        }
                    } else {
                        if image_type.is_array {
                            ffi::ASTBasicType::IImageCubeArray
                        } else {
                            ffi::ASTBasicType::IImageCube
                        }
                    }
                }
                ImageBasicType::Uint => {
                    if image_type.is_sampled {
                        if image_type.is_array {
                            ffi::ASTBasicType::USamplerCubeArray
                        } else {
                            ffi::ASTBasicType::USamplerCube
                        }
                    } else {
                        if image_type.is_array {
                            ffi::ASTBasicType::UImageCubeArray
                        } else {
                            ffi::ASTBasicType::UImageCube
                        }
                    }
                }
            },
            ImageDimension::Rect => match image_basic_type {
                ImageBasicType::Float => {
                    // Rect storage images are a desktop GLSL feature
                    debug_assert!(image_type.is_sampled);
                    ffi::ASTBasicType::Sampler2DRect
                }
                ImageBasicType::Int => {
                    debug_assert!(image_type.is_sampled);
                    ffi::ASTBasicType::ISampler2DRect
                }
                ImageBasicType::Uint => {
                    debug_assert!(image_type.is_sampled);
                    ffi::ASTBasicType::USampler2DRect
                }
            },
            ImageDimension::Buffer => match image_basic_type {
                ImageBasicType::Float => {
                    if image_type.is_sampled {
                        ffi::ASTBasicType::SamplerBuffer
                    } else {
                        ffi::ASTBasicType::ImageBuffer
                    }
                }
                ImageBasicType::Int => {
                    if image_type.is_sampled {
                        ffi::ASTBasicType::ISamplerBuffer
                    } else {
                        ffi::ASTBasicType::IImageBuffer
                    }
                }
                ImageBasicType::Uint => {
                    if image_type.is_sampled {
                        ffi::ASTBasicType::USamplerBuffer
                    } else {
                        ffi::ASTBasicType::UImageBuffer
                    }
                }
            },
            ImageDimension::External => ffi::ASTBasicType::SamplerExternalOES,
            ImageDimension::ExternalY2Y => ffi::ASTBasicType::SamplerExternal2DY2YEXT,
            ImageDimension::Video => ffi::ASTBasicType::SamplerVideoWEBGL,
            ImageDimension::PixelLocal => match image_basic_type {
                ImageBasicType::Float => ffi::ASTBasicType::PixelLocalANGLE,
                ImageBasicType::Int => ffi::ASTBasicType::IPixelLocalANGLE,
                ImageBasicType::Uint => ffi::ASTBasicType::UPixelLocalANGLE,
            },
            ImageDimension::Subpass => match image_basic_type {
                ImageBasicType::Float => ffi::ASTBasicType::SubpassInput,
                ImageBasicType::Int => ffi::ASTBasicType::ISubpassInput,
                ImageBasicType::Uint => ffi::ASTBasicType::USubpassInput,
            },
        }
    }

    fn get_qualifier(
        shader_type: ShaderType,
        is_es1: bool,
        decorations: &Decorations,
        built_in: Option<BuiltIn>,
        is_global: bool,
    ) -> ffi::ASTQualifier {
        if let Some(built_in) = built_in {
            match built_in {
                BuiltIn::InstanceID => ffi::ASTQualifier::InstanceID,
                BuiltIn::VertexID => ffi::ASTQualifier::VertexID,
                BuiltIn::Position => ffi::ASTQualifier::Position,
                BuiltIn::PointSize => ffi::ASTQualifier::PointSize,
                BuiltIn::BaseVertex => ffi::ASTQualifier::BaseVertex,
                BuiltIn::BaseInstance => ffi::ASTQualifier::BaseInstance,
                BuiltIn::DrawID => ffi::ASTQualifier::DrawID,
                BuiltIn::FragCoord => ffi::ASTQualifier::FragCoord,
                BuiltIn::FrontFacing => ffi::ASTQualifier::FrontFacing,
                BuiltIn::PointCoord => ffi::ASTQualifier::PointCoord,
                BuiltIn::HelperInvocation => ffi::ASTQualifier::HelperInvocation,
                BuiltIn::FragColor => ffi::ASTQualifier::FragColor,
                BuiltIn::FragData => ffi::ASTQualifier::FragData,
                BuiltIn::FragDepth => ffi::ASTQualifier::FragDepth,
                BuiltIn::SecondaryFragColorEXT => ffi::ASTQualifier::SecondaryFragColorEXT,
                BuiltIn::SecondaryFragDataEXT => ffi::ASTQualifier::SecondaryFragDataEXT,
                BuiltIn::DepthRange => ffi::ASTQualifier::DepthRange,
                BuiltIn::ViewIDOVR => ffi::ASTQualifier::ViewIDOVR,
                BuiltIn::ClipDistance => ffi::ASTQualifier::ClipDistance,
                BuiltIn::CullDistance => ffi::ASTQualifier::CullDistance,
                BuiltIn::LastFragColor => ffi::ASTQualifier::LastFragColor,
                BuiltIn::LastFragData => ffi::ASTQualifier::LastFragData,
                BuiltIn::LastFragDepthARM => ffi::ASTQualifier::LastFragDepth,
                BuiltIn::LastFragStencilARM => ffi::ASTQualifier::LastFragStencil,
                BuiltIn::ShadingRateEXT => ffi::ASTQualifier::ShadingRateEXT,
                BuiltIn::PrimitiveShadingRateEXT => ffi::ASTQualifier::PrimitiveShadingRateEXT,
                BuiltIn::SampleID => ffi::ASTQualifier::SampleID,
                BuiltIn::SamplePosition => ffi::ASTQualifier::SamplePosition,
                BuiltIn::SampleMaskIn => ffi::ASTQualifier::SampleMaskIn,
                BuiltIn::SampleMask => ffi::ASTQualifier::SampleMask,
                BuiltIn::NumSamples => ffi::ASTQualifier::NumSamples,
                BuiltIn::NumWorkGroups => ffi::ASTQualifier::NumWorkGroups,
                BuiltIn::WorkGroupSize => ffi::ASTQualifier::WorkGroupSize,
                BuiltIn::WorkGroupID => ffi::ASTQualifier::WorkGroupID,
                BuiltIn::LocalInvocationID => ffi::ASTQualifier::LocalInvocationID,
                BuiltIn::GlobalInvocationID => ffi::ASTQualifier::GlobalInvocationID,
                BuiltIn::LocalInvocationIndex => ffi::ASTQualifier::LocalInvocationIndex,
                BuiltIn::PerVertexIn => ffi::ASTQualifier::PerVertexIn,
                BuiltIn::PerVertexOut => ffi::ASTQualifier::PerVertexOut,
                BuiltIn::PrimitiveIDIn => ffi::ASTQualifier::PrimitiveIDIn,
                BuiltIn::InvocationID => ffi::ASTQualifier::InvocationID,
                BuiltIn::PrimitiveID => ffi::ASTQualifier::PrimitiveID,
                BuiltIn::LayerOut => ffi::ASTQualifier::LayerOut,
                BuiltIn::LayerIn => ffi::ASTQualifier::LayerIn,
                BuiltIn::PatchVerticesIn => ffi::ASTQualifier::PatchVerticesIn,
                BuiltIn::TessLevelOuter => ffi::ASTQualifier::TessLevelOuter,
                BuiltIn::TessLevelInner => ffi::ASTQualifier::TessLevelInner,
                BuiltIn::TessCoord => ffi::ASTQualifier::TessCoord,
                BuiltIn::BoundingBoxOES => ffi::ASTQualifier::BoundingBox,
                BuiltIn::PixelLocalEXT => ffi::ASTQualifier::PixelLocalEXT,
            }
        } else if decorations
            .decorations
            .iter()
            .any(|&decoration| matches!(decoration, Decoration::SpecConst(_)))
        {
            ffi::ASTQualifier::SpecConst
        } else {
            let is_input = decorations.has(Decoration::Input);
            let is_output = decorations.has(Decoration::Output);
            let is_inout = decorations.has(Decoration::InputOutput);
            let is_uniform = decorations.has(Decoration::Uniform);
            let is_buffer = decorations.has(Decoration::Buffer);
            let is_shared = decorations.has(Decoration::Shared);
            let is_smooth = decorations.has(Decoration::Smooth);
            let is_flat = decorations.has(Decoration::Flat);
            let is_noperspective = decorations.has(Decoration::NoPerspective);
            let is_centroid = decorations.has(Decoration::Centroid);
            let is_sample = decorations.has(Decoration::Sample);
            let is_patch = decorations.has(Decoration::Patch);

            if is_uniform {
                ffi::ASTQualifier::Uniform
            } else if is_buffer {
                ffi::ASTQualifier::Buffer
            } else if is_shared {
                ffi::ASTQualifier::Shared
            } else if is_input {
                if is_smooth {
                    ffi::ASTQualifier::SmoothIn
                } else if is_flat {
                    ffi::ASTQualifier::FlatIn
                } else if is_noperspective {
                    if is_centroid {
                        ffi::ASTQualifier::NoPerspectiveCentroidIn
                    } else if is_sample {
                        ffi::ASTQualifier::NoPerspectiveSampleIn
                    } else {
                        ffi::ASTQualifier::NoPerspectiveIn
                    }
                } else if is_centroid {
                    ffi::ASTQualifier::CentroidIn
                } else if is_sample {
                    ffi::ASTQualifier::SampleIn
                } else if is_patch {
                    ffi::ASTQualifier::PatchIn
                } else if is_es1 {
                    if shader_type == ShaderType::Vertex {
                        ffi::ASTQualifier::Attribute
                    } else {
                        ffi::ASTQualifier::VaryingIn
                    }
                } else {
                    match shader_type {
                        ShaderType::Vertex => ffi::ASTQualifier::VertexIn,
                        ShaderType::TessellationControl => ffi::ASTQualifier::TessControlIn,
                        ShaderType::TessellationEvaluation => ffi::ASTQualifier::TessEvaluationIn,
                        ShaderType::Geometry => ffi::ASTQualifier::GeometryIn,
                        ShaderType::Fragment => ffi::ASTQualifier::FragmentIn,
                        ShaderType::Compute => {
                            panic!("Internal error: Unexpected Input decoration in compute shader")
                        }
                    }
                }
            } else if is_output {
                if is_smooth {
                    ffi::ASTQualifier::SmoothOut
                } else if is_flat {
                    ffi::ASTQualifier::FlatOut
                } else if is_noperspective {
                    if is_centroid {
                        ffi::ASTQualifier::NoPerspectiveCentroidOut
                    } else if is_sample {
                        ffi::ASTQualifier::NoPerspectiveSampleOut
                    } else {
                        ffi::ASTQualifier::NoPerspectiveOut
                    }
                } else if is_centroid {
                    ffi::ASTQualifier::CentroidOut
                } else if is_sample {
                    ffi::ASTQualifier::SampleOut
                } else if is_patch {
                    ffi::ASTQualifier::PatchOut
                } else if is_es1 && shader_type != ShaderType::Fragment {
                    ffi::ASTQualifier::VaryingOut
                } else {
                    match shader_type {
                        ShaderType::Vertex => ffi::ASTQualifier::VertexOut,
                        ShaderType::TessellationControl => ffi::ASTQualifier::TessControlOut,
                        ShaderType::TessellationEvaluation => ffi::ASTQualifier::TessEvaluationOut,
                        ShaderType::Geometry => ffi::ASTQualifier::GeometryOut,
                        ShaderType::Fragment => ffi::ASTQualifier::FragmentOut,
                        ShaderType::Compute => {
                            panic!("Internal error: Unexpected Output decoration in compute shader")
                        }
                    }
                }
            } else if is_inout {
                ffi::ASTQualifier::FragmentInOut
            } else if is_global {
                // AST marks fields as EvqGlobal if not otherwise qualified.  If qualified, these
                // must be members of I/O blocks, and they are not qualified as in or out
                // explicitly.
                if is_smooth {
                    ffi::ASTQualifier::Smooth
                } else if is_flat {
                    ffi::ASTQualifier::Flat
                } else if is_noperspective {
                    if is_centroid {
                        ffi::ASTQualifier::NoPerspectiveCentroid
                    } else if is_sample {
                        ffi::ASTQualifier::NoPerspectiveSample
                    } else {
                        ffi::ASTQualifier::NoPerspective
                    }
                } else if is_centroid {
                    ffi::ASTQualifier::Centroid
                } else if is_sample {
                    ffi::ASTQualifier::Sample
                } else if is_patch {
                    ffi::ASTQualifier::Patch
                } else {
                    ffi::ASTQualifier::Global
                }
            } else {
                ffi::ASTQualifier::Temporary
            }
        }
    }

    fn get_layout_qualifier(decorations: &Decorations) -> ffi::ASTLayoutQualifier {
        let mut layout_qualifier = ffi::ASTLayoutQualifier {
            location: -1,
            matrix_packing: ffi::ASTLayoutMatrixPacking::Unspecified,
            block_storage: ffi::ASTLayoutBlockStorage::Unspecified,
            binding: -1,
            offset: -1,
            depth: ffi::ASTLayoutDepth::Unspecified,
            image_internal_format: ffi::ASTLayoutImageInternalFormat::Unspecified,
            num_views: -1,
            yuv: false,
            index: -1,
            noncoherent: false,
            push_constant: false,
            input_attachment_index: -1,
            raster_ordered: false,
        };

        for &decoration in &decorations.decorations {
            match decoration {
                Decoration::PushConstant => layout_qualifier.push_constant = true,
                Decoration::NonCoherent => layout_qualifier.noncoherent = true,
                Decoration::YUV => layout_qualifier.yuv = true,
                Decoration::Location(location) => layout_qualifier.location = location as i32,
                Decoration::Index(index) => layout_qualifier.index = index as i32,
                Decoration::InputAttachmentIndex(index) => {
                    layout_qualifier.input_attachment_index = index as i32
                }
                Decoration::SpecConst(location) => layout_qualifier.location = location as i32,
                Decoration::Block(storage) => layout_qualifier.block_storage = storage.into(),
                Decoration::Binding(binding) => layout_qualifier.binding = binding as i32,
                Decoration::Offset(offset) => layout_qualifier.offset = offset as i32,
                Decoration::MatrixPacking(packing) => {
                    layout_qualifier.matrix_packing = packing.into()
                }
                Decoration::Depth(depth) => layout_qualifier.depth = depth.into(),
                Decoration::ImageInternalFormat(format) => {
                    layout_qualifier.image_internal_format = format.into()
                }
                Decoration::NumViews(num_views) => layout_qualifier.num_views = num_views as i32,
                Decoration::RasterOrdered => layout_qualifier.raster_ordered = true,
                _ => (),
            };
        }

        layout_qualifier
    }

    fn get_memory_qualifier(decorations: &Decorations) -> ffi::ASTMemoryQualifier {
        let readonly = decorations.has(Decoration::ReadOnly);
        let writeonly = decorations.has(Decoration::WriteOnly);
        let coherent = decorations.has(Decoration::Coherent);
        let restrict_qualifier = decorations.has(Decoration::Restrict);
        let volatile_qualifier = decorations.has(Decoration::Volatile);

        ffi::ASTMemoryQualifier {
            readonly,
            writeonly,
            coherent,
            restrict_qualifier,
            volatile_qualifier,
        }
    }

    fn get_ast_type(
        shader_type: ShaderType,
        is_es1: bool,
        precision: Precision,
        decorations: &Decorations,
        built_in: Option<BuiltIn>,
        is_global: bool,
    ) -> ffi::ASTType {
        let invariant = decorations.has(Decoration::Invariant);
        let precise = decorations.has(Decoration::Precise);
        let interpolant = decorations.has(Decoration::Interpolant);

        ffi::ASTType {
            // Note: TypeId is unused after going back to AST.
            type_id: TYPE_ID_VOID.into(),
            qualifier: Self::get_qualifier(shader_type, is_es1, decorations, built_in, is_global),
            precision: precision.into(),
            layout_qualifier: Self::get_layout_qualifier(decorations),
            memory_qualifier: Self::get_memory_qualifier(decorations),
            invariant,
            precise,
            interpolant,
        }
    }

    fn built_in_str(built_in: BuiltIn, shader_type: ShaderType, is_es1: bool) -> &'static str {
        match built_in {
            BuiltIn::InstanceID => "gl_InstanceID",
            BuiltIn::VertexID => "gl_VertexID",
            BuiltIn::Position => "gl_Position",
            BuiltIn::PointSize => "gl_PointSize",
            BuiltIn::BaseVertex => "gl_BaseVertex",
            BuiltIn::BaseInstance => "gl_BaseInstance",
            BuiltIn::DrawID => "gl_DrawID",
            BuiltIn::FragCoord => "gl_FragCoord",
            BuiltIn::FrontFacing => "gl_FrontFacing",
            BuiltIn::PointCoord => "gl_PointCoord",
            BuiltIn::HelperInvocation => "gl_HelperInvocation",
            BuiltIn::FragColor => "gl_FragColor",
            BuiltIn::FragData => "gl_FragData",
            BuiltIn::FragDepth => {
                if is_es1 {
                    "gl_FragDepthEXT"
                } else {
                    "gl_FragDepth"
                }
            }
            BuiltIn::SecondaryFragColorEXT => "gl_SecondaryFragColorEXT",
            BuiltIn::SecondaryFragDataEXT => "gl_SecondaryFragDataEXT",
            BuiltIn::DepthRange => "gl_DepthRange",
            BuiltIn::ViewIDOVR => "gl_ViewID_OVR",
            BuiltIn::ClipDistance => "gl_ClipDistance",
            BuiltIn::CullDistance => "gl_CullDistance",
            BuiltIn::LastFragColor => "gl_LastFragColorARM",
            BuiltIn::LastFragData => "gl_LastFragData",
            BuiltIn::LastFragDepthARM => "gl_LastFragDepthARM",
            BuiltIn::LastFragStencilARM => "gl_LastFragStencilARM",
            BuiltIn::ShadingRateEXT => "gl_ShadingRateEXT",
            BuiltIn::PrimitiveShadingRateEXT => "gl_PrimitiveShadingRateEXT",
            BuiltIn::SampleID => "gl_SampleID",
            BuiltIn::SamplePosition => "gl_SamplePosition",
            BuiltIn::SampleMaskIn => "gl_SampleMaskIn",
            BuiltIn::SampleMask => "gl_SampleMask",
            BuiltIn::NumSamples => "gl_NumSamples",
            BuiltIn::NumWorkGroups => "gl_NumWorkGroups",
            BuiltIn::WorkGroupSize => "gl_WorkGroupSize",
            BuiltIn::WorkGroupID => "gl_WorkGroupID",
            BuiltIn::LocalInvocationID => "gl_LocalInvocationID",
            BuiltIn::GlobalInvocationID => "gl_GlobalInvocationID",
            BuiltIn::LocalInvocationIndex => "gl_LocalInvocationIndex",
            BuiltIn::PerVertexIn => "gl_in",
            BuiltIn::PerVertexOut => {
                if shader_type == ShaderType::TessellationControl {
                    "gl_out"
                } else {
                    ""
                }
            }
            BuiltIn::PrimitiveIDIn => "gl_PrimitiveIDIn",
            BuiltIn::InvocationID => "gl_InvocationID",
            BuiltIn::PrimitiveID => "gl_PrimitiveID",
            BuiltIn::LayerOut => "gl_Layer",
            BuiltIn::LayerIn => "gl_Layer",
            BuiltIn::PatchVerticesIn => "gl_PatchVerticesIn",
            BuiltIn::TessLevelOuter => "gl_TessLevelOuter",
            BuiltIn::TessLevelInner => "gl_TessLevelInner",
            BuiltIn::TessCoord => "gl_TessCoord",
            BuiltIn::BoundingBoxOES => "gl_BoundingBoxOES",
            BuiltIn::PixelLocalEXT => "gl_PixelLocalEXT",
        }
    }

    fn declare_variable(
        &self,
        variable: *mut TIntermTyped,
        initializer: Option<ConstantId>,
    ) -> *mut TIntermNode {
        if let Some(constant_id) = initializer {
            let initializer = self.constants[&constant_id];
            unsafe { ffi::declare_variable_with_initializer(variable, initializer) }
        } else {
            unsafe { ffi::declare_variable(variable) }
        }
    }

    fn get_expression(&mut self, id: TypedId) -> ffi::Expression {
        match id.id {
            Id::Register(id) => {
                // The first time an expression is used, don't duplicate it.  If it's ever used more
                // than once, duplicate it then.
                let needs_copy = !self.needs_deep_copy.insert(id);
                ffi::Expression { node: self.expressions[&id], needs_copy }
            }
            Id::Constant(id) => ffi::Expression { node: self.constants[&id], needs_copy: true },
            Id::Variable(id) => ffi::Expression { node: self.variables[&id], needs_copy: true },
        }
    }

    fn get_sampler_type(ir_meta: &IRMeta, sampler: TypedId) -> ffi::ASTBasicType {
        let (basic_type, image_type) = ir_meta.get_type(sampler.type_id).get_image_type();
        Self::legacy_image_basic_type(basic_type, image_type)
    }
}

impl ast::Target for Generator<'_> {
    type BlockResult = *mut TIntermBlock;

    fn begin(&mut self) {}
    fn end(&mut self) -> *mut TIntermBlock {
        unsafe {
            ffi::finalize(
                self.legacy_compiler,
                &self.type_declarations,
                &self.global_variables,
                &self.function_declarations,
            )
        }
    }

    fn new_type(&mut self, ir_meta: &IRMeta, id: TypeId, type_info: &Type) {
        let legacy_type = match type_info {
            &Type::Scalar(basic_type) => unsafe {
                ffi::make_basic_type(Self::legacy_basic_type(basic_type))
            },
            &Type::Vector(type_id, count) => unsafe {
                ffi::make_vector_type(self.types[&type_id], count)
            },
            &Type::Matrix(type_id, count) => unsafe {
                ffi::make_matrix_type(self.types[&type_id], count)
            },
            &Type::Array(type_id, count) => unsafe {
                ffi::make_array_type(self.types[&type_id], count)
            },
            &Type::UnsizedArray(type_id) => unsafe {
                ffi::make_unsized_array_type(self.types[&type_id])
            },
            &Type::Image(basic_type, ref image_type) => unsafe {
                ffi::make_basic_type(Self::legacy_image_basic_type(basic_type, image_type))
            },
            Type::Struct(name, fields, specialization) => {
                let is_interface_block = *specialization == StructSpecialization::InterfaceBlock;
                let fields = fields
                    .iter()
                    .map(|field| ffi::ASTFieldInfo {
                        name: field.name.name,
                        base_type: self.types[&field.type_id],
                        ast_type: Self::get_ast_type(
                            ir_meta.get_shader_type(),
                            self.options.is_es1,
                            field.precision,
                            &field.decorations,
                            None,
                            // AST marks fields as EvqGlobal
                            true,
                        ),
                    })
                    .collect::<Vec<_>>();

                let symbol_type = Self::legacy_struct_symbol_type(name);
                let legacy_type = unsafe {
                    ffi::make_struct_type(
                        self.legacy_compiler,
                        &ffi::SymbolName {
                            name: name.name,
                            symbol_type,
                            id: Self::id_to_append(name, id.id),
                        },
                        &fields,
                        is_interface_block,
                    )
                };
                if !is_interface_block && symbol_type != ffi::ASTSymbolType::BuiltIn {
                    self.type_declarations
                        .push(unsafe { ffi::declare_struct(self.legacy_compiler, legacy_type) });
                }

                legacy_type
            }
            &Type::Pointer(pointee_type_id) => self.types[&pointee_type_id],
            Type::DeadCodeEliminated => {
                return;
            }
        };

        self.types.insert(id, legacy_type);
    }

    fn new_constant(&mut self, ir_meta: &IRMeta, id: ConstantId, constant: &Constant) {
        let constant = match &constant.value {
            &ConstantValue::Float(f) => unsafe { ffi::make_float_constant(f) },
            &ConstantValue::Int(i) => unsafe { ffi::make_int_constant(i) },
            &ConstantValue::Uint(u) => unsafe { ffi::make_uint_constant(u) },
            &ConstantValue::Bool(b) => unsafe { ffi::make_bool_constant(b) },
            &ConstantValue::YuvCsc(yuv_csc) => unsafe {
                ffi::make_yuv_csc_constant(yuv_csc.into())
            },
            ConstantValue::Composite(elements) => {
                let constant_type = self.types[&constant.type_id];
                let type_info = ir_meta.get_type(constant.type_id);
                let value = unsafe {
                    ffi::make_composite_constant(
                        &elements.iter().map(|element| self.constants[element]).collect::<Vec<_>>(),
                        constant_type,
                    )
                };

                // With the AST, small constants are kept as TIntermConstantUnion, while
                // the rest are stored in temp variables to avoid excessive repetition:
                //
                // - All arrays use temp variables
                // - Otherwise, non-structs use inline constants
                // - Structures with arrays use temp variables
                // - Otherwise only structs with 16 bytes or less are inlined.
                //
                // For simplicity, small struct constants are also placed in variables (which is
                // not a common scenario).
                if type_info.is_array() || type_info.is_struct() {
                    let variable = unsafe {
                        ffi::make_constant_variable(self.legacy_compiler, constant_type, value)
                    };
                    let declaration =
                        unsafe { ffi::declare_variable_with_initializer(variable, value) };

                    self.global_variables.push(declaration);
                    variable
                } else {
                    value
                }
            }
        };

        self.constants.insert(id, constant);
    }

    fn new_variable(&mut self, ir_meta: &IRMeta, id: VariableId, variable: &Variable) {
        let var_type = self.types[&variable.type_id];
        let is_global = variable.scope == VariableScope::Global;
        let ast_type = Self::get_ast_type(
            ir_meta.get_shader_type(),
            self.options.is_es1,
            variable.precision,
            &variable.decorations,
            variable.built_in,
            is_global,
        );

        let (var_name, symbol_type, field_symbol_type) = if let Some(built_in) = variable.built_in {
            let var_name =
                Self::built_in_str(built_in, ir_meta.get_shader_type(), self.options.is_es1);
            (
                var_name,
                if var_name.is_empty() {
                    // gl_PerVertex can have an empty instance name when it's the output of
                    // geometry shader.
                    ffi::ASTSymbolType::Empty
                } else {
                    ffi::ASTSymbolType::BuiltIn
                },
                Some(ffi::ASTSymbolType::BuiltIn),
            )
        } else {
            (variable.name.name, Self::legacy_symbol_type(&variable.name), None)
        };

        let is_redeclared_built_in = match variable.built_in {
            Some(BuiltIn::PerVertexIn) => ir_meta.is_per_vertex_in_redeclared(),
            Some(BuiltIn::PerVertexOut) => ir_meta.is_per_vertex_out_redeclared(),
            // Always redeclare gl_FragDepth if possible even if the shader didn't.
            Some(BuiltIn::FragDepth) => self.options.extensions.EXT_conservative_depth,
            // Always redeclare gl_Clip/CullDistance even if the shader didn't.
            Some(BuiltIn::ClipDistance) | Some(BuiltIn::CullDistance) => true,
            _ => false,
        };

        let legacy_variable = unsafe {
            ffi::make_variable(
                self.legacy_compiler,
                &ffi::SymbolName {
                    name: var_name,
                    symbol_type,
                    id: Self::id_to_append(&variable.name, id.id),
                },
                var_type,
                &ast_type,
                is_redeclared_built_in,
                variable.is_static_use,
            )
        };

        self.variables.insert(id, legacy_variable);

        // Most built-ins may not be redeclared, but may still be modified via global statements
        // like `invariant gl_Position;`.  Take those into account here.
        if !is_redeclared_built_in && variable.built_in.is_some() {
            if ast_type.invariant {
                self.global_variables
                    .push(unsafe { ffi::globally_qualify_built_in_invariant(legacy_variable) });
            }
            if ast_type.precise {
                self.global_variables
                    .push(unsafe { ffi::globally_qualify_built_in_precise(legacy_variable) });
            }
        }

        // The AST expects to have a dedicated variable for each field of an unnamed interface
        // block.  In theory, the IR should be able to still generate (unnamed).field and let the
        // AST side handle things correctly, but currently there are too many places that assume to
        // be able to find or reference nodes based on field alone.
        debug_assert!(ir_meta.get_type(variable.type_id).is_pointer());
        if let Type::Struct(_, fields, StructSpecialization::InterfaceBlock) =
            ir_meta.get_type(ir_meta.get_type(variable.type_id).get_element_type_id().unwrap())
        {
            if symbol_type == ffi::ASTSymbolType::Empty {
                for (index, field) in fields.iter().enumerate() {
                    let field_name = field.name.name;
                    let field_symbol_type =
                        field_symbol_type.unwrap_or_else(|| Self::legacy_symbol_type(&field.name));

                    let field_type = self.types[&field.type_id];
                    let field_ast_type = Self::get_ast_type(
                        ir_meta.get_shader_type(),
                        self.options.is_es1,
                        field.precision,
                        &field.decorations,
                        None,
                        true,
                    );

                    let legacy_field_variable = unsafe {
                        ffi::make_nameless_block_field_variable(
                            self.legacy_compiler,
                            legacy_variable,
                            index as u32,
                            &ffi::SymbolName {
                                name: field_name,
                                symbol_type: field_symbol_type,
                                id: SYMBOL_NAME_NO_ID,
                            },
                            field_type,
                            &field_ast_type,
                        )
                    };

                    self.nameless_block_field_variables
                        .insert((id, index as u32), legacy_field_variable);
                }
            }
        }
    }

    fn new_function(&mut self, ir_meta: &IRMeta, id: FunctionId, function: &Function) {
        let params = function
            .params
            .iter()
            .map(|param| self.variables[&param.variable_id])
            .collect::<Vec<_>>();
        let param_directions = function
            .params
            .iter()
            .map(|param| Self::legacy_param_direction(param.direction))
            .collect::<Vec<_>>();

        let return_type = self.types[&function.return_type_id];
        let return_ast_type = Self::get_ast_type(
            ir_meta.get_shader_type(),
            self.options.is_es1,
            function.return_precision,
            &function.return_decorations,
            None,
            false,
        );
        let symbol_type = Self::legacy_symbol_type(&function.name);

        let legacy_function = unsafe {
            ffi::make_function(
                self.legacy_compiler,
                &ffi::SymbolName {
                    name: function.name.name,
                    symbol_type,
                    id: Self::id_to_append(&function.name, id.id),
                },
                return_type,
                &return_ast_type,
                &params,
                &param_directions,
            )
        };
        self.functions.insert(id, legacy_function);
    }

    fn global_scope(&mut self, ir_meta: &IRMeta) {
        // The global properties such as early_fragment_tests, blend_equation_advanced, TCS, TES
        // and GS parameters are already parsed and stashed in TCompiler. The IR currently does not
        // modify them, so there is no need to pass them back to it.

        // Declare global variables.  Local variables are declared in the block they are
        // encountered.
        ir_meta.all_global_variables().iter().for_each(|&id| {
            let variable = ir_meta.get_variable(id);
            debug_assert!(variable.scope == VariableScope::Global);

            // Most built-ins cannot be redeclared, don't add a declaration for them in the AST.
            // gl_FragDepth can be redeclared, but not gl_FragDepthEXT.
            let skip_declaration = match variable.built_in {
                Some(BuiltIn::PerVertexIn) => !ir_meta.is_per_vertex_in_redeclared(),
                Some(BuiltIn::PerVertexOut) => !ir_meta.is_per_vertex_out_redeclared(),
                Some(BuiltIn::FragDepth) => !self.options.extensions.EXT_conservative_depth,
                Some(BuiltIn::LastFragData)
                | Some(BuiltIn::LastFragColor)
                | Some(BuiltIn::LastFragDepthARM)
                | Some(BuiltIn::LastFragStencilARM)
                | Some(BuiltIn::ClipDistance)
                | Some(BuiltIn::CullDistance) => false,
                Some(_) => true,
                _ => false,
            };

            if !skip_declaration {
                let declaration = self.declare_variable(self.variables[&id], variable.initializer);
                self.global_variables.push(declaration);
            }
        });
    }

    fn begin_block(&mut self, ir_meta: &IRMeta, variables: &Vec<VariableId>) -> *mut TIntermBlock {
        let block = unsafe { ffi::make_interm_block() };
        variables.iter().for_each(|&id| {
            let variable = ir_meta.get_variable(id);
            let declaration = self.declare_variable(self.variables[&id], variable.initializer);
            unsafe { ffi::append_instructions_to_block(block, &[declaration]) };
        });
        block
    }

    fn merge_blocks(&mut self, blocks: Vec<*mut TIntermBlock>) -> *mut TIntermBlock {
        debug_assert!(!blocks.is_empty());
        unsafe { ffi::append_blocks_to_block(blocks[0], &blocks[1..]) };
        blocks[0]
    }

    fn swizzle_single(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        id: TypedId,
        index: u32,
    ) {
        let expr = unsafe { ffi::swizzle(&self.get_expression(id), &[index]) };
        self.expressions.insert(result, expr);
    }

    fn swizzle_multi(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        id: TypedId,
        indices: &Vec<u32>,
    ) {
        let expr = unsafe { ffi::swizzle(&self.get_expression(id), indices) };
        self.expressions.insert(result, expr);
    }

    fn index(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        id: TypedId,
        index: TypedId,
    ) {
        let expr = unsafe { ffi::index(&self.get_expression(id), &self.get_expression(index)) };
        self.expressions.insert(result, expr);
    }

    fn select_field(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        id: TypedId,
        index: u32,
        _field: &Field,
    ) {
        // When selecting a field of a nameless interface block, the AST expects to see references
        // to the field variables directly.
        if let Id::Variable(var_id) = id.id {
            if let Some(&field_variable) = self.nameless_block_field_variables.get(&(var_id, index))
            {
                self.expressions.insert(result, field_variable);
                self.needs_deep_copy.insert(result);
                return;
            }
        }
        let expr = unsafe { ffi::select_field(&self.get_expression(id), index) };
        self.expressions.insert(result, expr);
    }

    fn construct_single(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        type_id: TypeId,
        id: TypedId,
    ) {
        let expr = unsafe { ffi::construct(self.types[&type_id], &[self.get_expression(id)]) };
        self.expressions.insert(result, expr);
    }

    fn construct_multi(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        type_id: TypeId,
        ids: &Vec<TypedId>,
    ) {
        let expr = unsafe {
            ffi::construct(
                self.types[&type_id],
                &ids.iter().map(|&id| self.get_expression(id)).collect::<Vec<_>>(),
            )
        };
        self.expressions.insert(result, expr);
    }

    fn load(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        pointer: TypedId,
    ) {
        // Nothing to do, AST does not distinguish pointers from values, so load is implicitly
        // done.  Just associate the id of the loaded value to the pointer expression.
        let expr = self.get_expression(pointer);
        self.expressions.insert(result, expr.node);

        // Because two ids reference the same node, make sure the duplicate one always requires a
        // copy.
        self.needs_deep_copy.insert(result);
    }

    fn store(&mut self, block_result: &mut *mut TIntermBlock, pointer: TypedId, value: TypedId) {
        let assignment =
            unsafe { ffi::store(&self.get_expression(pointer), &self.get_expression(value)) };
        unsafe { ffi::append_instructions_to_block(*block_result, &[assignment]) };
    }

    fn call(
        &mut self,
        block_result: &mut *mut TIntermBlock,
        result: Option<RegisterId>,
        function_id: FunctionId,
        params: &Vec<TypedId>,
    ) {
        let params = params.iter().map(|&id| self.get_expression(id)).collect::<Vec<_>>();
        let function = self.functions[&function_id];
        match result {
            Some(result) => {
                let expr = unsafe { ffi::call(function, &params) };
                self.expressions.insert(result, expr);
            }
            None => {
                let statement = unsafe { ffi::call_void(function, &params) };
                unsafe { ffi::append_instructions_to_block(*block_result, &[statement]) };
            }
        };
    }

    fn unary(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        unary_op: UnaryOpCode,
        id: TypedId,
    ) {
        let operand = self.get_expression(id);
        let expr = unsafe {
            match unary_op {
                UnaryOpCode::ArrayLength => ffi::array_length(&operand),
                UnaryOpCode::Negate => ffi::negate(&operand),
                UnaryOpCode::PostfixIncrement => ffi::postfix_increment(&operand),
                UnaryOpCode::PostfixDecrement => ffi::postfix_decrement(&operand),
                UnaryOpCode::PrefixIncrement => ffi::prefix_increment(&operand),
                UnaryOpCode::PrefixDecrement => ffi::prefix_decrement(&operand),
                UnaryOpCode::LogicalNot => ffi::logical_not(&operand),
                UnaryOpCode::BitwiseNot => ffi::bitwise_not(&operand),
                UnaryOpCode::Radians => ffi::built_in_radians(self.legacy_compiler, &operand),
                UnaryOpCode::Degrees => ffi::built_in_degrees(self.legacy_compiler, &operand),
                UnaryOpCode::Sin => ffi::built_in_sin(self.legacy_compiler, &operand),
                UnaryOpCode::Cos => ffi::built_in_cos(self.legacy_compiler, &operand),
                UnaryOpCode::Tan => ffi::built_in_tan(self.legacy_compiler, &operand),
                UnaryOpCode::Asin => ffi::built_in_asin(self.legacy_compiler, &operand),
                UnaryOpCode::Acos => ffi::built_in_acos(self.legacy_compiler, &operand),
                UnaryOpCode::Atan => ffi::built_in_atan(self.legacy_compiler, &operand),
                UnaryOpCode::Sinh => ffi::built_in_sinh(self.legacy_compiler, &operand),
                UnaryOpCode::Cosh => ffi::built_in_cosh(self.legacy_compiler, &operand),
                UnaryOpCode::Tanh => ffi::built_in_tanh(self.legacy_compiler, &operand),
                UnaryOpCode::Asinh => ffi::built_in_asinh(self.legacy_compiler, &operand),
                UnaryOpCode::Acosh => ffi::built_in_acosh(self.legacy_compiler, &operand),
                UnaryOpCode::Atanh => ffi::built_in_atanh(self.legacy_compiler, &operand),
                UnaryOpCode::Exp => ffi::built_in_exp(self.legacy_compiler, &operand),
                UnaryOpCode::Log => ffi::built_in_log(self.legacy_compiler, &operand),
                UnaryOpCode::Exp2 => ffi::built_in_exp2(self.legacy_compiler, &operand),
                UnaryOpCode::Log2 => ffi::built_in_log2(self.legacy_compiler, &operand),
                UnaryOpCode::Sqrt => ffi::built_in_sqrt(self.legacy_compiler, &operand),
                UnaryOpCode::Inversesqrt => {
                    ffi::built_in_inversesqrt(self.legacy_compiler, &operand)
                }
                UnaryOpCode::Abs => ffi::built_in_abs(self.legacy_compiler, &operand),
                UnaryOpCode::Sign => ffi::built_in_sign(self.legacy_compiler, &operand),
                UnaryOpCode::Floor => ffi::built_in_floor(self.legacy_compiler, &operand),
                UnaryOpCode::Trunc => ffi::built_in_trunc(self.legacy_compiler, &operand),
                UnaryOpCode::Round => ffi::built_in_round(self.legacy_compiler, &operand),
                UnaryOpCode::RoundEven => ffi::built_in_roundeven(self.legacy_compiler, &operand),
                UnaryOpCode::Ceil => ffi::built_in_ceil(self.legacy_compiler, &operand),
                UnaryOpCode::Fract => ffi::built_in_fract(self.legacy_compiler, &operand),
                UnaryOpCode::Isnan => ffi::built_in_isnan(self.legacy_compiler, &operand),
                UnaryOpCode::Isinf => ffi::built_in_isinf(self.legacy_compiler, &operand),
                UnaryOpCode::FloatBitsToInt => {
                    ffi::built_in_floatbitstoint(self.legacy_compiler, &operand)
                }
                UnaryOpCode::FloatBitsToUint => {
                    ffi::built_in_floatbitstouint(self.legacy_compiler, &operand)
                }
                UnaryOpCode::IntBitsToFloat => {
                    ffi::built_in_intbitstofloat(self.legacy_compiler, &operand)
                }
                UnaryOpCode::UintBitsToFloat => {
                    ffi::built_in_uintbitstofloat(self.legacy_compiler, &operand)
                }
                UnaryOpCode::PackSnorm2x16 => {
                    ffi::built_in_packsnorm2x16(self.legacy_compiler, &operand)
                }
                UnaryOpCode::PackHalf2x16 => {
                    ffi::built_in_packhalf2x16(self.legacy_compiler, &operand)
                }
                UnaryOpCode::UnpackSnorm2x16 => {
                    ffi::built_in_unpacksnorm2x16(self.legacy_compiler, &operand)
                }
                UnaryOpCode::UnpackHalf2x16 => {
                    ffi::built_in_unpackhalf2x16(self.legacy_compiler, &operand)
                }
                UnaryOpCode::PackUnorm2x16 => {
                    ffi::built_in_packunorm2x16(self.legacy_compiler, &operand)
                }
                UnaryOpCode::UnpackUnorm2x16 => {
                    ffi::built_in_unpackunorm2x16(self.legacy_compiler, &operand)
                }
                UnaryOpCode::PackUnorm4x8 => {
                    ffi::built_in_packunorm4x8(self.legacy_compiler, &operand)
                }
                UnaryOpCode::PackSnorm4x8 => {
                    ffi::built_in_packsnorm4x8(self.legacy_compiler, &operand)
                }
                UnaryOpCode::UnpackUnorm4x8 => {
                    ffi::built_in_unpackunorm4x8(self.legacy_compiler, &operand)
                }
                UnaryOpCode::UnpackSnorm4x8 => {
                    ffi::built_in_unpacksnorm4x8(self.legacy_compiler, &operand)
                }
                UnaryOpCode::Length => ffi::built_in_length(self.legacy_compiler, &operand),
                UnaryOpCode::Normalize => ffi::built_in_normalize(self.legacy_compiler, &operand),
                UnaryOpCode::Transpose => ffi::built_in_transpose(self.legacy_compiler, &operand),
                UnaryOpCode::Determinant => {
                    ffi::built_in_determinant(self.legacy_compiler, &operand)
                }
                UnaryOpCode::Inverse => ffi::built_in_inverse(self.legacy_compiler, &operand),
                UnaryOpCode::Any => ffi::built_in_any(self.legacy_compiler, &operand),
                UnaryOpCode::All => ffi::built_in_all(self.legacy_compiler, &operand),
                UnaryOpCode::Not => ffi::built_in_not(self.legacy_compiler, &operand),
                UnaryOpCode::BitfieldReverse => {
                    ffi::built_in_bitfieldreverse(self.legacy_compiler, &operand)
                }
                UnaryOpCode::BitCount => ffi::built_in_bitcount(self.legacy_compiler, &operand),
                UnaryOpCode::FindLSB => ffi::built_in_findlsb(self.legacy_compiler, &operand),
                UnaryOpCode::FindMSB => ffi::built_in_findmsb(self.legacy_compiler, &operand),
                UnaryOpCode::DFdx => ffi::built_in_dfdx(self.legacy_compiler, &operand),
                UnaryOpCode::DFdy => ffi::built_in_dfdy(self.legacy_compiler, &operand),
                UnaryOpCode::Fwidth => ffi::built_in_fwidth(self.legacy_compiler, &operand),
                UnaryOpCode::InterpolateAtCentroid => {
                    ffi::built_in_interpolateatcentroid(self.legacy_compiler, &operand)
                }
                UnaryOpCode::AtomicCounter => {
                    ffi::built_in_atomiccounter(self.legacy_compiler, &operand)
                }
                UnaryOpCode::AtomicCounterIncrement => {
                    ffi::built_in_atomiccounterincrement(self.legacy_compiler, &operand)
                }
                UnaryOpCode::AtomicCounterDecrement => {
                    ffi::built_in_atomiccounterdecrement(self.legacy_compiler, &operand)
                }
                UnaryOpCode::ImageSize => ffi::built_in_imagesize(self.legacy_compiler, &operand),
                UnaryOpCode::PixelLocalLoadANGLE => {
                    ffi::built_in_pixellocalload(self.legacy_compiler, &operand)
                }
            }
        };
        self.expressions.insert(result, expr);
    }

    fn binary(
        &mut self,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        binary_op: BinaryOpCode,
        lhs: TypedId,
        rhs: TypedId,
    ) {
        let lhs = self.get_expression(lhs);
        let rhs = self.get_expression(rhs);
        let expr = unsafe {
            match binary_op {
                BinaryOpCode::Add => ffi::add(&lhs, &rhs),
                BinaryOpCode::Sub => ffi::sub(&lhs, &rhs),
                BinaryOpCode::Mul => ffi::mul(&lhs, &rhs),
                BinaryOpCode::VectorTimesScalar => ffi::vector_times_scalar(&lhs, &rhs),
                BinaryOpCode::MatrixTimesScalar => ffi::matrix_times_scalar(&lhs, &rhs),
                BinaryOpCode::VectorTimesMatrix => ffi::vector_times_matrix(&lhs, &rhs),
                BinaryOpCode::MatrixTimesVector => ffi::matrix_times_vector(&lhs, &rhs),
                BinaryOpCode::MatrixTimesMatrix => ffi::matrix_times_matrix(&lhs, &rhs),
                BinaryOpCode::Div => ffi::div(&lhs, &rhs),
                BinaryOpCode::IMod => ffi::imod(&lhs, &rhs),
                BinaryOpCode::LogicalXor => ffi::logical_xor(&lhs, &rhs),
                BinaryOpCode::Equal => ffi::equal(&lhs, &rhs),
                BinaryOpCode::NotEqual => ffi::not_equal(&lhs, &rhs),
                BinaryOpCode::LessThan => ffi::less_than(&lhs, &rhs),
                BinaryOpCode::GreaterThan => ffi::greater_than(&lhs, &rhs),
                BinaryOpCode::LessThanEqual => ffi::less_than_equal(&lhs, &rhs),
                BinaryOpCode::GreaterThanEqual => ffi::greater_than_equal(&lhs, &rhs),
                BinaryOpCode::BitShiftLeft => ffi::bit_shift_left(&lhs, &rhs),
                BinaryOpCode::BitShiftRight => ffi::bit_shift_right(&lhs, &rhs),
                BinaryOpCode::BitwiseOr => ffi::bitwise_or(&lhs, &rhs),
                BinaryOpCode::BitwiseXor => ffi::bitwise_xor(&lhs, &rhs),
                BinaryOpCode::BitwiseAnd => ffi::bitwise_and(&lhs, &rhs),
                BinaryOpCode::Atan => ffi::built_in_atan_binary(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Pow => ffi::built_in_pow(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Mod => ffi::built_in_mod(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Min => ffi::built_in_min(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Max => ffi::built_in_max(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Step => ffi::built_in_step(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Modf => ffi::built_in_modf(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Frexp => ffi::built_in_frexp(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Ldexp => ffi::built_in_ldexp(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Distance => ffi::built_in_distance(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Dot => ffi::built_in_dot(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Cross => ffi::built_in_cross(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::Reflect => ffi::built_in_reflect(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::MatrixCompMult => {
                    ffi::built_in_matrixcompmult(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::OuterProduct => {
                    ffi::built_in_outerproduct(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::LessThanVec => {
                    ffi::built_in_lessthanvec(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::LessThanEqualVec => {
                    ffi::built_in_lessthanequalvec(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::GreaterThanVec => {
                    ffi::built_in_greaterthanvec(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::GreaterThanEqualVec => {
                    ffi::built_in_greaterthanequalvec(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::EqualVec => ffi::built_in_equalvec(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::NotEqualVec => {
                    ffi::built_in_notequalvec(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::InterpolateAtSample => {
                    ffi::built_in_interpolateatsample(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::InterpolateAtOffset => {
                    ffi::built_in_interpolateatoffset(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::AtomicAdd => {
                    ffi::built_in_atomicadd(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::AtomicMin => {
                    ffi::built_in_atomicmin(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::AtomicMax => {
                    ffi::built_in_atomicmax(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::AtomicAnd => {
                    ffi::built_in_atomicand(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::AtomicOr => ffi::built_in_atomicor(self.legacy_compiler, &lhs, &rhs),
                BinaryOpCode::AtomicXor => {
                    ffi::built_in_atomicxor(self.legacy_compiler, &lhs, &rhs)
                }
                BinaryOpCode::AtomicExchange => {
                    ffi::built_in_atomicexchange(self.legacy_compiler, &lhs, &rhs)
                }
            }
        };
        self.expressions.insert(result, expr);
    }

    fn built_in(
        &mut self,
        block_result: &mut *mut TIntermBlock,
        result: Option<RegisterId>,
        built_in_op: BuiltInOpCode,
        args: &Vec<TypedId>,
    ) {
        let args = args.iter().map(|&arg| self.get_expression(arg)).collect::<Vec<_>>();
        let (expr, statement) = unsafe {
            match built_in_op {
                BuiltInOpCode::Clamp => {
                    (Some(ffi::built_in_clamp(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::Mix => (Some(ffi::built_in_mix(self.legacy_compiler, &args)), None),
                BuiltInOpCode::Smoothstep => {
                    (Some(ffi::built_in_smoothstep(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::Fma => (Some(ffi::built_in_fma(self.legacy_compiler, &args)), None),
                BuiltInOpCode::Faceforward => {
                    (Some(ffi::built_in_faceforward(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::Refract => {
                    (Some(ffi::built_in_refract(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::BitfieldExtract => {
                    (Some(ffi::built_in_bitfieldextract(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::BitfieldInsert => {
                    (Some(ffi::built_in_bitfieldinsert(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::UaddCarry => {
                    (Some(ffi::built_in_uaddcarry(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::UsubBorrow => {
                    (Some(ffi::built_in_usubborrow(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::UmulExtended => {
                    (None, Some(ffi::built_in_umulextended(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::ImulExtended => {
                    (None, Some(ffi::built_in_imulextended(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::TextureSize => {
                    (Some(ffi::built_in_texturesize(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::TextureQueryLod => {
                    (Some(ffi::built_in_texturequerylod(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::TexelFetch => {
                    (Some(ffi::built_in_texelfetch(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::TexelFetchOffset => {
                    (Some(ffi::built_in_texelfetchoffset(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::Rgb2Yuv => {
                    (Some(ffi::built_in_rgb_2_yuv(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::Yuv2Rgb => {
                    (Some(ffi::built_in_yuv_2_rgb(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::AtomicCompSwap => {
                    (Some(ffi::built_in_atomiccompswap(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageStore => {
                    (None, Some(ffi::built_in_imagestore(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::ImageLoad => {
                    (Some(ffi::built_in_imageload(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicAdd => {
                    (Some(ffi::built_in_imageatomicadd(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicMin => {
                    (Some(ffi::built_in_imageatomicmin(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicMax => {
                    (Some(ffi::built_in_imageatomicmax(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicAnd => {
                    (Some(ffi::built_in_imageatomicand(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicOr => {
                    (Some(ffi::built_in_imageatomicor(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicXor => {
                    (Some(ffi::built_in_imageatomicxor(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicExchange => {
                    (Some(ffi::built_in_imageatomicexchange(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::ImageAtomicCompSwap => {
                    (Some(ffi::built_in_imageatomiccompswap(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::PixelLocalStoreANGLE => {
                    (None, Some(ffi::built_in_pixellocalstore(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::MemoryBarrier => {
                    (None, Some(ffi::built_in_memorybarrier(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::MemoryBarrierAtomicCounter => (
                    None,
                    Some(ffi::built_in_memorybarrieratomiccounter(self.legacy_compiler, &args)),
                ),
                BuiltInOpCode::MemoryBarrierBuffer => {
                    (None, Some(ffi::built_in_memorybarrierbuffer(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::MemoryBarrierImage => {
                    (None, Some(ffi::built_in_memorybarrierimage(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::Barrier => {
                    (None, Some(ffi::built_in_barrier(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::MemoryBarrierShared => {
                    (None, Some(ffi::built_in_memorybarriershared(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::GroupMemoryBarrier => {
                    (None, Some(ffi::built_in_groupmemorybarrier(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::EmitVertex => {
                    (None, Some(ffi::built_in_emitvertex(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::EndPrimitive => {
                    (None, Some(ffi::built_in_endprimitive(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::SubpassLoad => {
                    (Some(ffi::built_in_subpassload(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::BeginInvocationInterlockNV => (
                    None,
                    Some(ffi::built_in_begininvocationinterlocknv(self.legacy_compiler, &args)),
                ),
                BuiltInOpCode::EndInvocationInterlockNV => (
                    None,
                    Some(ffi::built_in_endinvocationinterlocknv(self.legacy_compiler, &args)),
                ),
                BuiltInOpCode::BeginFragmentShaderOrderingINTEL => (
                    None,
                    Some(ffi::built_in_beginfragmentshaderorderingintel(
                        self.legacy_compiler,
                        &args,
                    )),
                ),
                BuiltInOpCode::BeginInvocationInterlockARB => (
                    None,
                    Some(ffi::built_in_begininvocationinterlockarb(self.legacy_compiler, &args)),
                ),
                BuiltInOpCode::EndInvocationInterlockARB => (
                    None,
                    Some(ffi::built_in_endinvocationinterlockarb(self.legacy_compiler, &args)),
                ),
                BuiltInOpCode::NumSamples => {
                    (Some(ffi::built_in_numsamples(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::SamplePosition => {
                    (Some(ffi::built_in_sampleposition(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::InterpolateAtCenter => {
                    (Some(ffi::built_in_interpolateatcenter(self.legacy_compiler, &args)), None)
                }
                BuiltInOpCode::LoopForwardProgress => {
                    (None, Some(ffi::built_in_loopforwardprogress(self.legacy_compiler, &args)))
                }
                BuiltInOpCode::Saturate => {
                    (Some(ffi::built_in_saturate(self.legacy_compiler, &args)), None)
                }
            }
        };
        if let Some(result) = result {
            self.expressions.insert(result, expr.unwrap());
        } else {
            unsafe { ffi::append_instructions_to_block(*block_result, &[statement.unwrap()]) };
        }
    }

    fn texture(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        offset: Option<TypedId>,
    ) {
        let sampler_type = Self::get_sampler_type(ir_meta, sampler);
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);

        let expr = if let Some(offset) = offset {
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_textureoffset(
                    self.legacy_compiler,
                    &[sampler, coord, offset],
                    is_proj,
                )
            }
        } else {
            unsafe {
                ffi::built_in_texture(
                    self.legacy_compiler,
                    &[sampler, coord],
                    sampler_type,
                    is_proj,
                )
            }
        };
        self.expressions.insert(result, expr);
    }

    fn texture_compare(
        &mut self,
        _ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        compare: TypedId,
    ) {
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let compare = self.get_expression(compare);

        let expr = unsafe {
            ffi::built_in_texture_with_compare(self.legacy_compiler, &[sampler, coord, compare])
        };
        self.expressions.insert(result, expr);
    }

    fn texture_lod(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        lod: TypedId,
        offset: Option<TypedId>,
    ) {
        let sampler_type = Self::get_sampler_type(ir_meta, sampler);
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let lod = self.get_expression(lod);

        let expr = if let Some(offset) = offset {
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_texturelodoffset(
                    self.legacy_compiler,
                    &[sampler, coord, lod, offset],
                    is_proj,
                )
            }
        } else {
            unsafe {
                ffi::built_in_texturelod(
                    self.legacy_compiler,
                    &[sampler, coord, lod],
                    sampler_type,
                    is_proj,
                )
            }
        };
        self.expressions.insert(result, expr);
    }

    fn texture_compare_lod(
        &mut self,
        _ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        compare: TypedId,
        lod: TypedId,
    ) {
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let compare = self.get_expression(compare);
        let lod = self.get_expression(lod);

        let expr = unsafe {
            ffi::built_in_texturelod_with_compare(
                self.legacy_compiler,
                &[sampler, coord, compare, lod],
            )
        };
        self.expressions.insert(result, expr);
    }

    fn texture_bias(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        bias: TypedId,
        offset: Option<TypedId>,
    ) {
        let sampler_type = Self::get_sampler_type(ir_meta, sampler);
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let bias = self.get_expression(bias);

        let expr = if let Some(offset) = offset {
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_textureoffset(
                    self.legacy_compiler,
                    &[sampler, coord, offset, bias],
                    is_proj,
                )
            }
        } else {
            unsafe {
                ffi::built_in_texture(
                    self.legacy_compiler,
                    &[sampler, coord, bias],
                    sampler_type,
                    is_proj,
                )
            }
        };
        self.expressions.insert(result, expr);
    }

    fn texture_compare_bias(
        &mut self,
        _ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        compare: TypedId,
        bias: TypedId,
    ) {
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let compare = self.get_expression(compare);
        let bias = self.get_expression(bias);

        let expr = unsafe {
            ffi::built_in_texture_with_compare(
                self.legacy_compiler,
                &[sampler, coord, compare, bias],
            )
        };
        self.expressions.insert(result, expr);
    }

    fn texture_grad(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        is_proj: bool,
        dx: TypedId,
        dy: TypedId,
        offset: Option<TypedId>,
    ) {
        let sampler_type = Self::get_sampler_type(ir_meta, sampler);
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let dx = self.get_expression(dx);
        let dy = self.get_expression(dy);

        let expr = if let Some(offset) = offset {
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_texturegradoffset(
                    self.legacy_compiler,
                    &[sampler, coord, dx, dy, offset],
                    is_proj,
                )
            }
        } else {
            unsafe {
                ffi::built_in_texturegrad(
                    self.legacy_compiler,
                    &[sampler, coord, dx, dy],
                    sampler_type,
                    is_proj,
                )
            }
        };
        self.expressions.insert(result, expr);
    }

    fn texture_gather(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        offset: Option<TypedId>,
    ) {
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);

        let expr = if let Some(offset) = offset {
            let is_offset_array = ir_meta.get_type(offset.type_id).is_array();
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_texturegatheroffset(
                    self.legacy_compiler,
                    &[sampler, coord, offset],
                    is_offset_array,
                )
            }
        } else {
            unsafe { ffi::built_in_texturegather(self.legacy_compiler, &[sampler, coord]) }
        };
        self.expressions.insert(result, expr);
    }

    fn texture_gather_component(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        component: TypedId,
        offset: Option<TypedId>,
    ) {
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let component = self.get_expression(component);

        let expr = if let Some(offset) = offset {
            let is_offset_array = ir_meta.get_type(offset.type_id).is_array();
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_texturegatheroffset(
                    self.legacy_compiler,
                    &[sampler, coord, offset, component],
                    is_offset_array,
                )
            }
        } else {
            unsafe {
                ffi::built_in_texturegather(self.legacy_compiler, &[sampler, coord, component])
            }
        };
        self.expressions.insert(result, expr);
    }

    fn texture_gather_ref(
        &mut self,
        ir_meta: &IRMeta,
        _block_result: &mut *mut TIntermBlock,
        result: RegisterId,
        sampler: TypedId,
        coord: TypedId,
        refz: TypedId,
        offset: Option<TypedId>,
    ) {
        let sampler = self.get_expression(sampler);
        let coord = self.get_expression(coord);
        let refz = self.get_expression(refz);

        let expr = if let Some(offset) = offset {
            let is_offset_array = ir_meta.get_type(offset.type_id).is_array();
            let offset = self.get_expression(offset);
            unsafe {
                ffi::built_in_texturegatheroffset(
                    self.legacy_compiler,
                    &[sampler, coord, refz, offset],
                    is_offset_array,
                )
            }
        } else {
            unsafe { ffi::built_in_texturegather(self.legacy_compiler, &[sampler, coord, refz]) }
        };
        self.expressions.insert(result, expr);
    }

    fn branch_discard(&mut self, block: &mut *mut TIntermBlock) {
        unsafe { ffi::branch_discard(*block) };
    }
    fn branch_return(&mut self, block: &mut *mut TIntermBlock, value: Option<TypedId>) {
        if let Some(id) = value {
            unsafe { ffi::branch_return_value(*block, &self.get_expression(id)) };
        } else {
            unsafe { ffi::branch_return(*block) };
        }
    }
    fn branch_break(&mut self, block: &mut *mut TIntermBlock) {
        unsafe { ffi::branch_break(*block) };
    }
    fn branch_continue(&mut self, block: &mut *mut TIntermBlock) {
        unsafe { ffi::branch_continue(*block) };
    }
    fn branch_if(
        &mut self,
        block: &mut *mut TIntermBlock,
        condition: TypedId,
        true_block: Option<*mut TIntermBlock>,
        false_block: Option<*mut TIntermBlock>,
    ) {
        // The true block should always be present.
        let true_block = true_block.unwrap();
        let condition = self.get_expression(condition);
        if let Some(false_block) = false_block {
            unsafe { ffi::branch_if_else(*block, &condition, true_block, false_block) };
        } else {
            unsafe { ffi::branch_if(*block, &condition, true_block) };
        }
    }
    fn branch_loop(
        &mut self,
        block: &mut *mut TIntermBlock,
        loop_condition_block: Option<*mut TIntermBlock>,
        body_block: Option<*mut TIntermBlock>,
    ) {
        // The condition and body blocks should always be present.
        unsafe { ffi::branch_loop(*block, loop_condition_block.unwrap(), body_block.unwrap()) };
    }
    fn branch_do_loop(
        &mut self,
        block: &mut *mut TIntermBlock,
        body_block: Option<*mut TIntermBlock>,
    ) {
        // The condition and body blocks should always be present.  The difference between
        // DoLoop and Loop is effectively that the condition block is evaluated after the body
        // instead of before.
        unsafe { ffi::branch_do_loop(*block, body_block.unwrap()) };
    }
    fn branch_loop_if(&mut self, block: &mut *mut TIntermBlock, condition: TypedId) {
        // The condition block of a loop ends in `if (!condition) break;`
        unsafe { ffi::branch_loop_if(*block, &self.get_expression(condition)) };
    }
    fn branch_switch(
        &mut self,
        block: &mut *mut TIntermBlock,
        value: TypedId,
        case_ids: &Vec<Option<ConstantId>>,
        case_blocks: Vec<*mut TIntermBlock>,
    ) {
        let value = self.get_expression(value);
        let case_labels = case_ids
            .iter()
            .map(|id| id.map(|id| self.constants[&id]).unwrap_or(std::ptr::null_mut()))
            .collect::<Vec<_>>();

        unsafe { ffi::branch_switch(*block, &value, &case_labels, &case_blocks) };
    }

    // Take the current AST and place it as the body of the given function.
    fn end_function(&mut self, block_result: *mut TIntermBlock, id: FunctionId) {
        let declaration = unsafe { ffi::declare_function(self.functions[&id], block_result) };
        self.function_declarations.push(declaration);
    }
}
