// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A set of independent workarounds that are localized to instructions.  These workarounds are
// independent and are applied in one pass.
//
// * Clamp array indices if non-constant, ensuring robustness.

use crate::ir::*;
use crate::*;

pub struct Options {
    // Clamp non-constant indices to the bounds of the entity being indexed.
    pub clamp_indirect_indices: bool,
}

pub fn run(ir: &mut IR, options: &Options) {
    traverser::transformer::for_each_instruction(
        &mut ir.meta,
        &mut ir.function_entries,
        &|ir_meta, instruction| {
            let (opcode, result) = instruction.get_op_and_result(ir_meta);
            match *opcode {
                OpCode::ExtractVectorComponentDynamic(indexed, index)
                | OpCode::ExtractMatrixColumn(indexed, index)
                | OpCode::ExtractArrayElement(indexed, index)
                | OpCode::AccessVectorComponentDynamic(indexed, index)
                | OpCode::AccessMatrixColumn(indexed, index)
                | OpCode::AccessArrayElement(indexed, index) => {
                    if options.clamp_indirect_indices {
                        clamp_indirect_index(ir_meta, indexed, index, result.unwrap())
                    } else {
                        vec![]
                    }
                }
                _ => vec![],
            }
        },
    );
}

fn clamp_indirect_index(
    ir_meta: &mut IRMeta,
    indexed: TypedId,
    index: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    // No need to clamp constant indices, they are already validated to be in range.
    if index.id.is_constant() {
        return vec![];
    }

    let mut indexed_type = ir_meta.get_type(indexed.type_id);
    if indexed_type.is_pointer() {
        let pointee_type_id = ir_meta.get_pointee_type(indexed.type_id);
        indexed_type = ir_meta.get_type(pointee_type_id);
    }

    // Don't clamp indirect indices on unsized arrays in buffer blocks.  They are covered by the
    // relevant robust access behavior of the backend.
    if indexed_type.is_unsized_array() {
        return vec![];
    }

    let mut transforms = vec![];

    // On GLSL es 100, clamp is only defined for float, so float arguments are used.
    // Float clamp is unconditionally emitted to work around driver bugs with integer clamp on
    // Qualcomm.  http://crbug.com/40770896
    //
    // Generate the following:
    //
    //     index'   = ConstructScalarFromScalar index (float)
    //     clamped  = Clamp index' 0 MAX
    //     clamped' = ConstructScalarFromScalar clamped (int)
    let max = match *indexed_type {
        Type::Vector(_, max) | Type::Matrix(_, max) | Type::Array(_, max) => {
            ir_meta.get_constant_float_typed((max - 1) as f32)
        }
        _ => panic!("Internal error: Invalid type to index"),
    };
    let index_as_float = traverser::add_typed_instruction(
        &mut transforms,
        instruction::make!(construct, ir_meta, TYPE_ID_FLOAT, vec![index]),
    );
    let clamped = traverser::add_typed_instruction(
        &mut transforms,
        instruction::make!(
            built_in,
            ir_meta,
            BuiltInOpCode::Clamp,
            vec![index_as_float, TYPED_CONSTANT_ID_FLOAT_ZERO, max]
        ),
    );
    let clamped = traverser::add_typed_instruction(
        &mut transforms,
        instruction::make!(construct, ir_meta, index.type_id, vec![clamped]),
    );

    // Replace the original instruction with one using the new index.
    traverser::add_typed_instruction(
        &mut transforms,
        instruction::make_with_result_id!(index, ir_meta, result, indexed, clamped),
    );
    transforms
}
