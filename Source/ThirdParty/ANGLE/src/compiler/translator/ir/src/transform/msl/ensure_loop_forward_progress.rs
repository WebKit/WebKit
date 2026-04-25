// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This pass adds a function call to every loop that cannot be proven to be finite.  The function
// call is a pretend built-in that the MSL generator implements (with a volatile access) to
// work around a metal compiler crash in the presence of infinite loops.
use crate::ir::*;
use crate::*;

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    // For each variable, records how many times it's used in a Store or some output operation.
    variable_store_count: Vec<u32>,
}

pub fn run(ir: &mut IR) {
    let variable_store_count = vec![0; ir.meta.all_variables().len()];
    let mut state = State { ir_meta: &mut ir.meta, variable_store_count };

    // To avoid going over the instructions multiple times in nested loops, in a first pass gather
    // how many times variables are written to.  Later, when a loop is encountered, we can tell if
    // the body of the loop is writing to its variable in some way or not.
    traverser::visitor::for_each_instruction(
        &mut state,
        &ir.function_entries,
        &mark_variable_writes,
    );

    // Try to detect loop patterns that are obviously finite, covering the most common cases.  It's
    // functionally ok to add the `OpCode::LoopForwardProgress` workaround to any loop, but it's
    // best to avoid it as much as possible.
    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, _, entry| {
            traverser::transformer::for_each_block(
                state,
                entry,
                &|state, block| {
                    add_forward_progress(state, block);
                    block
                },
                &|_, block| block,
            )
        },
    );
}

fn mark_variable_write(ir_meta: &IRMeta, variable_store_count: &mut Vec<u32>, pointer: TypedId) {
    util::trace_back(
        ir_meta,
        variable_store_count,
        pointer.id,
        &mut |_, _| {},
        &mut |variable_store_count, variable_id| {
            variable_store_count[variable_id.id as usize] += 1;
        },
        &mut |_, _, opcode| {
            // Only interested in write access to loop variables and only scalar and vector types
            // are of interest.
            match *opcode {
                OpCode::AccessVectorComponent(base_id, _)
                | OpCode::AccessVectorComponentMulti(base_id, _)
                | OpCode::AccessVectorComponentDynamic(base_id, _) => Some(base_id.id),
                _ => None,
            }
        },
    );
}

fn mark_variable_writes(state: &mut State, instruction: &BlockInstruction) {
    let (opcode, _) = instruction.get_op_and_result(state.ir_meta);
    util::inspect_pointer_access(
        state.ir_meta,
        &mut state.variable_store_count,
        opcode,
        &|variable_store_count, pointer, access| {
            // Not interested in reads
            if access != util::PointerAccess::Read {
                // Treat `inout` parameters as being written to.
                mark_variable_write(state.ir_meta, variable_store_count, pointer);
            }
        },
    );
}

// Currently, only match loops in the following form:
//
//     for (...; variable op constant; expression)
//
// Where:
//
// * `op` can only be one of > >= < <= == or !=
// * `constant` can be a non-zero constant, or a read-only value such as shader inputs and uniforms.
// * `expression` can only be one of:
//   * variable++
//   * variable--
//   * variable += constant
//   * variable -= constant
fn matches_finite_loop_condition(ir_meta: &IRMeta, block: &Block) -> Option<VariableId> {
    // Match either:
    //
    //     r1 = Load v
    //     r2 = Op r1 c
    //     LoopIf r2
    //
    // Or:
    //
    //     r1 = Access* v1 ...
    //     r2 = Access* r1 ...
    //     ...
    //     rN-1 = Access* rN-2 ...
    //     rN = Load rN-1
    //
    //     rM = Load v
    //
    //     rO = Op rM rN
    //     LoopIf rO
    //
    // To match the latter without worrying about the order of instructions, the following
    // algorithm is used:
    //
    // * Take the instruction of the `LoopIf` register, it must be `Op a b`.
    // * Back trace `a`, it must be a single `Load v`
    // * Back trace `b`, it must either be a constant, or a series of Access instructions leading to
    //   a Load.
    // * During the above, gather the visited registers.  Verify that:
    //   * Each instruction in the block produces a register that's in the gathered set (i.e. there
    //     are no other instructions)
    //   * The `Load v` and `Op` instructions are in this block.  It's ok if right hand side is not
    //     evaluated every frame, since it corresponds to a read-only variable.

    let op_result = match *block.get_terminating_op() {
        OpCode::LoopIf(TypedId { id: Id::Register(op_result), .. }) => op_result,
        _ => return None,
    };
    let (loop_variable_value, comparator) =
        if let OpCode::Binary(expect_compare_op, loop_variable_value, comparator) =
            ir_meta.get_instruction(op_result).op
            && loop_variable_value.id.is_register()
            && matches!(
                expect_compare_op,
                BinaryOpCode::Equal
                    | BinaryOpCode::NotEqual
                    | BinaryOpCode::LessThan
                    | BinaryOpCode::GreaterThan
                    | BinaryOpCode::LessThanEqual
                    | BinaryOpCode::GreaterThanEqual
            )
        {
            (loop_variable_value.id.get_register(), comparator.id)
        } else {
            return None;
        };

    // Expect loop_variable_value is loaded from a variable, which is denoted as the loop variable.
    let loop_variable = if let OpCode::Load(loop_variable) =
        ir_meta.get_instruction(loop_variable_value).op
        && loop_variable.id.is_variable()
    {
        loop_variable.id.get_variable()
    } else {
        return None;
    };

    // Expect comparator is either a constant or loaded from a read-only variable.  Comparing with
    // any constant is fine.
    let mut is_comparator_constant = false;
    let mut visited_registers = HashSet::from([op_result, loop_variable_value]);
    util::trace_back(
        ir_meta,
        &mut is_comparator_constant,
        comparator,
        &mut |is_comparator_constant, _| {
            *is_comparator_constant = true;
        },
        &mut |is_comparator_constant, variable_id| {
            let variable = ir_meta.get_variable(variable_id);
            // Consider uniforms and shader inputs as read-only.
            *is_comparator_constant = variable.decorations.has(Decoration::Uniform)
                || variable.decorations.has(Decoration::Input);
        },
        &mut |_, result, opcode| match *opcode {
            OpCode::Load(pointer)
            | OpCode::AccessVectorComponent(pointer, _)
            | OpCode::AccessVectorComponentMulti(pointer, _)
            | OpCode::AccessVectorComponentDynamic(pointer, _)
            | OpCode::AccessMatrixColumn(pointer, _)
            | OpCode::AccessStructField(pointer, _)
            | OpCode::AccessArrayElement(pointer, _) => {
                visited_registers.insert(result);
                Some(pointer.id)
            }
            _ => None,
        },
    );
    if !is_comparator_constant {
        return None;
    }

    // Check that the block doesn't have any other instructions, and that the loop variable load
    // and compare are indeed in this block.  For example, a shader doing `i < (side_effect(),10)`
    // is rejected.
    let mut visited_loop_variable_load = false;
    let mut visited_op = false;
    for instruction in &block.instructions[0..block.instructions.len() - 1] {
        match *instruction {
            BlockInstruction::Void(_) => return None,
            BlockInstruction::Register(register) => {
                if !visited_registers.contains(&register) {
                    return None;
                }
                if register == loop_variable_value {
                    visited_loop_variable_load = true;
                }
                if register == op_result {
                    visited_op = true;
                }
            }
        };
    }

    (visited_loop_variable_load && visited_op).then_some(loop_variable)
}

fn constant_is_one(ir_meta: &IRMeta, id: ConstantId) -> bool {
    let constant = ir_meta.get_constant(id);
    match constant.value {
        ConstantValue::Float(f) => f == 1.0 || f == -1.0,
        ConstantValue::Int(i) => i == 1 || i == -1,
        ConstantValue::Uint(u) => u == 1,
        ConstantValue::Composite(ref components) => {
            components.iter().all(|&component| constant_is_one(ir_meta, component))
        }
        _ => false,
    }
}

fn matches_finite_loop_continue(
    ir_meta: &IRMeta,
    block: &Block,
    loop_variable: VariableId,
) -> bool {
    // The instructions in the continue block must match one of the following:
    //
    //     _ = IncDecOp v
    //         Continue
    //
    // Where `IncDecOp` is one of UnaryOpCode::{PostfixIncrement, PostfixDecrement,
    // PrefixIncrement, PrefixDecrement}, or:
    //
    //     r1 = Load v
    //     r2 = StepOp r1 c
    //          Store v r2
    //          Continue
    //
    // Where `StepOp` is one of BinaryOpCode::{Add, Sub}, and `c` is an increment step of 1.
    match block.instructions[..] {
        [BlockInstruction::Register(expect_inc_dec), BlockInstruction::Void(OpCode::Continue)] => {
            let expect_inc_dec = ir_meta.get_instruction(expect_inc_dec);
            match expect_inc_dec.op {
                OpCode::Unary(expect_inc_dec_op, expect_loop_variable) => {
                    expect_loop_variable.id.is_variable()
                        && expect_loop_variable.id.get_variable() == loop_variable
                        && matches!(
                            expect_inc_dec_op,
                            UnaryOpCode::PrefixIncrement
                                | UnaryOpCode::PrefixDecrement
                                | UnaryOpCode::PostfixIncrement
                                | UnaryOpCode::PostfixDecrement
                        )
                }
                _ => false,
            }
        }
        [
            BlockInstruction::Register(expect_load),
            BlockInstruction::Register(expect_step),
            BlockInstruction::Void(OpCode::Store(expect_loop_variable_store, expect_op_result)),
            BlockInstruction::Void(OpCode::Continue),
        ] => {
            let expect_load = ir_meta.get_instruction(expect_load);
            let expect_step = ir_meta.get_instruction(expect_step);
            match (&expect_load.op, &expect_step.op) {
                (
                    &OpCode::Load(expect_loop_variable),
                    &OpCode::Binary(expect_step_op, expect_loaded_value, expect_step_constant),
                ) => {
                    expect_loop_variable.id.is_variable()
                        && expect_loop_variable_store.id.is_variable()
                        && expect_loop_variable.id.get_variable() == loop_variable
                        && expect_loop_variable_store.id.get_variable() == loop_variable
                        && expect_loaded_value.id == Id::new_register(expect_load.result.id)
                        && expect_op_result.id == Id::new_register(expect_step.result.id)
                        && expect_step_constant.id.is_constant()
                        && matches!(expect_step_op, BinaryOpCode::Add | BinaryOpCode::Sub)
                        && constant_is_one(ir_meta, expect_step_constant.id.get_constant())
                }
                _ => false,
            }
        }
        _ => false,
    }
}

fn is_possibly_infinite_loop(state: &State, block: &Block) -> bool {
    // The block must end in a `Loop`.
    if !matches!(block.get_terminating_op(), OpCode::Loop) {
        return false;
    }
    // Without a continue block, it's considered a possibly infinite loop.
    if !block.has_loop_continue_block() {
        return true;
    }

    // Check if the condition matches the finite loop pattern, and if so get the loop variable.
    let loop_variable =
        matches_finite_loop_condition(state.ir_meta, block.get_loop_condition_block());
    if let Some(loop_variable) = loop_variable {
        // The variable is allowed to be written to exactly once (which is in the continue block).
        // Otherwise consider this a possibly infinite loop.  This will reject loops where the
        // variable is modified by the body of the loop.
        if state.variable_store_count[loop_variable.id as usize] != 1 {
            return true;
        }

        // Check if the continue block matches the finite loop pattern.
        return !matches_finite_loop_continue(
            state.ir_meta,
            block.get_loop_continue_block(),
            loop_variable,
        );
    }

    // Any other loop is considered potentially infinite.
    true
}

fn add_forward_progress(state: &mut State, block: &mut Block) {
    if is_possibly_infinite_loop(state, block) {
        // Prepend the following to the body of the loop:
        //
        //     LoopForwardProgress
        block.get_loop_body_block_mut().prepend_instruction(instruction::built_in(
            state.ir_meta,
            BuiltInOpCode::LoopForwardProgress,
            vec![],
        ));
    }
}
