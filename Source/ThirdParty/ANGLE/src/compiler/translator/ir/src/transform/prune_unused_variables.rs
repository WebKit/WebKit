// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Remove unreferenced variables from the IR.  During traversal, additionally unreferenced
// constants and types are tracked and marked as dead-code-eliminated.
//
// Eventually this transformation should be replaced with a more comprehensive
// dead-code-elimination algorithm.
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
}

pub fn run(ir: &mut IR) {
    let referenced_variables = vec![false; ir.meta.all_variables().len()];
    let referenced_constants = vec![false; ir.meta.all_constants().len()];
    let referenced_types = vec![false; ir.meta.all_types().len()];

    let mut state = State {
        ir_meta: &mut ir.meta,
        referenced: Referenced {
            variables: referenced_variables,
            constants: referenced_constants,
            types: referenced_types,
        },
    };

    // Don't prune interface variables, as reflection info is not yet gathered.
    // TODO(http://anglebug.com/349994211): Once reflection info is collected before this step,
    // removing the following loop would let inactive variables be pruned.
    for &variable_id in state.ir_meta.all_global_variables() {
        let variable = state.ir_meta.get_variable(variable_id);
        if !variable.decorations.decorations.is_empty() || variable.built_in.is_some() {
            state.referenced.variables[variable_id.id as usize] = true;
        }
    }

    // Visit all instructions and mark types, constants and variables that are visited.
    traverser::visitor::for_each_function(
        &mut state,
        &mut ir.function_entries,
        |state, function_id| {
            state
                .ir_meta
                .get_function(function_id)
                .params
                .iter()
                .for_each(|param| state.referenced.variables[param.variable_id.id as usize] = true);
        },
        |state, block, _, _| {
            visit_block(state, block);
            traverser::visitor::VISIT_SUB_BLOCKS
        },
        |_, _| {},
    );

    // Remove unreferenced variables from blocks.
    traverser::transformer::for_each_function(
        &mut state,
        &mut ir.function_entries,
        |_, _| {},
        &|state, entry| {
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

fn visit_block(state: &mut State, block: &Block) {
    for instruction in block.instructions.iter() {
        let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
        if let Some(id) = result {
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
    }
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

fn mark_referenced(id: u32, referenced: &mut Vec<bool>, to_process: &mut Vec<usize>) {
    let id = id as usize;
    if !referenced[id] {
        referenced[id] = true;
        to_process.push(id);
    }
}

// For constants that are live, mark their constituting components as live too.
fn mark_referenced_constant_components(ir_meta: &IRMeta, referenced: &mut Vec<bool>) {
    let mut to_process = referenced
        .iter()
        .enumerate()
        .filter_map(|(id, is_referenced)| if *is_referenced { Some(id) } else { None })
        .collect::<Vec<_>>();

    while !to_process.is_empty() {
        let id = to_process.pop().unwrap();
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
fn mark_referenced_type_components(ir_meta: &IRMeta, referenced: &mut Vec<bool>) {
    let mut to_process = referenced
        .iter()
        .enumerate()
        .filter_map(|(id, is_referenced)| if *is_referenced { Some(id) } else { None })
        .collect::<Vec<_>>();

    while !to_process.is_empty() {
        let id = to_process.pop().unwrap();
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
