// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// In the first pass, a list of dependencies is created between registers.  For example, in:
//
//     r3 = Add r1 r2
//
// {r1, r2} is associated with r3.  Later, if r3 is used anywhere, r1 and r2 are also marked as
// used.  At the same time, any register that's used in an instruction with side effects (such as
// Store, Return, PostfixIncrement, etc) is marked as used, which recursively sets all its
// dependencies as used.  In the end, any (instruction associated with a) register that is never
// marked as used can be eliminated.
//
// In the second pass, unused registers are eliminated.  At the same time, unused variables,
// constants and types are tracked and removed from the IR at the end.
//
// Note: To make the first pass more powerful, the IR should be SSA-ified.  This requires merge
// block inputs to be extended to a list and allowed in branches other than `If`.
use crate::ir::*;
use crate::*;

#[cfg_attr(debug_assertions, derive(Debug))]
struct Referenced {
    pub variables: Vec<bool>,
    pub constants: Vec<bool>,
    pub types: Vec<bool>,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    referenced: Referenced,
    // Given RegisterId, lists registers that were used in evaluating it; this is a very short
    // list, which is basically the number of register arguments in the instruction
    // corresponding to that register id.
    register_deps: Vec<Vec<RegisterId>>,
    // Whether a given RegisterId is live.  A register is live if it's used in an instruction with
    // a side effect, or in evaluating another register that is live.
    is_live: Vec<bool>,
}

pub fn run(ir: &mut IR) {
    let referenced_variables = vec![false; ir.meta.all_variables().len()];
    let referenced_constants = vec![false; ir.meta.all_constants().len()];
    let referenced_types = vec![false; ir.meta.all_types().len()];
    let register_deps = vec![vec![]; ir.meta.total_register_count() as usize];
    let is_live = vec![false; ir.meta.total_register_count() as usize];

    let mut state = State {
        ir_meta: &mut ir.meta,
        referenced: Referenced {
            variables: referenced_variables,
            constants: referenced_constants,
            types: referenced_types,
        },
        register_deps,
        is_live,
    };

    // Build the register deps graph
    traverser::visitor::for_each_function(
        &mut state,
        &ir.function_entries,
        |_, _| {},
        |state, block, _, _| {
            visit_registers(state, block);
            traverser::visitor::VISIT_SUB_BLOCKS
        },
        |_, _| {},
    );

    // Don't prune interface variables, as reflection info is not yet gathered.
    // TODO(http://anglebug.com/349994211): Once reflection info is collected before this step,
    // removing the following loop would let inactive variables be pruned.
    for &variable_id in state.ir_meta.all_global_variables() {
        let variable = state.ir_meta.get_variable(variable_id);
        if !variable.decorations.decorations.is_empty() || variable.built_in.is_some() {
            state.referenced.variables[variable_id.id as usize] = true;
        }
    }

    // Remove dead instructions, and mark types, constants and variables that are visited in the
    // leftover instructions.
    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, function_id, entry| {
            // Consider all function parameters as live.
            state
                .ir_meta
                .get_function(function_id)
                .params
                .iter()
                .for_each(|param| state.referenced.variables[param.variable_id.id as usize] = true);

            // Traverse the blocks.  Note that blocks are transformed after sub-blocks are visited,
            // see comments in `visit_registers` and `transform_block`.
            transform_blocks_in_reverse_order(state, entry);
        },
    );

    // Remove unreferenced variables from blocks.
    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        &|state, _, entry| {
            traverser::transformer::for_each_block(
                state,
                entry,
                &|state, block| {
                    // Prune local variables to exclude unreferenced variables.
                    block
                        .variables
                        .retain(|variable_id| state.referenced.variables[variable_id.id as usize]);
                    block
                },
                &|_, block| block,
            )
        },
    );
    // Prune unreferenced global variables too.
    state
        .ir_meta
        .prune_global_variables(|variable_id| state.referenced.variables[variable_id.id as usize]);

    // Mark constants and types that are used outside instructions as referenced.
    mark_referenced_variable_types_and_initializers(state.ir_meta, &mut state.referenced);
    mark_referenced_constant_components(state.ir_meta, &mut state.referenced.constants);
    mark_referenced_type_components(state.ir_meta, &mut state.referenced.types);

    // Mark the variables, types and constants that are no longer referenced as
    // dead-code-eliminated.
    state.referenced.variables.iter().enumerate().for_each(|(id, is_referenced)| {
        if !is_referenced {
            state.ir_meta.dead_code_eliminate_variable(VariableId { id: id as u32 });
        }
    });
    state.referenced.constants.iter().enumerate().for_each(|(id, is_referenced)| {
        if !is_referenced {
            state.ir_meta.dead_code_eliminate_constant(ConstantId { id: id as u32 });
        }
    });
    state.referenced.types.iter().enumerate().for_each(|(id, is_referenced)| {
        if !is_referenced {
            state.ir_meta.dead_code_eliminate_type(TypeId { id: id as u32 });
        }
    });
}

fn extract_registers(ids: &mut impl Iterator<Item = TypedId>) -> Vec<RegisterId> {
    // While we could technically end up with duplicate ids in `deps`, that would be very rare and
    // also not matter; when propagating liveness, the algorithm stops if a register is already live
    // so the duplicates automatically get skipped.
    ids.filter_map(|id| if let Id::Register(id) = id.id { Some(id) } else { None }).collect()
}

fn propagate_liveness(
    is_live: &mut [bool],
    register_deps: &[Vec<RegisterId>],
    live_registers: Vec<RegisterId>,
) {
    let mut live_registers = live_registers;
    while let Some(register_id) = live_registers.pop() {
        // If not live, mark it as live and push its deps on the stack to be processed recursively.
        if !is_live[register_id.id as usize] {
            is_live[register_id.id as usize] = true;
            live_registers.extend(&register_deps[register_id.id as usize]);
        }
    }
}

fn visit_registers(state: &mut State, block: &Block) {
    // If the input to the merge block is used, so are the values being merged from the sub-blocks.
    if let Some(merge_block) = block.merge_block.as_ref()
        && let Some(input) = merge_block.input
    {
        block.for_each_sub_block(
            &mut state.register_deps[input.id.id as usize],
            &|deps, sub_block| {
                let OpCode::Merge(Some(id)) =
                    sub_block.get_merge_chain_last_block().get_terminating_op()
                else {
                    panic!(
                        "Internal error: Sub blocks of block with merge input must terminate with \
                         Merge(Some(..))"
                    );
                };
                if let Id::Register(id) = id.id {
                    deps.push(id);
                }
            },
        );
    }

    for instruction in block.instructions.iter() {
        let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
        // Instructions with side effect shouldn't be dead-code eliminated.  "return value" is
        // considered with side effect as well, as the only control flow instruction that can make a
        // register live (other than Merge, which is handled above).
        //
        // Currently, DCE may only remove control flow in basic cases (in particulal, only `if`
        // blocks whose body is entirely eliminated), so LoopIf and Switch are also considered
        // with side effect.
        //
        // To support DCE of `if` (and subsequently DCE of the `if` expression), the `If` id is not
        // actually marked as live here.  Later, removal of dead instructions is done in reverse
        // order.  This way, when a block is visited, it can check its sub-blocks and eliminate the
        // `If` if possible, or mark its expression id as live otherwise.
        let has_side_effect = opcode.has_side_effect()
            || matches!(opcode, OpCode::Return(_) | OpCode::LoopIf(_) | OpCode::Switch(..));

        let deps = match opcode {
            OpCode::MergeInput
            | OpCode::Discard
            | OpCode::Break
            | OpCode::Continue
            | OpCode::Passthrough
            | OpCode::NextBlock
            | OpCode::If(_)
            | OpCode::Loop
            | OpCode::DoLoop
            | OpCode::Return(None)
            | OpCode::Merge(_) => vec![],
            OpCode::Call(_, params)
            | OpCode::ConstructVectorFromMultiple(params)
            | OpCode::ConstructMatrixFromMultiple(params)
            | OpCode::ConstructStruct(params)
            | OpCode::ConstructArray(params)
            | OpCode::BuiltIn(_, params) => extract_registers(&mut params.iter().copied()),
            OpCode::Return(Some(id))
            | OpCode::LoopIf(id)
            | OpCode::ExtractVectorComponent(id, _)
            | OpCode::ExtractVectorComponentMulti(id, _)
            | OpCode::ExtractStructField(id, _)
            | OpCode::ConstructScalarFromScalar(id)
            | OpCode::ConstructVectorFromScalar(id)
            | OpCode::ConstructMatrixFromScalar(id)
            | OpCode::ConstructMatrixFromMatrix(id)
            | OpCode::AccessVectorComponent(id, _)
            | OpCode::AccessVectorComponentMulti(id, _)
            | OpCode::AccessStructField(id, _)
            | OpCode::Load(id)
            | OpCode::Unary(_, id)
            | OpCode::Alias(id)
            | OpCode::Switch(id, _) => extract_registers(&mut std::iter::once(*id)),
            OpCode::ExtractVectorComponentDynamic(lhs, rhs)
            | OpCode::ExtractMatrixColumn(lhs, rhs)
            | OpCode::ExtractArrayElement(lhs, rhs)
            | OpCode::AccessVectorComponentDynamic(lhs, rhs)
            | OpCode::AccessMatrixColumn(lhs, rhs)
            | OpCode::AccessArrayElement(lhs, rhs)
            | OpCode::Store(lhs, rhs)
            | OpCode::Binary(_, lhs, rhs) => extract_registers(&mut [*lhs, *rhs].iter().copied()),
            OpCode::Texture(texture_op, sampler, coord) => match texture_op {
                TextureOpCode::Implicit { is_proj: _, offset }
                | TextureOpCode::Gather { offset } => {
                    extract_registers(&mut [*sampler, *coord].iter().chain(offset.iter()).copied())
                }
                TextureOpCode::Compare { compare } => {
                    extract_registers(&mut [*sampler, *coord, *compare].iter().copied())
                }
                TextureOpCode::Lod { is_proj: _, lod, offset } => extract_registers(
                    &mut [*sampler, *coord, *lod].iter().chain(offset.iter()).copied(),
                ),
                TextureOpCode::CompareLod { compare, lod } => {
                    extract_registers(&mut [*sampler, *coord, *compare, *lod].iter().copied())
                }
                TextureOpCode::Bias { is_proj: _, bias, offset } => extract_registers(
                    &mut [*sampler, *coord, *bias].iter().chain(offset.iter()).copied(),
                ),
                TextureOpCode::CompareBias { compare, bias } => {
                    extract_registers(&mut [*sampler, *coord, *compare, *bias].iter().copied())
                }
                TextureOpCode::Grad { is_proj: _, dx, dy, offset } => extract_registers(
                    &mut [*sampler, *coord, *dx, *dy].iter().chain(offset.iter()).copied(),
                ),
                TextureOpCode::GatherComponent { component, offset } => extract_registers(
                    &mut [*sampler, *coord, *component].iter().chain(offset.iter()).copied(),
                ),
                TextureOpCode::GatherRef { refz, offset } => extract_registers(
                    &mut [*sampler, *coord, *refz].iter().chain(offset.iter()).copied(),
                ),
            },
        };

        // If the instruction produces a value, associate the parameters with the result.  If it has
        // a side effect, mark its result live.
        if let Some(id) = result {
            // Every register is evaluated only once
            let id = id.id.id as usize;
            debug_assert!(state.register_deps[id].is_empty());
            state.register_deps[id] = deps.clone();
            if has_side_effect {
                state.is_live[id] = true;
            }
        }

        // If the instruction has side effect, its result is live (set above).  Propagate liveness
        // to its arguments.
        if has_side_effect {
            propagate_liveness(&mut state.is_live, &state.register_deps, deps);
        }
    }
}

fn record_reference(referenced: &mut Referenced, id: TypedId) {
    referenced.types[id.type_id.id as usize] = true;
    match id.id {
        Id::Register(_) => {}
        Id::Constant(constant_id) => {
            referenced.constants[constant_id.id as usize] = true;
        }
        Id::Variable(variable_id) => {
            referenced.variables[variable_id.id as usize] = true;
        }
    }
}

// When blocks are traversed, some statistics are returned, which are used for further pruning:
//
// * If a loop's body does not contain `continue`, the continue block can be eliminated.
#[derive(Copy, Clone)]
struct BlockStats {
    // Whether this block has `continue` anywhere nested in it.  If a `continue` is under a nested
    // loop, that doesn't count because it cannot lead to the execution of the outer loop's
    // continue block.
    has_continue: bool,
}

impl BlockStats {
    fn new() -> BlockStats {
        BlockStats { has_continue: false }
    }

    fn accumulate_has_continue(&self, child: &BlockStats) -> BlockStats {
        BlockStats { has_continue: self.has_continue || child.has_continue }
    }
}

fn transform_blocks_in_reverse_order(state: &mut State, block: &mut Block) -> BlockStats {
    let mut merge_block_stats = BlockStats::new();
    // First visit the merge block
    if let Some(merge_block) = block.merge_block.as_mut() {
        merge_block_stats = transform_blocks_in_reverse_order(state, merge_block);
    }

    let is_loop = matches!(block.get_terminating_op(), OpCode::Loop | OpCode::DoLoop);
    let is_do_loop = matches!(block.get_terminating_op(), OpCode::DoLoop);

    // Then, every sub-block.
    let block1_stats = block
        .block1
        .as_mut()
        .map(|sub_block| transform_blocks_in_reverse_order(state, sub_block))
        .unwrap_or(BlockStats::new());
    // Before visiting the continue block of a loop, check to see if its
    // body has any `continue` instructions at all.  If it doesn't, the continue block is dead code
    // and can be removed.  For example:
    //
    //     for (...; ++i)
    //     {
    //         // no continue, then:
    //         break;
    //     }
    //
    // Similarly, before visiting the condition block of a do-loop, check to see if its body has any
    // `continue` instructions.  If it doesn't, the condition block is dead code.  For example:
    //
    //     do
    //     {
    //         // no continue, then:
    //         break;
    //     } while (condition)
    if !block1_stats.has_continue {
        if is_loop {
            block.block2.take();
        }
        if is_do_loop {
            block.loop_condition.take();
        }
    }
    let block2_stats = block
        .block2
        .as_mut()
        .map(|sub_block| transform_blocks_in_reverse_order(state, sub_block))
        .unwrap_or(BlockStats::new());
    let loop_condition_stats = block
        .loop_condition
        .as_mut()
        .map(|sub_block| transform_blocks_in_reverse_order(state, sub_block))
        .unwrap_or(BlockStats::new());
    let case_blocks_stats =
        block.case_blocks.iter_mut().fold(BlockStats::new(), |acc, sub_block| {
            let case_stats = transform_blocks_in_reverse_order(state, sub_block);
            acc.accumulate_has_continue(&case_stats)
        });

    // The loop condition is not allowed to have a `continue` branch.
    debug_assert!(!loop_condition_stats.has_continue);

    // Finally, visit this block.
    transform_block(state, block);

    // Calculate stats for this block
    let ends_in_continue = matches!(block.get_terminating_op(), OpCode::Continue);
    BlockStats {
        // The block may `continue` if it's not a loop, and has a nested `continue` in sub-blocks or
        // merge block.  If it's a loop, nested sub-blocks may contain `continue`, but that doesn't
        // affect the outer loop that needs to know if this block may `continue` to _its_ continue
        // block.
        has_continue: ends_in_continue
            || merge_block_stats.has_continue
            || (!is_loop
                && (block1_stats.has_continue
                    || block2_stats.has_continue
                    || case_blocks_stats.has_continue)),
    }
}

fn transform_block(state: &mut State, block: &mut Block) {
    // If the input to the merge block is unused, remove it, and turn the Merge(Some(_))
    // instructions to Merge(None) in the sub-blocks.
    if let Some(merge_block) = block.merge_block.as_mut()
        && let Some(input) = merge_block.input
        && !state.is_live[input.id.id as usize]
    {
        merge_block.input = None;

        block.for_each_sub_block_mut(&mut (), &|_, sub_block| {
            debug_assert!(matches!(
                sub_block.get_merge_chain_last_block().get_terminating_op(),
                OpCode::Merge(Some(_))
            ));
            let last_block = sub_block.get_merge_chain_last_block_mut();
            last_block.unterminate();
            last_block.terminate(OpCode::Merge(None));
        });
    }

    // Check if this block ends in `If(id)`:
    //
    // * If both sub-blocks are empty, turn the terminator to `NextBlock` and remove the sub-blocks.
    //   This lets `id` be eliminated.
    // * Otherwise mark `id` as live and propagate liveness before continuing.
    //
    // Note that blocks are transformed after their sub-blocks, including merge blocks.  Otherwise
    // this algorithm won't work as the instructions of this block are about to be pruned after
    // this; all the `If`s after this block must have already been visited so that liveness
    // information for anything that might depend on registers defined in this block is already
    // calculated.
    if let OpCode::If(expression) = *block.get_terminating_op() {
        let mut all_sub_blocks_empty = true;
        block.for_each_sub_block_mut(
            &mut all_sub_blocks_empty,
            &|all_sub_blocks_empty, sub_block| {
                if !util::is_empty_block(sub_block) {
                    *all_sub_blocks_empty = false;
                }
            },
        );
        if all_sub_blocks_empty {
            // Branch to next block directly and eliminate the true and false sub-blocks.
            block.unterminate();
            block.terminate(OpCode::NextBlock);
            block.block1 = None;
            block.block2 = None;
        } else {
            // The `if` expression is live after all.
            if let Id::Register(expression) = expression.id {
                propagate_liveness(&mut state.is_live, &state.register_deps, vec![expression]);
            }
        }
    }

    block.instructions.retain(|instruction| {
        let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
        if let Some(id) = result {
            // If the result is not live, remove this instruction before visiting it, so its
            // (non-register) parameters aren't considered live.
            if !state.is_live[id.id.id as usize] {
                return false;
            }

            state.referenced.types[id.type_id.id as usize] = true;
        }

        match opcode {
            OpCode::MergeInput
            | OpCode::Discard
            | OpCode::Break
            | OpCode::Continue
            | OpCode::Passthrough
            | OpCode::NextBlock
            | OpCode::Loop
            | OpCode::DoLoop
            | OpCode::Return(None)
            | OpCode::Merge(None) => (),
            OpCode::Call(_, params)
            | OpCode::ConstructVectorFromMultiple(params)
            | OpCode::ConstructMatrixFromMultiple(params)
            | OpCode::ConstructStruct(params)
            | OpCode::ConstructArray(params)
            | OpCode::BuiltIn(_, params) => {
                for param in params {
                    record_reference(&mut state.referenced, *param);
                }
            }
            OpCode::Return(Some(id))
            | OpCode::Merge(Some(id))
            | OpCode::If(id)
            | OpCode::LoopIf(id)
            | OpCode::ExtractVectorComponent(id, _)
            | OpCode::ExtractVectorComponentMulti(id, _)
            | OpCode::ExtractStructField(id, _)
            | OpCode::ConstructScalarFromScalar(id)
            | OpCode::ConstructVectorFromScalar(id)
            | OpCode::ConstructMatrixFromScalar(id)
            | OpCode::ConstructMatrixFromMatrix(id)
            | OpCode::AccessVectorComponent(id, _)
            | OpCode::AccessVectorComponentMulti(id, _)
            | OpCode::AccessStructField(id, _)
            | OpCode::Load(id)
            | OpCode::Unary(_, id)
            | OpCode::Alias(id) => record_reference(&mut state.referenced, *id),
            OpCode::Switch(id, cases) => {
                record_reference(&mut state.referenced, *id);
                for case in cases {
                    case.map(|id| {
                        state.referenced.constants[id.id as usize] = true;
                    });
                }
            }
            OpCode::ExtractVectorComponentDynamic(lhs, rhs)
            | OpCode::ExtractMatrixColumn(lhs, rhs)
            | OpCode::ExtractArrayElement(lhs, rhs)
            | OpCode::AccessVectorComponentDynamic(lhs, rhs)
            | OpCode::AccessMatrixColumn(lhs, rhs)
            | OpCode::AccessArrayElement(lhs, rhs)
            | OpCode::Store(lhs, rhs)
            | OpCode::Binary(_, lhs, rhs) => {
                record_reference(&mut state.referenced, *lhs);
                record_reference(&mut state.referenced, *rhs);
            }
            OpCode::Texture(texture_op, sampler, coord) => {
                record_reference(&mut state.referenced, *sampler);
                record_reference(&mut state.referenced, *coord);
                match texture_op {
                    TextureOpCode::Implicit { is_proj: _, offset }
                    | TextureOpCode::Gather { offset } => {
                        offset.map(|id| record_reference(&mut state.referenced, id));
                    }
                    TextureOpCode::Compare { compare } => {
                        record_reference(&mut state.referenced, *compare);
                    }
                    TextureOpCode::Lod { is_proj: _, lod, offset } => {
                        record_reference(&mut state.referenced, *lod);
                        offset.map(|id| record_reference(&mut state.referenced, id));
                    }
                    TextureOpCode::CompareLod { compare, lod } => {
                        record_reference(&mut state.referenced, *compare);
                        record_reference(&mut state.referenced, *lod);
                    }
                    TextureOpCode::Bias { is_proj: _, bias, offset } => {
                        record_reference(&mut state.referenced, *bias);
                        offset.map(|id| record_reference(&mut state.referenced, id));
                    }
                    TextureOpCode::CompareBias { compare, bias } => {
                        record_reference(&mut state.referenced, *compare);
                        record_reference(&mut state.referenced, *bias);
                    }
                    TextureOpCode::Grad { is_proj: _, dx, dy, offset } => {
                        record_reference(&mut state.referenced, *dx);
                        record_reference(&mut state.referenced, *dy);
                        offset.map(|id| record_reference(&mut state.referenced, id));
                    }
                    TextureOpCode::GatherComponent { component, offset } => {
                        record_reference(&mut state.referenced, *component);
                        offset.map(|id| record_reference(&mut state.referenced, id));
                    }
                    TextureOpCode::GatherRef { refz, offset } => {
                        record_reference(&mut state.referenced, *refz);
                        offset.map(|id| record_reference(&mut state.referenced, id));
                    }
                }
            }
        };
        // Keep the instruction as-is, the transformation part of this pass is only concerned with
        // removing dead instructions, and that's done at the top of this closure.
        true
    });
}

// For variables that are live, mark their type and initializer as live too.
fn mark_referenced_variable_types_and_initializers(ir_meta: &IRMeta, referenced: &mut Referenced) {
    referenced.variables.iter().enumerate().for_each(|(id, is_referenced)| {
        if *is_referenced {
            let variable = ir_meta.get_variable(VariableId { id: id as u32 });
            referenced.types[variable.type_id.id as usize] = true;
            variable.initializer.map(|id| referenced.constants[id.id as usize] = true);
        }
    });
}

fn mark_referenced(id: u32, referenced: &mut [bool], to_process: &mut Vec<usize>) {
    let id = id as usize;
    if !referenced[id] {
        referenced[id] = true;
        to_process.push(id);
    }
}

// For constants that are live, mark their constituting components as live too.
fn mark_referenced_constant_components(ir_meta: &IRMeta, referenced: &mut [bool]) {
    let mut to_process = referenced
        .iter()
        .enumerate()
        .filter_map(|(id, is_referenced)| if *is_referenced { Some(id) } else { None })
        .collect::<Vec<_>>();

    while let Some(id) = to_process.pop() {
        let constant = ir_meta.get_constant(ConstantId { id: id as u32 });
        if constant.value.is_composite() {
            let components = constant.value.get_composite_elements();
            for component_id in components {
                mark_referenced(component_id.id, referenced, &mut to_process);
            }
        }
    }
}

// For types that are live, mark their subtypes as live too.  We don't prune basic types (float,
// uvec4, etc), only structs have any reason to be dead-code-eliminated so that we don't need to
// unnecessarily declare them.
fn mark_referenced_type_components(ir_meta: &IRMeta, referenced: &mut [bool]) {
    let mut to_process = referenced
        .iter()
        .enumerate()
        .filter_map(|(id, is_referenced)| if *is_referenced { Some(id) } else { None })
        .collect::<Vec<_>>();

    while let Some(id) = to_process.pop() {
        let type_info = ir_meta.get_type(TypeId { id: id as u32 });
        match type_info {
            Type::Scalar(_) | Type::Vector(..) | Type::Matrix(..) | Type::Image(..) => (),
            Type::Struct(_, fields, _) => {
                for field in fields {
                    mark_referenced(field.type_id.id, referenced, &mut to_process);
                }
            }
            Type::Array(element_id, _)
            | Type::UnsizedArray(element_id)
            | Type::Pointer(element_id) => {
                mark_referenced(element_id.id, referenced, &mut to_process);
            }
            Type::DeadCodeEliminated => {
                panic!("Internal error: A dead-code-eliminated type cannot be referenced");
            }
        }
    }
}
