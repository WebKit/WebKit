// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Turn array of array of uniforms into flat arrays:
//
//     uniform type u[Dn]...[D2][D1];
//     ->
//     uniform type u[Dn*...*D2*D1];
//
// Let's define
//
//     Pn = D(n-1)*...*D2*D1
//
// In that case, we have:
//
//     u[In]...[I2][I1] => u + In*Pn + ... + I2*P2 + I1*P1
//
// The monomorphize_unsupported_functions and rewrite_struct_samplers passes must already be run, in
// which case access instructions from `u` all the way to the single element being accessed is in
// the same function:
//
//     indexedN = AccessArrayElement u In
//     ...
//     indexed2 = AccessArrayElement indexed3 I2
//     indexed1 = AccessArrayElement indexed2 I1
//
// The above is transformed to:
//
//     offsetN = Mul In Pn
//     ...
//     offset2 = Mul I2 P2
//     offset1 = Mul I1 P1
//     accOffsetN_1 = Add offsetN + offsetN_1
//     ...
//     accOffset2   = Add accOffset3 + offset2
//     accOffset1   = Add accOffset2 + offset1
//     indexed1 = AccessArrayElement u accOffset1
//
// If an index is a constant, the above is automatically constant folded during instruction
// generation.

use crate::ir::*;
use crate::*;

// For each type, cache `Dn` and `Pn`.
#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
struct ArrayInfo {
    is_array: bool,
    is_array_of_array: bool,
    // Dn:
    array_size: u32,
    // Pn:
    stride: u32,
}

impl ArrayInfo {
    fn new() -> ArrayInfo {
        ArrayInfo { is_array: false, is_array_of_array: false, array_size: 1, stride: 1 }
    }
}

// For each AccessArrayElement, if the pointer being accessed is tracked, cache the calculated
// `accOffsetN` as well as the original uniform variable being accessed.
#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
struct AccessInfo {
    uniform_var: TypedId,
    acc_offset: TypedId,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    array_of_array_uniforms: HashSet<VariableId>,
    array_info: Vec<ArrayInfo>,
    access_info: Vec<Option<AccessInfo>>,
}

pub fn run(ir: &mut IR) {
    let array_info = gather_array_info(&ir.meta);
    let array_of_array_uniforms = flatten_array_of_array_uniforms(&mut ir.meta, &array_info);
    if array_of_array_uniforms.is_empty() {
        return;
    }

    let access_info = vec![None; ir.meta.total_register_count() as usize];
    let mut state =
        State { ir_meta: &mut ir.meta, array_of_array_uniforms, array_info, access_info };

    traverser::transformer::for_each_instruction(
        &mut state,
        &mut ir.function_entries,
        &|state, instruction| {
            let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
            match *opcode {
                OpCode::AccessArrayElement(pointer, index) => {
                    transform_array_element(state, pointer, index, result.unwrap())
                }
                _ => vec![],
            }
        },
    );
}

fn gather_array_info(ir_meta: &IRMeta) -> Vec<ArrayInfo> {
    let mut array_info = vec![ArrayInfo::new(); ir_meta.all_types().len()];

    ir_meta.all_types().iter().enumerate().skip(MAX_PREDEFINED_TYPE_ID as usize).for_each(
        |(id, type_info)| {
            match *type_info {
                Type::Pointer(element_type_id) => {
                    // For simplicity, propagate type info identically to pointer types
                    array_info[id] = array_info[element_type_id.id as usize].clone();
                }
                Type::Array(element_type_id, size) => {
                    // Calculate info for this array.  The stride is the element's array dimension
                    // times the element's stride.
                    let element_info = &array_info[element_type_id.id as usize];
                    let is_array_of_array = element_info.is_array;
                    let stride = element_info.stride * element_info.array_size;
                    array_info[id] =
                        ArrayInfo { is_array: true, is_array_of_array, array_size: size, stride };
                }
                _ => (),
            }
        },
    );

    array_info
}

fn flatten_array_of_array_uniforms(
    ir_meta: &mut IRMeta,
    array_info: &[ArrayInfo],
) -> HashSet<VariableId> {
    let mut array_of_array_uniforms = HashSet::new();

    for variable_id in ir_meta.all_global_variables().clone() {
        let variable = ir_meta.get_variable(variable_id);
        if variable.decorations.has(Decoration::Uniform) {
            let info = &array_info[variable.type_id.id as usize];
            if !info.is_array_of_array {
                continue;
            }

            // Only looking for opaque uniforms
            let base_type_id = ir_meta.get_pointee_type(variable.type_id);
            let base_type_id = ir_meta.get_base_element_type(base_type_id);
            if !matches!(
                ir_meta.get_type(base_type_id),
                Type::Image(..) | Type::Scalar(BasicType::AtomicCounter)
            ) {
                continue;
            }

            // Need to transform access to this variable
            array_of_array_uniforms.insert(variable_id);

            // Replace the type of the variable with a flattened array.  This is ok because
            // monomorphize_unsupported_functions has ensured that the variable cannot be passed to
            // a function; the only remaining possible use of this variable is it getting indexed
            // until a single element is used, which is an instruction that is also transformed by
            // this pass.
            let flattened_type_id =
                ir_meta.get_array_type_id(base_type_id, info.array_size * info.stride);
            let flattened_type_id = ir_meta.get_pointer_type_id(flattened_type_id);
            ir_meta.get_variable_mut(variable_id).type_id = flattened_type_id;
        }
    }

    array_of_array_uniforms
}

fn multiply_index_by_stride(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    index: TypedId,
    stride: u32,
) -> TypedId {
    // Make sure the type of all indices match by using unsigned int for all
    let stride_constant = ir_meta.get_constant_uint_typed(stride, index.precision);
    let index = if index.type_id == TYPE_ID_INT {
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, TYPE_ID_UINT, vec![index], None),
        )
    } else {
        index
    };

    if stride == 1 {
        // No need to multiply by one, just take the index
        index
    } else {
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(mul, ir_meta, index, stride_constant),
        )
    }
}

fn accumulate_offset(
    ir_meta: &mut IRMeta,
    transforms: &mut Vec<traverser::Transform>,
    acc_offset: TypedId,
    offset: TypedId,
) -> TypedId {
    traverser::add_typed_instruction(
        transforms,
        instruction::make!(add, ir_meta, acc_offset, offset),
    )
}

fn transform_array_element(
    state: &mut State,
    pointer: TypedId,
    index: TypedId,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    let mut transforms = vec![];

    match pointer.id {
        Id::Variable(variable_id) => {
            // If this is an array-of-array uniform variable, replace the instruction:
            //
            //     indexedN = AccessArrayElement u In
            //
            // with:
            //
            //     offsetN  = Mul In Pn
            //
            if state.array_of_array_uniforms.contains(&variable_id) {
                let info = &state.array_info[pointer.type_id.id as usize];
                let acc_offset =
                    multiply_index_by_stride(state.ir_meta, &mut transforms, index, info.stride);

                state.access_info[result.id.id as usize] =
                    Some(AccessInfo { uniform_var: pointer, acc_offset });

                if transforms.is_empty() {
                    // Ensure the instruction is removed even if the new instructions are constant
                    // folded
                    transforms.push(traverser::Transform::Remove);
                }
            }
        }
        Id::Register(register_id) => {
            // If the access chain is tracked for this pointer, replace the instruction:
            //
            //     indexedM = AccessArrayElement indexedM+1 Im
            //
            // with:
            //
            //     offsetM    = Mul Im Pm
            //     accOffsetM = Add accOffsetM+1 offsetM
            //
            if let Some(AccessInfo { uniform_var, acc_offset }) =
                state.access_info[register_id.id as usize]
            {
                let info = &state.array_info[pointer.type_id.id as usize];
                let is_final_index = !info.is_array_of_array;
                let offset =
                    multiply_index_by_stride(state.ir_meta, &mut transforms, index, info.stride);
                let acc_offset =
                    accumulate_offset(state.ir_meta, &mut transforms, acc_offset, offset);

                if is_final_index {
                    // The result of this access is no longer an array.  Replace it with the final
                    // flattened access:
                    //
                    //     indexed1 = AccessArrayElement u accOffset1
                    //
                    traverser::add_typed_instruction(
                        &mut transforms,
                        instruction::make_with_result_id!(
                            index,
                            state.ir_meta,
                            result,
                            uniform_var,
                            acc_offset,
                        ),
                    );
                } else {
                    state.access_info[result.id.id as usize] =
                        Some(AccessInfo { uniform_var, acc_offset });

                    if transforms.is_empty() {
                        // Ensure the instruction is removed even if the new instructions are
                        // constant folded
                        transforms.push(traverser::Transform::Remove);
                    }
                }
            }
        }
        Id::Constant(..) => panic!("Internal error: Unexpected AccessArrayElement on a constant"),
    };

    transforms
}
