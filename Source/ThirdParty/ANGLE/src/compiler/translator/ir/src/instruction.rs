// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions to create instructions given parameters.  The result type and precision is
// automatically deduced, and const-folding is done if possible.

use crate::ir::*;
use crate::*;

// Helper functions that perform constant folding per instruction
mod const_fold {
    use crate::ir::*;

    fn apply_unary_componentwise<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32) -> f32 + Copy,
        IntOp: Fn(i32) -> i32 + Copy,
        UintOp: Fn(u32) -> u32 + Copy,
        BoolOp: Fn(bool) -> bool + Copy,
    {
        let constant = ir_meta.get_constant(constant_id);
        debug_assert!(constant.type_id == result_type_id);

        match &constant.value {
            &ConstantValue::Float(f) => ir_meta.get_constant_float(float_op(f)),
            &ConstantValue::Int(i) => ir_meta.get_constant_int(int_op(i)),
            &ConstantValue::Uint(u) => ir_meta.get_constant_uint(uint_op(u)),
            &ConstantValue::Bool(b) => ir_meta.get_constant_bool(bool_op(b)),
            ConstantValue::YuvCsc(_) => {
                panic!("Internal error: Ops not allowed on YUV CSC constants")
            }
            ConstantValue::Composite(components) => {
                let type_id = constant.type_id;
                let element_type_id = ir_meta.get_type(type_id).get_element_type_id().unwrap();

                let mapped = components
                    .clone()
                    .iter()
                    .map(|&component_id| {
                        apply_unary_componentwise(
                            ir_meta,
                            component_id,
                            element_type_id,
                            float_op,
                            int_op,
                            uint_op,
                            bool_op,
                        )
                    })
                    .collect();
                ir_meta.get_constant_composite(result_type_id, mapped)
            }
        }
    }

    fn apply_binary_to_scalars<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32, f32) -> f32 + Copy,
        IntOp: Fn(i32, i32) -> i32 + Copy,
        UintOp: Fn(u32, u32) -> u32 + Copy,
        BoolOp: Fn(bool, bool) -> bool + Copy,
    {
        let lhs = ir_meta.get_constant(lhs_constant_id);
        let rhs = ir_meta.get_constant(rhs_constant_id);

        match (&lhs.value, &rhs.value) {
            (&ConstantValue::Float(f1), &ConstantValue::Float(f2)) => {
                ir_meta.get_constant_float(float_op(f1, f2))
            }
            (&ConstantValue::Int(i1), &ConstantValue::Int(i2)) => {
                ir_meta.get_constant_int(int_op(i1, i2))
            }
            (&ConstantValue::Uint(u1), &ConstantValue::Uint(u2)) => {
                ir_meta.get_constant_uint(uint_op(u1, u2))
            }
            (&ConstantValue::Bool(b1), &ConstantValue::Bool(b2)) => {
                ir_meta.get_constant_bool(bool_op(b1, b2))
            }
            (&ConstantValue::YuvCsc(_), &ConstantValue::YuvCsc(_)) => {
                panic!("Internal error: Ops not allowed on YUV CSC constants")
            }
            _ => panic!("Internal error: Expected scalars when constant folding a binary op"),
        }
    }

    fn apply_binary_componentwise_scalar_rhs<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32, f32) -> f32 + Copy,
        IntOp: Fn(i32, i32) -> i32 + Copy,
        UintOp: Fn(u32, u32) -> u32 + Copy,
        BoolOp: Fn(bool, bool) -> bool + Copy,
    {
        let lhs = ir_meta.get_constant(lhs_constant_id);

        if let ConstantValue::Composite(lhs_components) = &lhs.value {
            let result_element_type_id =
                ir_meta.get_type(result_type_id).get_element_type_id().unwrap_or(result_type_id);

            let components = lhs_components
                .clone()
                .iter()
                .map(|&lhs_component_id| {
                    apply_binary_componentwise_scalar_rhs(
                        ir_meta,
                        lhs_component_id,
                        rhs_constant_id,
                        result_element_type_id,
                        float_op,
                        int_op,
                        uint_op,
                        bool_op,
                    )
                })
                .collect();
            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            apply_binary_to_scalars(
                ir_meta,
                lhs_constant_id,
                rhs_constant_id,
                result_type_id,
                float_op,
                int_op,
                uint_op,
                bool_op,
            )
        }
    }

    fn apply_binary_componentwise_scalar_lhs<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32, f32) -> f32 + Copy,
        IntOp: Fn(i32, i32) -> i32 + Copy,
        UintOp: Fn(u32, u32) -> u32 + Copy,
        BoolOp: Fn(bool, bool) -> bool + Copy,
    {
        let rhs = ir_meta.get_constant(rhs_constant_id);

        if let ConstantValue::Composite(rhs_components) = &rhs.value {
            let result_element_type_id =
                ir_meta.get_type(result_type_id).get_element_type_id().unwrap_or(result_type_id);

            let components = rhs_components
                .clone()
                .iter()
                .map(|&rhs_component_id| {
                    apply_binary_componentwise_scalar_lhs(
                        ir_meta,
                        lhs_constant_id,
                        rhs_component_id,
                        result_element_type_id,
                        float_op,
                        int_op,
                        uint_op,
                        bool_op,
                    )
                })
                .collect();
            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            apply_binary_to_scalars(
                ir_meta,
                lhs_constant_id,
                rhs_constant_id,
                result_type_id,
                float_op,
                int_op,
                uint_op,
                bool_op,
            )
        }
    }

    fn apply_binary_componentwise_non_scalar<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32, f32) -> f32 + Copy,
        IntOp: Fn(i32, i32) -> i32 + Copy,
        UintOp: Fn(u32, u32) -> u32 + Copy,
        BoolOp: Fn(bool, bool) -> bool + Copy,
    {
        let lhs = ir_meta.get_constant(lhs_constant_id);
        let rhs = ir_meta.get_constant(rhs_constant_id);

        if let (
            ConstantValue::Composite(lhs_components),
            ConstantValue::Composite(rhs_components),
        ) = (&lhs.value, &rhs.value)
        {
            let result_element_type_id =
                ir_meta.get_type(result_type_id).get_element_type_id().unwrap_or(result_type_id);

            let components = lhs_components
                .clone()
                .iter()
                .zip(rhs_components.clone().iter())
                .map(|(&lhs_component_id, &rhs_component_id)| {
                    apply_binary_componentwise_non_scalar(
                        ir_meta,
                        lhs_component_id,
                        rhs_component_id,
                        result_element_type_id,
                        float_op,
                        int_op,
                        uint_op,
                        bool_op,
                    )
                })
                .collect();
            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            apply_binary_to_scalars(
                ir_meta,
                lhs_constant_id,
                rhs_constant_id,
                result_type_id,
                float_op,
                int_op,
                uint_op,
                bool_op,
            )
        }
    }

    fn apply_binary_componentwise<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32, f32) -> f32 + Copy,
        IntOp: Fn(i32, i32) -> i32 + Copy,
        UintOp: Fn(u32, u32) -> u32 + Copy,
        BoolOp: Fn(bool, bool) -> bool + Copy,
    {
        let is_lhs_scalar = !ir_meta.get_constant(lhs_constant_id).value.is_composite();
        let is_rhs_scalar = !ir_meta.get_constant(rhs_constant_id).value.is_composite();

        if is_rhs_scalar {
            apply_binary_componentwise_scalar_rhs(
                ir_meta,
                lhs_constant_id,
                rhs_constant_id,
                result_type_id,
                float_op,
                int_op,
                uint_op,
                bool_op,
            )
        } else if is_lhs_scalar {
            apply_binary_componentwise_scalar_lhs(
                ir_meta,
                lhs_constant_id,
                rhs_constant_id,
                result_type_id,
                float_op,
                int_op,
                uint_op,
                bool_op,
            )
        } else {
            apply_binary_componentwise_non_scalar(
                ir_meta,
                lhs_constant_id,
                rhs_constant_id,
                result_type_id,
                float_op,
                int_op,
                uint_op,
                bool_op,
            )
        }
    }

    pub fn composite_element(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        index: u32,
        _result_type_id: TypeId,
    ) -> ConstantId {
        ir_meta.get_constant(constant_id).value.get_composite_elements()[index as usize]
    }

    pub fn vector_component_multi(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        fields: &[u32],
        result_type_id: TypeId,
    ) -> ConstantId {
        let composite_elements = ir_meta.get_constant(constant_id).value.get_composite_elements();
        let new_elements = fields.iter().map(|&field| composite_elements[field as usize]).collect();
        ir_meta.get_constant_composite(result_type_id, new_elements)
    }

    pub fn index(
        ir_meta: &mut IRMeta,
        indexed_constant_id: ConstantId,
        index_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let index = ir_meta.get_constant(index_constant_id).value.get_index();
        composite_element(ir_meta, indexed_constant_id, index, result_type_id)
    }

    // Taking a list of constants, flattens them so every element is a basic type.  For example,
    // [v.xy, m2x3, ...] becomes [v.x, v.y, m[0][0], m[0][1], ...]
    fn flatten_constructor_args<I>(
        ir_meta: &IRMeta,
        args: std::iter::Peekable<I>,
    ) -> Vec<ConstantId>
    where
        I: Iterator<Item = ConstantId>,
    {
        let mut result = Vec::new();

        for arg in args {
            let constant = ir_meta.get_constant(arg);

            if let ConstantValue::Composite(ids) = &constant.value {
                let flattened = flatten_constructor_args(ir_meta, ids.iter().copied().peekable());
                result.extend(flattened);
            } else {
                result.push(arg);
            }
        }

        result
    }

    // Taking a list of all-scalar constants, casts the values to the given basic_type.
    fn cast_constructor_args(
        ir_meta: &mut IRMeta,
        args: Vec<ConstantId>,
        basic_type: BasicType,
    ) -> Vec<ConstantId> {
        args.iter()
            .map(|&arg| {
                let constant = ir_meta.get_constant(arg);
                if let ConstantValue::Float(f) = constant.value {
                    match basic_type {
                        BasicType::Int => ir_meta.get_constant_int(f as i32),
                        // Note: ESSL 3.00.6 section 5.4.1. It is undefined to convert a negative
                        // floating-point value to an uint
                        BasicType::Uint => ir_meta.get_constant_uint(if f < 0.0 {
                            f as i32 as u32
                        } else {
                            f as u32
                        }),
                        BasicType::Bool => ir_meta.get_constant_bool(f != 0.0),
                        _ => arg,
                    }
                } else if let ConstantValue::Int(i) = constant.value {
                    match basic_type {
                        BasicType::Float => ir_meta.get_constant_float(i as f32),
                        BasicType::Uint => ir_meta.get_constant_uint(i as u32),
                        BasicType::Bool => ir_meta.get_constant_bool(i != 0),
                        _ => arg,
                    }
                } else if let ConstantValue::Uint(u) = constant.value {
                    match basic_type {
                        BasicType::Float => ir_meta.get_constant_float(u as f32),
                        BasicType::Int => ir_meta.get_constant_int(u as i32),
                        BasicType::Bool => ir_meta.get_constant_bool(u != 0),
                        _ => arg,
                    }
                } else if let ConstantValue::Bool(b) = constant.value {
                    match basic_type {
                        BasicType::Float => ir_meta.get_constant_float(b.into()),
                        BasicType::Int => ir_meta.get_constant_int(b as i32),
                        BasicType::Uint => ir_meta.get_constant_uint(b as u32),
                        _ => arg,
                    }
                } else {
                    // This function is called on scalars, so `Composite` is impossible.
                    // Additionally, GLSL forbids type conversion to and from
                    // yuvCscStandardEXT.
                    arg
                }
            })
            .collect()
    }

    // Construct a vector from a scalar by replicating it.
    fn construct_vector_from_scalar(
        ir_meta: &mut IRMeta,
        arg: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let type_info = ir_meta.get_type(result_type_id);
        let vec_size = type_info.get_vector_size().unwrap();
        let args = vec![arg; vec_size as usize];
        ir_meta.get_constant_composite(result_type_id, args)
    }

    // Construct a matrix from a scalar by setting the diagonal elements with that scalar while
    // everywhere else is filled with zeros.
    fn construct_matrix_from_scalar(
        ir_meta: &mut IRMeta,
        arg: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let type_info = ir_meta.get_type(result_type_id);
        let &Type::Matrix(vector_type_id, column_count) = type_info else { unreachable!() };
        let &Type::Vector(_, row_count) = ir_meta.get_type(vector_type_id) else { unreachable!() };

        // Create columns where every component is 0 except the one at index = column_index.
        let columns = (0..column_count)
            .map(|column| {
                let column_components = (0..row_count)
                    .map(|row| if row == column { arg } else { CONSTANT_ID_FLOAT_ZERO })
                    .collect();
                ir_meta.get_constant_composite(vector_type_id, column_components)
            })
            .collect();
        ir_meta.get_constant_composite(result_type_id, columns)
    }

    // Construct a matrix from a matrix by starting with the identity matrix and overwriting it with
    // components from the argument.
    fn construct_matrix_from_matrix(
        ir_meta: &mut IRMeta,
        arg: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let type_info = ir_meta.get_type(result_type_id);
        let &Type::Matrix(vector_type_id, column_count) = type_info else { unreachable!() };
        let &Type::Vector(_, row_count) = ir_meta.get_type(vector_type_id) else { unreachable!() };

        let input = ir_meta.get_constant(arg);
        let input_columns = input.value.get_composite_elements();
        let input_column_components: Vec<_> = input_columns
            .iter()
            .map(|&column_id| ir_meta.get_constant(column_id).value.get_composite_elements())
            .collect();
        let input_column_count = input_columns.len() as u32;
        let input_row_count = input_column_components[0].len() as u32;

        // Create columns where every component is taken from the input matrix, except if it's out
        // of bounds.  In that case, the component is 1 on the diagonal and 0 elsewhere.
        let columns: Vec<_> = (0..column_count)
            .map(|column| {
                (0..row_count)
                    .map(|row| {
                        if column < input_column_count && row < input_row_count {
                            input_column_components[column as usize][row as usize]
                        } else if row == column {
                            CONSTANT_ID_FLOAT_ONE
                        } else {
                            CONSTANT_ID_FLOAT_ZERO
                        }
                    })
                    .collect()
            })
            .collect();

        let columns = columns
            .into_iter()
            .map(|column_components| {
                ir_meta.get_constant_composite(vector_type_id, column_components)
            })
            .collect();
        ir_meta.get_constant_composite(result_type_id, columns)
    }

    // Construct a vector from multiple components.
    fn construct_vector_from_many(
        ir_meta: &mut IRMeta,
        args: Vec<ConstantId>,
        result_type_id: TypeId,
    ) -> ConstantId {
        ir_meta.get_constant_composite(result_type_id, args)
    }

    // Construct a matrix from multiple components.
    fn construct_matrix_from_many(
        ir_meta: &mut IRMeta,
        args: Vec<ConstantId>,
        result_type_id: TypeId,
    ) -> ConstantId {
        let type_info = ir_meta.get_type(result_type_id);
        let &Type::Matrix(vector_type_id, column_count) = type_info else { unreachable!() };
        let &Type::Vector(_, row_count) = ir_meta.get_type(vector_type_id) else { unreachable!() };

        let columns = (0..column_count)
            .map(|column| {
                let start = (column * row_count) as usize;
                let end = start + row_count as usize;
                ir_meta.get_constant_composite(vector_type_id, args[start..end].to_vec())
            })
            .collect();
        ir_meta.get_constant_composite(result_type_id, columns)
    }

    pub fn construct(
        ir_meta: &mut IRMeta,
        args: &mut impl Iterator<Item = ConstantId>,
        result_type_id: TypeId,
    ) -> ConstantId {
        let type_info = ir_meta.get_type(result_type_id);

        // For arrays and struct, simply make the constant out of the arguments.
        if type_info.is_struct() || type_info.is_array() {
            return ir_meta.get_constant_composite(result_type_id, args.collect());
        }

        let is_vector = type_info.is_vector();
        let is_matrix = type_info.is_matrix();
        let mut args = args.peekable();
        let first_arg = *args.peek().unwrap();
        let is_first_arg_matrix =
            ir_meta.get_type(ir_meta.get_constant(first_arg).type_id).is_matrix();

        if is_matrix && is_first_arg_matrix {
            // Matrix-from-matrix construction is special with the way a sub-region of the argument
            // is picked out, so flatten_constructor_args` cannot be used.
            construct_matrix_from_matrix(ir_meta, first_arg, result_type_id)
        } else {
            // For scalars, vectors and matrices, first flatten the components for simplicity.
            let args = flatten_constructor_args(ir_meta, args);

            // Then cast the basic type to the type of the result, if needed.
            let basic_type =
                ir_meta.get_type(ir_meta.get_scalar_type(result_type_id)).get_scalar_basic_type();
            let args = cast_constructor_args(ir_meta, args, basic_type);

            if is_vector && args.len() == 1 {
                construct_vector_from_scalar(ir_meta, args[0], result_type_id)
            } else if is_matrix && args.len() == 1 {
                construct_matrix_from_scalar(ir_meta, args[0], result_type_id)
            } else if is_vector {
                construct_vector_from_many(ir_meta, args, result_type_id)
            } else if is_matrix {
                construct_matrix_from_many(ir_meta, args, result_type_id)
            } else {
                // The type cast is enough to satisfy scalar constructors.
                args[0]
            }
        }
    }

    pub fn negate(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_unary_componentwise(
            ir_meta,
            constant_id,
            result_type_id,
            |f| -f,
            |i| i.wrapping_neg(),
            |u| u.wrapping_neg(),
            |_| panic!("Internal error: Cannot negate a bool"),
        )
    }
    pub fn add(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 + f2,
            |i1, i2| i1.wrapping_add(i2),
            |u1, u2| u1.wrapping_add(u2),
            |_, _| panic!("Internal error: Cannot add bools"),
        )
    }
    pub fn sub(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 - f2,
            |i1, i2| i1.wrapping_sub(i2),
            |u1, u2| u1.wrapping_sub(u2),
            |_, _| panic!("Internal error: Cannot subtract bools"),
        )
    }
    pub fn mul(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 * f2,
            |i1, i2| i1.wrapping_mul(i2),
            |u1, u2| u1.wrapping_mul(u2),
            |_, _| panic!("Internal error: Cannot multiply bools"),
        )
    }
    pub fn div(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 / f2,
            |i1, i2| i1.checked_div(i2).unwrap_or(i32::MAX),
            |u1, u2| u1.checked_div(u2).unwrap_or(u32::MAX),
            |_, _| panic!("Internal error: Cannot divide bools"),
        )
    }
    pub fn imod(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |_, _| panic!("Internal error: Cannot use % to calculate remainder of floats"),
            // ESSL 3.00.6 section 5.9: Results of modulus are undefined when either one of the
            // operands is negative.
            |i1, i2| if i1 < 0 || i2 < 0 { 0 } else { i1.checked_rem(i2).unwrap_or(0) },
            |u1, u2| u1.checked_rem(u2).unwrap_or(0),
            |_, _| panic!("Internal error: Cannot use % on bools"),
        )
    }
    pub fn dot(ir_meta: &IRMeta, lhs_constant_id: ConstantId, rhs_constant_id: ConstantId) -> f32 {
        let lhs = ir_meta.get_constant(lhs_constant_id);
        let rhs = ir_meta.get_constant(rhs_constant_id);

        if !lhs.value.is_composite() {
            return lhs.value.get_float() * rhs.value.get_float();
        }

        let lhs_components = lhs.value.get_composite_elements();
        let rhs_components = rhs.value.get_composite_elements();

        lhs_components
            .iter()
            .zip(rhs_components.iter())
            .map(|(&lhs_component_id, &rhs_component_id)| {
                ir_meta.get_constant(lhs_component_id).value.get_float()
                    * ir_meta.get_constant(rhs_component_id).value.get_float()
            })
            .sum()
    }
    pub fn transpose(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let constant = ir_meta.get_constant(constant_id);
        let columns = constant.value.get_composite_elements();
        let column_count = columns.len();
        let row_count = ir_meta.get_type(result_type_id).get_matrix_size().unwrap();

        let mut transposed_data = vec![Vec::new(); row_count as usize];

        // Create new columns out of rows of the matrix.
        columns.iter().for_each(|&column_id| {
            let column = ir_meta.get_constant(column_id).value.get_composite_elements();
            debug_assert!(column.len() == row_count as usize);
            column.iter().enumerate().for_each(|(row_index, &component)| {
                transposed_data[row_index].push(component);
            });
        });

        // Create constants for these columns.
        let transposed_column_type_id =
            ir_meta.get_vector_type_id(BasicType::Float, column_count as u32);
        let transposed_column_ids = transposed_data
            .into_iter()
            .map(|data| ir_meta.get_constant_composite(transposed_column_type_id, data))
            .collect();

        // Finally, assemble the matrix itself.
        ir_meta.get_constant_composite(result_type_id, transposed_column_ids)
    }
    pub fn vector_times_matrix(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let rhs = ir_meta.get_constant(rhs_constant_id);
        let rhs_columns = rhs.value.get_composite_elements();

        let result_column_ids = rhs_columns
            .clone()
            .iter()
            .map(|&column_id| ir_meta.get_constant_float(dot(ir_meta, lhs_constant_id, column_id)))
            .collect();
        ir_meta.get_constant_composite(result_type_id, result_column_ids)
    }
    pub fn matrix_times_vector(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let lhs = ir_meta.get_constant(lhs_constant_id);
        let matrix_type = ir_meta.get_type(lhs.type_id);
        let column_count = matrix_type.get_matrix_size().unwrap();
        let row_count = ir_meta.get_type(result_type_id).get_vector_size().unwrap();

        // Transpose the matrix first, so `vector_times_matrix` can be reused.
        let transposed_type_id = ir_meta.get_matrix_type_id(row_count, column_count);
        let transposed = transpose(ir_meta, lhs_constant_id, transposed_type_id);
        vector_times_matrix(ir_meta, rhs_constant_id, transposed, result_type_id)
    }
    pub fn matrix_times_matrix(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let lhs = ir_meta.get_constant(lhs_constant_id);
        let rhs = ir_meta.get_constant(rhs_constant_id);
        let lhs_type = ir_meta.get_type(lhs.type_id);
        let result_type = ir_meta.get_type(result_type_id);

        let lhs_columns = lhs.value.get_composite_elements();
        let rhs_columns = rhs.value.get_composite_elements().clone();

        let result_column_type_id = result_type.get_element_type_id().unwrap();

        // Say LHS has C1 columns of R1 components, and RHS has C2 columns of R2 components.  For
        // matrix times matrix to work, C1 == R2 must hold.  The result will have C2 columns of R1
        // components.
        let rhs_column_count = rhs_columns.len();
        let common_component_count = lhs_columns.len();
        let lhs_row_count = ir_meta.get_type(result_column_type_id).get_vector_size().unwrap();

        debug_assert!(
            ir_meta.get_type(lhs_type.get_element_type_id().unwrap()).get_vector_size().unwrap()
                == lhs_row_count
        );
        debug_assert!(result_type.get_matrix_size().unwrap() == rhs_column_count as u32);

        // The result at column c, row r, is the dot product of row r in lhs and column c in rhs.
        // Equivalently, it's the dot product of column r in transposed lhs and column c in rhs.
        // The lhs is thus transposed so the `dot` helper can be used.
        let lhs_transposed_type_id =
            ir_meta.get_matrix_type_id(lhs_row_count, common_component_count as u32);
        let lhs_transposed_id = transpose(ir_meta, lhs_constant_id, lhs_transposed_type_id);

        let lhs = ir_meta.get_constant(lhs_transposed_id);
        let lhs_columns = lhs.value.get_composite_elements().clone();

        let result_columns_ids = rhs_columns
            .iter()
            .map(|&rhs_column_id| {
                let result_column_ids = lhs_columns
                    .iter()
                    .map(|&lhs_column_id| {
                        ir_meta.get_constant_float(dot(ir_meta, lhs_column_id, rhs_column_id))
                    })
                    .collect();
                ir_meta.get_constant_composite(result_column_type_id, result_column_ids)
            })
            .collect();
        ir_meta.get_constant_composite(result_type_id, result_columns_ids)
    }
    pub fn logical_not(
        _ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        debug_assert!(constant_id == CONSTANT_ID_TRUE || constant_id == CONSTANT_ID_FALSE);
        if constant_id == CONSTANT_ID_FALSE { CONSTANT_ID_TRUE } else { CONSTANT_ID_FALSE }
    }
    pub fn logical_xor(
        _ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        debug_assert!(lhs_constant_id == CONSTANT_ID_TRUE || lhs_constant_id == CONSTANT_ID_FALSE);
        debug_assert!(rhs_constant_id == CONSTANT_ID_TRUE || rhs_constant_id == CONSTANT_ID_FALSE);

        if lhs_constant_id == rhs_constant_id { CONSTANT_ID_FALSE } else { CONSTANT_ID_TRUE }
    }
    pub fn equal(
        _ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        // Note: because the IR ensures constants are unique, no need for a recursive equality
        // check of the components.
        if lhs_constant_id == rhs_constant_id { CONSTANT_ID_TRUE } else { CONSTANT_ID_FALSE }
    }
    pub fn not_equal(
        _ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        if lhs_constant_id == rhs_constant_id { CONSTANT_ID_FALSE } else { CONSTANT_ID_TRUE }
    }
    fn compare_scalars<FloatCompare, IntCompare, UintCompare>(
        ir_meta: &IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        float_compare: FloatCompare,
        int_compare: IntCompare,
        uint_compare: UintCompare,
    ) -> ConstantId
    where
        FloatCompare: FnOnce(f32, f32) -> bool,
        IntCompare: FnOnce(i32, i32) -> bool,
        UintCompare: FnOnce(u32, u32) -> bool,
    {
        let lhs_constant = ir_meta.get_constant(lhs_constant_id);
        let rhs_constant = ir_meta.get_constant(rhs_constant_id);

        let result = match (&lhs_constant.value, &rhs_constant.value) {
            (&ConstantValue::Float(lhs_f), &ConstantValue::Float(rhs_f)) => {
                float_compare(lhs_f, rhs_f)
            }
            (&ConstantValue::Int(lhs_i), &ConstantValue::Int(rhs_i)) => int_compare(lhs_i, rhs_i),
            (&ConstantValue::Uint(lhs_u), &ConstantValue::Uint(rhs_u)) => {
                uint_compare(lhs_u, rhs_u)
            }
            _ => false,
        };

        if result { CONSTANT_ID_TRUE } else { CONSTANT_ID_FALSE }
    }
    pub fn less_than(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        compare_scalars(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            |f1, f2| f1 < f2,
            |i1, i2| i1 < i2,
            |u1, u2| u1 < u2,
        )
    }
    pub fn greater_than(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        compare_scalars(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            |f1, f2| f1 > f2,
            |i1, i2| i1 > i2,
            |u1, u2| u1 > u2,
        )
    }
    pub fn less_than_equal(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        compare_scalars(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            |f1, f2| f1 <= f2,
            |i1, i2| i1 <= i2,
            |u1, u2| u1 <= u2,
        )
    }
    pub fn greater_than_equal(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        compare_scalars(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            |f1, f2| f1 >= f2,
            |i1, i2| i1 >= i2,
            |u1, u2| u1 >= u2,
        )
    }
    pub fn bitwise_not(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_unary_componentwise(
            ir_meta,
            constant_id,
            result_type_id,
            |_| panic!("Internal error: Cannot bitwise-not a float"),
            |i| !i,
            |u| !u,
            |_| panic!("Internal error: Cannot bitwise-not a bool"),
        )
    }
    pub fn bit_shift_left(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        // GLSL allows the operands of shift to differ in signedness.  Cast the rhs to the type of
        // lhs before calling the helper.  This can be conveniently done with `construct`, which
        // may turn a scalar rhs into a vector.  This is ok because the operation is component-wise
        // anyway.
        let lhs_type_id = ir_meta.get_constant(lhs_constant_id).type_id;
        let rhs_constant_id =
            construct(ir_meta, &mut std::iter::once(rhs_constant_id), lhs_type_id);

        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |_, _| panic!("Internal error: Cannot use << on floats"),
            |i1, i2| i1.checked_shl(i2 as u32).unwrap_or(0),
            |u1, u2| u1.checked_shl(u2).unwrap_or(0),
            |_, _| panic!("Internal error: Cannot use << on bools"),
        )
    }
    pub fn bit_shift_right(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        // See bit_shift_left
        let lhs_type_id = ir_meta.get_constant(lhs_constant_id).type_id;
        let rhs_constant_id =
            construct(ir_meta, &mut std::iter::once(rhs_constant_id), lhs_type_id);

        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |_, _| panic!("Internal error: Cannot use >> on floats"),
            |i1, i2| i1.checked_shr(i2 as u32).unwrap_or(0),
            |u1, u2| u1.checked_shr(u2).unwrap_or(0),
            |_, _| panic!("Internal error: Cannot use >> on bools"),
        )
    }
    pub fn bitwise_or(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |_, _| panic!("Internal error: Cannot use | on floats"),
            |i1, i2| i1 | i2,
            |u1, u2| u1 | u2,
            |_, _| panic!("Internal error: Cannot use | on bools"),
        )
    }
    pub fn bitwise_xor(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |_, _| panic!("Internal error: Cannot use ^ on floats"),
            |i1, i2| i1 ^ i2,
            |u1, u2| u1 ^ u2,
            |_, _| panic!("Internal error: Cannot use ^ on bools"),
        )
    }
    pub fn bitwise_and(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |_, _| panic!("Internal error: Cannot use & on floats"),
            |i1, i2| i1 & i2,
            |u1, u2| u1 & u2,
            |_, _| panic!("Internal error: Cannot use & on bools"),
        )
    }
    // Helpers to fold unary built-ins that do not apply the operation component-wise or don't
    // match the common patterns.
    fn bitcast_helper<Op>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        op: Op,
    ) -> ConstantId
    where
        Op: Fn(&mut IRMeta, &ConstantValue) -> ConstantId + Copy,
    {
        let constant = ir_meta.get_constant(constant_id);
        let element_type_id =
            ir_meta.get_type(result_type_id).get_element_type_id().unwrap_or(result_type_id);

        if let ConstantValue::Composite(components) = &constant.value {
            let mapped = components
                .clone()
                .iter()
                .map(|&component_id| bitcast_helper(ir_meta, component_id, element_type_id, op))
                .collect();
            ir_meta.get_constant_composite(result_type_id, mapped)
        } else {
            op(ir_meta, &constant.value.clone())
        }
    }
    fn built_in_floatbitstoint(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| {
            ir_meta.get_constant_int(value.get_float().to_bits() as i32)
        })
    }
    fn built_in_floatbitstouint(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| {
            ir_meta.get_constant_uint(value.get_float().to_bits())
        })
    }
    fn built_in_intbitstofloat(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| {
            ir_meta.get_constant_float(f32::from_bits(value.get_int() as u32))
        })
    }
    fn built_in_uintbitstofloat(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| {
            ir_meta.get_constant_float(f32::from_bits(value.get_uint()))
        })
    }
    fn built_in_bitcount(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| match *value {
            ConstantValue::Int(i) => ir_meta.get_constant_int(i.count_ones() as i32),
            ConstantValue::Uint(u) => ir_meta.get_constant_int(u.count_ones() as i32),
            _ => {
                panic!("Internal error: The bitCount() built-in only applies to [u]int types");
            }
        })
    }
    fn built_in_findlsb(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| match *value {
            ConstantValue::Int(i) => {
                ir_meta.get_constant_int(if i == 0 { -1 } else { i.trailing_zeros() as i32 })
            }
            ConstantValue::Uint(u) => {
                ir_meta.get_constant_int(if u == 0 { -1 } else { u.trailing_zeros() as i32 })
            }
            _ => {
                panic!("Internal error: The findLSB() built-in only applies to [u]int types");
            }
        })
    }
    fn built_in_findmsb(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        bitcast_helper(ir_meta, constant_id, result_type_id, |ir_meta, value| {
            match *value {
                ConstantValue::Int(i) => {
                    // Note: For negative numbers, look for zero instead of one in value. Using
                    // complement handles the intValue == -1 special case, where the return value
                    // needs to be -1.
                    println!("Got {i}");
                    let i = if i < 0 { !i } else { i };
                    println!("Maybe complement: {i}");
                    ir_meta.get_constant_int(if i == 0 {
                        -1
                    } else {
                        31 - i.leading_zeros() as i32
                    })
                }
                ConstantValue::Uint(u) => ir_meta.get_constant_int(if u == 0 {
                    -1
                } else {
                    31 - u.leading_zeros() as i32
                }),
                _ => {
                    panic!("Internal error: The findMSB() built-in only applies to [u]int types");
                }
            }
        })
    }
    fn built_in_any(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let constant = ir_meta.get_constant(constant_id);
        debug_assert!(result_type_id == TYPE_ID_BOOL);

        let any = constant.value.get_composite_elements().contains(&CONSTANT_ID_TRUE);
        ir_meta.get_constant_bool(any)
    }
    fn built_in_all(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let constant = ir_meta.get_constant(constant_id);
        debug_assert!(result_type_id == TYPE_ID_BOOL);

        let all = constant
            .value
            .get_composite_elements()
            .iter()
            .all(|&component_id| component_id == CONSTANT_ID_TRUE);
        ir_meta.get_constant_bool(all)
    }
    fn get_vector_length(ir_meta: &IRMeta, constant: &Constant) -> f32 {
        let sum: f32 = constant
            .value
            .get_composite_elements()
            .iter()
            .map(|&component_id| {
                let component = ir_meta.get_constant(component_id).value.get_float();
                component * component
            })
            .sum();
        sum.sqrt()
    }
    fn built_in_length(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        debug_assert!(result_type_id == TYPE_ID_FLOAT);

        let constant = ir_meta.get_constant(constant_id);
        if !constant.value.is_composite() {
            return constant_id;
        }

        let length = get_vector_length(ir_meta, constant);
        ir_meta.get_constant_float(length)
    }
    fn built_in_normalize(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let constant = ir_meta.get_constant(constant_id);
        debug_assert!(result_type_id == constant.type_id);

        if !constant.value.is_composite() {
            return CONSTANT_ID_FLOAT_ONE;
        }

        let length = get_vector_length(ir_meta, constant);

        let mapped = constant
            .value
            .get_composite_elements()
            .clone()
            .iter()
            .map(|&component_id| {
                let component = ir_meta.get_constant(component_id).value.get_float() / length;
                ir_meta.get_constant_float(component)
            })
            .collect();

        ir_meta.get_constant_composite(result_type_id, mapped)
    }
    fn built_in_transpose(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        transpose(ir_meta, constant_id, result_type_id)
    }
    fn determinant_2x2(m: &[&[f32]; 2]) -> f32 {
        m[0][0] * m[1][1] - m[0][1] * m[1][0]
    }
    fn determinant_3x3(m: &[&[f32]; 3]) -> f32 {
        let sub0 = [&m[1][1..], &m[2][1..]];
        let sub1 = [&m[0][1..], &m[2][1..]];
        let sub2 = [&m[0][1..], &m[1][1..]];

        m[0][0] * determinant_2x2(&sub0) - m[1][0] * determinant_2x2(&sub1)
            + m[2][0] * determinant_2x2(&sub2)
    }
    fn determinant_4x4(m: &[&[f32]; 4]) -> f32 {
        let sub0 = [&m[1][1..], &m[2][1..], &m[3][1..]];
        let sub1 = [&m[0][1..], &m[2][1..], &m[3][1..]];
        let sub2 = [&m[0][1..], &m[1][1..], &m[3][1..]];
        let sub3 = [&m[0][1..], &m[1][1..], &m[2][1..]];

        m[0][0] * determinant_3x3(&sub0) - m[1][0] * determinant_3x3(&sub1)
            + m[2][0] * determinant_3x3(&sub2)
            - m[3][0] * determinant_3x3(&sub3)
    }
    fn get_matrix_values(ir_meta: &IRMeta, constant_id: ConstantId) -> ([[f32; 4]; 4], usize) {
        let columns = ir_meta.get_constant(constant_id).value.get_composite_elements();
        let mut column_values = [[0.; 4]; 4];
        let column_count = columns.len();

        // Gather all constant values in the matrix first to simplify processing.
        columns.iter().enumerate().for_each(|(column_index, &column_id)| {
            let column = ir_meta.get_constant(column_id).value.get_composite_elements();
            // Note: only needed on square matrices in GLSL.
            debug_assert!(column.len() == column_count);
            column.iter().enumerate().for_each(|(row_index, &component)| {
                column_values[column_index][row_index] =
                    ir_meta.get_constant(component).value.get_float();
            });
        });

        (column_values, column_count)
    }
    fn determinant_helper(m: &[[f32; 4]; 4], size: usize) -> f32 {
        let m2x2: [&[f32]; 2] = [&m[0], &m[1]];
        let m3x3: [&[f32]; 3] = [&m[0], &m[1], &m[2]];
        let m4x4: [&[f32]; 4] = [&m[0], &m[1], &m[2], &m[3]];
        match size {
            2 => determinant_2x2(&m2x2),
            3 => determinant_3x3(&m3x3),
            4 => determinant_4x4(&m4x4),
            _ => panic!(
                "Internal error: Invalid matrix dimensions when calculating determinant/inverse"
            ),
        }
    }
    fn determinant(ir_meta: &IRMeta, constant_id: ConstantId) -> f32 {
        let (m, size) = get_matrix_values(ir_meta, constant_id);
        determinant_helper(&m, size)
    }
    fn built_in_determinant(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        _result_type_id: TypeId,
    ) -> ConstantId {
        let determinant = determinant(ir_meta, constant_id);
        ir_meta.get_constant_float(determinant)
    }
    fn built_in_inverse(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let (m, size) = get_matrix_values(ir_meta, constant_id);
        let determinant = determinant_helper(&m, size);
        let determinant_reciprocal = if determinant == 0. { 0. } else { determinant.recip() };

        // The inverse of A is the transpose of the cofactor matrix times the reciprocal of the
        // determinant of A.  In the following, the cofactor matrix is calculated and
        // simultaneously transposed.
        let mut coft = [[0.; 4]; 4];
        let vec_type_id = match size {
            2 => {
                coft[0][0] = m[1][1];
                coft[1][0] = -m[1][0];
                coft[0][1] = -m[0][1];
                coft[1][1] = m[0][0];
                TYPE_ID_VEC2
            }
            3 => {
                coft[0][0] = m[1][1] * m[2][2] - m[2][1] * m[1][2];
                coft[1][0] = -(m[1][0] * m[2][2] - m[2][0] * m[1][2]);
                coft[2][0] = m[1][0] * m[2][1] - m[2][0] * m[1][1];
                coft[0][1] = -(m[0][1] * m[2][2] - m[2][1] * m[0][2]);
                coft[1][1] = m[0][0] * m[2][2] - m[2][0] * m[0][2];
                coft[2][1] = -(m[0][0] * m[2][1] - m[2][0] * m[0][1]);
                coft[0][2] = m[0][1] * m[1][2] - m[1][1] * m[0][2];
                coft[1][2] = -(m[0][0] * m[1][2] - m[1][0] * m[0][2]);
                coft[2][2] = m[0][0] * m[1][1] - m[1][0] * m[0][1];
                TYPE_ID_VEC3
            }
            4 => {
                coft[0][0] = m[1][1] * m[2][2] * m[3][3]
                    + m[2][1] * m[3][2] * m[1][3]
                    + m[3][1] * m[1][2] * m[2][3]
                    - m[1][1] * m[3][2] * m[2][3]
                    - m[2][1] * m[1][2] * m[3][3]
                    - m[3][1] * m[2][2] * m[1][3];
                coft[1][0] = -(m[1][0] * m[2][2] * m[3][3]
                    + m[2][0] * m[3][2] * m[1][3]
                    + m[3][0] * m[1][2] * m[2][3]
                    - m[1][0] * m[3][2] * m[2][3]
                    - m[2][0] * m[1][2] * m[3][3]
                    - m[3][0] * m[2][2] * m[1][3]);
                coft[2][0] = m[1][0] * m[2][1] * m[3][3]
                    + m[2][0] * m[3][1] * m[1][3]
                    + m[3][0] * m[1][1] * m[2][3]
                    - m[1][0] * m[3][1] * m[2][3]
                    - m[2][0] * m[1][1] * m[3][3]
                    - m[3][0] * m[2][1] * m[1][3];
                coft[3][0] = -(m[1][0] * m[2][1] * m[3][2]
                    + m[2][0] * m[3][1] * m[1][2]
                    + m[3][0] * m[1][1] * m[2][2]
                    - m[1][0] * m[3][1] * m[2][2]
                    - m[2][0] * m[1][1] * m[3][2]
                    - m[3][0] * m[2][1] * m[1][2]);
                coft[0][1] = -(m[0][1] * m[2][2] * m[3][3]
                    + m[2][1] * m[3][2] * m[0][3]
                    + m[3][1] * m[0][2] * m[2][3]
                    - m[0][1] * m[3][2] * m[2][3]
                    - m[2][1] * m[0][2] * m[3][3]
                    - m[3][1] * m[2][2] * m[0][3]);
                coft[1][1] = m[0][0] * m[2][2] * m[3][3]
                    + m[2][0] * m[3][2] * m[0][3]
                    + m[3][0] * m[0][2] * m[2][3]
                    - m[0][0] * m[3][2] * m[2][3]
                    - m[2][0] * m[0][2] * m[3][3]
                    - m[3][0] * m[2][2] * m[0][3];
                coft[2][1] = -(m[0][0] * m[2][1] * m[3][3]
                    + m[2][0] * m[3][1] * m[0][3]
                    + m[3][0] * m[0][1] * m[2][3]
                    - m[0][0] * m[3][1] * m[2][3]
                    - m[2][0] * m[0][1] * m[3][3]
                    - m[3][0] * m[2][1] * m[0][3]);
                coft[3][1] = m[0][0] * m[2][1] * m[3][2]
                    + m[2][0] * m[3][1] * m[0][2]
                    + m[3][0] * m[0][1] * m[2][2]
                    - m[0][0] * m[3][1] * m[2][2]
                    - m[2][0] * m[0][1] * m[3][2]
                    - m[3][0] * m[2][1] * m[0][2];
                coft[0][2] = m[0][1] * m[1][2] * m[3][3]
                    + m[1][1] * m[3][2] * m[0][3]
                    + m[3][1] * m[0][2] * m[1][3]
                    - m[0][1] * m[3][2] * m[1][3]
                    - m[1][1] * m[0][2] * m[3][3]
                    - m[3][1] * m[1][2] * m[0][3];
                coft[1][2] = -(m[0][0] * m[1][2] * m[3][3]
                    + m[1][0] * m[3][2] * m[0][3]
                    + m[3][0] * m[0][2] * m[1][3]
                    - m[0][0] * m[3][2] * m[1][3]
                    - m[1][0] * m[0][2] * m[3][3]
                    - m[3][0] * m[1][2] * m[0][3]);
                coft[2][2] = m[0][0] * m[1][1] * m[3][3]
                    + m[1][0] * m[3][1] * m[0][3]
                    + m[3][0] * m[0][1] * m[1][3]
                    - m[0][0] * m[3][1] * m[1][3]
                    - m[1][0] * m[0][1] * m[3][3]
                    - m[3][0] * m[1][1] * m[0][3];
                coft[3][2] = -(m[0][0] * m[1][1] * m[3][2]
                    + m[1][0] * m[3][1] * m[0][2]
                    + m[3][0] * m[0][1] * m[1][2]
                    - m[0][0] * m[3][1] * m[1][2]
                    - m[1][0] * m[0][1] * m[3][2]
                    - m[3][0] * m[1][1] * m[0][2]);
                coft[0][3] = -(m[0][1] * m[1][2] * m[2][3]
                    + m[1][1] * m[2][2] * m[0][3]
                    + m[2][1] * m[0][2] * m[1][3]
                    - m[0][1] * m[2][2] * m[1][3]
                    - m[1][1] * m[0][2] * m[2][3]
                    - m[2][1] * m[1][2] * m[0][3]);
                coft[1][3] = m[0][0] * m[1][2] * m[2][3]
                    + m[1][0] * m[2][2] * m[0][3]
                    + m[2][0] * m[0][2] * m[1][3]
                    - m[0][0] * m[2][2] * m[1][3]
                    - m[1][0] * m[0][2] * m[2][3]
                    - m[2][0] * m[1][2] * m[0][3];
                coft[2][3] = -(m[0][0] * m[1][1] * m[2][3]
                    + m[1][0] * m[2][1] * m[0][3]
                    + m[2][0] * m[0][1] * m[1][3]
                    - m[0][0] * m[2][1] * m[1][3]
                    - m[1][0] * m[0][1] * m[2][3]
                    - m[2][0] * m[1][1] * m[0][3]);
                coft[3][3] = m[0][0] * m[1][1] * m[2][2]
                    + m[1][0] * m[2][1] * m[0][2]
                    + m[2][0] * m[0][1] * m[1][2]
                    - m[0][0] * m[2][1] * m[1][2]
                    - m[1][0] * m[0][1] * m[2][2]
                    - m[2][0] * m[1][1] * m[0][2];
                TYPE_ID_VEC4
            }
            _ => panic!("Internal error: Invalid matrix dimensions when calculating inverse"),
        };

        let columns = (0..size)
            .map(|column_index| {
                let column = (0..size)
                    .map(|row_index| {
                        ir_meta.get_constant_float(
                            coft[column_index][row_index] * determinant_reciprocal,
                        )
                    })
                    .collect();
                ir_meta.get_constant_composite(vec_type_id, column)
            })
            .collect();
        ir_meta.get_constant_composite(result_type_id, columns)
    }
    fn float_isx_helper<FloatOp>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32) -> bool + Copy,
    {
        let constant = ir_meta.get_constant(constant_id);
        let type_id = constant.type_id;
        let element_type_id = ir_meta.get_type(type_id).get_element_type_id().unwrap_or(type_id);

        match &constant.value {
            &ConstantValue::Float(f) => ir_meta.get_constant_bool(float_op(f)),
            ConstantValue::Int(_)
            | ConstantValue::Uint(_)
            | ConstantValue::Bool(_)
            | ConstantValue::YuvCsc(_) => {
                panic!("Internal error: is<foo> not allowed on constants other than float")
            }
            ConstantValue::Composite(components) => {
                let mapped = components
                    .clone()
                    .iter()
                    .map(|&component_id| {
                        float_isx_helper(ir_meta, component_id, element_type_id, float_op)
                    })
                    .collect();
                ir_meta.get_constant_composite(result_type_id, mapped)
            }
        }
    }
    fn built_in_isnan(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        float_isx_helper(ir_meta, constant_id, result_type_id, |f| f.is_nan())
    }
    fn built_in_isinf(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        float_isx_helper(ir_meta, constant_id, result_type_id, |f| f.is_infinite())
    }
    fn pack2x16_helper<Pack>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        pack: Pack,
    ) -> ConstantId
    where
        Pack: Fn(f32) -> u16 + Copy,
    {
        debug_assert!(result_type_id == TYPE_ID_UINT);

        // Expect a vec2, produce a uint.  First float is written to the least significant bits.
        let fs = ir_meta.get_constant(constant_id).value.get_composite_elements();
        let f1 = ir_meta.get_constant(fs[0]).value.get_float();
        let f2 = ir_meta.get_constant(fs[1]).value.get_float();

        let result = pack(f1) as u32 | (pack(f2) as u32) << 16;
        ir_meta.get_constant_uint(result)
    }
    fn unpack2x16_helper<Unpack>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        unpack: Unpack,
    ) -> ConstantId
    where
        Unpack: Fn(u16) -> f32 + Copy,
    {
        debug_assert!(result_type_id == TYPE_ID_VEC2);

        // Expect a uint, produce a vec2.  First float is taken from the least significant bits.
        let packed = ir_meta.get_constant(constant_id).value.get_uint();
        let f1 = unpack((packed & 0xFFFF) as u16);
        let f2 = unpack((packed >> 16) as u16);

        let unpacked = vec![ir_meta.get_constant_float(f1), ir_meta.get_constant_float(f2)];
        ir_meta.get_constant_composite(result_type_id, unpacked)
    }
    fn pack4x8_helper<Pack>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        pack: Pack,
    ) -> ConstantId
    where
        Pack: Fn(f32) -> u8 + Copy,
    {
        debug_assert!(result_type_id == TYPE_ID_UINT);

        // Expect a vec4, produce a uint.  First float is written to the least significant bits.
        let fs = ir_meta.get_constant(constant_id).value.get_composite_elements();
        let f1 = ir_meta.get_constant(fs[0]).value.get_float();
        let f2 = ir_meta.get_constant(fs[1]).value.get_float();
        let f3 = ir_meta.get_constant(fs[2]).value.get_float();
        let f4 = ir_meta.get_constant(fs[3]).value.get_float();

        let result = pack(f1) as u32
            | (pack(f2) as u32) << 8
            | (pack(f3) as u32) << 16
            | (pack(f4) as u32) << 24;
        ir_meta.get_constant_uint(result)
    }
    fn unpack4x8_helper<Unpack>(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
        unpack: Unpack,
    ) -> ConstantId
    where
        Unpack: Fn(u8) -> f32 + Copy,
    {
        debug_assert!(result_type_id == TYPE_ID_VEC4);

        // Expect a uint, produce a vec4.  First float is taken from the least significant bits.
        let packed = ir_meta.get_constant(constant_id).value.get_uint();
        let f1 = unpack((packed & 0xFF) as u8);
        let f2 = unpack((packed >> 8 & 0xFF) as u8);
        let f3 = unpack((packed >> 16 & 0xFF) as u8);
        let f4 = unpack((packed >> 24 & 0xFF) as u8);

        let unpacked = vec![
            ir_meta.get_constant_float(f1),
            ir_meta.get_constant_float(f2),
            ir_meta.get_constant_float(f3),
            ir_meta.get_constant_float(f4),
        ];
        ir_meta.get_constant_composite(result_type_id, unpacked)
    }
    fn f32_to_snorm16(v: f32) -> i16 {
        (v.clamp(-1., 1.) * 32767.).round() as i16
    }
    fn built_in_packsnorm2x16(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        pack2x16_helper(ir_meta, constant_id, result_type_id, |f| f32_to_snorm16(f) as u16)
    }
    fn snorm16_to_f32(v: i16) -> f32 {
        (v as f32 / 32767.).clamp(-1., 1.)
    }
    fn built_in_unpacksnorm2x16(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        unpack2x16_helper(ir_meta, constant_id, result_type_id, |u| snorm16_to_f32(u as i16))
    }
    fn f32_to_unorm16(v: f32) -> u16 {
        (v.clamp(0., 1.) * 65535.).round() as u16
    }
    fn built_in_packunorm2x16(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        pack2x16_helper(ir_meta, constant_id, result_type_id, f32_to_unorm16)
    }
    fn unorm16_to_f32(v: u16) -> f32 {
        v as f32 / 65535.
    }
    fn built_in_unpackunorm2x16(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        unpack2x16_helper(ir_meta, constant_id, result_type_id, unorm16_to_f32)
    }
    fn f32_to_f16(v: f32) -> u16 {
        let v = v.to_bits();
        let sign = (v & 0x80000000) >> 16;
        let abs = v & 0x7FFFFFFF;

        if abs > 0x7F800000 {
            // NaN
            0x7FFF
        } else if abs > 0x47FFEFFF {
            // Infinity
            (sign | 0x7C00) as u16
        } else if abs < 0x38800000 {
            // Denormal
            let mantissa = (abs & 0x007FFFFF) | 0x00800000;
            let e = 113 - (abs >> 23);

            let abs = if e < 24 { mantissa >> e } else { 0 };

            (sign | (abs + 0x00000FFF + (abs >> 13 & 1)) >> 13) as u16
        } else {
            (sign | (abs + 0xC8000000 + 0x00000FFF + (abs >> 13 & 1)) >> 13) as u16
        }
    }
    fn built_in_packhalf2x16(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        pack2x16_helper(ir_meta, constant_id, result_type_id, f32_to_f16)
    }
    fn f16_to_f32(v: u16) -> f32 {
        // Based on http://www.fox-toolkit.org/ftp/fasthalffloatconversion.pdf
        // See also Float16ToFloat32.py, this is non-baked copy of that generator.
        let offset = v >> 10;
        let offset = if offset == 0 || offset == 32 { 0 } else { 1024 };
        let offset = offset + (v & 0x3FF);

        let mantissa = if offset == 0 {
            0
        } else if offset < 1024 {
            let mut m = (offset as u32) << 13;
            let mut e = 0x38800000;
            while (m & 0x00800000) == 0 {
                e -= 0x00800000;
                m <<= 1;
            }
            m &= !0x00800000;
            m | e
        } else {
            0x38000000 + ((offset as u32 - 1024) << 13)
        };

        let exponent = v >> 10;
        let exponent = if exponent == 0 {
            0
        } else if exponent < 31 {
            (exponent as u32) << 23
        } else if exponent == 31 {
            0x47800000
        } else if exponent == 32 {
            0x80000000
        } else if exponent < 63 {
            0x80000000 + ((exponent as u32 - 32) << 23)
        } else {
            0xC7800000
        };

        f32::from_bits(mantissa + exponent)
    }
    fn built_in_unpackhalf2x16(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        unpack2x16_helper(ir_meta, constant_id, result_type_id, f16_to_f32)
    }
    fn f32_to_snorm8(v: f32) -> i8 {
        (v.clamp(-1., 1.) * 127.).round() as i8
    }
    fn built_in_packsnorm4x8(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        pack4x8_helper(ir_meta, constant_id, result_type_id, |f| f32_to_snorm8(f) as u8)
    }
    fn snorm8_to_f32(v: i8) -> f32 {
        (v as f32 / 127.).clamp(-1., 1.)
    }
    fn built_in_unpacksnorm4x8(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        unpack4x8_helper(ir_meta, constant_id, result_type_id, |u| snorm8_to_f32(u as i8))
    }
    fn f32_to_unorm8(v: f32) -> u8 {
        (v.clamp(0., 1.) * 255.).round() as u8
    }
    fn built_in_packunorm4x8(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        pack4x8_helper(ir_meta, constant_id, result_type_id, f32_to_unorm8)
    }
    fn unorm8_to_f32(v: u8) -> f32 {
        v as f32 / 255.
    }
    fn built_in_unpackunorm4x8(
        ir_meta: &mut IRMeta,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        unpack4x8_helper(ir_meta, constant_id, result_type_id, unorm8_to_f32)
    }
    pub fn built_in_unary(
        ir_meta: &mut IRMeta,
        op: UnaryOpCode,
        constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let float_op = match op {
            UnaryOpCode::Radians => |f: f32| f.to_radians(),
            UnaryOpCode::Degrees => |f: f32| f.to_degrees(),
            UnaryOpCode::Sin => |f: f32| f.sin(),
            UnaryOpCode::Cos => |f: f32| f.cos(),
            UnaryOpCode::Tan => |f: f32| f.tan(),
            // For asin(x), results are undefined if |x| > 1, we are choosing to set result to 0.
            UnaryOpCode::Asin => |f: f32| if f.abs() > 1. { 0. } else { f.asin() },
            // For acos(x), results are undefined if |x| > 1, we are choosing to set result to 0.
            UnaryOpCode::Acos => |f: f32| if f.abs() > 1. { 0. } else { f.acos() },
            UnaryOpCode::Atan => |f: f32| f.atan(),
            UnaryOpCode::Sinh => |f: f32| f.sinh(),
            UnaryOpCode::Cosh => |f: f32| f.cosh(),
            UnaryOpCode::Tanh => |f: f32| f.tanh(),
            UnaryOpCode::Asinh => |f: f32| f.asinh(),
            // For acosh(x), results are undefined if |x| < 1, we are choosing to set result to 0.
            UnaryOpCode::Acosh => |f: f32| if f.abs() < 1. { 0. } else { f.acosh() },
            // For atanh(x), results are undefined if |x| >= 1, we are choosing to set result to 0.
            UnaryOpCode::Atanh => |f: f32| if f.abs() >= 1. { 0. } else { f.atanh() },
            UnaryOpCode::Exp => |f: f32| f.exp(),
            // For log(x), results are undefined if x <= 0, we are choosing to set result to 0.
            UnaryOpCode::Log => |f: f32| if f <= 0. { 0. } else { f.ln() },
            UnaryOpCode::Exp2 => |f: f32| f.exp2(),
            // For log2(x), results are undefined if x <= 0, we are choosing to set result to 0.
            UnaryOpCode::Log2 => |f: f32| f.log2(),
            // For sqrt(x), results are undefined if x < 0, we are choosing to set result to 0.
            UnaryOpCode::Sqrt => |f: f32| if f < 0. { 0. } else { f.sqrt() },
            // For inversesqrt(x), results are undefined if x <= 0, we are choosing to set result
            // to 0.
            UnaryOpCode::Inversesqrt => |f: f32| if f <= 0. { 0. } else { f.sqrt().recip() },
            UnaryOpCode::Abs => |f: f32| f.abs(),
            UnaryOpCode::Sign => |f: f32| {
                if f > 0. {
                    1.
                } else if f < 0. {
                    -1.
                } else {
                    0.
                }
            },
            UnaryOpCode::Floor => |f: f32| f.floor(),
            UnaryOpCode::Trunc => |f: f32| f.trunc(),
            UnaryOpCode::Round => |f: f32| f.round(),
            UnaryOpCode::RoundEven => |f: f32| f.round_ties_even(),
            UnaryOpCode::Ceil => |f: f32| f.ceil(),
            UnaryOpCode::Fract => |f: f32| f.fract(),
            UnaryOpCode::Isnan => {
                return built_in_isnan(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Isinf => {
                return built_in_isinf(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::FloatBitsToInt => {
                return built_in_floatbitstoint(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::FloatBitsToUint => {
                return built_in_floatbitstouint(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::IntBitsToFloat => {
                return built_in_intbitstofloat(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::UintBitsToFloat => {
                return built_in_uintbitstofloat(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::PackSnorm2x16 => {
                return built_in_packsnorm2x16(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::PackHalf2x16 => {
                return built_in_packhalf2x16(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::UnpackSnorm2x16 => {
                return built_in_unpacksnorm2x16(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::UnpackHalf2x16 => {
                return built_in_unpackhalf2x16(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::PackUnorm2x16 => {
                return built_in_packunorm2x16(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::UnpackUnorm2x16 => {
                return built_in_unpackunorm2x16(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::PackUnorm4x8 => {
                return built_in_packunorm4x8(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::PackSnorm4x8 => {
                return built_in_packsnorm4x8(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::UnpackUnorm4x8 => {
                return built_in_unpackunorm4x8(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::UnpackSnorm4x8 => {
                return built_in_unpacksnorm4x8(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Length => {
                return built_in_length(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Normalize => {
                return built_in_normalize(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Transpose => {
                return built_in_transpose(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Determinant => {
                return built_in_determinant(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Inverse => {
                return built_in_inverse(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::Any => {
                return built_in_any(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::All => {
                return built_in_all(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::BitCount => {
                return built_in_bitcount(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::FindLSB => {
                return built_in_findlsb(ir_meta, constant_id, result_type_id);
            }
            UnaryOpCode::FindMSB => {
                return built_in_findmsb(ir_meta, constant_id, result_type_id);
            }
            // For dFdx, dFdy and fwidth, note that derivates of constants is 0
            UnaryOpCode::DFdx => |_f: f32| 0.,
            UnaryOpCode::DFdy => |_f: f32| 0.,
            UnaryOpCode::Fwidth => |_f: f32| 0.,
            UnaryOpCode::InterpolateAtCentroid => return constant_id,
            UnaryOpCode::AtomicCounter
            | UnaryOpCode::AtomicCounterIncrement
            | UnaryOpCode::AtomicCounterDecrement
            | UnaryOpCode::ImageSize
            | UnaryOpCode::PixelLocalLoadANGLE => {
                panic!("Internal error: Unexpected built-ins to constant-fold")
            }
            _ => |_| panic!("Internal error: Invalid built-in operation on float"),
        };

        let int_op = match op {
            UnaryOpCode::Abs => |i: i32| i.abs(),
            UnaryOpCode::Sign => |i: i32| {
                if i > 0 {
                    1
                } else if i < 0 {
                    -1
                } else {
                    0
                }
            },
            UnaryOpCode::BitfieldReverse => |i: i32| i.reverse_bits(),
            _ => |_| panic!("Internal error: Invalid built-in operation on int"),
        };

        let uint_op = match op {
            UnaryOpCode::BitfieldReverse => |u: u32| u.reverse_bits(),
            _ => |_| panic!("Internal error: Invalid built-in operation on uint"),
        };

        let bool_op = match op {
            UnaryOpCode::Not => |b: bool| !b,
            _ => |_| panic!("Internal error: Invalid built-in operation on uint"),
        };

        apply_unary_componentwise(
            ir_meta,
            constant_id,
            result_type_id,
            float_op,
            int_op,
            uint_op,
            bool_op,
        )
    }

    fn built_in_distance(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        debug_assert!(result_type_id == TYPE_ID_FLOAT);

        let lhs = ir_meta.get_constant(lhs_constant_id);
        let rhs = ir_meta.get_constant(rhs_constant_id);

        if !lhs.value.is_composite() {
            return ir_meta
                .get_constant_float((lhs.value.get_float() - rhs.value.get_float()).abs());
        }

        let lhs = lhs.value.get_composite_elements();
        let rhs = rhs.value.get_composite_elements();

        let sum: f32 = lhs
            .iter()
            .zip(rhs.iter())
            .map(|(&lhs_component_id, &rhs_component_id)| {
                let lhs_component = ir_meta.get_constant(lhs_component_id).value.get_float();
                let rhs_component = ir_meta.get_constant(rhs_component_id).value.get_float();
                let diff = rhs_component - lhs_component;
                diff * diff
            })
            .sum();
        ir_meta.get_constant_float(sum.sqrt())
    }
    fn built_in_dot(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        debug_assert!(result_type_id == TYPE_ID_FLOAT);
        ir_meta.get_constant_float(dot(ir_meta, lhs_constant_id, rhs_constant_id))
    }
    fn built_in_cross(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        debug_assert!(result_type_id == TYPE_ID_VEC3);

        let lhs = ir_meta.get_constant(lhs_constant_id).value.get_composite_elements();
        let rhs = ir_meta.get_constant(rhs_constant_id).value.get_composite_elements();

        let x2 = ir_meta.get_constant(lhs[2]).value.get_float();
        let x1 = ir_meta.get_constant(lhs[1]).value.get_float();
        let x0 = ir_meta.get_constant(lhs[0]).value.get_float();
        let y2 = ir_meta.get_constant(rhs[2]).value.get_float();
        let y1 = ir_meta.get_constant(rhs[1]).value.get_float();
        let y0 = ir_meta.get_constant(rhs[0]).value.get_float();

        let components = vec![
            ir_meta.get_constant_float(x1 * y2 - y1 * x2),
            ir_meta.get_constant_float(x2 * y0 - y2 * x0),
            ir_meta.get_constant_float(x0 * y1 - y0 * x1),
        ];
        ir_meta.get_constant_composite(result_type_id, components)
    }
    fn built_in_reflect(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        // For the incident vector I and surface orientation N, returns the reflection
        // direction:
        //     I - 2 * dot(N, I) * N.
        let n_dot_i = dot(ir_meta, rhs_constant_id, lhs_constant_id);

        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |i, n| i - 2. * n_dot_i * n,
            |_, _| panic!("Internal error: The reflect() built-in only applies to float types"),
            |_, _| panic!("Internal error: The reflect() built-in only applies to float types"),
            |_, _| panic!("Internal error: The reflect() built-in only applies to float types"),
        )
    }
    fn built_in_outerproduct(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let lhs_type_id = ir_meta.get_constant(lhs_constant_id).type_id;
        let rhs = ir_meta.get_constant(rhs_constant_id).value.get_composite_elements().clone();
        let column_count = rhs.len();

        let columns = (0..column_count)
            .map(|column| mul(ir_meta, lhs_constant_id, rhs[column], lhs_type_id))
            .collect();
        ir_meta.get_constant_composite(result_type_id, columns)
    }
    fn compare_helper<FloatOp, IntOp, UintOp, BoolOp>(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
        float_op: FloatOp,
        int_op: IntOp,
        uint_op: UintOp,
        bool_op: BoolOp,
    ) -> ConstantId
    where
        FloatOp: Fn(f32, f32) -> bool + Copy,
        IntOp: Fn(i32, i32) -> bool + Copy,
        UintOp: Fn(u32, u32) -> bool + Copy,
        BoolOp: Fn(bool, bool) -> bool + Copy,
    {
        let lhs_constant = ir_meta.get_constant(lhs_constant_id);
        let rhs_constant = ir_meta.get_constant(rhs_constant_id);

        if let (
            ConstantValue::Composite(lhs_components),
            ConstantValue::Composite(rhs_components),
        ) = (&lhs_constant.value, &rhs_constant.value)
        {
            let mapped = lhs_components
                .clone()
                .iter()
                .zip(rhs_components.clone().iter())
                .map(|(&lhs_component_id, &rhs_component_id)| {
                    let lhs = ir_meta.get_constant(lhs_component_id).value.clone();
                    let rhs = ir_meta.get_constant(rhs_component_id).value.clone();

                    match lhs {
                        ConstantValue::Float(lhs) => ir_meta.get_constant_bool(float_op(lhs, rhs.get_float())),
                        ConstantValue::Int(lhs) => ir_meta.get_constant_bool(int_op(lhs, rhs.get_int())),
                        ConstantValue::Uint(lhs) => ir_meta.get_constant_bool(uint_op(lhs, rhs.get_uint())),
                        ConstantValue::Bool(lhs) => ir_meta.get_constant_bool(bool_op(lhs, rhs.get_bool())),
                        _ => { panic!("Internal error: Comparison built-ins only valid on float, [u]int and bool values"); }
                    }
                })
                .collect();
            ir_meta.get_constant_composite(result_type_id, mapped)
        } else {
            panic!("Internal error: Comparison built-ins only valid on vector types");
        }
    }
    fn built_in_lessthan(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        compare_helper(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 < f2,
            |i1, i2| i1 < i2,
            |u1, u2| u1 < u2,
            |_, _| panic!("Internal error: lessThan() does not accept bool vectors"),
        )
    }
    fn built_in_lessthanequal(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        compare_helper(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 <= f2,
            |i1, i2| i1 <= i2,
            |u1, u2| u1 <= u2,
            |_, _| panic!("Internal error: lessThanEqual() does not accept bool vectors"),
        )
    }
    fn built_in_greaterthan(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        compare_helper(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 > f2,
            |i1, i2| i1 > i2,
            |u1, u2| u1 > u2,
            |_, _| panic!("Internal error: greaterThan() does not accept bool vectors"),
        )
    }
    fn built_in_greaterthanequal(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        compare_helper(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 >= f2,
            |i1, i2| i1 >= i2,
            |u1, u2| u1 >= u2,
            |_, _| panic!("Internal error: greaterThanEqual() does not accept bool vectors"),
        )
    }
    fn built_in_equal(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        compare_helper(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 == f2,
            |i1, i2| i1 == i2,
            |u1, u2| u1 == u2,
            |b1, b2| b1 == b2,
        )
    }
    fn built_in_notequal(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        compare_helper(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            |f1, f2| f1 != f2,
            |i1, i2| i1 != i2,
            |u1, u2| u1 != u2,
            |b1, b2| b1 != b2,
        )
    }
    fn ldexp_helper(ir_meta: &mut IRMeta, x: f32, exp: i32) -> ConstantId {
        let result = if (-126..=128).contains(&exp) { x * 2.0_f32.powi(exp) } else { 0.0 };
        ir_meta.get_constant_float(result)
    }
    fn built_in_ldexp(
        ir_meta: &mut IRMeta,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        let lhs_constant = ir_meta.get_constant(lhs_constant_id);
        let rhs_constant = ir_meta.get_constant(rhs_constant_id);

        if let (
            ConstantValue::Composite(lhs_components),
            ConstantValue::Composite(rhs_components),
        ) = (&lhs_constant.value, &rhs_constant.value)
        {
            let mapped = lhs_components
                .clone()
                .iter()
                .zip(rhs_components.clone().iter())
                .map(|(&lhs_component_id, &rhs_component_id)| {
                    let lhs = ir_meta.get_constant(lhs_component_id).value.clone();
                    let rhs = ir_meta.get_constant(rhs_component_id).value.clone();
                    ldexp_helper(ir_meta, lhs.get_float(), rhs.get_int())
                })
                .collect();
            ir_meta.get_constant_composite(result_type_id, mapped)
        } else {
            ldexp_helper(ir_meta, lhs_constant.value.get_float(), rhs_constant.value.get_int())
        }
    }
    pub fn built_in_binary(
        ir_meta: &mut IRMeta,
        op: BinaryOpCode,
        lhs_constant_id: ConstantId,
        rhs_constant_id: ConstantId,
        result_type_id: TypeId,
    ) -> ConstantId {
        if op == BinaryOpCode::Ldexp {
            return built_in_ldexp(ir_meta, lhs_constant_id, rhs_constant_id, result_type_id);
        }

        let float_op = match op {
            BinaryOpCode::Atan => |y: f32, x: f32| y.atan2(x),
            // For pow(x, y), results are undefined if x < 0 or if x = 0 and y <= 0.
            BinaryOpCode::Pow => {
                |x: f32, y: f32| if x < 0. || (x == 0. && y <= 0.) { 0. } else { x.powf(y) }
            }
            BinaryOpCode::Mod => |f1: f32, f2: f32| f1 - f2 * (f1 / f2).floor(),
            BinaryOpCode::Min => |f1: f32, f2: f32| f1.min(f2),
            BinaryOpCode::Max => |f1: f32, f2: f32| f1.max(f2),
            BinaryOpCode::Step => |edge: f32, x: f32| if x < edge { 0. } else { 1. },
            BinaryOpCode::Distance => {
                return built_in_distance(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::Dot => {
                return built_in_dot(ir_meta, lhs_constant_id, rhs_constant_id, result_type_id);
            }
            BinaryOpCode::Cross => {
                return built_in_cross(ir_meta, lhs_constant_id, rhs_constant_id, result_type_id);
            }
            BinaryOpCode::Reflect => {
                return built_in_reflect(ir_meta, lhs_constant_id, rhs_constant_id, result_type_id);
            }
            BinaryOpCode::MatrixCompMult => |f1: f32, f2: f32| f1 * f2,
            BinaryOpCode::OuterProduct => {
                return built_in_outerproduct(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::LessThanVec => {
                return built_in_lessthan(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::LessThanEqualVec => {
                return built_in_lessthanequal(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::GreaterThanVec => {
                return built_in_greaterthan(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::GreaterThanEqualVec => {
                return built_in_greaterthanequal(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::EqualVec => {
                return built_in_equal(ir_meta, lhs_constant_id, rhs_constant_id, result_type_id);
            }
            BinaryOpCode::NotEqualVec => {
                return built_in_notequal(
                    ir_meta,
                    lhs_constant_id,
                    rhs_constant_id,
                    result_type_id,
                );
            }
            BinaryOpCode::InterpolateAtSample | BinaryOpCode::InterpolateAtOffset => {
                return lhs_constant_id;
            }
            BinaryOpCode::Modf
            | BinaryOpCode::Frexp
            | BinaryOpCode::Ldexp
            | BinaryOpCode::AtomicAdd
            | BinaryOpCode::AtomicMin
            | BinaryOpCode::AtomicMax
            | BinaryOpCode::AtomicAnd
            | BinaryOpCode::AtomicOr
            | BinaryOpCode::AtomicXor
            | BinaryOpCode::AtomicExchange => {
                panic!("Internal error: Unexpected built-ins to constant-fold")
            }
            _ => panic!("Internal error: Invalid built-in operation on float"),
        };

        let int_op = match op {
            BinaryOpCode::Min => |i1: i32, i2: i32| i1.min(i2),
            BinaryOpCode::Max => |i1: i32, i2: i32| i1.max(i2),
            _ => |_, _| panic!("Internal error: Invalid built-in operation on int"),
        };

        let uint_op = match op {
            BinaryOpCode::Min => |u1: u32, u2: u32| u1.min(u2),
            BinaryOpCode::Max => |u1: u32, u2: u32| u1.max(u2),
            _ => |_, _| panic!("Internal error: Invalid built-in operation on uint"),
        };

        let bool_op = |_, _| panic!("Internal error: Invalid built-in operation on uint");

        apply_binary_componentwise(
            ir_meta,
            lhs_constant_id,
            rhs_constant_id,
            result_type_id,
            float_op,
            int_op,
            uint_op,
            bool_op,
        )
    }

    fn built_in_clamp_with_scalar_limits(
        ir_meta: &mut IRMeta,
        x_constant_id: ConstantId,
        min_constant: &ConstantValue,
        max_constant: &ConstantValue,
        result_type_id: TypeId,
    ) -> ConstantId {
        apply_unary_componentwise(
            ir_meta,
            x_constant_id,
            result_type_id,
            |f| {
                let min = min_constant.get_float();
                let max = max_constant.get_float();
                if min > max { 0. } else { f.clamp(min, max) }
            },
            |i| {
                let min = min_constant.get_int();
                let max = max_constant.get_int();
                if min > max { 0 } else { i.clamp(min, max) }
            },
            |u| {
                let min = min_constant.get_uint();
                let max = max_constant.get_uint();
                if min > max { 0 } else { u.clamp(min, max) }
            },
            |_| panic!("Internal error: The clamp() built-in does not apply to bool types"),
        )
    }
    pub fn built_in_clamp(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // clamp(x, min, max) either accepts scalars for min, max or a vector of matching size with
        // x.
        let max_constant = ir_meta.get_constant(operands[2]);
        let min_constant = ir_meta.get_constant(operands[1]);
        let x_constant = ir_meta.get_constant(operands[0]);

        if let (
            ConstantValue::Composite(x_components),
            ConstantValue::Composite(min_components),
            ConstantValue::Composite(max_components),
        ) = (&x_constant.value, &min_constant.value, &max_constant.value)
        {
            let result_element_type_id =
                ir_meta.get_type(result_type_id).get_element_type_id().unwrap();

            let components = x_components
                .clone()
                .iter()
                .zip(min_components.clone().iter())
                .zip(max_components.clone().iter())
                .map(|((&x_component_id, &min_component_id), &max_component_id)| {
                    let min = ir_meta.get_constant(min_component_id).value.clone();
                    let max = ir_meta.get_constant(max_component_id).value.clone();

                    built_in_clamp_with_scalar_limits(
                        ir_meta,
                        x_component_id,
                        &min,
                        &max,
                        result_element_type_id,
                    )
                })
                .collect();

            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            built_in_clamp_with_scalar_limits(
                ir_meta,
                operands[0],
                &min_constant.value.clone(),
                &max_constant.value.clone(),
                result_type_id,
            )
        }
    }

    fn built_in_mix_with_scalar_factor(
        ir_meta: &mut IRMeta,
        x_constant_id: ConstantId,
        y_constant_id: ConstantId,
        a_constant: &ConstantValue,
        result_type_id: TypeId,
    ) -> ConstantId {
        if let &ConstantValue::Bool(a) = a_constant {
            if a { y_constant_id } else { x_constant_id }
        } else {
            apply_binary_componentwise(
                ir_meta,
                x_constant_id,
                y_constant_id,
                result_type_id,
                |f1, f2| {
                    let a = a_constant.get_float();
                    f1 * (1. - a) + f2 * a
                },
                |_, _| panic!("Internal error: The mix() built-in only applies to float types"),
                |_, _| panic!("Internal error: The mix() built-in only applies to float types"),
                |_, _| panic!("Internal error: The mix() built-in only applies to float types"),
            )
        }
    }
    pub fn built_in_mix(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // mix(x, y, a) either accepts scalar for a, or a vector of matching size with x and y.
        // The type of a could possibly be bool instead of float (but not if x and y are vectors
        // and a is a scalar).
        let a_constant = ir_meta.get_constant(operands[2]);
        let y_constant = ir_meta.get_constant(operands[1]);
        let x_constant = ir_meta.get_constant(operands[0]);

        if let (
            ConstantValue::Composite(x_components),
            ConstantValue::Composite(y_components),
            ConstantValue::Composite(a_components),
        ) = (&x_constant.value, &y_constant.value, &a_constant.value)
        {
            let result_element_type_id =
                ir_meta.get_type(result_type_id).get_element_type_id().unwrap();

            let components = x_components
                .clone()
                .iter()
                .zip(y_components.clone().iter())
                .zip(a_components.clone().iter())
                .map(|((&x_component_id, &y_component_id), &a_component_id)| {
                    let a = ir_meta.get_constant(a_component_id).value.clone();

                    built_in_mix_with_scalar_factor(
                        ir_meta,
                        x_component_id,
                        y_component_id,
                        &a,
                        result_element_type_id,
                    )
                })
                .collect();

            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            built_in_mix_with_scalar_factor(
                ir_meta,
                operands[0],
                operands[1],
                &a_constant.value.clone(),
                result_type_id,
            )
        }
    }
    fn built_in_smoothstep_with_scalar_edges(
        ir_meta: &mut IRMeta,
        x_constant_id: ConstantId,
        edge0_constant: &ConstantValue,
        edge1_constant: &ConstantValue,
        result_type_id: TypeId,
    ) -> ConstantId {
        let edge0 = edge0_constant.get_float();
        let edge1 = edge1_constant.get_float();

        apply_unary_componentwise(
            ir_meta,
            x_constant_id,
            result_type_id,
            |f| {
                if edge0 >= edge1 {
                    0.
                } else {
                    // Returns 0.0 if x <= edge0 and 1.0 if x >= edge1 and performs smooth
                    // Hermite interpolation between 0 and 1 when edge0 < x < edge1.
                    let t = ((f - edge0) / (edge1 - edge0)).clamp(0., 1.);
                    t * t * (3. - 2. * t)
                }
            },
            |_| panic!("Internal error: The smoothstep() built-in only applies to float types"),
            |_| panic!("Internal error: The smoothstep() built-in only applies to float types"),
            |_| panic!("Internal error: The smoothstep() built-in only applies to float types"),
        )
    }
    pub fn built_in_smoothstep(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // smoothstep(edge0, edge1, x) either accepts scalar for edge0 and edge1, or a vector of
        // matching size with x.
        let x_constant = ir_meta.get_constant(operands[2]);
        let edge1_constant = ir_meta.get_constant(operands[1]);
        let edge0_constant = ir_meta.get_constant(operands[0]);

        if let (
            ConstantValue::Composite(edge0_components),
            ConstantValue::Composite(edge1_components),
            ConstantValue::Composite(x_components),
        ) = (&edge0_constant.value, &edge1_constant.value, &x_constant.value)
        {
            let result_element_type_id =
                ir_meta.get_type(result_type_id).get_element_type_id().unwrap();

            let components = edge0_components
                .clone()
                .iter()
                .zip(edge1_components.clone().iter())
                .zip(x_components.clone().iter())
                .map(|((&edge0_component_id, &edge1_component_id), &x_component_id)| {
                    let edge0 = ir_meta.get_constant(edge0_component_id).value.clone();
                    let edge1 = ir_meta.get_constant(edge1_component_id).value.clone();

                    built_in_smoothstep_with_scalar_edges(
                        ir_meta,
                        x_component_id,
                        &edge0,
                        &edge1,
                        result_element_type_id,
                    )
                })
                .collect();

            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            built_in_smoothstep_with_scalar_edges(
                ir_meta,
                operands[2],
                &edge0_constant.value.clone(),
                &edge1_constant.value.clone(),
                result_type_id,
            )
        }
    }
    pub fn built_in_fma(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // fma(a, b, c) accepts scalar or vectors of float.
        let c_constant = ir_meta.get_constant(operands[2]);
        let b_constant = ir_meta.get_constant(operands[1]);
        let a_constant = ir_meta.get_constant(operands[0]);

        if let (
            ConstantValue::Composite(a_components),
            ConstantValue::Composite(b_components),
            ConstantValue::Composite(c_components),
        ) = (&a_constant.value, &b_constant.value, &c_constant.value)
        {
            let components = a_components
                .clone()
                .iter()
                .zip(b_components.clone().iter())
                .zip(c_components.clone().iter())
                .map(|((&a_component_id, &b_component_id), &c_component_id)| {
                    let a = ir_meta.get_constant(a_component_id).value.get_float();
                    let b = ir_meta.get_constant(b_component_id).value.get_float();
                    let c = ir_meta.get_constant(c_component_id).value.get_float();

                    ir_meta.get_constant_float(a * b + c)
                })
                .collect();

            ir_meta.get_constant_composite(result_type_id, components)
        } else {
            let a = a_constant.value.get_float();
            let b = b_constant.value.get_float();
            let c = c_constant.value.get_float();

            ir_meta.get_constant_float(a * b + c)
        }
    }
    pub fn built_in_faceforward(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // faceforward(N, I, Nref) accepts scalar or vectors of float.
        let nref_constant_id = operands[2];
        let i_constant_id = operands[1];
        let n_constant_id = operands[0];

        // If dot(Nref, I) < 0 return N, otherwise return -N.
        let nref_dot_i = dot(ir_meta, nref_constant_id, i_constant_id);
        if nref_dot_i < 0. { n_constant_id } else { negate(ir_meta, n_constant_id, result_type_id) }
    }
    pub fn built_in_refract(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // refract(I, N, eta) accepts matching scalars or vectors of float for I and N, and float
        // for eta.
        let eta = ir_meta.get_constant(operands[2]).value.get_float();
        let n_constant_id = operands[1];
        let i_constant_id = operands[0];

        // For the incident vector I and surface normal N, and the ratio of indices of
        // refraction eta, return the refraction vector, R.
        //
        //     k = 1.0 - eta * eta * (1.0 - dot(N, I) * dot(N, I))
        //     if (k < 0.0)
        //         return genType(0.0)
        //     else
        //         return eta * I - (eta * dot(N, I) + sqrt(k)) * N
        let n_dot_i = dot(ir_meta, n_constant_id, i_constant_id);
        let k = 1. - eta * eta * (1. - n_dot_i * n_dot_i);
        let sqrt_k = k.sqrt();
        apply_binary_componentwise(
            ir_meta,
            i_constant_id,
            n_constant_id,
            result_type_id,
            |i, n| {
                if k < 0. { 0. } else { eta * i - (eta * n_dot_i + sqrt_k) * n }
            },
            |_, _| panic!("Internal error: The refract() built-in only applies to float types"),
            |_, _| panic!("Internal error: The refract() built-in only applies to float types"),
            |_, _| panic!("Internal error: The refract() built-in only applies to float types"),
        )
    }
    fn built_in_bitfieldextract_unsigned_scalar(value: u32, offset: i32, bits: i32) -> u32 {
        // If bits is zero, the result will be zero. The result will be undefined if offset or bits
        // is negative, or if the sum of offset and bits is greater than the number of bits used to
        // store the operand.
        if offset < 0 || bits <= 0 || offset + bits > 32 {
            0
        } else {
            let value = value >> offset;
            let mask = if bits == 32 { u32::MAX } else { (1 << bits) - 1 };
            value & mask
        }
    }
    fn built_in_bitfieldextract_signed_scalar(value: i32, offset: i32, bits: i32) -> i32 {
        // For unsigned data types, the most significant bits of the result will be set to zero.
        // For signed data types, the most significant bits will be set to the value of bit offset
        // + base - 1 (i.e., it is sign extended to the width of the return type).
        let unsigned = built_in_bitfieldextract_unsigned_scalar(value as u32, offset, bits);
        let sign_extended = if bits >= 32 || bits <= 0 {
            unsigned
        } else {
            let sign_bit_set = (unsigned >> (bits - 1)) & 1 != 0;
            if sign_bit_set { unsigned | ((1 << (32 - bits)) - 1) << bits } else { unsigned }
        };
        sign_extended as i32
    }
    pub fn built_in_bitfieldextract(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // bitfieldExtract(value, offset, bits) accepts a scalar or vector integer for value, and a
        // scalar signed integer for offset and bits.
        let bits = ir_meta.get_constant(operands[2]).value.get_int();
        let offset = ir_meta.get_constant(operands[1]).value.get_int();
        let value_constant_id = operands[0];

        apply_unary_componentwise(
            ir_meta,
            value_constant_id,
            result_type_id,
            |_| {
                panic!(
                    "Internal error: The bitfieldExtract() built-in only applies to [u]int types"
                )
            },
            |i| built_in_bitfieldextract_signed_scalar(i, offset, bits),
            |u| built_in_bitfieldextract_unsigned_scalar(u, offset, bits),
            |_| {
                panic!(
                    "Internal error: The bitfieldExtract() built-in only applies to [u]int types"
                )
            },
        )
    }
    fn built_in_bitfieldinsert_unsigned_scalar(
        base: u32,
        insert: u32,
        offset: i32,
        bits: i32,
    ) -> u32 {
        // If bits is zero, the result will simply be the original value of base. The result will
        // be undefined if offset or bits is negative, or if the sum of offset and bits is greater
        // than the number of bits used to store the operand.
        if bits == 0 {
            base
        } else if offset < 0 || bits < 0 || offset + bits > 32 {
            0
        } else if bits == 32 {
            // Per the above check, offset must be 0
            insert
        } else {
            // The result will have bits [offset, offset + bits - 1] taken from bits [0, bits - 1]
            // of insert, and all other bits taken directly from the corresponding bits of base.
            let mask = (1 << bits) - 1;
            base & !(mask << offset) | (insert & mask) << offset
        }
    }
    pub fn built_in_bitfieldinsert(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        // bitfieldInsert(base, insert, offset, bits) accepts matching integer scalars or vectors
        // for base and insert, and a scalar signed integer for offset and bits.
        let bits = ir_meta.get_constant(operands[3]).value.get_int();
        let offset = ir_meta.get_constant(operands[2]).value.get_int();
        let insert_constant_id = operands[1];
        let base_constant_id = operands[0];

        apply_binary_componentwise(
            ir_meta,
            base_constant_id,
            insert_constant_id,
            result_type_id,
            |_, _| {
                panic!("Internal error: The bitfieldInsert() built-in only applies to [u]int types")
            },
            |base, insert| {
                built_in_bitfieldinsert_unsigned_scalar(base as u32, insert as u32, offset, bits)
                    as i32
            },
            |base, insert| built_in_bitfieldinsert_unsigned_scalar(base, insert, offset, bits),
            |_, _| {
                panic!("Internal error: The bitfieldInsert() built-in only applies to [u]int types")
            },
        )
    }
    fn built_in_rgb_yuv_transform(r: f32, g: f32, b: f32, m: &[f32; 12]) -> [f32; 3] {
        [
            r * m[0] + g * m[3] + b * m[6] + m[9],
            r * m[1] + g * m[4] + b * m[7] + m[10],
            r * m[2] + g * m[5] + b * m[8] + m[11],
        ]
    }
    pub fn built_in_rgb_yuv_helper(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        itu601: &[f32; 12],
        itu601_full_range: &[f32; 12],
        itu709: &[f32; 12],
        result_type_id: TypeId,
    ) -> ConstantId {
        // rgb_2_yuv(color, conv_standard) takes a vec3 for color and yuvCscStandardEXT for
        // conv_standard.  It uses a mat4x3 per the given standard that is multiplied by vec4(color,
        // 1). yuv_2_rgb(color, conv_standard) operates similarly, using the inverse matrix.
        let standard = ir_meta.get_constant(operands[1]).value.get_yuv_csc();
        let color = ir_meta.get_constant(operands[0]).value.get_composite_elements();

        let b = ir_meta.get_constant(color[2]).value.get_float();
        let g = ir_meta.get_constant(color[1]).value.get_float();
        let r = ir_meta.get_constant(color[0]).value.get_float();

        // See details where these constants come from in EmulateYUVBuiltIns.cpp.
        // TODO(http://anglebug.com/349994211): update the above comment once that transformation
        // is done in the IR.
        let conversion_matrix = match standard {
            YuvCscStandard::Itu601 => itu601,
            YuvCscStandard::Itu601FullRange => itu601_full_range,
            YuvCscStandard::Itu709 => itu709,
        };

        let components = built_in_rgb_yuv_transform(r, g, b, conversion_matrix)
            .iter()
            .map(|&f| ir_meta.get_constant_float(f))
            .collect();
        ir_meta.get_constant_composite(result_type_id, components)
    }
    pub fn built_in_rgb_2_yuv(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        let itu601 = [
            0.256782, -0.148219, 0.439220, 0.504143, -0.291001, -0.367798, 0.097898, 0.439220,
            -0.071422, 0.062745, 0.501961, 0.501961,
        ];

        let itu601_full_range = [
            0.298993, -0.168732, 0.500005, 0.587016, -0.331273, -0.418699, 0.113991, 0.500005,
            -0.081306, 0.000000, 0.501961, 0.501961,
        ];

        let itu709 = [
            0.182580, -0.100641, 0.439219, 0.614243, -0.338579, -0.398950, 0.062000, 0.439219,
            -0.040269, 0.062745, 0.501961, 0.501961,
        ];

        built_in_rgb_yuv_helper(
            ir_meta,
            operands,
            &itu601,
            &itu601_full_range,
            &itu709,
            result_type_id,
        )
    }
    pub fn built_in_yuv_2_rgb(
        ir_meta: &mut IRMeta,
        operands: &[ConstantId],
        result_type_id: TypeId,
    ) -> ConstantId {
        let itu601 = [
            1.164384, 1.164384, 1.164384, 0.0, -0.391721, 2.017232, 1.596027, -0.812926, 0.0,
            -0.874202, 0.531626, -1.085631,
        ];

        let itu601_full_range = [
            1.000000, 1.000000, 1.000000, 0.000000, -0.344100, 1.772, 1.402, -0.714100, 0.000000,
            -0.703749, 0.531175, -0.889475,
        ];

        let itu709 = [
            1.164384, 1.164384, 1.164384, 0.000000, -0.213221, 2.112402, 1.792741, -0.532882,
            0.000000, -0.972945, 0.301455, -1.133402,
        ];

        built_in_rgb_yuv_helper(
            ir_meta,
            operands,
            &itu601,
            &itu601_full_range,
            &itu709,
            result_type_id,
        )
    }
    pub fn built_in(
        ir_meta: &mut IRMeta,
        op: BuiltInOpCode,
        operands: Vec<ConstantId>,
        result_type_id: TypeId,
    ) -> ConstantId {
        let fold = match op {
            BuiltInOpCode::Clamp => built_in_clamp,
            BuiltInOpCode::Mix => built_in_mix,
            BuiltInOpCode::Smoothstep => built_in_smoothstep,
            BuiltInOpCode::Fma => built_in_fma,
            BuiltInOpCode::Faceforward => built_in_faceforward,
            BuiltInOpCode::Refract => built_in_refract,
            BuiltInOpCode::BitfieldExtract => built_in_bitfieldextract,
            BuiltInOpCode::BitfieldInsert => built_in_bitfieldinsert,
            BuiltInOpCode::Rgb2Yuv => built_in_rgb_2_yuv,
            BuiltInOpCode::Yuv2Rgb => built_in_yuv_2_rgb,
            BuiltInOpCode::UaddCarry
            | BuiltInOpCode::UsubBorrow
            | BuiltInOpCode::UmulExtended
            | BuiltInOpCode::ImulExtended
            | BuiltInOpCode::TextureSize
            | BuiltInOpCode::TextureQueryLod
            | BuiltInOpCode::TexelFetch
            | BuiltInOpCode::TexelFetchOffset
            | BuiltInOpCode::AtomicCompSwap
            | BuiltInOpCode::ImageStore
            | BuiltInOpCode::ImageLoad
            | BuiltInOpCode::ImageAtomicAdd
            | BuiltInOpCode::ImageAtomicMin
            | BuiltInOpCode::ImageAtomicMax
            | BuiltInOpCode::ImageAtomicAnd
            | BuiltInOpCode::ImageAtomicOr
            | BuiltInOpCode::ImageAtomicXor
            | BuiltInOpCode::ImageAtomicExchange
            | BuiltInOpCode::ImageAtomicCompSwap
            | BuiltInOpCode::PixelLocalStoreANGLE
            | BuiltInOpCode::MemoryBarrier
            | BuiltInOpCode::MemoryBarrierAtomicCounter
            | BuiltInOpCode::MemoryBarrierBuffer
            | BuiltInOpCode::MemoryBarrierImage
            | BuiltInOpCode::Barrier
            | BuiltInOpCode::MemoryBarrierShared
            | BuiltInOpCode::GroupMemoryBarrier
            | BuiltInOpCode::EmitVertex
            | BuiltInOpCode::EndPrimitive
            | BuiltInOpCode::SubpassLoad
            | BuiltInOpCode::BeginInvocationInterlockNV
            | BuiltInOpCode::EndInvocationInterlockNV
            | BuiltInOpCode::BeginFragmentShaderOrderingINTEL
            | BuiltInOpCode::BeginInvocationInterlockARB
            | BuiltInOpCode::EndInvocationInterlockARB
            | BuiltInOpCode::NumSamples
            | BuiltInOpCode::SamplePosition
            | BuiltInOpCode::InterpolateAtCenter
            | BuiltInOpCode::LoopForwardProgress
            | BuiltInOpCode::Saturate => {
                panic!("Internal error: Unexpected built-ins to constant-fold")
            }
        };

        fold(ir_meta, &operands, result_type_id)
    }
}

// Helper functions that derive the result type of an operation.
mod promote {
    use crate::ir::*;

    // A helper to get the type id of an element being indexed.  The difference with
    // `Type::get_element_type_id` is that if the type is a `Pointer`, the result is also a
    // pointer.
    //
    // Additionally, to support swizzles, the element is optionally vectorized.
    fn get_indexed_type_id(
        ir_meta: &mut IRMeta,
        type_id: TypeId,
        vector_size: Option<u32>,
    ) -> TypeId {
        let type_info = ir_meta.get_type(type_id);
        let is_pointer = type_info.is_pointer();
        let mut element_type_id = type_info.get_element_type_id().unwrap();

        if is_pointer {
            element_type_id = ir_meta.get_type(element_type_id).get_element_type_id().unwrap();
        }

        vector_size.inspect(|&size| {
            element_type_id = ir_meta.get_vector_type_id_from_element_id(element_type_id, size)
        });

        if is_pointer {
            element_type_id = ir_meta.get_pointer_type_id(element_type_id);
        }

        element_type_id
    }
    // Used by some binary operations between scalars and vectors, returns the vector type.
    fn promote_scalar_if_necessary(ir_meta: &mut IRMeta, type1: TypeId, type2: TypeId) -> TypeId {
        if ir_meta.get_type(type1).is_scalar() { type2 } else { type1 }
    }
    // Used by increment/decrement operations which take a pointer by produce a register.
    fn dereference(ir_meta: &IRMeta, pointer_type: TypeId) -> TypeId {
        ir_meta.get_type(pointer_type).get_element_type_id().unwrap()
    }

    // The type of `vector.x` derived from the type of `vector`, used for AccessVectorComponent and
    // ExtractVectorComponent.
    pub fn vector_component(ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        get_indexed_type_id(ir_meta, operand_type, None)
    }

    // The type of `vector.xy` derived from the type of `vector`, used for
    // AccessVectorComponentMulti and ExtractVectorComponentMulti.
    pub fn vector_component_multi(
        ir_meta: &mut IRMeta,
        operand_type: TypeId,
        vector_size: u32,
    ) -> TypeId {
        get_indexed_type_id(ir_meta, operand_type, Some(vector_size))
    }

    // The type of `a[i]` derived from the type of `a`, used for AccessVectorComponentDynamic,
    // AccessMatrixColumn, AccessArrayElement, ExtractVectorComponentDynamic, ExtractMatrixColumn,
    // and ExtractArrayElement.
    pub fn index(ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        get_indexed_type_id(ir_meta, operand_type, None)
    }

    // The type of `strct.field` derived from the type of `strct` and the selected field, used for
    // AccessStructField and ExtractStructField,
    pub fn struct_field(ir_meta: &mut IRMeta, type_id: TypeId, field: u32) -> TypeId {
        let mut type_info = ir_meta.get_type(type_id);
        let is_pointer = type_info.is_pointer();
        if is_pointer {
            type_info = ir_meta.get_type(type_info.get_element_type_id().unwrap());
        }

        let mut field_type_id = type_info.get_struct_field(field).type_id;

        if is_pointer {
            field_type_id = ir_meta.get_pointer_type_id(field_type_id);
        }

        field_type_id
    }

    // The result type of Negate and BitwiseNot is the same as their operand.
    pub fn negate(_ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        operand_type
    }
    pub fn bitwise_not(_ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        operand_type
    }
    // The result type of post/prefix increment/decrement is the same as their operand, except it
    // is dereferenced.
    pub fn postfix_increment(ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        dereference(ir_meta, operand_type)
    }
    pub fn postfix_decrement(ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        dereference(ir_meta, operand_type)
    }
    pub fn prefix_increment(ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        dereference(ir_meta, operand_type)
    }
    pub fn prefix_decrement(ir_meta: &mut IRMeta, operand_type: TypeId) -> TypeId {
        dereference(ir_meta, operand_type)
    }

    // Add, Sub, Mul, Div, IMod, BitwiseOr, BitwiseAnd, and BitwiseXor implicitly promote a scalar
    // operand to vector if they other operand is a vector.
    pub fn add(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn sub(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn mul(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn div(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn imod(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn bitwise_or(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn bitwise_and(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }
    pub fn bitwise_xor(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        promote_scalar_if_necessary(ir_meta, lhs_type, rhs_type)
    }

    // VectorTimesScalar and MatrixTimesScalar apply the scalar to each component, and the result
    // is identical to the vector/matrix itself.
    pub fn vector_times_scalar(
        _ir_meta: &mut IRMeta,
        lhs_type: TypeId,
        _rhs_type: TypeId,
    ) -> TypeId {
        lhs_type
    }
    pub fn matrix_times_scalar(
        _ir_meta: &mut IRMeta,
        lhs_type: TypeId,
        _rhs_type: TypeId,
    ) -> TypeId {
        lhs_type
    }

    // For VectorTimesMatrix, the result type is a vector of the matrix's row count.
    pub fn vector_times_matrix(
        ir_meta: &mut IRMeta,
        _lhs_type: TypeId,
        rhs_type: TypeId,
    ) -> TypeId {
        let size = ir_meta.get_type(rhs_type).get_matrix_size().unwrap();
        ir_meta.get_vector_type_id(BasicType::Float, size)
    }
    // For MatrixTimesVector, the result type is a vector of the matrix's column count.
    pub fn matrix_times_vector(
        ir_meta: &mut IRMeta,
        lhs_type: TypeId,
        _rhs_type: TypeId,
    ) -> TypeId {
        ir_meta.get_type(lhs_type).get_element_type_id().unwrap()
    }
    // For MatrixTimesMatrix, the result type is a matrix with rhs's column count of lhs's column
    // type.
    pub fn matrix_times_matrix(ir_meta: &mut IRMeta, lhs_type: TypeId, rhs_type: TypeId) -> TypeId {
        let result_column_type_id = ir_meta.get_type(lhs_type).get_element_type_id().unwrap();
        let result_row_count = ir_meta.get_type(result_column_type_id).get_vector_size().unwrap();
        let result_column_count = ir_meta.get_type(rhs_type).get_matrix_size().unwrap();
        ir_meta.get_matrix_type_id(result_column_count, result_row_count)
    }

    // BitShiftLeft and BitShiftRight produce a result with the same type as the value being
    // shifted.
    pub fn bit_shift_left(_ir_meta: &mut IRMeta, lhs_type: TypeId, _rhs_type: TypeId) -> TypeId {
        lhs_type
    }
    pub fn bit_shift_right(_ir_meta: &mut IRMeta, lhs_type: TypeId, _rhs_type: TypeId) -> TypeId {
        lhs_type
    }

    // Used to create a scalar or vector based on vector parameters
    fn change_base_type(
        ir_meta: &mut IRMeta,
        original_type_id: TypeId,
        new_basic_type: BasicType,
    ) -> TypeId {
        let vec_size = ir_meta.get_type(original_type_id).get_vector_size();
        match vec_size {
            Some(n) => ir_meta.get_vector_type_id(new_basic_type, n),
            None => ir_meta.get_basic_type_id(new_basic_type),
        }
    }
    // Used to swap row and column size of a matrix
    fn get_transposed_type(ir_meta: &mut IRMeta, mat_type_id: TypeId) -> TypeId {
        let mat_type = ir_meta.get_type(mat_type_id);
        let column_count = mat_type.get_matrix_size().unwrap();
        let column_type_id = mat_type.get_element_type_id().unwrap();
        let row_count = ir_meta.get_type(column_type_id).get_vector_size().unwrap();

        ir_meta.get_matrix_type_id(row_count, column_count)
    }
    // Used to create a type based on the number of dimensions in an image.  Both imageSize and
    // textureSize return int or ivecN, so a base type of Int is unconditionally used.
    fn get_image_size_type(ir_meta: &mut IRMeta, image_type_id: TypeId) -> TypeId {
        let image_type = ir_meta.get_type(image_type_id);
        let (_, image_type) = image_type.get_image_type();
        let is_array = image_type.is_array;
        let dimensions = match image_type.dimension {
            ImageDimension::D2
            | ImageDimension::Cube
            | ImageDimension::Rect
            | ImageDimension::External
            | ImageDimension::ExternalY2Y
            | ImageDimension::Video
            | ImageDimension::PixelLocal
            | ImageDimension::Subpass => {
                if is_array {
                    3
                } else {
                    2
                }
            }
            ImageDimension::D3 => 3,
            ImageDimension::Buffer => 1,
        };

        if dimensions == 1 {
            TYPE_ID_INT
        } else {
            ir_meta.get_vector_type_id(BasicType::Int, dimensions)
        }
    }
    // Used to create the gvec4 type that results from reading from an image (with imageLoad,
    // texture*, pixelLocalLoadANGLE).
    fn get_image_read_type(ir_meta: &mut IRMeta, image_type_id: TypeId) -> TypeId {
        let image_basic_type = ir_meta.get_type(image_type_id).get_image_type().0;
        match image_basic_type {
            ImageBasicType::Float => TYPE_ID_VEC4,
            ImageBasicType::Int => TYPE_ID_IVEC4,
            ImageBasicType::Uint => TYPE_ID_UVEC4,
        }
    }
    pub fn built_in_unary(ir_meta: &mut IRMeta, op: UnaryOpCode, operand_type: TypeId) -> TypeId {
        match op {
            UnaryOpCode::Radians
            | UnaryOpCode::Degrees
            | UnaryOpCode::Sin
            | UnaryOpCode::Cos
            | UnaryOpCode::Tan
            | UnaryOpCode::Asin
            | UnaryOpCode::Acos
            | UnaryOpCode::Atan
            | UnaryOpCode::Sinh
            | UnaryOpCode::Cosh
            | UnaryOpCode::Tanh
            | UnaryOpCode::Asinh
            | UnaryOpCode::Acosh
            | UnaryOpCode::Atanh
            | UnaryOpCode::Exp
            | UnaryOpCode::Log
            | UnaryOpCode::Exp2
            | UnaryOpCode::Log2
            | UnaryOpCode::Sqrt
            | UnaryOpCode::Inversesqrt
            | UnaryOpCode::Abs
            | UnaryOpCode::Sign
            | UnaryOpCode::Floor
            | UnaryOpCode::Trunc
            | UnaryOpCode::Round
            | UnaryOpCode::RoundEven
            | UnaryOpCode::Ceil
            | UnaryOpCode::Fract
            | UnaryOpCode::Normalize
            | UnaryOpCode::Inverse
            | UnaryOpCode::Not
            | UnaryOpCode::BitfieldReverse
            | UnaryOpCode::DFdx
            | UnaryOpCode::DFdy
            | UnaryOpCode::Fwidth
            | UnaryOpCode::InterpolateAtCentroid => operand_type,
            UnaryOpCode::FloatBitsToInt => change_base_type(ir_meta, operand_type, BasicType::Int),
            UnaryOpCode::FloatBitsToUint => {
                change_base_type(ir_meta, operand_type, BasicType::Uint)
            }
            UnaryOpCode::IntBitsToFloat | UnaryOpCode::UintBitsToFloat => {
                change_base_type(ir_meta, operand_type, BasicType::Float)
            }
            UnaryOpCode::PackUnorm2x16
            | UnaryOpCode::PackSnorm2x16
            | UnaryOpCode::PackHalf2x16
            | UnaryOpCode::PackUnorm4x8
            | UnaryOpCode::PackSnorm4x8 => TYPE_ID_UINT,
            UnaryOpCode::UnpackSnorm2x16
            | UnaryOpCode::UnpackHalf2x16
            | UnaryOpCode::UnpackUnorm2x16 => TYPE_ID_VEC2,
            UnaryOpCode::UnpackUnorm4x8 | UnaryOpCode::UnpackSnorm4x8 => TYPE_ID_VEC4,
            UnaryOpCode::Length | UnaryOpCode::Determinant => TYPE_ID_FLOAT,
            UnaryOpCode::Transpose => get_transposed_type(ir_meta, operand_type),
            UnaryOpCode::Isnan | UnaryOpCode::Isinf | UnaryOpCode::Any | UnaryOpCode::All => {
                TYPE_ID_BOOL
            }
            UnaryOpCode::BitCount | UnaryOpCode::FindLSB | UnaryOpCode::FindMSB => {
                change_base_type(ir_meta, operand_type, BasicType::Int)
            }
            UnaryOpCode::AtomicCounter
            | UnaryOpCode::AtomicCounterIncrement
            | UnaryOpCode::AtomicCounterDecrement => TYPE_ID_UINT,
            UnaryOpCode::ImageSize => get_image_size_type(ir_meta, operand_type),
            UnaryOpCode::PixelLocalLoadANGLE => get_image_read_type(ir_meta, operand_type),
            _ => panic!("Internal error: Unexpected unary op is not a built-in"),
        }
    }

    // Used to get the matrix type that's the result of outerProduct().
    // For outerProduct(vecM, vecN) the result is matNxM.
    fn get_outerproduct_type(
        ir_meta: &mut IRMeta,
        lhs_type_id: TypeId,
        rhs_type_id: TypeId,
    ) -> TypeId {
        let column_count = ir_meta.get_type(rhs_type_id).get_vector_size().unwrap();
        let row_count = ir_meta.get_type(lhs_type_id).get_vector_size().unwrap();

        ir_meta.get_matrix_type_id(column_count, row_count)
    }
    pub fn built_in_binary(
        ir_meta: &mut IRMeta,
        op: BinaryOpCode,
        lhs_type: TypeId,
        rhs_type: TypeId,
    ) -> TypeId {
        match op {
            BinaryOpCode::Atan
            | BinaryOpCode::Pow
            | BinaryOpCode::Mod
            | BinaryOpCode::Min
            | BinaryOpCode::Max
            | BinaryOpCode::Modf
            | BinaryOpCode::Frexp
            | BinaryOpCode::Ldexp
            | BinaryOpCode::Cross
            | BinaryOpCode::Reflect
            | BinaryOpCode::MatrixCompMult
            | BinaryOpCode::InterpolateAtSample
            | BinaryOpCode::InterpolateAtOffset => lhs_type,
            BinaryOpCode::Step
            | BinaryOpCode::AtomicAdd
            | BinaryOpCode::AtomicMin
            | BinaryOpCode::AtomicMax
            | BinaryOpCode::AtomicAnd
            | BinaryOpCode::AtomicOr
            | BinaryOpCode::AtomicXor
            | BinaryOpCode::AtomicExchange => rhs_type,
            BinaryOpCode::Distance | BinaryOpCode::Dot => TYPE_ID_FLOAT,
            BinaryOpCode::OuterProduct => get_outerproduct_type(ir_meta, lhs_type, rhs_type),
            BinaryOpCode::LessThanVec
            | BinaryOpCode::LessThanEqualVec
            | BinaryOpCode::GreaterThanVec
            | BinaryOpCode::GreaterThanEqualVec
            | BinaryOpCode::EqualVec
            | BinaryOpCode::NotEqualVec => change_base_type(ir_meta, lhs_type, BasicType::Bool),
            _ => panic!("Internal error: Unexpected binary op is not a built-in"),
        }
    }

    pub fn built_in(
        ir_meta: &mut IRMeta,
        op: BuiltInOpCode,
        operand_types: &mut impl Iterator<Item = TypeId>,
    ) -> TypeId {
        match op {
            BuiltInOpCode::Clamp
            | BuiltInOpCode::Mix
            | BuiltInOpCode::Fma
            | BuiltInOpCode::Faceforward
            | BuiltInOpCode::Refract
            | BuiltInOpCode::BitfieldExtract
            | BuiltInOpCode::BitfieldInsert
            | BuiltInOpCode::UaddCarry
            | BuiltInOpCode::UsubBorrow
            | BuiltInOpCode::InterpolateAtCenter
            | BuiltInOpCode::Saturate => operand_types.next().unwrap(),
            BuiltInOpCode::Smoothstep
            | BuiltInOpCode::AtomicCompSwap
            | BuiltInOpCode::ImageAtomicAdd
            | BuiltInOpCode::ImageAtomicMin
            | BuiltInOpCode::ImageAtomicMax
            | BuiltInOpCode::ImageAtomicAnd
            | BuiltInOpCode::ImageAtomicOr
            | BuiltInOpCode::ImageAtomicXor
            | BuiltInOpCode::ImageAtomicExchange
            | BuiltInOpCode::ImageAtomicCompSwap => operand_types.last().unwrap(),
            BuiltInOpCode::UmulExtended
            | BuiltInOpCode::ImulExtended
            | BuiltInOpCode::ImageStore
            | BuiltInOpCode::PixelLocalStoreANGLE
            | BuiltInOpCode::MemoryBarrier
            | BuiltInOpCode::MemoryBarrierAtomicCounter
            | BuiltInOpCode::MemoryBarrierBuffer
            | BuiltInOpCode::MemoryBarrierImage
            | BuiltInOpCode::Barrier
            | BuiltInOpCode::MemoryBarrierShared
            | BuiltInOpCode::GroupMemoryBarrier
            | BuiltInOpCode::EmitVertex
            | BuiltInOpCode::EndPrimitive
            | BuiltInOpCode::BeginInvocationInterlockNV
            | BuiltInOpCode::EndInvocationInterlockNV
            | BuiltInOpCode::BeginFragmentShaderOrderingINTEL
            | BuiltInOpCode::BeginInvocationInterlockARB
            | BuiltInOpCode::EndInvocationInterlockARB
            | BuiltInOpCode::LoopForwardProgress => TYPE_ID_VOID,
            BuiltInOpCode::TextureSize => {
                get_image_size_type(ir_meta, operand_types.next().unwrap())
            }
            BuiltInOpCode::TextureQueryLod => TYPE_ID_VEC2,
            BuiltInOpCode::TexelFetch
            | BuiltInOpCode::TexelFetchOffset
            | BuiltInOpCode::ImageLoad
            | BuiltInOpCode::SubpassLoad => {
                get_image_read_type(ir_meta, operand_types.next().unwrap())
            }
            BuiltInOpCode::Rgb2Yuv | BuiltInOpCode::Yuv2Rgb => TYPE_ID_VEC3,
            BuiltInOpCode::NumSamples => TYPE_ID_UINT,
            BuiltInOpCode::SamplePosition => TYPE_ID_VEC2,
        }
    }

    pub fn built_in_texture(ir_meta: &mut IRMeta, op: &TextureOpCode, sampler: TypeId) -> TypeId {
        // The result of sampling from a texture is always gvec4, except for shadow samplers, in
        // which case it's vec4 for textureGather* and float otherwise.
        if matches!(op, TextureOpCode::GatherRef { .. }) {
            // textureGather* with shadow samplers always takes a refZ parameter.
            TYPE_ID_VEC4
        } else {
            let image_type = ir_meta.get_type(sampler).get_image_type().1;
            if image_type.is_shadow { TYPE_ID_FLOAT } else { get_image_read_type(ir_meta, sampler) }
        }
    }
}

// Helper functions that derive the precision of an operation.
pub mod precision {
    use crate::ir::*;
    use crate::*;

    // Taking some precision and comparing it with another:
    //
    // * Upgrade to highp is either no-op or an upgrade, so it can unconditionally be done.
    // * Upgrade to mediump is only necessary if the original precision was lowp.
    // * Upgrade to lowp is never necessary.
    //
    // If either precision is NotApplicable, use the other one.  This can happen with constants as
    // they don't have a precision.
    pub fn higher_precision(one: Precision, other: Precision) -> Precision {
        match one {
            Precision::High => Precision::High,
            Precision::Medium if other == Precision::Low => Precision::Medium,
            _ => {
                // The other's precision is at least as high as this one, unless it doesn't have a
                // precision at all.
                if other == Precision::NotApplicable { one } else { other }
            }
        }
    }

    // Take a set of precisions and return the highest per `higher_precision`.
    fn highest_precision(precisions: &mut impl Iterator<Item = Precision>) -> Precision {
        precisions.reduce(higher_precision).unwrap()
    }

    // Constructor precision is derived from its parameters, except for structs.
    pub fn construct(
        ir_meta: &IRMeta,
        type_id: TypeId,
        args: &mut impl Iterator<Item = Precision>,
    ) -> Precision {
        if !util::is_precision_applicable_to_type(ir_meta, type_id) {
            Precision::NotApplicable
        } else {
            args.fold(Precision::NotApplicable, |accumulator, precision| {
                higher_precision(accumulator, precision)
            })
        }
    }

    // Array length is usually a constant and therefore has no precision.  If it is called on an
    // unsized array at the end of an SSBO, it's highp.
    pub fn array_length() -> Precision {
        // Note: ANGLE has always treated length() as highp, but the spec does not seem to mandate
        // that.  Rather, it seems the precision is unspecified and must be derived from
        // neighboring operations, although it is unclear whether the following is specifically
        // referencing written as such because length() typically returns a constant or not.
        //
        // > The precision is determined using the same rules as for other cases where there is no
        // > intrinsic precision.
        Precision::High
    }

    // The result of Negate, post/prefix increment/decrement and BitwiseNot has the same precision
    // as the operand.
    pub fn negate(operand: Precision) -> Precision {
        operand
    }
    pub fn bitwise_not(operand: Precision) -> Precision {
        operand
    }
    pub fn postfix_increment(operand: Precision) -> Precision {
        operand
    }
    pub fn postfix_decrement(operand: Precision) -> Precision {
        operand
    }
    pub fn prefix_increment(operand: Precision) -> Precision {
        operand
    }
    pub fn prefix_decrement(operand: Precision) -> Precision {
        operand
    }

    // The result of most binary operations is the higher of the operands.
    pub fn add(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn sub(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn mul(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn div(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn imod(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn vector_times_scalar(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn matrix_times_scalar(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn vector_times_matrix(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn matrix_times_vector(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn matrix_times_matrix(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn bitwise_or(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn bitwise_xor(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }
    pub fn bitwise_and(lhs: Precision, rhs: Precision) -> Precision {
        higher_precision(lhs, rhs)
    }

    // Shift operations have the same precision in the result as the value being shifted.
    pub fn bit_shift_left(lhs: Precision, _rhs: Precision) -> Precision {
        lhs
    }
    pub fn bit_shift_right(lhs: Precision, _rhs: Precision) -> Precision {
        lhs
    }

    // Operations that produce bool have no precision.
    pub fn logical_not(_operand: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn logical_xor(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn equal(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn not_equal(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn less_than(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn greater_than(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn less_than_equal(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }
    pub fn greater_than_equal(_lhs: Precision, _rhs: Precision) -> Precision {
        Precision::NotApplicable
    }

    pub fn built_in_unary(op: UnaryOpCode, operand: Precision) -> Precision {
        match op {
            UnaryOpCode::FloatBitsToInt
            | UnaryOpCode::FloatBitsToUint
            | UnaryOpCode::IntBitsToFloat
            | UnaryOpCode::UintBitsToFloat
            | UnaryOpCode::PackUnorm2x16
            | UnaryOpCode::PackSnorm2x16
            | UnaryOpCode::PackHalf2x16
            | UnaryOpCode::PackUnorm4x8
            | UnaryOpCode::PackSnorm4x8
            | UnaryOpCode::UnpackSnorm2x16
            | UnaryOpCode::UnpackUnorm2x16
            | UnaryOpCode::BitfieldReverse
            | UnaryOpCode::AtomicCounter
            | UnaryOpCode::AtomicCounterIncrement
            | UnaryOpCode::AtomicCounterDecrement
            | UnaryOpCode::ImageSize => Precision::High,

            UnaryOpCode::UnpackHalf2x16
            | UnaryOpCode::UnpackUnorm4x8
            | UnaryOpCode::UnpackSnorm4x8 => Precision::Medium,

            UnaryOpCode::BitCount | UnaryOpCode::FindLSB | UnaryOpCode::FindMSB => Precision::Low,

            UnaryOpCode::Isnan
            | UnaryOpCode::Isinf
            | UnaryOpCode::Any
            | UnaryOpCode::All
            | UnaryOpCode::Not => Precision::NotApplicable,

            UnaryOpCode::Radians
            | UnaryOpCode::Degrees
            | UnaryOpCode::Sin
            | UnaryOpCode::Cos
            | UnaryOpCode::Tan
            | UnaryOpCode::Asin
            | UnaryOpCode::Acos
            | UnaryOpCode::Atan
            | UnaryOpCode::Sinh
            | UnaryOpCode::Cosh
            | UnaryOpCode::Tanh
            | UnaryOpCode::Asinh
            | UnaryOpCode::Acosh
            | UnaryOpCode::Atanh
            | UnaryOpCode::Exp
            | UnaryOpCode::Log
            | UnaryOpCode::Exp2
            | UnaryOpCode::Log2
            | UnaryOpCode::Sqrt
            | UnaryOpCode::Inversesqrt
            | UnaryOpCode::Abs
            | UnaryOpCode::Sign
            | UnaryOpCode::Floor
            | UnaryOpCode::Trunc
            | UnaryOpCode::Round
            | UnaryOpCode::RoundEven
            | UnaryOpCode::Ceil
            | UnaryOpCode::Fract
            | UnaryOpCode::Length
            | UnaryOpCode::Normalize
            | UnaryOpCode::Transpose
            | UnaryOpCode::Determinant
            | UnaryOpCode::Inverse
            | UnaryOpCode::DFdx
            | UnaryOpCode::DFdy
            | UnaryOpCode::Fwidth
            | UnaryOpCode::InterpolateAtCentroid
            | UnaryOpCode::PixelLocalLoadANGLE => operand,
            _ => panic!("Internal error: Unexpected unary op is not a built-in"),
        }
    }

    pub fn built_in_binary(op: BinaryOpCode, lhs: Precision, rhs: Precision) -> Precision {
        match op {
            BinaryOpCode::Frexp
            | BinaryOpCode::Ldexp
            | BinaryOpCode::AtomicAdd
            | BinaryOpCode::AtomicMin
            | BinaryOpCode::AtomicMax
            | BinaryOpCode::AtomicAnd
            | BinaryOpCode::AtomicOr
            | BinaryOpCode::AtomicXor
            | BinaryOpCode::AtomicExchange => Precision::High,

            BinaryOpCode::InterpolateAtSample | BinaryOpCode::InterpolateAtOffset => lhs,

            BinaryOpCode::LessThanVec
            | BinaryOpCode::LessThanEqualVec
            | BinaryOpCode::GreaterThanVec
            | BinaryOpCode::GreaterThanEqualVec
            | BinaryOpCode::EqualVec
            | BinaryOpCode::NotEqualVec => Precision::NotApplicable,

            BinaryOpCode::Atan
            | BinaryOpCode::Pow
            | BinaryOpCode::Mod
            | BinaryOpCode::Min
            | BinaryOpCode::Max
            | BinaryOpCode::Step
            | BinaryOpCode::Modf
            | BinaryOpCode::Distance
            | BinaryOpCode::Dot
            | BinaryOpCode::Cross
            | BinaryOpCode::Reflect
            | BinaryOpCode::MatrixCompMult
            | BinaryOpCode::OuterProduct => higher_precision(lhs, rhs),
            _ => panic!("Internal error: Unexpected binary op is not a built-in"),
        }
    }

    pub fn built_in(
        op: BuiltInOpCode,
        operands: &mut impl Iterator<Item = Precision>,
    ) -> Precision {
        match op {
            BuiltInOpCode::TextureSize
            | BuiltInOpCode::UaddCarry
            | BuiltInOpCode::UsubBorrow
            | BuiltInOpCode::AtomicCompSwap
            | BuiltInOpCode::ImageAtomicAdd
            | BuiltInOpCode::ImageAtomicMin
            | BuiltInOpCode::ImageAtomicMax
            | BuiltInOpCode::ImageAtomicAnd
            | BuiltInOpCode::ImageAtomicOr
            | BuiltInOpCode::ImageAtomicXor
            | BuiltInOpCode::ImageAtomicExchange
            | BuiltInOpCode::ImageAtomicCompSwap
            | BuiltInOpCode::NumSamples
            | BuiltInOpCode::SamplePosition => Precision::High,

            BuiltInOpCode::BitfieldExtract
            | BuiltInOpCode::BitfieldInsert
            | BuiltInOpCode::InterpolateAtCenter
            | BuiltInOpCode::TextureQueryLod
            | BuiltInOpCode::TexelFetch
            | BuiltInOpCode::TexelFetchOffset
            | BuiltInOpCode::ImageLoad
            | BuiltInOpCode::SubpassLoad => operands.next().unwrap(),

            BuiltInOpCode::Clamp
            | BuiltInOpCode::Mix
            | BuiltInOpCode::Smoothstep
            | BuiltInOpCode::Fma
            | BuiltInOpCode::Faceforward
            | BuiltInOpCode::Refract
            | BuiltInOpCode::Rgb2Yuv
            | BuiltInOpCode::Yuv2Rgb
            | BuiltInOpCode::Saturate => highest_precision(operands),

            BuiltInOpCode::UmulExtended
            | BuiltInOpCode::ImulExtended
            | BuiltInOpCode::ImageStore
            | BuiltInOpCode::PixelLocalStoreANGLE
            | BuiltInOpCode::MemoryBarrier
            | BuiltInOpCode::MemoryBarrierAtomicCounter
            | BuiltInOpCode::MemoryBarrierBuffer
            | BuiltInOpCode::MemoryBarrierImage
            | BuiltInOpCode::Barrier
            | BuiltInOpCode::MemoryBarrierShared
            | BuiltInOpCode::GroupMemoryBarrier
            | BuiltInOpCode::EmitVertex
            | BuiltInOpCode::EndPrimitive
            | BuiltInOpCode::BeginInvocationInterlockNV
            | BuiltInOpCode::EndInvocationInterlockNV
            | BuiltInOpCode::BeginFragmentShaderOrderingINTEL
            | BuiltInOpCode::BeginInvocationInterlockARB
            | BuiltInOpCode::EndInvocationInterlockARB
            | BuiltInOpCode::LoopForwardProgress => {
                panic!("Internal error: invalid to derive precision of void built-in")
            }
        }
    }

    pub fn built_in_texture(_op: &TextureOpCode, sampler: Precision) -> Precision {
        // Result of texture sampling always has the same precision as the sampler.
        sampler
    }

    // A helper that does the opposite of the above; it takes an instruction, and looks at the
    // operands that don't already have a precision (but should such as constants, or registers
    // derived from constants).  In the case of constants, the precision is immediately fixed.  For
    // registers, they are added to a list so the caller can continue processing them.
    pub fn propagate_to_id(
        id: &mut TypedId,
        precision: Precision,
        to_propagate: &mut Vec<(RegisterId, Precision)>,
    ) {
        if id.precision == Precision::NotApplicable && precision != Precision::NotApplicable {
            match id.id {
                Id::Register(register_id) => {
                    id.precision = precision;
                    to_propagate.push((register_id, precision));
                }
                Id::Constant(_) => id.precision = precision,
                Id::Variable(_) => {
                    panic!("Internal error: Unexpected need to propagate precision to a variable")
                }
            }
        }
    }
    pub fn propagate_to_ids(
        ids: &mut std::slice::IterMut<'_, TypedId>,
        precision: Precision,
        to_propagate: &mut Vec<(RegisterId, Precision)>,
    ) {
        ids.for_each(|id| {
            propagate_to_id(id, precision, to_propagate);
        });
    }
    pub fn propagate(
        instruction: &mut Instruction,
        function_arg_precisions: &std::collections::HashMap<FunctionId, Vec<Precision>>,
        struct_member_precisions: &std::collections::HashMap<TypeId, Vec<Precision>>,
        to_propagate: &mut Vec<(RegisterId, Precision)>,
    ) {
        let precision = instruction.result.precision;

        let propagate_by_matching_precision =
            |ids: &mut std::slice::IterMut<'_, TypedId>,
             to_match: &[Precision],
             to_propagate: &mut Vec<(RegisterId, Precision)>| {
                ids.zip(to_match.iter()).for_each(|(id, &expect)| {
                    propagate_to_id(id, expect, to_propagate);
                });
            };

        match instruction.op {
            OpCode::ExtractVectorComponentDynamic(ref mut indexed, ref mut index)
            | OpCode::ExtractMatrixColumn(ref mut indexed, ref mut index)
            | OpCode::ExtractArrayElement(ref mut indexed, ref mut index) => {
                propagate_to_id(indexed, precision, to_propagate);
                // For constant indices, always apply highp
                propagate_to_id(index, Precision::High, to_propagate);
            }
            // Pointers cannot have been derived from constants (and be without precision).
            OpCode::AccessVectorComponentDynamic(_, ref mut index)
            | OpCode::AccessMatrixColumn(_, ref mut index)
            | OpCode::AccessArrayElement(_, ref mut index) => {
                // For constant indices, always apply highp
                propagate_to_id(index, Precision::High, to_propagate);
            }
            OpCode::ExtractVectorComponent(ref mut indexed, _)
            | OpCode::ExtractVectorComponentMulti(ref mut indexed, _)
            | OpCode::ExtractStructField(ref mut indexed, _) => {
                propagate_to_id(indexed, precision, to_propagate);
            }
            // For constructors, propagate precision to arguments.
            OpCode::ConstructScalarFromScalar(ref mut arg)
            | OpCode::ConstructVectorFromScalar(ref mut arg)
            | OpCode::ConstructMatrixFromScalar(ref mut arg)
            | OpCode::ConstructMatrixFromMatrix(ref mut arg) => {
                propagate_to_id(arg, precision, to_propagate);
            }
            OpCode::ConstructVectorFromMultiple(ref mut args)
            | OpCode::ConstructMatrixFromMultiple(ref mut args)
            | OpCode::ConstructArray(ref mut args) => {
                propagate_to_ids(&mut args.iter_mut(), precision, to_propagate);
            }
            // For function calls, propagate precision of each parameter to its corresponding
            // argument.
            OpCode::Call(function_id, ref mut args) => {
                propagate_by_matching_precision(
                    &mut args.iter_mut(),
                    &function_arg_precisions[&function_id],
                    to_propagate,
                );
            }
            // For struct constructors, propagate precision of each member to its corresponding
            // argument.
            OpCode::ConstructStruct(ref mut args) => {
                propagate_by_matching_precision(
                    &mut args.iter_mut(),
                    &struct_member_precisions[&instruction.result.type_id],
                    to_propagate,
                );
            }
            OpCode::Alias(ref mut alias_id) => {
                propagate_to_id(alias_id, precision, to_propagate);
            }
            // Pointers cannot have been derived from constants (and be without precision).
            OpCode::AccessVectorComponent(..)
            | OpCode::AccessVectorComponentMulti(..)
            | OpCode::AccessStructField(..)
            | OpCode::Load(..) => {}

            OpCode::Unary(op, ref mut operand) => {
                match op {
                    UnaryOpCode::ArrayLength => {
                        // Precision of result does not affect the operand in any way.  For now,
                        // don't assign any precision to the argument; it may not be applicable to
                        // it, e.g. if it's a bool array.
                    },
                    // Precision of result does not affect the operand in any way, assume highp as
                    // otherwise the constant cannot take precision from anywhere else:
                    UnaryOpCode::PackSnorm2x16 |
                    UnaryOpCode::PackHalf2x16 |
                    UnaryOpCode::PackUnorm2x16 |
                    UnaryOpCode::PackUnorm4x8 |
                    UnaryOpCode::PackSnorm4x8 |
                    UnaryOpCode::BitCount |
                    UnaryOpCode::FindMSB |
                    UnaryOpCode::FindLSB |
                    UnaryOpCode::Isnan |
                    UnaryOpCode::Isinf |
                    // Arguments of these built-ins are always highp
                    UnaryOpCode::FloatBitsToInt |
                    UnaryOpCode::FloatBitsToUint |
                    UnaryOpCode::IntBitsToFloat |
                    UnaryOpCode::UintBitsToFloat |
                    UnaryOpCode::UnpackSnorm2x16 |
                    UnaryOpCode::UnpackHalf2x16 |
                    UnaryOpCode::UnpackUnorm2x16 |
                    UnaryOpCode::UnpackUnorm4x8 |
                    UnaryOpCode::UnpackSnorm4x8 |
                    UnaryOpCode::BitfieldReverse => {
                        propagate_to_id(operand, Precision::High, to_propagate);
                    },
                    _ => {
                        propagate_to_id(operand, precision, to_propagate);
                    }
                };
            }
            OpCode::Binary(op, ref mut lhs, ref mut rhs) => {
                match op {
                    BinaryOpCode::Add
                    | BinaryOpCode::Sub
                    | BinaryOpCode::Mul
                    | BinaryOpCode::VectorTimesScalar
                    | BinaryOpCode::MatrixTimesScalar
                    | BinaryOpCode::VectorTimesMatrix
                    | BinaryOpCode::MatrixTimesVector
                    | BinaryOpCode::MatrixTimesMatrix
                    | BinaryOpCode::Div
                    | BinaryOpCode::IMod
                    | BinaryOpCode::BitShiftLeft
                    | BinaryOpCode::BitShiftRight
                    | BinaryOpCode::BitwiseOr
                    | BinaryOpCode::BitwiseXor
                    | BinaryOpCode::BitwiseAnd
                    | BinaryOpCode::Atan
                    | BinaryOpCode::Pow
                    | BinaryOpCode::Mod
                    | BinaryOpCode::Min
                    | BinaryOpCode::Max
                    | BinaryOpCode::Step
                    | BinaryOpCode::Modf
                    | BinaryOpCode::Distance
                    | BinaryOpCode::Dot
                    | BinaryOpCode::Cross
                    | BinaryOpCode::Reflect
                    | BinaryOpCode::MatrixCompMult
                    | BinaryOpCode::OuterProduct
                    | BinaryOpCode::InterpolateAtOffset => {
                        propagate_to_id(lhs, precision, to_propagate);
                        propagate_to_id(rhs, precision, to_propagate);
                    }
                    BinaryOpCode::LogicalXor => {
                        // Parameters are boolean, so there are no precisions.
                    }
                    BinaryOpCode::Frexp | BinaryOpCode::Ldexp => {
                        // Arguments of frexp and ldexp are always highp.
                        propagate_to_id(lhs, Precision::High, to_propagate);
                        propagate_to_id(rhs, Precision::High, to_propagate);
                    }
                    BinaryOpCode::Equal
                    | BinaryOpCode::NotEqual
                    | BinaryOpCode::LessThan
                    | BinaryOpCode::GreaterThan
                    | BinaryOpCode::LessThanEqual
                    | BinaryOpCode::GreaterThanEqual
                    | BinaryOpCode::LessThanVec
                    | BinaryOpCode::LessThanEqualVec
                    | BinaryOpCode::GreaterThanVec
                    | BinaryOpCode::GreaterThanEqualVec
                    | BinaryOpCode::EqualVec
                    | BinaryOpCode::NotEqualVec => {
                        // Apply precision from one operand to the other, as the result does not
                        // have a precision.  If neither has a precision, precision cannot be
                        // derived, AST generation will assume highp.
                        if lhs.precision != Precision::NotApplicable {
                            propagate_to_id(rhs, lhs.precision, to_propagate);
                        } else {
                            propagate_to_id(lhs, rhs.precision, to_propagate);
                        }
                    }

                    BinaryOpCode::InterpolateAtSample => {
                        // Precision is only applicable to the lhs.  Assume highp for the sample
                        // parameter, which otherwise cannot get a precision from anywhere.
                        propagate_to_id(lhs, precision, to_propagate);
                        propagate_to_id(rhs, Precision::High, to_propagate);
                    }

                    BinaryOpCode::AtomicAdd
                    | BinaryOpCode::AtomicMin
                    | BinaryOpCode::AtomicMax
                    | BinaryOpCode::AtomicAnd
                    | BinaryOpCode::AtomicOr
                    | BinaryOpCode::AtomicXor
                    | BinaryOpCode::AtomicExchange => {
                        // Atomics always apply to 32-bit values.  The first argument is a pointer
                        // (so cannot be derived from constants), so only the second argument may
                        // need a precision.
                        propagate_to_id(rhs, precision, to_propagate);
                    }
                };
            }
            OpCode::BuiltIn(op, ref mut params) => {
                match op {
                    BuiltInOpCode::Clamp |
                    BuiltInOpCode::Mix |
                    BuiltInOpCode::Smoothstep |
                    BuiltInOpCode::Fma |
                    BuiltInOpCode::Faceforward |
                    BuiltInOpCode::Refract |
                    BuiltInOpCode::BitfieldExtract |
                    BuiltInOpCode::BitfieldInsert |
                    BuiltInOpCode::TextureQueryLod |
                    BuiltInOpCode::TexelFetch |
                    BuiltInOpCode::TexelFetchOffset |
                    BuiltInOpCode::Rgb2Yuv |
                    BuiltInOpCode::Yuv2Rgb |
                    BuiltInOpCode::AtomicCompSwap |
                    BuiltInOpCode::ImageLoad |
                    BuiltInOpCode::ImageAtomicAdd |
                    BuiltInOpCode::ImageAtomicMin |
                    BuiltInOpCode::ImageAtomicMax |
                    BuiltInOpCode::ImageAtomicAnd |
                    BuiltInOpCode::ImageAtomicOr |
                    BuiltInOpCode::ImageAtomicXor |
                    BuiltInOpCode::ImageAtomicExchange |
                    BuiltInOpCode::ImageAtomicCompSwap |
                    BuiltInOpCode::SamplePosition |
                    BuiltInOpCode::InterpolateAtCenter |
                    BuiltInOpCode::Saturate => {
                        propagate_to_ids(&mut params.iter_mut(), precision, to_propagate);
                    },
                    BuiltInOpCode::TextureSize => {
                        // Return precision is highp.  Assume highp for the lod parameter.
                        propagate_to_id(params.last_mut().unwrap(), Precision::High, to_propagate);
                    },
                    BuiltInOpCode::UaddCarry |
                    BuiltInOpCode::UsubBorrow => {
                        // The parameters are highp,highp,lowp
                        propagate_to_id(&mut params[0], Precision::High, to_propagate);
                        propagate_to_id(&mut params[1], Precision::High, to_propagate);
                        propagate_to_id(&mut params[2], Precision::Low, to_propagate);
                    },
                    BuiltInOpCode::UmulExtended |
                    BuiltInOpCode::ImulExtended => {
                        // The parameters are highp,highp,pointer,pointer
                        propagate_to_id(&mut params[0], Precision::High, to_propagate);
                        propagate_to_id(&mut params[1], Precision::High, to_propagate);
                    },
                    // No arguments, so no precision to propagate.
                    BuiltInOpCode::NumSamples |
                    // The parameter is a pointer, so cannot be derived from constants.
                    BuiltInOpCode::SubpassLoad => {
                    },
                    // Void instructions are handled by the caller.
                    BuiltInOpCode::ImageStore |
                    BuiltInOpCode::MemoryBarrier |
                    BuiltInOpCode::MemoryBarrierAtomicCounter |
                    BuiltInOpCode::MemoryBarrierBuffer |
                    BuiltInOpCode::MemoryBarrierImage |
                    BuiltInOpCode::Barrier |
                    BuiltInOpCode::MemoryBarrierShared |
                    BuiltInOpCode::GroupMemoryBarrier |
                    BuiltInOpCode::EmitVertex |
                    BuiltInOpCode::EndPrimitive |
                    BuiltInOpCode::BeginInvocationInterlockNV |
                    BuiltInOpCode::EndInvocationInterlockNV |
                    BuiltInOpCode::BeginFragmentShaderOrderingINTEL |
                    BuiltInOpCode::BeginInvocationInterlockARB |
                    BuiltInOpCode::EndInvocationInterlockARB |
                    BuiltInOpCode::PixelLocalStoreANGLE |
                    BuiltInOpCode::LoopForwardProgress => {
                        panic!("Internal error: Unexpected void instruction when propagating precision to constants");
                    }
                };
            }
            OpCode::Texture(ref mut texture_op, _sampler, ref mut coord) => {
                // Apply precision to coord.  Note that sampler is a pointer and cannot be derived
                // from constants.
                propagate_to_id(coord, precision, to_propagate);
                match texture_op {
                    TextureOpCode::Implicit { offset, .. }
                    | TextureOpCode::Lod { offset, .. }
                    | TextureOpCode::Bias { offset, .. }
                    | TextureOpCode::Grad { offset, .. }
                    | TextureOpCode::Gather { offset }
                    | TextureOpCode::GatherComponent { offset, .. }
                    | TextureOpCode::GatherRef { offset, .. } => {
                        offset
                            .iter_mut()
                            .for_each(|offset| propagate_to_id(offset, precision, to_propagate));
                    }
                    _ => {}
                };
                match texture_op {
                    TextureOpCode::Implicit { .. } => {}
                    TextureOpCode::Compare { compare } => {
                        propagate_to_id(compare, precision, to_propagate);
                    }
                    TextureOpCode::Lod { lod, .. } => {
                        propagate_to_id(lod, precision, to_propagate);
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        propagate_to_id(compare, precision, to_propagate);
                        propagate_to_id(lod, precision, to_propagate);
                    }
                    TextureOpCode::Bias { bias, .. } => {
                        propagate_to_id(bias, precision, to_propagate);
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        propagate_to_id(compare, precision, to_propagate);
                        propagate_to_id(bias, precision, to_propagate);
                    }
                    TextureOpCode::Grad { dx, dy, .. } => {
                        propagate_to_id(dx, precision, to_propagate);
                        propagate_to_id(dy, precision, to_propagate);
                    }
                    TextureOpCode::Gather { .. } => {}
                    TextureOpCode::GatherComponent { component, .. } => {
                        propagate_to_id(component, precision, to_propagate);
                    }
                    TextureOpCode::GatherRef { refz, .. } => {
                        propagate_to_id(refz, precision, to_propagate);
                    }
                };
            }
            OpCode::MergeInput => {
                // Handled specially by the caller.
            }
            // Void instructions are handled by the caller.
            OpCode::Discard
            | OpCode::Return(_)
            | OpCode::Break
            | OpCode::Continue
            | OpCode::Passthrough
            | OpCode::NextBlock
            | OpCode::Merge(_)
            | OpCode::If(_)
            | OpCode::Loop
            | OpCode::DoLoop
            | OpCode::LoopIf(_)
            | OpCode::Switch(..)
            | OpCode::Store(..) => panic!(
                "Internal error: Unexpected void instruction when propagating precision to constants"
            ),
        }
    }
}

// The result of creating an instruction.  It's either a constant (if constant-folded), a void
// instruction or a register id referencing the instruction in IRMeta::instructions.  Sometimes
// an instruction can detect that it's a no-op too.
#[cfg_attr(debug_assertions, derive(Debug))]
pub enum Result {
    Constant(TypedConstantId),
    Void(OpCode),
    Register(TypedRegisterId),
    NoOp(TypedId),
}

impl Result {
    pub fn get_result_id(&self) -> TypedId {
        match *self {
            instruction::Result::Constant(id) => TypedId::from_typed_constant_id(id),
            instruction::Result::Register(id) => TypedId::from_register_id(id),
            instruction::Result::NoOp(id) => id,
            _ => panic!("Internal error: Expected instruction with value"),
        }
    }

    // Use to make sure the result of the instruction is not a new register, but rather an
    // existing one.  During transformations, an instruction producing register N may need to
    // be replaced with other instructions.  Making sure the end result has the same id
    // obviates the need to transform the IR further to replace the result id with a new one.
    pub fn override_result_id(&mut self, ir_meta: &mut IRMeta, id: TypedRegisterId) {
        match self {
            Result::Register(replace_by) => {
                ir_meta.replace_instruction(id.id, replace_by.id);
                *replace_by = id;
            }
            Result::Constant(constant_id) => {
                // Create an alias for this constant.
                let mut alias =
                    instruction::alias(ir_meta, TypedId::from_typed_constant_id(*constant_id));
                alias.override_result_id(ir_meta, id);
                *self = alias;
            }
            Result::NoOp(replace_by) => {
                // Create an alias for this id.
                let mut alias = instruction::alias(ir_meta, *replace_by);
                alias.override_result_id(ir_meta, id);
                *self = alias;
            }
            _ => panic!("Internal error: Expected typed instruction when replacing result id"),
        }
    }
}

// Helper macros to create an instruction, returning a Result.  For example:
//
//     instruction::make!(negate, ir_meta, operand)
//     instruction::make!(add, ir_meta, lhs, rhs)
macro_rules! make {
        ($func:ident, $ir_meta:expr, $($params:expr),*) => {
            instruction::$func($ir_meta, $($params),*)
        }
    }
pub(crate) use make;
// Similar to make!(), but with a specific result id.
//
//     instruction::make_with_result_id!(negate, ir_meta, result_id, operand)
macro_rules! make_with_result_id {
        ($func:ident, $ir_meta:expr, $result:expr, $($params:expr),*) => {
            {
                let mut inst = instruction::$func($ir_meta, $($params),*);
                inst.override_result_id($ir_meta, $result);
                inst
            }
        }
    }
pub(crate) use make_with_result_id;

fn make_constant(constant_id: ConstantId, type_id: TypeId, precision: Precision) -> Result {
    Result::Constant(TypedConstantId::new(constant_id, type_id, precision))
}

fn make_register(
    ir_meta: &mut IRMeta,
    op: OpCode,
    type_id: TypeId,
    precision: Precision,
) -> Result {
    Result::Register(ir_meta.new_register(op, type_id, precision))
}

// A generic helper to make a unary operator.
fn unary_op<Promote, DerivePrecision, UnaryOp, ConstFold>(
    ir_meta: &mut IRMeta,
    operand: TypedId,
    promote: Promote,
    derive_precision: DerivePrecision,
    op: UnaryOp,
    const_fold: ConstFold,
) -> Result
where
    Promote: FnOnce(&mut IRMeta, TypeId) -> TypeId,
    DerivePrecision: FnOnce(Precision) -> Precision,
    UnaryOp: FnOnce(&mut IRMeta, TypedId, TypeId) -> OpCode,
    ConstFold: FnOnce(&mut IRMeta, ConstantId, TypeId) -> ConstantId,
{
    let result_type_id = promote(ir_meta, operand.type_id);
    let precision = derive_precision(operand.precision);

    // If the operand is constant, constant fold the operation
    if let Id::Constant(constant_id) = operand.id {
        let folded = const_fold(ir_meta, constant_id, result_type_id);
        make_constant(folded, result_type_id, precision)
    } else {
        // Otherwise make an instruction
        let op = op(ir_meta, operand, result_type_id);
        make_register(ir_meta, op, result_type_id, precision)
    }
}

// A generic helper to make a binary operator.
fn binary_op<Promote, DerivePrecision, BinaryOp, ConstFold>(
    ir_meta: &mut IRMeta,
    lhs: TypedId,
    rhs: TypedId,
    promote: Promote,
    derive_precision: DerivePrecision,
    op: BinaryOp,
    const_fold: ConstFold,
) -> Result
where
    Promote: FnOnce(&mut IRMeta, TypeId, TypeId) -> TypeId,
    DerivePrecision: FnOnce(Precision, Precision) -> Precision,
    BinaryOp: FnOnce(&mut IRMeta, TypedId, TypedId) -> OpCode,
    ConstFold: FnOnce(&mut IRMeta, ConstantId, ConstantId, TypeId) -> ConstantId,
{
    let result_type_id = promote(ir_meta, lhs.type_id, rhs.type_id);
    let precision = derive_precision(lhs.precision, rhs.precision);

    // If both operands are constant, constant fold the operation
    if let (Id::Constant(lhs_constant_id), Id::Constant(rhs_constant_id)) = (lhs.id, rhs.id) {
        let folded = const_fold(ir_meta, lhs_constant_id, rhs_constant_id, result_type_id);
        make_constant(folded, result_type_id, precision)
    } else {
        // Otherwise make an instruction
        let op = op(ir_meta, lhs, rhs);
        make_register(ir_meta, op, result_type_id, precision)
    }
}

// Call a user-defined function.
pub fn call(ir_meta: &mut IRMeta, id: FunctionId, args: Vec<TypedId>) -> Result {
    let call = OpCode::Call(id, args);

    let function = ir_meta.get_function(id);
    let return_type_id = function.return_type_id;
    let return_precision = function.return_precision;

    // The result depends on whether the function returns void or a value.
    // If the function is not void, push a new id for its result.
    if return_type_id != TYPE_ID_VOID {
        make_register(ir_meta, call, return_type_id, return_precision)
    } else {
        Result::Void(call)
    }
}

// Block terminating instructions.
pub fn branch_discard() -> Result {
    Result::Void(OpCode::Discard)
}
pub fn branch_return(value: Option<TypedId>) -> Result {
    Result::Void(OpCode::Return(value))
}
pub fn branch_break() -> Result {
    Result::Void(OpCode::Break)
}
pub fn branch_continue() -> Result {
    Result::Void(OpCode::Continue)
}
pub fn branch_passthrough() -> Result {
    Result::Void(OpCode::Passthrough)
}
pub fn branch_next_block() -> Result {
    Result::Void(OpCode::NextBlock)
}
pub fn branch_merge(id: Option<TypedId>) -> Result {
    Result::Void(OpCode::Merge(id))
}

fn load_from_pointer(ir_meta: &mut IRMeta, to_load: TypedId, pointee_type_id: TypeId) -> Result {
    make_register(ir_meta, OpCode::Load(to_load), pointee_type_id, to_load.precision)
}

// Load a value from a pointer and return it.
pub fn load(ir_meta: &mut IRMeta, to_load: TypedId) -> Result {
    let type_info = ir_meta.get_type(to_load.type_id);

    match type_info {
        &Type::Pointer(pointee_type_id) => load_from_pointer(ir_meta, to_load, pointee_type_id),
        _ => Result::NoOp(to_load),
    }
}

// Store a value into a pointer.
pub fn store(_ir_meta: &mut IRMeta, pointer: TypedId, value: TypedId) -> Result {
    Result::Void(OpCode::Store(pointer, value))
}

pub fn alias(ir_meta: &mut IRMeta, id: TypedId) -> Result {
    make_register(ir_meta, OpCode::Alias(id), id.type_id, id.precision)
}

fn same_precision_as_operand(precision: Precision) -> Precision {
    precision
}

fn same_precision_as_lhs(lhs: Precision, _rhs: Precision) -> Precision {
    lhs
}

// Take a component of a vector, like `vector.y`.
pub fn vector_component(ir_meta: &mut IRMeta, vector: TypedId, component: u32) -> Result {
    // To avoid constant index on swizzles, like var.xyz[1], check if the value being indexed
    // is a swizzle, in which case the swizzle components can be folded and the swizzle
    // applied to the original vector.
    //
    // Note that this logic is not valid if the original swizzle was on a pointer and the
    // latter is on a loaded value, because the pointer's pointee value may have changed in
    // between.
    let mut vector = vector;
    let mut component = component;

    if let Id::Register(register_id) = vector.id {
        let instruction = ir_meta.get_instruction(register_id);
        match &instruction.op {
            &OpCode::ExtractVectorComponentMulti(original_vector, ref original_components)
            | &OpCode::AccessVectorComponentMulti(original_vector, ref original_components) => {
                vector = original_vector;
                component = original_components[component as usize];
            }
            _ => (),
        }
    }

    unary_op(
        ir_meta,
        vector,
        promote::vector_component,
        same_precision_as_operand,
        |ir_meta, operand, result_type_id| {
            let is_pointer = ir_meta.get_type(result_type_id).is_pointer();
            if is_pointer {
                OpCode::AccessVectorComponent(operand, component)
            } else {
                OpCode::ExtractVectorComponent(operand, component)
            }
        },
        |ir_meta, constant_id, result_type_id| {
            const_fold::composite_element(ir_meta, constant_id, component, result_type_id)
        },
    )
}

fn merge_swizzle_components(components: &[u32], to_apply: &[u32]) -> Vec<u32> {
    to_apply.iter().map(|&index| components[index as usize]).collect()
}

// Take multiple components of a vector, like `vector.yxy`.
pub fn vector_component_multi(
    ir_meta: &mut IRMeta,
    vector: TypedId,
    components: Vec<u32>,
) -> Result {
    // If the swizzle selects every element in order, optimize that out.
    {
        let mut type_info = ir_meta.get_type(vector.type_id);
        if type_info.is_pointer() {
            type_info = ir_meta.get_type(type_info.get_element_type_id().unwrap());
        }
        let vec_size = type_info.get_vector_size().unwrap() as usize;
        let identity = [0, 1, 2, 3];
        if components[..] == identity[0..vec_size] {
            return Result::NoOp(vector);
        }
    }

    // To avoid swizzles of swizzles, like var.xyz.xz, check if the value being swizzled is
    // itself a swizzle, in which case the swizzle components can be folded and the swizzle
    // applied to the original vector.
    //
    // Note that this logic is not valid if the original swizzle was on a pointer and the
    // latter is on a loaded value, because the pointer's pointee value may have changed in
    // between.
    let mut vector = vector;
    let mut components = components;

    if let Id::Register(register_id) = vector.id {
        let instruction = ir_meta.get_instruction(register_id);
        match &instruction.op {
            &OpCode::ExtractVectorComponentMulti(original_vector, ref original_components)
            | &OpCode::AccessVectorComponentMulti(original_vector, ref original_components) => {
                vector = original_vector;
                components = merge_swizzle_components(original_components, &components);
            }
            _ => (),
        }
    }

    let components_clone = components.clone();
    unary_op(
        ir_meta,
        vector,
        |ir_meta, operand_type| {
            promote::vector_component_multi(ir_meta, operand_type, components.len() as u32)
        },
        same_precision_as_operand,
        |ir_meta, operand, result_type_id| {
            let is_pointer = ir_meta.get_type(result_type_id).is_pointer();
            if is_pointer {
                OpCode::AccessVectorComponentMulti(operand, components_clone)
            } else {
                OpCode::ExtractVectorComponentMulti(operand, components_clone)
            }
        },
        |ir_meta, constant_id, result_type_id| {
            const_fold::vector_component_multi(ir_meta, constant_id, &components, result_type_id)
        },
    )
}

// Equivalent of operator [].
pub fn index(ir_meta: &mut IRMeta, indexed: TypedId, index: TypedId) -> Result {
    // Note: constant index on a vector should use vector_component() instead.
    debug_assert!(!(ir_meta.get_type(indexed.type_id).is_vector() && index.id.is_constant()));

    binary_op(
        ir_meta,
        indexed,
        index,
        |ir_meta, indexed_type, _| promote::index(ir_meta, indexed_type),
        same_precision_as_lhs,
        |ir_meta, indexed, index| {
            let mut referenced_indexed_type = ir_meta.get_type(indexed.type_id);
            let is_pointer = referenced_indexed_type.is_pointer();

            if is_pointer {
                referenced_indexed_type =
                    ir_meta.get_type(referenced_indexed_type.get_element_type_id().unwrap());
            }

            match referenced_indexed_type {
                Type::Vector(..) => {
                    if is_pointer {
                        OpCode::AccessVectorComponentDynamic(indexed, index)
                    } else {
                        OpCode::ExtractVectorComponentDynamic(indexed, index)
                    }
                }
                Type::Matrix(..) => {
                    if is_pointer {
                        OpCode::AccessMatrixColumn(indexed, index)
                    } else {
                        OpCode::ExtractMatrixColumn(indexed, index)
                    }
                }
                _ => {
                    if is_pointer {
                        OpCode::AccessArrayElement(indexed, index)
                    } else {
                        OpCode::ExtractArrayElement(indexed, index)
                    }
                }
            }
        },
        const_fold::index,
    )
}

// Select a field of a struct, like `block.field`.
pub fn struct_field(ir_meta: &mut IRMeta, struct_id: TypedId, field_index: u32) -> Result {
    // Use the precision of the field itself on the result
    let mut struct_type_info = ir_meta.get_type(struct_id.type_id);
    if struct_type_info.is_pointer() {
        struct_type_info = ir_meta.get_type(struct_type_info.get_element_type_id().unwrap());
    }
    let field_precision = struct_type_info.get_struct_field(field_index).precision;

    unary_op(
        ir_meta,
        struct_id,
        |ir_meta, operand_type| promote::struct_field(ir_meta, operand_type, field_index),
        |_| field_precision,
        |ir_meta, operand, result_type_id| {
            let is_pointer = ir_meta.get_type(result_type_id).is_pointer();

            if is_pointer {
                OpCode::AccessStructField(operand, field_index)
            } else {
                OpCode::ExtractStructField(operand, field_index)
            }
        },
        |ir_meta, constant_id, result_type_id| {
            const_fold::composite_element(ir_meta, constant_id, field_index, result_type_id)
        },
    )
}

fn verify_construct_arg_component_count(
    ir_meta: &mut IRMeta,
    type_id: TypeId,
    args: &[TypedId],
) -> bool {
    let type_info = ir_meta.get_type(type_id);
    match type_info {
        // Structs and arrays always require an exact number of elements.
        Type::Array(..) | Type::Struct(..) => {
            return true;
        }
        // Matrix-from-matrix constructors are allowed to have mismatching component count.
        Type::Matrix(..) if args.len() == 1 => {
            return true;
        }
        _ => (),
    }

    let expected_total_components = type_info.get_total_component_count(ir_meta);

    let args_total_components: u32 = args
        .iter()
        .map(|id| {
            let type_info = ir_meta.get_type(id.type_id);
            type_info.get_total_component_count(ir_meta)
        })
        .sum();

    // Vec and matrix constructors either take a scalar, or the caller should have ensured the
    // number of arguments in args matches the constructed type.
    args_total_components == 1 || args_total_components == expected_total_components
}

// Construct a value of a type from the given arguments.
pub fn construct(ir_meta: &mut IRMeta, type_id: TypeId, args: Vec<TypedId>) -> Result {
    // Note: For vector and matrix constructors with multiple components, it is expected that
    // the total components in `args` matches the components needed for type_id.
    debug_assert!(verify_construct_arg_component_count(ir_meta, type_id, &args));

    // Constructor precision is derived from its parameters, except for structs.
    let promoted_precision =
        precision::construct(ir_meta, type_id, &mut args.iter().map(|id| id.precision));

    // If the type of the first argument is the same as the result, the rest of the arguments
    // will be stripped (if any) and the cast is a no-op.  In that case, push args[0] back to
    // the stack and early out.
    //
    // Note: Per the GLSL spec, the constructor precision is derived from its arguments,
    // meaning that `vecN(vN, vM)` where vN is mediump and vM is highp should have highp
    // results, so replacing that with `vN` with the same precision is not entirely correct.
    // For maximum correctness, the `Alias` op can be used to change the precision and give
    // this a new id, but this is such a niche case that we're ignoring it for simplicity.
    if args[0].type_id == type_id {
        return Result::NoOp(args[0]);
    }

    let all_constants = args.iter().all(|id| id.id.is_constant());
    if all_constants {
        let folded = const_fold::construct(
            ir_meta,
            &mut args.iter().map(|id| id.id.get_constant().unwrap()),
            type_id,
        );
        make_constant(folded, type_id, promoted_precision)
    } else {
        let type_info = ir_meta.get_type(type_id);
        let arg0_type_info = ir_meta.get_type(args[0].type_id);

        // Decide the opcode
        let op = match type_info {
            Type::Scalar(..) => OpCode::ConstructScalarFromScalar(args[0]),
            Type::Vector(..) => {
                if args.len() == 1 && arg0_type_info.is_scalar() {
                    OpCode::ConstructVectorFromScalar(args[0])
                } else {
                    OpCode::ConstructVectorFromMultiple(args)
                }
            }
            Type::Matrix(..) => {
                if args.len() == 1 && arg0_type_info.is_scalar() {
                    OpCode::ConstructMatrixFromScalar(args[0])
                } else if args.len() == 1 && arg0_type_info.is_matrix() {
                    OpCode::ConstructMatrixFromMatrix(args[0])
                } else {
                    OpCode::ConstructMatrixFromMultiple(args)
                }
            }
            Type::Struct(..) => OpCode::ConstructStruct(args),
            Type::Array(..) => OpCode::ConstructArray(args),
            _ => panic!("Internal error: Type cannot be constructed"),
        };

        make_register(ir_meta, op, type_id, promoted_precision)
    }
}

// Get the array length of an unsized array.
pub fn array_length(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    // This should only be called for unsized arrays, otherwise the size is already known to
    // the caller.
    let type_info = ir_meta.get_type(operand.type_id);
    debug_assert!(type_info.is_pointer());
    let pointee_type_id = type_info.get_element_type_id().unwrap();

    let type_info = ir_meta.get_type(pointee_type_id);
    debug_assert!(type_info.is_unsized_array());

    make_register(
        ir_meta,
        OpCode::Unary(UnaryOpCode::ArrayLength, operand),
        TYPE_ID_INT,
        precision::array_length(),
    )
}

// Evaluate -operand
//
// Result type: Same as operand's
// Result precision: Same as operand's
pub fn negate(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        promote::negate,
        precision::negate,
        |_, operand, _| OpCode::Unary(UnaryOpCode::Negate, operand),
        const_fold::negate,
    )
}

// Evaluate operand++
//
// Result type: Same as operand's
// Result precision: Same as operand's
pub fn postfix_increment(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        promote::postfix_increment,
        precision::postfix_increment,
        |_, operand, _| OpCode::Unary(UnaryOpCode::PostfixIncrement, operand),
        |_, _, _| panic!("Internal error: Operand of ++ cannot be a constant"),
    )
}

// Evaluate operand--
//
// Result type: Same as operand's
// Result precision: Same as operand's
pub fn postfix_decrement(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        promote::postfix_decrement,
        precision::postfix_decrement,
        |_, operand, _| OpCode::Unary(UnaryOpCode::PostfixDecrement, operand),
        |_, _, _| panic!("Internal error: Operand of -- cannot be a constant"),
    )
}

// Evaluate ++operand
//
// Result type: Same as operand's
// Result precision: Same as operand's
pub fn prefix_increment(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        promote::prefix_increment,
        precision::prefix_increment,
        |_, operand, _| OpCode::Unary(UnaryOpCode::PrefixIncrement, operand),
        |_, _, _| panic!("Internal error: Operand of ++ cannot be a constant"),
    )
}

// Evaluate --operand
//
// Result type: Same as operand's
// Result precision: Same as operand's
pub fn prefix_decrement(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        promote::prefix_decrement,
        precision::prefix_decrement,
        |_, operand, _| OpCode::Unary(UnaryOpCode::PrefixDecrement, operand),
        |_, _, _| panic!("Internal error: Operand of -- cannot be a constant"),
    )
}

// Evaluate operand1+operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector/matrix,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn add(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::add,
        precision::add,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::Add, lhs, rhs),
        const_fold::add,
    )
}

// Evaluate operand1-operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector/matrix,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn sub(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::sub,
        precision::sub,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::Sub, lhs, rhs),
        const_fold::sub,
    )
}

// Evaluate operand1*operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector/matrix,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn mul(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::mul,
        precision::mul,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::Mul, lhs, rhs),
        const_fold::mul,
    )
}

// Evaluate operand1*operand2
//
// Result type: Same as operand1.
// Result precision: Higher of the two operands.
pub fn vector_times_scalar(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    // Make sure the vector is operand1
    let (vector, scalar) =
        if ir_meta.get_type(lhs.type_id).is_scalar() { (rhs, lhs) } else { (lhs, rhs) };

    binary_op(
        ir_meta,
        vector,
        scalar,
        promote::vector_times_scalar,
        precision::vector_times_scalar,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::VectorTimesScalar, lhs, rhs),
        const_fold::mul,
    )
}

// Evaluate operand1*operand2
//
// Result type: Same as operand1.
// Result precision: Higher of the two operands.
pub fn matrix_times_scalar(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    // Make sure the matrix is operand1
    let (matrix, scalar) =
        if ir_meta.get_type(lhs.type_id).is_scalar() { (rhs, lhs) } else { (lhs, rhs) };

    binary_op(
        ir_meta,
        matrix,
        scalar,
        promote::matrix_times_scalar,
        precision::matrix_times_scalar,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::MatrixTimesScalar, lhs, rhs),
        const_fold::mul,
    )
}

// Evaluate operand1*operand2
//
// Result type: Vector of operand2's row count.
// Result precision: Higher of the two operands.
pub fn vector_times_matrix(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::vector_times_matrix,
        precision::vector_times_matrix,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::VectorTimesMatrix, lhs, rhs),
        const_fold::vector_times_matrix,
    )
}

// Evaluate operand1*operand2
//
// Result type: operand1's column type.
// Result precision: Higher of the two operands.
pub fn matrix_times_vector(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::matrix_times_vector,
        precision::matrix_times_vector,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::MatrixTimesVector, lhs, rhs),
        const_fold::matrix_times_vector,
    )
}

// Evaluate operand1*operand2
//
// Result type: matrix with operand2's column count of operand1's column type.
// Result precision: Higher of the two operands.
pub fn matrix_times_matrix(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::matrix_times_matrix,
        precision::matrix_times_matrix,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::MatrixTimesMatrix, lhs, rhs),
        const_fold::matrix_times_matrix,
    )
}

// Evaluate operand1/operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector/matrix,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn div(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::div,
        precision::div,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::Div, lhs, rhs),
        const_fold::div,
    )
}

// Evaluate operand1%operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn imod(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::imod,
        precision::imod,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::IMod, lhs, rhs),
        const_fold::imod,
    )
}

// Evaluate !operand
//
// Result type: Same as operand (bool)
// Result precision: Not applicable
pub fn logical_not(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    // If ! is applied to an instruction that already has a !, use the original instead.
    if let Id::Register(register_id) = operand.id {
        let operand_instruction = ir_meta.get_instruction(register_id);
        if let OpCode::Unary(UnaryOpCode::LogicalNot, original) = operand_instruction.op {
            return Result::NoOp(original);
        }
    }

    unary_op(
        ir_meta,
        operand,
        |_, _| TYPE_ID_BOOL,
        precision::logical_not,
        |_, operand, _| OpCode::Unary(UnaryOpCode::LogicalNot, operand),
        const_fold::logical_not,
    )
}

// Evaluate operand1^^operand2
//
// Result type: Same as operand1 and operand2 (bool)
// Result precision: Not applicable
pub fn logical_xor(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::logical_xor,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::LogicalXor, lhs, rhs),
        const_fold::logical_xor,
    )
}

// Evaluate operand1==operand2
//
// Result type: bool
// Result precision: Not applicable
pub fn equal(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::equal,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::Equal, lhs, rhs),
        const_fold::equal,
    )
}

// Evaluate operand1!=operand2
//
// Result type: bool
// Result precision: Not applicable
pub fn not_equal(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::not_equal,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::NotEqual, lhs, rhs),
        const_fold::not_equal,
    )
}

// Evaluate operand1<operand2
//
// Result type: bool
// Result precision: Not applicable
pub fn less_than(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::less_than,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::LessThan, lhs, rhs),
        const_fold::less_than,
    )
}

// Evaluate operand1>operand2
//
// Result type: bool
// Result precision: Not applicable
pub fn greater_than(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::greater_than,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::GreaterThan, lhs, rhs),
        const_fold::greater_than,
    )
}

// Evaluate operand1<=operand2
//
// Result type: bool
// Result precision: Not applicable
pub fn less_than_equal(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::less_than_equal,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::LessThanEqual, lhs, rhs),
        const_fold::less_than_equal,
    )
}

// Evaluate operand1>=operand2
//
// Result type: bool
// Result precision: Not applicable
pub fn greater_than_equal(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        |_, _, _| TYPE_ID_BOOL,
        precision::greater_than_equal,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::GreaterThanEqual, lhs, rhs),
        const_fold::greater_than_equal,
    )
}

// Evaluate ~operand
//
// Result type: Same as operand
// Result precision: Same as operand
pub fn bitwise_not(ir_meta: &mut IRMeta, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        promote::bitwise_not,
        precision::bitwise_not,
        |_, operand, _| OpCode::Unary(UnaryOpCode::BitwiseNot, operand),
        const_fold::bitwise_not,
    )
}

// Evaluate operand1<<operand2
//
// Result type: Same as operand1
// Result precision: Same as operand1
pub fn bit_shift_left(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::bit_shift_left,
        precision::bit_shift_left,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::BitShiftLeft, lhs, rhs),
        const_fold::bit_shift_left,
    )
}

// Evaluate operand1>>operand2
//
// Result type: Same as operand1
// Result precision: Same as operand1
pub fn bit_shift_right(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::bit_shift_right,
        precision::bit_shift_right,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::BitShiftRight, lhs, rhs),
        const_fold::bit_shift_right,
    )
}

// Evaluate operand1|operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn bitwise_or(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::bitwise_or,
        precision::bitwise_or,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::BitwiseOr, lhs, rhs),
        const_fold::bitwise_or,
    )
}

// Evaluate operand1^operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn bitwise_xor(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::bitwise_xor,
        precision::bitwise_xor,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::BitwiseXor, lhs, rhs),
        const_fold::bitwise_xor,
    )
}

// Evaluate operand1&operand2
//
// Result type: Same as operand1, unless it's a scalar and operand2 is a vector,
//              in which case same as operand2.
// Result precision: Higher of the two operands.
pub fn bitwise_and(ir_meta: &mut IRMeta, lhs: TypedId, rhs: TypedId) -> Result {
    binary_op(
        ir_meta,
        lhs,
        rhs,
        promote::bitwise_and,
        precision::bitwise_and,
        |_, lhs, rhs| OpCode::Binary(BinaryOpCode::BitwiseAnd, lhs, rhs),
        const_fold::bitwise_and,
    )
}

// Evaluate built_in(operand)
//
// Result type: Typically same as operand's but with exceptions.  See the GLSL ES spec.
// Result precision: Typically same as operand's but with exceptions.  See the GLSL ES spec.
pub fn built_in_unary(ir_meta: &mut IRMeta, op: UnaryOpCode, operand: TypedId) -> Result {
    unary_op(
        ir_meta,
        operand,
        |ir_meta, operand_type| promote::built_in_unary(ir_meta, op, operand_type),
        |operand_precision| precision::built_in_unary(op, operand_precision),
        |_, operand, _| OpCode::Unary(op, operand),
        |ir_meta, constant_id, result_type_id| {
            const_fold::built_in_unary(ir_meta, op, constant_id, result_type_id)
        },
    )
}

// Evaluate built_in(operand1, operand2)
//
// Result type: Depends on the built-in.  See the GLSL ES spec.
// Result precision: Typically the higher of operand1's and operand2's but with exceptions.  See the
// GLSL ES spec.
pub fn built_in_binary(
    ir_meta: &mut IRMeta,
    op: BinaryOpCode,
    operand1: TypedId,
    operand2: TypedId,
) -> Result {
    binary_op(
        ir_meta,
        operand1,
        operand2,
        |ir_meta, operand1_type, operand2_type| {
            promote::built_in_binary(ir_meta, op, operand1_type, operand2_type)
        },
        |operand1_precision, operand2_precision| {
            precision::built_in_binary(op, operand1_precision, operand2_precision)
        },
        |_, operand1, operand2| OpCode::Binary(op, operand1, operand2),
        |ir_meta, constant1_id, constant2_id, result_type_id| {
            const_fold::built_in_binary(ir_meta, op, constant1_id, constant2_id, result_type_id)
        },
    )
}

// Evaluate built_in(operands...)
//
// Result type: Depends on the built-in.  See the GLSL ES spec.
// Result precision: Typically the higher of operand1's and operand2's but with exceptions.  See the
// GLSL ES spec.
pub fn built_in(ir_meta: &mut IRMeta, op: BuiltInOpCode, operands: Vec<TypedId>) -> Result {
    let result_type_id = promote::built_in(ir_meta, op, &mut operands.iter().map(|id| id.type_id));
    let precision = (result_type_id != TYPE_ID_VOID)
        .then(|| precision::built_in(op, &mut operands.iter().map(|id| id.precision)));

    // If all operands are constant, constant fold the operation
    if op.may_const_fold() && operands.iter().all(|id| id.id.is_constant()) {
        // Note: No built-in that returns `void` can possibly take all-constant arguments.
        debug_assert!(result_type_id != TYPE_ID_VOID);

        let constants = operands.iter().map(|id| id.id.get_constant().unwrap()).collect();
        let folded = const_fold::built_in(ir_meta, op, constants, result_type_id);
        make_constant(folded, result_type_id, precision.unwrap())
    } else {
        // Otherwise make an instruction
        let op = OpCode::BuiltIn(op, operands);

        if result_type_id != TYPE_ID_VOID {
            make_register(ir_meta, op, result_type_id, precision.unwrap())
        } else {
            // If there is no result, make a void instruction
            Result::Void(op)
        }
    }
}

// Evaluate texture*(operands...)
//
// Result type: Typically vec4 with the same basic type as the sampler, or float if "compare".  See
// the GLSL ES spec. Result precision: Same as the sampler operand.  See the GLSL ES spec.
pub fn built_in_texture(
    ir_meta: &mut IRMeta,
    op: TextureOpCode,
    sampler: TypedId,
    coord: TypedId,
) -> Result {
    let result_type_id = promote::built_in_texture(ir_meta, &op, sampler.type_id);

    // Constant folding is impossible, as the sampler cannot be a constant.
    debug_assert!(sampler.id.get_constant().is_none());

    // Make an instruction
    let precision = precision::built_in_texture(&op, sampler.precision);
    let op = OpCode::Texture(op, sampler, coord);
    make_register(ir_meta, op, result_type_id, precision)
}
