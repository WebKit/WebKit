// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Miscellaneous utility functions

use crate::ir::*;
use crate::*;

// Calculates the DAG order of the functions.  If the functions are declared in this order,
// there is no need for forward declarations.  Returns an empty vector if there is recursion.
pub fn calculate_function_decl_order(
    ir_meta: &IRMeta,
    function_entries: &[Option<Block>],
) -> Vec<FunctionId> {
    // Build the function call graph first.
    let mut call_graph = vec![HashSet::new(); function_entries.len()];

    traverser::visitor::for_each_function(
        &mut (FunctionId { id: 0 }, &mut call_graph),
        function_entries,
        |(current_function, _), id| {
            *current_function = id;
        },
        |(current_function, call_graph), block, _, _| {
            for instruction in &block.instructions {
                let (opcode, _) = instruction.get_op_and_result(ir_meta);
                if let &OpCode::Call(callee, _) = opcode {
                    call_graph[current_function.id as usize].insert(callee);
                }
            }

            traverser::visitor::VISIT_SUB_BLOCKS
        },
        |_, _| {},
    );

    #[derive(Copy, Clone, PartialEq)]
    #[cfg_attr(debug_assertions, derive(Debug))]
    enum VisitState {
        NotVisited,
        Visiting,
        Visited,
    }
    #[derive(Copy, Clone, PartialEq)]
    #[cfg_attr(debug_assertions, derive(Debug))]
    enum VisitOrder {
        Pre,
        Post,
    }

    // Call DFS from main(), calculating the topological sort and whether there are any cycles
    // in one go.
    let mut decl_order = Vec::new();
    let mut visit_state = vec![VisitState::NotVisited; function_entries.len()];
    let mut visit_stack = vec![(ir_meta.get_main_function_id().unwrap(), VisitOrder::Pre)];

    while let Some((function_id, visit)) = visit_stack.pop() {
        let state = &mut visit_state[function_id.id as usize];

        match visit {
            VisitOrder::Pre => {
                match *state {
                    // If this node is already visited, move on, we have reached it from another
                    // call path.
                    VisitState::Visited => {
                        continue;
                    }
                    // If this node is being visited and we cycle back to it, there's a loop!
                    // This was already validated by the parser, so this would be an internal
                    // error.
                    VisitState::Visiting => {
                        panic!("Internal error: recursion detected");
                    }
                    VisitState::NotVisited => {
                        // Schedule this node to be visited again after the callees.
                        *state = VisitState::Visiting;
                        visit_stack.push((function_id, VisitOrder::Post));
                        // Visit the callees.
                        call_graph[function_id.id as usize].iter().for_each(|id| {
                            visit_stack.push((*id, VisitOrder::Pre));
                        });
                    }
                }
            }
            VisitOrder::Post => {
                // Once all callees are visited, this function can be declared.
                debug_assert!(*state == VisitState::Visiting);
                *state = VisitState::Visited;
                decl_order.push(function_id);
            }
        };
    }

    decl_order
}

// Check if a block has nothing but a `Merge(None)`.  It could also be a chain of `NextBlock`s
// ending in `Merge(None)`.  If the blocks have any variables defined, they are ignored; the
// variables are local a to block that doesn't use them.
pub fn is_empty_block(block: &Block) -> bool {
    let mut current_block = block;

    loop {
        if current_block.instructions.len() != 1 {
            return false;
        }

        if let OpCode::Merge(None) = current_block.get_terminating_op() {
            return true;
        }

        if let OpCode::NextBlock = current_block.get_terminating_op() {
            current_block = current_block.merge_block.as_ref().unwrap();
        } else {
            return false;
        }
    }
}

struct DuplicateBlockContext<'a, 'b, 'c> {
    ir_meta: &'a mut IRMeta,
    variable_map: &'b mut HashMap<VariableId, Id>,
    register_map: &'c mut HashMap<RegisterId, RegisterId>,
}

// Duplicate a variable, helper for monomorphization and block duplication.
pub fn duplicate_variable(ir_meta: &mut IRMeta, variable_id: VariableId) -> VariableId {
    let variable = ir_meta.get_variable(variable_id);
    let replacement = Variable::new(
        variable.name,
        variable.type_id,
        variable.precision,
        variable.decorations.clone(),
        variable.built_in,
        variable.initializer,
        variable.scope,
    );
    ir_meta.add_variable(replacement)
}

// Duplicate a block (including its sub-blocks) with new temp ids.  This is useful for example
// to monomorphize functions.
pub fn duplicate_block(
    ir_meta: &mut IRMeta,
    variable_map: &mut HashMap<VariableId, Id>,
    block: &Block,
) -> Block {
    let mut register_map = HashMap::new();

    let mut context =
        DuplicateBlockContext { ir_meta, variable_map, register_map: &mut register_map };

    traverser::visitor::visit_block_instructions(
        &mut context,
        block,
        &|context, block| new_block_to_duplicate(context, &block.variables, block.input),
        &|context, block_result, instructions| {
            duplicate_instructions(context, block_result, instructions)
        },
        &|context,
          block_result,
          branch_opcode,
          loop_condition_block_result,
          block1_result,
          block2_result,
          case_block_results| {
            duplicate_branch_instruction(
                context,
                block_result,
                branch_opcode,
                loop_condition_block_result,
                block1_result,
                block2_result,
                case_block_results,
            )
        },
        &|_, block_result_chain| merge_duplicated_block_chain(block_result_chain),
    )
}

fn new_block_to_duplicate(
    context: &mut DuplicateBlockContext,
    block_variables: &[VariableId],
    block_input: Option<TypedRegisterId>,
) -> Block {
    let mut new_block = Block::new();
    new_block.variables = block_variables
        .iter()
        .map(|&variable_id| {
            // Note that variables inside blocks should always be Temporary, not Internal or
            // ShaderInterface.  As such, it's ok to reuse the same name, they will be
            // disambiguated during codegen if necessary.
            debug_assert!(
                context.ir_meta.get_variable(variable_id).name.source == NameSource::Temporary
            );

            let replacement = duplicate_variable(context.ir_meta, variable_id);
            context.variable_map.insert(variable_id, Id::new_variable(replacement));
            replacement
        })
        .collect();

    new_block.input = block_input.map(|input| {
        let replacement =
            context.ir_meta.new_register(OpCode::MergeInput, input.type_id, input.precision);

        context.register_map.insert(input.id, replacement.id);
        replacement
    });

    new_block
}

macro_rules! mapped_id {
    ($context:ident, $id:ident) => {{
        let mapped = match $id.id {
            Id::Register(r) => Id::new_register(*$context.register_map.get(&r).unwrap_or(&r)),
            Id::Variable(v) => *$context.variable_map.get(&v).unwrap_or(&$id.id),
            Id::Constant(_) => $id.id,
        };

        TypedId::new(mapped, $id.type_id, $id.precision)
    }};
}

macro_rules! mapped_ids {
    ($context:ident, $ids:ident) => {
        $ids.iter().map(|&id| mapped_id!($context, id)).collect()
    };
}

fn duplicate_instructions(
    context: &mut DuplicateBlockContext,
    block: &mut Block,
    instructions: &[BlockInstruction],
) {
    instructions.iter().for_each(|instruction| {
        let (op, result) = instruction.get_op_and_result(context.ir_meta);
        let duplicate_op = match op {
            &OpCode::ExtractVectorComponent(id, index) => {
                OpCode::ExtractVectorComponent(mapped_id!(context, id), index)
            }
            &OpCode::AccessVectorComponent(id, index) => {
                OpCode::AccessVectorComponent(mapped_id!(context, id), index)
            }
            &OpCode::ExtractVectorComponentMulti(id, ref indices) => {
                OpCode::ExtractVectorComponentMulti(mapped_id!(context, id), indices.clone())
            }
            &OpCode::AccessVectorComponentMulti(id, ref indices) => {
                OpCode::AccessVectorComponentMulti(mapped_id!(context, id), indices.clone())
            }
            &OpCode::ExtractVectorComponentDynamic(id, index) => {
                OpCode::ExtractVectorComponentDynamic(
                    mapped_id!(context, id),
                    mapped_id!(context, index),
                )
            }
            &OpCode::AccessVectorComponentDynamic(id, index) => {
                OpCode::AccessVectorComponentDynamic(
                    mapped_id!(context, id),
                    mapped_id!(context, index),
                )
            }
            &OpCode::ExtractMatrixColumn(id, index) => {
                OpCode::ExtractMatrixColumn(mapped_id!(context, id), mapped_id!(context, index))
            }
            &OpCode::AccessMatrixColumn(id, index) => {
                OpCode::AccessMatrixColumn(mapped_id!(context, id), mapped_id!(context, index))
            }
            &OpCode::ExtractArrayElement(id, index) => {
                OpCode::ExtractArrayElement(mapped_id!(context, id), mapped_id!(context, index))
            }
            &OpCode::AccessArrayElement(id, index) => {
                OpCode::AccessArrayElement(mapped_id!(context, id), mapped_id!(context, index))
            }
            &OpCode::ExtractStructField(id, index) => {
                OpCode::ExtractStructField(mapped_id!(context, id), index)
            }
            &OpCode::AccessStructField(id, index) => {
                OpCode::AccessStructField(mapped_id!(context, id), index)
            }
            &OpCode::ConstructScalarFromScalar(id) => {
                OpCode::ConstructScalarFromScalar(mapped_id!(context, id))
            }
            &OpCode::ConstructVectorFromScalar(id) => {
                OpCode::ConstructVectorFromScalar(mapped_id!(context, id))
            }
            &OpCode::ConstructMatrixFromScalar(id) => {
                OpCode::ConstructMatrixFromScalar(mapped_id!(context, id))
            }
            &OpCode::ConstructMatrixFromMatrix(id) => {
                OpCode::ConstructMatrixFromMatrix(mapped_id!(context, id))
            }
            OpCode::ConstructVectorFromMultiple(ids) => {
                OpCode::ConstructVectorFromMultiple(mapped_ids!(context, ids))
            }
            OpCode::ConstructMatrixFromMultiple(ids) => {
                OpCode::ConstructMatrixFromMultiple(mapped_ids!(context, ids))
            }
            OpCode::ConstructStruct(ids) => OpCode::ConstructStruct(mapped_ids!(context, ids)),
            OpCode::ConstructArray(ids) => OpCode::ConstructArray(mapped_ids!(context, ids)),
            &OpCode::Load(pointer) => OpCode::Load(mapped_id!(context, pointer)),
            &OpCode::Store(pointer, value) => {
                OpCode::Store(mapped_id!(context, pointer), mapped_id!(context, value))
            }
            &OpCode::Call(function_id, ref params) => {
                OpCode::Call(function_id, mapped_ids!(context, params))
            }
            &OpCode::Unary(unary_op, id) => OpCode::Unary(unary_op, mapped_id!(context, id)),
            &OpCode::Binary(binary_op, lhs, rhs) => {
                OpCode::Binary(binary_op, mapped_id!(context, lhs), mapped_id!(context, rhs))
            }
            &OpCode::BuiltIn(built_in_op, ref ids) => {
                OpCode::BuiltIn(built_in_op, mapped_ids!(context, ids))
            }
            &OpCode::Texture(ref texture_op, sampler, coord) => {
                let texture_op = match *texture_op {
                    TextureOpCode::Implicit { is_proj, offset } => TextureOpCode::Implicit {
                        is_proj,
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    TextureOpCode::Compare { compare } => {
                        TextureOpCode::Compare { compare: mapped_id!(context, compare) }
                    }
                    TextureOpCode::Lod { is_proj, lod, offset } => TextureOpCode::Lod {
                        is_proj,
                        lod: mapped_id!(context, lod),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    TextureOpCode::CompareLod { compare, lod } => TextureOpCode::CompareLod {
                        compare: mapped_id!(context, compare),
                        lod: mapped_id!(context, lod),
                    },
                    TextureOpCode::Bias { is_proj, bias, offset } => TextureOpCode::Bias {
                        is_proj,
                        bias: mapped_id!(context, bias),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    TextureOpCode::CompareBias { compare, bias } => TextureOpCode::CompareBias {
                        compare: mapped_id!(context, compare),
                        bias: mapped_id!(context, bias),
                    },
                    TextureOpCode::Grad { is_proj, dx, dy, offset } => TextureOpCode::Grad {
                        is_proj,
                        dx: mapped_id!(context, dx),
                        dy: mapped_id!(context, dy),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    TextureOpCode::Gather { offset } => {
                        TextureOpCode::Gather { offset: offset.map(|id| mapped_id!(context, id)) }
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        TextureOpCode::GatherComponent {
                            component: mapped_id!(context, component),
                            offset: offset.map(|id| mapped_id!(context, id)),
                        }
                    }
                    TextureOpCode::GatherRef { refz, offset } => TextureOpCode::GatherRef {
                        refz: mapped_id!(context, refz),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                };
                OpCode::Texture(
                    texture_op,
                    mapped_id!(context, sampler),
                    mapped_id!(context, coord),
                )
            }
            _ => panic!("Internal error: unexpected op when duplicating block"),
        };
        match result {
            Some(original_id) => {
                let new_id = context.ir_meta.new_register(
                    duplicate_op,
                    original_id.type_id,
                    original_id.precision,
                );
                context.register_map.insert(original_id.id, new_id.id);
                block.add_register_instruction(new_id.id);
            }
            None => {
                block.add_void_instruction(duplicate_op);
            }
        };
    });
}

fn duplicate_branch_instruction(
    context: &mut DuplicateBlockContext,
    block: &mut Block,
    op: &OpCode,
    loop_condition_block_result: Option<Block>,
    block1_result: Option<Block>,
    block2_result: Option<Block>,
    case_block_results: Vec<Block>,
) {
    block.set_sub_blocks(
        loop_condition_block_result,
        block1_result,
        block2_result,
        case_block_results,
    );

    match *op {
        OpCode::Discard => block.terminate(OpCode::Discard),
        OpCode::Return(id) => block.terminate(OpCode::Return(id.map(|id| mapped_id!(context, id)))),
        OpCode::Break => block.terminate(OpCode::Break),
        OpCode::Continue => block.terminate(OpCode::Continue),
        OpCode::Passthrough => block.terminate(OpCode::Passthrough),
        OpCode::NextBlock => block.terminate(OpCode::NextBlock),
        OpCode::Merge(id) => block.terminate(OpCode::Merge(id.map(|id| mapped_id!(context, id)))),
        OpCode::If(id) => block.terminate(OpCode::If(mapped_id!(context, id))),
        OpCode::Loop => block.terminate(OpCode::Loop),
        OpCode::DoLoop => block.terminate(OpCode::DoLoop),
        OpCode::LoopIf(id) => block.terminate(OpCode::LoopIf(mapped_id!(context, id))),
        OpCode::Switch(id, ref case_ids) => {
            block.terminate(OpCode::Switch(mapped_id!(context, id), case_ids.clone()))
        }
        _ => panic!("Internal error: unexpected branch op when duplicating block"),
    }
}

fn merge_duplicated_block_chain(mut block_chain: Vec<Block>) -> Block {
    let mut current_block = block_chain.pop().unwrap();

    while let Some(mut parent_block) = block_chain.pop() {
        parent_block.set_merge_block(current_block);
        current_block = parent_block;
    }

    current_block
}

pub fn is_precision_applicable_to_type(ir_meta: &IRMeta, type_id: TypeId) -> bool {
    let mut base_type_id = type_id;
    while let Some(id) = ir_meta.get_type(base_type_id).get_element_type_id() {
        base_type_id = id;
    }
    matches!(base_type_id, TYPE_ID_FLOAT | TYPE_ID_INT | TYPE_ID_UINT)
}

// Helper to walk back the instructions starting from an id, in search of some origin.
//
// For example, if a transformation encounters `OpLoad id` and wants to find out if `id`
// corresponds to a uniform, it has to look into the `id`.  If it's a variable, it can check its
// properties directly, but if it's a register it has to find out what instruction produces it.
// For example, it could be `AccessArrayElement base index`, in which case it has to recursively
// repeat this process until it arrives at the base variable.
//
// Also noteworthy is that this helper simplifies transformation by letting them generally be
// oblivious to `Alias` instructions as it automatically skips over them.
//
// The register callback must return `None` if recursion needs to stop, or `Some(id)` to continue
// exploring.
pub fn trace_back<State, InspectConstant, InspectVariable, InspectRegister>(
    ir_meta: &IRMeta,
    state: &mut State,
    id: Id,
    inspect_constant: &mut InspectConstant,
    inspect_variable: &mut InspectVariable,
    inspect_register: &mut InspectRegister,
) where
    InspectConstant: FnMut(&mut State, ConstantId),
    InspectVariable: FnMut(&mut State, VariableId),
    InspectRegister: FnMut(&mut State, RegisterId, &OpCode) -> Option<Id>,
{
    let mut id = id;
    loop {
        id = match id {
            Id::Constant(constant_id) => {
                inspect_constant(state, constant_id);
                break;
            }
            Id::Variable(variable_id) => {
                inspect_variable(state, variable_id);
                break;
            }
            Id::Register(register_id) => {
                let instruction = ir_meta.get_instruction(register_id);
                // Automatically skip over `Alias` instructions.
                if let OpCode::Alias(alias_id) = instruction.op {
                    alias_id.id
                } else if let Some(to_inspect) =
                    inspect_register(state, register_id, &instruction.op)
                {
                    to_inspect
                } else {
                    break;
                }
            }
        }
    }
}

// ESSL 100 Appendix A.4 Control Flow specifies the shape of supported `for` loops.  The format is:
//
//     for (type variable = constant; variable op constant; expression)
//
// Where:
//
// * `type` can only be `int` of `float`
// * `op` can only be one of > >= < <= == or !=
// * `expression` can only be one of:
//   * variable++
//   * variable--
//   * variable += constant
//   * variable -= constant
//
// For some outputs, we need to keep the `for` loops of this shape as `for` loops and not turn them
// into `while` loops.  In particular:
//
// * ESSL 100 needs this to comply with the spec, as the output is passed to the OpenGL ES driver.
// * HLSL needs this for D3D9.
//
// This function checks whether the block ends in loop in the above form.  If so, the information
// needed to reconstruct the for loop is returned.
pub struct TrivialLoopInfo {
    // The members are mapped to a for loop as such:
    //
    //                for (int i = 0; i < 10; i++)
    //                         ^   ^    ^  ^    ^
    //                         |   |    |  |    |
    //              loop_variable  |    |  |   ascending + increment_step
    //           variable_initializer   | condition_comparator
    //                              condition_op
    pub loop_variable: VariableId,
    pub variable_initializer: ConstantId,
    pub condition_op: BinaryOpCode,
    pub condition_comparator: ConstantId,
    // True if expression is ++ or +=, false if -- or -=
    pub ascending: bool,
    // The constant step if expression is += or -=, implicitly 1 if None.
    pub increment_step: Option<ConstantId>,
}
pub fn block_ends_in_trivial_for_loop(ir_meta: &IRMeta, block: &Block) -> Option<TrivialLoopInfo> {
    // The block must end in a `Loop`.
    if !matches!(block.get_terminating_op(), OpCode::Loop) || !block.has_loop_continue_block() {
        return None;
    }

    // The instructions in the condition block must match:
    //
    //     r1 = Load v
    //     r2 = Op r1 c
    //     LoopIf r2
    //
    // Where `Op` is one of BinaryOpCode::{Equal, NotEqual, LessThan, GreaterThan, LessThanEqual,
    // GreaterThanEqual}, `v` is a variable id and `c` is a constant id.
    let loop_condition = block.get_loop_condition_block();
    let (loop_variable, condition_op, condition_comparator) = match loop_condition.instructions[..]
    {
        [
            BlockInstruction::Register(expect_load),
            BlockInstruction::Register(expect_op),
            BlockInstruction::Void(OpCode::LoopIf(expect_op_result)),
        ] => {
            let expect_load = ir_meta.get_instruction(expect_load);
            let expect_op = ir_meta.get_instruction(expect_op);

            // Extract loop variable `v`, condition operator `Op` and comparator constant `c` if
            // the pattern matches the expectation.
            match (&expect_load.op, &expect_op.op) {
                (
                    &OpCode::Load(expect_variable),
                    &OpCode::Binary(
                        expect_compare_op,
                        expect_loaded_value,
                        expect_comparator_constant,
                    ),
                ) if expect_variable.id.is_variable()
                    && expect_loaded_value.id == Id::new_register(expect_load.result.id)
                    && expect_op_result.id == Id::new_register(expect_op.result.id)
                    && expect_comparator_constant.id.is_constant()
                    && matches!(
                        expect_compare_op,
                        BinaryOpCode::Equal
                            | BinaryOpCode::NotEqual
                            | BinaryOpCode::LessThan
                            | BinaryOpCode::GreaterThan
                            | BinaryOpCode::LessThanEqual
                            | BinaryOpCode::GreaterThanEqual
                    ) =>
                {
                    (
                        expect_variable.id.get_variable(),
                        expect_compare_op,
                        expect_comparator_constant.id.get_constant(),
                    )
                }
                _ => {
                    return None;
                }
            }
        }
        _ => {
            return None;
        }
    };

    // Check that the variable is indeed declared in this block, it's scoped to the `for` loop, and
    // extract its initializer.
    if !block.variables.contains(&loop_variable) {
        return None;
    }
    let variable = ir_meta.get_variable(loop_variable);
    let variable_initializer = if let Some(initializer) = variable.initializer
        && variable.scope == VariableScope::ForLoopVariable
    {
        initializer
    } else {
        return None;
    };

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
    // Where `StepOp` is one of BinaryOpCode::{Add, Sub}, and `c` is the increment step.
    //
    // Extract whether the loop direction is ascending or descending, and in the second pattern
    // above what the increment `c` is.
    let loop_continue = block.get_loop_continue_block();
    let (ascending, increment_step) = match loop_continue.instructions[..] {
        [BlockInstruction::Register(expect_inc_dec), BlockInstruction::Void(OpCode::Continue)] => {
            let expect_inc_dec = ir_meta.get_instruction(expect_inc_dec);
            match expect_inc_dec.op {
                // Note that while ESSL 100 requires postfix operators, we also detect prefix
                // operators which are functionality identical in this case, to help with the other
                // generators that need to detect this sort of `for` loop.
                OpCode::Unary(expect_inc_dec_op, expect_loop_variable)
                    if expect_loop_variable.id.is_variable()
                        && expect_loop_variable.id.get_variable() == loop_variable
                        && matches!(
                            expect_inc_dec_op,
                            UnaryOpCode::PrefixIncrement
                                | UnaryOpCode::PrefixDecrement
                                | UnaryOpCode::PostfixIncrement
                                | UnaryOpCode::PostfixDecrement
                        ) =>
                {
                    (
                        matches!(
                            expect_inc_dec_op,
                            UnaryOpCode::PrefixIncrement | UnaryOpCode::PostfixIncrement
                        ),
                        None,
                    )
                }
                _ => {
                    return None;
                }
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
                ) if expect_loop_variable.id.is_variable()
                    && expect_loop_variable_store.id.is_variable()
                    && expect_loop_variable.id.get_variable() == loop_variable
                    && expect_loop_variable_store.id.get_variable() == loop_variable
                    && expect_loaded_value.id == Id::new_register(expect_load.result.id)
                    && expect_op_result.id == Id::new_register(expect_step.result.id)
                    && expect_step_constant.id.is_constant()
                    && matches!(expect_step_op, BinaryOpCode::Add | BinaryOpCode::Sub) =>
                {
                    (
                        matches!(expect_step_op, BinaryOpCode::Add),
                        expect_step_constant.id.get_if_constant(),
                    )
                }
                _ => {
                    return None;
                }
            }
        }
        _ => {
            return None;
        }
    };

    Some(TrivialLoopInfo {
        loop_variable,
        variable_initializer,
        condition_op,
        condition_comparator,
        ascending,
        increment_step,
    })
}

// Transformations may need to know when variables are read from or written to.  Since read and
// write can be done in a number of instructions, this helper can be used to avoid having to
// enumerate all of them.
#[derive(Copy, Clone, PartialEq)]
pub enum PointerAccess {
    Read,
    Write,
    ReadWrite,
}
pub fn inspect_pointer_access<State, OnAccess>(
    ir_meta: &IRMeta,
    state: &mut State,
    opcode: &OpCode,
    on_access: &OnAccess,
) where
    OnAccess: Fn(&mut State, TypedId, PointerAccess),
{
    match opcode {
        // Read accesses
        &OpCode::Load(pointer)
        | &OpCode::Binary(BinaryOpCode::AtomicAdd, pointer, _)
        | &OpCode::Binary(BinaryOpCode::AtomicMin, pointer, _)
        | &OpCode::Binary(BinaryOpCode::AtomicMax, pointer, _)
        | &OpCode::Binary(BinaryOpCode::AtomicAnd, pointer, _)
        | &OpCode::Binary(BinaryOpCode::AtomicOr, pointer, _)
        | &OpCode::Binary(BinaryOpCode::AtomicXor, pointer, _)
        | &OpCode::Binary(BinaryOpCode::AtomicExchange, pointer, _) => {
            on_access(state, pointer, PointerAccess::Read);
        }

        // Write accesses
        &OpCode::Store(pointer, _)
        | &OpCode::Binary(BinaryOpCode::Modf, _, pointer)
        | &OpCode::Binary(BinaryOpCode::Frexp, _, pointer) => {
            on_access(state, pointer, PointerAccess::Write);
        }
        OpCode::BuiltIn(BuiltInOpCode::UaddCarry, args)
        | OpCode::BuiltIn(BuiltInOpCode::UsubBorrow, args) => {
            on_access(state, args[2], PointerAccess::Write);
        }
        OpCode::BuiltIn(BuiltInOpCode::UmulExtended, args)
        | OpCode::BuiltIn(BuiltInOpCode::ImulExtended, args) => {
            on_access(state, args[3], PointerAccess::Write);
            on_access(state, args[2], PointerAccess::Write);
        }

        // Read/write access
        &OpCode::Unary(UnaryOpCode::PrefixIncrement, pointer)
        | &OpCode::Unary(UnaryOpCode::PrefixDecrement, pointer)
        | &OpCode::Unary(UnaryOpCode::PostfixIncrement, pointer)
        | &OpCode::Unary(UnaryOpCode::PostfixDecrement, pointer) => {
            on_access(state, pointer, PointerAccess::ReadWrite);
        }

        // Calls could read or write from variables depending on the parameter direction.
        &OpCode::Call(function_id, ref args) => {
            let param_directions =
                ir_meta.get_function(function_id).params.iter().map(|param| param.direction);
            args.iter().zip(param_directions).for_each(|(&arg, direction)| {
                // `out` and `inout` parameters are always pointers.
                match direction {
                    FunctionParamDirection::Input => {
                        if ir_meta.get_type(arg.type_id).is_pointer() {
                            on_access(state, arg, PointerAccess::Read);
                        }
                    }
                    FunctionParamDirection::InputOutput => {
                        on_access(state, arg, PointerAccess::ReadWrite);
                    }
                    FunctionParamDirection::Output => {
                        on_access(state, arg, PointerAccess::Write);
                    }
                };
            });
        }
        _ => {}
    };
}
