// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Walk over the IR and propagate precision to constants and constant-derived registers which don't
// get a precision assigned to them during parse.
use crate::ir::*;
use crate::*;

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    current_function_return_precision: Precision,
    // Used to propagate newly-assigned register precisions to their instructions.
    updated_precisions: HashMap<RegisterId, Precision>,
    // Used to propagate precision to function call arguments
    function_arg_precisions: HashMap<FunctionId, Vec<Precision>>,
    // Used to propagate precision to struct constructor arguments
    struct_member_precisions: HashMap<TypeId, Vec<Precision>>,
}

pub fn run(ir: &mut IR) {
    let mut state = State {
        ir_meta: &mut ir.meta,
        current_function_return_precision: Precision::NotApplicable,
        updated_precisions: HashMap::new(),
        function_arg_precisions: HashMap::new(),
        struct_member_precisions: HashMap::new(),
    };

    // Prepare a map of functions to the precision of their arguments.
    traverser::visitor::for_each_function(
        &mut state,
        &ir.function_entries,
        |state, function_id| {
            let argument_precision = state
                .ir_meta
                .get_function(function_id)
                .params
                .iter()
                .map(|param| state.ir_meta.get_variable(param.variable_id).precision)
                .collect();
            state.function_arg_precisions.insert(function_id, argument_precision);
        },
        |_, _, _, _| traverser::visitor::STOP,
        |_, _| {},
    );

    // Prepare a map of structs to the precision of their members.
    state.ir_meta.all_types().iter().enumerate().for_each(|(index, type_info)| {
        if let Type::Struct(_, fields, _) = type_info {
            let member_precision = fields.iter().map(|field| field.precision).collect();
            state.struct_member_precisions.insert(TypeId { id: index as u32 }, member_precision);
        }
    });

    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, function_id, entry| {
            state.current_function_return_precision =
                state.ir_meta.get_function(function_id).return_precision;
            propagate_precision_in_block(state, entry, Precision::NotApplicable);
        },
    );
}

fn propagate_precision_in_block(
    state: &mut State,
    block: &mut Block,
    merge_block_input_precision: Precision,
) {
    // Propagate precision in the current block first

    // Propagate precision in the merge block before the sub-blocks of this block.  This is so that
    // if the merge input of the merge block gets a precision, we can propagate it to the Merge()
    // instructions of the sub-blocks without having to revisit them twice.

    if let Some(merge_block) = block.merge_block.as_mut() {
        propagate_precision_in_block(state, merge_block, merge_block_input_precision);
    }

    propagate_precision_in_instructions(state, block, merge_block_input_precision);

    // Because the merge blocks are visited already, we know what the merge block input precision
    // is going to be (if updated).
    let this_block_merge_input_precision = block
        .merge_block
        .as_ref()
        .map(|merge_block| {
            merge_block.input.map(|id| id.precision).unwrap_or(Precision::NotApplicable)
        })
        .unwrap_or(Precision::NotApplicable);

    block.for_each_sub_block_mut(state, &|state, sub_block| {
        propagate_precision_in_block(state, sub_block, this_block_merge_input_precision)
    });

    // At the end, if the merge input has gotten a precision, update it.
    if let Some(id) = block.input.as_mut() {
        if let Some(&precision) = state.updated_precisions.get(&id.id) {
            // See comment above, it's unexpected for this register to have been used
            // multiple times if it didn't have a precision (and so was derived from
            // non-variable constants).
            debug_assert!(id.precision == Precision::NotApplicable);
            id.precision = precision;
        } else if id.precision == Precision::NotApplicable
            && util::is_precision_applicable_to_type(state.ir_meta, id.type_id)
        {
            // If precision is applicable but could not be derived above, then it's impossible to
            // derive a precision for the merge input.  Just assign highp to it.
            // For example, in:
            //
            //     (u ? constant0 : constant1) < constant2
            //
            // The result is a boolean, the condition is boolean, and all arguments are constant.
            // The merge input for the ternary expression will not get a precision in that case
            // with the logic above.
            let instruction = state.ir_meta.get_instruction_mut(id.id);
            instruction.result.precision = Precision::High;
            id.precision = Precision::High;
        }
    }
}

fn propagate_precision_in_instructions(
    state: &mut State,
    block: &mut Block,
    merge_block_input_precision: Precision,
) {
    for instruction in block.instructions.iter_mut() {
        // To propagate precision, there are two steps.  First, the precision of the operands need
        // to be determined; this is typically the same as the result precision, but some built-ins
        // impose specific precision requirements on their arguments.  Next, the precision is
        // propagated to those operands that don't already have one (which is only the case for
        // constants, merge inputs that have only received constants, and any op that only uses
        // those as operands).
        let mut new_ids_to_update = vec![];
        match instruction {
            BlockInstruction::Register(id) => {
                let instruction = state.ir_meta.get_instruction_mut(*id);
                instruction::precision::propagate(
                    instruction,
                    &state.function_arg_precisions,
                    &state.struct_member_precisions,
                    &mut new_ids_to_update,
                );
            }
            BlockInstruction::Void(OpCode::Merge(Some(id))) => {
                instruction::precision::propagate_to_id(
                    id,
                    merge_block_input_precision,
                    &mut new_ids_to_update,
                );
            }
            BlockInstruction::Void(OpCode::Return(Some(id))) => {
                instruction::precision::propagate_to_id(
                    id,
                    state.current_function_return_precision,
                    &mut new_ids_to_update,
                );
            }
            BlockInstruction::Void(OpCode::Store(pointer, value)) => {
                instruction::precision::propagate_to_id(
                    value,
                    pointer.precision,
                    &mut new_ids_to_update,
                );
            }
            BlockInstruction::Void(OpCode::BuiltIn(
                BuiltInOpCode::PixelLocalStoreANGLE,
                params,
            )) => {
                let precision = params[0].precision;
                instruction::precision::propagate_to_id(
                    &mut params[1],
                    precision,
                    &mut new_ids_to_update,
                );
            }
            BlockInstruction::Void(OpCode::BuiltIn(BuiltInOpCode::ImageStore, params)) => {
                let precision = params[0].precision;
                instruction::precision::propagate_to_ids(
                    &mut params.iter_mut(),
                    precision,
                    &mut new_ids_to_update,
                );
            }
            _ => {}
        };

        while let Some((id, precision)) = new_ids_to_update.pop() {
            let type_id = state.ir_meta.get_instruction(id).result.type_id;
            if !util::is_precision_applicable_to_type(state.ir_meta, type_id) {
                continue;
            }

            state.updated_precisions.insert(id, precision);

            let instruction = state.ir_meta.get_instruction_mut(id);
            // Note: It should never be possible to assign precision to the same register twice
            // during parse, because the registers that don't have a precision but should are
            // derived purely from constants (not const variables) and so cannot have been used
            // multiple times in different expressions.
            //
            // Post-parse transformations should not leave precision undefined.
            debug_assert!(instruction.result.precision == Precision::NotApplicable);
            instruction.result.precision = precision;
            // Propagate precision to the operands if needed
            instruction::precision::propagate(
                instruction,
                &state.function_arg_precisions,
                &state.struct_member_precisions,
                &mut new_ids_to_update,
            );
        }
    }
}
