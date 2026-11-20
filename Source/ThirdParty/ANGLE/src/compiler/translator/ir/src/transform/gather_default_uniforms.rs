// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Gather default uniforms into a single struct (e.g. ANGLE_DefaultUniformBlock
// or defaultUniforms) and bind it as a uniform buffer (e.g. ANGLE_defaultUniformBlock).

use crate::ir::*;
use crate::*;
use std::collections::HashMap;

fn is_opaque_type(ir_meta: &IRMeta, type_id: TypeId) -> bool {
    let type_id = ir_meta.get_pointee_type(type_id);
    let type_id = ir_meta.get_base_element_type(type_id);
    let type_info = ir_meta.get_type(type_id);
    debug_assert!(!matches!(*type_info, Type::UnsizedArray(_)));
    debug_assert!(!type_info.is_struct_containing_samplers(ir_meta));
    type_info.is_opaque()
}

pub struct Options {
    pub struct_name: &'static str,
    pub instance_name: &'static str,
    // Uniforms whose type matches this predicate are left as individual variables instead of
    // being gathered into the block.
    pub exclude: Option<fn(&IRMeta, TypeId) -> bool>,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    block_typed_id: TypedId,
    default_uniform_map: &'a HashMap<VariableId, u32>,
}

pub fn run(ir: &mut IR, options: &Options) {
    let default_uniforms = gather_uniforms(&ir.meta, options);
    if default_uniforms.is_empty() {
        return;
    }

    let (block_typed_id, default_uniform_map) =
        declare_default_uniforms_block(&mut ir.meta, options, &default_uniforms);

    let mut state =
        State { ir_meta: &mut ir.meta, block_typed_id, default_uniform_map: &default_uniform_map };

    traverser::transformer::for_each_instruction(
        &mut state,
        &mut ir.function_entries,
        &|state, instruction| {
            let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
            match *opcode {
                OpCode::AccessVectorComponent(base, _)
                | OpCode::AccessVectorComponentMulti(base, _)
                | OpCode::AccessVectorComponentDynamic(base, _)
                | OpCode::AccessMatrixColumn(base, _)
                | OpCode::AccessArrayElement(base, _)
                | OpCode::AccessStructField(base, _)
                | OpCode::Load(base) => transform_access_or_load(
                    state,
                    instruction::Builder::<traverser::Transforms>::new(),
                    base,
                    result.unwrap(),
                ),
                OpCode::Call(id, ref args) => transform_call(
                    state,
                    instruction::Builder::<traverser::Transforms>::new(),
                    id,
                    &args.clone(),
                    result,
                ),
                _ => traverser::Transforms::new(),
            }
        },
    );

    // Prune all replaced default uniform global variables.
    ir.meta
        .prune_global_variables(|variable_id, _| !default_uniform_map.contains_key(&variable_id));
}

fn gather_uniforms(ir_meta: &IRMeta, options: &Options) -> Vec<VariableId> {
    ir_meta
        .all_global_variables()
        .iter()
        .copied()
        .filter(|&var_id| {
            let var = ir_meta.get_variable(var_id);
            debug_assert!(!var.is_dead_code_eliminated);
            debug_assert!(!ir_meta.get_type(var.type_id).is_dead_code_eliminated());

            var.decorations.has(Decoration::Uniform)
                && !has_decoration!(var.decorations, Decoration::Block)
                && !var.decorations.has(Decoration::Buffer)
                && var.built_in.is_none()
                && !options.exclude.is_some_and(|exclude| exclude(ir_meta, var.type_id))
                && !is_opaque_type(ir_meta, var.type_id)
        })
        .collect()
}

fn declare_default_uniforms_block(
    ir_meta: &mut IRMeta,
    options: &Options,
    default_uniforms: &[VariableId],
) -> (TypedId, HashMap<VariableId, u32>) {
    let mut fields = Vec::new();
    let mut default_uniform_map = HashMap::new();

    for (index, &var_id) in default_uniforms.iter().enumerate() {
        let var = ir_meta.get_variable(var_id);
        let pointee_type_id = ir_meta.get_pointee_type(var.type_id);
        let field = Field::new(
            var.name,
            pointee_type_id,
            var.precision,
            var.precise,
            var.decorations.clone(),
            None,
        );
        fields.push(field);
        default_uniform_map.insert(var_id, index as u32);
    }

    let struct_type_id = ir_meta.get_struct_type_id(
        Name::new_exact(options.struct_name),
        fields,
        StructSpecialization::InterfaceBlock,
    );

    let mut decorations = Decorations::new_none();
    decorations.decorations.push(Decoration::Uniform);
    decorations.decorations.push(Decoration::Block(BlockStorage::Std140));

    let (_block_var_id, block_typed_id) = ir_meta.declare_variable(
        Name::new_exact(options.instance_name),
        struct_type_id,
        Precision::High,
        false,
        decorations,
        None,
        None,
        VariableScope::Global,
    );

    (block_typed_id, default_uniform_map)
}

fn get_uniform_field(
    state: &mut State,
    builder: &mut instruction::Builder<traverser::Transforms>,
    operand: TypedId,
) -> Option<TypedId> {
    if let Id::Variable(var_id) = operand.id
        && let Some(&field_index) = state.default_uniform_map.get(&var_id)
    {
        Some(builder.struct_field(state.ir_meta, state.block_typed_id, field_index))
    } else {
        None
    }
}

fn transform_access_or_load(
    state: &mut State,
    mut builder: instruction::Builder<traverser::Transforms>,
    base: TypedId,
    result: TypedRegisterId,
) -> traverser::Transforms {
    let Some(field) = get_uniform_field(state, &mut builder, base) else {
        return builder.finish();
    };
    let accessed = match state.ir_meta.get_instruction(result.id).op {
        OpCode::AccessVectorComponent(_, index) => {
            builder.vector_component(state.ir_meta, field, index)
        }
        OpCode::AccessVectorComponentMulti(_, ref indices) => {
            let indices = indices.clone();
            builder.vector_component_multi(state.ir_meta, field, indices)
        }
        OpCode::AccessVectorComponentDynamic(_, index)
        | OpCode::AccessMatrixColumn(_, index)
        | OpCode::AccessArrayElement(_, index) => builder.index(state.ir_meta, field, index),
        OpCode::AccessStructField(_, index) => builder.struct_field(state.ir_meta, field, index),
        OpCode::Load(_) => builder.load(state.ir_meta, field),
        _ => {
            unreachable!("Internal error: Unexpected instruction while gathering default uniforms")
        }
    };
    builder.finish_with_result(state.ir_meta, result, accessed)
}

fn transform_call(
    state: &mut State,
    mut builder: instruction::Builder<traverser::Transforms>,
    function_id: FunctionId,
    args: &[TypedId],
    result: Option<TypedRegisterId>,
) -> traverser::Transforms {
    let mut new_args = args.to_vec();
    let mut changed = false;
    for arg in &mut new_args {
        if let Some(field) = get_uniform_field(state, &mut builder, *arg) {
            *arg = field;
            changed = true;
        }
    }
    if !changed {
        return builder.finish();
    }
    if let Some(result) = result {
        let call_res = builder.call(state.ir_meta, function_id, new_args);
        builder.finish_with_result(state.ir_meta, result, call_res)
    } else {
        builder.call_void(state.ir_meta, function_id, new_args);
        builder.finish()
    }
}
