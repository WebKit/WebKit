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
    function_entries: &Vec<Option<Block>>,
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

    while !visit_stack.is_empty() {
        let (function_id, visit) = visit_stack.pop().unwrap();
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

struct DuplicateBlockContext<'a, 'b, 'c> {
    ir_meta: &'a mut IRMeta,
    variable_map: &'b mut HashMap<VariableId, Id>,
    register_map: &'c mut HashMap<RegisterId, RegisterId>,
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
    block_variables: &Vec<VariableId>,
    block_input: Option<TypedRegisterId>,
) -> Block {
    let mut new_block = Block::new();
    new_block.variables = block_variables
        .iter()
        .map(|&variable_id| {
            let variable = context.ir_meta.get_variable(variable_id);

            // Note that variables inside blocks should always be Temporary, not Internal or
            // ShaderInterface.  As such, it's ok to reuse the same name, they will be
            // disambiguated during codegen if necessary.
            debug_assert!(variable.name.source == NameSource::Temporary);

            let replacement = Variable::new(
                variable.name,
                variable.type_id,
                variable.precision,
                variable.decorations.clone(),
                variable.built_in,
                variable.initializer,
                variable.scope,
            );
            let replacement = context.ir_meta.add_variable(replacement);

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
                let texture_op = match texture_op {
                    &TextureOpCode::Implicit { is_proj, offset } => TextureOpCode::Implicit {
                        is_proj,
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    &TextureOpCode::Compare { compare } => {
                        TextureOpCode::Compare { compare: mapped_id!(context, compare) }
                    }
                    &TextureOpCode::Lod { is_proj, lod, offset } => TextureOpCode::Lod {
                        is_proj,
                        lod: mapped_id!(context, lod),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    &TextureOpCode::CompareLod { compare, lod } => TextureOpCode::CompareLod {
                        compare: mapped_id!(context, compare),
                        lod: mapped_id!(context, lod),
                    },
                    &TextureOpCode::Bias { is_proj, bias, offset } => TextureOpCode::Bias {
                        is_proj,
                        bias: mapped_id!(context, bias),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    &TextureOpCode::CompareBias { compare, bias } => TextureOpCode::CompareBias {
                        compare: mapped_id!(context, compare),
                        bias: mapped_id!(context, bias),
                    },
                    &TextureOpCode::Grad { is_proj, dx, dy, offset } => TextureOpCode::Grad {
                        is_proj,
                        dx: mapped_id!(context, dx),
                        dy: mapped_id!(context, dy),
                        offset: offset.map(|id| mapped_id!(context, id)),
                    },
                    &TextureOpCode::Gather { offset } => {
                        TextureOpCode::Gather { offset: offset.map(|id| mapped_id!(context, id)) }
                    }
                    &TextureOpCode::GatherComponent { component, offset } => {
                        TextureOpCode::GatherComponent {
                            component: mapped_id!(context, component),
                            offset: offset.map(|id| mapped_id!(context, id)),
                        }
                    }
                    &TextureOpCode::GatherRef { refz, offset } => TextureOpCode::GatherRef {
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

    match op {
        &OpCode::Discard => block.terminate(OpCode::Discard),
        &OpCode::Return(id) => {
            block.terminate(OpCode::Return(id.map(|id| mapped_id!(context, id))))
        }
        &OpCode::Break => block.terminate(OpCode::Break),
        &OpCode::Continue => block.terminate(OpCode::Continue),
        &OpCode::Passthrough => block.terminate(OpCode::Passthrough),
        &OpCode::NextBlock => block.terminate(OpCode::NextBlock),
        &OpCode::Merge(id) => block.terminate(OpCode::Merge(id.map(|id| mapped_id!(context, id)))),
        &OpCode::If(id) => block.terminate(OpCode::If(mapped_id!(context, id))),
        &OpCode::Loop => block.terminate(OpCode::Loop),
        &OpCode::DoLoop => block.terminate(OpCode::DoLoop),
        &OpCode::LoopIf(id) => block.terminate(OpCode::LoopIf(mapped_id!(context, id))),
        &OpCode::Switch(id, ref case_ids) => {
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
    return matches!(base_type_id, TYPE_ID_FLOAT | TYPE_ID_INT | TYPE_ID_UINT);
}
