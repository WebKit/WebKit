// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Break down vec and mat arguments to their scalar components.  This can be to work around driver
// bugs where the constructor is not correctly implemented, but it could also be used for the SPIR-V
// generator.
//
// Constructors that are transformed are:
//
#[rustfmt::skip]  // Stop rustfmt from messing up the alignments in the comment
//
// - vecN(scalar): translates to vecN(scalar, ..., scalar)
// - vecN(vec1, vec2, ...): translates to vecN(vec1.x, vec1.y, vec2.x, ...)
// - vecN(matrix): translates to vecN(matrix[0][0], matrix[0][1], ...)
// - matNxM(scalar): translates to matNxM(scalar, 0, ..., 0
//                                        0, scalar, ..., 0
//                                        ...
//                                        0, 0, ..., scalar)
// - matNxM(vec1, vec2, ...): translates to matNxM(vec1.x, vec1.y, vec2.x, ...)
// - matNxM(matrixAxB): translates to matNxM(matrix[0][0], matrix[0][1], ..., 0
//                                           matrix[1][0], matrix[1][1], ..., 0
//                                           ...
//                                           0,            0,            ..., 1)
fn rustfmt_applies_to_this_and_the_comment_above() {}

use crate::ir::*;
use crate::*;

pub fn run(ir: &mut IR) {
    traverser::transformer::for_each_instruction(
        &mut ir.meta,
        &mut ir.function_entries,
        &scalarize_args,
    );
}

fn scalarize_args(
    ir_meta: &mut IRMeta,
    instruction: &BlockInstruction,
) -> Vec<traverser::Transform> {
    let (opcode, result) = instruction.get_op_and_result(ir_meta);
    match opcode {
        &OpCode::ConstructVectorFromScalar(arg) => {
            construct_vector_from_scalar(ir_meta, arg, result.unwrap())
        }
        &OpCode::ConstructMatrixFromScalar(arg) => {
            construct_matrix_from_scalar(ir_meta, arg, result.unwrap())
        }
        &OpCode::ConstructMatrixFromMatrix(arg) => {
            construct_matrix_from_matrix(ir_meta, arg, result.unwrap())
        }
        OpCode::ConstructVectorFromMultiple(args) => {
            construct_vector_from_multiple(ir_meta, &args.clone(), result.unwrap())
        }
        OpCode::ConstructMatrixFromMultiple(args) => {
            construct_matrix_from_multiple(ir_meta, &args.clone(), result.unwrap())
        }
        _ => vec![],
    }
}

fn construct_helper(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    type_id: TypeId,
    args: Vec<TypedId>,
    result_id: Option<TypedRegisterId>,
) -> TypedId {
    if let Some(result_id) = result_id {
        traverser::add_typed_instruction(
            transforms,
            instruction::make_with_result_id!(construct, ir_meta, result_id, type_id, args, None),
        )
    } else {
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, type_id, args, None),
        )
    }
}

// Flatten a constructor argument into its scalar components.
fn flatten(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    id: TypedId,
    components: &mut Vec<TypedId>,
) {
    let type_info = ir_meta.get_type(id.type_id);
    match *type_info {
        Type::Matrix(_, column_count) => {
            // Flatten matrices column by column
            for column in 0..column_count {
                let column = ir_meta.get_constant_uint_typed(column, Precision::Unassigned);
                let column_id = traverser::add_typed_instruction(
                    transforms,
                    instruction::make!(index, ir_meta, id, column),
                );
                flatten(ir_meta, transforms, column_id, components);
            }
        }
        Type::Vector(_, component_count) => {
            components.extend((0..component_count).map(|component| {
                traverser::add_typed_instruction(
                    transforms,
                    instruction::make!(vector_component, ir_meta, id, component),
                )
            }));
        }
        _ => components.push(id),
    };
}

fn flatten_constructor_args(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    args: &[TypedId],
) -> Vec<TypedId> {
    let mut components = vec![];
    for &arg in args {
        flatten(ir_meta, transforms, arg, &mut components);
    }
    components
}

fn cast(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    value: TypedId,
    to_type: TypeId,
    preserved_precision: Precision,
) -> TypedId {
    if value.type_id != to_type {
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, to_type, vec![value], Some(preserved_precision)),
        )
    } else {
        value
    }
}

// Taking a list of all-scalar components, casts them to the basic type of the constructor.
fn cast_constructor_args(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    args: Vec<TypedId>,
    constructor_type_id: TypeId,
    preserved_precision: Precision,
) -> Vec<TypedId> {
    let constructor_basic_type_id = ir_meta.get_scalar_type(constructor_type_id);

    args.iter()
        .map(|&arg| cast(ir_meta, transforms, arg, constructor_basic_type_id, preserved_precision))
        .collect()
}

fn construct_vector_from_scalar(
    ir_meta: &mut IRMeta,
    arg: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let mut transforms = vec![];
    // Assuming original instruction is:
    //     r3  (int[3], mediump) = ConstructVectorFromScalar r2 (bool)
    // This function transforms it to the following 2 new instructions:
    //     r7  (int) = ConstructScalarFromScalar r2 (bool)
    //     r3  (int[3]) = ConstructVectorFromMultiple (r7, r7, r7)
    // Since r2 is a boolean type, it does not have a precision, new instruction result registers
    // have no precision, either. This is because of the following code in instruction::construct():
    //
    //     Constructor precision is derived from its parameters, except for structs.
    //     let promoted_precision = precision::construct(ir_meta, type_id, &mut args.iter().map(|id|
    //                                                    id.precision));
    //
    // This is incorrect as r3 ends up losing the original precision.
    // We need to preserve the original result precision by passing it to cast() function,
    // so that r7 ends up with the original precision.
    // Then, util::construct_vector_from_scalar() will correctly set the precision of r3 based on
    // r7's precision.

    // Such transformations can be observed in the IR dump when running the following tests:
    // dEQP-GLES2.functional.shaders.conversions.scalar_to_vector.bool_to_ivec3_vertex
    // --use-angle=webgpu
    // dEQP-GLES2.functional.fragment_ops.interaction.basic_shader.64 --use-angle=webgpu
    // GLSLTest.FunctionArgumentEvalOrderIncrement/ES2_WebGPU

    // Cache original result precision
    let preserved_precision = result.precision;
    let constructor_basic_type_id = ir_meta.get_scalar_type(result.type_id);
    // Pass target precision into scalar cast
    let arg = cast(ir_meta, &mut transforms, arg, constructor_basic_type_id, preserved_precision);

    util::construct_vector_from_scalar(
        ir_meta,
        &mut transforms,
        arg,
        result.type_id,
        Some(result),
        construct_helper,
    );
    transforms
}
fn construct_matrix_from_scalar(
    ir_meta: &mut IRMeta,
    arg: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let mut transforms = vec![];

    // Cache original result precision
    let preserved_precision = result.precision;
    let constructor_basic_type_id = ir_meta.get_scalar_type(result.type_id);

    // Pass target precision into scalar cast
    let arg = cast(ir_meta, &mut transforms, arg, constructor_basic_type_id, preserved_precision);

    util::construct_matrix_from_scalar(
        ir_meta,
        &mut transforms,
        arg,
        result.type_id,
        Some(result),
        TYPED_CONSTANT_ID_FLOAT_ZERO,
        construct_helper,
    );
    transforms
}
fn construct_matrix_from_matrix(
    ir_meta: &mut IRMeta,
    arg: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let mut transforms = vec![];
    util::construct_matrix_from_matrix(
        ir_meta,
        &mut transforms,
        arg,
        result.type_id,
        Some(result),
        TYPED_CONSTANT_ID_FLOAT_ZERO,
        TYPED_CONSTANT_ID_FLOAT_ONE,
        |ir_meta, transforms, id| {
            let type_info = ir_meta.get_type(id.type_id);
            match *type_info {
                Type::Vector(_, element_count) => (0..element_count)
                    .map(|element| {
                        traverser::add_typed_instruction(
                            transforms,
                            instruction::make!(vector_component, ir_meta, id, element),
                        )
                    })
                    .collect(),
                Type::Matrix(_, column_count) => (0..column_count)
                    .map(|column| {
                        // column should have a precision assigned after propagate_precision pass.
                        // since the number of columns of matrix is small, Precision::Low is
                        // sufficient.
                        let column = ir_meta.get_constant_uint_typed(column, Precision::Low);
                        traverser::add_typed_instruction(
                            transforms,
                            instruction::make!(index, ir_meta, id, column),
                        )
                    })
                    .collect(),
                _ => panic!("Internal error: expected matrix or vector"),
            }
        },
        construct_helper,
    );
    transforms
}
fn construct_vector_from_multiple(
    ir_meta: &mut IRMeta,
    args: &[TypedId],
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let mut transforms = vec![];
    // Cache original result precision
    let preserved_precision = result.precision;
    let args = flatten_constructor_args(ir_meta, &mut transforms, args);
    let args =
        cast_constructor_args(ir_meta, &mut transforms, args, result.type_id, preserved_precision);

    traverser::add_typed_instruction(
        &mut transforms,
        instruction::make_with_result_id!(construct, ir_meta, result, result.type_id, args, None),
    );
    transforms
}
fn construct_matrix_from_multiple(
    ir_meta: &mut IRMeta,
    args: &[TypedId],
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let mut transforms = vec![];
    // Cache original result precision
    let preserved_precision = result.precision;
    let args = flatten_constructor_args(ir_meta, &mut transforms, args);
    let args =
        cast_constructor_args(ir_meta, &mut transforms, args, result.type_id, preserved_precision);

    util::construct_matrix_from_multiple(
        ir_meta,
        &mut transforms,
        &args,
        result.type_id,
        Some(result),
        construct_helper,
    );
    transforms
}
